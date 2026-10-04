#include <apps/openmw/mwclass/esm4npc.hpp>
#include <apps/openmw/mwworld/livecellref.hpp>
#include <components/esm4/projectilerules.hpp>
#include <limits>
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

TEST(OblivionCombat, deathCountsRequireWinningActorBasesAndClearWithoutInferringLife)
{
    ESM4::RuntimeState saved;
    saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
    saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
    saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
    saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
    const auto base = ESM::FormKey::content("actors.esm", 4);
    saved.mNativeDeathCounts[base] = 65535;
    MWWorld::ESMStore store;
    ESM4::Npc npc{};
    npc.mFormKey = base;
    npc.mIsTES4 = true;
    store.getWritable<ESM4::Npc>().insertStatic(npc, base);
    MWMechanics::OblivionCombatService service;
    service.restore(saved, store);
    EXPECT_EQ(service.getDeadCount(base), -1);
    EXPECT_EQ(service.getDeadCount({}), 0);
    auto invalid = saved;
    invalid.mNativeDeathCounts[ESM::FormKey::content("actors.esm", 5)] = 1;
    EXPECT_THROW(service.restore(invalid, store), std::invalid_argument);
    auto after = saved;
    service.capture(after);
    EXPECT_EQ(after, saved);
    after.mVersion = 12;
    EXPECT_THROW(service.capture(after), std::invalid_argument);
    EXPECT_EQ(service.getDeadCount(base), -1);
    service.clear();
    EXPECT_EQ(service.getDeadCount(base), 0);
    after = saved;
    service.capture(after);
    EXPECT_TRUE(after.mNativeDeathCounts.empty());
}

TEST(OblivionCombatService, NativeBreathCaptureRestoreRejectAndClear)
{
    auto state = savedState();
    ESM4::RuntimeActorValues values;
    values.mActor = state.mPlayer.mReference;
    values.mBase = ESM::FormKey::dynamic("player-base", 1);
    values.mOwner = ESM4::ActorValueOwner::Player;
    values.mPlayerFormValues = {{100, 30, 40, 0}};
    state.mNativeActorValues.push_back(values);
    state.mNativeActorBreath = {{values.mActor, .125f}};
    MWMechanics::OblivionCombatService service;
    EXPECT_FALSE(service.findActorBreath(values.mActor));
    service.restore(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()));
    EXPECT_EQ(service.findActorBreath(values.mActor), .125f);
    auto captured = savedState();
    service.capture(captured);
    EXPECT_EQ(captured, state);
    auto invalid = state;
    invalid.mNativeActorValues.clear();
    EXPECT_THROW(service.restore(invalid), std::runtime_error);
    service.capture(captured);
    EXPECT_EQ(captured, state);
    auto old = savedState(13);
    const auto before = old;
    EXPECT_THROW(service.capture(old), std::invalid_argument);
    EXPECT_EQ(old, before);
    service.clear();
    EXPECT_FALSE(service.findActorBreath(values.mActor));
    service.capture(captured);
    EXPECT_TRUE(captured.mNativeActorBreath.empty());
    service.restore(before);
    EXPECT_FALSE(service.findActorBreath(values.mActor));
}


