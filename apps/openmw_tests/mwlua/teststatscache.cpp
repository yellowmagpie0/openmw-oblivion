#include <gtest/gtest.h>
#include <array>

#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <components/esm3/readerscache.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/vfs/manager.hpp>
#include "apps/openmw/mwbase/environment.hpp"
#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwlua/localscripts.hpp"
#include "apps/openmw/mwlua/objectlists.hpp"
#include "apps/openmw/mwlua/luamanagerimp.hpp"
#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/livecellref.hpp"
#include "apps/openmw/mwworld/worldmodel.hpp"

namespace
{
    struct InspectableScripts : MWLua::LocalScripts
    {
        using LocalScripts::LocalScripts;
        MWLua::SelfObject& self() { return mData; }
    };
    std::vector<std::pair<int, int>> writes;
    void setter(const MWLua::SelfObject::CachedStat::Index& index, std::string_view,
        const MWWorld::Ptr&, const sol::object& value)
    {
        const int i = std::get<int>(index);
        writes.emplace_back(i, value.as<int>());
        if (i == 1)
            throw std::runtime_error("deliberately rejected stat write");
    }

    template <class F>
    void withScripts(F&& check)
    {
        writes.clear();
        MWWorld::ESMStore store;
        MWBase::Environment environment;
        environment.setESMStore(store);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(store, readers);
        environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        ESM::NPC base{};
        base.blank();
        base.mId = ESM::RefId::stringRefId("cache-test");
        ESM::CellRef reference{};
        reference.blank();
        reference.mRefID = base.mId;
        MWWorld::LiveCellRef<ESM::NPC> live(reference, &base);
        MWWorld::Ptr ptr(&live);
        model.registerPtr(ptr);
        VFS::Manager vfs;
        LuaUtil::ScriptsConfiguration configuration;
        LuaUtil::LuaState lua(&vfs, &configuration);
        MWLua::LuaManager manager(&vfs, {});
        InspectableScripts scripts(&lua, MWLua::LObject(ptr));
        check(scripts, manager, lua, model, live);
    }

    TEST(LuaStatsCacheTest, failedBatchDrainsWithoutReplayingCommittedPrefix)
    {
        withScripts([](auto& scripts, auto& manager, auto& lua, auto&, auto&) {
            auto& self = scripts.self();
            using Key = MWLua::SelfObject::CachedStat;
            const Key first(&setter, 0, "current"), bad(&setter, 1, "current"), last(&setter, 2, "current");
            self.cacheStat(manager, first, sol::make_object(lua.unsafeState(), 11));
            self.cacheStat(manager, bad, sol::make_object(lua.unsafeState(), 22));
            self.cacheStat(manager, last, sol::make_object(lua.unsafeState(), 33));
            EXPECT_THROW(scripts.applyStatsCache(), std::runtime_error);
            EXPECT_EQ(writes, (std::vector<std::pair<int, int>>{{0, 11}, {1, 22}}));
            EXPECT_EQ(self.getCachedStat(first), nullptr);
            EXPECT_EQ(self.getCachedStat(bad), nullptr);
            EXPECT_EQ(self.getCachedStat(last), nullptr);
            self.cacheStat(manager, last, sol::make_object(lua.unsafeState(), 44));
            EXPECT_NO_THROW(scripts.applyStatsCache());
            EXPECT_EQ(writes, (std::vector<std::pair<int, int>>{{0, 11}, {1, 22}, {2, 44}}));
            EXPECT_EQ(self.getCachedStat(last), nullptr);
        });
    }

    TEST(LuaStatsCacheTest, successfulBatchUsesLastValueAndDrains)
    {
        withScripts([](auto& scripts, auto& manager, auto& lua, auto&, auto&) {
            auto& self = scripts.self();
            const MWLua::SelfObject::CachedStat key(&setter, 2, "current");
            self.cacheStat(manager, key, sol::make_object(lua.unsafeState(), 11));
            self.cacheStat(manager, key, sol::make_object(lua.unsafeState(), 42));
            ASSERT_NE(self.getCachedStat(key), nullptr);
            EXPECT_EQ(self.getCachedStat(key)->template as<int>(), 42);
            EXPECT_NO_THROW(scripts.applyStatsCache());
            EXPECT_EQ(writes, (std::vector<std::pair<int, int>>{{2, 42}}));
            EXPECT_EQ(self.getCachedStat(key), nullptr);
        });
    }

