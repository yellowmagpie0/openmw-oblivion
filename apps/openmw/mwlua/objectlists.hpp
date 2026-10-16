#ifndef MWLUA_OBJECTLISTS_H
#define MWLUA_OBJECTLISTS_H

#include <set>
#include <span>
#include <cstdint>

#include "object.hpp"

namespace MWLua
{

    // ObjectLists is used to track lists of game objects like nearby.items, nearby.actors, etc.
    class ObjectLists
    {
    public:
        class PreparedSceneAdditions
        {
            struct Data;
            std::unique_ptr<Data> mData;
            explicit PreparedSceneAdditions(std::unique_ptr<Data> data);
            friend class ObjectLists;
        public:
            ~PreparedSceneAdditions();
            bool isValid() const noexcept;
            bool commit() noexcept;
        };
        // Membership only: supplied references already have reserved identities.
        // Does not register pointers or run scripts. Cancellation changes no list.
        std::unique_ptr<PreparedSceneAdditions> prepareSceneAdditions(std::span<const MWWorld::Ptr> references);

        class RetainedRuntime
        {
            friend class ObjectLists;
            struct Data;
            std::unique_ptr<Data> mData;
            explicit RetainedRuntime(std::unique_ptr<Data> data);
        public:
            ~RetainedRuntime();
            RetainedRuntime(RetainedRuntime&&) noexcept;
            RetainedRuntime& operator=(RetainedRuntime&&) noexcept;
            RetainedRuntime(const RetainedRuntime&) = delete;
            const ObjectLists& incomingLists() const;
            bool detach() noexcept;
            bool exchange() noexcept;
            bool restore() noexcept;
            void discard() noexcept;
        };
        ObjectLists() = default;
        ~ObjectLists();
        ObjectLists(const ObjectLists&) = delete;
        ObjectLists& operator=(const ObjectLists&) = delete;
        // Scripts and both World domains must be retained separately and idle.
        RetainedRuntime retainRuntime();
        void update(); // Should be called every frame.
        void clear(); // Should be called every time before starting or loading a new game.

        ObjectIdList getActivatorsInScene() const { return mActivatorsInScene.mList; }
        ObjectIdList getActorsInScene() const { return mActorsInScene.mList; }
        ObjectIdList getContainersInScene() const { return mContainersInScene.mList; }
        ObjectIdList getDoorsInScene() const { return mDoorsInScene.mList; }
        ObjectIdList getItemsInScene() const { return mItemsInScene.mList; }
        ObjectIdList getPlayers() const { return mPlayers; }

        void objectAddedToScene(const MWWorld::Ptr& ptr);
        void objectRemovedFromScene(const MWWorld::Ptr& ptr);

        void setPlayer(const MWWorld::Ptr& player) { requireMutableRuntime(); *mPlayers = { getId(player) }; }

    private:
        struct ObjectGroup
        {
            void updateList();
            void clear();

            bool mChanged = false;
            ObjectIdList mList = std::make_shared<std::vector<ObjectId>>();
            std::set<ObjectId> mSet;
        };

        ObjectGroup* chooseGroup(const MWWorld::Ptr& ptr);
        void addToGroup(ObjectGroup& group, const MWWorld::Ptr& ptr);
        void removeFromGroup(ObjectGroup& group, const MWWorld::Ptr& ptr);

        ObjectGroup mActivatorsInScene;
        ObjectGroup mActorsInScene;
        ObjectGroup mContainersInScene;
        ObjectGroup mDoorsInScene;
        ObjectGroup mItemsInScene;
        ObjectIdList mPlayers = std::make_shared<std::vector<ObjectId>>();
        RetainedRuntime::Data* mRetainedRuntime = nullptr;
        bool mParkedRuntime = false;
        std::shared_ptr<const char> mPreparationIdentity = std::make_shared<const char>(0);
        std::uint64_t mRevision = 0;
        void requireMutableRuntime() const;
        void swapRuntime(ObjectLists& other) noexcept;
    };

}

#endif // MWLUA_OBJECTLISTS_H