namespace
{
    ESM4::RuntimeState combatMembershipState()
    {
        auto state = savedState();
        for (std::uint32_t i = 1; i <= 3; ++i)
        {
            ESM4::RuntimeReferenceState ref;
            ref.mKey = ESM::FormKey::content("actors.esm", i);
            ref.mBase = ESM::FormKey::content("actors.esm", 100 + i);
            ref.mCell = state.mPlayer.mCell;
            state.mReferences.push_back(ref);
            ESM4::RuntimeActorValues values;
            values.mActor = ref.mKey;
            values.mBase = ref.mBase;
            state.mNativeActorValues.push_back(values);
            state.mNativeActorLife.push_back({ref.mKey, ref.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        }
        return state;
    }
}

TEST(OblivionCombatService, OpponentsAreSymmetricIdempotentAndPersistAcrossReload)
{
    auto state = combatMembershipState();
    const auto a = state.mReferences[0].mKey, b = state.mReferences[1].mKey, c = state.mReferences[2].mKey;
    MWMechanics::OblivionCombatService service;
    service.restore(state);
    EXPECT_FALSE(service.isInCombat(a));
    EXPECT_TRUE(service.engage(a, b));
    EXPECT_FALSE(service.engage(b, a));
    EXPECT_TRUE(service.engage(c, b));
    EXPECT_TRUE(service.isInCombat(a));
    EXPECT_TRUE(service.isInCombatWith(b, a));
    EXPECT_FALSE(service.isInCombatWith(a, c));
    EXPECT_EQ(service.combatOpponents(b), (std::vector<ESM::FormKey>{a, c}));
    service.capture(state);
    EXPECT_EQ(state.mNativeCombatEngagements,
        (std::set<std::pair<ESM::FormKey, ESM::FormKey>>{{a, b}, {b, c}}));
    MWMechanics::OblivionCombatService restored;
    restored.restore(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()));
    EXPECT_EQ(restored.combatOpponents(b), service.combatOpponents(b));
    EXPECT_TRUE(restored.stopCombat(a));
    EXPECT_FALSE(restored.stopCombat(a));
    EXPECT_FALSE(restored.isInCombat(a));
    EXPECT_EQ(restored.combatOpponents(b), (std::vector<ESM::FormKey>{c}));
    EXPECT_TRUE(restored.isInCombat(c));
    restored.clear();
    EXPECT_FALSE(restored.isInCombat(b));
    EXPECT_TRUE(restored.combatOpponents(c).empty());
    restored.capture(state);
    EXPECT_TRUE(state.mNativeCombatEngagements.empty());
}

TEST(OblivionCombatService, InvalidEngagementAndRestoreLeaveExistingOpponentsUntouched)
{
    auto state = combatMembershipState();
    const auto a = state.mReferences[0].mKey, b = state.mReferences[1].mKey, c = state.mReferences[2].mKey;
    state.mNativeActorLife[2].mPhase = ESM4::ActorLifePhase::Dead;
    MWMechanics::OblivionCombatService service;
    service.restore(state);
    ASSERT_TRUE(service.engage(a, b));
    for (const auto& bad : {a, c, ESM::FormKey{}, ESM::FormKey::dynamic("missing", 1)})
        EXPECT_THROW(service.engage(a, bad), std::invalid_argument);
    auto invalid = state;
    invalid.mNativeCombatEngagements.emplace(a, c);
    EXPECT_THROW(service.restore(invalid), std::runtime_error);
    EXPECT_TRUE(service.isInCombatWith(a, b));
    EXPECT_FALSE(service.isInCombat(c));
    auto old = savedState(14);
    const auto before = old;
    EXPECT_THROW(service.capture(old), std::invalid_argument);
    EXPECT_EQ(old, before); // A downgrade cannot partially replace the destination.
    service.restore(savedState(14));
    EXPECT_FALSE(service.isInCombat(a));
    EXPECT_TRUE(service.combatOpponents(b).empty());
}

TEST(OblivionCombatService, NativeActorClockCaptureRestoreIsAtomicAndClearResetsIt)
{
    auto state = savedState();
    ESM4::RuntimeActorValues values;
    values.mActor = state.mPlayer.mReference;
    values.mBase = ESM::FormKey::dynamic("player-base", 1);
    values.mOwner = ESM4::ActorValueOwner::Player;
    state.mNativeActorValues.push_back(values);
    state.mNativeActorManagerTime = .125f;
    state.mNativeActorUpdateTimes = {{values.mActor, 100000.f}};
    MWMechanics::OblivionCombatService service;
    service.restore(state);
    auto captured = savedState();
    service.capture(captured);
    EXPECT_EQ(captured, state);
    auto invalid = state;
    invalid.mNativeActorUpdateTimes.emplace(ESM::FormKey::dynamic("missing", 1), 0.f);
    EXPECT_THROW(service.restore(invalid), std::runtime_error);
    service.capture(captured);
    EXPECT_EQ(captured, state);
    auto old = savedState(15);
    const auto before = old;
    EXPECT_THROW(service.capture(old), std::invalid_argument);
    EXPECT_EQ(old, before);
    service.clear();
    service.capture(captured);
    EXPECT_EQ(captured.mNativeActorManagerTime, 0);
    EXPECT_TRUE(captured.mNativeActorUpdateTimes.empty());
    service.restore(state);
    service.restore(before);
    service.capture(captured);
    EXPECT_EQ(captured.mNativeActorManagerTime, 0);
    EXPECT_TRUE(captured.mNativeActorUpdateTimes.empty());
}


namespace
{
    ESM4::RuntimeActorRagdoll physicalSnapshot(const ESM4::RuntimeState& state)
    {
        ESM4::RuntimeActorRagdoll result;
        result.mBase = state.mReferences.front().mBase;
        result.mModel = "characters/_male/skeleton.nif";
        result.mAssetHash = "0123456789abcdef0123456789abcdef";
        ESM4::RuntimeRagdollBody body;
        body.mRecord = 12;
        body.mNodeRecord = 8;
        body.mPosition = {-0.0f, 2, 3};
        body.mLinearVelocity = {1, 2, 3};
        body.mAngularVelocity = {4, 5, 6};
        result.mBodies.push_back(body);
        return result;
    }
}

TEST(OblivionCombatService, PhysicalPosePersistsThroughServiceAndBinaryRestart)
{
    auto state = combatMembershipState();
    const auto actor = state.mReferences.front().mKey;
    const auto pose = physicalSnapshot(state);
    MWMechanics::OblivionCombatService service;
    service.restore(state);
    ASSERT_FALSE(service.actorRagdoll(actor));
    ASSERT_TRUE(service.syncActorRagdoll(actor, std::nullopt, pose));
    service.capture(state);
    ASSERT_EQ(state.mNativeActorRagdolls.size(), 1);
    EXPECT_EQ(state.mNativeActorRagdolls.at(actor), pose);
    // Fresh authority receives only decoded bytes, without borrowing service data.
    const auto bytes = state.serializeBinary();
    MWMechanics::OblivionCombatService restored;
    restored.restore(ESM4::RuntimeState::deserializeBinary(bytes));
    EXPECT_EQ(restored.actorRagdoll(actor), pose);
    auto copy = restored.actorRagdoll(actor);
    copy->mBodies[0].mPosition[0] = 100;
    EXPECT_EQ(restored.actorRagdoll(actor), pose);
    auto recaptured = savedState();
    recaptured.mReferences = state.mReferences;
    restored.capture(recaptured);
    EXPECT_EQ(recaptured.serializeBinary(), bytes); // Includes negative zero.
    EXPECT_EQ(recaptured.mNativeActorLife, state.mNativeActorLife);
    EXPECT_EQ(recaptured.mCombatRngState, state.mCombatRngState);
    EXPECT_TRUE(recaptured.mPendingDeathEvents.empty());
}

TEST(OblivionCombatService, StalePhysicalPublicationCannotOverwriteOrRecreateReleasedBodies)
{
    auto state = combatMembershipState();
    const auto actor = state.mReferences.front().mKey;
    const auto original = physicalSnapshot(state);
    auto newer = original;
    newer.mBodies[0].mPosition[1] = 40;
    MWMechanics::OblivionCombatService service;
    service.restore(state);
    ASSERT_TRUE(service.syncActorRagdoll(actor, std::nullopt, original));
    EXPECT_FALSE(service.syncActorRagdoll(actor, std::nullopt, newer));
    ASSERT_TRUE(service.syncActorRagdoll(actor, original, newer));
    EXPECT_FALSE(service.syncActorRagdoll(actor, original, original));
    EXPECT_FALSE(service.syncActorRagdoll(actor, original, std::nullopt));
    EXPECT_EQ(service.actorRagdoll(actor), newer);
    ASSERT_TRUE(service.syncActorRagdoll(actor, newer, std::nullopt));
    EXPECT_FALSE(service.syncActorRagdoll(actor, newer, original));
    EXPECT_FALSE(service.actorRagdoll(actor));
    EXPECT_TRUE(service.syncActorRagdoll(actor, std::nullopt, std::nullopt));
    ASSERT_TRUE(service.syncActorRagdoll(actor, std::nullopt, original));
    state.mNativeActorRagdolls.emplace(actor, original);
    service.capture(state);
    EXPECT_EQ(state.mNativeActorRagdolls.at(actor), original);
}

TEST(OblivionCombatService, InvalidPhysicalPublicationAndRestoreAreAtomic)
{
    auto state = combatMembershipState();
    const auto actor = state.mReferences.front().mKey;
    const auto pose = physicalSnapshot(state);
    MWMechanics::OblivionCombatService service;
    service.restore(state);
    EXPECT_THROW(service.syncActorRagdoll(ESM::FormKey{}, std::nullopt, pose), std::invalid_argument);
    auto wrongBase = pose;
    wrongBase.mBase = state.mReferences.back().mBase;
    EXPECT_THROW(service.syncActorRagdoll(actor, std::nullopt, wrongBase), std::invalid_argument);
    auto invalid = pose;
    invalid.mBodies[0].mRotation[0] = 2;
    EXPECT_THROW(service.syncActorRagdoll(actor, std::nullopt, invalid), std::runtime_error);
    EXPECT_FALSE(service.actorRagdoll(actor));
    ASSERT_TRUE(service.syncActorRagdoll(actor, std::nullopt, pose));
    EXPECT_THROW(service.syncActorRagdoll(actor, pose, invalid), std::runtime_error);
    for (unsigned field = 0; field < 5; ++field)
    {
        auto changed = pose;
        switch (field)
        {
            case 0: changed.mModel = "characters/_male/another.nif"; break;
            case 1: changed.mAssetHash[0] = 'a'; break;
            case 2: changed.mBodies[0].mRecord = 16; break;
            case 3: changed.mBodies[0].mNodeRecord = 9; break;
            case 4:
                auto extra = changed.mBodies[0];
                extra.mRecord = 20;
                extra.mNodeRecord = 16;
                changed.mBodies.push_back(extra);
                break;
        }
        EXPECT_THROW(service.syncActorRagdoll(actor, pose, changed), std::invalid_argument);
    }
    service.capture(state);
    const auto before = state.serializeBinary();
    state.mNativeActorRagdolls.at(actor) = invalid;
    EXPECT_THROW(service.restore(state), std::runtime_error);
    service.capture(state);
    EXPECT_EQ(state.serializeBinary(), before);
    EXPECT_EQ(service.actorRagdoll(actor), pose);
}

TEST(OblivionCombatService, PhysicalDowngradeClearAndLegacyReplacementDoNotInventHistory)
{
    auto state = combatMembershipState();
    const auto actor = state.mReferences.front().mKey;
    const auto pose = physicalSnapshot(state);
    MWMechanics::OblivionCombatService service;
    service.restore(state);
    ASSERT_TRUE(service.syncActorRagdoll(actor, std::nullopt, pose));
    auto old = savedState(30);
    const auto before = old;
    EXPECT_THROW(service.capture(old), std::invalid_argument);
    EXPECT_EQ(old, before);
    EXPECT_EQ(service.actorRagdoll(actor), pose);
    service.clear();
    EXPECT_FALSE(service.actorRagdoll(actor));
    service.capture(state);
    EXPECT_TRUE(state.mNativeActorRagdolls.empty());
    state = combatMembershipState();
    service.restore(state);
    ASSERT_TRUE(service.syncActorRagdoll(actor, std::nullopt, pose));
    state.mVersion = 30;
    service.restore(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()));
    EXPECT_FALSE(service.actorRagdoll(actor));
    auto current = state;
    current.mVersion = 31;
    service.capture(current);
    EXPECT_TRUE(current.mNativeActorRagdolls.empty());
    EXPECT_EQ(current.mNativeActorLife, state.mNativeActorLife);
}