    TEST(LuaStatsCacheTest, missingActorDiscardsQueuedValues)
    {
        withScripts([](auto& scripts, auto& manager, auto& lua, auto& model, auto& live) {
            auto& self = scripts.self();
            const MWLua::SelfObject::CachedStat key(&setter, 2, "current");
            self.cacheStat(manager, key, sol::make_object(lua.unsafeState(), 42));
            model.deregisterLiveCellRef(live);
            EXPECT_THROW(scripts.applyStatsCache(), std::runtime_error);
            EXPECT_TRUE(writes.empty());
            EXPECT_EQ(self.getCachedStat(key), nullptr);
        });
    }
    TEST(LuaObjectListsTest, RetentionPreservesPackageVectorsMembershipAndPendingUpdates)
    {
        withScripts([](auto&, auto&, auto&, auto&, auto& live) {
            MWWorld::Ptr ptr(&live);
            MWLua::ObjectLists lists;
            lists.objectAddedToScene(ptr);
            lists.setPlayer(ptr);
            const auto originalActors = lists.getActorsInScene();
            const auto originalPlayers = lists.getPlayers();
            EXPECT_TRUE(originalActors->empty()); // Pending update must remain pending while parked.
            auto retained = lists.retainRuntime();
            const auto incomingActors = retained.incomingLists().getActorsInScene();
            const auto incomingPlayers = retained.incomingLists().getPlayers();
            EXPECT_NE(originalActors, incomingActors);
            EXPECT_NE(originalPlayers, incomingPlayers);
            EXPECT_THROW(lists.update(), std::logic_error);
            EXPECT_THROW(lists.objectAddedToScene(ptr), std::logic_error);
            ASSERT_TRUE(retained.detach());
            EXPECT_EQ(lists.getActorsInScene(), incomingActors);
            EXPECT_EQ(lists.getPlayers(), incomingPlayers);
            EXPECT_THROW(retained.incomingLists(), std::logic_error);
            EXPECT_TRUE(incomingPlayers->empty());
            lists.objectAddedToScene(ptr);
            lists.update();
            EXPECT_EQ(*incomingActors, (std::vector<MWLua::ObjectId>{ MWLua::getId(ptr) }));
            lists.clear();
            EXPECT_TRUE(originalActors->empty());
            EXPECT_EQ(originalPlayers->size(), 1);
            ASSERT_TRUE(retained.exchange());
            EXPECT_EQ(lists.getActorsInScene(), originalActors);
            EXPECT_TRUE(originalActors->empty());
            ASSERT_TRUE(retained.restore());
            lists.update();
            EXPECT_EQ(*originalActors, (std::vector<MWLua::ObjectId>{ MWLua::getId(ptr) }));
            EXPECT_TRUE(incomingActors->empty());
            lists.objectRemovedFromScene(ptr);
            lists.update();
            EXPECT_TRUE(originalActors->empty());
        });
    }

    TEST(LuaObjectListsTest, RetentionScopeDiscardAndOwnerDeathKeepSelectedLists)
    {
        auto lists = std::make_unique<MWLua::ObjectLists>();
        const auto original = lists->getActorsInScene();
        {
            auto retained = lists->retainRuntime();
            ASSERT_TRUE(retained.detach());
            auto moved = std::move(retained);
            EXPECT_FALSE(retained.restore());
        }
        EXPECT_EQ(lists->getActorsInScene(), original);
        auto retained = lists->retainRuntime();
        const auto incoming = retained.incomingLists().getActorsInScene();
        ASSERT_TRUE(retained.detach());
        ASSERT_TRUE(retained.exchange());
        retained.discard();
        EXPECT_EQ(lists->getActorsInScene(), incoming);
        auto orphan = lists->retainRuntime();
        ASSERT_TRUE(orphan.detach());
        lists.reset();
        EXPECT_FALSE(orphan.restore());
        EXPECT_FALSE(orphan.exchange());
        EXPECT_THROW(orphan.incomingLists(), std::logic_error);
    }

    TEST(LuaManagerRetentionTest, QueuedActionsAndTeleportResumeInOrderWithoutReplay)
    {
        withScripts([](auto&, auto& manager, auto&, auto&, auto&) {
            std::vector<int> calls;
            manager.addAction([&] { calls.push_back(1); EXPECT_THROW(manager.retainRuntime(), std::logic_error); });
            manager.addAction([&] { calls.push_back(2); });
            manager.addTeleportPlayerAction([&] { calls.push_back(3); });
            auto retained = manager.retainRuntime();
            EXPECT_THROW(manager.addAction([] {}), std::logic_error);
            EXPECT_THROW(manager.applyDelayedActions(), std::logic_error);
            ASSERT_TRUE(retained.detach());
            manager.applyDelayedActions();
            EXPECT_TRUE(calls.empty());
            manager.addAction([&] { calls.push_back(9); });
            ASSERT_TRUE(retained.exchange());
            ASSERT_TRUE(retained.exchange());
            ASSERT_TRUE(retained.restore());
            EXPECT_TRUE(calls.empty());
            manager.applyDelayedActions();
            manager.applyDelayedActions();
            EXPECT_EQ(calls, (std::vector<int>{ 1, 2, 3 }));
        });
    }

