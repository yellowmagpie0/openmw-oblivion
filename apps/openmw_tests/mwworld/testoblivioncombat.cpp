#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadachr.hpp>
#include "apps/openmw/mwworld/esmstore.hpp"
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
    actor.mPlayerFormValues = {{-1, 2, 3, 4}};
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
    auto v9 = savedState(9);
    EXPECT_THROW(service.capture(v9), std::invalid_argument);
    EXPECT_TRUE(v9.mNativeActorValues.empty());
    EXPECT_EQ(v9.mPhysicalActions, ESM4::ActionLedgerState{});
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

TEST(OblivionCombatService, SavedActorQueriesValidateWinningContentWithoutAWorld)
{
    auto state = savedState();
    MWWorld::ESMStore store;
    ESM4::Npc npc{};
    npc.mId = {0x800, 2};
    npc.mFormKey = ESM::FormKey::content("actors.esm", 0x800);
    npc.mIsTES4 = true;
    store.getWritable<ESM4::Npc>().insertStatic(npc, npc.mFormKey);
    ESM4::ActorCharacter reference{};
    reference.mId = {0x900, 2};
    reference.mFormKey = ESM::FormKey::content("actors.esm", 0x900);
    reference.mBaseKey = npc.mFormKey;
    store.getWritable<ESM4::ActorCharacter>().insertStatic(reference, reference.mFormKey);
    ESM4::RuntimeReferenceState savedRef;
    savedRef.mKey = reference.mFormKey;
    savedRef.mBase = npc.mFormKey;
    savedRef.mCell = state.mPlayer.mCell;
    state.mReferences = {savedRef};
    ESM4::RuntimeActorValues values;
    values.mActor = reference.mFormKey;
    values.mBase = npc.mFormKey;
    values.mValues[0] = {100.75f, {.5f, -.5f, std::nullopt}};
    values.mValues[11].mBase = 12.75f;
    state.mNativeActorValues = {values};
    state.mPhysicalActions = {3, {1}};
    MWMechanics::OblivionCombatService service;
    service.restore(state, store); // No Environment, World, CellStore or live Ptr.
    EXPECT_EQ(service.getNonPlayerValue(values.mActor, 0, store), 100.75f);
    EXPECT_EQ(service.getNonPlayerIntegerValue(values.mActor, 0, store), 99);
    EXPECT_EQ(service.getNonPlayerBaseValue(values.mActor, 0, store), 100);
    EXPECT_EQ(service.getNonPlayerBaseValue(values.mActor, 11, store), 12);
    EXPECT_THROW(service.getNonPlayerValue(values.mActor, 11, store), std::invalid_argument);
    EXPECT_THROW(service.getNonPlayerIntegerValue(values.mActor, 48, store), std::invalid_argument);
    EXPECT_THROW(service.getNonPlayerBaseValue(values.mActor, 72, store), std::invalid_argument);
    EXPECT_THROW(service.getNonPlayerValue(state.mPlayer.mReference, 0, store), std::invalid_argument);
    auto replacement = state;
    replacement.mPhysicalActions = {100, {99}};
    const auto reject = [&](const MWWorld::ESMStore& invalidStore) {
        EXPECT_THROW(service.restore(replacement, invalidStore), std::invalid_argument);
        EXPECT_THROW(service.getNonPlayerValue(values.mActor, 0, invalidStore), std::invalid_argument);
        EXPECT_TRUE(service.isActionPending(1));
        EXPECT_FALSE(service.isActionPending(99));
        EXPECT_EQ(*service.findActorValues(values.mActor), values);
    };
    MWWorld::ESMStore missing;
    reject(missing);
    ASSERT_TRUE(store.getWritable<ESM4::ActorCharacter>().eraseStatic(reference.mFormKey));
    reject(store);
    auto mismatch = reference;
    mismatch.mBaseKey = ESM::FormKey::content("actors.esm", 0x801);
    store.getWritable<ESM4::ActorCharacter>().insertStatic(mismatch, reference.mFormKey);
    reject(store);
    store.getWritable<ESM4::ActorCharacter>().insertStatic(reference, reference.mFormKey);
    ESM4::Creature ambiguous{};
    ambiguous.mId = npc.mId;
    ambiguous.mFormKey = npc.mFormKey;
    ambiguous.mAttackReach = 64;
    store.getWritable<ESM4::Creature>().insertStatic(ambiguous, npc.mFormKey);
    reject(store);
    ASSERT_TRUE(store.getWritable<ESM4::Creature>().eraseStatic(npc.mFormKey));
    npc.mIsTES4 = false;
    store.getWritable<ESM4::Npc>().insertStatic(npc, npc.mFormKey);
    reject(store);
    npc.mIsTES4 = true;
    store.getWritable<ESM4::Npc>().insertStatic(npc, npc.mFormKey);
    state.mNativeActorValues[0].mActor = ESM::FormKey::dynamic("spawned", 5);
    state.mReferences[0].mKey = state.mNativeActorValues[0].mActor;
    service.restore(state, store); // Dynamic references have no content ACHR.
    EXPECT_EQ(service.getNonPlayerValue(state.mReferences[0].mKey, 0, store), 100.75f);
}

