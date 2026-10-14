#include <gtest/gtest.h>

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

}
