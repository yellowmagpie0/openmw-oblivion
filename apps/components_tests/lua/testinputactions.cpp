#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <components/lua/inputactions.hpp>
#include <components/lua/scriptscontainer.hpp>
#include <components/testing/util.hpp>

namespace
{
    using namespace testing;
    using namespace TestingOpenMW;

    TEST(LuaInputActionsTest, MultiTree)
    {
        {
            LuaUtil::InputAction::MultiTree tree;
            auto a = tree.insert();
            auto b = tree.insert();
            auto c = tree.insert();
            auto d = tree.insert();
            EXPECT_TRUE(tree.multiEdge(c, { a, b }));
            EXPECT_TRUE(tree.multiEdge(a, { d }));
            EXPECT_FALSE(tree.multiEdge(d, { c }));
        }

        {
            LuaUtil::InputAction::MultiTree tree;
            auto a = tree.insert();
            auto b = tree.insert();
            auto c = tree.insert();
            EXPECT_TRUE(tree.multiEdge(b, { a }));
            EXPECT_TRUE(tree.multiEdge(c, { a, b }));
        }
    }

    TEST(LuaInputActionsTest, SwappingRegistriesPreservesValuesDependenciesAndCallbacks)
    {
        sol::state lua;
        lua["calls"] = 0;
        LuaUtil::InputAction::Registry original, incoming;
        const auto insert = [&](auto& registry, const char* key) {
            registry.insert({key, LuaUtil::InputAction::Type::Boolean, "test", key, "description",
                sol::make_object(lua, false), false});
        };
        insert(original, "a");
        insert(original, "b");
        insert(incoming, "c");
        const auto callback = [&](const char* source) {
            sol::table hidden(lua, sol::create);
            hidden[LuaUtil::ScriptsContainer::sScriptIdKey] = LuaUtil::ScriptId{};
            return LuaUtil::Callback({lua.load(source)(), hidden});
        };
        ASSERT_TRUE(original.bind("a", callback("return function() return true end"), {}));
        ASSERT_TRUE(original.bind("b", callback("return function(_, _, a) return a end"), {"a"}));
        original.registerHandler("b", callback("return function() calls = calls + 1 end"));
        original.update(1);
        ASSERT_EQ(lua["calls"].get<int>(), 1);

        using std::swap;
        swap(original, incoming);
        EXPECT_EQ(original.firstKey(), "c");
        EXPECT_EQ(incoming.firstKey(), "a");
        EXPECT_EQ(incoming.nextKey("a"), "b");
        ASSERT_TRUE(incoming["b"].has_value());
        EXPECT_EQ(incoming["b"]->mName, "b");
        EXPECT_TRUE(incoming.valueOfType("b", LuaUtil::InputAction::Type::Boolean).as<bool>());
        ASSERT_TRUE(incoming.bind("a", callback("return function() return false end"), {}));
        incoming.update(1);
        EXPECT_FALSE(incoming.valueOfType("b", LuaUtil::InputAction::Type::Boolean).as<bool>());
        EXPECT_EQ(lua["calls"].get<int>(), 2);

        LuaUtil::InputTrigger::Registry triggers, otherTriggers;
        triggers.insert({"old", "test", "old name", "description", false});
        otherTriggers.insert({"new", "test", "new name", "description", false});
        triggers.registerHandler("old", callback("return function() calls = calls + 10 end"));
        swap(triggers, otherTriggers);
        EXPECT_EQ(triggers.firstKey(), "new");
        ASSERT_TRUE(otherTriggers["old"].has_value());
        EXPECT_EQ(otherTriggers["old"]->mName, "old name");
        otherTriggers.activate("old");
        EXPECT_EQ(lua["calls"].get<int>(), 12);
    }

    TEST(LuaInputActionsTest, Registry)
    {
        sol::state lua;
        LuaUtil::InputAction::Registry registry;
        LuaUtil::InputAction::Info a({ "a", LuaUtil::InputAction::Type::Boolean, "test", "a_name", "a_description",
            sol::make_object(lua, false), false });
        registry.insert(a);
        LuaUtil::InputAction::Info b({ "b", LuaUtil::InputAction::Type::Boolean, "test", "b_name", "b_description",
            sol::make_object(lua, false), false });
        registry.insert(b);
        LuaUtil::Callback bindA({ lua.load("return function() return true end")(), sol::table(lua, sol::create) });
        LuaUtil::Callback bindBToA(
            { lua.load("return function(_, _, aValue) return aValue end")(), sol::table(lua, sol::create) });
        EXPECT_TRUE(registry.bind("a", bindA, {}));
        EXPECT_TRUE(registry.bind("b", bindBToA, { "a" }));
        registry.update(1.0);
        sol::object bValue = registry.valueOfType("b", LuaUtil::InputAction::Type::Boolean);
        EXPECT_TRUE(bValue.is<bool>());
        LuaUtil::Callback badA(
            { lua.load("return function() return 'not_a_bool' end")(), sol::table(lua, sol::create) });
        EXPECT_TRUE(registry.bind("a", badA, {}));
        testing::internal::CaptureStderr();
        registry.update(1.0);
        sol::object aValue = registry.valueOfType("a", LuaUtil::InputAction::Type::Boolean);
        EXPECT_TRUE(aValue.is<bool>());
        bValue = registry.valueOfType("b", LuaUtil::InputAction::Type::Boolean);
        EXPECT_TRUE(bValue.is<bool>() && bValue.as<bool>() == aValue.as<bool>());
    }
}