TEST(OblivionCombatService, SharedBasesRestoreCaptureAndClearWithoutLosingTypes)
{
    auto state = savedState();
    state.mNativeActorBases = {{ESM::FormKey::content("actors.esm", 0x800), ESM4::ActorBaseKind::Npc,
        {{8, std::int32_t{16777217}}, {40, 2147483648.f}}}};
    state.mPhysicalActions = {3, {1}};
    MWMechanics::OblivionCombatService service;
    service.restore(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()));
    auto captured = savedState();
    service.capture(captured);
    EXPECT_EQ(captured.mNativeActorBases, state.mNativeActorBases);
    EXPECT_EQ(captured.mPhysicalActions, state.mPhysicalActions);
    auto old = savedState(10);
    const auto unchanged = old;
    EXPECT_THROW(service.capture(old), std::invalid_argument);
    EXPECT_EQ(old, unchanged);
    auto invalid = state;
    invalid.mNativeActorBases.push_back(invalid.mNativeActorBases[0]);
    invalid.mPhysicalActions = {10, {9}};
    EXPECT_THROW(service.restore(invalid), std::runtime_error);
    service.capture(captured);
    EXPECT_EQ(captured.mNativeActorBases, state.mNativeActorBases);
    EXPECT_EQ(captured.mPhysicalActions, state.mPhysicalActions);
    service.restore(savedState(10));
    service.capture(captured);
    EXPECT_TRUE(captured.mNativeActorBases.empty());
    service.restore(state);
    service.clear();
    service.capture(captured);
    EXPECT_TRUE(captured.mNativeActorBases.empty());
}

TEST(OblivionCombatService, SharedBaseContentPreflightIsAtomicAndRequiresCorrectNativeKind)
{
    MWWorld::ESMStore store;
    ESM4::Npc npc{};
    npc.mId = {0x800, 2};
    npc.mFormKey = ESM::FormKey::content("actors.esm", 0x800);
    npc.mIsTES4 = true;
    store.getWritable<ESM4::Npc>().insertStatic(npc, npc.mFormKey);
    auto state = savedState();
    state.mNativeActorBases = {{npc.mFormKey, ESM4::ActorBaseKind::Npc, {{8, std::int32_t{16777217}}}}};
    state.mPhysicalActions = {3, {1}};
    MWMechanics::OblivionCombatService service;
    service.restore(state, store);
    auto replacement = state;
    replacement.mPhysicalActions = {10, {9}};
    const auto reject = [&](const MWWorld::ESMStore& content) {
        EXPECT_THROW(service.restore(replacement, content), std::invalid_argument);
        auto captured = savedState();
        service.capture(captured);
        EXPECT_EQ(captured.mNativeActorBases, state.mNativeActorBases);
        EXPECT_EQ(captured.mPhysicalActions, state.mPhysicalActions);
    };
    MWWorld::ESMStore missing;
    reject(missing);
    replacement.mNativeActorBases[0].mKind = ESM4::ActorBaseKind::Creature;
    reject(store);
    replacement = state;
    ESM4::Creature creature{};
    creature.mId = npc.mId;
    creature.mFormKey = npc.mFormKey;
    creature.mAttackReach = 1;
    store.getWritable<ESM4::Creature>().insertStatic(creature, creature.mFormKey);
    reject(store); // Ambiguous across native base kinds.
    ASSERT_TRUE(store.getWritable<ESM4::Npc>().eraseStatic(npc.mFormKey));
    replacement.mNativeActorBases[0].mKind = ESM4::ActorBaseKind::Creature;
    EXPECT_NO_THROW(service.restore(replacement, store));
    replacement.mNativeActorBases[0].mBase = ESM::FormKey::dynamic("player-base", 1);
    EXPECT_THROW(service.restore(replacement, store), std::invalid_argument);
    replacement.mNativeActorBases[0].mKind = ESM4::ActorBaseKind::Npc;
    EXPECT_NO_THROW(service.restore(replacement, store));
}

