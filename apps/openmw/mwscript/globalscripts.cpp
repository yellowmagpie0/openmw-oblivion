#include "globalscripts.hpp"
#include <stdexcept>
#include <type_traits>

#include <limits>
#include <optional>

#include <components/debug/debuglog.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/globalscript.hpp>
#include <components/esm3/loadscpt.hpp>
#include <components/esm3/loadsscr.hpp>
#include <components/misc/strings/lower.hpp>

#include "../mwworld/esmstore.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/scriptmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/worldmodel.hpp"

#include "interpretercontext.hpp"

namespace
{
    struct ScriptCreatingVisitor
    {
        ESM::GlobalScript operator()(const MWWorld::Ptr& ptr) const
        {
            ESM::GlobalScript script;
            script.mRunning = false;
            if (!ptr.isEmpty())
            {
                if (MWBase::Environment::get().getWorld()->getPlayerPtr() == ptr)
                    script.mTargetId = ptr.getCellRef().getRefId();
                else if (ptr.getCellRef().getRefNum().isSet())
                {
                    script.mTargetId = ptr.getCellRef().getRefId();
                    script.mTargetRef = ptr.getCellRef().getRefNum();
                }
            }
            return script;
        }

        ESM::GlobalScript operator()(const std::pair<ESM::RefNum, ESM::RefId>& pair) const
        {
            ESM::GlobalScript script;
            script.mTargetId = pair.second;
            script.mTargetRef = pair.first;
            script.mRunning = false;
            return script;
        }
    };

    struct PtrGettingVisitor
    {
        const MWWorld::Ptr* operator()(const MWWorld::Ptr& ptr) const { return &ptr; }

        const MWWorld::Ptr* operator()(const std::pair<ESM::RefNum, ESM::RefId>& pair) const { return nullptr; }
    };

    struct PtrResolvingVisitor
    {
        MWWorld::Ptr operator()(const MWWorld::Ptr& ptr) const { return ptr; }

        MWWorld::Ptr operator()(const std::pair<ESM::RefNum, ESM::RefId>& pair) const
        {
            if (pair.first.isSet())
                return MWBase::Environment::get().getWorldModel()->getPtr(pair.first);
            else if (pair.second.empty())
                return MWWorld::Ptr();
            return MWBase::Environment::get().getWorld()->searchPtr(pair.second, false);
        }
    };

    class MatchPtrVisitor
    {
        const MWWorld::Ptr& mPtr;

    public:
        MatchPtrVisitor(const MWWorld::Ptr& ptr)
            : mPtr(ptr)
        {
        }

        bool operator()(const MWWorld::Ptr& ptr) const { return ptr == mPtr; }

        bool operator()(const std::pair<ESM::RefNum, ESM::RefId>& pair) const { return false; }
    };

    struct IdGettingVisitor
    {
        ESM::RefId operator()(const MWWorld::Ptr& ptr) const
        {
            if (ptr.isEmpty())
                return ESM::RefId();
            return ptr.mRef->mRef.getRefId();
        }

        ESM::RefId operator()(const std::pair<ESM::RefNum, ESM::RefId>& pair) const { return pair.second; }
    };
}

namespace MWScript
{
    namespace
    {
        struct RuntimeCall
        {
            unsigned& mDepth;
            explicit RuntimeCall(unsigned& depth) : mDepth(depth) { ++mDepth; }
            ~RuntimeCall() { --mDepth; }
        };
    }