TEST(OblivionCombatService, PhysicalGroupPublicationRejectsLateStaleAndInvalidUpdatesAtomically)
{
    auto state = combatMembershipState();
    const auto first = state.mReferences.front().mKey;
    const auto last = state.mReferences.back().mKey;
    ASSERT_LT(first, last);
    const auto middle = state.mReferences[1].mKey;
    auto one = physicalSnapshot(state);
    auto two = one;
    two.mBase = state.mReferences.back().mBase;
    MWMechanics::OblivionCombatService service;
    service.restore(state);
    ASSERT_TRUE(service.syncActorRagdoll(first, std::nullopt, one));
    ASSERT_TRUE(service.syncActorRagdoll(last, std::nullopt, two));
    auto untouched = one;
    untouched.mBase = state.mReferences[1].mBase;
    ASSERT_TRUE(service.syncActorRagdoll(middle, std::nullopt, untouched));
    const auto action = service.allocateAction();
    auto changed = one;
    changed.mBodies[0].mPosition = {99, 88, 77};
    auto changedTwo = two;
    changedTwo.mBodies[0].mPosition = {66, 55, 44};
    for (unsigned field = 0; field < 7; ++field)
    {
        MWMechanics::OblivionCombatService::PhysicalPoseUpdates updates{
            {first, {one, changed}}, {last, {two, changedTwo}}};
        if (field == 0) updates.at(last).first = std::nullopt;
        if (field == 1) updates.at(last).first->mBodies[0].mPosition[0] = 123;
        if (field == 2) updates.at(last).second->mBase = one.mBase;
        if (field == 3) updates.at(last).second->mAssetHash[0] = 'a';
        if (field == 4) updates.at(last).second->mBodies[0].mRecord = 16;
        if (field == 5) updates.at(last).second->mBodies[0].mRotation[0] = 2;
        if (field == 6) updates.at(last).second->mModel = "another.nif";
        auto before = state;
        service.capture(before);
        if (field < 2)
            EXPECT_FALSE(service.syncActorRagdolls(updates));
        else
            EXPECT_ANY_THROW(service.syncActorRagdolls(updates));
        EXPECT_EQ(service.actorRagdoll(first), one);
        EXPECT_EQ(service.actorRagdoll(last), two);
        EXPECT_EQ(service.actorRagdoll(middle), untouched);
        auto after = state;
        service.capture(after);
        EXPECT_EQ(after.serializeBinary(), before.serializeBinary());
        EXPECT_TRUE(service.isActionPending(action));
        // Reset only after checking both owners, to reach every bad-late case.
        service.restore(state);
        ASSERT_TRUE(service.syncActorRagdoll(middle, std::nullopt, untouched));
        EXPECT_EQ(service.allocateAction(), action);
        ASSERT_TRUE(service.syncActorRagdoll(first, std::nullopt, one));
        ASSERT_TRUE(service.syncActorRagdoll(last, std::nullopt, two));
    }
    ASSERT_TRUE(service.syncActorRagdolls({{first, {one, changed}}, {last, {two, changedTwo}}}));
    EXPECT_EQ(service.actorRagdoll(first), changed);
    EXPECT_EQ(service.actorRagdoll(last), changedTwo);
    EXPECT_TRUE(service.syncActorRagdolls({}));
    ASSERT_TRUE(service.syncActorRagdolls({{first, {changed, std::nullopt}}}));
    EXPECT_FALSE(service.actorRagdoll(first));
    EXPECT_EQ(service.actorRagdoll(last), changedTwo);
    ASSERT_TRUE(service.syncActorRagdolls({{first, {std::nullopt, one}}, {last, {changedTwo, std::nullopt}}}));
    EXPECT_EQ(service.actorRagdoll(first), one);
    EXPECT_FALSE(service.actorRagdoll(last));
    auto captured = state;
    service.capture(captured);
    MWMechanics::OblivionCombatService restored;
    restored.restore(ESM4::RuntimeState::deserializeBinary(captured.serializeBinary()));
    EXPECT_EQ(restored.actorRagdoll(first), one);
    EXPECT_FALSE(restored.actorRagdoll(last));
    EXPECT_EQ(restored.actorRagdoll(middle), untouched);
    EXPECT_TRUE(restored.isActionPending(action));
}