    TEST(LuaManagerRetentionTest, ScopeDiscardAndOwnerDeathRetireTheCorrectQueues)
    {
        VFS::Manager vfs;
        auto manager = std::make_unique<MWLua::LuaManager>(&vfs, std::filesystem::path{});
        int old = 0, incoming = 0;
        manager->addAction([&] { ++old; });
        {
            auto retained = manager->retainRuntime();
            ASSERT_TRUE(retained.detach());
            manager->addAction([&] { ++incoming; });
            auto moved = std::move(retained);
        }
        manager->applyDelayedActions();
        EXPECT_EQ(old, 1); EXPECT_EQ(incoming, 0);
        auto retained = manager->retainRuntime();
        ASSERT_TRUE(retained.detach());
        manager->addAction([&] { ++incoming; });
        ASSERT_TRUE(retained.exchange());
        retained.discard();
        manager->applyDelayedActions();
        EXPECT_EQ(incoming, 1);
        auto orphan = manager->retainRuntime();
        ASSERT_TRUE(orphan.detach());
        manager.reset();
        EXPECT_FALSE(orphan.restore()); EXPECT_FALSE(orphan.exchange());
    }

}

namespace
{
    TEST(LuaObjectListsTest, PreparedGroupPublishesMembershipWithoutRegisteringOrUpdatingBorrowedVectorsEarly)
    {
        withScripts([](auto&,auto&,auto&,auto& model,auto& first) {
            auto second=first; second.mRef.unsetRefNum();
            const std::array<MWWorld::Ptr,2> refs{{MWWorld::Ptr(&first),MWWorld::Ptr(&second)}};
            model.registerPtr(refs[1]);
            const auto revision=model.getPtrRegistryRevision();
            MWLua::ObjectLists lists;
            const auto view=lists.getActorsInScene();
            { auto cancelled=lists.prepareSceneAdditions(refs); ASSERT_TRUE(cancelled->isValid()); }
            lists.update(); EXPECT_TRUE(view->empty());
            auto group=lists.prepareSceneAdditions(refs); ASSERT_TRUE(group->isValid());
            EXPECT_TRUE(view->empty()); ASSERT_TRUE(group->commit());
            EXPECT_EQ(model.getPtrRegistryRevision(),revision);
            EXPECT_TRUE(view->empty()); // Ordinary per-frame list publication remains deferred.
            lists.update(); EXPECT_EQ(*view,(std::vector<MWLua::ObjectId>{MWLua::getId(refs[0]),MWLua::getId(refs[1])}));
            EXPECT_FALSE(group->isValid()); EXPECT_FALSE(group->commit());
            EXPECT_THROW(lists.prepareSceneAdditions(refs),std::invalid_argument);
        });
    }
    TEST(LuaObjectListsTest, PreparedGroupRejectsDuplicateChangedMembershipAndClearedOwners)
    {
        withScripts([](auto&,auto&,auto&,auto&,auto& live) {
            const std::array<MWWorld::Ptr,1> refs{{MWWorld::Ptr(&live)}};
            const std::array<MWWorld::Ptr,2> duplicate{{refs[0],refs[0]}};
            MWLua::ObjectLists lists;
            EXPECT_THROW(lists.prepareSceneAdditions(duplicate),std::invalid_argument);
            auto pending=lists.prepareSceneAdditions(refs);
            lists.objectAddedToScene(refs[0]); EXPECT_FALSE(pending->isValid()); EXPECT_FALSE(pending->commit());
            lists.clear();
            auto cleared=lists.prepareSceneAdditions(refs); lists.clear();
            EXPECT_FALSE(cleared->isValid()); EXPECT_FALSE(cleared->commit());
            lists.update(); EXPECT_TRUE(lists.getActorsInScene()->empty());
        });
    }
    TEST(LuaObjectListsTest, PreparedMembershipTracksRetainedSelectionAndReferenceOrOwnerDestruction)
    {
        std::unique_ptr<MWLua::ObjectLists::PreparedSceneAdditions> orphan;
        auto lists=std::make_unique<MWLua::ObjectLists>();
        withScripts([&](auto&,auto&,auto&,auto&,auto& live) {
            const std::array<MWWorld::Ptr,1> refs{{MWWorld::Ptr(&live)}};
            auto pending=lists->prepareSceneAdditions(refs);
            auto retained=lists->retainRuntime(); EXPECT_FALSE(pending->isValid());
            ASSERT_TRUE(retained.detach()); EXPECT_FALSE(pending->isValid());
            ASSERT_TRUE(retained.restore()); EXPECT_TRUE(pending->isValid());
            orphan=std::move(pending);
        });
        EXPECT_FALSE(orphan->isValid()); EXPECT_FALSE(orphan->commit());
        lists.reset(); EXPECT_FALSE(orphan->isValid()); EXPECT_FALSE(orphan->commit());
    }
}