    struct GlobalScripts::RetainedRuntime::Data
    {
        GlobalScripts* mOwner;
        decltype(GlobalScripts::mRestoreLifetime) mRestoreLifetime{};
        decltype(GlobalScripts::mRestoreEpoch) mRestoreEpoch{};
        decltype(GlobalScripts::mScripts) mScripts{};
        bool mDetached = false;
        bool mOriginalSelected = false;
        explicit Data(GlobalScripts& owner) : mOwner(&owner) { mRestoreLifetime = std::make_shared<const char>(); mRestoreEpoch = owner.mRestoreEpoch; }
        void swap() noexcept
        {
            using std::swap;
            static_assert(std::is_nothrow_swappable_v<decltype(mRestoreLifetime)>);
            swap(mOwner->mRestoreLifetime, mRestoreLifetime);
            static_assert(std::is_nothrow_swappable_v<decltype(mRestoreEpoch)>);
            swap(mOwner->mRestoreEpoch, mRestoreEpoch);
            static_assert(std::is_nothrow_swappable_v<decltype(mScripts)>);
            swap(mOwner->mScripts, mScripts);
        }
    };
    void GlobalScripts::requireMutableRuntime() const
    {
        if (mRetainedRuntime && !mRetainedRuntime->mDetached)
            throw std::logic_error("Outgoing GlobalScripts runtime is frozen before detachment");
    }
    GlobalScripts::RetainedRuntime::RetainedRuntime(std::unique_ptr<Data> data) : mData(std::move(data)) {}
    GlobalScripts::RetainedRuntime::~RetainedRuntime()
    {
        if (mData && mData->mOwner && !restore()) std::terminate();
    }
    GlobalScripts::RetainedRuntime::RetainedRuntime(RetainedRuntime&&) noexcept = default;
    GlobalScripts::RetainedRuntime& GlobalScripts::RetainedRuntime::operator=(RetainedRuntime&& other) noexcept
    {
        if (this != &other)
        {
            if (mData && mData->mOwner && !restore()) std::terminate();
            mData = std::move(other.mData);
        }
        return *this;
    }
    GlobalScripts::RetainedRuntime GlobalScripts::retainRuntime()
    {
        if (mRetainedRuntime || mActiveCalls) throw std::logic_error("GlobalScripts retention requires an idle runtime");
        auto data = std::make_unique<RetainedRuntime::Data>(*this);
        mRetainedRuntime = data.get();
        return RetainedRuntime(std::move(data));
    }
    bool GlobalScripts::RetainedRuntime::detach() noexcept
    {
        if (!mData || !mData->mOwner || mData->mDetached || mData->mOwner->mActiveCalls) return false;

        mData->swap(); mData->mDetached = true;
        return true;
    }
    bool GlobalScripts::RetainedRuntime::exchange() noexcept
    {
        if (!mData || !mData->mOwner || !mData->mDetached || mData->mOwner->mActiveCalls) return false;

        mData->swap(); mData->mOriginalSelected = !mData->mOriginalSelected;
        return true;
    }
    bool GlobalScripts::RetainedRuntime::restore() noexcept
    {
        if (!mData || !mData->mOwner || mData->mOwner->mActiveCalls) return false;

        if (mData->mDetached && !mData->mOriginalSelected) mData->swap();
        mData->mOwner->mRetainedRuntime = nullptr;
        mData->mOwner = nullptr;
        mData.reset();
        return true;
    }
    void GlobalScripts::RetainedRuntime::discard() noexcept
    {
        if (!mData || !mData->mOwner) return;
        if (!mData->mDetached) { restore(); return; }
        if (mData->mOriginalSelected && !exchange()) std::terminate();

        mData->mOwner->mRetainedRuntime = nullptr;
        mData->mOwner = nullptr;
        mData.reset();
    }
    GlobalScripts::~GlobalScripts()
    {
        if (mRetainedRuntime)
        {
            mRetainedRuntime->mRestoreLifetime.reset();
            mRetainedRuntime->mOwner = nullptr;
        }
    }


    GlobalScriptDesc::GlobalScriptDesc()
        : mRunning(false)
    {
    }

    const MWWorld::Ptr* GlobalScriptDesc::getPtrIfPresent() const
    {
        return std::visit(PtrGettingVisitor(), mTarget);
    }

    MWWorld::Ptr GlobalScriptDesc::getPtr()
    {
        MWWorld::Ptr ptr = std::visit(PtrResolvingVisitor{}, mTarget);
        mTarget = ptr;
        return ptr;
    }

    ESM::RefId GlobalScriptDesc::getId() const
    {
        return std::visit(IdGettingVisitor{}, mTarget);
    }