TEST(OblivionCombatService, ScriptQueriesDistinguishLiveFloatDisabledFormAndFlooredBase)
{
    auto state = savedState();
    MWWorld::ESMStore store;
    ESM4::Creature creature{};
    creature.mId = {0x800, 2};
    creature.mFormKey = ESM::FormKey::content("actors.esm", 0x800);
    creature.mAttackReach = 1;
    store.getWritable<ESM4::Creature>().insertStatic(creature, creature.mFormKey);
    ESM4::RuntimeActorValues npc;
    npc.mActor = ESM::FormKey::dynamic("query-creature", 1);
    npc.mBase = creature.mFormKey;
    npc.mValues[8] = {16777216.f, {0, .5f, 0}};
    npc.mValues[11].mBase = 17.75f;
    npc.mValues[12] = {21, {0, .75f, 0}};
    npc.mValues[28].mBase = 23;
    npc.mValues[37].mBase = 500;
    npc.mValues[48].mBase = 2;
    ESM4::RuntimeReferenceState reference;
    reference.mKey = npc.mActor;
    reference.mBase = npc.mBase;
    reference.mCell = state.mPlayer.mCell;
    state.mReferences.push_back(reference);
    ESM4::RuntimeActorValues player;
    player.mOwner = ESM4::ActorValueOwner::Player;
    player.mActor = ESM::FormKey::dynamic("player", 1);
    player.mBase = ESM::FormKey::dynamic("player-base", 1);
    player.mPlayerFormValues = {{7, 3, 9, -3}};
    player.mValues[8] = {87, {10, .5f, -2}};
    player.mValues[9] = {100, {20, 0, 0}};
    player.mValues[11].mBase = 247;
    state.mNativeActorValues = {player, npc};
    state.mNativeActorBases = {{npc.mBase, ESM4::ActorBaseKind::Creature, {{8, std::int32_t{16777217}}}}};
    MWMechanics::OblivionCombatService service;
    service.restore(state, store);
    EXPECT_EQ(service.getScriptActorValue(npc.mActor, 8, false, false, store), 16777218.);
    EXPECT_EQ(service.getScriptActorValue(npc.mActor, 8, true, false, store), 16777216.);
    EXPECT_EQ(service.getScriptActorValue(npc.mActor, 8, false, true, store), 16777217.);
    EXPECT_EQ(service.getScriptActorValue(npc.mActor, 8, true, true, store), 16777216.);
    EXPECT_EQ(service.getScriptActorValue(npc.mActor, 28, false, false, store), 21.75);
    EXPECT_EQ(service.getScriptActorValue(npc.mActor, 28, false, true, store), 23);
    EXPECT_EQ(service.getScriptActorValue(npc.mActor, 11, false, true, store), 17);
    EXPECT_EQ(service.getScriptActorValue(npc.mActor, 37, false, true, store), 0);
    EXPECT_EQ(service.getScriptActorValue(npc.mActor, 48, false, true, store), 2);
    EXPECT_THROW(service.getScriptActorValue(npc.mActor, 48, false, false, store), std::invalid_argument);
    EXPECT_EQ(service.getScriptActorValue(player.mActor, 8, false, false, store), 95.5);
    EXPECT_EQ(service.getScriptActorValue(player.mActor, 8, true, false, store), 87);
    EXPECT_EQ(service.getScriptActorValue(player.mActor, 8, false, true, store), 7);
    EXPECT_EQ(service.getScriptActorValue(player.mActor, 8, true, true, store), 87);
    EXPECT_EQ(service.getScriptActorValue(player.mActor, 9, false, true, store), 3);
    EXPECT_EQ(service.getScriptActorValue(player.mActor, 11, false, true, store), -3);
    EXPECT_EQ(service.getScriptActorValue(player.mActor, 11, true, true, store), 247);
    EXPECT_THROW(service.getScriptActorValue(player.mActor, 11, false, false, store), std::invalid_argument);
    EXPECT_THROW(service.getScriptActorValue(player.mActor, 255, false, false, store), std::invalid_argument);
    MWWorld::ESMStore missing;
    EXPECT_THROW(service.getScriptActorValue(npc.mActor, 8, false, true, missing), std::invalid_argument);
    auto after = savedState();
    service.capture(after);
    EXPECT_EQ(after.mNativeActorValues, state.mNativeActorValues);
    EXPECT_EQ(after.mNativeActorBases, state.mNativeActorBases);
}

