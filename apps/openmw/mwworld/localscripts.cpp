#include "localscripts.hpp"

#include <components/debug/debuglog.hpp>
#include <components/esm3/loadcont.hpp>
#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadscpt.hpp>

#include "cellstore.hpp"
#include "class.hpp"
#include "containerstore.hpp"
#include "esmstore.hpp"

namespace
{

    struct AddScriptsVisitor
    {
        AddScriptsVisitor(MWWorld::LocalScripts& scripts)
            : mScripts(scripts)
        {
        }
        MWWorld::LocalScripts& mScripts;

        bool operator()(const MWWorld::Ptr& ptr)
        {
            if (ptr.mRef->isDeleted())
                return true;

            const ESM::RefId& script = ptr.getClass().getScript(ptr);

            if (!script.empty())
                mScripts.add(script, ptr);

            return true;
        }
    };

    struct AddContainerItemScriptsVisitor
    {
        AddContainerItemScriptsVisitor(MWWorld::LocalScripts& scripts)
            : mScripts(scripts)
        {
        }
        MWWorld::LocalScripts& mScripts;

        bool operator()(const MWWorld::Ptr& containerPtr)
        {
            // Ignore containers without generated content
            if (containerPtr.getType() == ESM::Container::sRecordId
                && containerPtr.getRefData().getCustomData() == nullptr)
                return true;

            MWWorld::ContainerStore& container = containerPtr.getClass().getContainerStore(containerPtr);
            for (const auto& ptr : container)
            {
                const ESM::RefId& script = ptr.getClass().getScript(ptr);
                if (!script.empty())
                {
                    MWWorld::Ptr item = ptr;
                    item.mCell = containerPtr.getCell();
                    mScripts.add(script, item);
                }
            }
            return true;
        }
    };

}

struct MWWorld::LocalScripts::PreparedRegistration::Data
{
    ESM::RefId mName;
    std::shared_ptr<const ESM::Script> mScript;
    MWScript::Locals mLocals;
    std::list<std::pair<ESM::RefId, Ptr>> mNode;
};

MWWorld::LocalScripts::PreparedRegistration::PreparedRegistration(
    const ESM::RefId& name, const ESMStore& content, const ESMStore& incoming)
    : mData(std::make_unique<Data>())
{
    mData->mName = name;
    if (const auto* script = content.searchForRestore<ESM::Script>(name, incoming))
    {
        mData->mScript = std::make_shared<const ESM::Script>(*script);
        mData->mLocals = MWScript::Locals::prepareConfiguration(name);
        mData->mNode.emplace_back(name, Ptr{});
    }
}
MWWorld::LocalScripts::PreparedRegistration::~PreparedRegistration() = default;
MWWorld::LocalScripts::PreparedRegistration::PreparedRegistration(PreparedRegistration&&) noexcept = default;
MWWorld::LocalScripts::PreparedRegistration& MWWorld::LocalScripts::PreparedRegistration::operator=(PreparedRegistration&&) noexcept = default;
MWWorld::LocalScripts::PreparedRegistration::PreparedRegistration(const PreparedRegistration& other)
    : mData(other.mData ? std::make_unique<Data>(*other.mData) : nullptr) {}
MWWorld::LocalScripts::PreparedRegistration& MWWorld::LocalScripts::PreparedRegistration::operator=(const PreparedRegistration& other)
{
    if (this != &other) { PreparedRegistration copy(other); *this = std::move(copy); }
    return *this;
}

MWWorld::LocalScripts::LocalScripts(const MWWorld::ESMStore& store)
    : mStore(store)
{
    mIter = mScripts.end();
}

void MWWorld::LocalScripts::startIteration()
{
    mIter = mScripts.begin();
}

bool MWWorld::LocalScripts::getNext(std::pair<ESM::RefId, Ptr>& script)
{
    if (mIter != mScripts.end())
    {
        auto iter = mIter++;
        script = *iter;
        return true;
    }
    return false;
}

void MWWorld::LocalScripts::add(const ESM::RefId& scriptName, const Ptr& ptr, PreparedRegistration* prepared)
{
    if (prepared && (!prepared->mData || prepared->mData->mName != scriptName
        || (prepared->mData->mScript && prepared->mData->mNode.empty())))
        throw std::logic_error("Prepared local script registration is unavailable");
    const ESM::Script* script = prepared ? prepared->mData->mScript.get() : mStore.get<ESM::Script>().search(scriptName);
    if (script)
    {
        try
        {
            ptr.getRefData().setLocals(*script, prepared ? &prepared->mData->mLocals : nullptr);

            for (auto iter = mScripts.begin(); iter != mScripts.end(); ++iter)
                if (iter->second == ptr)
                {
                    Log(Debug::Warning) << "Error: tried to add local script twice for " << ptr.getCellRef().getRefId();
                    remove(ptr);
                    break;
                }

            if (prepared)
            {
                auto& node = prepared->mData->mNode;
                node.front().second = ptr;
                mScripts.splice(mScripts.end(), node);
            }
            else
                mScripts.emplace_back(scriptName, ptr);
        }
        catch (const std::exception& exception)
        {
            Log(Debug::Error) << "failed to add local script " << scriptName
                              << " because an exception has been thrown: " << exception.what();
        }
    }
    else
        Log(Debug::Warning) << "failed to add local script " << scriptName << " because the script does not exist.";
}

void MWWorld::LocalScripts::addCell(CellStore* cell)
{
    AddScriptsVisitor addScriptsVisitor(*this);
    cell->forEach(addScriptsVisitor);

    AddContainerItemScriptsVisitor addContainerItemScriptsVisitor(*this);
    cell->forEachType<ESM::NPC>(addContainerItemScriptsVisitor);
    cell->forEachType<ESM::Creature>(addContainerItemScriptsVisitor);
    cell->forEachType<ESM::Container>(addContainerItemScriptsVisitor);
}

void MWWorld::LocalScripts::clear()
{
    mScripts.clear();
}

void MWWorld::LocalScripts::clearCell(CellStore* cell)
{
    auto iter = mScripts.begin();

    while (iter != mScripts.end())
    {
        if (iter->second.mCell == cell)
        {
            if (iter == mIter)
                ++mIter;

            mScripts.erase(iter++);
        }
        else
            ++iter;
    }
}

void MWWorld::LocalScripts::remove(const MWWorld::CellRef* ref)
{
    for (auto iter = mScripts.begin(); iter != mScripts.end(); ++iter)
        if (&(iter->second.getCellRef()) == ref)
        {
            if (iter == mIter)
                ++mIter;

            mScripts.erase(iter);
            break;
        }
}

void MWWorld::LocalScripts::remove(const Ptr& ptr)
{
    for (auto iter = mScripts.begin(); iter != mScripts.end(); ++iter)
        if (iter->second == ptr)
        {
            if (iter == mIter)
                ++mIter;

            mScripts.erase(iter);
            break;
        }
}

bool MWWorld::LocalScripts::isRunning(const ESM::RefId& scriptName, const Ptr& ptr) const
{
    return std::ranges::find(mScripts, std::pair(scriptName, ptr)) != mScripts.end();
}