    GlobalScripts::GlobalScripts(const MWWorld::ESMStore& store)
        : mStore(store)
    {
    }

    void GlobalScripts::addScript(const ESM::RefId& name, const MWWorld::Ptr& target)
    {
        requireMutableRuntime();
        const auto iter = mScripts.find(name);

        if (iter == mScripts.end())
        {
            if (const ESM::Script* script = mStore.get<ESM::Script>().search(name))
            {
                auto desc = std::make_shared<GlobalScriptDesc>();
                MWWorld::Ptr ptr = target;
                desc->mTarget = ptr;
                desc->mRunning = true;
                desc->mLocals.configure(*script);
                mScripts.insert(std::make_pair(name, desc));
            }
            else
            {
                Log(Debug::Error) << "Failed to add global script " << name << ": script record not found";
            }
        }
        else if (!iter->second->mRunning)
        {
            iter->second->mRunning = true;
            MWWorld::Ptr ptr = target;
            iter->second->mTarget = ptr;
        }
    }

    void GlobalScripts::removeScript(const ESM::RefId& name)
    {
        requireMutableRuntime();
        const auto iter = mScripts.find(name);

        if (iter != mScripts.end())
            iter->second->mRunning = false;
    }

    bool GlobalScripts::isRunning(const ESM::RefId& name) const
    {
        const auto iter = mScripts.find(name);

        if (iter == mScripts.end())
            return false;

        return iter->second->mRunning;
    }

    void GlobalScripts::run()
    {
        requireMutableRuntime();
        const RuntimeCall call(mActiveCalls);
        for (const auto& script : mScripts)
        {
            if (script.second->mRunning)
            {
                MWScript::InterpreterContext context(script.second);
                if (!MWBase::Environment::get().getScriptManager()->run(script.first, context))
                    script.second->mRunning = false;
            }
        }
    }

    void GlobalScripts::clear()
    {
        requireMutableRuntime();
        ++mRestoreEpoch;
        mScripts.clear();
    }

    void GlobalScripts::addStartup()
    {
        requireMutableRuntime();
        // make list of global scripts to be added
        std::vector<ESM::RefId> scripts;

        const ESM::RefId main = ESM::RefId::stringRefId("main");
        if (mStore.get<ESM::Script>().search(main) != nullptr)
            scripts.emplace_back(main);

        for (MWWorld::Store<ESM::StartScript>::iterator iter = mStore.get<ESM::StartScript>().begin();
             iter != mStore.get<ESM::StartScript>().end(); ++iter)
        {
            scripts.push_back(iter->mId);
        }

        // add scripts
        for (auto iter(scripts.begin()); iter != scripts.end(); ++iter)
        {
            try
            {
                addScript(*iter);
            }
            catch (const std::exception& exception)
            {
                Log(Debug::Error) << "Failed to add start script " << *iter << " because an exception has "
                                  << "been thrown: " << exception.what();
            }
        }
    }

    size_t GlobalScripts::countSavedGameRecords() const
    {
        return mScripts.size();
    }

    void GlobalScripts::write(ESM::ESMWriter& writer, Loading::Listener& progress) const
    {
        for (const auto& [id, desc] : mScripts)
        {
            ESM::GlobalScript script = std::visit(ScriptCreatingVisitor{}, desc->mTarget);

            script.mId = id;

            desc->mLocals.write(script.mLocals, id);

            script.mRunning = desc->mRunning;

            writer.startRecord(ESM::REC_GSCR);
            script.save(writer);
            writer.endRecord(ESM::REC_GSCR);
        }
    }

