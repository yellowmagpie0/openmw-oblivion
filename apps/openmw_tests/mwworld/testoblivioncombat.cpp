#include <apps/openmw/mwmechanics/oblivioncombat.hpp>
#include <components/esm4/runtimestate.hpp>

#include <gtest/gtest.h>

#include <stdexcept>

namespace
{
    ESM4::RuntimeState savedState(std::uint32_t version = ESM4::CurrentRuntimeStateVersion)
    {
        ESM4::RuntimeState state;
        state.mVersion = version;
        state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        state.mPlayer.mCell = ESM::FormKey::content("oblivion.esm", 1);
        if (version >= 3)
        {
            state.mPlayer.mRace = ESM::FormKey::content("oblivion.esm", 2);
            state.mPlayer.mClass = ESM::FormKey::content("oblivion.esm", 3);
        }
        return state;
    }
}

TEST(OblivionCombatService, SaveRestoreAndClearRetainActionOwnership)
{
    MWMechanics::OblivionCombatService service;
    const auto first = service.allocateAction();
    const auto second = service.allocateAction();
    ASSERT_TRUE(service.consumeAction(second));
    auto saved = savedState();
    service.capture(saved);
    MWMechanics::OblivionCombatService restored;
    restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
    EXPECT_TRUE(restored.isActionPending(first));
    EXPECT_TRUE(restored.isActionConsumed(second));
    EXPECT_FALSE(restored.consumeAction(second));
    EXPECT_TRUE(restored.consumeAction(first));
    EXPECT_EQ(restored.allocateAction(), 3);
    restored.clear();
    restored.capture(saved);
    EXPECT_EQ(saved.mPhysicalActions, ESM4::ActionLedgerState{});
    EXPECT_FALSE(restored.isActionConsumed(first));
    EXPECT_EQ(restored.allocateAction(), 1);
}

TEST(OblivionCombatService, InvalidOrForeignRestoreDoesNotChangeLiveActions)
{
    MWMechanics::OblivionCombatService service;
    const auto first = service.allocateAction();
    auto invalid = savedState();
    invalid.mProfile = ESM::GameProfile::Morrowind;
    EXPECT_THROW(service.restore(invalid), std::runtime_error);
    const auto beforeCapture = invalid;
    EXPECT_THROW(service.capture(invalid), std::invalid_argument);
    EXPECT_EQ(invalid, beforeCapture);
    invalid = savedState();
    invalid.mPhysicalActions = {3, {1, 1}};
    EXPECT_THROW(service.restore(invalid), std::runtime_error);
    EXPECT_TRUE(service.isActionPending(first));
    EXPECT_EQ(service.allocateAction(), 2);
    auto old = savedState(7);
    EXPECT_THROW(service.capture(old), std::invalid_argument);
    EXPECT_EQ(old.mPhysicalActions, ESM4::ActionLedgerState{});
}

TEST(OblivionCombatService, EveryLegacySchemaStartsANewActionNamespace)
{
    MWMechanics::OblivionCombatService service;
    for (std::uint32_t version = 1; version < 8; ++version)
    {
        SCOPED_TRACE(version);
        service.allocateAction();
        auto old = savedState(version);
        if (version >= 2)
            old.mScriptEventSequence = 12345;
        service.restore(ESM4::RuntimeState::deserializeBinary(old.serializeBinary()));
        EXPECT_EQ(service.allocateAction(), 1);
        auto current = savedState();
        service.capture(current);
        EXPECT_EQ(current.mPhysicalActions, (ESM4::ActionLedgerState{2, {1}}));
    }
}

TEST(OblivionCombatService, NativeValuesAndActionsRestoreTogetherOrRemainUnchanged)
{
    auto state = savedState();
    ESM4::RuntimeActorValues actor;
    actor.mActor = state.mPlayer.mReference;
    actor.mBase = ESM::FormKey::content("oblivion.esm", 7);
    actor.mOwner = ESM4::ActorValueOwner::Player;
    actor.mValues[8] = {100, {10, 5, -20}};
    state.mNativeActorValues = {actor};
    state.mPhysicalActions = {3, {1}};
    MWMechanics::OblivionCombatService service;
    service.restore(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()));
    ASSERT_NE(service.findActorValues(actor.mActor), nullptr);
    EXPECT_EQ(*service.findActorValues(actor.mActor), actor);
    EXPECT_EQ(service.findActorValues(ESM::FormKey::content("missing.esm", 1)), nullptr);
    auto invalid = state;
    invalid.mNativeActorValues.push_back(actor);
    invalid.mPhysicalActions = {20, {19}};
    EXPECT_THROW(service.restore(invalid), std::runtime_error);
    EXPECT_EQ(*service.findActorValues(actor.mActor), actor);
    EXPECT_TRUE(service.isActionPending(1));
    EXPECT_FALSE(service.isActionPending(19));
    auto captured = savedState();
    service.capture(captured);
    EXPECT_EQ(captured.mNativeActorValues, state.mNativeActorValues);
    EXPECT_EQ(captured.mPhysicalActions, state.mPhysicalActions);
    auto old = savedState(8);
    EXPECT_THROW(service.capture(old), std::invalid_argument);
    EXPECT_TRUE(old.mNativeActorValues.empty());
    EXPECT_EQ(old.mPhysicalActions, ESM4::ActionLedgerState{});
    service.restore(old);
    EXPECT_EQ(service.findActorValues(actor.mActor), nullptr);
    service.restore(state);
    service.clear();
    service.capture(captured);
    EXPECT_TRUE(captured.mNativeActorValues.empty());
    EXPECT_EQ(captured.mPhysicalActions, ESM4::ActionLedgerState{});
}
