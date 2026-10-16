#include "objectlists.hpp"

#include <stdexcept>

#include <components/misc/resourcehelpers.hpp>

#include "../mwbase/environment.hpp"

#include "../mwclass/container.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/worldmodel.hpp"

namespace MWLua
{

    struct ObjectLists::RetainedRuntime::Data
    {
        ObjectLists* mOwner;
        std::unique_ptr<ObjectLists> mArchive = std::make_unique<ObjectLists>();
        bool mDetached = false;
        bool mOriginalSelected = false;
        explicit Data(ObjectLists& owner) : mOwner(&owner) {}
        void swap() noexcept
        {
            mOwner->swapRuntime(*mArchive);
            mArchive->mParkedRuntime = true;
        }
        void release() noexcept { mArchive.reset(); mOwner = nullptr; }
    };
    void ObjectLists::swapRuntime(ObjectLists& other) noexcept
    {
        auto swapGroup = [](ObjectGroup& a, ObjectGroup& b) {
            std::swap(a.mChanged, b.mChanged);
            a.mList.swap(b.mList);
            a.mSet.swap(b.mSet);
        };
        swapGroup(mActivatorsInScene, other.mActivatorsInScene);
        swapGroup(mActorsInScene, other.mActorsInScene);
        swapGroup(mContainersInScene, other.mContainersInScene);
        swapGroup(mDoorsInScene, other.mDoorsInScene);
        swapGroup(mItemsInScene, other.mItemsInScene);
        mPlayers.swap(other.mPlayers);
        mPreparationIdentity.swap(other.mPreparationIdentity);
        std::swap(mRevision, other.mRevision);
    }
    ObjectLists::~ObjectLists()
    { if (mRetainedRuntime) mRetainedRuntime->release(); }
    void ObjectLists::requireMutableRuntime() const
    {
        if (mParkedRuntime || (mRetainedRuntime && !mRetainedRuntime->mDetached))
            throw std::logic_error("Lua object lists are parked or frozen before detachment");
    }
    ObjectLists::RetainedRuntime ObjectLists::retainRuntime()
    {
        if (mRetainedRuntime || mParkedRuntime)
            throw std::logic_error("Lua object lists are already retained or parked");
        auto data = std::make_unique<RetainedRuntime::Data>(*this);
        mRetainedRuntime = data.get();
        return RetainedRuntime(std::move(data));
    }
    ObjectLists::RetainedRuntime::RetainedRuntime(std::unique_ptr<Data> data) : mData(std::move(data)) {}
    ObjectLists::RetainedRuntime::~RetainedRuntime()
    { if (mData && mData->mOwner && !restore()) std::terminate(); }
    ObjectLists::RetainedRuntime::RetainedRuntime(RetainedRuntime&&) noexcept = default;
    ObjectLists::RetainedRuntime& ObjectLists::RetainedRuntime::operator=(RetainedRuntime&& other) noexcept
    {
        if (this != &other)
        {
            if (mData && mData->mOwner && !restore()) std::terminate();
            mData = std::move(other.mData);
        }
        return *this;
    }
    const ObjectLists& ObjectLists::RetainedRuntime::incomingLists() const
    {
        if (!mData || !mData->mOwner || mData->mDetached)
            throw std::logic_error("Incoming Lua lists are only available before detachment");
        return *mData->mArchive;
    }
    bool ObjectLists::RetainedRuntime::detach() noexcept
    {
        if (!mData || !mData->mOwner || mData->mDetached) return false;
        mData->swap(); mData->mDetached = true;
        return true;
    }
    bool ObjectLists::RetainedRuntime::exchange() noexcept
    {
        if (!mData || !mData->mOwner || !mData->mDetached) return false;
        mData->swap(); mData->mOriginalSelected = !mData->mOriginalSelected;
        return true;
    }
    bool ObjectLists::RetainedRuntime::restore() noexcept
    {
        if (!mData || !mData->mOwner) return false;
        if (mData->mDetached && !mData->mOriginalSelected) mData->swap();
        mData->mOwner->mRetainedRuntime = nullptr;
        mData->release(); mData.reset();
        return true;
    }
    void ObjectLists::RetainedRuntime::discard() noexcept
    {
        if (!mData || !mData->mOwner) return;
        if (!mData->mDetached) { restore(); return; }
        if (mData->mOriginalSelected) exchange();
        mData->mOwner->mRetainedRuntime = nullptr;
        mData->release(); mData.reset();
    }

    void ObjectLists::update()
    {
        requireMutableRuntime();
        mActivatorsInScene.updateList();
        mActorsInScene.updateList();
        mContainersInScene.updateList();
        mDoorsInScene.updateList();
        mItemsInScene.updateList();
    }

    void ObjectLists::clear()
    {
        requireMutableRuntime();
        auto identity = std::make_shared<const char>(0);
        mPreparationIdentity.swap(identity);
        ++mRevision;
        mActivatorsInScene.clear();
        mActorsInScene.clear();
        mContainersInScene.clear();
        mDoorsInScene.clear();
        mItemsInScene.clear();
    }

    ObjectLists::ObjectGroup* ObjectLists::chooseGroup(const MWWorld::Ptr& ptr)
    {
        // It is important to check `isMarker` first.
        // For example "prisonmarker" has class "Door" despite that it is only an invisible marker.
        if (Misc::ResourceHelpers::isHiddenMarker(ptr.getCellRef().getRefId()))
            return nullptr;
        const MWWorld::Class& cls = ptr.getClass();
        if (cls.isActivator())
            return &mActivatorsInScene;
        if (cls.isActor() || ptr.mRef->getType() == ESM::REC_NPC_4 || ptr.mRef->getType() == ESM::REC_CREA4)
            return &mActorsInScene;
        if (ptr.mRef->getType() == ESM::REC_DOOR || ptr.mRef->getType() == ESM::REC_DOOR4)
            return &mDoorsInScene;
        if (typeid(cls) == typeid(MWClass::Container))
            return &mContainersInScene;
        if (cls.isItem(ptr) || ptr.mRef->getType() == ESM::REC_LIGH)
            return &mItemsInScene;
        return nullptr;
    }