    bool GlobalScripts::readRecord(ESM::ESMReader& reader, uint32_t type)
    {
        requireMutableRuntime();
        if (type == ESM::REC_GSCR)
        {
            ESM::GlobalScript script;
            script.load(reader);

            auto iter = mScripts.find(script.mId);

            if (iter == mScripts.end())
            {
                if (const ESM::Script* scriptRecord = mStore.get<ESM::Script>().search(script.mId))
                {
                    try
                    {
                        auto desc = std::make_shared<GlobalScriptDesc>();
                        if (!script.mTargetId.empty() || script.mTargetRef.isSet())
                        {
                            desc->mTarget = std::make_pair(script.mTargetRef, script.mTargetId);
                        }
                        desc->mLocals.configure(*scriptRecord);

                        iter = mScripts.insert(std::make_pair(script.mId, desc)).first;
                    }
                    catch (const std::exception& exception)
                    {
                        Log(Debug::Error) << "Failed to add start script " << script.mId
                                          << " because an exception has been thrown: " << exception.what();

                        return true;
                    }
                }
                else // script does not exist anymore
                    return true;
            }

            iter->second->mRunning = script.mRunning;
            iter->second->mLocals.read(script.mLocals, script.mId);

            return true;
        }

        return false;
    }

    std::function<bool()> GlobalScripts::prepareRecord(ESM::GlobalScript&& script)
    {
        if (mRestoreEpoch == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("Global script restore epoch exhausted");
        struct Plan
        {
            ESM::GlobalScript mState;
            std::optional<ESM::Script> mScript;
            Locals mLayout;
            std::map<ESM::RefId, std::shared_ptr<GlobalScriptDesc>> mNode;
            std::weak_ptr<const char> mLifetime;
            std::uint64_t mEpoch;
            bool mUsed = false;
        };
        auto plan = std::make_shared<Plan>();
        plan->mState = std::move(script);
        // SCPT is content-only in the save definition store. Never configure
        // against the outgoing global descriptor, whose values disappear on clear.
        if (const auto* record = mStore.get<ESM::Script>().search(plan->mState.mId))
        {
            plan->mScript = *record;
            plan->mLayout = Locals::prepareConfiguration(record->mId);
            auto desc = std::make_shared<GlobalScriptDesc>();
            if (!plan->mState.mTargetId.empty() || plan->mState.mTargetRef.isSet())
                desc->mTarget = std::make_pair(plan->mState.mTargetRef, plan->mState.mTargetId);
            plan->mNode.emplace(record->mId, std::move(desc));
        }
        plan->mLifetime = mRestoreLifetime;
        plan->mEpoch = mRestoreEpoch + 1;
        return [this, plan] {
            if (plan->mUsed || plan->mLifetime.expired() || mRestoreEpoch != plan->mEpoch)
                return false;
            plan->mUsed = true;
            auto iter = mScripts.find(plan->mState.mId);
            if (iter == mScripts.end())
            {
                if (!plan->mScript) return true; // Removed content script.
                auto& desc = plan->mNode.begin()->second;
                desc->mLocals.configure(*plan->mScript, &plan->mLayout);
                mScripts.merge(plan->mNode);
                iter = mScripts.find(plan->mState.mId);
            }
            iter->second->mRunning = plan->mState.mRunning;
            // Keep this at record position: unsaved variables and an existing
            // target retain values established by earlier restoration callbacks.
            iter->second->mLocals.read(plan->mState.mLocals, plan->mState.mId);
            return true;
        };
    }

    Locals& GlobalScripts::getLocals(const ESM::RefId& name)
    {
        requireMutableRuntime();
        auto iter = mScripts.find(name);

        if (iter == mScripts.end())
        {
            const ESM::Script* script = mStore.get<ESM::Script>().find(name);

            auto desc = std::make_shared<GlobalScriptDesc>();
            desc->mLocals.configure(*script);

            iter = mScripts.emplace(name, desc).first;
        }

        return iter->second->mLocals;
    }

    const GlobalScriptDesc* GlobalScripts::getScriptIfPresent(const ESM::RefId& name) const
    {
        auto iter = mScripts.find(name);
        if (iter == mScripts.end())
            return nullptr;
        return iter->second.get();
    }

    void GlobalScripts::updatePtrs(const MWWorld::Ptr& base, const MWWorld::Ptr& updated)
    {
        requireMutableRuntime();
        MatchPtrVisitor visitor(base);
        for (const auto& script : mScripts)
        {
            if (std::visit(visitor, script.second->mTarget))
                script.second->mTarget = updated;
        }
    }
}