TEST(OblivionCombatService, PlayerBowTimerAuthoritySaveRestoreContinuationAndClear)
{
    using Phase = ESM4::BowAnimationPhase;
    auto state = savedState();
    ESM4::RuntimeActorValues values;
    values.mActor = state.mPlayer.mReference;
    values.mBase = ESM::FormKey::dynamic("player-base", 1);
    values.mOwner = ESM4::ActorValueOwner::Player;
    values.mPlayerFormValues = {{100, 30, 40, 0}};
    state.mNativeActorValues.push_back(values);
    MWMechanics::OblivionCombatService service;
    EXPECT_THROW(service.updatePlayerBowTimer(.25f, 4, Phase::Start), std::logic_error);
    service.restore(state);
    EXPECT_EQ(service.playerBowTimer(), 0);
    EXPECT_EQ(service.updatePlayerBowTimer(.25f, 4, Phase::Start), .25f);
    EXPECT_EQ(service.updatePlayerBowTimer(.375f, 5, Phase::Hold), .625f);
    service.capture(state);
    ASSERT_EQ(state.mNativePlayerBowTimer, .625f);
    const auto actorBefore = state.mNativeActorValues;
    MWMechanics::OblivionCombatService restored;
    restored.restore(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()));
    EXPECT_EQ(restored.playerBowTimer(), .625f);
    EXPECT_EQ(restored.updatePlayerBowTimer(.25f, 5, Phase::Release),
        service.updatePlayerBowTimer(.25f, 5, Phase::Release));
    restored.capture(state);
    EXPECT_EQ(state.mNativeActorValues, actorBefore);
    EXPECT_EQ(restored.updatePlayerBowTimer(.25f, 5, Phase::End), 0);
    restored.clear(); EXPECT_EQ(restored.playerBowTimer(), 0);
    auto empty = savedState(); restored.capture(empty);
    EXPECT_FALSE(empty.mNativePlayerBowTimer);
}

