#include <filesystem>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <components/lua/asyncpackage.hpp>
#include <components/lua/storage.hpp>

namespace
{
    using namespace testing;

    template <typename T>
    T get(sol::state_view& lua, std::string luaCode)
    {
        return lua.safe_script("return " + luaCode).get<T>();
    }

    TEST(LuaUtilStorageTest, Subscribe)
    {
        // Note: LuaUtil::Callback can be used only if Lua is initialized via LuaUtil::LuaState
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;
            storage.setActive(true);

            sol::table callbackHiddenData(lua, sol::create);
            callbackHiddenData[LuaUtil::ScriptsContainer::sScriptIdKey] = LuaUtil::ScriptId{};
            LuaUtil::getAsyncPackageInitializer(
                lua.lua_state(), []() { return 0.0; }, []() { return 0.0; })(callbackHiddenData);
            lua["async"] = LuaUtil::AsyncPackageId{ nullptr, 0, callbackHiddenData };

            lua["mutable"] = storage.getMutableSection(lua, "test");
            lua["ro"] = storage.getReadOnlySection(lua, "test");

            lua.safe_script(R"(
                callbackCalls = {}
                ro:subscribe(async:callback(function(section, key)
                    table.insert(callbackCalls, section .. '_' .. (key or '*'))
                end))
            )");

            lua.safe_script("mutable:set('x', 5)");
            EXPECT_EQ(get<int>(lua, "mutable:get('x')"), 5);
            EXPECT_EQ(get<int>(lua, "ro:get('x')"), 5);

            EXPECT_THROW(lua.safe_script("ro:set('y', 3)"), std::exception);

            lua.safe_script("t1 = mutable:asTable()");
            lua.safe_script("t2 = ro:asTable()");
            EXPECT_EQ(get<int>(lua, "t1.x"), 5);
            EXPECT_EQ(get<int>(lua, "t2.x"), 5);

            lua.safe_script("mutable:reset()");
            EXPECT_TRUE(get<bool>(lua, "ro:get('x') == nil"));

            lua.safe_script("mutable:reset({x=4, y=7})");
            EXPECT_EQ(get<int>(lua, "ro:get('x')"), 4);
            EXPECT_EQ(get<int>(lua, "ro:get('y')"), 7);

            EXPECT_THAT(get<std::string>(lua, "table.concat(callbackCalls, ', ')"), "test_x, test_*, test_*");
        });
    }

    TEST(LuaUtilStorageTest, Table)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;
            auto& lua = view.sol();
            storage.setActive(true);
            lua["mutable"] = storage.getMutableSection(lua, "test");
            lua["ro"] = storage.getReadOnlySection(lua, "test");

            lua.safe_script("mutable:set('x', { y = 'abc', z = 7 })");
            EXPECT_EQ(get<int>(lua, "mutable:get('x').z"), 7);
            EXPECT_THROW(lua.safe_script("mutable:get('x').z = 3"), std::exception);
            EXPECT_NO_THROW(lua.safe_script("mutable:getCopy('x').z = 3"));
            EXPECT_EQ(get<int>(lua, "mutable:get('x').z"), 7);
            EXPECT_EQ(get<int>(lua, "ro:get('x').z"), 7);
            EXPECT_EQ(get<std::string>(lua, "ro:get('x').y"), "abc");
        });
    }

    TEST(LuaUtilStorageTest, Saving)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;
            auto& lua = view.sol();
            storage.setActive(true);

            lua["permanent"] = storage.getMutableSection(lua, "permanent");
            lua["temporary"] = storage.getMutableSection(lua, "temporary");
            lua.safe_script("temporary:removeOnExit()");
            lua.safe_script("permanent:set('x', 1)");
            lua.safe_script("temporary:set('y', 2)");

            const auto tmpFile = std::filesystem::temp_directory_path() / "test_storage.bin";
            storage.save(lua, tmpFile);
            EXPECT_EQ(get<int>(lua, "permanent:get('x')"), 1);
            EXPECT_EQ(get<int>(lua, "temporary:get('y')"), 2);

            storage.clearTemporaryAndRemoveCallbacks();
            lua["permanent"] = storage.getMutableSection(lua, "permanent");
            lua["temporary"] = storage.getMutableSection(lua, "temporary");
            EXPECT_EQ(get<int>(lua, "permanent:get('x')"), 1);
            EXPECT_TRUE(get<bool>(lua, "temporary:get('y') == nil"));

            lua.safe_script("permanent:set('x', 3)");
            lua.safe_script("permanent:set('z', 4)");

            LuaUtil::LuaStorage storage2;
            storage2.setActive(true);
            storage2.load(lua, tmpFile);
            lua["permanent"] = storage2.getMutableSection(lua, "permanent");
            lua["temporary"] = storage2.getMutableSection(lua, "temporary");

            EXPECT_EQ(get<int>(lua, "permanent:get('x')"), 1);
            EXPECT_TRUE(get<bool>(lua, "permanent:get('z') == nil"));
            EXPECT_TRUE(get<bool>(lua, "temporary:get('y') == nil"));
        });
    }

    TEST(LuaUtilStorageTest, RetainedRuntimePreservesViewsAndCallbacks)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            auto& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;
            storage.setActive(true);
            sol::table hidden(lua, sol::create);
            hidden[LuaUtil::ScriptsContainer::sScriptIdKey] = LuaUtil::ScriptId{};
            LuaUtil::getAsyncPackageInitializer(lua.lua_state(), [] { return 0.; }, [] { return 0.; })(hidden);
            lua["async"] = LuaUtil::AsyncPackageId{ nullptr, 0, hidden };
            lua["old"] = storage.getMutableSection(lua, "persistent");
            lua["temporary"] = storage.getMutableSection(lua, "temporary");
            lua["session"] = storage.getMutableSection(lua, "session");
            auto package = LuaUtil::LuaStorage::initGlobalPackage(view, &storage);
            lua["lifeTime"] = package["LIFE_TIME"];
            lua.safe_script(R"(
                old:set('x', { n = 7 })
                cached = old:get('x')
                temporary:removeOnExit()
                temporary:set('x', 8)
                session:setLifeTime(lifeTime.GameSession)
                session:set('x', 9)
                calls = 0
                old:subscribe(async:callback(function() calls = calls + 1 end))
            )");
            auto retained = storage.retainRuntime();
            EXPECT_THROW(storage.setActive(false), std::logic_error);
            EXPECT_THROW(storage.retainRuntime(), std::logic_error);
            ASSERT_TRUE(retained.detach());
            EXPECT_FALSE(retained.detach());
            EXPECT_THROW(lua.safe_script("old:get('x')"), std::exception);
            EXPECT_THROW(lua.safe_script("temporary:removeOnExit()"), std::exception);
            lua["incoming"] = storage.getMutableSection(lua, "persistent");
            lua["incomingTemporary"] = storage.getMutableSection(lua, "temporary");
            lua["incomingSession"] = storage.getMutableSection(lua, "session");
            EXPECT_EQ(get<int>(lua, "incoming:get('x').n"), 7);
            EXPECT_EQ(get<int>(lua, "incomingSession:get('x')"), 9);
            EXPECT_TRUE(get<bool>(lua, "incomingTemporary:get('x') == nil"));
            lua.safe_script("incoming:set('x', { n = 11 })");
            EXPECT_EQ(get<int>(lua, "calls"), 0);
            ASSERT_TRUE(retained.exchange());
            EXPECT_TRUE(get<bool>(lua, "old:get('x') == cached"));
            EXPECT_EQ(get<int>(lua, "old:get('x').n"), 7);
            EXPECT_EQ(get<int>(lua, "temporary:get('x')"), 8);
            EXPECT_THROW(lua.safe_script("incoming:get('x')"), std::exception);
            ASSERT_TRUE(retained.exchange());
            ASSERT_TRUE(retained.restore());
            EXPECT_FALSE(retained.restore());
            EXPECT_THROW(lua.safe_script("incoming:set('x', 1)"), std::exception);
            lua.safe_script("old:set('x', 12)");
            EXPECT_EQ(get<int>(lua, "calls"), 1);
        });
    }

    TEST(LuaUtilStorageTest, RetainedRuntimeDiscardAndScopeRestore)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            auto& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;
            storage.setActive(true);
            lua["old"] = storage.getMutableSection(lua, "test");
            lua.safe_script("old:set('x', 1)");
            {
                auto retained = storage.retainRuntime();
                ASSERT_TRUE(retained.detach());
                lua["abandoned"] = storage.getMutableSection(lua, "test");
                lua.safe_script("abandoned:set('x', 2)");
                auto moved = std::move(retained);
                EXPECT_FALSE(retained.restore());
            }
            EXPECT_EQ(get<int>(lua, "old:get('x')"), 1);
            EXPECT_THROW(lua.safe_script("abandoned:get('x')"), std::exception);
            auto retained = storage.retainRuntime();
            ASSERT_TRUE(retained.detach());
            lua["incoming"] = storage.getMutableSection(lua, "test");
            lua.safe_script("incoming:set('x', 3)");
            ASSERT_TRUE(retained.exchange());
            retained.discard();
            EXPECT_EQ(get<int>(lua, "incoming:get('x')"), 3);
            EXPECT_THROW(lua.safe_script("old:get('x')"), std::exception);
        });
    }

    TEST(LuaUtilStorageTest, RetainedRuntimeOwnerDeathInvalidatesErasedViews)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            auto& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            auto storage = std::make_unique<LuaUtil::LuaStorage>();
            storage->setActive(true);
            lua["erased"] = storage->getMutableSection(lua, "temporary");
            lua.safe_script("erased:removeOnExit()");
            storage->clearTemporaryAndRemoveCallbacks();
            auto retained = storage->retainRuntime();
            ASSERT_TRUE(retained.detach());
            EXPECT_THROW(lua.safe_script("erased:set('x', 1)"), std::exception);
            storage.reset();
            EXPECT_FALSE(retained.restore());
            EXPECT_FALSE(retained.exchange());
            EXPECT_THROW(lua.safe_script("erased:setLifeTime(0)"), std::exception);
            EXPECT_THROW(lua.safe_script("erased:get('x')"), std::exception);
        });
    }

    TEST(LuaUtilStorageTest, RetentionRequiresIdleCallbacks)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            auto& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;
            storage.setActive(true);
            sol::table hidden(lua, sol::create);
            hidden[LuaUtil::ScriptsContainer::sScriptIdKey] = LuaUtil::ScriptId{};
            LuaUtil::getAsyncPackageInitializer(lua.lua_state(), [] { return 0.; }, [] { return 0.; })(hidden);
            lua["async"] = LuaUtil::AsyncPackageId{ nullptr, 0, hidden };
            lua["section"] = storage.getMutableSection(lua, "test");
            int attempts = 0;
            lua["tryRetain"] = [&] {
                ++attempts;
                EXPECT_THROW(storage.retainRuntime(), std::logic_error);
            };
            // Menu scripts may subscribe and set lifetimes while game storage is inactive.
            storage.setActive(false);
            lua.safe_script("section:removeOnExit(); section:setLifeTime(0); section:subscribe(async:callback(tryRetain))");
            storage.setActive(true);
            lua.safe_script("section:set('x', 1)");
            EXPECT_EQ(attempts, 1);
            auto retained = storage.retainRuntime();
            ASSERT_TRUE(retained.detach());
            lua["incoming"] = storage.getMutableSection(lua, "test");
            lua.safe_script("incoming:set('x', 2)");
            EXPECT_EQ(attempts, 1);
            ASSERT_TRUE(retained.restore());
            lua.safe_script("section:set('x', 3)");
            EXPECT_EQ(attempts, 2);
        });
    }

}