TEST(OblivionCombatService, LifeAndDeathQueueRestoreAtomicallyAndConsumeBeforeCallbackSave)
{
    auto saved = savedState();
    const auto player = saved.mPlayer.mReference;
    saved.mNativeActorLife = {{player, ESM::FormKey::dynamic("player-base", 1), ESM4::ActorLifePhase::Dead, 0, {}}};
    saved.mNextDeathEvent = 7;
    saved.mPendingDeathEvents = {{2, player, {}}, {6, player, player}};
    saved.mPhysicalActions = {3, {1}};
    MWMechanics::OblivionCombatService service;
    MWWorld::ESMStore store;
    service.restore(saved, store);
    ASSERT_NE(service.findActorLife(player), nullptr);
    EXPECT_EQ(service.findActorLife(player)->mPhase, ESM4::ActorLifePhase::Dead);
    auto invalid = saved;
    invalid.mNativeActorLife[0].mRecoveryRemaining = 1;
    EXPECT_THROW(service.restore(invalid, store), std::runtime_error);
    auto check = savedState();
    service.capture(check);
    EXPECT_EQ(check, saved);
    const auto first = service.takeNextDeathEvent();
    ASSERT_TRUE(first);
    EXPECT_EQ(first->mId, 2);
    // This capture is the state a callback-triggered save must observe.
    service.capture(check);
    EXPECT_EQ(check.mPendingDeathEvents, (std::vector<ESM4::RuntimeActorDeathEvent>{{6, player, player}}));
    EXPECT_EQ(check.mNextDeathEvent, 7);
    MWMechanics::OblivionCombatService restarted;
    restarted.restore(ESM4::RuntimeState::deserializeBinary(check.serializeBinary()), store);
    const auto second = restarted.takeNextDeathEvent();
    ASSERT_TRUE(second);
    EXPECT_EQ(second->mId, 6);
    EXPECT_EQ(second->mKiller, player);
    EXPECT_FALSE(restarted.takeNextDeathEvent());
    auto legacy = savedState(11);
    const auto original = legacy;
    EXPECT_THROW(restarted.capture(legacy), std::invalid_argument);
    EXPECT_EQ(legacy, original);
    restarted.capture(check);
    EXPECT_TRUE(check.mPendingDeathEvents.empty());
    EXPECT_EQ(check.mNextDeathEvent, 7); // Never recycle a consumed event ID.
    restarted.clear();
    restarted.capture(check);
    EXPECT_TRUE(check.mNativeActorLife.empty());
    EXPECT_EQ(check.mNextDeathEvent, 1);
    EXPECT_FALSE(restarted.findActorLife(player));
}

TEST(OblivionCombatService, LifeContentPreflightRejectsMissingActorsAndNonActorKillersBeforeCommit)
{
    auto saved = savedState();
    const auto actor = ESM::FormKey::content("actors.esm", 0x100);
    const auto base = ESM::FormKey::content("actors.esm", 0x200);
    ESM4::RuntimeReferenceState reference;
    reference.mKey = actor;
    reference.mBase = base;
    reference.mCell = saved.mPlayer.mCell;
    saved.mReferences.push_back(reference);
    saved.mNativeActorLife = {{actor, base, ESM4::ActorLifePhase::EssentialUnconscious, 4.5f, saved.mPlayer.mReference}};
    MWWorld::ESMStore store;
    ESM4::Npc npc{};
    npc.mFormKey = base;
    npc.mIsTES4 = true;
    store.getWritable<ESM4::Npc>().insertStatic(npc, base);
    ESM4::ActorCharacter placed{};
    placed.mFormKey = actor;
    placed.mBaseKey = base;
    store.getWritable<ESM4::ActorCharacter>().insertStatic(placed, actor);
    MWMechanics::OblivionCombatService service;
    service.restore(saved, store);
    EXPECT_EQ(service.findActorLife(actor)->mRecoveryRemaining, 4.5f);
    const auto action = service.allocateAction();
    auto before = saved;
    service.capture(before);
    MWWorld::ESMStore missing;
    EXPECT_THROW(service.restore(saved, missing), std::invalid_argument);
    auto invalid = saved;
    ESM4::RuntimeReferenceState source = reference;
    source.mKey = ESM::FormKey::content("actors.esm", 0x300);
    source.mBase = ESM::FormKey::content("actors.esm", 0x400);
    invalid.mReferences.push_back(source);
    invalid.mNativeActorLife[0].mKiller = source.mKey;
    EXPECT_NO_THROW(invalid.validate());
    EXPECT_THROW(service.restore(invalid, store), std::invalid_argument);
    auto after = saved;
    service.capture(after);
    EXPECT_EQ(after, before);
    EXPECT_TRUE(service.isActionPending(action));
}