TEST(OblivionCombatService, PlayerBowTimerInvalidUpdateRestoreAndDowngradeAreAtomic)
{
    using Phase = ESM4::BowAnimationPhase;
    auto state = savedState();
    ESM4::RuntimeActorValues values;
    values.mActor = state.mPlayer.mReference;
    values.mBase = ESM::FormKey::dynamic("player-base", 1);
    values.mOwner = ESM4::ActorValueOwner::Player;
    values.mPlayerFormValues = {{100, 30, 40, 0}};
    state.mNativeActorValues.push_back(values);
    MWMechanics::OblivionCombatService service; service.restore(state);
    service.updatePlayerBowTimer(.625f, 4, Phase::Attach);
    service.capture(state); const auto before = state.serializeBinary();
    EXPECT_THROW(service.updatePlayerBowTimer(-1, 5, Phase::Hold), std::invalid_argument);
    auto bad = state; bad.mNativePlayerBowTimer = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(service.restore(bad), std::runtime_error);
    service.capture(state); EXPECT_EQ(state.serializeBinary(), before);
    auto old = savedState(36); const auto untouched = old.serializeBinary();
    EXPECT_THROW(service.capture(old), std::invalid_argument);
    EXPECT_EQ(old.serializeBinary(), untouched);
    auto legacy = state; legacy.mVersion = 36; legacy.mNativePlayerBowTimer.reset();
    service.restore(ESM4::RuntimeState::deserializeBinary(legacy.serializeBinary()));
    EXPECT_EQ(service.playerBowTimer(), 0);
}