    void ObjectLists::objectAddedToScene(const MWWorld::Ptr& ptr)
    {
        requireMutableRuntime();
        MWBase::Environment::get().getWorldModel()->registerPtr(ptr);
        ObjectGroup* group = chooseGroup(ptr);
        if (group)
            addToGroup(*group, ptr);
    }

    void ObjectLists::objectRemovedFromScene(const MWWorld::Ptr& ptr)
    {
        requireMutableRuntime();
        ObjectGroup* group = chooseGroup(ptr);
        if (group)
            removeFromGroup(*group, ptr);
    }

    void ObjectLists::ObjectGroup::updateList()
    {
        if (mChanged)
        {
            mList->clear();
            for (ObjectId id : mSet)
                mList->push_back(id);
            mChanged = false;
        }
    }

    void ObjectLists::ObjectGroup::clear()
    {
        mChanged = false;
        mList->clear();
        mSet.clear();
    }

    void ObjectLists::addToGroup(ObjectGroup& group, const MWWorld::Ptr& ptr)
    {
        ++mRevision;
        group.mSet.insert(getId(ptr));
        group.mChanged = true;
    }

    void ObjectLists::removeFromGroup(ObjectGroup& group, const MWWorld::Ptr& ptr)
    {
        ++mRevision;
        group.mSet.erase(getId(ptr));
        group.mChanged = true;
    }
}

namespace MWLua
{
    struct ObjectLists::PreparedSceneAdditions::Data
    {
        struct Entry
        {
            MWWorld::Ptr mPtr;
            std::weak_ptr<const char> mReferenceIdentity;
            ObjectId mId;
            ESM::RefId mRecord;
            unsigned mType;
            int mCount;
            bool mEnabled;
            ObjectGroup* mGroup;
            std::set<ObjectId>::node_type mNode;
        };
        ObjectLists* mOwner;
        std::weak_ptr<const char> mIdentity;
        std::uint64_t mRevision;
        std::vector<Entry> mEntries;
        bool mConsumed = false;
        bool current() const noexcept
        {
            if (mConsumed || mIdentity.expired()) return false;
            if (mIdentity.lock() != mOwner->mPreparationIdentity || mRevision != mOwner->mRevision
                || mOwner->mParkedRuntime
                || (mOwner->mRetainedRuntime && !mOwner->mRetainedRuntime->mDetached)) return false;
            for (const auto& e : mEntries)
            {
                if (e.mReferenceIdentity.expired()) return false;
                if (getId(e.mPtr) != e.mId || e.mPtr.getType() != e.mType
                    || e.mPtr.getCellRef().getRefId() != e.mRecord
                    || e.mPtr.getCellRef().getCount(false) != e.mCount
                    || e.mPtr.getRefData().isEnabled() != e.mEnabled
                    || mOwner->chooseGroup(e.mPtr) != e.mGroup
                    || (e.mGroup && e.mGroup->mSet.contains(e.mId))) return false;
            }
            return true;
        }
    };
    ObjectLists::PreparedSceneAdditions::PreparedSceneAdditions(std::unique_ptr<Data> data) : mData(std::move(data)) {}
    ObjectLists::PreparedSceneAdditions::~PreparedSceneAdditions() = default;
    bool ObjectLists::PreparedSceneAdditions::isValid() const noexcept { return mData && mData->current(); }
    bool ObjectLists::PreparedSceneAdditions::commit() noexcept
    {
        if (!isValid()) return false;
        for (auto& e : mData->mEntries)
            if (e.mGroup)
            {
                if (!e.mGroup->mSet.insert(std::move(e.mNode)).inserted) std::terminate();
                e.mGroup->mChanged = true;
            }
        ++mData->mOwner->mRevision;
        mData->mConsumed = true;
        return true;
    }
    std::unique_ptr<ObjectLists::PreparedSceneAdditions> ObjectLists::prepareSceneAdditions(
        std::span<const MWWorld::Ptr> references)
    {
        requireMutableRuntime();
        if (references.empty()) throw std::invalid_argument("empty prepared Lua object-list addition");
        auto data = std::make_unique<PreparedSceneAdditions::Data>();
        data->mOwner = this; data->mIdentity = mPreparationIdentity; data->mRevision = mRevision;
        data->mEntries.reserve(references.size());
        std::set<ObjectId> seen;
        for (const auto& ptr : references)
        {
            if (ptr.isEmpty() || !getId(ptr).isSet() || ptr.getCellRef().getCount(false) <= 0
                || !ptr.getRefData().isEnabled() || !seen.insert(getId(ptr)).second)
                throw std::invalid_argument("Lua scene addition requires distinct live reserved references");
            auto* group = chooseGroup(ptr);
            if (group && group->mSet.contains(getId(ptr)))
                throw std::invalid_argument("Lua scene reference is already active");
            std::set<ObjectId> staged;
            if (group) staged.insert(getId(ptr));
            data->mEntries.push_back({ptr, ptr.getRefData().prepareRuntimeIdentity(), getId(ptr),
                ptr.getCellRef().getRefId(), ptr.getType(), ptr.getCellRef().getCount(false),
                ptr.getRefData().isEnabled(), group, group ? staged.extract(staged.begin()) : std::set<ObjectId>::node_type{}});
        }
        return std::unique_ptr<PreparedSceneAdditions>(new PreparedSceneAdditions(std::move(data)));
    }
}