namespace
{
    ESM4::RuntimeState bowServiceState()
    {
        auto state = savedState();
        ESM4::RuntimeActorValues values;
        values.mActor = state.mPlayer.mReference;
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mProcess = ESM4::ActorValueProcess::Active;
        values.mProcessAction = -1; values.mProcessKnockedState = 0;
        values.mPlayerFormValues = {{100, 30, 40, 0}};
        state.mNativeActorValues = {values};
        state.mNativeActorLife = {{values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}}};
        return state;
    }
    ESM4::RuntimeBowState preparedBow()
    {
        ESM4::RuntimeBowState bow;
        bow.mBowBase = ESM::FormKey::content("oblivion.esm", 0x25231);
        bow.mAmmoBase = ESM::FormKey::content("oblivion.esm", 0x17829);
        bow.mAnimationGroup = "bowattack"; bow.mKeyTimes = {0, .25f, 1, 1.1f, 2};
        return bow;
    }
}

TEST(OblivionCombatService, OwnedBowDrawHoldReleaseSaveResumeAndNoReplay)
{
    using Phase = ESM4::BowAnimationPhase; using Event = ESM4::BowActionEvent;
    auto state = bowServiceState(); const auto actor = state.mPlayer.mReference;
    MWMechanics::OblivionCombatService service; service.restore(state);
    const auto id = service.beginBowDraw(actor, preparedBow());
    EXPECT_EQ(service.getProcessAction(actor), 4);
    ASSERT_TRUE(service.advanceBowPlayback(id, actor, .3f, false, true));
    EXPECT_EQ(service.pendingBowEvent(actor, true, true), Event::Attach);
    EXPECT_EQ(service.pendingBowEvent(actor, false, true), Event::None);
    EXPECT_FALSE(service.advanceBowPlayback(id, actor, 10, false, true));
    EXPECT_FALSE(service.commitBowRelease(id, actor));
    EXPECT_FALSE(service.confirmBowAttachment(id + 1, actor));
    ASSERT_TRUE(service.confirmBowAttachment(id, actor)); EXPECT_EQ(service.getProcessAction(actor), 5);
    EXPECT_FALSE(service.confirmBowAttachment(id, actor));
    ASSERT_TRUE(service.advanceBowPlayback(id, actor, 1, false, true));
    EXPECT_EQ(service.findBowState(actor)->mProgress.mPhase, Phase::Hold);
    ASSERT_TRUE(service.advanceBowPlayback(id, actor, .5f, true, true));
    EXPECT_EQ(service.findBowState(actor)->mProgress.mPhase, Phase::Hold);
    EXPECT_NEAR(service.playerBowTimer(), 1.8f, 1e-6f);
    service.capture(state);
    MWMechanics::OblivionCombatService resumed;
    resumed.restore(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()));
    ASSERT_TRUE(resumed.bindBowPlayback(id, actor));
    ASSERT_TRUE(resumed.advanceBowPlayback(id, actor, .2f, false, true));
    ASSERT_TRUE(service.advanceBowPlayback(id, actor, .2f, false, true));
    EXPECT_EQ(resumed.findBowState(actor)->mProgress, service.findBowState(actor)->mProgress);
    EXPECT_EQ(resumed.pendingBowEvent(actor, true, true), Event::Release);
    EXPECT_FALSE(resumed.advanceBowPlayback(id, actor, 10, false, true));
    ASSERT_TRUE(resumed.commitBowRelease(id, actor));
    EXPECT_EQ(resumed.getProcessAction(actor), 3); EXPECT_TRUE(resumed.isActionConsumed(id));
    EXPECT_FALSE(resumed.commitBowRelease(id, actor));
    resumed.capture(state); MWMechanics::OblivionCombatService released;
    released.restore(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()));
    EXPECT_FALSE(released.commitBowRelease(id, actor));
    ASSERT_TRUE(released.advanceBowPlayback(id, actor, .9f, false, true));
    EXPECT_EQ(released.findBowState(actor)->mProgress.mPhase, Phase::End);
    EXPECT_EQ(released.playerBowTimer(), 0);
    ASSERT_TRUE(released.finishBowPlayback(id, actor));
    EXPECT_FALSE(released.findBowState(actor)); EXPECT_EQ(released.getProcessAction(actor), -1);
    EXPECT_FALSE(released.finishBowPlayback(id, actor));
}

TEST(OblivionCombatService, BowInvalidFramesAdmissionAndDowngradeLeaveWholeStateUnchanged)
{
    auto state = bowServiceState(); const auto actor = state.mPlayer.mReference;
    MWMechanics::OblivionCombatService service; service.restore(state);
    auto bad = preparedBow(); bad.mKeyTimes[2] = -.5f;
    EXPECT_THROW(service.beginBowDraw(actor, bad), std::runtime_error);
    service.capture(state); EXPECT_EQ(state.mPhysicalActions.mNext, 1);
    const auto id = service.beginBowDraw(actor, preparedBow());
    service.capture(state); const auto before = state.serializeBinary();
    EXPECT_THROW(service.beginBowDraw(actor, preparedBow()), std::invalid_argument);
    EXPECT_THROW(service.beginMeleeStrike(actor, ESM4::MeleeStrikeKind::Left, "attackleft"), std::invalid_argument);
    EXPECT_FALSE(service.beginBlocking(actor));
    EXPECT_THROW(service.advanceBowPlayback(id, actor, -1, false, true), std::invalid_argument);
    EXPECT_FALSE(service.advanceBowPlayback(id + 1, actor, 1, false, true));
    EXPECT_FALSE(service.advanceBowPlayback(id, actor, 1, false, false));
    auto old = savedState(37); const auto oldBefore = old;
    EXPECT_THROW(service.capture(old), std::invalid_argument); EXPECT_EQ(old, oldBefore);
    auto invalid = state; invalid.mNativeBowStates.begin()->second.mReleaseCommitted = true;
    EXPECT_THROW(service.restore(invalid), std::runtime_error);
    service.capture(state); EXPECT_EQ(state.serializeBinary(), before);
}

TEST(OblivionCombatService, BowCancellationRetiresOwnershipAndPreservesForeignProcessAction)
{
    auto state = bowServiceState(); const auto actor = state.mPlayer.mReference;
    MWMechanics::OblivionCombatService service; service.restore(state);
    auto id = service.beginBowDraw(actor, preparedBow());
    EXPECT_FALSE(service.cancelBowDraw(id + 1, actor));
    service.setProcessAction(actor, 6);
    EXPECT_FALSE(service.bindBowPlayback(id, actor));
    EXPECT_FALSE(service.advanceBowPlayback(id, actor, .3f, false, true));
    EXPECT_TRUE(service.consumeAction(id, actor));
    EXPECT_FALSE(service.findBowState(actor)); EXPECT_EQ(service.getProcessAction(actor), 6);
    service.setProcessAction(actor, -1); id = service.beginBowDraw(actor, preparedBow());
    EXPECT_EQ(service.cancelActorActions(actor), 1);
    EXPECT_FALSE(service.findBowState(actor)); EXPECT_EQ(service.getProcessAction(actor), -1);
    id = service.beginBowDraw(actor, preparedBow());
    service.setProcessKnockedState(actor, 1);
    EXPECT_FALSE(service.advanceBowPlayback(id, actor, .3f, false, true));
    EXPECT_EQ(service.cancelActorActions(actor), 1);
    EXPECT_FALSE(service.findBowState(actor)); EXPECT_TRUE(service.isActionConsumed(id));
}

TEST(OblivionCombatService, BowDrawIdentityCannotCommitAMeleeResourceTransaction)
{
    auto state = bowServiceState(); const auto actor = state.mPlayer.mReference;
    MWMechanics::OblivionCombatService service; service.restore(state);
    const auto id = service.beginBowDraw(actor, preparedBow());
    ESM4::Npc base{}; ESM4::ActorCharacter reference{};
    reference.mFormKey = actor; reference.mBaseKey = state.mNativeActorValues[0].mBase;
    MWClass::ESM4Npc::registerSelf();
    MWWorld::LiveCellRef<ESM4::Npc> live(reference, &base);
    const MWWorld::Ptr ptr(&live, nullptr);
    service.capture(state); const auto before = state.serializeBinary();
    EXPECT_FALSE(service.commitPhysicalContact(id, ptr, {}, {-7, 0, 0, 0}, nullptr, false, {}, {}));
    service.capture(state); EXPECT_EQ(state.serializeBinary(), before);
    EXPECT_TRUE(service.isActionPending(id, actor));
}

TEST(OblivionCombatService, PlayerBowInputHoldAndReleaseContinueExactlyAcrossSave)
{
    using Phase = ESM4::BowAnimationPhase;
    auto state = bowServiceState(); const auto actor = state.mPlayer.mReference;
    MWMechanics::OblivionCombatService service; service.restore(state);
    const auto id = service.beginBowDraw(actor, preparedBow());
    ASSERT_TRUE(service.findBowState(actor)->mPlayerHoldLatched);
    ASSERT_TRUE(service.advancePlayerBowPlayback(id, actor, .3f, true, true, true, false, true));
    ASSERT_TRUE(service.confirmBowAttachment(id, actor));
    ASSERT_TRUE(service.advancePlayerBowPlayback(id, actor, 1, true, false, true, false, true));
    ASSERT_EQ(service.findBowState(actor)->mProgress.mPhase, Phase::Hold);
    ASSERT_TRUE(service.advancePlayerBowPlayback(id, actor, .5f, true, false, true, false, true));
    ASSERT_EQ(service.findBowState(actor)->mProgress.mPhase, Phase::Hold);
    service.capture(state);
    MWMechanics::OblivionCombatService resumed;
    resumed.restore(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()));
    ASSERT_TRUE(resumed.bindBowPlayback(id, actor));
    for (auto* current : {&service, &resumed})
    {
        ASSERT_TRUE(current->advancePlayerBowPlayback(id, actor, .01f, false, false, true, false, true));
        EXPECT_FALSE(current->findBowState(actor)->mPlayerHoldLatched);
        EXPECT_EQ(current->playerBowTimer(), 1.5f);
        ASSERT_EQ(current->findBowState(actor)->mProgress.mPhase, Phase::Hold);
    }
    auto a = state, b = state; service.capture(a); resumed.capture(b);
    EXPECT_EQ(a.serializeBinary(), b.serializeBinary());
    MWMechanics::OblivionCombatService afterRelease;
    afterRelease.restore(ESM4::RuntimeState::deserializeBinary(b.serializeBinary()));
    ASSERT_TRUE(afterRelease.bindBowPlayback(id, actor));
    ASSERT_TRUE(afterRelease.advancePlayerBowPlayback(id, actor, .2f, true, true, true, false, true));
    ASSERT_TRUE(service.advancePlayerBowPlayback(id, actor, .2f, true, true, true, false, true));
    EXPECT_FALSE(afterRelease.findBowState(actor)->mPlayerHoldLatched);
    EXPECT_EQ(afterRelease.playerBowTimer(), 1.5f);
    EXPECT_EQ(afterRelease.findBowState(actor)->mProgress.mPhase, Phase::Release);
    service.capture(a); afterRelease.capture(b); EXPECT_EQ(a.serializeBinary(), b.serializeBinary());
}

TEST(OblivionCombatService, PlayerBowInputFrameRejectsAtomicallyAndCancellationRemovesLatch)
{
    auto state = bowServiceState(); const auto actor = state.mPlayer.mReference;
    MWMechanics::OblivionCombatService service; service.restore(state);
    const auto id = service.beginBowDraw(actor, preparedBow());
    service.capture(state); const auto before = state.serializeBinary();
    EXPECT_THROW(service.advancePlayerBowPlayback(id, actor, -1, false, false, true, false, true),
        std::invalid_argument);
    EXPECT_FALSE(service.advancePlayerBowPlayback(id + 1, actor, 1, false, false, true, false, true));
    EXPECT_FALSE(service.advancePlayerBowPlayback(id, actor, 1, false, false, true, false, false));
    service.capture(state); EXPECT_EQ(state.serializeBinary(), before);
    auto old = state; old.mVersion = 38;
    EXPECT_THROW(service.capture(old), std::invalid_argument);
    ASSERT_TRUE(service.cancelBowDraw(id, actor));
    EXPECT_FALSE(service.findBowState(actor));
    service.capture(state); EXPECT_TRUE(state.mNativeBowStates.empty());
    const auto next = service.beginBowDraw(actor, preparedBow());
    EXPECT_GT(next, id); EXPECT_TRUE(service.findBowState(actor)->mPlayerHoldLatched);
}
