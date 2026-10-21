#include "crimefixture.hpp"

#include <components/esm4/loadrefr.hpp>
#include <components/esm4/runtimestate.hpp>

#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <bit>
#include <algorithm>
#include <limits>
#include <memory>
#include <sstream>

namespace
{
    TEST(ESM4RuntimeState, nativeReferenceDefaultsToUnlocked)
    {
        ESM4::Reference reference;
        EXPECT_FALSE(reference.mIsLocked);
        EXPECT_EQ(reference.mLockLevel, 0);
    }

    ESM4::RuntimeState makeState()
    {
        ESM4::RuntimeState state;
        state.mNextDynamicSerial = 73;
        state.mContent = { { "oblivion.esm", "sha256:base" }, { "knights.esp", "sha256:knights" } };
        state.mClock = { 3, 8, 17, 13.5, 30.0 };
        state.mPlayer.mReference = ESM::FormKey::dynamic("save-1", 1);
        state.mPlayer.mCell = ESM::FormKey::content("Oblivion.esm", 0x1650f);
        state.mPlayer.mPosition.pos[0] = 12.5f;
        state.mPlayer.mPosition.pos[1] = -3.f;
        state.mPlayer.mPosition.rot[2] = 1.25f;
        state.mPlayer.mActorValues = { { "health", 42.25 }, { "magicka", 31.0 } };
        state.mPlayer.mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x18baa), 1, 125, 80.f,
            0x4, 2, ESM::FormKey::content("Oblivion.esm", 0x7) } };
        state.mPlayer.mInventory.front().mRemainingUsageTime = 812.5f;
        state.mPlayer.mName = "Bendu Olo";
        state.mPlayer.mRace = ESM::FormKey::content("Oblivion.esm", 0x907);
        state.mPlayer.mClass = ESM::FormKey::content("Oblivion.esm", 0x237a8);
        state.mPlayer.mBirthSign = ESM::FormKey::content("Oblivion.esm", 0x22a37);
        state.mPlayer.mCharacterGenerationFlags = 15;
        state.mGlobals.emplace(ESM::FormKey::content("Oblivion.esm", 0x33), std::int64_t(9));
        state.mGlobals.emplace(ESM::FormKey::content("Oblivion.esm", 0x34), 1.5);

        ESM4::RuntimeReferenceState reference;
        reference.mKey = ESM::FormKey::content("Oblivion.esm", 0x100);
        reference.mBase = ESM::FormKey::content("Oblivion.esm", 0x200);
        reference.mCell = state.mPlayer.mCell;
        reference.mEnabled = false;
        reference.mPosition.pos[2] = 64.f;
        reference.mOwner = ESM::FormKey::content("Oblivion.esm", 0x300);
        reference.mLockLevel = 40;
        reference.mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x400), 3, -1, -1.f, 0, -1,
            ESM::FormKey::content("Oblivion.esm", 0x300) } };
        reference.mCustomState = { { "harvested", true }, { "label", std::string("opened") } };
        state.mReferences.push_back(std::move(reference));
        state.mScriptEventSequence = 91;
        ESM4::RuntimeScriptInstance script;
        script.mUnit = "content:oblivion.esm:04e90e";
        script.mContext = ESM::FormKey::content("Oblivion.esm", 0x1fc41);
        script.mOnLoadFired = true;
        script.mLocals = { std::int64_t(7), 2.5, std::string("named"),
            ESM::FormKey::content("Oblivion.esm", 0x2466e), std::monostate{} };
        state.mScriptInstances.push_back(std::move(script));
        state.mQuests.push_back({ ESM::FormKey::content("Oblivion.esm", 0x32a15), 19, true, { 10, 19 } });
        return state;
    }

    ESM4::RuntimeState makeM14State()
    {
        ESM4::RuntimeState state = makeState();
        state.mVersion = 5;
        state.mAiRngState = 0x123456789abcdef0ULL;

        ESM4::RuntimeActorAiState actor;
        actor.mActor = ESM::FormKey::content("Oblivion.esm", 0x500);
        actor.mBase = ESM::FormKey::content("Oblivion.esm", 0x501);
        actor.mPackage = ESM::FormKey::content("Knights.esp", 0x502);
        actor.mScriptPackage = ESM::FormKey::dynamic("m14-script", 1);
        actor.mTarget = ESM::FormKey::content("Oblivion.esm", 0x503);
        actor.mTargetBase = ESM::FormKey::content("Oblivion.esm", 0x504);
        actor.mCell = state.mPlayer.mCell;
        actor.mPathgrid = ESM::FormKey::content("Oblivion.esm", 0x505);
        actor.mDoor = ESM::FormKey::content("Oblivion.esm", 0x506);
        actor.mDestinationCell = state.mPlayer.mCell;
        actor.mDestinationPosition = state.mPlayer.mPosition;
        actor.mDestinationPosition.pos[0] = 128.f;
        actor.mLastValidCell = state.mPlayer.mCell;
        actor.mLastValidPosition = state.mPlayer.mPosition;
        actor.mLastValidPosition.pos[1] = 256.f;
        actor.mActionItem = ESM::FormKey::content("Oblivion.esm", 0x507);
        actor.mLastTransitionDoor = ESM::FormKey::content("Oblivion.esm", 0x508);
        actor.mCompanionGroup = ESM::FormKey::dynamic("m14-group", 1);
        actor.mCompanionSideWith = ESM::FormKey::content("Oblivion.esm", 0x50b);
        actor.mMount = ESM::FormKey::content("Oblivion.esm", 0x509);
        actor.mRider = ESM::FormKey::content("Oblivion.esm", 0x50a);
        actor.mScheduleWindow = ESM4::ScheduleWindow{
            { 3, 8, 17, 12.0 }, { 3, 8, 17, 16.0 }, 4.0 };
        actor.mConditionResult = ESM4::ConditionResult::True;
        actor.mSource = ESM4::PackageSource::Script;
        actor.mPackageType = ESM4::AIPackageType::Eat;
        actor.mProcedure = ESM4::PackageProcedure::Eat;
        actor.mPhase = ESM4::PackagePhase::Wait;
        actor.mTier = ESM4::ProcessTier::Low;
        actor.mBoundary = ESM4::PhaseBoundary::None;
        actor.mListIndex = 7;
        actor.mPathNode = 4;
        actor.mRepathAttempts = 2;
        actor.mFormationIndex = 3;
        actor.mSelectionGeneration = 11;
        actor.mRouteGeneration = 12;
        actor.mTransitionGeneration = 13;
        actor.mActionTimer = 1.5f;
        actor.mDurationRemaining = 2.5f;
        actor.mNoProgressSeconds = 0.25f;
        actor.mDoorCooldown = 0.5f;
        actor.mLowProcessTimer = 0.75f;
        actor.mNextLowProcessTick = 0.25f;
        actor.mRestrained = true;
        actor.mActionReserved = true;
        actor.mHasDestination = true;
        actor.mInterruptionReason = "m14-test";
        state.mActorAi.push_back(actor);

        state.mPathPoints.push_back({ actor.mPathgrid, actor.mPathNode, false });
        state.mCompanions.push_back({ actor.mTarget, actor.mActor, actor.mCompanionGroup,
            actor.mCompanionSideWith, actor.mFormationIndex });
        state.mMounts.push_back({ actor.mMount, actor.mRider, actor.mBase, actor.mLastTransitionDoor, true });
        state.mDetectionVectors.push_back({ actor.mActor, actor.mTarget, 72.5, true, true });
        return state;
    }

    TEST(ESM4RuntimeState, binaryRoundTripPreservesEveryStateFamily)
    {
        const ESM4::RuntimeState expected = makeState();
        const auto bytes = expected.serializeBinary();
        const ESM4::RuntimeState actual = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(actual, expected);
        EXPECT_EQ(actual.serializeBinary(), bytes);
        EXPECT_EQ(actual.canonicalJson(), expected.canonicalJson());
    }

    TEST(ESM4RuntimeState, scriptedLookReferenceSurvivesSaveWithoutChangingAiTarget)
    {
        auto state = makeM14State();
        auto& reference = state.mReferences.front();
        reference.mCustomState["obscript.look_target"] = state.mPlayer.mReference.serialize();
        auto loaded = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        EXPECT_EQ(std::get<std::string>(loaded.mReferences.front().mCustomState.at("obscript.look_target")),
            state.mPlayer.mReference.serialize());
        EXPECT_EQ(loaded.mActorAi, state.mActorAi);
        loaded.mReferences.front().mCustomState.erase("obscript.look_target");
        const auto stopped = ESM4::RuntimeState::deserializeBinary(loaded.serializeBinary());
        EXPECT_FALSE(stopped.mReferences.front().mCustomState.contains("obscript.look_target"));
        EXPECT_EQ(stopped.mActorAi, state.mActorAi);
        EXPECT_EQ(stopped.mReferences.front().mCustomState.at("label"), reference.mCustomState.at("label"));
        for (const std::string value : { "null", "content:oblivion.esm:000000", "garbage",
                 "content:Oblivion.esm:000001", "dynamic:player:0000000000000000" })
        {
            reference.mCustomState["obscript.look_target"] = value;
            EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        }
        reference.mCustomState["obscript.look_target"] = true;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, versionSixPreservesPendingPackageCompletionOrder)
    {
        auto state = makeM14State();
        state.mVersion = 6;
        const auto actor = state.mActorAi.front().mActor;
        const auto package = state.mActorAi.front().mPackage;
        state.mPendingPackageDone = { { actor, package },
            { ESM::FormKey::content("Oblivion.esm", 0x100), package }, { actor, package } };
        auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        EXPECT_EQ(restored, state);
        EXPECT_EQ(restored.canonicalJson(), state.canonicalJson());
        // A callback is removed before invocation. A save taken by that
        // callback must retain only the remaining FIFO, including repeats.
        restored.mPendingPackageDone.erase(restored.mPendingPackageDone.begin());
        auto resumed = ESM4::RuntimeState::deserializeBinary(restored.serializeBinary());
        EXPECT_EQ(resumed.mPendingPackageDone, restored.mPendingPackageDone);
        ASSERT_EQ(resumed.mPendingPackageDone.size(), 2u);
        EXPECT_EQ(resumed.mPendingPackageDone.back().mActor, actor);
        state.mVersion = 5;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, versionSixRejectsInvalidOrTruncatedPackageCompletion)
    {
        auto state = makeState();
        state.mPendingPackageDone.push_back({ {}, ESM::FormKey::content("Oblivion.esm", 1) });
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state.mPendingPackageDone.front() = { ESM::FormKey::content("Oblivion.esm", 2), {} };
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state.mPendingPackageDone.front().mPackage = ESM::FormKey::content("Oblivion.esm", 1);
        auto bytes = state.serializeBinary();
        bytes.pop_back();
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bytes), std::runtime_error);
    }

    TEST(ESM4RuntimeState, versionSevenPersistsDoorAnimationTraversalState)
    {
        auto state = makeM14State();
        state.mVersion = 7;
        state.mActorAi.front().mPhase = ESM4::PackagePhase::Door;
        state.mActorAi.front().mDoorAnimationStarted = true;
        const auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        ASSERT_EQ(restored.mActorAi.size(), 1u);
        EXPECT_TRUE(restored.mActorAi.front().mDoorAnimationStarted);
        EXPECT_NE(restored.canonicalJson().find("\"door_animation_started\":true"), std::string::npos);

        state.mVersion = 6;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state.mVersion = 7;
        state.mActorAi.front().mPhase = ESM4::PackagePhase::Path;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, versionFiveDoesNotSynthesizeCompletionCallbacks)
    {
        auto state = makeM14State();
        state.mActorAi.front().mPhase = ESM4::PackagePhase::Complete;
        const auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        EXPECT_TRUE(restored.mPendingPackageDone.empty());
        EXPECT_EQ(restored.mVersion, 5u);
    }

    TEST(ESM4RuntimeState, versionFivePersistsNativeAiIntentRelationsAndTimers)
    {
        const ESM4::RuntimeState expected = makeM14State();
        const auto bytes = expected.serializeBinary();
        const ESM4::RuntimeState actual = ESM4::RuntimeState::deserializeBinary(bytes);
        ASSERT_EQ(actual.mVersion, 5u);
        ASSERT_EQ(actual.mActorAi.size(), 1u);
        EXPECT_EQ(actual.mActorAi.front(), expected.mActorAi.front());
        EXPECT_EQ(actual.mPathPoints, expected.mPathPoints);
        EXPECT_EQ(actual.mCompanions, expected.mCompanions);
        EXPECT_EQ(actual.mMounts, expected.mMounts);
        EXPECT_EQ(actual.mDetectionVectors, expected.mDetectionVectors);
        EXPECT_EQ(actual.serializeBinary(), bytes);
        const std::string json = actual.canonicalJson();
        EXPECT_NE(json.find("\"destination_cell\":\"content:oblivion.esm:01650f\""), std::string::npos);
        EXPECT_NE(json.find("\"path_points\":[{\"pathgrid\":\"content:oblivion.esm:000505\""),
            std::string::npos);
        EXPECT_NE(json.find("\"mounted\":true"), std::string::npos);
        EXPECT_NE(json.find("\"detection_vectors\":[{\"observer\":\"content:oblivion.esm:000500\""),
            std::string::npos);
    }

    TEST(ESM4RuntimeState, versionFiveAllowsAnIdleActorWithUnknownPackageType)
    {
        ESM4::RuntimeState state = makeState();
        state.mAiRngState = 1;
        ESM4::RuntimeActorAiState actor;
        actor.mActor = ESM::FormKey::content("Oblivion.esm", 0x600);
        actor.mBase = ESM::FormKey::content("Oblivion.esm", 0x601);
        actor.mCell = state.mPlayer.mCell;
        actor.mSource = ESM4::PackageSource::None;
        actor.mPackageType = ESM4::AIPackageType::Unknown;
        actor.mProcedure = ESM4::PackageProcedure::None;
        state.mActorAi.push_back(actor);

        const auto bytes = state.serializeBinary();
        const ESM4::RuntimeState restored = ESM4::RuntimeState::deserializeBinary(bytes);
        ASSERT_EQ(restored.mActorAi.size(), 1u);
        EXPECT_EQ(restored.mActorAi.front().mPackageType, ESM4::AIPackageType::Unknown);
    }

    TEST(ESM4RuntimeState, rejectsNonReciprocalMountedActorState)
    {
        ESM4::RuntimeState state = makeState();
        state.mAiRngState = 1;

        ESM4::RuntimeActorAiState horse;
        horse.mActor = ESM::FormKey::content("Oblivion.esm", 0x610);
        horse.mBase = ESM::FormKey::content("Oblivion.esm", 0x611);
        horse.mCell = state.mPlayer.mCell;
        horse.mRider = ESM::FormKey::content("Oblivion.esm", 0x620);

        ESM4::RuntimeActorAiState rider;
        rider.mActor = ESM::FormKey::content("Oblivion.esm", 0x620);
        rider.mBase = ESM::FormKey::content("Oblivion.esm", 0x621);
        rider.mCell = state.mPlayer.mCell;
        rider.mMount = horse.mActor;

        state.mActorAi = { horse, rider };
        state.mMounts.push_back({ horse.mActor, rider.mActor, horse.mBase, {}, true });
        rider.mMount = {};
        state.mActorAi[1] = rider;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, openMwSaveRecordSupportsChunkedPayloads)
    {
        ESM4::RuntimeState expected = makeState();
        expected.mReferences[0].mCustomState["large"] = std::string(70'000, 'x');

        auto output = std::make_unique<std::stringstream>();
        ESM::ESMWriter writer;
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        writer.save(*output);
        writer.startRecord(ESM4::RuntimeState::sRecordId);
        expected.save(writer);
        writer.endRecord(ESM4::RuntimeState::sRecordId);
        writer.close();

        std::unique_ptr<std::istream> input = std::move(output);
        ESM::ESMReader reader;
        reader.open(std::move(input), "runtime-state-stream");
        ASSERT_TRUE(reader.hasMoreRecs());
        EXPECT_EQ(reader.getRecName(), ESM4::RuntimeState::sRecordId);
        reader.getRecHeader();
        ESM4::RuntimeState actual;
        actual.load(reader);
        EXPECT_EQ(actual, expected);
    }

    TEST(ESM4RuntimeState, formKeysAndDiagnosticsSurvivePluginReordering)
    {
        const ESM4::RuntimeState state = makeState();
        ESM::FormKeyResolver oldOrder({ "Oblivion.esm", "Knights.esp" });
        ESM::FormKeyResolver newOrder({ "Knights.esp", "Oblivion.esm" });
        const ESM::FormKey stable = oldOrder.toFormKey({ 0x100, 0 });
        ASSERT_EQ(newOrder.toFormId(stable), (ESM::FormId{ 0x100, 1 }));
        EXPECT_TRUE(state.getMissingContentFiles({ "KNIGHTS.ESP", "OBLIVION.ESM" }).empty());
        EXPECT_EQ(state.getMissingContentFiles({ "Oblivion.esm" }), (std::vector<std::string>{ "knights.esp" }));
        EXPECT_NO_THROW(state.validateContent(
            { { "KNIGHTS.ESP", "sha256:knights" }, { "OBLIVION.ESM", "sha256:base" } }));
        EXPECT_THROW(state.validateContent({ { "Oblivion.esm", "sha256:base" } }), std::runtime_error);
        try
        {
            state.validateContent(
                { { "Oblivion.esm", "sha256:different" }, { "Knights.esp", "sha256:knights" } });
            FAIL() << "Fingerprint mismatch was accepted";
        }
        catch (const std::runtime_error& error)
        {
            EXPECT_NE(std::string(error.what()).find("fingerprint mismatch for oblivion.esm"), std::string::npos);
        }
    }

    TEST(ESM4RuntimeState, rejectsTruncationCorruptionAndTrailingData)
    {
        const auto valid = makeState().serializeBinary();
        auto truncated = valid;
        truncated.pop_back();
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);

        auto badMagic = valid;
        badMagic[0] ^= 0xff;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(badMagic), std::runtime_error);

        auto trailing = valid;
        trailing.push_back(0);
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(trailing), std::runtime_error);
    }

    TEST(ESM4RuntimeState, rejectsWrongProfileSchemaAndDynamicSerial)
    {
        auto state = makeState();
        state.mProfile = ESM::GameProfile::Morrowind;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state.mProfile = ESM::GameProfile::Oblivion;
        state.mVersion = ESM4::CurrentRuntimeStateVersion + 1;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state.mVersion = ESM4::CurrentRuntimeStateVersion;
        state.mNextDynamicSerial = 0;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state = makeState();
        state.mPlayer.mCell = {};
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state = makeState();
        state.mPlayer.mCharacterGenerationFlags = 0x20;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, rejectsDuplicateReferencesAndInvalidInventory)
    {
        auto state = makeState();
        state.mReferences.push_back(state.mReferences.front());
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state.mReferences.pop_back();
        state.mReferences.front().mInventory.front().mCount = 0;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, rejectsNonFiniteState)
    {
        auto state = makeState();
        state.mPlayer.mPosition.pos[0] = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state = makeState();
        state.mGlobals.begin()->second = std::numeric_limits<double>::infinity();
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state = makeState();
        state.mPlayer.mInventory.front().mRemainingUsageTime = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, canonicalJsonIsStableAndContainsStableKeys)
    {
        const std::string json = makeState().canonicalJson();
        EXPECT_NE(json.find("\"schema_version\":" + std::to_string(ESM4::CurrentRuntimeStateVersion)),
            std::string::npos);
        EXPECT_NE(json.find("content:oblivion.esm:01650f"), std::string::npos);
        EXPECT_NE(json.find("dynamic:save-1:0000000000000001"), std::string::npos);
        EXPECT_NE(json.find("\"inventory\":[{\"base\":\"content:oblivion.esm:018baa\",\"count\":1,"
                                 "\"condition\":125,\"charge\":80,\"equipped_slots\":4,\"hotkey\":2,"
                                 "\"owner\":\"content:oblivion.esm:000007\","
                                 "\"remaining_usage_time\":812.5,\"ownership_rank\":null,\"ownership_global\":\"null\",\"extra_scale\":null}]"),
            std::string::npos);
        EXPECT_NE(json.find("\"script_event_sequence\":91"), std::string::npos);
        EXPECT_NE(json.find("\"stage\":19"), std::string::npos);
        EXPECT_EQ(json, makeState().canonicalJson());
    }

    TEST(ESM4RuntimeState, NativeProcessAction26PreservesEverySignedWord)
    {
        auto state = makeState();
        ESM4::RuntimeActorValues actor;
        actor.mActor = state.mPlayer.mReference;
        actor.mBase = ESM::FormKey::content("oblivion.esm", 7);
        actor.mOwner = ESM4::ActorValueOwner::Player;
        state.mNativeActorValues = {actor};
        for (int raw = -32768; raw <= 32767; ++raw)
        {
            state.mNativeActorValues[0].mProcessAction = static_cast<std::int16_t>(raw);
            const auto binary = state.serializeBinary();
            const auto restored = ESM4::RuntimeState::deserializeBinary(binary);
            ASSERT_EQ(restored.mNativeActorValues[0].mProcessAction,
                state.mNativeActorValues[0].mProcessAction) << raw;
            ASSERT_EQ(restored.serializeBinary(), binary) << raw;
            ASSERT_NE(restored.canonicalJson().find("\"process_action\":" + std::to_string(raw)),
                std::string::npos) << raw;
        }
    }

    TEST(ESM4RuntimeState, NativeProcessAction26PreservesUnknownAndRejectsLossyDowngrade)
    {
        auto state = makeState();
        state.mVersion = 25;
        ESM4::RuntimeActorValues actor;
        actor.mActor = state.mPlayer.mReference;
        actor.mBase = ESM::FormKey::content("oblivion.esm", 7);
        actor.mOwner = ESM4::ActorValueOwner::Player;
        state.mNativeActorValues = {actor};
        const auto old = state.serializeBinary();
        auto decoded = ESM4::RuntimeState::deserializeBinary(old);
        EXPECT_FALSE(decoded.mNativeActorValues[0].mProcessAction);
        EXPECT_EQ(decoded.serializeBinary(), old);
        EXPECT_EQ(decoded.canonicalJson().find("process_action"), std::string::npos);
        decoded.mVersion = 26;
        const auto unknown = decoded.serializeBinary();
        EXPECT_NE(decoded.canonicalJson().find("\"process_action\":null"), std::string::npos);
        EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(unknown).mNativeActorValues[0].mProcessAction);
        decoded.mNativeActorValues[0].mProcessAction = 0;
        const auto known = decoded.serializeBinary();
        const auto marker = std::mismatch(unknown.begin(), unknown.end(), known.begin()).first - unknown.begin();
        auto corrupt = known;
        corrupt[marker] = 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
        decoded.mVersion = 25;
        EXPECT_THROW(decoded.serializeBinary(), std::runtime_error);
        decoded.mVersion = 26;
        decoded.mNativeActorValues[0].mProcess = ESM4::ActorValueProcess::Low;
        EXPECT_THROW(decoded.serializeBinary(), std::runtime_error);
        decoded.mNativeActorValues[0].mProcessAction.reset();
        EXPECT_NO_THROW(decoded.serializeBinary());
    }

    TEST(ESM4RuntimeState, NativeProcessKnockedBytePreservesAllSignedStates)
    {
        auto state = makeState();
        ESM4::RuntimeActorValues actor;
        actor.mActor = state.mPlayer.mReference;
        actor.mBase = ESM::FormKey::content("oblivion.esm", 7);
        actor.mOwner = ESM4::ActorValueOwner::Player;
        state.mNativeActorValues = {actor};
        for (int raw = -128; raw <= 127; ++raw)
        {
            state.mNativeActorValues[0].mProcessKnockedState = static_cast<std::int8_t>(raw);
            const auto binary = state.serializeBinary();
            const auto restored = ESM4::RuntimeState::deserializeBinary(binary);
            ASSERT_EQ(restored.mNativeActorValues[0].mProcessKnockedState,
                state.mNativeActorValues[0].mProcessKnockedState) << raw;
            EXPECT_NE(restored.canonicalJson().find("\"process_knocked_state\":" + std::to_string(raw)),
                std::string::npos);
        }
    }

    TEST(ESM4RuntimeState, nativeActorValuesPreserveSparsePresenceAndSignedZero)
    {
        auto state = makeState();
        state.mVersion = 9;
        ESM4::RuntimeActorValues actor;
        actor.mActor = state.mPlayer.mReference;
        actor.mBase = ESM::FormKey::content("oblivion.esm", 7);
        actor.mOwner = ESM4::ActorValueOwner::Player;
        actor.mValues.back() = {1.25f, {0.f, std::nullopt, -0.f}};
        state.mNativeActorValues.push_back(actor);
        const auto bytes = state.serializeBinary();
        const std::vector<std::uint8_t> suffix{0, 0, 160, 63, 5, 0, 0, 0, 0, 0, 0, 0, 128};
        ASSERT_GE(bytes.size(), suffix.size());
        EXPECT_TRUE(std::equal(suffix.begin(), suffix.end(), bytes.end() - suffix.size()));
        const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
        ASSERT_EQ(restored.mNativeActorValues.size(), 1);
        const auto& value = restored.mNativeActorValues[0].mValues.back();
        EXPECT_EQ(value, actor.mValues.back());
        EXPECT_FALSE(value.mModifiers[1].has_value());
        ASSERT_TRUE(value.mModifiers[0].has_value());
        ASSERT_TRUE(value.mModifiers[2].has_value());
        EXPECT_FALSE(std::signbit(*value.mModifiers[0]));
        EXPECT_TRUE(std::signbit(*value.mModifiers[2]));
        EXPECT_EQ(restored.serializeBinary(), bytes);
        EXPECT_EQ(restored.canonicalJson(), state.canonicalJson());
        EXPECT_NE(restored.canonicalJson().find("[1.25,0,null,-0]"), std::string::npos);
        for (std::size_t remove = 1; remove <= 360; ++remove)
        {
            auto truncated = bytes;
            truncated.resize(bytes.size() - remove);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
        }
        auto corrupt = bytes;
        corrupt[corrupt.size() - 9] = 8;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
        state.mNativeActorValues.clear();
        const auto countOffset = state.serializeBinary().size() - 4;
        corrupt = bytes;
        corrupt[countOffset + 8] = 'X'; // invalid actor key kind
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
        corrupt = bytes;
        std::fill_n(corrupt.begin() + countOffset, 4, 0xff);
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
        corrupt = bytes;
        corrupt[countOffset] = 2;
        corrupt.insert(corrupt.end(), bytes.begin() + countOffset + 4, bytes.end());
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
    }

    TEST(ESM4RuntimeState, sharedBaseOverridesPreserveTypedValuesAndCanonicalOrder)
    {
        auto state = makeState();
        state.mVersion = 11; // The suffix offsets in this case describe the v11 wire format.
        ESM4::RuntimeActorBaseOverride npc{ESM::FormKey::content("oblivion.esm", 7), ESM4::ActorBaseKind::Npc,
            {{8, std::int32_t{16777217}}, {9, std::int32_t{65535}}, {40, 2147483648.f}}};
        ESM4::RuntimeActorBaseOverride creature{ESM::FormKey::content("oblivion.esm", 8), ESM4::ActorBaseKind::Creature,
            {{12, std::int32_t{255}}, {19, std::int32_t{0}}, {26, std::int32_t{1}}}};
        state.mNativeActorBases = {npc};
        const auto oneBase = state.serializeBinary();
        const std::vector<std::uint8_t> suffix{8, 0, 1, 0, 0, 1, 9, 0, 255, 255, 0, 0, 40, 1, 0, 0, 0, 79};
        ASSERT_GE(oneBase.size(), suffix.size());
        EXPECT_TRUE(std::equal(suffix.begin(), suffix.end(), oneBase.end() - suffix.size()));
        for (const auto [offset, value] : {std::pair{17, 2}, {18, 72}, {5, 0}, {23, 2}})
        {
            auto corrupt = oneBase;
            corrupt[corrupt.size() - offset] = value;
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
        }
        auto nan = oneBase;
        nan[nan.size() - 2] = 192;
        nan[nan.size() - 1] = 127;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(nan), std::runtime_error);
        state.mNativeActorBases = {npc, creature};
        const auto bytes = state.serializeBinary();
        const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(restored.mNativeActorBases, state.mNativeActorBases);
        EXPECT_EQ(restored.canonicalJson(), state.canonicalJson());
        EXPECT_NE(restored.canonicalJson().find("\"values\":[[8,0,16777217],[9,0,65535],[40,1,2147483648]]"),
            std::string::npos);
        std::reverse(state.mNativeActorBases.begin(), state.mNativeActorBases.end());
        for (auto& base : state.mNativeActorBases)
            std::reverse(base.mValues.begin(), base.mValues.end());
        EXPECT_EQ(state.serializeBinary(), bytes);
        EXPECT_EQ(state.canonicalJson(), restored.canonicalJson());
        for (std::uint32_t version = 1; version <= 10; ++version)
        {
            ESM4::RuntimeState legacy;
            legacy.mVersion = version;
            legacy.mPlayer.mReference = state.mPlayer.mReference;
            legacy.mPlayer.mCell = state.mPlayer.mCell;
            if (version >= 3)
            {
                legacy.mPlayer.mRace = state.mPlayer.mRace;
                legacy.mPlayer.mClass = state.mPlayer.mClass;
            }
            EXPECT_NO_THROW(legacy.serializeBinary());
            auto promoted = ESM4::RuntimeState::deserializeBinary(legacy.serializeBinary());
            EXPECT_TRUE(promoted.mNativeActorBases.empty());
            promoted.mVersion = ESM4::CurrentRuntimeStateVersion;
            promoted.mPlayer.mRace = state.mPlayer.mRace;
            promoted.mPlayer.mClass = state.mPlayer.mClass;
            EXPECT_TRUE(ESM4::RuntimeState::deserializeBinary(promoted.serializeBinary()).mNativeActorBases.empty());
            legacy.mNativeActorBases = {npc};
            EXPECT_THROW(legacy.serializeBinary(), std::runtime_error);
        }
        state.mNativeActorBases.clear();
        const auto countOffset = state.serializeBinary().size() - 4;
        for (std::size_t cut = countOffset; cut < bytes.size(); ++cut)
        {
            auto truncated = bytes;
            truncated.resize(cut);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
        }
        auto corrupt = bytes;
        std::fill_n(corrupt.begin() + countOffset, 4, 255);
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
        state.mVersion = 10;
        const auto old = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        EXPECT_TRUE(old.mNativeActorBases.empty());
        EXPECT_EQ(old.canonicalJson().find("native_actor_bases"), std::string::npos);
    }

    TEST(ESM4RuntimeState, sharedBaseOverridesRejectInvalidStorageAndDuplicates)
    {
        auto state = makeState();
        const ESM4::RuntimeActorBaseOverride valid{ESM::FormKey::content("oblivion.esm", 7), ESM4::ActorBaseKind::Npc,
            {{8, std::int32_t{16777217}}}};
        state.mNativeActorBases = {valid, valid};
        EXPECT_THROW(state.validate(), std::runtime_error);
        state.mNativeActorBases = {valid};
        auto& base = state.mNativeActorBases[0];
        base.mBase = {};
        EXPECT_THROW(state.validate(), std::runtime_error);
        base = valid;
        base.mKind = static_cast<ESM4::ActorBaseKind>(2);
        EXPECT_THROW(state.validate(), std::runtime_error);
        base = valid;
        base.mValues.push_back(base.mValues[0]);
        EXPECT_THROW(state.validate(), std::runtime_error);
        for (const ESM4::ActorBaseValueSet invalid : {ESM4::ActorBaseValueSet{0, std::int32_t{-1}},
                 {7, std::int32_t{256}}, {9, std::int32_t{65536}}, {10, std::int32_t{-1}},
                 {8, 100.f}, {40, std::int32_t{1}}, {11, std::int32_t{0}}, {37, std::int32_t{0}},
                 {72, 0.f}, {40, std::numeric_limits<float>::infinity()}})
        {
            base = valid;
            base.mValues = {invalid};
            EXPECT_THROW(state.validate(), std::runtime_error);
        }
        base = valid;
        base.mKind = ESM4::ActorBaseKind::Creature;
        base.mValues = {{28, std::int32_t{1}}}; // Must store the canonical runtime group key.
        EXPECT_THROW(state.validate(), std::runtime_error);
        base.mValues = {{12, std::int32_t{1}}};
        EXPECT_NO_THROW(state.validate());
    }

    TEST(ESM4RuntimeState, playerFormInputsHaveIndependentVersionTenWireStorage)
    {
        auto state = makeState();
        state.mVersion = 10;
        ESM4::RuntimeActorValues actor;
        actor.mActor = state.mPlayer.mReference;
        actor.mBase = ESM::FormKey::content("oblivion.esm", 7);
        actor.mOwner = ESM4::ActorValueOwner::Player;
        actor.mValues[8].mBase = 123.f; // Resolved cache must not replace raw input.
        actor.mPlayerFormValues = {{-1, 0, std::numeric_limits<std::int32_t>::min(),
            std::numeric_limits<std::int32_t>::max()}};
        state.mNativeActorValues = {actor};
        const auto bytes = state.serializeBinary();
        const std::vector<std::uint8_t> suffix{1, 255, 255, 255, 255, 0, 0, 0, 0,
            0, 0, 0, 128, 255, 255, 255, 127};
        ASSERT_GE(bytes.size(), suffix.size());
        EXPECT_TRUE(std::equal(suffix.begin(), suffix.end(), bytes.end() - suffix.size()));
        auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(restored.mNativeActorValues, state.mNativeActorValues);
        EXPECT_EQ(restored.canonicalJson(), state.canonicalJson());
        EXPECT_NE(state.canonicalJson().find("\"player_form_values\":[-1,0,-2147483648,2147483647]"),
            std::string::npos);
        for (std::size_t remove = 1; remove <= suffix.size(); ++remove)
        {
            auto truncated = bytes;
            truncated.resize(bytes.size() - remove);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
        }
        auto corrupt = bytes;
        corrupt[bytes.size() - suffix.size()] = 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
        state.mVersion = 9;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        EXPECT_THROW(state.canonicalJson(), std::runtime_error);
        state.mVersion = 10;
        state.mNativeActorValues[0].mOwner = ESM4::ActorValueOwner::NonPlayer;
        EXPECT_THROW(state.mNativeActorValues[0].validate(), std::runtime_error);
        state.mNativeActorValues[0] = actor;
        state.mNativeActorValues[0].mPlayerFormValues.reset();
        state.mVersion = 9;
        const auto legacy = state.serializeBinary();
        restored = ESM4::RuntimeState::deserializeBinary(legacy);
        EXPECT_FALSE(restored.mNativeActorValues[0].mPlayerFormValues);
        EXPECT_EQ(restored.mNativeActorValues[0].mValues[8].mBase, 123.f);
        EXPECT_EQ(restored.canonicalJson().find("player_form_values"), std::string::npos);
        restored.mVersion = 10;
        const auto migrated = ESM4::RuntimeState::deserializeBinary(restored.serializeBinary());
        EXPECT_EQ(migrated.mNativeActorValues, restored.mNativeActorValues);
        EXPECT_NE(migrated.canonicalJson().find("\"player_form_values\":null"), std::string::npos);
    }

    TEST(ESM4RuntimeState, passiveInitialMagnitudePreservesVersionNineteenWireAndLegacyUnknown)
    {
        auto state = makeState(); state.mVersion = 18;
        ESM4::RuntimeActorValues actor;
        actor.mActor = state.mReferences[0].mKey; actor.mBase = state.mReferences[0].mBase;
        actor.mPassiveAbilities = {{ESM4::RuntimePassiveAbility{ESM::FormKey::content("abilities.esp", 0x123),
            {{7, ESM::fourCC("FOAT"), 5, -10.f}}}}};
        state.mNativeActorValues = {actor};
        const auto legacy = state.serializeBinary();
        auto restored = ESM4::RuntimeState::deserializeBinary(legacy);
        EXPECT_FALSE(restored.mNativeActorValues[0].mPassiveAbilities->front().mEffects[0].mInitialMagnitude);
        EXPECT_EQ(restored.serializeBinary(), legacy);
        state.mVersion = 19;
        auto& effect = state.mNativeActorValues[0].mPassiveAbilities->front().mEffects[0];
        effect.mInitialMagnitude = 25.f;
        const auto bytes = state.serializeBinary();
        auto expected = legacy; expected[std::string_view("OMW4STATE").size()] = 19;
        const std::array<std::uint8_t, 5> field{1, 0, 0, 0xc8, 0x41};
        expected.insert(expected.end() - 40, field.begin(), field.end());
        EXPECT_EQ(bytes, expected);
        restored = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(restored.mNativeActorValues, state.mNativeActorValues);
        EXPECT_EQ(restored.canonicalJson(), state.canonicalJson());
        EXPECT_EQ(restored.serializeBinary(), bytes);
        effect.mInitialMagnitude.reset();
        const auto unknown = state.serializeBinary();
        expected = legacy; expected[std::string_view("OMW4STATE").size()] = 19;
        expected.insert(expected.end() - 40, 0);
        EXPECT_EQ(unknown, expected);
        EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(unknown).mNativeActorValues[0]
            .mPassiveAbilities->front().mEffects[0].mInitialMagnitude);
    }

    TEST(ESM4RuntimeState, passiveInitialMagnitudeRejectsDowngradeNonfiniteAndMalformedPresence)
    {
        auto state = makeState(); state.mVersion = 19;
        ESM4::RuntimeActorValues actor;
        actor.mActor = state.mReferences[0].mKey; actor.mBase = state.mReferences[0].mBase;
        actor.mPassiveAbilities = {{ESM4::RuntimePassiveAbility{ESM::FormKey::content("abilities.esp", 0x123),
            {{7, ESM::fourCC("FOAT"), 5, -10.f, -0.f}}}}};
        state.mNativeActorValues = {actor};
        const auto bytes = state.serializeBinary();
        const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_TRUE(std::signbit(*restored.mNativeActorValues[0].mPassiveAbilities->front().mEffects[0].mInitialMagnitude));
        EXPECT_EQ(restored.serializeBinary(), bytes);
        state.mVersion = 18;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        EXPECT_THROW(state.canonicalJson(), std::runtime_error);
        state.mVersion = 19;
        auto& effect = state.mNativeActorValues[0].mPassiveAbilities->front().mEffects[0];
        for (float value : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            effect.mInitialMagnitude = value;
            EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        }
        auto corrupt = bytes; corrupt[bytes.size() - 45] = 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
        for (std::size_t i = 0; i < 5; ++i)
        {
            auto truncated = bytes; truncated.resize(bytes.size() - 45 + i);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
        }
    }

    TEST(ESM4RuntimeState, passiveOwnershipPreservesAppliedOrderBitsAndVersionEighteenWire)
    {
        auto state = makeState();
        state.mVersion = 17;
        ESM4::RuntimeActorValues actor;
        actor.mActor = state.mReferences[0].mKey;
        actor.mBase = state.mReferences[0].mBase;
        state.mNativeActorValues = {actor};
        const auto legacy = state.serializeBinary();
        const auto spell = ESM::FormKey::content("abilities.esp", 0x123);
        ESM4::RuntimePassiveAbility ability{spell,
            {{7, ESM::fourCC("FOAT"), 5, -0.f}, {2, ESM::fourCC("FOSP"), 9, 50.f}}};
        std::vector<std::uint8_t> field{1};
        const auto integer = [&](std::uint32_t value) {
            for (unsigned i = 0; i < 4; ++i)
                field.push_back((value >> (8 * i)) & 255);
        };
        integer(1);
        const auto key = spell.serialize();
        integer(static_cast<std::uint32_t>(key.size()));
        field.insert(field.end(), key.begin(), key.end());
        integer(2);
        integer(7); integer(ESM::fourCC("FOAT")); integer(5); integer(0x80000000);
        integer(2); integer(ESM::fourCC("FOSP")); integer(9); integer(0x42480000);
        state.mVersion = 18;
        state.mNativeActorValues[0].mPassiveAbilities = {{ability}};
        const auto bytes = state.serializeBinary();
        auto expected = legacy;
        expected[std::string_view("OMW4STATE").size()] = 18;
        expected.insert(expected.end() - 40, field.begin(), field.end());
        EXPECT_EQ(bytes, expected);
        const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(restored.mNativeActorValues, state.mNativeActorValues);
        EXPECT_TRUE(std::signbit(restored.mNativeActorValues[0].mPassiveAbilities->front().mEffects[0].mStoredMagnitude));
        EXPECT_EQ(restored.serializeBinary(), bytes);
        EXPECT_EQ(restored.canonicalJson(), state.canonicalJson());
        EXPECT_NE(restored.canonicalJson().find("\"passive_abilities\":[{\"spell\":\"" + key), std::string::npos);
        const auto offset = legacy.size() - 40;
        auto corrupt = bytes;
        corrupt[offset] = 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
        for (std::size_t count = 0; count < field.size(); ++count)
        {
            auto truncated = bytes;
            truncated.resize(offset + count);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
        }
        state.mVersion = 17;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        EXPECT_THROW(state.canonicalJson(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, passiveOwnershipDistinguishesLegacyUnknownFromKnownEmpty)
    {
        auto state = makeState();
        state.mVersion = 17;
        ESM4::RuntimeActorValues actor;
        actor.mActor = state.mReferences[0].mKey;
        actor.mBase = state.mReferences[0].mBase;
        state.mNativeActorValues = {actor};
        const auto legacy = state.serializeBinary();
        auto restored = ESM4::RuntimeState::deserializeBinary(legacy);
        EXPECT_FALSE(restored.mNativeActorValues[0].mPassiveAbilities);
        EXPECT_EQ(restored.serializeBinary(), legacy);
        EXPECT_EQ(restored.canonicalJson().find("passive_abilities"), std::string::npos);
        restored.mVersion = 18;
        const auto unknown = restored.serializeBinary();
        EXPECT_NE(restored.canonicalJson().find("\"passive_abilities\":null"), std::string::npos);
        restored.mNativeActorValues[0].mPassiveAbilities.emplace();
        const auto empty = restored.serializeBinary();
        EXPECT_NE(empty, unknown);
        const auto decoded = ESM4::RuntimeState::deserializeBinary(empty);
        ASSERT_TRUE(decoded.mNativeActorValues[0].mPassiveAbilities);
        EXPECT_TRUE(decoded.mNativeActorValues[0].mPassiveAbilities->empty());
        EXPECT_NE(decoded.canonicalJson().find("\"passive_abilities\":[]"), std::string::npos);
        restored.mVersion = 17;
        EXPECT_THROW(restored.serializeBinary(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, passiveOwnershipRejectsInvalidDuplicateOrUnadmittedEffects)
    {
        ESM4::RuntimeActorValues actor;
        actor.mActor = ESM::FormKey::content("actors.esm", 0x900);
        actor.mBase = ESM::FormKey::content("actors.esm", 0x800);
        ESM4::RuntimePassiveAbility valid{ESM::FormKey::content("abilities.esp", 0x123),
            {{0, ESM::fourCC("FOAT"), 5, -10.f}}};
        actor.mPassiveAbilities = {{valid}};
        EXPECT_NO_THROW(actor.validate());
        const auto reject = [&](const ESM4::RuntimePassiveAbility& ability) {
            actor.mPassiveAbilities = {{ability}};
            EXPECT_THROW(actor.validate(), std::runtime_error);
        };
        auto invalid = valid;
        invalid.mSpell = {};
        reject(invalid);
        invalid = valid;
        invalid.mEffects.clear();
        reject(invalid);
        invalid = valid;
        invalid.mEffects.push_back(invalid.mEffects.front());
        reject(invalid);
        invalid = valid;
        invalid.mEffects[0].mCode = ESM::fourCC("SEFF");
        reject(invalid);
        invalid = valid;
        invalid.mEffects[0].mActorValue = 72;
        reject(invalid);
        for (const auto magnitude : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            invalid = valid;
            invalid.mEffects[0].mStoredMagnitude = magnitude;
            reject(invalid);
        }
        actor.mPassiveAbilities = {{valid, valid}};
        EXPECT_THROW(actor.validate(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, nonPlayerFormHealthPreservesIntegerVersionSeventeenWireAndLegacyAbsence)
    {
        auto state = makeState();
        state.mVersion = 17;
        ESM4::RuntimeActorValues actor;
        actor.mActor = state.mReferences[0].mKey;
        actor.mBase = state.mReferences[0].mBase;
        for (const auto health : {std::numeric_limits<std::int32_t>::min(), -16777217, 0, 16777217,
                 std::numeric_limits<std::int32_t>::max()})
        {
            actor.mNonPlayerFormHealth = health;
            actor.mValues[8].mBase = static_cast<float>(health);
            state.mNativeActorValues = {actor};
            const auto bytes = state.serializeBinary();
            // Native v11..16 empty collections and clock occupy 40 bytes
            // after the v17 actor's optional raw Health field.
            const std::size_t offset = bytes.size() - 40 - 5;
            EXPECT_EQ(bytes[offset], 1);
            const auto bits = static_cast<std::uint32_t>(health);
            for (unsigned i = 0; i < 4; ++i)
                EXPECT_EQ(bytes[offset + 1 + i], (bits >> (i * 8)) & 255);
            const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
            EXPECT_EQ(restored.mNativeActorValues, state.mNativeActorValues);
            EXPECT_EQ(restored.canonicalJson(), state.canonicalJson());
            EXPECT_NE(restored.canonicalJson().find("\"nonplayer_form_health\":" + std::to_string(health)),
                std::string::npos);
            auto corrupt = bytes;
            corrupt[offset] = 2;
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
            for (unsigned remove = 1; remove <= 5; ++remove)
            {
                auto truncated = bytes;
                truncated.resize(offset + 5 - remove);
                EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
            }
        }
        state.mVersion = 16;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        EXPECT_THROW(state.canonicalJson(), std::runtime_error);
        state.mNativeActorValues[0].mNonPlayerFormHealth.reset();
        const auto legacy = state.serializeBinary();
        auto restored = ESM4::RuntimeState::deserializeBinary(legacy);
        EXPECT_FALSE(restored.mNativeActorValues[0].mNonPlayerFormHealth);
        EXPECT_EQ(restored.canonicalJson().find("nonplayer_form_health"), std::string::npos);
        EXPECT_EQ(restored.serializeBinary(), legacy);
        restored.mVersion = 17;
        const auto promoted = ESM4::RuntimeState::deserializeBinary(restored.serializeBinary());
        EXPECT_EQ(promoted.mNativeActorValues, restored.mNativeActorValues);
        EXPECT_NE(promoted.canonicalJson().find("\"nonplayer_form_health\":null"), std::string::npos);
    }

    TEST(ESM4RuntimeState, nonPlayerFormHealthRejectsPlayerOwnershipAndRoundedBaseConflicts)
    {
        ESM4::RuntimeActorValues actor;
        actor.mActor = ESM::FormKey::content("actors.esm", 0x900);
        actor.mBase = ESM::FormKey::content("actors.esm", 0x800);
        actor.mNonPlayerFormHealth = 16777217;
        actor.mValues[8].mBase = 16777216.f;
        EXPECT_NO_THROW(actor.validate());
        actor.mValues[8].mBase = 16777218.f;
        EXPECT_THROW(actor.validate(), std::runtime_error);
        actor.mValues[8].mBase = 16777216.f;
        actor.mOwner = ESM4::ActorValueOwner::Player;
        EXPECT_THROW(actor.validate(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, nativeActorValuesRejectInvalidAndDanglingAuthority)
    {
        auto state = makeState();
        ESM4::RuntimeActorValues actor;
        actor.mActor = state.mReferences[0].mKey;
        actor.mBase = state.mReferences[0].mBase;
        state.mNativeActorValues = {actor};
        EXPECT_NO_THROW(state.validate());
        const auto reject = [&](const ESM4::RuntimeActorValues& invalid) {
            state.mNativeActorValues = {invalid};
            EXPECT_THROW(state.serializeBinary(), std::runtime_error);
            EXPECT_THROW(state.canonicalJson(), std::runtime_error);
        };
        auto invalid = actor;
        invalid.mActor = ESM::FormKey::content("oblivion.esm", 0x123);
        reject(invalid);
        invalid = actor;
        invalid.mBase = ESM::FormKey::content("oblivion.esm", 0x123);
        reject(invalid);
        invalid = actor;
        invalid.mOwner = ESM4::ActorValueOwner::Player;
        reject(invalid);
        invalid = actor;
        invalid.mOwner = static_cast<ESM4::ActorValueOwner>(2);
        reject(invalid);
        invalid = actor;
        invalid.mProcess = static_cast<ESM4::ActorValueProcess>(2);
        reject(invalid);
        invalid = actor;
        invalid.mValues[71].mModifiers[2] = std::numeric_limits<float>::quiet_NaN();
        reject(invalid);
        invalid = actor;
        invalid.mValues[8] = {std::numeric_limits<float>::max(), {std::numeric_limits<float>::max(), {}, {}}};
        reject(invalid);
        state.mNativeActorValues = {actor, actor};
        EXPECT_THROW(state.validate(), std::runtime_error);
        state.mNativeActorValues = {actor};
        state.mVersion = 8;
        EXPECT_THROW(state.validate(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, nativeActorValuesCanonicalizeActorOrder)
    {
        auto state = makeState();
        ESM4::RuntimeActorValues player;
        player.mActor = state.mPlayer.mReference;
        player.mBase = ESM::FormKey::content("oblivion.esm", 7);
        player.mOwner = ESM4::ActorValueOwner::Player;
        ESM4::RuntimeActorValues npc;
        npc.mActor = state.mReferences[0].mKey;
        npc.mBase = state.mReferences[0].mBase;
        state.mNativeActorValues = {player, npc};
        const auto bytes = state.serializeBinary();
        const auto json = state.canonicalJson();
        std::reverse(state.mNativeActorValues.begin(), state.mNativeActorValues.end());
        EXPECT_EQ(state.serializeBinary(), bytes);
        EXPECT_EQ(state.canonicalJson(), json);
    }

    TEST(ESM4RuntimeState, everyLegacySchemaMigratesWithoutInventingNativeModifiers)
    {
        for (std::uint32_t version = 1; version < 9; ++version)
        {
            SCOPED_TRACE(version);
            ESM4::RuntimeState state;
            state.mVersion = version;
            state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            state.mPlayer.mCell = ESM::FormKey::content("oblivion.esm", 1);
            if (version >= 3)
            {
                state.mPlayer.mRace = ESM::FormKey::content("oblivion.esm", 2);
                state.mPlayer.mClass = ESM::FormKey::content("oblivion.esm", 3);
            }
            auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
            EXPECT_TRUE(restored.mNativeActorValues.empty());
            EXPECT_EQ(restored.canonicalJson().find("native_actor_values"), std::string::npos);
            restored.mVersion = 9;
            restored.mPlayer.mRace = ESM::FormKey::content("oblivion.esm", 2);
            restored.mPlayer.mClass = ESM::FormKey::content("oblivion.esm", 3);
            EXPECT_TRUE(ESM4::RuntimeState::deserializeBinary(restored.serializeBinary()).mNativeActorValues.empty());
        }
    }

    TEST(ESM4RuntimeState, versionEightPersistsCanonicalPhysicalActions)
    {
        auto state = makeState();
        state.mVersion = 8;
        state.mPhysicalActions = {5, {3, 1}};
        const auto bytes = state.serializeBinary();
        const std::vector<std::uint8_t> suffix{
            5, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0,
            1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0};
        ASSERT_GE(bytes.size(), suffix.size());
        EXPECT_TRUE(std::equal(suffix.begin(), suffix.end(), bytes.end() - suffix.size()));
        const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(restored.mPhysicalActions, (ESM4::ActionLedgerState{5, {1, 3}}));
        EXPECT_EQ(restored.canonicalJson(), state.canonicalJson());
        EXPECT_NE(restored.canonicalJson().find("\"physical_actions\":{\"next\":5,\"pending\":[1,3]}"),
            std::string::npos);
        EXPECT_EQ(restored.serializeBinary(), bytes);
        ESM4::ActionLedger actions;
        actions.restore(restored.mPhysicalActions);
        EXPECT_TRUE(actions.isConsumed(2));
        EXPECT_TRUE(actions.consume(1));
        EXPECT_FALSE(actions.consume(2));
        state.mVersion = 7;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, versionEightRejectsCorruptTruncatedAndOversizedActionLists)
    {
        auto state = makeState();
        state.mVersion = 8;
        for (const auto& invalid : std::vector<ESM4::ActionLedgerState>{
            {0, {}}, {2, {0}}, {2, {2}}, {2, {1, 1}}})
        {
            state.mPhysicalActions = invalid;
            EXPECT_THROW(state.serializeBinary(), std::runtime_error);
            EXPECT_THROW(state.canonicalJson(), std::runtime_error);
        }
        state.mPhysicalActions = {5, {1, 3}};
        const auto bytes = state.serializeBinary();
        for (std::size_t remove = 1; remove <= 28; ++remove)
        {
            auto truncated = bytes;
            truncated.resize(bytes.size() - remove);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
        }
        auto corrupt = bytes;
        corrupt[bytes.size() - 28] = 0; // next ID zero
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
        corrupt = bytes;
        corrupt[bytes.size() - 8] = 1; // duplicate pending ID
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
        corrupt = bytes;
        std::fill(corrupt.end() - 20, corrupt.end() - 16, 0xff); // oversized count
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
    }

    TEST(ESM4RuntimeState, allLegacyVersionsStartWithAnEmptyPhysicalActionNamespace)
    {
        for (std::uint32_t version = 1; version < 8; ++version)
        {
            SCOPED_TRACE(version);
            ESM4::RuntimeState state;
            state.mVersion = version;
            state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            state.mPlayer.mCell = ESM::FormKey::content("oblivion.esm", 1);
            if (version >= 3)
            {
                state.mPlayer.mRace = ESM::FormKey::content("oblivion.esm", 2);
                state.mPlayer.mClass = ESM::FormKey::content("oblivion.esm", 3);
            }
            auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
            EXPECT_EQ(restored.mPhysicalActions, ESM4::ActionLedgerState{});
            EXPECT_EQ(restored.canonicalJson().find("physical_actions"), std::string::npos);
            restored.mVersion = 8;
            restored.mPlayer.mRace = ESM::FormKey::content("oblivion.esm", 2);
            restored.mPlayer.mClass = ESM::FormKey::content("oblivion.esm", 3);
            EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(restored.serializeBinary()).mPhysicalActions,
                ESM4::ActionLedgerState{});
        }
    }

    TEST(ESM4RuntimeState, versionOnePayloadMigratesWithEmptyScriptState)
    {
        auto state = makeState();
        state.mVersion = 1;
        state.mScriptEventSequence = 0;
        state.mScriptInstances.clear();
        state.mQuests.clear();
        state.mPlayer.mName.clear();
        state.mPlayer.mRace = {};
        state.mPlayer.mClass = {};
        state.mPlayer.mBirthSign = {};
        state.mPlayer.mCharacterGenerationFlags = 0;
        state.mPlayer.mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x18baa), 2, -1, -1.f, 0, -1, {}, -1.f } };
        state.mReferences[0].mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x400), -1, -1, -1.f, 0, -1, {}, -1.f } };
        const auto bytes = state.serializeBinary();
        const ESM4::RuntimeState loaded = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(loaded.mVersion, 1u);
        EXPECT_TRUE(loaded.mScriptInstances.empty());
        EXPECT_TRUE(loaded.mQuests.empty());
    }

    TEST(ESM4RuntimeState, versionTwoPayloadMigratesWithoutCharacterGenerationState)
    {
        auto state = makeState();
        state.mVersion = 2;
        state.mPlayer.mName.clear();
        state.mPlayer.mRace = {};
        state.mPlayer.mClass = {};
        state.mPlayer.mBirthSign = {};
        state.mPlayer.mCharacterGenerationFlags = 0;
        state.mPlayer.mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x18baa), 2, -1, -1.f, 0, -1, {}, -1.f } };
        state.mReferences[0].mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x400), -1, -1, -1.f, 0, -1, {}, -1.f } };
        const ESM4::RuntimeState loaded = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        EXPECT_EQ(loaded.mVersion, 2u);
        EXPECT_TRUE(loaded.mPlayer.mRace.isNull());
        EXPECT_EQ(loaded.canonicalJson().find("character_generation_flags"), std::string::npos);
    }

    TEST(ESM4RuntimeState, versionThreePayloadMigratesWithoutM13ItemMetadata)
    {
        auto state = makeState();
        state.mVersion = 3;
        state.mPlayer.mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x18baa), 2, -1, -1.f, 0, -1, {}, -1.f } };
        state.mReferences[0].mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x400), -1, -1, -1.f, 0, -1, {}, -1.f } };
        const ESM4::RuntimeState loaded = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        EXPECT_EQ(loaded.mVersion, 3u);
        ASSERT_EQ(loaded.mPlayer.mInventory.size(), 1u);
        EXPECT_EQ(loaded.mPlayer.mInventory[0].mCondition, -1);
        EXPECT_EQ(loaded.mPlayer.mInventory[0].mCharge, -1.f);
        EXPECT_EQ(loaded.mPlayer.mInventory[0].mEquippedSlots, 0u);
        EXPECT_EQ(loaded.mPlayer.mInventory[0].mHotkey, -1);
        EXPECT_TRUE(loaded.mPlayer.mInventory[0].mOwner.isNull());
        EXPECT_EQ(loaded.mPlayer.mInventory[0].mRemainingUsageTime, -1.f);
    }
    TEST(ESM4RuntimeState, nativeLifeRoundTripKeepsEssentialTimerAndDeathCallbackOrder)
    {
        auto state = makeState();
        const auto& reference = state.mReferences.front();
        state.mNativeActorLife = {
            {reference.mKey, reference.mBase, ESM4::ActorLifePhase::EssentialUnconscious, 3.125f, state.mPlayer.mReference},
            {state.mPlayer.mReference, ESM::FormKey::dynamic("player-base", 1), ESM4::ActorLifePhase::Dead, 0, reference.mKey}};
        state.mNativeActorBases = {{reference.mBase, ESM4::ActorBaseKind::Npc, {{8, std::int32_t{100}}}}};
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mKey;
        values.mBase = reference.mBase;
        values.mValues[8].mBase = 100;
        state.mNativeActorValues = {values};
        state.mNextDeathEvent = 10;
        // The actor may have been resurrected before a queued callback executes.
        state.mPendingDeathEvents = {{4, state.mPlayer.mReference, reference.mKey}, {9, reference.mKey, {}}};
        const auto bytes = state.serializeBinary();
        const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(restored.mNativeActorLife, state.mNativeActorLife);
        EXPECT_EQ(restored.mPendingDeathEvents, state.mPendingDeathEvents);
        EXPECT_EQ(restored.mNextDeathEvent, 10);
        EXPECT_EQ(restored.serializeBinary(), bytes);
        EXPECT_NE(restored.canonicalJson().find("\"recovery_remaining\":3.125"), std::string::npos);
        EXPECT_NE(restored.canonicalJson().find("\"next_death_event\":10"), std::string::npos);
        for (std::size_t remove = 1; remove < 60; ++remove)
        {
            auto truncated = bytes;
            truncated.resize(bytes.size() - remove);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
        }
        auto reordered = state;
        std::reverse(reordered.mNativeActorLife.begin(), reordered.mNativeActorLife.end());
        EXPECT_EQ(reordered.serializeBinary(), bytes);
        std::reverse(reordered.mPendingDeathEvents.begin(), reordered.mPendingDeathEvents.end());
        EXPECT_THROW(reordered.serializeBinary(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, nativeLifeRejectsInvalidPhasesBindingsTimersEventsAndLegacyConflicts)
    {
        auto valid = makeState();
        const auto& reference = valid.mReferences.front();
        valid.mNativeActorLife = {{reference.mKey, reference.mBase, ESM4::ActorLifePhase::Dead, 0, {}}};
        valid.mNextDeathEvent = 3;
        valid.mPendingDeathEvents = {{2, reference.mKey, valid.mPlayer.mReference}};
        valid.validate();
        const auto reject = [&](auto edit) {
            auto state = valid;
            edit(state);
            EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        };
        reject([](auto& state) { state.mNativeActorLife.push_back(state.mNativeActorLife[0]); });
        reject([](auto& state) { state.mNativeActorLife[0].mActor = {}; });
        reject([](auto& state) { state.mNativeActorLife[0].mBase = {}; });
        reject([](auto& state) { state.mNativeActorLife[0].mPhase = static_cast<ESM4::ActorLifePhase>(255); });
        reject([](auto& state) { state.mNativeActorLife[0].mRecoveryRemaining = 1; });
        reject([](auto& state) { state.mNativeActorLife[0].mRecoveryRemaining = -1; });
        reject([](auto& state) { state.mNativeActorLife[0].mRecoveryRemaining = std::numeric_limits<float>::infinity(); });
        reject([](auto& state) { state.mNativeActorLife[0].mKiller = ESM::FormKey::content("missing.esm", 1); });
        reject([](auto& state) { state.mNativeActorLife[0].mPhase = ESM4::ActorLifePhase::Alive;
            state.mNativeActorLife[0].mKiller = state.mPlayer.mReference; });
        reject([](auto& state) { state.mReferences[0].mCustomState["obscript.dead"] = false; });
        reject([](auto& state) { state.mReferences[0].mCustomState["obscript.dead"] = 1.0; });
        reject([](auto& state) { state.mNextDeathEvent = 0; });
        reject([](auto& state) { state.mPendingDeathEvents[0].mId = 0; });
        reject([](auto& state) { state.mPendingDeathEvents[0].mId = 3; });
        reject([](auto& state) { state.mPendingDeathEvents[0].mActor = {}; });
        reject([](auto& state) { state.mPendingDeathEvents[0].mKiller = ESM::FormKey::content("missing.esm", 1); });
        reject([](auto& state) { state.mPendingDeathEvents.push_back(state.mPendingDeathEvents[0]); });
        reject([](auto& state) { state.mVersion = 11; });
        valid.mReferences[0].mCustomState["obscript.dead"] = true;
        EXPECT_NO_THROW(valid.validate());
        valid.mNativeActorLife[0].mPhase = ESM4::ActorLifePhase::EssentialUnconscious;
        valid.mNativeActorLife[0].mRecoveryRemaining = .5f;
        valid.mReferences[0].mCustomState["obscript.dead"] = false;
        EXPECT_NO_THROW(valid.validate());
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mKey;
        values.mBase = ESM::FormKey::content("other.esm", 1);
        valid.mNativeActorValues.push_back(values);
        EXPECT_THROW(valid.validate(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, legacyVersionsDoNotInventLifeOrDeathEventsFromHealth)
    {
        for (std::uint32_t version = 1; version < 12; ++version)
        {
            SCOPED_TRACE(version);
            ESM4::RuntimeState state;
            state.mVersion = version;
            state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            state.mPlayer.mCell = ESM::FormKey::content("oblivion.esm", 1);
            state.mPlayer.mActorValues["health.current"] = -10;
            if (version >= 3)
            {
                state.mPlayer.mRace = ESM::FormKey::content("oblivion.esm", 2);
                state.mPlayer.mClass = ESM::FormKey::content("oblivion.esm", 3);
            }
            const auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
            EXPECT_TRUE(restored.mNativeActorLife.empty());
            EXPECT_TRUE(restored.mPendingDeathEvents.empty());
            EXPECT_EQ(restored.mNextDeathEvent, 1);
            EXPECT_EQ(restored.canonicalJson().find("native_actor_life"), std::string::npos);
        }
    }

    TEST(ESM4RuntimeState, deathCountsRoundTripIndependentOfResidentLife)
    {
        auto state = makeState();
        const auto base = ESM::FormKey::content("actors.esm", 0x123);
        state.mNativeDeathCounts = {{base, 65535}, {ESM::FormKey::dynamic("player-base", 1), 32768}};
        const auto bytes = state.serializeBinary();
        const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(restored, state);
        EXPECT_EQ(restored.canonicalJson(), state.canonicalJson());
        EXPECT_NE(restored.canonicalJson().find("\"count\":65535"), std::string::npos);
        EXPECT_TRUE(restored.mNativeActorLife.empty());
        auto truncated = bytes;
        truncated.pop_back();
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
        state.mVersion = 12;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state.mVersion = 13;
        state.mNativeDeathCounts[{}] = 0;
        EXPECT_THROW(state.validate(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, deathCountsRejectDuplicateAndNoncanonicalBinaryBases)
    {
        auto state = makeState();
        state.mVersion = 13;
        const auto base = ESM::FormKey::content("actors.esm", 0x123);
        state.mNativeDeathCounts = {{base, 1}};
        const auto bytes = state.serializeBinary();
        const auto entrySize = 4 + base.serialize().size() + 2;
        const auto offset = bytes.size() - entrySize - 4;
        auto duplicate = bytes;
        duplicate[offset] = 2;
        duplicate.insert(duplicate.end(), bytes.begin() + offset + 4, bytes.end());
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(duplicate), std::runtime_error);
        auto noncanonical = bytes;
        noncanonical[offset + 8 + 8] = 'A'; // Uppercase first plugin character.
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(noncanonical), std::runtime_error);
        auto oversized = bytes;
        oversized[offset] = 0xff;
        oversized[offset + 1] = 0xff;
        oversized[offset + 2] = 0xff;
        oversized[offset + 3] = 0x7f;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(oversized), std::runtime_error);
    }

    TEST(ESM4RuntimeState, legacyVersionsDoNotInventHistoricalDeathCounts)
    {
        for (std::uint32_t version = 1; version < 13; ++version)
        {
            ESM4::RuntimeState state;
            state.mVersion = version;
            state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            state.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
            if (version >= 3)
            {
                state.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
                state.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
            }
            const auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
            EXPECT_TRUE(restored.mNativeDeathCounts.empty());
            EXPECT_EQ(restored.canonicalJson().find("native_death_counts"), std::string::npos);
        }
    }

    TEST(ESM4RuntimeState, nativeBreathRoundTripsExactFloatsAndRejectsMalformedState)
    {
        auto state = makeState();
        const auto& reference = state.mReferences.front();
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mKey;
        values.mBase = reference.mBase;
        state.mNativeActorValues.push_back(values);
        for (float remaining : {0.f, -1.25f, .125f, 20.f, std::numeric_limits<float>::max()})
        {
            state.mNativeActorBreath = {{reference.mKey, remaining}};
            const auto bytes = state.serializeBinary();
            const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
            EXPECT_EQ(restored, state);
            EXPECT_EQ(restored.canonicalJson(), state.canonicalJson());
            EXPECT_NE(restored.canonicalJson().find("native_actor_breath"), std::string::npos);
            auto truncated = bytes;
            truncated.pop_back();
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
        }
        state.mVersion = 13;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state.mVersion = 14;
        for (float invalid : {std::numeric_limits<float>::infinity(),
                 -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            state.mNativeActorBreath[reference.mKey] = invalid;
            EXPECT_THROW(state.validate(), std::runtime_error);
        }
        state.mNativeActorBreath = {{ESM::FormKey::dynamic("missing", 1), 1}};
        EXPECT_THROW(state.validate(), std::runtime_error);
        state.mNativeActorBreath = {{{}, 1}};
        EXPECT_THROW(state.validate(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, nativeBreathBinaryRejectsDuplicateNoncanonicalAndOversizedEntries)
    {
        auto state = makeState();
        state.mVersion = 14;
        const auto& reference = state.mReferences.front();
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mKey;
        values.mBase = reference.mBase;
        state.mNativeActorValues.push_back(values);
        state.mNativeActorBreath = {{reference.mKey, .125f}};
        const auto bytes = state.serializeBinary();
        const auto offset = bytes.size() - (4 + reference.mKey.serialize().size() + 4) - 4;
        auto duplicate = bytes;
        duplicate[offset] = 2;
        duplicate.insert(duplicate.end(), bytes.begin() + offset + 4, bytes.end());
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(duplicate), std::runtime_error);
        auto noncanonical = bytes;
        noncanonical[offset + 8 + 8] = 'O';
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(noncanonical), std::runtime_error);
        auto oversized = bytes;
        std::fill_n(oversized.begin() + offset, 4, 0xff);
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(oversized), std::runtime_error);
        auto nonfinite = bytes;
        const std::array<std::uint8_t, 4> infinity{0, 0, 0x80, 0x7f};
        std::copy(infinity.begin(), infinity.end(), nonfinite.end() - 4);
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(nonfinite), std::runtime_error);
    }

    TEST(ESM4RuntimeState, legacyVersionsLeaveNativeBreathUninitialized)
    {
        for (std::uint32_t version = 1; version < 14; ++version)
        {
            ESM4::RuntimeState state;
            state.mVersion = version;
            state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            state.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
            if (version >= 3)
            {
                state.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
                state.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
            }
            const auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
            EXPECT_TRUE(restored.mNativeActorBreath.empty());
            EXPECT_EQ(restored.canonicalJson().find("native_actor_breath"), std::string::npos);
        }
    }



    ESM4::RuntimeState engagedState()
    {
        auto state = makeState();
        const auto& reference = state.mReferences.front();
        ESM4::RuntimeActorValues actor;
        actor.mActor = reference.mKey;
        actor.mBase = reference.mBase;
        state.mNativeActorValues.push_back(actor);
        actor.mActor = state.mPlayer.mReference;
        actor.mBase = ESM::FormKey::dynamic("player-base", 1);
        actor.mOwner = ESM4::ActorValueOwner::Player;
        state.mNativeActorValues.push_back(actor);
        for (const auto& value : state.mNativeActorValues)
            state.mNativeActorLife.push_back({value.mActor, value.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        state.mNativeCombatEngagements.emplace(reference.mKey, state.mPlayer.mReference);
        return state;
    }

    TEST(ESM4RuntimeState, combatEngagementRoundTripAndEndpointValidation)
    {
        const auto state = engagedState();
        EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()), state);
        EXPECT_NE(state.canonicalJson().find("native_combat_engagements"), std::string::npos);
        auto invalid = state;
        invalid.mVersion = 14;
        EXPECT_THROW(invalid.validate(), std::runtime_error);
        invalid = state;
        invalid.mNativeActorValues.pop_back();
        EXPECT_THROW(invalid.validate(), std::runtime_error);
        invalid = state;
        invalid.mNativeActorLife.pop_back();
        EXPECT_THROW(invalid.validate(), std::runtime_error);
        invalid = state;
        invalid.mNativeActorLife.front().mPhase = ESM4::ActorLifePhase::Dead;
        EXPECT_THROW(invalid.validate(), std::runtime_error);
        invalid.mNativeActorLife.front().mPhase = ESM4::ActorLifePhase::EssentialUnconscious;
        EXPECT_NO_THROW(invalid.validate()); // Membership survives a recoverable knockout.
        const auto [first, second] = *state.mNativeCombatEngagements.begin();
        for (const auto& pair : {std::pair{first, first}, std::pair{second, first},
                 std::pair{ESM::FormKey{}, second}, std::pair{first, ESM::FormKey::dynamic("missing", 1)}})
        {
            invalid = state;
            invalid.mNativeCombatEngagements = {pair};
            EXPECT_THROW(invalid.validate(), std::runtime_error);
        }
    }

    TEST(ESM4RuntimeState, combatEngagementWireRejectsDuplicatesOversizeAndTruncation)
    {
        auto state = engagedState();
        state.mVersion = 15; // This test mutates the v15 trailing engagement section.
        const auto bytes = state.serializeBinary();
        const auto [first, second] = *state.mNativeCombatEngagements.begin();
        const auto size = 8 + first.serialize().size() + second.serialize().size();
        const auto offset = bytes.size() - size - 4;
        auto invalid = bytes;
        invalid[offset] = 2;
        invalid.insert(invalid.end(), bytes.begin() + offset + 4, bytes.end());
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
        invalid = bytes;
        std::fill_n(invalid.begin() + offset, 4, 0xff);
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
        invalid = bytes;
        invalid[offset + 8] = 'C'; // Noncanonical key spelling.
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
        for (std::size_t cut = 1; cut <= size + 4; ++cut)
        {
            SCOPED_TRACE(cut);
            invalid.assign(bytes.begin(), bytes.end() - cut);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
        }
    }

    TEST(ESM4RuntimeState, versionsOneThroughFourteenDoNotInventCombatEngagements)
    {
        for (std::uint32_t version = 1; version < 15; ++version)
        {
            ESM4::RuntimeState state;
            state.mVersion = version;
            state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            state.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
            if (version >= 3)
            {
                state.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
                state.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
            }
            const auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
            EXPECT_TRUE(restored.mNativeCombatEngagements.empty());
            EXPECT_EQ(restored.canonicalJson().find("native_combat_engagements"), std::string::npos);
        }
    }

    TEST(ESM4RuntimeState, nativeActorClocksPreserveExactBitsAndRejectInvalidState)
    {
        auto state = engagedState();
        const auto actor = state.mNativeActorValues.front().mActor;
        for (const float time : {0.f, -0.f, -1.f, .125f, 100000.f, -std::numeric_limits<float>::max()})
        {
            state.mNativeActorManagerTime = time;
            state.mNativeActorUpdateTimes = {{actor, time}};
            const auto bytes = state.serializeBinary();
            const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
            EXPECT_EQ(restored, state);
            EXPECT_EQ(restored.serializeBinary(), bytes);
            EXPECT_EQ(std::signbit(restored.mNativeActorManagerTime), std::signbit(time));
            EXPECT_EQ(std::signbit(restored.mNativeActorUpdateTimes.at(actor)), std::signbit(time));
            EXPECT_EQ(restored.canonicalJson(), state.canonicalJson());
            if (time == 0.f && std::signbit(time))
            {
                EXPECT_NE(restored.canonicalJson().find("\"native_actor_manager_time\":-0."), std::string::npos);
                EXPECT_NE(restored.canonicalJson().find("\"time\":-0."), std::string::npos);
            }
        }
        for (const float time : {std::nextafter(100000.f, 100001.f), std::numeric_limits<float>::infinity(),
                 -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            auto invalid = state;
            invalid.mNativeActorManagerTime = time;
            EXPECT_THROW(invalid.validate(), std::runtime_error);
            invalid = state;
            invalid.mNativeActorUpdateTimes[actor] = time;
            EXPECT_THROW(invalid.validate(), std::runtime_error);
        }
        state.mNativeActorUpdateTimes = {{ESM::FormKey::dynamic("missing", 1), 0}};
        EXPECT_THROW(state.validate(), std::runtime_error);
        state.mNativeActorUpdateTimes.clear();
        state.mVersion = 15;
        EXPECT_THROW(state.validate(), std::runtime_error);
        state.mNativeActorManagerTime = 0;
        state.mNativeActorUpdateTimes = {{actor, 0}};
        EXPECT_THROW(state.validate(), std::runtime_error);
    }

    TEST(ESM4RuntimeState, actorClockWireRejectsDuplicateNoncanonicalOversizedAndTruncatedData)
    {
        for (unsigned version = 16; version <= ESM4::CurrentRuntimeStateVersion; ++version)
        {
            SCOPED_TRACE(version);
            auto state = engagedState();
            state.mVersion = version;
            const auto actor = state.mNativeActorValues.front().mActor;
            state.mNativeActorManagerTime = 99999.5f;
            state.mNativeActorUpdateTimes = {{actor, 99999.f}};
            const auto bytes = state.serializeBinary();
            // Later schemas append empty ownership/melee/animation collections
            // and the combat seed. Locate the clock section, not payload end.
            const unsigned suffix = (version >= 20 ? 4 : 0) + (version >= 21 ? 4 : 0)
                + (version >= 23 ? 4 : 0) + (version >= 27 ? 4 : 0)
                + (version >= 30 ? 4 : 0) + (version >= 31 ? 4 : 0)
                + (version >= 36 ? 1 : 0) + (version >= 37 ? 1 : 0) + (version >= 38 ? 4 : 0)
                + (version >= 42 ? 36 : 0) + (version >= 53 ? 4 : 0);
            const auto clockEnd = bytes.size() - suffix;
            const auto entrySize = 8 + actor.serialize().size();
            const auto countOffset = clockEnd - entrySize - 4;
            ASSERT_EQ(bytes[countOffset], 1);
            ASSERT_EQ(std::string(bytes.begin() + countOffset + 8, bytes.begin() + clockEnd - 4), actor.serialize());
            auto invalid = bytes;
            invalid[countOffset] = 2;
            invalid.insert(invalid.begin() + clockEnd, bytes.begin() + countOffset + 4, bytes.begin() + clockEnd);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
            invalid = bytes;
            std::fill_n(invalid.begin() + countOffset, 4, 0xff);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
            invalid = bytes;
            invalid[countOffset + 8] = 'C';
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
            for (const auto offset : {clockEnd - 4, countOffset - 4})
            {
                invalid = bytes;
                const std::array<std::uint8_t, 4> infinity{0, 0, 0x80, 0x7f};
                std::copy(infinity.begin(), infinity.end(), invalid.begin() + offset);
                EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
            }
            for (std::size_t cut = 1; cut <= entrySize + 8; ++cut)
            {
                SCOPED_TRACE(cut);
                invalid.assign(bytes.begin(), bytes.begin() + clockEnd - cut);
                EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
            }
        }
    }

    TEST(ESM4RuntimeState, olderVersionsLeaveActorClocksUninitialized)
    {
        for (std::uint32_t version = 1; version < 16; ++version)
        {
            ESM4::RuntimeState state;
            state.mVersion = version;
            state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            state.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
            if (version >= 3)
            {
                state.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
                state.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
            }
            const auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
            EXPECT_EQ(restored.mNativeActorManagerTime, 0);
            EXPECT_TRUE(restored.mNativeActorUpdateTimes.empty());
            EXPECT_EQ(restored.canonicalJson().find("native_actor_manager_time"), std::string::npos);
            EXPECT_EQ(restored.canonicalJson().find("native_actor_update_times"), std::string::npos);
        }
    }

    ESM4::RuntimeState ownedActionState()
    {
        auto state = makeState();
        state.mVersion = 20; // These owner-tail tests deliberately retain the v20 wire.
        const auto& reference = state.mReferences.front();
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mKey; values.mBase = reference.mBase;
        state.mNativeActorValues = {values};
        state.mNativeActorLife = {{values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}}};
        state.mPhysicalActions = {7, {5, 2, 1}};
        state.mPhysicalActionOwners = {{5, values.mActor}, {2, values.mActor}};
        return state;
    }

    TEST(ESM4RuntimeState, physicalActionOwnersHaveVersionTwentyCanonicalWireAndLegacyIsolation)
    {
        auto state = ownedActionState();
        const auto bytes = state.serializeBinary();
        const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(restored.mPhysicalActionOwners, state.mPhysicalActionOwners);
        EXPECT_EQ(restored.serializeBinary(), bytes);
        EXPECT_EQ(restored.canonicalJson(), state.canonicalJson());
        EXPECT_NE(state.canonicalJson().find("\"physical_action_owners\":[{\"id\":2,\"actor\":\"content:oblivion.esm:000100\"},{\"id\":5"), std::string::npos);
        state.mVersion = 19;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        state.mPhysicalActionOwners.clear();
        const auto old = state.serializeBinary();
        auto promoted = old;
        promoted[std::string_view("OMW4STATE").size()] = 20;
        promoted.insert(promoted.end(), 4, 0);
        state.mVersion = 20;
        EXPECT_EQ(state.serializeBinary(), promoted);
        EXPECT_TRUE(ESM4::RuntimeState::deserializeBinary(old).mPhysicalActionOwners.empty());
        EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(old).canonicalJson().find("physical_action_owners"), std::string::npos);
    }

    TEST(ESM4RuntimeState, physicalActionOwnersRejectInvalidBindingsAndCorruptTail)
    {
        auto state = ownedActionState();
        const auto reject = [&](auto change) {
            auto broken = state; change(broken);
            EXPECT_THROW(broken.serializeBinary(), std::runtime_error);
        };
        reject([](auto& x) { x.mPhysicalActionOwners.emplace(0, x.mNativeActorValues[0].mActor); });
        reject([](auto& x) { x.mPhysicalActionOwners.emplace(3, x.mNativeActorValues[0].mActor); });
        reject([](auto& x) { x.mPhysicalActionOwners[2] = {}; });
        reject([](auto& x) { x.mPhysicalActionOwners[2] = ESM::FormKey::dynamic("missing", 1); });
        reject([](auto& x) { x.mNativeActorValues.clear(); });
        reject([](auto& x) { x.mNativeActorLife.clear(); });
        reject([](auto& x) { x.mNativeActorLife[0].mPhase = ESM4::ActorLifePhase::Dead; });
        reject([](auto& x) { x.mNativeActorLife[0].mPhase = ESM4::ActorLifePhase::EssentialUnconscious; });
        state.mPhysicalActionOwners.erase(5);
        const auto bytes = state.serializeBinary();
        const auto key = state.mPhysicalActionOwners.begin()->second.serialize();
        const auto tailSize = 4 + 8 + 4 + key.size();
        const auto start = bytes.size() - tailSize;
        EXPECT_EQ(bytes[start], 1);
        EXPECT_EQ(bytes[start + 4], 2);
        for (std::size_t cut = 1; cut <= tailSize; ++cut)
        {
            auto bad = bytes; bad.resize(bytes.size() - cut);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad), std::runtime_error);
        }
        auto duplicate = bytes;
        duplicate[start] = 2;
        duplicate.insert(duplicate.end(), bytes.begin() + start + 4, bytes.end());
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(duplicate), std::runtime_error);
        auto excessive = bytes;
        std::fill_n(excessive.begin() + start, 4, 255);
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(excessive), std::runtime_error);
        auto noncanonical = bytes;
        noncanonical[start + 16] = 'C';
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(noncanonical), std::runtime_error);
    }

}

namespace
{
    ESM4::RuntimeState meleeState()
    {
        auto state = ownedActionState();
        state.mVersion = 21;
        ESM4::RuntimeMeleeState melee;
        melee.mInput = {.25f, true, false, ESM4::MeleeQueuedStrike::Power};
        melee.mStrike = ESM4::RuntimeMeleeStrike{2, ESM4::MeleeStrikeKind::ForwardPower,
            ESM::FormKey::content("oblivion.esm", 0x400), "onehandattackforwardpower", 1.25f, .5f, false};
        state.mNativeMeleeStates.emplace(state.mReferences.front().mKey, melee);
        return state;
    }

    TEST(ESM4RuntimeState, meleeStateVersionTwentyOneExactWireAndCommittedFollowThrough)
    {
        auto state = meleeState();
        const auto actor = state.mReferences.front().mKey;
        auto legacy = state;
        legacy.mNativeMeleeStates.clear(); legacy.mVersion = 20;
        auto expected = legacy.serializeBinary();
        expected[std::string_view("OMW4STATE").size()] = 21;
        const auto integer = [&](std::uint64_t value, unsigned bytes) {
            for (unsigned i = 0; i < bytes; ++i) expected.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
        };
        const auto text = [&](std::string_view value) {
            integer(value.size(), 4); expected.insert(expected.end(), value.begin(), value.end());
        };
        integer(1, 4); text("content:oblivion.esm:000100");
        integer(0x3e800000, 4); integer(1, 1); integer(0, 1); integer(2, 1); integer(1, 1);
        integer(2, 8); integer(3, 1); text("content:oblivion.esm:000400");
        text("onehandattackforwardpower"); integer(0x3fa00000, 4);
        integer(0x3f000000, 4); integer(0, 1);
        for (unsigned kind = 0; kind <= 6; ++kind)
        for (unsigned queued = 0; queued <= 2; ++queued)
        {
            auto candidate = state;
            auto& melee = candidate.mNativeMeleeStates.at(actor);
            melee.mStrike->mKind = static_cast<ESM4::MeleeStrikeKind>(kind);
            melee.mInput.mQueued = static_cast<ESM4::MeleeQueuedStrike>(queued);
            EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(candidate.serializeBinary()).mNativeMeleeStates,
                candidate.mNativeMeleeStates);
        }
        const auto bytes = state.serializeBinary();
        EXPECT_EQ(bytes, expected);
        auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(restored.mNativeMeleeStates, state.mNativeMeleeStates);
        EXPECT_EQ(restored.serializeBinary(), bytes);
        EXPECT_EQ(restored.canonicalJson(), state.canonicalJson());
        auto& melee = state.mNativeMeleeStates.at(actor);
        melee.mStrike->mContactCommitted = true;
        std::erase(state.mPhysicalActions.mPending, 2);
        state.mPhysicalActionOwners.erase(2);
        restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        EXPECT_TRUE(restored.mNativeMeleeStates.at(actor).mStrike->mContactCommitted);
        EXPECT_EQ(restored.mNativeMeleeStates.at(actor).mStrike->mAnimationTime, .5f);
        EXPECT_EQ(restored.mNativeMeleeStates.at(actor).mInput.mQueued, ESM4::MeleeQueuedStrike::Power);
        melee.mInput.mInputHeld = false; // Release does not erase a queued strike.
        melee.mStrike.reset();
        restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        EXPECT_FALSE(restored.mNativeMeleeStates.at(actor).mStrike);
        EXPECT_FALSE(restored.mNativeMeleeStates.at(actor).mInput.mInputHeld);
        EXPECT_EQ(restored.mNativeMeleeStates.at(actor).mInput.mQueued, ESM4::MeleeQueuedStrike::Power);
        EXPECT_TRUE(ESM4::RuntimeState::deserializeBinary(legacy.serializeBinary()).mNativeMeleeStates.empty());
    }

    TEST(ESM4RuntimeState, meleeStateRejectsReplayingUnownedIncapacitatedAndMalformedContinuation)
    {
        const auto state = meleeState();
        const auto actor = state.mReferences.front().mKey;
        const auto reject = [&](auto change) {
            auto broken = state; change(broken);
            EXPECT_THROW(broken.serializeBinary(), std::runtime_error);
        };
        reject([](auto& b) { b.mVersion = 20; });
        reject([&](auto& b) { b.mNativeMeleeStates.at(actor).mInput.mQueued = static_cast<ESM4::MeleeQueuedStrike>(3); });
        for (float value : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            reject([&](auto& b) { b.mNativeMeleeStates.at(actor).mInput.mHeldSeconds = value; });
            reject([&](auto& b) { b.mNativeMeleeStates.at(actor).mStrike->mAnimationTime = value; });
        }
        reject([&](auto& b) { b.mNativeMeleeStates.at(actor).mStrike->mKind = static_cast<ESM4::MeleeStrikeKind>(7); });
        for (std::uint64_t id : {0u, 1u, 6u, 7u})
            reject([&](auto& b) { b.mNativeMeleeStates.at(actor).mStrike->mActionId = id; });
        reject([&](auto& b) { b.mNativeMeleeStates.at(actor).mStrike->mAnimationGroup.clear(); });
        reject([&](auto& b) { b.mNativeMeleeStates.at(actor).mStrike->mAnimationGroup = std::string("bad\0name", 8); });
        for (float speed : {0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
            reject([&](auto& b) { b.mNativeMeleeStates.at(actor).mStrike->mPlaybackSpeed = speed; });
        reject([&](auto& b) { b.mNativeMeleeStates.at(actor).mStrike->mContactCommitted = true; });
        reject([](auto& b) { b.mPhysicalActionOwners.erase(2); });
        reject([](auto& b) { std::erase(b.mPhysicalActions.mPending, 2); b.mPhysicalActionOwners.erase(2); });
        reject([&](auto& b) { auto node = b.mNativeMeleeStates.extract(actor);
            node.key() = ESM::FormKey::content("foreign.esm", 0x100); b.mNativeMeleeStates.insert(std::move(node)); });
        reject([](auto& b) { b.mPhysicalActionOwners.clear(); b.mPhysicalActions.mPending.clear();
            b.mNativeActorLife[0].mPhase = ESM4::ActorLifePhase::EssentialUnconscious; });
        auto old = state; old.mNativeMeleeStates.clear(); old.mVersion = 20;
        const auto prefix = old.serializeBinary().size();
        const auto bytes = state.serializeBinary();
        for (std::size_t end = prefix; end < bytes.size(); ++end)
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary({bytes.begin(), bytes.begin() + end}), std::exception);
        const auto inputOffset = prefix + 8 + actor.serialize().size();
        for (const std::size_t offset : {inputOffset + 4, inputOffset + 5, inputOffset + 7, bytes.size() - 1})
        {
            auto broken = bytes; broken[offset] = 2;
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(broken), std::runtime_error);
        }
        auto duplicate = bytes; duplicate[prefix] = 2;
        duplicate.insert(duplicate.end(), bytes.begin() + prefix + 4, bytes.end());
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(duplicate), std::runtime_error);
        auto empty = old; empty.mVersion = 21;
        auto expected = old.serializeBinary(); expected[std::string_view("OMW4STATE").size()] = 21;
        expected.insert(expected.end(), 4, 0);
        EXPECT_EQ(empty.serializeBinary(), expected);
    }
}

namespace
{
    TEST(ESM4RuntimeState, ordinaryMeleePhaseVersionTwentyTwoWireMigrationAndCorruption)
    {
        auto old = meleeState();
        const auto actor = old.mReferences.front().mKey;
        const auto oldBytes = old.serializeBinary();
        auto expected = oldBytes;
        expected[std::string_view("OMW4STATE").size()] = 22;
        expected.push_back(0);
        auto current = ESM4::RuntimeState::deserializeBinary(oldBytes);
        current.mVersion = 22;
        EXPECT_EQ(current.serializeBinary(), expected);
        EXPECT_EQ(current.mNativeMeleeStates.at(actor).mStrike->mOrdinaryPhase, ESM4::OrdinaryMeleePhase::Start);
        auto sortedActions = old.mPhysicalActions;
        std::sort(sortedActions.mPending.begin(), sortedActions.mPending.end());
        EXPECT_EQ(current.mPhysicalActions, sortedActions);
        EXPECT_EQ(current.mPhysicalActionOwners, old.mPhysicalActionOwners);
        for (unsigned phase = 0; phase <= 3; ++phase)
        for (bool committed : {false, true})
        {
            auto candidate = current;
            auto& strike = *candidate.mNativeMeleeStates.at(actor).mStrike;
            strike.mOrdinaryPhase = static_cast<ESM4::OrdinaryMeleePhase>(phase);
            strike.mContactCommitted = committed;
            if (committed)
            {
                std::erase(candidate.mPhysicalActions.mPending, strike.mActionId);
                candidate.mPhysicalActionOwners.erase(strike.mActionId);
            }
            const auto bytes = candidate.serializeBinary();
            EXPECT_EQ(bytes.back(), phase);
            EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(bytes).mNativeMeleeStates, candidate.mNativeMeleeStates);
            EXPECT_NE(candidate.canonicalJson().find("\"ordinary_phase\":" + std::to_string(phase)), std::string::npos);
        }
        auto bad = current;
        bad.mNativeMeleeStates.at(actor).mStrike->mOrdinaryPhase = static_cast<ESM4::OrdinaryMeleePhase>(4);
        EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
        auto corrupt = expected; corrupt.back() = 255;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
        auto truncated = expected; truncated.pop_back();
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
        bad = current; bad.mVersion = 21;
        bad.mNativeMeleeStates.at(actor).mStrike->mOrdinaryPhase = ESM4::OrdinaryMeleePhase::Contact;
        EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
        EXPECT_EQ(old.serializeBinary(), oldBytes); // Historical v21 remains byte-identical.
    }
}

TEST(ESM4RuntimeState, NativeKnockbackVersionThirtyWireAndIndependentLifetime)
{
    auto old = meleeState();
    old.mVersion = 29;
    const auto actor = old.mReferences.front().mKey;
    const auto legacyBytes = old.serializeBinary();
    auto state = old;
    state.mVersion = 30;
    auto expected = legacyBytes;
    expected[std::string_view("OMW4STATE").size()] = 30;
    expected.insert(expected.end(), 4, 0);
    EXPECT_EQ(state.serializeBinary(), expected);
    EXPECT_TRUE(ESM4::RuntimeState::deserializeBinary(legacyBytes).mNativeActorKnockback.empty());
    state.mNativeActorKnockback.emplace(actor, ESM4::TimedKnockbackState{{-0.f, -2, 3}, .125f});
    const auto bytes = state.serializeBinary();
    const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
    EXPECT_EQ(restored.mNativeActorKnockback, state.mNativeActorKnockback);
    EXPECT_TRUE(std::signbit(restored.mNativeActorKnockback.at(actor).mAcceleration[0]));
    EXPECT_NE(restored.canonicalJson().find("\"acceleration\":[-0.0,-2,3],\"remaining\":0.125"), std::string::npos);
    state.mNativeMeleeStates.clear();
    state.mPhysicalActionOwners.clear();
    state.mPhysicalActions.mPending.clear();
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()).mNativeActorKnockback,
        state.mNativeActorKnockback);
    for (float bad : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        auto malformed = state;
        malformed.mNativeActorKnockback.at(actor).mRemaining = bad;
        EXPECT_THROW(malformed.serializeBinary(), std::runtime_error);
    }
    auto malformed = state;
    malformed.mNativeActorKnockback.emplace(ESM::FormKey{}, ESM4::TimedKnockbackState{});
    EXPECT_THROW(malformed.serializeBinary(), std::runtime_error);
    malformed = state;
    malformed.mNativeActorKnockback.at(actor).mAcceleration[1] = std::numeric_limits<float>::infinity();
    EXPECT_THROW(malformed.serializeBinary(), std::runtime_error);
    malformed = state; malformed.mVersion = 29;
    EXPECT_THROW(malformed.serializeBinary(), std::runtime_error);
    auto truncated = bytes; truncated.pop_back();
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
    EXPECT_EQ(old.serializeBinary(), legacyBytes);
}

TEST(ESM4RuntimeState, NativeRagdollVersionThirtyOneWireAndMigration)
{
    auto old = meleeState();
    old.mVersion = 30;
    const auto actor = old.mReferences.front().mKey;
    const auto oldBytes = old.serializeBinary();
    auto state = old;
    state.mVersion = 31;
    auto expected = oldBytes;
    expected[std::string_view("OMW4STATE").size()] = 31;
    expected.insert(expected.end(), 4, 0);
    EXPECT_EQ(state.serializeBinary(), expected);
    EXPECT_TRUE(ESM4::RuntimeState::deserializeBinary(oldBytes).mNativeActorRagdolls.empty());
    ESM4::RuntimeActorRagdoll pose;
    pose.mBase = old.mReferences.front().mBase;
    pose.mModel = "meshes/characters/_male/skeleton.nif";
    pose.mAssetHash = "00112233445566778899aabbccddeeff";
    ESM4::RuntimeRagdollBody body;
    body.mRecord = 12;
    body.mNodeRecord = 8;
    body.mPosition = {-0.f, -2, 3};
    body.mLinearVelocity = {1, 2, 3};
    body.mAngularVelocity = {4, 5, 6};
    pose.mBodies.push_back(body);
    state.mNativeActorRagdolls.emplace(actor, pose);
    const auto bytes = state.serializeBinary();
    const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
    EXPECT_EQ(restored.mNativeActorRagdolls, state.mNativeActorRagdolls);
    EXPECT_TRUE(std::signbit(restored.mNativeActorRagdolls.at(actor).mBodies[0].mPosition[0]));
    EXPECT_NE(restored.canonicalJson().find("\"position\":[-0.0,-2,3]"), std::string::npos);
    const auto appendInteger = [&](std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i)
            expected.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    };
    const auto appendString = [&](const std::string& text) {
        appendInteger(text.size());
        expected.insert(expected.end(), text.begin(), text.end());
    };
    expected.resize(expected.size() - 4);
    appendInteger(1);
    appendString(actor.serialize());
    appendString(pose.mBase.serialize());
    appendString(pose.mModel);
    appendString(pose.mAssetHash);
    appendInteger(1);
    appendInteger(12);
    appendInteger(8);
    for (const auto& values : {body.mRotation, std::array<float, 9>{-0.f, -2, 3, 1, 2, 3, 4, 5, 6}})
        for (float value : values)
            appendInteger(std::bit_cast<std::uint32_t>(value));
    EXPECT_EQ(bytes, expected);
    for (std::size_t size = oldBytes.size(); size < bytes.size(); ++size)
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary({bytes.begin(), bytes.begin() + size}), std::runtime_error);
    EXPECT_EQ(old.serializeBinary(), oldBytes);
}

TEST(ESM4RuntimeState, NativeRagdollRejectsInvalidAssetOwnerAndBodySnapshots)
{
    auto state = meleeState();
    state.mVersion = 31;
    const auto actor = state.mReferences.front().mKey;
    ESM4::RuntimeActorRagdoll pose;
    pose.mBase = state.mReferences.front().mBase;
    pose.mModel = "meshes/characters/_male/skeleton.nif";
    pose.mAssetHash = "00112233445566778899aabbccddeeff";
    ESM4::RuntimeRagdollBody snapshotBody;
    snapshotBody.mRecord = 12;
    snapshotBody.mNodeRecord = 8;
    pose.mBodies.push_back(snapshotBody);
    state.mNativeActorRagdolls.emplace(actor, pose);
    ASSERT_NO_THROW(state.serializeBinary());
    const auto rejects = [&](auto mutate) {
        auto invalid = state;
        mutate(invalid);
        EXPECT_THROW(invalid.serializeBinary(), std::runtime_error);
    };
    rejects([](auto& s) { s.mVersion = 30; });
    rejects([](auto& s) { s.mNativeActorLife.clear(); });
    rejects([](auto& s) { s.mNativeActorValues.clear(); });
    rejects([&](auto& s) { s.mNativeActorRagdolls.at(actor).mBase = ESM::FormKey{}; });
    for (const std::string model : {"", "../skeleton.nif", "/skeleton.nif", "Meshes/skeleton.nif", "meshes\\skeleton.nif"})
        rejects([&](auto& s) { s.mNativeActorRagdolls.at(actor).mModel = model; });
    for (const std::string hash : {"", "invalid", "00112233445566778899AABBCCDDEEFF"})
        rejects([&](auto& s) { s.mNativeActorRagdolls.at(actor).mAssetHash = hash; });
    rejects([&](auto& s) { s.mNativeActorRagdolls.at(actor).mBodies.clear(); });
    rejects([&](auto& s) { s.mNativeActorRagdolls.at(actor).mBodies.push_back(pose.mBodies[0]); });
    rejects([&](auto& s) { auto body = pose.mBodies[0]; body.mRecord = 13; s.mNativeActorRagdolls.at(actor).mBodies.push_back(body); });
    rejects([&](auto& s) { s.mNativeActorRagdolls.at(actor).mBodies[0].mRotation[0] = -1; });
    rejects([&](auto& s) { s.mNativeActorRagdolls.at(actor).mBodies[0].mRotation[1] = .5f; });
    for (float bad : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        for (unsigned field = 0; field < 4; ++field)
            rejects([&](auto& s) {
                auto& body = s.mNativeActorRagdolls.at(actor).mBodies[0];
                switch (field)
                {
                    case 0: body.mRotation[0] = bad; break;
                    case 1: body.mPosition[0] = bad; break;
                    case 2: body.mLinearVelocity[0] = bad; break;
                    default: body.mAngularVelocity[0] = bad; break;
                }
            });
}

TEST(ESM4RuntimeState, NativeAnimationClockAndSequenceTimingPersistIndependentlyOfMeleeInput)
{
    auto state = meleeState();
    state.mVersion = 23;
    const auto actor = state.mReferences.front().mKey;
    state.mNativeAnimationClocks.emplace(actor, 1000.199951171875f);
    ESM4::MeleeSequenceTiming timing;
    timing.mEasing = true;
    timing.mEaseEnd = .1f;
    state.mNativeMeleeStates.at(actor).mStrike->mSequenceTiming = timing;
    auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
    EXPECT_EQ(restored.mNativeAnimationClocks, state.mNativeAnimationClocks);
    EXPECT_EQ(restored.mNativeMeleeStates, state.mNativeMeleeStates);
    timing = ESM4::updateMeleeSequenceTiming(timing, 1000.199951171875f, 1, 0, 1);
    state.mNativeMeleeStates.at(actor).mStrike->mSequenceTiming = timing;
    restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
    EXPECT_EQ(restored.mNativeMeleeStates, state.mNativeMeleeStates);
    EXPECT_NE(restored.canonicalJson().find("\"sequence_timing\":{"), std::string::npos);
    EXPECT_NE(restored.canonicalJson().find("\"native_animation_clocks\":["), std::string::npos);
    state.mNativeAnimationClocks.at(actor) = -0.f;
    timing.mOffset = -0.f; timing.mEaseStart = -0.f; timing.mLastInput = -0.f;
    timing.mEaseEnd = -0.f; timing.mWeightedTime = -0.f; timing.mOutputTime = -0.f;
    state.mNativeMeleeStates.at(actor).mStrike->mSequenceTiming = timing;
    const auto json = state.canonicalJson();
    for (const std::string name : {"clock", "offset", "ease_start", "last_input", "ease_end", "weighted_time", "output_time"})
        EXPECT_NE(json.find("\"" + name + "\":-0.0"), std::string::npos);
    restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
    EXPECT_TRUE(std::signbit(restored.mNativeAnimationClocks.at(actor)));
    EXPECT_TRUE(std::signbit(*restored.mNativeMeleeStates.at(actor).mStrike->mSequenceTiming->mOffset));
    state.mNativeMeleeStates.clear();
    state.mPhysicalActionOwners.clear();
    state.mPhysicalActions.mPending.clear();
    state.mNativeActorLife.front().mPhase = ESM4::ActorLifePhase::Dead;
    state.mNativeCombatEngagements.clear();
    state.mNativeActorLife.front().mRecoveryRemaining = 0;
    EXPECT_NO_THROW(state.validate()); // Global clock survives incapacitation.
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()).mNativeAnimationClocks,
        state.mNativeAnimationClocks);
    state.mVersion = 22;
    EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    state.mNativeAnimationClocks.clear();
    EXPECT_TRUE(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()).mNativeAnimationClocks.empty());
}

TEST(ESM4RuntimeState, NativeAnimationTimingRejectsDanglingPartialAndNonfiniteState)
{
    auto state = meleeState(); state.mVersion = 23;
    const auto actor = state.mReferences.front().mKey;
    state.mNativeAnimationClocks[actor] = 0;
    state.mNativeMeleeStates.at(actor).mStrike->mSequenceTiming.emplace();
    auto bad = state;
    bad.mNativeAnimationClocks.clear();
    EXPECT_THROW(bad.validate(), std::runtime_error);
    bad = state;
    bad.mNativeAnimationClocks[ESM::FormKey::dynamic("missing", 1)] = 0;
    EXPECT_THROW(bad.validate(), std::runtime_error);
    bad = state; bad.mVersion = 22;
    EXPECT_THROW(bad.validate(), std::runtime_error);
    for (float value : {std::numeric_limits<float>::quiet_NaN(),
             std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -1.f})
    {
        bad = state; bad.mNativeAnimationClocks[actor] = value;
        EXPECT_THROW(bad.validate(), std::runtime_error);
    }
    bad = state;
    bad.mNativeMeleeStates.at(actor).mStrike->mSequenceTiming->mOffset = 0;
    EXPECT_THROW(bad.validate(), std::runtime_error); // Partial initialization isn't canonical.
    const auto bytes = state.serializeBinary();
    const auto text = actor.serialize();
    const auto clockStart = bytes.size() - (4 + 4 + text.size() + 4);
    auto duplicate = bytes;
    duplicate[clockStart] = 2;
    duplicate.insert(duplicate.end(), bytes.begin() + clockStart + 4, bytes.end());
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(duplicate), std::runtime_error);
    auto excessive = bytes;
    std::fill_n(excessive.begin() + clockStart, 4, 255);
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(excessive), std::runtime_error);
    auto noncanonical = bytes;
    noncanonical[clockStart + 8 + std::string_view("content:").size()] = 'O';
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(noncanonical), std::runtime_error);
    const auto timingStart = clockStart - 17;
    for (unsigned flag = 0; flag < 5; ++flag)
    {
        auto invalidBoolean = bytes;
        invalidBoolean[timingStart + flag] = 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalidBoolean), std::runtime_error);
    }
    for (std::size_t cut = 1; cut <= 20; ++cut)
    {
        auto truncated = bytes; truncated.resize(bytes.size()-cut);
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
    }
}

TEST(ESM4RuntimeState, Condition24PreservesNativeFloatBitsAndRejectsLossyDowngrade)
{
    for (const std::uint32_t bits : {0u, 0x80000000u, 1u, 0x33800000u,
            0x3eaaaaabu, 0x42c7ffffu, 0x43000000u, 0x7f7fffffu})
    {
        auto state = makeState();
        state.mPlayer.mInventory.front().mCondition = std::bit_cast<float>(bits);
        const auto bytes = state.serializeBinary();
        const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(static_cast<float>(restored.mPlayer.mInventory.front().mCondition)), bits);
        EXPECT_EQ(restored.serializeBinary(), bytes);
        if (bits == 0x80000000u)
        {
            EXPECT_NE(restored.canonicalJson().find("\"condition\":-0.0"), std::string::npos);
        }
        if (bits != 0u && bits != 0x43000000u)
        {
            state.mVersion = 23;
            EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        }
    }
    auto state = makeState();
    for (const double value : {-0.5, -2., std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), std::numeric_limits<double>::max()})
    {
        state.mPlayer.mInventory.front().mCondition = value;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }
}

TEST(ESM4RuntimeState, LegacyConditionKeepsFullSignedIntegerRangeBeforePromotion)
{
    auto state = makeState();
    state.mActorAi.clear();
    state.mPlayer.mInventory.front().mCondition = std::numeric_limits<std::int32_t>::max();
    state.mVersion = 23;
    const auto bytes = state.serializeBinary();
    const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
    EXPECT_EQ(restored.mPlayer.mInventory.front().mCondition, 2147483647.);
    EXPECT_EQ(restored.serializeBinary(), bytes);
    auto promoted = restored;
    promoted.mVersion = 24;
    const auto native = ESM4::RuntimeState::deserializeBinary(promoted.serializeBinary());
    EXPECT_EQ(native.mPlayer.mInventory.front().mCondition, 2147483648.);
}

TEST(ESM4RuntimeState, NativeProcessKnockedByteRetainsUnknownLegacyAndRejectsLossyDowngrade)
{
    auto state = makeState();
    state.mVersion = 24;
    ESM4::RuntimeActorValues actor;
    actor.mActor = state.mPlayer.mReference;
    actor.mBase = ESM::FormKey::content("oblivion.esm", 7);
    actor.mOwner = ESM4::ActorValueOwner::Player;
    state.mNativeActorValues = {actor};
    const auto legacy = state.serializeBinary();
    auto restored = ESM4::RuntimeState::deserializeBinary(legacy);
    EXPECT_FALSE(restored.mNativeActorValues[0].mProcessKnockedState);
    EXPECT_EQ(restored.serializeBinary(), legacy);
    EXPECT_EQ(restored.canonicalJson().find("process_knocked_state"), std::string::npos);
    restored.mVersion = 25;
    const auto unknown = restored.serializeBinary();
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(unknown).mNativeActorValues[0].mProcessKnockedState);
    EXPECT_NE(restored.canonicalJson().find("\"process_knocked_state\":null"), std::string::npos);
    restored.mNativeActorValues[0].mProcessKnockedState = 0;
    const auto known = restored.serializeBinary();
    EXPECT_NE(known, unknown);
    // Only the presence marker differs until the inserted signed byte.
    const auto differing = std::mismatch(unknown.begin(), unknown.end(), known.begin(), known.end());
    auto corrupt = known;
    corrupt[std::distance(unknown.begin(), differing.first)] = 2;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
    restored.mVersion = 24;
    EXPECT_THROW(restored.serializeBinary(), std::runtime_error);
    restored.mVersion = 25;
    restored.mNativeActorValues[0].mProcess = ESM4::ActorValueProcess::Low;
    EXPECT_THROW(restored.serializeBinary(), std::runtime_error);
    restored.mNativeActorValues[0].mProcessKnockedState.reset();
    EXPECT_NO_THROW(restored.serializeBinary());
}

TEST(ESM4RuntimeState, CombatRandom27PreservesUnsignedSeedsAndRejectsLossyDowngrade)
{
    for (const std::uint32_t seed : {0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu})
    {
        auto state = makeState();
        state.mCombatRngState = seed;
        state.mAiRngState = 0x123456789abcdef0ull;
        const auto bytes = state.serializeBinary();
        const auto loaded = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(loaded.mCombatRngState, seed);
        EXPECT_EQ(loaded.mAiRngState, state.mAiRngState);
        EXPECT_EQ(loaded.serializeBinary(), bytes);
        EXPECT_NE(loaded.canonicalJson().find("\"combat_rng_state\":" + std::to_string(seed)), std::string::npos);
        auto truncated = bytes;
        truncated.pop_back();
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
        state.mVersion = 26;
        if (seed != 1)
        {
            EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        }
        else
        {
            const auto legacy = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
            EXPECT_EQ(legacy.mCombatRngState, 1u);
            EXPECT_EQ(legacy.canonicalJson().find("combat_rng_state"), std::string::npos);
        }
    }
    for (unsigned version = 1; version < 27; ++version)
    {
        auto state = makeState();
        state.mVersion = version;
        state.mPlayer.mInventory.clear();
        state.mReferences.clear();
        state.mScriptEventSequence = 0;
        state.mScriptInstances.clear();
        state.mQuests.clear();
        if (version < 3)
        {
            state.mPlayer.mName.clear();
            state.mPlayer.mRace = {};
            state.mPlayer.mClass = {};
            state.mPlayer.mBirthSign = {};
            state.mPlayer.mCharacterGenerationFlags = 0;
        }
        EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()).mCombatRngState, 1u);
    }
}

TEST(ESM4RuntimeState, MeleeAi28PersistsOwnedTargetAndRejectsDanglingOrLossyState)
{
    auto state = meleeState();
    state.mVersion = 28;
    std::sort(state.mPhysicalActions.mPending.begin(), state.mPhysicalActions.mPending.end());
    const auto actor = state.mReferences.front().mKey;
    const auto target = state.mPlayer.mReference;
    ESM4::RuntimeActorValues playerValues;
    playerValues.mActor = target;
    playerValues.mBase = ESM::FormKey::dynamic("player-base", 1);
    playerValues.mOwner = ESM4::ActorValueOwner::Player;
    state.mNativeActorValues.push_back(playerValues);
    state.mNativeActorLife.push_back({target, playerValues.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
    state.mNativeCombatEngagements.emplace(actor, target);
    auto& melee = state.mNativeMeleeStates.at(actor);
    melee.mStrike.reset();
    melee.mAiIntent = ESM4::RuntimeMeleeAiIntent{target, ESM::FormKey::content("oblivion.esm", 0x600)};
    const auto bytes = state.serializeBinary();
    auto legacyWire = state;
    legacyWire.mVersion = 27;
    legacyWire.mNativeMeleeStates.at(actor).mAiIntent.reset();
    auto expected = legacyWire.serializeBinary();
    expected[std::string_view("OMW4STATE").size()] = 28;
    std::vector<std::uint8_t> intentWire{1};
    for (const auto& key : {target, melee.mAiIntent->mStyle})
    {
        const auto text = key.serialize();
        for (unsigned byte = 0; byte < 4; ++byte)
            intentWire.push_back(static_cast<std::uint8_t>(text.size() >> (8 * byte)));
        intentWire.insert(intentWire.end(), text.begin(), text.end());
    }
    expected.insert(expected.end() - 8, intentWire.begin(), intentWire.end());
    EXPECT_EQ(bytes, expected);
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(bytes), state);
    EXPECT_NE(state.canonicalJson().find("ai_intent"), std::string::npos);
    auto invalid = state;
    invalid.mVersion = 27;
    EXPECT_THROW(invalid.validate(), std::runtime_error);
    invalid = state;
    invalid.mNativeMeleeStates.at(actor).mAiIntent->mTarget = actor;
    EXPECT_THROW(invalid.validate(), std::runtime_error);
    invalid = state;
    invalid.mNativeMeleeStates.at(actor).mAiIntent->mTarget = ESM::FormKey::content("missing.esm", 1);
    EXPECT_THROW(invalid.validate(), std::runtime_error);
    invalid = state;
    invalid.mNativeMeleeStates.at(actor).mAiIntent->mStyle = {};
    EXPECT_THROW(invalid.validate(), std::runtime_error);
    invalid = state;
    invalid.mNativeCombatEngagements.clear();
    EXPECT_THROW(invalid.validate(), std::runtime_error);
    invalid = state;
    invalid.mNativeMeleeStates.clear();
    invalid.mNativeMeleeStates.emplace(target, melee);
    EXPECT_THROW(invalid.validate(), std::runtime_error);
    invalid = state;
    for (auto& life : invalid.mNativeActorLife)
        if (life.mActor == target) life.mPhase = ESM4::ActorLifePhase::EssentialUnconscious;
    EXPECT_THROW(invalid.validate(), std::runtime_error);
    for (unsigned version = 21; version < 28; ++version)
    {
        auto legacy = state;
        legacy.mVersion = version;
        legacy.mNativeMeleeStates.at(actor).mAiIntent.reset();
        const auto restored = ESM4::RuntimeState::deserializeBinary(legacy.serializeBinary());
        EXPECT_FALSE(restored.mNativeMeleeStates.at(actor).mAiIntent);
        EXPECT_EQ(restored.canonicalJson().find("ai_intent"), std::string::npos);
    }
    const auto tailSize = 1 + 8 + target.serialize().size() + melee.mAiIntent->mStyle.serialize().size();
    auto malformed = bytes;
    malformed[bytes.size() - 8 - tailSize] = 2;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(malformed), std::runtime_error);
    for (std::size_t cut = 1; cut <= tailSize + 8; ++cut)
    {
        auto truncated = bytes;
        truncated.resize(bytes.size() - cut);
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
    }
}

TEST(ESM4RuntimeState, NativeActorDraw29HasExactNullableWireAndRejectsCorruptFlags)
{
    auto state = ownedActionState();
    state.mVersion = 29;
    state.mPhysicalActions.mPending.clear();
    state.mPhysicalActionOwners.clear();
    constexpr std::string_view marker = "native-draw-wire-boundary";
    state.mReferences.front().mCustomState = {{"boundary", std::string(marker)}};
    auto legacy = state;
    legacy.mVersion = 28;
    const auto legacyBytes = legacy.serializeBinary();
    const auto found = std::search(legacyBytes.begin(), legacyBytes.end(), marker.begin(), marker.end());
    ASSERT_NE(found, legacyBytes.end());
    const auto boundary = std::distance(legacyBytes.begin(), found) + marker.size();
    const std::array<std::optional<ESM4::ActorDrawState>, 4> states{
        std::nullopt, ESM4::ActorDrawState::Nothing, ESM4::ActorDrawState::Weapon, ESM4::ActorDrawState::Spell};
    for (const auto draw : states)
    {
        SCOPED_TRACE(draw ? static_cast<int>(*draw) : -1);
        state.mReferences.front().mActorDrawState = draw;
        auto expected = legacyBytes;
        expected[std::string_view("OMW4STATE").size()] = 29;
        std::vector<std::uint8_t> wire{static_cast<std::uint8_t>(draw.has_value())};
        if (draw)
            wire.push_back(static_cast<std::uint8_t>(*draw));
        expected.insert(expected.begin() + boundary, wire.begin(), wire.end());
        const auto bytes = state.serializeBinary();
        EXPECT_EQ(bytes, expected);
        EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(bytes), state);
        const auto jsonValue = draw ? std::to_string(static_cast<unsigned>(*draw)) : "null";
        EXPECT_NE(state.canonicalJson().find("\"actor_draw_state\":" + jsonValue), std::string::npos);
        auto invalid = bytes;
        invalid[boundary] = 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
        if (draw)
        {
            invalid = bytes;
            invalid[boundary + 1] = 255;
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
            invalid.assign(bytes.begin(), bytes.begin() + boundary + 1);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
        }
    }
}

TEST(ESM4RuntimeState, NativeActorDraw29RequiresMatchingNonPlayerValueAndLifeOwnership)
{
    auto state = ownedActionState();
    state.mVersion = 29;
    state.mPhysicalActions.mPending.clear();
    state.mPhysicalActionOwners.clear();
    state.mReferences.front().mActorDrawState = ESM4::ActorDrawState::Weapon;
    ASSERT_NO_THROW(state.validate());
    const auto reject = [&](auto mutate) {
        auto invalid = state;
        mutate(invalid);
        EXPECT_THROW(invalid.serializeBinary(), std::runtime_error);
        EXPECT_THROW(invalid.canonicalJson(), std::runtime_error);
    };
    reject([](auto& x) { x.mVersion = 28; });
    reject([](auto& x) { x.mReferences.front().mActorDrawState = static_cast<ESM4::ActorDrawState>(3); });
    reject([](auto& x) { x.mNativeActorValues.clear(); });
    reject([](auto& x) { x.mNativeActorLife.clear(); });
    reject([](auto& x) { x.mNativeActorLife.front().mBase = ESM::FormKey::content("oblivion.esm", 0x201); });
    reject([](auto& x) {
        auto extra = x.mReferences.front();
        extra.mKey = ESM::FormKey::content("oblivion.esm", 0x101);
        x.mReferences.push_back(std::move(extra));
    });
    reject([](auto& x) { x.mReferences.front().mKey = x.mPlayer.mReference; });
}

TEST(ESM4RuntimeState, OlderNativeReferencesDoNotInventDrawState)
{
    for (unsigned version = 1; version < 29; ++version)
    {
        SCOPED_TRACE(version);
        ESM4::RuntimeState state;
        state.mVersion = version;
        state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        state.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        if (version >= 3)
        {
            state.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
            state.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        }
        ESM4::RuntimeReferenceState reference;
        reference.mKey = ESM::FormKey::content("actors.esm", 4);
        reference.mBase = ESM::FormKey::content("actors.esm", 5);
        reference.mCell = state.mPlayer.mCell;
        state.mReferences.push_back(reference);
        const auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        ASSERT_EQ(restored.mReferences.size(), 1u);
        EXPECT_FALSE(restored.mReferences.front().mActorDrawState);
        EXPECT_EQ(restored.canonicalJson().find("actor_draw_state"), std::string::npos);
        state.mReferences.front().mActorDrawState = ESM4::ActorDrawState::Nothing;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }
}


namespace
{
    ESM4::RuntimeState packedRagdollState()
    {
        auto state = meleeState(); state.mVersion = 32;
        ESM4::RuntimeActorRagdoll pose;
        pose.mBase = state.mReferences.front().mBase;
        pose.mModel = "meshes/characters/_male/skeleton.nif";
        pose.mAssetHash = "00112233445566778899aabbccddeeff";
        ESM4::RuntimeRagdollBody body; body.mRecord = 12; body.mNodeRecord = 8;
        body.mNativePackedVelocity = ESM4::PhysicalWorldSceneVelocities{
            {66.83216857910156f, -133.66433715820312f, 200.41783142089844f, -0.f},
            {26.377605438232422f, -52.755210876464844f, 79.13282012939453f, .2110208421945572f}};
        pose.mBodies.push_back(body);
        state.mNativeActorRagdolls.emplace(state.mReferences.front().mKey, pose);
        return state;
    }
}

TEST(ESM4RuntimeState, NativePackedRagdollVersionThirtyTwoWireAndLegacyAbsence)
{
    auto state = packedRagdollState(); const auto actor = state.mReferences.front().mKey;
    const auto native = *state.mNativeActorRagdolls.at(actor).mBodies[0].mNativePackedVelocity;
    auto old = state; old.mVersion = 31;
    old.mNativeActorRagdolls.at(actor).mBodies[0].mNativePackedVelocity.reset();
    const auto legacy = old.serializeBinary();
    auto expected = legacy; expected[std::string_view("OMW4STATE").size()] = 32;
    expected.push_back(1);
    for (const auto& vector : {native.mLinear, native.mAngular})
        for (float value : vector)
        {
            const auto bits = std::bit_cast<std::uint32_t>(value);
            for (unsigned byte = 0; byte < 4; ++byte)
                expected.push_back(static_cast<std::uint8_t>(bits >> (byte * 8)));
        }
    EXPECT_EQ(state.serializeBinary(), expected);
    const auto decoded = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
    EXPECT_EQ(decoded.mNativeActorRagdolls, state.mNativeActorRagdolls);
    ASSERT_TRUE(decoded.mNativeActorRagdolls.at(actor).mBodies[0].mNativePackedVelocity);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(decoded.mNativeActorRagdolls.at(actor).mBodies[0].mNativePackedVelocity->mLinear[3]), 0x80000000u);
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(legacy).mNativeActorRagdolls.at(actor).mBodies[0].mNativePackedVelocity);
    EXPECT_EQ(old.serializeBinary(), legacy);
    old.mVersion = 32; auto absent = legacy; absent[std::string_view("OMW4STATE").size()] = 32; absent.push_back(0);
    EXPECT_EQ(old.serializeBinary(), absent);
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(absent).mNativeActorRagdolls, old.mNativeActorRagdolls);
    EXPECT_NE(decoded.canonicalJson().find("\"native_packed_velocity\":{\"linear\":[66.832168579101562,-133.66433715820312,200.41783142089844,-0.0]"), std::string::npos);
}

TEST(ESM4RuntimeState, NativePackedRagdollRejectsMalformedVersionPresenceAndPayload)
{
    auto state = packedRagdollState(); const auto actor = state.mReferences.front().mKey;
    auto invalid = state; invalid.mVersion = 31;
    EXPECT_THROW(invalid.serializeBinary(), std::runtime_error);
    invalid = state; invalid.mNativeActorRagdolls.at(actor).mBodies[0].mNativePackedVelocity->mAngular[3] = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(invalid.validate(), std::runtime_error);
    invalid = state; auto extra = invalid.mNativeActorRagdolls.at(actor).mBodies[0];
    extra.mRecord = 13; extra.mNodeRecord = 9; extra.mNativePackedVelocity.reset();
    invalid.mNativeActorRagdolls.at(actor).mBodies.push_back(extra);
    EXPECT_THROW(invalid.validate(), std::runtime_error);
    auto payload = state.serializeBinary();
    ASSERT_GE(payload.size(), 33u);
    for (std::size_t cut = payload.size() - 33; cut < payload.size(); ++cut)
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary({payload.begin(), payload.begin() + cut}), std::runtime_error);
    payload[payload.size() - 33] = 2;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(payload), std::runtime_error);
}


TEST(ESM4RuntimeState, NativeMotionRagdollVersionThirtyThreeWireAndLegacyAbsence)
{
    auto state = packedRagdollState(); const auto actor = state.mReferences.front().mKey;
    const auto legacy = state.serializeBinary();
    state.mVersion = 33;
    state.mNativeActorRagdolls.at(actor).mBodies[0].mNativeMotion = ESM4::RuntimeRagdollMotion::Keyframed;
    auto expected = legacy; expected[std::string_view("OMW4STATE").size()] = 33;
    expected.push_back(1); expected.push_back(6);
    EXPECT_EQ(state.serializeBinary(), expected);
    const auto decoded = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
    EXPECT_EQ(decoded.mNativeActorRagdolls, state.mNativeActorRagdolls);
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(legacy).mNativeActorRagdolls.at(actor).mBodies[0].mNativeMotion);
    EXPECT_NE(decoded.canonicalJson().find("\"native_motion\":6"), std::string::npos);
    state.mNativeActorRagdolls.at(actor).mBodies[0].mNativeMotion.reset();
    expected.pop_back(); expected.back() = 0;
    EXPECT_EQ(state.serializeBinary(), expected);
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(expected).mNativeActorRagdolls.at(actor).mBodies[0].mNativeMotion);
    state.mNativeActorRagdolls.at(actor).mBodies[0].mNativeMotion = ESM4::RuntimeRagdollMotion::Dynamic;
    expected.back() = 1; expected.push_back(1);
    EXPECT_EQ(state.serializeBinary(), expected);
}

TEST(ESM4RuntimeState, NativeMotionRagdollRejectsMalformedVersionPresenceAndPayload)
{
    auto state = packedRagdollState(); state.mVersion = 33; const auto actor = state.mReferences.front().mKey;
    state.mNativeActorRagdolls.at(actor).mBodies[0].mNativeMotion = ESM4::RuntimeRagdollMotion::Keyframed;
    auto invalid = state; invalid.mVersion = 32;
    EXPECT_THROW(invalid.serializeBinary(), std::runtime_error);
    for (auto value : {0, 2, 255})
    {
        invalid = state; invalid.mNativeActorRagdolls.at(actor).mBodies[0].mNativeMotion = static_cast<ESM4::RuntimeRagdollMotion>(value);
        EXPECT_THROW(invalid.validate(), std::runtime_error);
    }
    invalid = state; invalid.mNativeActorRagdolls.at(actor).mBodies[0].mNativePackedVelocity.reset();
    EXPECT_THROW(invalid.validate(), std::runtime_error);
    invalid = state; auto extra = invalid.mNativeActorRagdolls.at(actor).mBodies[0];
    extra.mRecord = 13; extra.mNodeRecord = 9; extra.mNativeMotion.reset();
    invalid.mNativeActorRagdolls.at(actor).mBodies.push_back(extra);
    EXPECT_THROW(invalid.validate(), std::runtime_error);
    const auto payload = state.serializeBinary();
    ASSERT_GE(payload.size(), 2u);
    for (std::size_t cut = payload.size() - 2; cut < payload.size(); ++cut)
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary({payload.begin(), payload.begin() + cut}), std::runtime_error);
    invalid = state; auto bad = payload; bad[bad.size() - 2] = 2;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad), std::runtime_error);
    bad = payload; bad.back() = 2; // Native getter2 is not logical Dynamic1.
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad), std::runtime_error);
}


TEST(ESM4RuntimeState, NativeBlendRagdollVersionThirtyFourWireAndLegacyAbsence)
{
    auto state = packedRagdollState(); const auto actor = state.mReferences.front().mKey;
    auto& pose = state.mNativeActorRagdolls.at(actor);
    state.mVersion = 33; pose.mBodies[0].mNativeMotion = ESM4::RuntimeRagdollMotion::Keyframed;
    const auto legacy = state.serializeBinary();
    state.mVersion = 34;
    pose.mNativeBlends = std::vector<ESM4::RuntimeRagdollBlendState>{{12, 0xf123, 0xffffffffu, -0.f, -2.f}};
    auto expected = legacy; expected[std::string_view("OMW4STATE").size()] = 34;
    const std::vector<std::uint8_t> suffix{1, 1,0,0,0, 12,0,0,0, 0x23,0xf1,
        0xff,0xff,0xff,0xff, 0,0,0,0x80, 0,0,0,0xc0};
    expected.insert(expected.end(), suffix.begin(), suffix.end());
    EXPECT_EQ(state.serializeBinary(), expected);
    const auto restored = ESM4::RuntimeState::deserializeBinary(expected);
    const auto& blend = restored.mNativeActorRagdolls.at(actor).mNativeBlends->front();
    EXPECT_EQ(blend.mRequestedMotion, 0xffffffffu); EXPECT_EQ(blend.mCollisionFlags, 0xf123);
    EXPECT_TRUE(std::signbit(blend.mHierarchyGain)); EXPECT_EQ(blend.mVelocityGain, -2.f);
    EXPECT_EQ(restored.mNativeActorRagdolls, state.mNativeActorRagdolls);
    EXPECT_EQ(restored.serializeBinary(), expected);
    EXPECT_NE(restored.canonicalJson().find("\"native_blends\":["), std::string::npos);
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(legacy).mNativeActorRagdolls.at(actor).mNativeBlends);
    expected.resize(expected.size() - suffix.size()); expected.push_back(0); pose.mNativeBlends.reset();
    EXPECT_EQ(state.serializeBinary(), expected);
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(expected).mNativeActorRagdolls.at(actor).mNativeBlends);
    pose.mNativeBlends.emplace(); expected.back() = 1; expected.insert(expected.end(), 4, 0);
    EXPECT_EQ(state.serializeBinary(), expected);
    EXPECT_TRUE(ESM4::RuntimeState::deserializeBinary(expected).mNativeActorRagdolls.at(actor).mNativeBlends->empty());
    EXPECT_NE(state.canonicalJson().find("\"native_blends\":[]"), std::string::npos);
    // A complete blend set can be a proper subset of the body set.
    auto extra = pose.mBodies[0]; extra.mRecord = 24; extra.mNodeRecord = 16; pose.mBodies.push_back(extra);
    pose.mNativeBlends = std::vector<ESM4::RuntimeRagdollBlendState>{{12, 8, 1, -2, 3}};
    EXPECT_NO_THROW(state.validate());
}

TEST(ESM4RuntimeState, NativeBlendRagdollRejectsMalformedVersionPresenceAndPayload)
{
    auto state = packedRagdollState(); state.mVersion = 34; const auto actor = state.mReferences.front().mKey;
    auto& pose = state.mNativeActorRagdolls.at(actor); pose.mBodies[0].mNativeMotion = ESM4::RuntimeRagdollMotion::Keyframed;
    pose.mNativeBlends = std::vector<ESM4::RuntimeRagdollBlendState>{{12, 8, 1, -2, 3}};
    auto invalid = state; invalid.mVersion = 33; EXPECT_THROW(invalid.serializeBinary(), std::runtime_error);
    invalid = state; invalid.mNativeActorRagdolls.at(actor).mBodies[0].mNativeMotion.reset();
    EXPECT_THROW(invalid.validate(), std::runtime_error);
    invalid = state; invalid.mNativeActorRagdolls.at(actor).mBodies[0].mNativePackedVelocity.reset();
    EXPECT_THROW(invalid.validate(), std::runtime_error);
    invalid = state; invalid.mNativeActorRagdolls.at(actor).mNativeBlends->front().mBodyRecord = 13;
    EXPECT_THROW(invalid.validate(), std::runtime_error);
    invalid = state; invalid.mNativeActorRagdolls.at(actor).mNativeBlends->push_back(pose.mNativeBlends->front());
    EXPECT_THROW(invalid.validate(), std::runtime_error);
    for (bool hierarchy : {false, true})
    {
        invalid = state; auto& blend = invalid.mNativeActorRagdolls.at(actor).mNativeBlends->front();
        (hierarchy ? blend.mHierarchyGain : blend.mVelocityGain) = std::numeric_limits<float>::infinity();
        EXPECT_THROW(invalid.validate(), std::runtime_error);
    }
    const auto payload = state.serializeBinary(); ASSERT_GE(payload.size(), 23u);
    for (std::size_t cut = payload.size() - 23; cut < payload.size(); ++cut)
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary({payload.begin(), payload.begin() + cut}), std::runtime_error);
    auto malformed = payload; malformed[malformed.size() - 23] = 2;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(malformed), std::runtime_error);
    malformed = payload; malformed[malformed.size() - 22] = 2;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(malformed), std::runtime_error);
    // Reverse a valid two-target list without changing its count or payload size.
    auto extra = pose.mBodies[0]; extra.mRecord = 24; extra.mNodeRecord = 16; pose.mBodies.push_back(extra);
    pose.mNativeBlends->push_back({24, 0xffff, 6, 1, 0}); EXPECT_NO_THROW(state.validate());
    std::reverse(pose.mNativeBlends->begin(), pose.mNativeBlends->end());
    EXPECT_THROW(state.validate(), std::runtime_error);
}


namespace
{
    ESM4::RuntimeState controllerRagdollState()
    {
        auto state = packedRagdollState(); state.mVersion = 35;
        auto& pose = state.mNativeActorRagdolls.begin()->second;
        pose.mBodies[0].mNativeMotion = ESM4::RuntimeRagdollMotion::Keyframed;
        pose.mNativeBlends.emplace(); pose.mNativeControllers.emplace();
        ESM4::RuntimeRagdollBlendController blend;
        blend.mRecord = 78; blend.mAttachedNode = 8;
        blend.mState.mTiming = {0xffff, -1, -0.f, 1, -1};
        blend.mState.mClock.mElapsed = -0.f; blend.mState.mCursor = 0xffffffffu;
        blend.mState.mCachedGains = {-0.f, -2}; blend.mState.mSetupState = 0xffffffffu;
        blend.mState.mKeys = {{0, {1, -0.f}}};
        pose.mNativeControllers->mBlends.push_back(blend);
        ESM4::RuntimeRagdollVelocityController velocity;
        velocity.mAttachedNode = 8; velocity.mTargetNode = 8; velocity.mPrecedesBlend = false;
        velocity.mState.mTiming = {0xd, 1, -0.f, 0, 4}; velocity.mState.mClock = {10, 11, 1};
        velocity.mState.mForceVector = {1, 2, 3, -0.f}; velocity.mState.mFrameDelta = .016f;
        pose.mNativeControllers->mVelocities.push_back(velocity);
        return state;
    }
    std::vector<std::uint8_t> controllerRagdollGoldenSuffix()
    {
        std::vector<std::uint8_t> bytes;
        const auto integer = [&](std::uint32_t value, unsigned size) {
            for (unsigned i = 0; i < size; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
        };
        const auto scalar = [&](float value) { integer(std::bit_cast<std::uint32_t>(value), 4); };
        integer(1, 1); integer(1, 4); // present, authored count
        integer(78, 4); integer(8, 4); integer(0, 1); integer(0xffff, 2);
        for (float value : {-1.f, -0.f, 1.f, -1.f, -std::numeric_limits<float>::max(),
                -std::numeric_limits<float>::max(), -0.f}) scalar(value);
        integer(0xffffffffu, 4); scalar(-0.f); scalar(-2); integer(0xffffffffu, 4);
        integer(1, 4); scalar(0); scalar(1); scalar(-0.f);
        integer(1, 4); integer(8, 4); integer(1, 1); integer(8, 4); integer(0, 1); integer(0xd, 2);
        for (float value : {1.f, -0.f, 0.f, 4.f, 10.f, 11.f, 1.f, 1.f, 2.f, 3.f, -0.f, .016f}) scalar(value);
        return bytes;
    }
}

TEST(ESM4RuntimeState, NativeControllersVersionThirtyFiveWireAndLegacyAbsence)
{
    auto state = controllerRagdollState(); const auto actor = state.mReferences.front().mKey;
    auto old = state; old.mVersion = 34; old.mNativeActorRagdolls.at(actor).mNativeControllers.reset();
    const auto legacy = old.serializeBinary(); auto prefix = legacy; prefix[std::string_view("OMW4STATE").size()] = 35;
    auto expected = prefix; const auto suffix = controllerRagdollGoldenSuffix(); ASSERT_EQ(suffix.size(), 140u);
    expected.insert(expected.end(), suffix.begin(), suffix.end()); EXPECT_EQ(state.serializeBinary(), expected);
    const auto restored = ESM4::RuntimeState::deserializeBinary(expected);
    EXPECT_EQ(restored.mNativeActorRagdolls, state.mNativeActorRagdolls); EXPECT_EQ(restored.serializeBinary(), expected);
    const auto& controls = *restored.mNativeActorRagdolls.at(actor).mNativeControllers;
    EXPECT_TRUE(std::signbit(controls.mBlends[0].mState.mCachedGains.mHierarchy));
    EXPECT_TRUE(std::signbit(controls.mVelocities[0].mState.mForceVector[3]));
    EXPECT_NE(restored.canonicalJson().find("\"native_controllers\":"), std::string::npos);
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(legacy).mNativeActorRagdolls.at(actor).mNativeControllers);
    state.mNativeActorRagdolls.at(actor).mNativeControllers.reset(); expected = prefix; expected.push_back(0);
    EXPECT_EQ(state.serializeBinary(), expected);
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(expected).mNativeActorRagdolls.at(actor).mNativeControllers);
    state.mNativeActorRagdolls.at(actor).mNativeControllers.emplace(); expected = prefix;
    expected.insert(expected.end(), {1, 0,0,0,0, 0,0,0,0}); EXPECT_EQ(state.serializeBinary(), expected);
    EXPECT_TRUE(ESM4::RuntimeState::deserializeBinary(expected).mNativeActorRagdolls.at(actor).mNativeControllers->mBlends.empty());
}

TEST(ESM4RuntimeState, NativeControllersRejectMalformedVersionIdentityTimingAndWire)
{
    auto state = controllerRagdollState(); const auto actor = state.mReferences.front().mKey;
    auto invalid = state; invalid.mVersion = 34; EXPECT_THROW(invalid.serializeBinary(), std::runtime_error);
    for (unsigned field = 0; field < 14; ++field)
    {
        invalid = state; auto& pose = invalid.mNativeActorRagdolls.at(actor); auto& controls = *pose.mNativeControllers;
        auto& blend = controls.mBlends[0]; auto& velocity = controls.mVelocities[0];
        switch (field)
        {
            case 0: pose.mNativeBlends.reset(); break;
            case 1: pose.mBodies[0].mNativeMotion.reset(); break;
            case 2: blend.mAttachedNode = 99; break;
            case 3: blend.mTargetNode = 99; break;
            case 4: controls.mBlends.push_back(blend); break;
            case 5: blend.mState.mTiming.mPhase = std::numeric_limits<float>::infinity(); break;
            case 6: blend.mState.mClock.mElapsed = std::numeric_limits<float>::quiet_NaN(); break;
            case 7: blend.mState.mCachedGains.mHierarchy = std::numeric_limits<float>::infinity(); break;
            case 8: blend.mState.mKeys = {{2, {0, 0}}, {1, {0, 0}}}; blend.mState.mCursor = 0; break;
            case 9: blend.mState.mKeys = {{0, {0, 0}}, {1, {0, 0}}}; blend.mState.mCursor = 1; break;
            case 10: velocity.mTargetNode = 99; break;
            case 11: velocity.mState.mTiming.mStartKey = 5; break;
            case 12: velocity.mState.mFrameDelta = -1; break;
            case 13: velocity.mState.mForceVector[3] = std::numeric_limits<float>::quiet_NaN(); break;
        }
        SCOPED_TRACE(field); EXPECT_THROW(invalid.validate(), std::runtime_error);
    }
    const auto payload = state.serializeBinary(); const auto suffix = controllerRagdollGoldenSuffix(); const auto offset = payload.size() - suffix.size();
    for (std::size_t cut = offset; cut < payload.size(); ++cut)
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary({payload.begin(), payload.begin() + cut}), std::runtime_error);
    for (std::size_t marker : {offset, offset + 13, offset + 84, offset + 89})
    {
        auto bad = payload; bad[marker] = 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad), std::runtime_error);
    }
    auto bad = payload; bad[offset + 1] = 2;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad), std::runtime_error);
}

TEST(ESM4RuntimeState, NativePhysicalCacheVersionThirtySixGoldenAndLegacyAbsence)
{
    auto old = makeState(); old.mVersion = 35;
    const auto legacy = old.serializeBinary(); auto prefix = legacy;
    prefix[std::string_view("OMW4STATE").size()] = 36;
    auto state = old; state.mVersion = 36;
    state.mNativePhysicalBlendTimeCache = ESM4::PhysicalBlendTimeCache{0xffffffffu, 1, -1, -0.f, -.25f};
    const std::vector<std::uint8_t> suffix{1, 255,255,255,255, 0,0,128,63, 0,0,128,191, 0,0,0,128, 0,0,128,190};
    auto expected = prefix; expected.insert(expected.end(), suffix.begin(), suffix.end());
    EXPECT_EQ(state.serializeBinary(), expected);
    const auto loaded = ESM4::RuntimeState::deserializeBinary(expected);
    ASSERT_TRUE(loaded.mNativePhysicalBlendTimeCache);
    EXPECT_EQ(*loaded.mNativePhysicalBlendTimeCache, *state.mNativePhysicalBlendTimeCache);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(loaded.mNativePhysicalBlendTimeCache->mKeyTime), 0x80000000u);
    EXPECT_EQ(loaded.serializeBinary(), expected);
    EXPECT_NE(loaded.canonicalJson().find("\"native_physical_blend_time_cache\":{\"cycle\":4294967295,\"stop_key\":1,\"start_key\":-1,\"key_time\":-0.0,\"result\":-0.25}"), std::string::npos);
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(legacy).mNativePhysicalBlendTimeCache);
    state.mNativePhysicalBlendTimeCache.reset(); expected = prefix; expected.push_back(0);
    EXPECT_EQ(state.serializeBinary(), expected);
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(expected).mNativePhysicalBlendTimeCache);
    for (const auto cycle : {0u, 1u, 2u, 3u, 0x80000000u, 0xffffffffu})
    {
        state.mNativePhysicalBlendTimeCache = ESM4::PhysicalBlendTimeCache{cycle, -0.f, 0, 1, -2};
        EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()).mNativePhysicalBlendTimeCache, state.mNativePhysicalBlendTimeCache);
    }
}

TEST(ESM4RuntimeState, NativePhysicalCacheRejectsDowngradeNonfiniteAndMalformedWire)
{
    auto state = makeState(); state.mVersion = 36; state.mNativePhysicalBlendTimeCache.emplace();
    const auto payload = state.serializeBinary(); const auto offset = payload.size() - 21;
    for (std::size_t cut = offset; cut < payload.size(); ++cut)
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(std::vector(payload.begin(), payload.begin() + cut)), std::runtime_error);
    auto badWire = payload; badWire[offset] = 2;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(badWire), std::runtime_error);
    for (unsigned field = 0; field < 4; ++field)
        for (const auto value : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
        {
            auto bad = state; auto& cache = *bad.mNativePhysicalBlendTimeCache;
            switch (field)
            {
                case 0: cache.mStopKey = value; break;
                case 1: cache.mStartKey = value; break;
                case 2: cache.mKeyTime = value; break;
                case 3: cache.mResult = value; break;
            }
            EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
            auto wire = payload; const auto bits = std::bit_cast<std::uint32_t>(value);
            for (unsigned lane = 0; lane < 4; ++lane) wire[offset + 5 + field * 4 + lane] = bits >> (lane * 8);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(wire), std::runtime_error);
        }
    state.mVersion = 35; EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    state.mNativePhysicalBlendTimeCache.reset(); EXPECT_NO_THROW(state.serializeBinary());
}

TEST(ESM4RuntimeState, PlayerBowTimerVersionThirtySevenGoldenAndLegacyAbsence)
{
    auto old = makeState(); old.mVersion = 36;
    const auto legacy = old.serializeBinary();
    auto prefix = legacy; prefix[std::string_view("OMW4STATE").size()] = 37;
    auto state = old; state.mVersion = 37; state.mNativePlayerBowTimer = -0.f;
    auto expected = prefix;
    expected.insert(expected.end(), {1, 0, 0, 0, 128});
    EXPECT_EQ(state.serializeBinary(), expected);
    const auto loaded = ESM4::RuntimeState::deserializeBinary(expected);
    ASSERT_TRUE(loaded.mNativePlayerBowTimer);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(*loaded.mNativePlayerBowTimer), 0x80000000u);
    EXPECT_EQ(loaded.serializeBinary(), expected);
    EXPECT_NE(loaded.canonicalJson().find("\"native_player_bow_timer\":-0.0"), std::string::npos);
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(legacy).mNativePlayerBowTimer);
    state.mNativePlayerBowTimer.reset(); expected = prefix; expected.push_back(0);
    EXPECT_EQ(state.serializeBinary(), expected);
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(expected).mNativePlayerBowTimer);
    for (float timer : {-.25f, .625f, std::numeric_limits<float>::max()})
    {
        state.mNativePlayerBowTimer = timer;
        EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()).mNativePlayerBowTimer, timer);
    }
}

TEST(ESM4RuntimeState, PlayerBowTimerRejectsNonfiniteBadWireAndLossyDowngrade)
{
    auto state = makeState(); state.mVersion = 37; state.mNativePlayerBowTimer = .625f;
    const auto bytes = state.serializeBinary();
    for (float bad : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        auto invalid = state; invalid.mNativePlayerBowTimer = bad;
        EXPECT_THROW(invalid.serializeBinary(), std::runtime_error);
        auto corrupt = bytes; const auto bits = std::bit_cast<std::uint32_t>(bad);
        for (unsigned i = 0; i < 4; ++i) corrupt[corrupt.size() - 4 + i] = (bits >> (8*i)) & 255;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
    }
    auto corrupt = bytes; corrupt[corrupt.size() - 5] = 2;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
    corrupt = bytes; corrupt.pop_back();
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
    corrupt = bytes; corrupt.push_back(0);
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
    state.mVersion = 36;
    EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    state.mNativePlayerBowTimer.reset();
    EXPECT_NO_THROW(state.serializeBinary());
}

namespace
{
    ESM4::RuntimeState bowState()
    {
        auto state = makeState();
        ESM4::RuntimeActorValues values;
        values.mActor = state.mPlayer.mReference;
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        state.mNativeActorValues = {values};
        state.mNativeActorLife = {{values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}}};
        state.mNativeAnimationClocks.emplace(values.mActor, .5f);
        state.mPhysicalActions = {2, {1}};
        state.mPhysicalActionOwners.emplace(1, values.mActor);
        ESM4::RuntimeBowState bow;
        bow.mActionId = 1;
        bow.mBowBase = ESM::FormKey::content("oblivion.esm", 0x25231);
        bow.mAmmoBase = ESM::FormKey::content("oblivion.esm", 0x17829);
        bow.mAnimationGroup = "bowattack";
        bow.mPlaybackRate = 1.5f;
        bow.mProgress = {ESM4::BowAnimationPhase::Attach, -0.f};
        bow.mKeyTimes = {0, .25f, 1, 1.5f, 2};
        state.mNativeBowStates.emplace(values.mActor, bow);
        return state;
    }
}

TEST(ESM4RuntimeState, BowThirtyEightIndependentGoldenWireAndLegacyAbsence)
{
    auto state = bowState(); state.mVersion = 38;
    auto old = state; old.mVersion = 37; old.mNativeBowStates.clear();
    const auto legacy = old.serializeBinary();
    auto expected = legacy;
    expected[9] = 38; // OMW4STATE magic is nine bytes; LE schema follows.
    const auto integer = [&](std::uint64_t value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i) expected.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    };
    const auto text = [&](std::string_view value) {
        integer(value.size(), 4); expected.insert(expected.end(), value.begin(), value.end());
    };
    integer(1, 4); text(state.mPlayer.mReference.serialize()); integer(1, 8);
    text("content:oblivion.esm:025231"); text("content:oblivion.esm:017829"); text("bowattack");
    integer(0x3fc00000, 4); integer(1, 1); integer(0x80000000, 4);
    for (const auto bits : {0u, 0x3e800000u, 0x3f800000u, 0x3fc00000u, 0x40000000u}) integer(bits, 4);
    integer(4, 2); integer(0, 1);
    const auto bytes = state.serializeBinary();
    EXPECT_EQ(bytes, expected);
    const auto loaded = ESM4::RuntimeState::deserializeBinary(bytes);
    EXPECT_EQ(loaded.mNativeBowStates, state.mNativeBowStates);
    EXPECT_TRUE(std::signbit(loaded.mNativeBowStates.begin()->second.mProgress.mSequenceOffset));
    EXPECT_NE(loaded.canonicalJson().find("\"sequence_offset\":-0.0"), std::string::npos);
    EXPECT_TRUE(ESM4::RuntimeState::deserializeBinary(legacy).mNativeBowStates.empty());
    auto empty = old; empty.mVersion = 38; auto emptyExpected = legacy; emptyExpected[9] = 38;
    emptyExpected.insert(emptyExpected.end(), 4, 0);
    EXPECT_EQ(empty.serializeBinary(), emptyExpected);
}

TEST(ESM4RuntimeState, BowRejectsMalformedOwnershipReplayAndTruncatedWire)
{
    const auto state = bowState();
    const auto rejects = [&](auto mutate) {
        auto bad = state; mutate(bad); EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
    };
    rejects([](auto& s) { s.mVersion = 37; });
    rejects([](auto& s) { s.mNativeActorLife.clear(); });
    rejects([](auto& s) { s.mNativeAnimationClocks.clear(); });
    rejects([](auto& s) { s.mPhysicalActionOwners.clear(); });
    rejects([](auto& s) { s.mNativeActorLife[0].mPhase = ESM4::ActorLifePhase::Dead; });
    rejects([](auto& s) { s.mNativeBowStates.begin()->second.mActionId = 2; });
    rejects([](auto& s) { s.mNativeBowStates.begin()->second.mBowBase = {}; });
    rejects([](auto& s) { s.mNativeBowStates.begin()->second.mAnimationGroup = ""; });
    rejects([](auto& s) { s.mNativeBowStates.begin()->second.mProgress.mPhase = ESM4::BowAnimationPhase::Hold; });
    rejects([](auto& s) { s.mNativeBowStates.begin()->second.mPlaybackRate = std::numeric_limits<float>::infinity(); });
    rejects([](auto& s) { s.mNativeBowStates.begin()->second.mKeyTimes[2] = .125f; });
    rejects([](auto& s) { s.mNativeBowStates.begin()->second.mReleaseCommitted = true; });
    const auto bytes = state.serializeBinary();
    for (std::size_t cut = bytes.size() - 40; cut < bytes.size(); ++cut)
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary({bytes.begin(), bytes.begin() + cut}), std::runtime_error);
    auto bad = bytes; bad.back() = 2;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad), std::runtime_error);
    auto committed = state;
    auto& bow = committed.mNativeBowStates.begin()->second;
    bow.mAction = 3; bow.mProgress.mPhase = ESM4::BowAnimationPhase::Release; bow.mReleaseCommitted = true;
    committed.mPhysicalActions.mPending.clear(); committed.mPhysicalActionOwners.clear();
    EXPECT_NO_THROW(committed.serializeBinary());
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(committed.serializeBinary()).mNativeBowStates, committed.mNativeBowStates);
}

TEST(ESM4RuntimeState, PlayerBowHoldLatchThirtyNineIndependentWireAndLegacyDefault)
{
    auto legacy = bowState(); legacy.mVersion = 38;
    const auto oldBytes = legacy.serializeBinary();
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(oldBytes).mNativeBowStates.begin()->second.mPlayerHoldLatched);
    for (bool latched : {false, true})
    {
        auto state = legacy; state.mVersion = 39;
        state.mNativeBowStates.begin()->second.mPlayerHoldLatched = latched;
        auto expected = oldBytes; expected[9] = 39; expected.push_back(latched);
        EXPECT_EQ(state.serializeBinary(), expected);
        EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(expected).mNativeBowStates, state.mNativeBowStates);
        expected.back() = 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(expected), std::runtime_error);
        expected.pop_back();
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(expected), std::runtime_error);
    }
    legacy.mNativeBowStates.begin()->second.mPlayerHoldLatched = true;
    EXPECT_THROW(legacy.serializeBinary(), std::runtime_error);
}

TEST(ESM4RuntimeState, LooseItemExtras40HaveExactNullableWireAndRejectCorruption)
{
    ESM4::RuntimeState state;
    state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
    state.mPlayer.mCell = ESM::FormKey::content("items.esm", 1);
    state.mPlayer.mRace = ESM::FormKey::content("items.esm", 2);
    state.mPlayer.mClass = ESM::FormKey::content("items.esm", 3);
    ESM4::RuntimeReferenceState reference;
    reference.mKey = ESM::FormKey::dynamic("dropped-item", 1);
    reference.mBase = ESM::FormKey::content("items.esm", 4);
    reference.mCell = state.mPlayer.mCell;
    constexpr std::string_view marker = "loose-item-extras-wire-boundary";
    reference.mCustomState.emplace("boundary", std::string(marker));
    state.mReferences.push_back(reference);
    state.mVersion = 39;
    const auto legacy = state.serializeBinary();
    const auto found = std::search(legacy.begin(), legacy.end(), marker.begin(), marker.end());
    ASSERT_NE(found, legacy.end());
    const auto boundary = std::distance(legacy.begin(), found) + marker.size() + 1; // absent draw flag
    state.mVersion = 40;
    const std::array<std::optional<float>, 9> values{std::nullopt, 0.f, -0.f, .1f, 49.8f,
        std::numeric_limits<float>::denorm_min(), std::numeric_limits<float>::min(), 7.25f,
        std::numeric_limits<float>::max()};
    for (const auto condition : values)
    for (const auto charge : values)
    {
        state.mReferences.front().mItemCondition = condition;
        state.mReferences.front().mItemCharge = charge;
        auto expected = legacy;
        expected[std::string_view("OMW4STATE").size()] = 40;
        std::vector<std::uint8_t> extraBytes;
        for (const auto value : {condition, charge})
        {
            extraBytes.push_back(value.has_value() ? 1 : 0);
            if (value)
            {
                const auto bits = std::bit_cast<std::uint32_t>(*value);
                for (unsigned shift = 0; shift < 32; shift += 8)
                    extraBytes.push_back(static_cast<std::uint8_t>(bits >> shift));
            }
        }
        expected.insert(expected.begin() + boundary, extraBytes.begin(), extraBytes.end());
        const auto binary = state.serializeBinary();
        EXPECT_EQ(binary, expected);
        const auto restored = ESM4::RuntimeState::deserializeBinary(binary);
        EXPECT_EQ(restored, state);
        for (const auto offset : {boundary, boundary + (condition ? 5 : 1)})
        {
            auto invalid = binary;
            invalid[offset] = 2;
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
        }
        for (const auto end : {boundary, boundary + static_cast<std::ptrdiff_t>(extraBytes.size()) - 1})
        {
            std::vector<std::uint8_t> truncated(binary.begin(), binary.begin() + end);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
        }
        if (condition)
        {
            EXPECT_EQ(std::bit_cast<std::uint32_t>(*restored.mReferences.front().mItemCondition),
                std::bit_cast<std::uint32_t>(*condition));
        }
        if (charge)
        {
            EXPECT_EQ(std::bit_cast<std::uint32_t>(*restored.mReferences.front().mItemCharge),
                std::bit_cast<std::uint32_t>(*charge));
        }
        EXPECT_NE(state.canonicalJson().find("\"item_condition\":"), std::string::npos);
        EXPECT_NE(state.canonicalJson().find("\"item_charge\":"), std::string::npos);
    }
    for (const float invalid : {-1.f, std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()})
    for (const bool charge : {false, true})
    {
        state.mReferences.front().mItemCondition = 1.f;
        state.mReferences.front().mItemCharge = 2.f;
        auto corruptWire = state.serializeBinary();
        const auto offset = boundary + (charge ? 6 : 1);
        const auto bits = std::bit_cast<std::uint32_t>(invalid);
        for (unsigned shift = 0; shift < 32; shift += 8)
            corruptWire[offset + shift / 8] = static_cast<std::uint8_t>(bits >> shift);
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corruptWire), std::runtime_error);
        (charge ? state.mReferences.front().mItemCharge : state.mReferences.front().mItemCondition) = invalid;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        EXPECT_THROW(state.canonicalJson(), std::runtime_error);
    }
}

TEST(ESM4RuntimeState, LooseItemExtras40DoNotMigrateBrokenDefaultsOrAttachToActors)
{
    for (unsigned version = 1; version < 40; ++version)
    {
        SCOPED_TRACE(version);
        ESM4::RuntimeState state;
        state.mVersion = version;
        state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        state.mPlayer.mCell = ESM::FormKey::content("items.esm", 1);
        if (version >= 3)
        {
            state.mPlayer.mRace = ESM::FormKey::content("items.esm", 2);
            state.mPlayer.mClass = ESM::FormKey::content("items.esm", 3);
        }
        ESM4::RuntimeReferenceState reference;
        reference.mKey = ESM::FormKey::dynamic("dropped-item", 1);
        reference.mBase = ESM::FormKey::content("items.esm", 4);
        reference.mCell = state.mPlayer.mCell;
        state.mReferences.push_back(reference);
        const auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        EXPECT_FALSE(restored.mReferences.front().mItemCondition);
        EXPECT_FALSE(restored.mReferences.front().mItemCharge);
        EXPECT_EQ(restored.canonicalJson().find("item_condition"), std::string::npos);
        for (const bool charge : {false, true})
        {
            auto invalid = state;
            (charge ? invalid.mReferences.front().mItemCharge : invalid.mReferences.front().mItemCondition) = 0.f;
            EXPECT_THROW(invalid.serializeBinary(), std::runtime_error);
        }
    }
    auto actor = ownedActionState();
    actor.mVersion = 40;
    for (const bool charge : {false, true})
    {
        auto invalid = actor;
        (charge ? invalid.mReferences.front().mItemCharge : invalid.mReferences.front().mItemCondition) = 0.f;
        EXPECT_THROW(invalid.serializeBinary(), std::runtime_error);
        EXPECT_THROW(invalid.canonicalJson(), std::runtime_error);
    }
}

TEST(ESM4RuntimeState, OwnershipExtras41HaveIndependentWireAndNullableSignedRank)
{
    ESM4::RuntimeState state;
    state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
    state.mPlayer.mCell = ESM::FormKey::content("items.esm", 1);
    state.mPlayer.mRace = ESM::FormKey::content("items.esm", 2);
    state.mPlayer.mClass = ESM::FormKey::content("items.esm", 3);
    ESM4::RuntimeReferenceState reference;
    reference.mKey = ESM::FormKey::dynamic("dropped-item", 1);
    reference.mBase = ESM::FormKey::content("items.esm", 4);
    reference.mCell = state.mPlayer.mCell;
    constexpr std::string_view marker = "native-ownership-wire-boundary";
    reference.mCustomState.emplace("boundary", std::string(marker));
    state.mReferences.push_back(reference);
    const std::array<std::optional<std::int32_t>, 8> ranks{std::nullopt,
        std::numeric_limits<std::int32_t>::min(), -2, -1, 0, 1, 9,
        std::numeric_limits<std::int32_t>::max()};
    for (const auto rank : ranks)
    for (const auto& global : {ESM::FormKey{}, ESM::FormKey::content("items.esm", 9)})
    {
        state.mReferences.front().mOwnershipRank = rank;
        state.mReferences.front().mOwnershipGlobal = global;
        const auto bytes = state.serializeBinary();
        const auto found = std::search(bytes.begin(), bytes.end(), marker.begin(), marker.end());
        ASSERT_NE(found, bytes.end());
        // Public wire: the custom string is followed by absent draw, condition
        // and charge flags, then v41 rank flag/int32 and length-prefixed key.
        const auto boundary = std::distance(bytes.begin(), found) + marker.size() + 3;
        std::vector<std::uint8_t> expected{static_cast<std::uint8_t>(rank.has_value())};
        const auto put32 = [&](std::uint32_t value) {
            for (unsigned i = 0; i < 4; ++i)
                expected.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
        };
        if (rank) put32(static_cast<std::uint32_t>(*rank));
        const auto key = global.serialize();
        put32(static_cast<std::uint32_t>(key.size()));
        expected.insert(expected.end(), key.begin(), key.end());
        ASSERT_LE(boundary + expected.size(), bytes.size());
        EXPECT_TRUE(std::equal(expected.begin(), expected.end(), bytes.begin() + boundary));
        const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(restored.mReferences.front().mOwnershipRank, rank);
        EXPECT_EQ(restored.mReferences.front().mOwnershipGlobal, global);
        auto corrupt = bytes;
        corrupt[boundary] = 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
        const auto json = restored.canonicalJson();
        EXPECT_NE(json.find("\"ownership_global\":\"" + key + "\""), std::string::npos);
    }
}

TEST(ESM4RuntimeState, OwnershipExtras41PreserveInventoryAndRejectOlderEnvelopeLoss)
{
    auto state = makeState();
    auto& item = state.mPlayer.mInventory.front();
    item.mOwnershipRank = -1;
    item.mOwnershipGlobal = ESM::FormKey::content("oblivion.esm", 0x34);
    state.mReferences.front().mOwnershipRank = -2;
    state.mReferences.front().mOwnershipGlobal = ESM::FormKey::content("oblivion.esm", 0x33);
    state.mReferences.front().mInventory.front().mOwnershipRank = std::numeric_limits<std::int32_t>::min();
    state.mReferences.front().mInventory.front().mOwnershipGlobal = item.mOwnershipGlobal;
    const auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
    EXPECT_EQ(restored.mPlayer.mInventory, state.mPlayer.mInventory);
    EXPECT_EQ(restored.mReferences, state.mReferences);
    state.mVersion = 40;
    EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    state.mVersion = 41;
    item.mOwnershipGlobal = ESM::FormKey::dynamic("not-a-content-global", 1);
    EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    item.mOwnershipGlobal = {};
    state.mReferences.front().mOwnershipGlobal = ESM::FormKey::dynamic("not-a-content-global", 1);
    EXPECT_THROW(state.serializeBinary(), std::runtime_error);
}

TEST(ESM4RuntimeState, OlderOwnershipEnvelopesDefaultToAbsentRatherThanInventedRank)
{
    for (std::uint32_t version = 1; version <= 40; ++version)
    {
        SCOPED_TRACE(version);
        ESM4::RuntimeState state;
        state.mVersion = version;
        state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        state.mPlayer.mCell = ESM::FormKey::content("items.esm", 1);
        if (version >= 3)
        {
            state.mPlayer.mRace = ESM::FormKey::content("items.esm", 2);
            state.mPlayer.mClass = ESM::FormKey::content("items.esm", 3);
        }
        ESM4::RuntimeInventoryItem item;
        item.mBase = ESM::FormKey::content("items.esm", 4);
        item.mCount = 1;
        state.mPlayer.mInventory.push_back(item);
        ESM4::RuntimeReferenceState reference;
        reference.mKey = ESM::FormKey::content("items.esm", 5);
        reference.mBase = ESM::FormKey::content("items.esm", 4);
        reference.mCell = state.mPlayer.mCell;
        state.mReferences.push_back(reference);
        const auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        EXPECT_FALSE(restored.mPlayer.mInventory.front().mOwnershipRank);
        EXPECT_TRUE(restored.mPlayer.mInventory.front().mOwnershipGlobal.isNull());
        EXPECT_FALSE(restored.mReferences.front().mOwnershipRank);
        EXPECT_TRUE(restored.mReferences.front().mOwnershipGlobal.isNull());
    }
}

TEST(ESM4RuntimeState, OwnershipExtras41ReadIndependentPythonPayloadAndReproduceExactBytes)
{
    // SHA-256: 6ddc656a3139a7d7cf2eb5b06f73ad0cd908d3f98c9c66e7065ece0a7a302740.
    // Input declared independently in the Python fixture, including signed rank
    // presence, different GLOB identities and exhausted item condition.
    const std::vector<std::uint8_t> payload{
#include "native_ownership_schema41_expected.inc"
    };
    const auto restored = ESM4::RuntimeState::deserializeBinary(payload);
    ASSERT_EQ(restored.mVersion, 41u);
    ASSERT_FALSE(restored.mPlayer.mInventory.empty());
    EXPECT_EQ(restored.mPlayer.mInventory.front().mOwnershipRank, -1);
    EXPECT_EQ(restored.mPlayer.mInventory.front().mOwnershipGlobal,
        ESM::FormKey::content("oblivion.esm", 0x34));
    ASSERT_EQ(restored.mReferences.size(), 1u);
    const auto& reference = restored.mReferences.front();
    EXPECT_EQ(reference.mKey, ESM::FormKey::content("oblivion.esm", 0x100));
    EXPECT_EQ(reference.mBase, ESM::FormKey::content("oblivion.esm", 0x200));
    EXPECT_EQ(reference.mOwner, ESM::FormKey::content("oblivion.esm", 0x300));
    EXPECT_EQ(reference.mOwnershipRank, -2);
    EXPECT_EQ(reference.mOwnershipGlobal, ESM::FormKey::content("oblivion.esm", 0x33));
    EXPECT_EQ(reference.mItemCondition, 0.f);
    EXPECT_EQ(reference.mItemCharge, 7.25f);
    ASSERT_EQ(reference.mInventory.size(), 1u);
    EXPECT_EQ(reference.mInventory.front().mCount, 3);
    EXPECT_EQ(reference.mInventory.front().mOwnershipRank, std::numeric_limits<std::int32_t>::min());
    EXPECT_EQ(reference.mInventory.front().mOwnershipGlobal, ESM::FormKey::content("oblivion.esm", 0x34));
    EXPECT_EQ(restored.serializeBinary(), payload);
}

TEST(ESM4RuntimeState, OwnershipExtras41RejectLossyGlobalKeysBeforeEncodingOrAfterReading)
{
    // Public FormKey fields can bypass its factories; reject lossy encodings
    // and invalid bindings before either binary or JSON output.
    const std::array<ESM::FormKey, 7> globals{{
        {ESM::FormKeyKind::Content, "oblivion.esm", 0},
        {ESM::FormKeyKind::Content, "oblivion.esm", 0x1000000},
        {ESM::FormKeyKind::Content, "Oblivion.esm", 0x33},
        {ESM::FormKeyKind::Content, "bad:plugin.esm", 0x33},
        {ESM::FormKeyKind::Content, "", 0x33},
        {ESM::FormKeyKind::Null, "ignored", 0},
        {ESM::FormKeyKind::Null, "", 1},
    }};
    for (const auto& global : globals)
    {
        SCOPED_TRACE(global.serialize());
        for (bool inventory : {false, true})
        {
            SCOPED_TRACE(inventory);
            auto state = makeState();
            if (inventory) state.mPlayer.mInventory.front().mOwnershipGlobal = global;
            else state.mReferences.front().mOwnershipGlobal = global;
            EXPECT_THROW(state.serializeBinary(), std::exception);
            EXPECT_THROW(state.canonicalJson(), std::exception);
        }
    }
}

TEST(ESM4RuntimeState, OwnershipExtras41RejectExplicitZeroGlobalIdentityInBinaryPayload)
{
    // Preserve length and all unrelated fields while replacing the declared
    // non-null content identity with zero. Both inventory and REFR readers
    // must reject it rather than silently treating it as the null sentinel.
    const auto global = ESM::FormKey::content("ownership-wire.esm", 9);
    const auto encoded = global.serialize();
    for (bool inventory : {false, true})
    {
        SCOPED_TRACE(inventory);
        auto state = makeState();
        if (inventory) state.mPlayer.mInventory.front().mOwnershipGlobal = global;
        else state.mReferences.front().mOwnershipGlobal = global;
        auto payload = state.serializeBinary();
        const auto found = std::search(payload.begin(), payload.end(), encoded.begin(), encoded.end());
        ASSERT_NE(found, payload.end());
        std::fill(found + static_cast<std::ptrdiff_t>(encoded.size()) - 6,
            found + static_cast<std::ptrdiff_t>(encoded.size()), '0');
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(payload), std::runtime_error);
    }
}


TEST(ESM4RuntimeState, Crime42PopulatedRoundTripAndOriginalLayoutsRemainDeliberate)
{
    auto state = makeState();
    state.mVersion = 42;
    state.mPhysicalActions.mNext = 10;
    state.mNativeCrime = Testing::crimeFixture();
    const auto bytes = state.serializeBinary();
    ASSERT_GE(bytes.size(), Testing::CrimeWireFixture.size());
    EXPECT_TRUE(std::equal(Testing::CrimeWireFixture.rbegin(), Testing::CrimeWireFixture.rend(), bytes.rbegin()));
    const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
    EXPECT_EQ(restored, state);
    EXPECT_NE(restored.canonicalJson().find("\"native_crime\":{\"next_incident\":3"), std::string::npos);
    for (std::uint32_t version = 1; version < 42; ++version)
    {
        SCOPED_TRACE(version);
        ESM4::RuntimeState old;
        old.mVersion = version;
        old.mPlayer.mReference = state.mPlayer.mReference;
        old.mPlayer.mCell = state.mPlayer.mCell;
        if (version >= 3) { old.mPlayer.mRace = state.mPlayer.mRace; old.mPlayer.mClass = state.mPlayer.mClass; }
        const auto migrated = ESM4::RuntimeState::deserializeBinary(old.serializeBinary());
        EXPECT_EQ(migrated.mNativeCrime, ESM4::CrimeStateContracts{});
        EXPECT_EQ(migrated.serializeBinary(), old.serializeBinary());
        old.mNativeCrime = state.mNativeCrime;
        EXPECT_THROW(old.serializeBinary(), std::runtime_error);
    }
}

TEST(ESM4RuntimeState, Crime42RejectsUnissuedOrPendingCommittedCausesAndMalformedSuffix)
{
    auto state = makeState();
    state.mPhysicalActions.mNext = 10;
    state.mNativeCrime = Testing::crimeFixture();
    auto bad = state;
    bad.mPhysicalActions.mNext = 9;
    EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
    bad = state; bad.mPhysicalActions.mPending = {9};
    EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
    const auto bytes = state.serializeBinary();
    for (std::size_t remove = 1; remove <= 36; ++remove)
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary({bytes.begin(), bytes.end() - remove}), std::runtime_error);
    // Each trailing quest/property/penalty/release flag must be exactly 0 or 1.
    for (std::size_t offset = 1; offset <= 5; ++offset)
    {
        auto corruptFlag = bytes; corruptFlag[corruptFlag.size() - offset] = 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corruptFlag), std::runtime_error);
    }
    auto extra = bytes; extra.push_back(0);
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(extra), std::runtime_error);
    auto empty = makeState();
    auto corrupt = empty.serializeBinary();
    // The empty suffix is three u64 counters followed by three u32 counts.
    corrupt[corrupt.size() - 12] = 0x41;
    corrupt[corrupt.size() - 11] = 0x42;
    corrupt[corrupt.size() - 10] = 0x0f; // 1,000,001 incidents
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
}

TEST(ESM4RuntimeState, FractionalBounty43PreservesNativeAmountsAndEveryLegacyInt32Exactly)
{
    auto state = makeState(); state.mVersion = 43; state.mPhysicalActions.mNext = 10;
    state.mNativeCrime = Testing::crimeFixture();
    for (double amount : {0.5, 0.25, -0.25, -0.0, 16777217.0, 2147483647.0, -2147483648.0})
    {
        SCOPED_TRACE(amount);
        state.mNativeCrime.mIncidents.front().mOutcome.mBountyDelta = amount;
        const auto bytes = state.serializeBinary();
        if (amount == .5)
            EXPECT_TRUE(std::equal(Testing::FractionalCrimeWireFixture.rbegin(),
                Testing::FractionalCrimeWireFixture.rend(), bytes.rbegin()));
        const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(restored, state);
        EXPECT_EQ(std::bit_cast<std::uint64_t>(restored.mNativeCrime.mIncidents.front().mOutcome.mBountyDelta),
            std::bit_cast<std::uint64_t>(amount));
        auto legacy = state; legacy.mVersion = 42;
        if (std::trunc(amount) != amount || (amount == 0 && std::signbit(amount)))
            EXPECT_THROW(legacy.serializeBinary(), std::runtime_error);
        else
        {
            const auto oldBytes = legacy.serializeBinary();
            const auto migrated = ESM4::RuntimeState::deserializeBinary(oldBytes);
            EXPECT_EQ(migrated.mNativeCrime.mIncidents.front().mOutcome.mBountyDelta, amount);
            EXPECT_EQ(migrated.serializeBinary(), oldBytes);
        }
    }
    for (double bad : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
             1e100, -1e100})
    {
        state.mNativeCrime.mIncidents.front().mOutcome.mBountyDelta = bad;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }
}

TEST(ESM4RuntimeState, OwnedBounty44PreservesRawBucketsAndMatchesIndependentWire)
{
    auto state = makeState();
    ESM4::RuntimeActorValues actor;
    actor.mActor = state.mPlayer.mReference;
    actor.mBase = ESM::FormKey::content("bounty-wire.esm", 9);
    actor.mOwner = ESM4::ActorValueOwner::Player;
    actor.mBounty = ESM4::CrimeBountyState{.25f, -3.5f};
    actor.mPlayerInShiveringIsles = true;
    state.mNativeActorValues = {actor};
    const auto bytes = state.serializeBinary();
    const auto base = actor.mBase.serialize();
    const auto found = std::search(bytes.begin(), bytes.end(), base.begin(), base.end());
    ASSERT_NE(found, bytes.end());
    // Owner/process, 72 zero bases with absent modifiers, then two absent
    // process-state flags precede the independently specified v44 field.
    const auto offset = std::distance(bytes.begin(), found) + base.size() + 2 + 72 * 5 + 2;
    const std::array<std::uint8_t, 10> expected{1, 0, 0, 128, 62, 0, 0, 96, 192, 1};
    ASSERT_GE(bytes.size(), offset + expected.size());
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), bytes.begin() + offset));
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(bytes), state);
    EXPECT_NE(state.canonicalJson().find(
        "\"bounty\":{\"normal\":0.25,\"shivering_isles\":-3.5,\"player_in_shivering_isles\":true}"),
        std::string::npos);
    for (const auto flag : {offset, offset + 9})
    {
        auto bad = bytes; bad[flag] = 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad), std::runtime_error);
    }
    for (float amount : {-0.f, std::numeric_limits<float>::denorm_min(), std::numeric_limits<float>::max()})
    {
        state.mNativeActorValues.front().mBounty = ESM4::CrimeBountyState{amount, -amount};
        const auto restored = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        const auto& bounty = *restored.mNativeActorValues.front().mBounty;
        EXPECT_EQ(std::bit_cast<std::uint32_t>(bounty.mNormal), std::bit_cast<std::uint32_t>(amount));
        EXPECT_EQ(std::bit_cast<std::uint32_t>(bounty.mShiveringIsles), std::bit_cast<std::uint32_t>(-amount));
    }
}

TEST(ESM4RuntimeState, OwnedBounty44RejectsDuplicateAuthorityAndForeignRealmStorage)
{
    auto state = makeState();
    ESM4::RuntimeActorValues actor;
    actor.mActor = state.mReferences.front().mKey;
    actor.mBase = state.mReferences.front().mBase;
    actor.mBounty = ESM4::CrimeBountyState{.25f, 0.f};
    state.mNativeActorValues = {actor};
    EXPECT_NO_THROW(state.validate());
    const auto reject = [&](const ESM4::RuntimeActorValues& bad) {
        state.mNativeActorValues = {bad};
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        EXPECT_THROW(state.canonicalJson(), std::runtime_error);
    };
    for (float value : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        auto bad = actor; bad.mBounty->mNormal = value; reject(bad);
    }
    auto bad = actor; bad.mBounty->mShiveringIsles = -1.f; reject(bad);
    bad = actor; bad.mPlayerInShiveringIsles = true; reject(bad);
    bad = actor; bad.mBounty.reset(); bad.mPlayerInShiveringIsles = true; reject(bad);
    bad = actor; bad.mValues[37].mBase = .25f; reject(bad);
    for (auto channel : {0, 1, 2})
    {
        bad = actor; bad.mValues[37].mModifiers[channel] = .25f;
        state.mNativeActorValues = {bad};
        EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()).mNativeActorValues.front(), bad);
    }
    bad = actor; bad.mValues[37].mModifiers[1] = 0.f;
    state.mNativeActorValues = {bad};
    EXPECT_NO_THROW(state.validate()); // Present zero is not a second amount.
    for (std::uint32_t version = 9; version < 44; ++version)
    {
        state.mVersion = version;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }
}

TEST(ESM4RuntimeState, OwnedBounty44PreservesEveryLegacyScalarWithoutGuessingRealm)
{
    auto state = makeState();
    ESM4::RuntimeActorValues actor;
    actor.mActor = state.mPlayer.mReference;
    actor.mBase = ESM::FormKey::content("oblivion.esm", 7);
    actor.mOwner = ESM4::ActorValueOwner::Player;
    actor.mValues[37] = {.25f, {1.f, {}, -0.f}};
    state.mNativeActorValues = {actor};
    for (std::uint32_t version = 9; version < 44; ++version)
    {
        SCOPED_TRACE(version);
        state.mVersion = version;
        const auto bytes = state.serializeBinary();
        auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(restored.serializeBinary(), bytes);
        EXPECT_FALSE(restored.mNativeActorValues.front().mBounty);
        EXPECT_FALSE(restored.mNativeActorValues.front().mPlayerInShiveringIsles);
        restored.mVersion = 44;
        const auto promoted = ESM4::RuntimeState::deserializeBinary(restored.serializeBinary());
        EXPECT_EQ(promoted.mNativeActorValues.front().mValues[37], actor.mValues[37]);
        EXPECT_FALSE(promoted.mNativeActorValues.front().mBounty);
        EXPECT_NE(promoted.canonicalJson().find("\"bounty\":null"), std::string::npos);
    }
}

TEST(ESM4RuntimeState, Bounty45Preserves44NativePayloadLayout)
{
    auto state = makeState();
    ESM4::RuntimeActorValues actor;
    actor.mActor = state.mPlayer.mReference;
    actor.mBase = ESM::FormKey::dynamic("player-base", 1);
    actor.mOwner = ESM4::ActorValueOwner::Player;
    actor.mBounty = ESM4::CrimeBountyState{.25f, -3.5f};
    actor.mPlayerInShiveringIsles = true;
    state.mNativeActorValues = {actor};
    state.mVersion = 44;
    const auto legacy = state.serializeBinary();
    state.mVersion = 45;
    auto expected = legacy;
    expected[9] = 45;
    EXPECT_EQ(state.serializeBinary(), expected);
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(legacy).mNativeActorValues,
        ESM4::RuntimeState::deserializeBinary(expected).mNativeActorValues);
}

TEST(ESM4RuntimeState, Reputation46OwnsExactSignedCountersAndIndependentAccumulatorPresence)
{
    auto state = makeState();
    ESM4::RuntimeActorValues actor;
    actor.mActor = state.mPlayer.mReference;
    actor.mBase = ESM::FormKey::content("reputation-wire.esm", 9);
    actor.mOwner = ESM4::ActorValueOwner::Player;
    actor.mReputation = ESM4::PlayerReputationState{16777217, -3, 777};
    actor.mValues[38].mModifiers = {.5f, .75f, -.25f};
    state.mNativeActorValues = {actor};
    const auto bytes = state.serializeBinary();
    const auto name = actor.mBase.serialize();
    const auto found = std::search(bytes.begin(), bytes.end(), name.begin(), name.end());
    ASSERT_NE(found, bytes.end());
    // Three present AV38 modifiers add twelve bytes to the zero/absent array.
    const auto offset = std::distance(bytes.begin(), found) + name.size() + 2 + 72 * 5 + 12 + 3;
    const std::vector<std::uint8_t> expected{1, 1, 0, 0, 1, 253, 255, 255, 255, 1, 9, 3, 0, 0};
    ASSERT_GE(bytes.size(), offset + expected.size());
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), bytes.begin() + offset));
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(bytes).mNativeActorValues, state.mNativeActorValues);
    for (const auto flag : {offset, offset + 9})
    {
        auto invalid = bytes; invalid[flag] = 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
    }
    for (const auto accumulated : {std::optional<std::int32_t>{}, std::optional<std::int32_t>{0},
             std::optional<std::int32_t>{std::numeric_limits<std::int32_t>::min()}})
    {
        state.mNativeActorValues.front().mReputation->mBountyAccumulator = accumulated;
        EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()).mNativeActorValues,
            state.mNativeActorValues);
    }
    state.mVersion = 45;
    EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    state.mVersion = 46;
    state.mPlayer.mActorValues["infamy"] = -3;
    EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    state.mPlayer.mActorValues.clear();
    state.mNativeActorValues.front().mValues[39].mBase = 1;
    EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    state.mNativeActorValues.front().mValues[39].mBase = 0;
    state.mNativeActorValues.front().mOwner = ESM4::ActorValueOwner::NonPlayer;
    EXPECT_THROW(state.serializeBinary(), std::runtime_error);
}

TEST(ESM4RuntimeState, DropRelationsRemovalMatchesOriginalFirstMatchAndExtraPresence)
{
    const auto target = ESM::FormKey::dynamic("native-reference", 1);
    const auto other = ESM::FormKey::dynamic("native-reference", 2);
    const auto third = ESM::FormKey::dynamic("native-reference", 3);
    const auto owner = ESM::FormKey::content("master.esm", 0x123);
    using Items = std::optional<std::vector<ESM::FormKey>>;
    struct Case { Items mBefore; ESM::FormKey mTarget; Items mAfter; };
    // Independently observed original424C00/65C620 and full4D7D80 wrapper.
    // Duplicate fixtures deliberately test the helper before wire validation.
    const std::array<Case, 9> cases{{
        {std::nullopt, target, std::nullopt},
        {std::vector<ESM::FormKey>{}, {}, std::vector<ESM::FormKey>{}},
        {std::vector<ESM::FormKey>{}, target, std::nullopt},
        {{ {target} }, target, std::nullopt},
        {{ {other} }, target, {{other}}},
        {{ {target, other, third} }, target, {{other, third}}},
        {{ {other, target, third} }, target, {{other, third}}},
        {{ {other, third, target} }, target, {{other, third}}},
        {{ {other, target, target} }, target, {{other, target}}},
    }};
    for (const auto& row : cases)
    {
        ESM4::DropRelations relations{owner, row.mBefore};
        relations.removeItem(row.mTarget);
        EXPECT_EQ(relations.mItems, row.mAfter);
        EXPECT_EQ(relations.mDropper, owner);
    }
}

TEST(ESM4RuntimeState, DropRelations47IndependentWirePresenceOrderAndLegacyBoundary)
{
    auto state = makeState();
    state.mVersion = 47;
    auto& reference = state.mReferences.front();
    reference.mOwnershipGlobal = ESM::FormKey::content("oblivion.esm", 0xdeadbe);
    auto legacy = state; legacy.mVersion = 46;
    const auto old = legacy.serializeBinary();
    const std::string marker = "content:oblivion.esm:deadbe";
    const auto found = std::search(old.begin(), old.end(), marker.begin(), marker.end());
    ASSERT_NE(found, old.end());
    const std::size_t boundary = std::distance(old.begin(), found) + marker.size();
    const auto actor = ESM::FormKey::content("oblivion.esm", 0x902);
    const auto first = ESM::FormKey::dynamic("native-reference", 3);
    const auto second = ESM::FormKey::content("oblivion.esm", 0x960);
    for (const bool dropper : {false, true})
    for (const int list : {0, 1, 2})
    {
        SCOPED_TRACE(dropper);
        SCOPED_TRACE(list);
        auto& relations = reference.mDropRelations;
        relations = {};
        if (dropper) relations.mDropper = actor;
        if (list) relations.mItems.emplace();
        if (list == 2) *relations.mItems = {first, second};
        std::vector<std::uint8_t> extra;
        const auto integer = [&](std::uint32_t value) {
            for (unsigned i = 0; i < 4; ++i) extra.push_back((value >> (i * 8)) & 0xff);
        };
        const auto key = [&](std::string_view text) {
            integer(text.size()); extra.insert(extra.end(), text.begin(), text.end());
        };
        extra.push_back(dropper);
        if (dropper) key("content:oblivion.esm:000902");
        const auto listFlag = extra.size();
        extra.push_back(list != 0);
        if (list)
        {
            integer(list == 2 ? 2 : 0);
            if (list == 2)
            {
                key("dynamic:native-reference:0000000000000003");
                key("content:oblivion.esm:000960");
            }
        }
        auto expected = old; expected[9] = 47;
        expected.insert(expected.begin() + boundary, extra.begin(), extra.end());
        EXPECT_EQ(state.serializeBinary(), expected);
        const auto decoded = ESM4::RuntimeState::deserializeBinary(expected);
        EXPECT_EQ(decoded.mReferences.front().mDropRelations, relations);
        EXPECT_EQ(decoded.serializeBinary(), expected);
        EXPECT_NE(decoded.canonicalJson().find("\"dropped_items\":"), std::string::npos);
        auto malformed = expected; malformed[boundary] = 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(malformed), std::exception);
        malformed = expected; malformed[boundary + listFlag] = 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(malformed), std::exception);
        if (!relations.empty())
        {
            auto rejected = state; rejected.mVersion = 46;
            EXPECT_THROW(rejected.serializeBinary(), std::runtime_error);
        }
    }
    const auto decodedOld = ESM4::RuntimeState::deserializeBinary(old);
    EXPECT_TRUE(decodedOld.mReferences.front().mDropRelations.empty());
    EXPECT_EQ(decodedOld.serializeBinary(), old);
}

TEST(ESM4RuntimeState, DropRelations47RejectNullDuplicateAndLossyIdentities)
{
    auto state = makeState();
    const auto valid = ESM::FormKey::dynamic("native-reference", 3);
    const std::array<ESM::FormKey, 6> invalid{{{},
        {ESM::FormKeyKind::Content,"Oblivion.esm",1},
        {ESM::FormKeyKind::Content,"oblivion.esm",0x1000000},
        {ESM::FormKeyKind::Content,"",1},
        {ESM::FormKeyKind::Dynamic,"native-reference",0},
        {ESM::FormKeyKind::Dynamic,"bad:namespace",3}}};
    for (const auto& key : invalid)
    for (const bool list : {false, true})
    {
        auto& relations = state.mReferences.front().mDropRelations;
        relations = {};
        if (list) relations.mItems = {{key}};
        else relations.mDropper = key;
        EXPECT_THROW(state.serializeBinary(), std::exception);
        EXPECT_THROW(state.canonicalJson(), std::exception);
    }
    state.mReferences.front().mDropRelations = {{}, {{valid, valid}}};
    EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    state.mReferences.front().mDropRelations = {valid, {}};
    const auto bytes = state.serializeBinary();
    const std::string text = "dynamic:native-reference:0000000000000003";
    auto bad = bytes;
    const auto position = std::search(bad.begin(), bad.end(), text.begin(), text.end());
    ASSERT_NE(position, bad.end()); *position = 'D';
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad), std::exception);
}

TEST(ESM4RuntimeState, InventoryExtraScale48PreservesPresenceAndBits)
{
    for (const auto bits : std::array<std::optional<unsigned>,10>{std::nullopt, 0u, 0x80000000u, 1u,
            0x80000001u, 0x3f800000u, 0x3e051eb8u, 0xc0000000u, 0x7f7fffffu, 0xff7fffffu})
    {
        auto state = makeState();
        auto scale = bits ? std::optional(std::bit_cast<float>(*bits)) : std::nullopt;
        state.mPlayer.mInventory.front().mExtraScale = scale;
        state.mReferences.front().mInventory.front().mExtraScale = scale;
        const auto bytes = state.serializeBinary();
        const auto restored = ESM4::RuntimeState::deserializeBinary(bytes);
        for (const auto& item : {restored.mPlayer.mInventory.front(), restored.mReferences.front().mInventory.front()})
        {
            ASSERT_EQ(item.mExtraScale.has_value(), bits.has_value());
            if (bits) EXPECT_EQ(std::bit_cast<unsigned>(*item.mExtraScale), *bits);
        }
        EXPECT_EQ(restored.serializeBinary(), bytes);
        if (bits == 0x80000000u) EXPECT_NE(restored.canonicalJson().find("\"extra_scale\":-0.0"), std::string::npos);
    }
    for (const float invalid : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        auto state = makeState(); state.mPlayer.mInventory.front().mExtraScale = invalid;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }
}

TEST(ESM4RuntimeState, InventoryExtraScale48RejectsLossyLegacyWritesAndDefaultsToAbsent)
{
    for (unsigned version=1; version<48; ++version)
    {
        ESM4::RuntimeState state; state.mVersion = version;
        state.mPlayer.mReference = ESM::FormKey::dynamic("player",1);
        state.mPlayer.mCell = ESM::FormKey::content("items.esm",1);
        if (version >= 3)
        {
            state.mPlayer.mRace = ESM::FormKey::content("items.esm",2);
            state.mPlayer.mClass = ESM::FormKey::content("items.esm",3);
        }
        ESM4::RuntimeInventoryItem item; item.mBase = ESM::FormKey::content("items.esm",4); item.mCount = 1;
        state.mPlayer.mInventory.push_back(item);
        EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()).mPlayer.mInventory.front().mExtraScale);
        state.mPlayer.mInventory.front().mExtraScale = 1;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    }
}

TEST(ESM4RuntimeState, InventoryExtraScale48ReadsIndependentlyConstructedWireAndRejectsBadPresence)
{
    const std::vector<std::uint8_t> bytes{
#include "native_inventory_scale_schema48_expected.inc"
    };
    auto state = ESM4::RuntimeState::deserializeBinary(bytes);
    ASSERT_TRUE(state.mPlayer.mInventory.front().mExtraScale);
    EXPECT_EQ(std::bit_cast<unsigned>(*state.mPlayer.mInventory.front().mExtraScale),0x80000000u);
    EXPECT_EQ(state.mReferences.front().mInventory.front().mExtraScale,1.f);
    EXPECT_EQ(state.serializeBinary(),bytes);
    const std::string base = "content:items.esm:000004";
    const auto found = std::search(bytes.begin(),bytes.end(),base.begin(),base.end());
    ASSERT_NE(found,bytes.end());
    const auto offset = std::size_t(found-bytes.begin())+base.size()+38;
    auto corrupt = bytes; corrupt[offset] = 2;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt),std::runtime_error);
    corrupt = bytes; const unsigned infinity = 0x7f800000u;
    for (unsigned i=0;i<4;++i) corrupt[offset+1+i] = (infinity>>(i*8))&255;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt),std::runtime_error);
}

TEST(ESM4RuntimeState, PowerMeleePhaseVersionFortyNineRoundTripsAndRejectsCorruptTags)
{
    auto old = meleeState(); old.mVersion = 48;
    const auto actor = old.mReferences.front().mKey;
    old.mNativeMeleeStates.at(actor).mStrike->mKind = ESM4::MeleeStrikeKind::StandingPower;
    old.mNativeMeleeStates.at(actor).mStrike->mAnimationGroup = "handtohandattackpower";
    const auto oldBytes = old.serializeBinary();
    auto current = ESM4::RuntimeState::deserializeBinary(oldBytes);
    EXPECT_FALSE(current.mNativeMeleeStates.at(actor).mStrike->mPowerPhase);
    current.mVersion = 49;
    for (unsigned phase = 0; phase <= 2; ++phase)
    {
        current.mNativeMeleeStates.at(actor).mStrike->mPowerPhase = static_cast<ESM4::PowerMeleePhase>(phase);
        const auto bytes = current.serializeBinary();
        EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(bytes).mNativeMeleeStates, current.mNativeMeleeStates);
        EXPECT_NE(current.canonicalJson().find("\"power_phase\":" + std::to_string(phase)), std::string::npos);
    }
    auto end = current.serializeBinary();
    current.mNativeMeleeStates.at(actor).mStrike->mPowerPhase = ESM4::PowerMeleePhase::Start;
    auto start = current.serializeBinary(); ASSERT_EQ(start.size(), end.size());
    std::vector<std::size_t> differences;
    for (std::size_t i = 0; i < start.size(); ++i) if (start[i] != end[i]) differences.push_back(i);
    ASSERT_EQ(differences.size(), 1u);
    auto corrupt = start; corrupt[differences[0]] = 3;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
    corrupt = start; corrupt[differences[0] - 1] = 2;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
    auto bad = current; bad.mVersion = 48;
    EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
    bad = current; bad.mNativeMeleeStates.at(actor).mStrike->mKind = ESM4::MeleeStrikeKind::Left;
    EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
    bad = current; bad.mNativeMeleeStates.at(actor).mStrike->mOrdinaryPhase = ESM4::OrdinaryMeleePhase::Contact;
    EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
    EXPECT_EQ(old.serializeBinary(), oldBytes);
}

TEST(ESM4RuntimeState, WorldRegistrationVersionFiftyHasExplicitWirePresenceAndCanonicalJson)
{
    auto state = controllerRagdollState();
    // The independent v35 controller suffix identifies the exact following
    // registration field. v49 has no registration field at that boundary.
    state.mVersion = 49;
    auto& pose = state.mNativeActorRagdolls.begin()->second;
    auto second = pose.mBodies.front(); second.mRecord = 24; second.mNodeRecord = 20;
    pose.mBodies.push_back(second);
    const auto old = state.serializeBinary();
    const auto suffix = controllerRagdollGoldenSuffix();
    const auto found = std::search(old.begin(), old.end(), suffix.begin(), suffix.end());
    ASSERT_NE(found, old.end());
    const auto offset = std::size_t(found - old.begin()) + suffix.size();
    auto expected = old; expected[std::string_view("OMW4STATE").size()] = 50;
    expected.insert(expected.begin() + offset, 0);
    state.mVersion = 50;
    EXPECT_EQ(state.serializeBinary(), expected);
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(expected).mNativeActorRagdolls.begin()->second.mWorldRegistration);
    EXPECT_EQ(state.canonicalJson().find("world_registration"), std::string::npos);
    pose.mWorldRegistration = ESM4::RuntimeRagdollWorldRegistration{{12, 24}, {25}};
    const std::vector<std::uint8_t> registration{
        1, 2,0,0,0, 12,0,0,0, 24,0,0,0, 1,0,0,0, 25,0,0,0};
    expected.erase(expected.begin() + offset);
    expected.insert(expected.begin() + offset, registration.begin(), registration.end());
    EXPECT_EQ(state.serializeBinary(), expected);
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(expected).mNativeActorRagdolls, state.mNativeActorRagdolls);
    EXPECT_NE(state.canonicalJson().find("\"world_registration\":{\"bodies\":[12,24],\"constraints\":[25]}"), std::string::npos);
    for (std::size_t i = 0; i < registration.size(); ++i)
    {
        const std::vector<std::uint8_t> truncated(expected.begin(), expected.begin() + offset + i);
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(truncated), std::runtime_error);
    }
    auto bad = expected; bad[offset] = 2;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad), std::runtime_error);
    bad = expected;
    for (unsigned i = 1; i < 5; ++i) bad[offset + i] = 0xff;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad), std::runtime_error);
    pose.mWorldRegistration.emplace();
    const auto empty = state.serializeBinary();
    const auto restored = ESM4::RuntimeState::deserializeBinary(empty);
    ASSERT_TRUE(restored.mNativeActorRagdolls.begin()->second.mWorldRegistration);
    EXPECT_TRUE(restored.mNativeActorRagdolls.begin()->second.mWorldRegistration->mBodies.empty());
    EXPECT_NE(state.canonicalJson().find("\"world_registration\":{\"bodies\":[],\"constraints\":[]}"), std::string::npos);
}

TEST(ESM4RuntimeState, WorldRegistrationRejectsMalformedRecordSetsAndOlderVersionWrites)
{
    auto state = packedRagdollState(); state.mVersion = 50;
    auto& pose = state.mNativeActorRagdolls.begin()->second;
    auto second = pose.mBodies.front(); second.mRecord = 24; second.mNodeRecord = 20;
    pose.mBodies.push_back(second);
    const std::array<ESM4::RuntimeRagdollWorldRegistration, 7> bad{{
        {{999}, {}}, {{24, 12}, {}}, {{12, 12}, {}}, {{12}, {25}},
        {{12, 24}, {25, 25}}, {{12, 24}, {12}}, {{12, 24}, {0xffffffffu}}}};
    for (const auto& selection : bad)
    {
        pose.mWorldRegistration = selection;
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
        EXPECT_THROW(state.canonicalJson(), std::runtime_error);
    }
    pose.mWorldRegistration = ESM4::RuntimeRagdollWorldRegistration{{12, 24}, {25}};
    ASSERT_NO_THROW(state.validate());
    state.mVersion = 49;
    EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    pose.mWorldRegistration.reset();
    EXPECT_NO_THROW(state.serializeBinary());
}

TEST(ESM4RuntimeState, BlendRegistrationVersionFiftyOneHasExplicitWireAndRejectsCorruptBooleans)
{
    auto state = packedRagdollState(); state.mVersion = 50;
    auto& pose = state.mNativeActorRagdolls.begin()->second;
    pose.mBodies[0].mNativeMotion = ESM4::RuntimeRagdollMotion::Dynamic;
    pose.mNativeBlends = std::vector<ESM4::RuntimeRagdollBlendState>{{12, 0xf123, 0xffffffffu, -0.f, -2.f}};
    const auto legacy = state.serializeBinary();
    const std::vector<std::uint8_t> fields{12,0,0,0, 0x23,0xf1, 0xff,0xff,0xff,0xff,
        0,0,0,0x80, 0,0,0,0xc0};
    const auto found = std::search(legacy.begin(),legacy.end(),fields.begin(),fields.end());
    ASSERT_NE(found,legacy.end()); const auto offset = std::size_t(found-legacy.begin())+fields.size();
    state.mVersion = 51;
    auto expected = legacy; expected[std::string_view("OMW4STATE").size()] = 51;
    expected.insert(expected.begin()+offset,0);
    EXPECT_EQ(state.serializeBinary(),expected);
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(expected).mNativeActorRagdolls.begin()->second.mNativeBlends->front().mRegistration);
    pose.mNativeBlends->front().mRegistration = ESM4::RuntimeRagdollBlendRegistration{
        std::numeric_limits<std::int32_t>::min(),true};
    expected.erase(expected.begin()+offset);
    const std::array<std::uint8_t,6> registration{1,0,0,0,0x80,1};
    expected.insert(expected.begin()+offset,registration.begin(),registration.end());
    EXPECT_EQ(state.serializeBinary(),expected);
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(expected).mNativeActorRagdolls,state.mNativeActorRagdolls);
    EXPECT_NE(state.canonicalJson().find("\"registration\":{\"counter\":-2147483648,\"remembered_world_owner\":true}"),std::string::npos);
    for (auto index : {offset,offset+5})
    {
        auto corrupt = expected; corrupt[index] = 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt),std::runtime_error);
    }
    for (std::size_t cut=offset;cut<offset+registration.size();++cut)
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary({expected.begin(),expected.begin()+cut}),std::runtime_error);
    state.mVersion = 50; EXPECT_THROW(state.serializeBinary(),std::runtime_error);
    pose.mNativeBlends->front().mRegistration.reset(); EXPECT_EQ(state.serializeBinary(),legacy);
}

TEST(ESM4RuntimeState, BlendRegistrationIsCompleteAndPreservesBothSignedExtremes)
{
    auto state = packedRagdollState(); state.mVersion = 51;
    auto& pose = state.mNativeActorRagdolls.begin()->second;
    pose.mBodies[0].mNativeMotion = ESM4::RuntimeRagdollMotion::Dynamic;
    auto second = pose.mBodies.front(); second.mRecord = 24; second.mNodeRecord = 20;
    pose.mBodies.push_back(second);
    pose.mNativeBlends = std::vector<ESM4::RuntimeRagdollBlendState>{
        {12,1,8,1,1,ESM4::RuntimeRagdollBlendRegistration{std::numeric_limits<std::int32_t>::max(),false}},
        {24,1,8,1,1}};
    EXPECT_THROW(state.serializeBinary(),std::runtime_error);
    pose.mNativeBlends->back().mRegistration = ESM4::RuntimeRagdollBlendRegistration{-1,true};
    const auto bytes = state.serializeBinary();
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(bytes).mNativeActorRagdolls,state.mNativeActorRagdolls);
}

TEST(ESM4RuntimeState, InternalFilterVersionFiftyTwoHasLiteralWireAndLegacyAbsence)
{
    auto state = packedRagdollState(); state.mVersion = 51;
    auto& pose = state.mNativeActorRagdolls.begin()->second;
    auto second = pose.mBodies.front(); second.mRecord = 24; second.mNodeRecord = 20; pose.mBodies.push_back(second);
    pose.mWorldRegistration = ESM4::RuntimeRagdollWorldRegistration{{12,24},{25}};
    const auto legacy = state.serializeBinary();
    const std::vector<std::uint8_t> registration{1,2,0,0,0, 12,0,0,0, 24,0,0,0, 1,0,0,0, 25,0,0,0};
    const auto found = std::search(legacy.begin(),legacy.end(),registration.begin(),registration.end());
    ASSERT_NE(found,legacy.end());
    ASSERT_EQ(std::search(found+1,legacy.end(),registration.begin(),registration.end()),legacy.end());
    const auto offset = std::size_t(found-legacy.begin())+registration.size();
    state.mVersion = 52;
    auto expected = legacy; expected[std::string_view("OMW4STATE").size()] = 52;
    expected.insert(expected.begin()+offset,0);
    EXPECT_EQ(state.serializeBinary(),expected);
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(expected).mNativeActorRagdolls.begin()->second.mInternalFilters);
    pose.mInternalFilters = std::vector<ESM4::RuntimeRagdollInternalFilter>{{12,0x12340108},{24,0x98760208}};
    const std::vector<std::uint8_t> words{1,2,0,0,0, 12,0,0,0, 8,1,0x34,0x12, 24,0,0,0, 8,2,0x76,0x98};
    expected.erase(expected.begin()+offset); expected.insert(expected.begin()+offset,words.begin(),words.end());
    EXPECT_EQ(state.serializeBinary(),expected);
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(expected).mNativeActorRagdolls,state.mNativeActorRagdolls);
    EXPECT_NE(state.canonicalJson().find("\"internal_filters\":[{\"body_record\":12,\"packed_filter\":305398024},"),std::string::npos);
    auto bad = expected; bad[offset] = 2;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad),std::runtime_error);
    bad = expected; bad[offset+1] = 0;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad),std::runtime_error);
    for (std::size_t cut=offset;cut<offset+words.size();++cut)
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary({expected.begin(),expected.begin()+cut}),std::runtime_error);
    state.mVersion = 51; EXPECT_THROW(state.serializeBinary(),std::runtime_error);
    pose.mInternalFilters.reset(); EXPECT_EQ(state.serializeBinary(),legacy);
}

TEST(ESM4RuntimeState, InternalFilterRecordsMustCoverEveryBodyInCanonicalOrderAndUseVerifiedLayers)
{
    auto state = packedRagdollState(); state.mVersion = 52;
    auto& pose = state.mNativeActorRagdolls.begin()->second;
    auto body = pose.mBodies.front(); body.mRecord = 24; body.mNodeRecord = 20; pose.mBodies.push_back(body);
    const std::vector<ESM4::RuntimeRagdollInternalFilter> valid{{12,0x12340108},{24,0x98760208}};
    for (auto words : {std::vector<ESM4::RuntimeRagdollInternalFilter>{},
            std::vector<ESM4::RuntimeRagdollInternalFilter>{{12,8}},
            std::vector<ESM4::RuntimeRagdollInternalFilter>{{24,8},{12,8}},
            std::vector<ESM4::RuntimeRagdollInternalFilter>{{12,8},{12,8}},
            std::vector<ESM4::RuntimeRagdollInternalFilter>{{12,8},{99,8}},
            std::vector<ESM4::RuntimeRagdollInternalFilter>{{12,32},{24,8}}})
    {
        pose.mInternalFilters = std::move(words); EXPECT_THROW(state.serializeBinary(),std::runtime_error);
    }
    pose.mInternalFilters = valid;
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()).mNativeActorRagdolls,state.mNativeActorRagdolls);
}

namespace
{
    ESM4::RuntimeState looseWeaponState()
    {
        auto state = makeState(); state.mVersion = 53;
        state.mReferences.front().mKey = ESM::FormKey::dynamic("dropped-item", 1);
        ESM4::RuntimeLooseWeapon weapon;
        weapon.mBase = state.mReferences.front().mBase;
        weapon.mModel = "meshes/weapons/iron/shortsword.nif";
        weapon.mAssetHash = "00112233445566778899aabbccddeeff";
        weapon.mPhysicalScale = 1.25f;
        weapon.mSceneCollisionFlags = 0x010d;
        weapon.mBody.mRecord = 12; weapon.mBody.mNodeRecord = 8;
        weapon.mBody.mPosition = {1.25f, -2, 64};
        weapon.mBody.mLinearVelocity = {3, 4, -5};
        weapon.mBody.mAngularVelocity = {-.5f, .25f, -0.f};
        weapon.mBody.mNativePackedVelocity = ESM4::PhysicalWorldSceneVelocities{{1, 2, 3, -0.f}, {4, 5, 6, .125f}};
        weapon.mBody.mNativeMotion = ESM4::RuntimeRagdollMotion::Dynamic;
        weapon.mRootLocal.emplace(); weapon.mRootLocal->mPosition = {2, 3, 4}; weapon.mRootLocal->mScale = .75f;
        state.mNativeLooseWeapons.emplace(state.mReferences.front().mKey, weapon);
        return state;
    }

    std::vector<std::uint8_t> looseWeaponGoldenSuffix()
    {
        // Literal schema53 suffix, also checked by the Python save inspector.
        constexpr std::string_view hex =
        "010000002500000064796e616d69633a64726f707065642d6974656d3a303030303030303030303030303030311b0000"
        "00636f6e74656e743a6f626c6976696f6e2e65736d3a303030323030220000006d65736865732f776561706f6e732f69"
        "726f6e2f73686f727473776f72642e6e6966200000003030313132323333343435353636373738383939616162626363"
        "6464656566660000a03f0d010c000000080000000000803f0000000000000000000000000000803f0000000000000000"
        "000000000000803f0000a03f000000c00000804200004040000080400000a0c0000000bf0000803e000000800000803f"
        "000000400000404000000080000080400000a0400000c0400000003e01010000803f0000000000000000000000000000"
        "803f0000000000000000000000000000803f0000004000004040000080400000403f";
        std::vector<std::uint8_t> bytes;
        const auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
        for (std::size_t i = 0; i < hex.size(); i += 2)
            bytes.push_back(static_cast<std::uint8_t>((digit(hex[i]) << 4) | digit(hex[i+1])));
        return bytes;
    }
}

TEST(ESM4RuntimeState, LooseWeaponVersionFiftyThreeHasLiteralWireAndLegacyAbsence)
{
    const auto state = looseWeaponState();
    auto old = state; old.mVersion = 52; old.mNativeLooseWeapons.clear();
    const auto legacy = old.serializeBinary();
    EXPECT_TRUE(ESM4::RuntimeState::deserializeBinary(legacy).mNativeLooseWeapons.empty());
    auto expected = legacy; expected[std::string_view("OMW4STATE").size()] = 53;
    const auto offset = expected.size(); const auto suffix = looseWeaponGoldenSuffix();
    expected.insert(expected.end(), suffix.begin(), suffix.end());
    auto multiple = state;
    const std::array<ESM::FormKey, 2> keys{ESM::FormKey::dynamic("a", 1), ESM::FormKey::dynamic("a-", 1)};
    multiple.mReferences = {state.mReferences.front(), state.mReferences.front()};
    multiple.mNativeLooseWeapons.clear();
    std::vector<std::uint8_t> orderedSuffix{2, 0, 0, 0};
    const auto tail = suffix.begin() + 8 + state.mNativeLooseWeapons.begin()->first.serialize().size();
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        multiple.mReferences[i].mKey = keys[i];
        multiple.mNativeLooseWeapons.emplace(keys[i], state.mNativeLooseWeapons.begin()->second);
        const auto key = keys[i].serialize();
        ASSERT_LT(key.size(), 256u);
        orderedSuffix.insert(orderedSuffix.end(), {static_cast<std::uint8_t>(key.size()), 0, 0, 0});
        orderedSuffix.insert(orderedSuffix.end(), key.begin(), key.end());
        orderedSuffix.insert(orderedSuffix.end(), tail, suffix.end());
    }
    const auto multiBytes = multiple.serializeBinary();
    ASSERT_GE(multiBytes.size(), orderedSuffix.size());
    EXPECT_TRUE(std::equal(orderedSuffix.rbegin(), orderedSuffix.rend(), multiBytes.rbegin()));
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(multiBytes).mNativeLooseWeapons, multiple.mNativeLooseWeapons);
    EXPECT_EQ(state.serializeBinary(), expected);
    const auto restored = ESM4::RuntimeState::deserializeBinary(expected);
    EXPECT_EQ(restored.mNativeLooseWeapons, state.mNativeLooseWeapons);
    EXPECT_EQ(restored.serializeBinary(), expected);
    const auto& body = restored.mNativeLooseWeapons.begin()->second.mBody;
    EXPECT_EQ(std::bit_cast<std::uint32_t>(body.mAngularVelocity[2]), 0x80000000u);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(body.mNativePackedVelocity->mLinear[3]), 0x80000000u);
    EXPECT_NE(restored.canonicalJson().find("\"native_loose_weapons\":[{\"reference\":\"dynamic:dropped-item:"), std::string::npos);
    EXPECT_NE(restored.canonicalJson().find("\"angular_velocity\":[-0.5,0.25,-0.0]"), std::string::npos);
    auto withoutRoot = state; withoutRoot.mNativeLooseWeapons.begin()->second.mRootLocal.reset();
    auto absent = expected; absent.resize(absent.size()-52); absent.back() = 0;
    EXPECT_EQ(withoutRoot.serializeBinary(), absent);
    EXPECT_FALSE(ESM4::RuntimeState::deserializeBinary(absent).mNativeLooseWeapons.begin()->second.mRootLocal);
    old.mVersion = 53;
    auto empty = legacy; empty[std::string_view("OMW4STATE").size()] = 53; empty.insert(empty.end(), 4, 0);
    EXPECT_EQ(old.serializeBinary(), empty);
    auto duplicate = expected; duplicate[offset] = 2;
    duplicate.insert(duplicate.end(), suffix.begin()+4, suffix.end());
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(duplicate), std::runtime_error);
    for (std::size_t cut = offset; cut < expected.size(); ++cut)
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary({expected.begin(), expected.begin()+cut}), std::runtime_error);
    auto bad = expected; bad[bad.size()-53] = 2;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad), std::runtime_error);
    bad = expected; bad[bad.size()-54] = 6;
    EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad), std::runtime_error);
}

TEST(ESM4RuntimeState, LooseWeaponRejectsInvalidIdentityMotionAndGeometry)
{
    const auto state = looseWeaponState();
    auto invalid = state; invalid.mVersion = 52;
    EXPECT_THROW(invalid.serializeBinary(), std::runtime_error);
    invalid = state; invalid.mReferences.clear(); EXPECT_THROW(invalid.serializeBinary(), std::runtime_error);
    invalid = state; invalid.mNativeLooseWeapons.begin()->second.mBase = ESM::FormKey::content("oblivion.esm", 7);
    EXPECT_THROW(invalid.serializeBinary(), std::runtime_error);
    invalid = state; invalid.mPlayer.mReference = invalid.mNativeLooseWeapons.begin()->first;
    EXPECT_THROW(invalid.serializeBinary(), std::runtime_error);
    for (int mutation = 0; mutation < 12; ++mutation)
    {
        invalid = state; auto& weapon = invalid.mNativeLooseWeapons.begin()->second;
        switch (mutation)
        {
            case 0: weapon.mModel = "meshes/../weapon.nif"; break;
            case 1: weapon.mAssetHash[0] = 'A'; break;
            case 2: weapon.mBody.mNodeRecord = 0xffffffffu; break;
            case 3: weapon.mBody.mNativePackedVelocity.reset(); break;
            case 4: weapon.mBody.mNativeMotion.reset(); break;
            case 5: weapon.mBody.mNativeMotion = ESM4::RuntimeRagdollMotion::Keyframed; break;
            case 6: weapon.mPhysicalScale = 0; break;
            case 7: weapon.mBody.mRotation[0] = -1; break;
            case 8: weapon.mBody.mLinearVelocity[0] = std::numeric_limits<float>::infinity(); break;
            case 9: weapon.mBody.mNativePackedVelocity->mAngular[3] = std::numeric_limits<float>::quiet_NaN(); break;
            case 10: weapon.mRootLocal->mRotation[4] = 2; break;
            case 11: weapon.mRootLocal->mScale = -1; break;
        }
        EXPECT_THROW(invalid.serializeBinary(), std::runtime_error) << mutation;
    }
    auto actor = packedRagdollState(); actor.mVersion = 53;
    auto weapon = state.mNativeLooseWeapons.begin()->second;
    weapon.mBase = actor.mReferences.front().mBase;
    actor.mNativeLooseWeapons.emplace(actor.mReferences.front().mKey, weapon);
    EXPECT_THROW(actor.serializeBinary(), std::runtime_error);
}

namespace
{
    ESM4::RuntimeState reactionState()
    {
        auto state = meleeState();
        state.mVersion = 54;
        std::sort(state.mPhysicalActions.mPending.begin(), state.mPhysicalActions.mPending.end());
        const auto actor = state.mReferences.front().mKey;
        state.mNativeActorValues.front().mProcess = ESM4::ActorValueProcess::Active;
        state.mNativeActorValues.front().mProcessAction = 6;
        state.mNativeAnimationClocks.emplace(actor, 0);
        auto& melee = state.mNativeMeleeStates.at(actor);
        melee.mStrike.reset();
        melee.mReaction = ESM4::RuntimeMeleeReaction{3, ESM4::MeleeReactionKind::BlockHit, "blockhit"};
        return state;
    }
}

TEST(ESM4RuntimeState, Reaction54PreservesActivationActiveAndUnanchoredRetirement)
{
    for (auto kind : {ESM4::MeleeReactionKind::BlockHit, ESM4::MeleeReactionKind::Recoil,
             ESM4::MeleeReactionKind::Stagger})
        for (int phase = 0; phase != 5; ++phase)
        {
            auto state = reactionState();
            const auto actor = state.mReferences.front().mKey;
            auto& reaction = *state.mNativeMeleeStates.at(actor).mReaction;
            reaction.mKind = kind;
            state.mNativeActorValues.front().mProcessAction = reaction.processAction();
            auto& timing = reaction.mSequenceTiming;
            if (phase == 1) { timing.mEasing = true; timing.mEaseEnd = .125f; }
            if (phase >= 2)
            {
                timing.mOffset = -0.f; timing.mEaseStart = -0.f; timing.mLastInput = .25f;
                timing.mWeightedTime = .25f; timing.mOutputTime = .25f;
                reaction.mStarted = true; reaction.mAnimationTime = .25f;
            }
            if (phase >= 3) reaction.mRetirement = ESM4::RuntimeReactionRetirement{.125f, {}};
            if (phase == 4) reaction.mRetirement->mStartClock = 1000.25f;
            const auto bytes = state.serializeBinary();
            const auto loaded = ESM4::RuntimeState::deserializeBinary(bytes);
            EXPECT_EQ(loaded.mNativeMeleeStates, state.mNativeMeleeStates);
            EXPECT_EQ(loaded.serializeBinary(), bytes);
            EXPECT_EQ(loaded.canonicalJson(), state.canonicalJson());
            EXPECT_EQ(loaded.mPhysicalActions, state.mPhysicalActions);
            const std::string prefix = "reaction54_" + std::to_string(static_cast<unsigned>(kind))
                + "_" + std::to_string(phase);
            std::string wire;
            constexpr char hex[] = "0123456789abcdef";
            for (auto byte : bytes) { wire += hex[byte >> 4]; wire += hex[byte & 15]; }
            RecordProperty(prefix + "_hex", wire);
            RecordProperty(prefix + "_json", state.canonicalJson());
            if (phase >= 2)
            {
                EXPECT_TRUE(std::signbit(*loaded.mNativeMeleeStates.at(actor).mReaction->mSequenceTiming.mOffset));
            }
        }
}

TEST(ESM4RuntimeState, Reaction54RejectsReplayConflictingPlaybackAndMalformedTimers)
{
    const auto state = reactionState();
    const auto actor = state.mReferences.front().mKey;
    const auto reject = [&](auto change) {
        auto bad = state; change(bad);
        EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
    };
    reject([](auto& s) { s.mVersion = 53; });
    reject([&](auto& s) { s.mNativeAnimationClocks.clear(); });
    reject([&](auto& s) { s.mNativeActorValues.front().mProcessAction = 7; });
    for (auto id : {0u, 1u, 2u, 5u, 7u})
        reject([&](auto& s) { s.mNativeMeleeStates.at(actor).mReaction->mContactId = id; });
    reject([&](auto& s) { s.mNativeMeleeStates.at(actor).mReaction->mKind = static_cast<ESM4::MeleeReactionKind>(3); });
    reject([&](auto& s) { s.mNativeMeleeStates.at(actor).mStrike = meleeState().mNativeMeleeStates.at(actor).mStrike; });
    reject([&](auto& s) { s.mNativeMeleeStates.at(actor).mReaction->mAnimationGroup.clear(); });
    reject([&](auto& s) { s.mNativeMeleeStates.at(actor).mReaction->mSequenceTiming.mOffset = 0; });
    reject([&](auto& s) { s.mNativeMeleeStates.at(actor).mReaction->mRetirement = ESM4::RuntimeReactionRetirement{.1f, {}}; });
    for (float bad : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        reject([&](auto& s) { s.mNativeMeleeStates.at(actor).mReaction->mAnimationTime = bad; });
    auto active = state;
    auto& reaction = *active.mNativeMeleeStates.at(actor).mReaction;
    reaction.mSequenceTiming = ESM4::updateMeleeSequenceTiming({}, 0, 1, 0, 1);
    for (float duration : {0.f, -1.f, std::numeric_limits<float>::infinity()})
    {
        reaction.mRetirement = ESM4::RuntimeReactionRetirement{duration, {}};
        EXPECT_THROW(active.serializeBinary(), std::runtime_error);
    }
    reaction.mRetirement = ESM4::RuntimeReactionRetirement{.125f, -1.f};
    EXPECT_THROW(active.serializeBinary(), std::runtime_error);
    reaction.mRetirement->mStartClock = 0;
    reaction.mSequenceTiming.mEasing = true;
    EXPECT_THROW(active.serializeBinary(), std::runtime_error);
    const auto bytes = state.serializeBinary();
    for (std::size_t size = 0; size < bytes.size(); ++size)
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary({bytes.begin(), bytes.begin() + size}), std::runtime_error);
    auto changed = state;
    changed.mNativeMeleeStates.at(actor).mReaction->mStarted = true;
    const auto changedBytes = changed.serializeBinary();
    unsigned differences = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        if (bytes[i] != changedBytes[i])
        {
            ++differences;
            auto malformed = bytes; malformed[i] = 2;
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(malformed), std::runtime_error);
        }
    EXPECT_EQ(differences, 1u);
}

TEST(ESM4RuntimeState, Reaction54DoesNotInventLegacySequenceOrClockOwnership)
{
    for (std::int16_t action : {6, 7, 8})
    {
        auto old = reactionState(); const auto actor = old.mReferences.front().mKey;
        old.mVersion = 53; old.mNativeMeleeStates.at(actor).mReaction.reset();
        old.mNativeAnimationClocks.clear(); old.mNativeActorValues.front().mProcessAction = action;
        const auto wire = old.serializeBinary();
        auto loaded = ESM4::RuntimeState::deserializeBinary(wire);
        EXPECT_FALSE(loaded.mNativeMeleeStates.at(actor).mReaction);
        EXPECT_TRUE(loaded.mNativeAnimationClocks.empty());
        EXPECT_EQ(loaded.serializeBinary(), wire);
        loaded.mVersion = 54;
        const auto current = ESM4::RuntimeState::deserializeBinary(loaded.serializeBinary());
        EXPECT_FALSE(current.mNativeMeleeStates.at(actor).mReaction);
        EXPECT_TRUE(current.mNativeAnimationClocks.empty());
        EXPECT_EQ(current.mNativeActorValues.front().mProcessAction, action);
    }
}

TEST(ESM4RuntimeState, Reaction55PreservesInactiveActionAndOrderedOutgoingSequences)
{
    for (bool anchored : {false, true})
    {
        auto state = reactionState(); state.mVersion = 55;
        const auto actor = state.mReferences.front().mKey;
        auto& melee = state.mNativeMeleeStates.at(actor);
        auto& reaction = *melee.mReaction;
        reaction.mSequenceTiming = ESM4::updateMeleeSequenceTiming({}, 0, 1, 0, 1);
        reaction.mSequenceTiming.mOffset = -0.f;
        reaction.mSequenceTiming.mEaseStart = .25f;
        reaction.mSequenceTiming.mLastInput = .5f;
        reaction.mSequenceTiming.mWeightedTime = .5f;
        reaction.mSequenceTiming.mOutputTime = .5f;
        reaction.mAnimationTime = .5f; reaction.mStarted = true; reaction.mInactive = true;
        reaction.mRetirement = ESM4::RuntimeReactionRetirement{.125f, .5f};
        state.mNativeAnimationClocks.at(actor) = .625f;
        ESM4::RuntimeRetiringMeleeSequence outgoing{3, "handtohandattackleft", .25f,
            reaction.mSequenceTiming, {.25f, anchored ? std::optional<float>(.5f) : std::nullopt}};
        melee.mRetiringSequences = {outgoing, outgoing};
        melee.mRetiringSequences.back().mAnimationGroup = "handtohandrecoil";
        const auto bytes = state.serializeBinary();
        const auto loaded = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(loaded.mNativeMeleeStates, state.mNativeMeleeStates);
        EXPECT_EQ(loaded.mPhysicalActions, state.mPhysicalActions);
        EXPECT_EQ(loaded.canonicalJson(), state.canonicalJson());
        EXPECT_EQ(loaded.serializeBinary(), bytes);
        EXPECT_TRUE(std::signbit(*loaded.mNativeMeleeStates.at(actor).mReaction->mSequenceTiming.mOffset));
        const std::string prefix = anchored ? "reaction55_anchored" : "reaction55_unanchored";
        std::string wire; constexpr char hex[] = "0123456789abcdef";
        for (auto byte : bytes) { wire += hex[byte >> 4]; wire += hex[byte & 15]; }
        RecordProperty(prefix + "_hex", wire);
        RecordProperty(prefix + "_json", state.canonicalJson());
        auto bad = state; bad.mVersion = 54;
        EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
        bad = state; bad.mNativeAnimationClocks.at(actor) = .5f;
        EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
        bad = state; bad.mNativeMeleeStates.at(actor).mRetiringSequences.front().mContactId = 1;
        EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
        bad = state; bad.mNativeMeleeStates.at(actor).mRetiringSequences.front().mRetirement.mDuration = 0;
        EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
        // A detached strike can be interrupted before its first manager update.
        melee.mRetiringSequences.front().mSequenceTiming = {};
        if (anchored)
        {
            EXPECT_THROW(state.serializeBinary(), std::runtime_error);
            melee.mRetiringSequences.front().mRetirement.mStartClock.reset();
        }
        EXPECT_NO_THROW(state.serializeBinary());
    }
}

TEST(ESM4RuntimeState, Reaction56PreservesCapturedLocalPoseAndRejectsInvalidOwnership)
{
    auto state = reactionState(); state.mVersion = 56;
    const auto actor = state.mReferences.front().mKey;
    auto& reaction = *state.mNativeMeleeStates.at(actor).mReaction;
    reaction.mSequenceTiming.mEasing = true;
    reaction.mSequenceTiming.mEaseEnd = .1f;
    reaction.mPoseSnapshot = ESM4::RuntimeMeleePoseSnapshot{0,
        {{"Bip01 Pelvis", {11, 22, 33}, {1, 0, 0, 0}, 2, 9},
         {"Bip01 Head", {-0.f, 0, 0}, {0, 0, 0, 1}, -0.f, 255}}};
    const auto bytes = state.serializeBinary();
    const auto loaded = ESM4::RuntimeState::deserializeBinary(bytes);
    EXPECT_EQ(loaded.mNativeMeleeStates, state.mNativeMeleeStates);
    EXPECT_EQ(loaded.canonicalJson(), state.canonicalJson());
    EXPECT_EQ(loaded.serializeBinary(), bytes);
    EXPECT_TRUE(std::signbit(loaded.mNativeMeleeStates.at(actor).mReaction->mPoseSnapshot->mTracks[1].mScale));
    std::string wire; constexpr char hex[] = "0123456789abcdef";
    for (auto byte : bytes) { wire += hex[byte >> 4]; wire += hex[byte & 15]; }
    RecordProperty("reaction56_hex", wire);
    RecordProperty("reaction56_json", state.canonicalJson());
    auto bad = state; bad.mVersion = 55;
    EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
    bad = state; bad.mNativeMeleeStates.at(actor).mReaction->mSequenceTiming.mEasing = false;
    EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
    bad = state; bad.mNativeMeleeStates.at(actor).mReaction->mSequenceTiming.mEaseEnd = 0;
    EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
    bad = state;
    auto& tracks = bad.mNativeMeleeStates.at(actor).mReaction->mPoseSnapshot->mTracks;
    tracks.push_back(tracks.front()); tracks.back().mBone = "bip01 pelvis";
    EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
    tracks.pop_back(); tracks.front().mRotation = {};
    EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
    tracks.front().mRotation = {1, 0, 0, 0};
    tracks.front().mTranslation[0] = std::numeric_limits<float>::infinity();
    EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
    reaction.mPoseSnapshot->mTracks.clear();
    EXPECT_NO_THROW(state.serializeBinary()); // A sequence can have no transform-controlled blocks.
}

namespace
{
    ESM4::RuntimeState firstPersonReactionState()
    {
        auto state = reactionState();
        state.mVersion = 57;
        const auto actor = state.mReferences.front().mKey;
        const auto player = state.mPlayer.mReference;
        ESM4::RuntimeActorValues values;
        values.mActor = player;
        values.mBase = ESM::FormKey::content("oblivion.esm", 7);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mProcess = ESM4::ActorValueProcess::Active;
        values.mProcessAction = 6;
        state.mNativeActorValues.push_back(values);
        state.mNativeActorLife.push_back({player, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        auto node = state.mNativeMeleeStates.extract(actor);
        node.key() = player;
        state.mNativeMeleeStates.insert(std::move(node));
        state.mNativeAnimationClocks.clear();
        state.mNativeAnimationClocks.emplace(player, .5f);
        auto& melee = state.mNativeMeleeStates.at(player);
        auto& body = *melee.mReaction;
        // Full original Player activation + first update. Same alias in both
        // views, but body state2 fades and first-person state1 starts directly.
        body.mSequenceTiming = {.mEasing = true, .mOffset = -.5f, .mEaseStart = .5f,
            .mEaseEnd = .6f, .mLastInput = 0.f};
        auto mirror = body;
        mirror.mSequenceTiming = {.mEasing = false, .mOffset = -.25f, .mEaseStart = 0.f,
            .mEaseEnd = .25f, .mLastInput = 0.f};
        melee.mFirstPerson = ESM4::RuntimeFirstPersonMeleeState{.25f, std::move(mirror)};
        return state;
    }
}

TEST(ESM4RuntimeState, Reaction57PreservesIndependentViewsThroughBodyRecoveryAndMirrorRetirement)
{
    for (unsigned phase = 0; phase < 6; ++phase)
    {
        auto state = firstPersonReactionState();
        auto& melee = state.mNativeMeleeStates.at(state.mPlayer.mReference);
        auto& first = *melee.mFirstPerson;
        if (phase == 1 || phase == 3)
        {
            first.mReaction->mStarted = true;
            first.mReaction->mAnimationTime = .125f;
            first.mReaction->mRetirement = ESM4::RuntimeReactionRetirement{.125f, .25f};
            auto& timing = first.mReaction->mSequenceTiming;
            timing.mLastInput = timing.mWeightedTime = timing.mOutputTime = .125f;
        }
        if (phase == 2)
        {
            melee.mReaction.reset();
            state.mNativeActorValues.back().mProcessAction = -1;
        }
        if (phase == 3)
        {
            first.mClock = .375f;
            first.mReaction->mInactive = true;
        }
        if (phase >= 4) first.mReaction.reset();
        if (phase == 5) first.mClock = -0.f;
        const auto bytes = state.serializeBinary();
        const auto loaded = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(loaded.mNativeMeleeStates, state.mNativeMeleeStates) << phase;
        EXPECT_EQ(loaded.mNativeAnimationClocks, state.mNativeAnimationClocks) << phase;
        EXPECT_EQ(loaded.mPhysicalActions.mNext, state.mPhysicalActions.mNext);
        EXPECT_EQ(loaded.mPhysicalActions.mPending, state.mPhysicalActions.mPending);
        EXPECT_EQ(loaded.mPhysicalActionOwners, state.mPhysicalActionOwners);
        EXPECT_EQ(loaded.serializeBinary(), bytes);
        EXPECT_EQ(loaded.canonicalJson(), state.canonicalJson());
        if (phase == 5)
        {
            EXPECT_TRUE(std::signbit(loaded.mNativeMeleeStates.at(state.mPlayer.mReference).mFirstPerson->mClock));
            EXPECT_NE(loaded.canonicalJson().find("\"first_person\":{\"clock\":-0.0,\"reaction\":null}"),
                std::string::npos);
        }
        std::string wire;
        constexpr char hex[] = "0123456789abcdef";
        for (auto byte : bytes) { wire += hex[byte >> 4]; wire += hex[byte & 15]; }
        RecordProperty("reaction57_hex_" + std::to_string(phase), wire);
        RecordProperty("reaction57_json_" + std::to_string(phase), state.canonicalJson());
    }
}

TEST(ESM4RuntimeState, Reaction57RejectsForeignOwnersReplayConflictsAndPrematureMirrorDeactivation)
{
    const auto valid = firstPersonReactionState();
    const auto actor = valid.mPlayer.mReference;
    const auto reject = [&](auto mutate) {
        auto state = valid;
        mutate(state);
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    };
    for (const float clock : {-1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
        reject([&](auto& state) { state.mNativeMeleeStates.at(actor).mFirstPerson->mClock = clock; });
    reject([&](auto& s) { s.mVersion = 56; });
    reject([&](auto& s) { s.mNativeAnimationClocks.clear(); });
    reject([&](auto& s) { s.mNativeActorValues.back().mProcess = ESM4::ActorValueProcess::Low; });
    reject([&](auto& s) { s.mNativeMeleeStates.at(actor).mFirstPerson->mReaction->mContactId = 2; });
    reject([&](auto& s) { s.mNativeMeleeStates.at(actor).mFirstPerson->mReaction->mContactId = 7; });
    reject([&](auto& s) { s.mNativeMeleeStates.at(actor).mFirstPerson->mReaction->mKind = ESM4::MeleeReactionKind::Recoil; });
    reject([&](auto& s) { s.mNativeMeleeStates.at(actor).mFirstPerson->mReaction->mSequenceTiming.mOffset.reset(); });
    reject([&](auto& s) {
        auto& reaction = *s.mNativeMeleeStates.at(actor).mFirstPerson->mReaction;
        reaction.mRetirement = ESM4::RuntimeReactionRetirement{.125f, .25f};
        reaction.mInactive = true;
    });
    reject([&](auto& s) {
        const auto npc = s.mReferences.front().mKey;
        auto node = s.mNativeMeleeStates.extract(actor);
        node.key() = npc;
        s.mNativeMeleeStates.insert(std::move(node));
        s.mNativeAnimationClocks.emplace(npc, .5f);
    });
    auto absent = valid;
    absent.mNativeMeleeStates.at(actor).mFirstPerson.reset();
    const auto bytes = valid.serializeBinary();
    const auto noMirror = absent.serializeBinary();
    const auto field = std::mismatch(bytes.begin(), bytes.end(), noMirror.begin(), noMirror.end()).first - bytes.begin();
    ASSERT_LT(field + 15, bytes.size());
    for (const std::size_t index : {std::size_t(field), std::size_t(field + 5), std::size_t(field + 14)})
    {
        auto bad = bytes;
        bad[index] = index == std::size_t(field + 14) ? 3 : 2;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad), std::runtime_error) << index;
    }
    for (std::size_t cut = 0; cut < bytes.size(); ++cut)
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(std::vector(bytes.begin(), bytes.begin() + cut)),
            std::runtime_error) << cut;
}

TEST(ESM4RuntimeState, Reaction57DoesNotInventFirstPersonStateWhenPromotingLegacy)
{
    auto legacy = reactionState();
    legacy.mVersion = 56;
    const auto bytes = legacy.serializeBinary();
    auto loaded = ESM4::RuntimeState::deserializeBinary(bytes);
    EXPECT_FALSE(loaded.mNativeMeleeStates.begin()->second.mFirstPerson);
    EXPECT_EQ(loaded.canonicalJson().find("first_person"), std::string::npos);
    loaded.mVersion = 57;
    const auto promoted = ESM4::RuntimeState::deserializeBinary(loaded.serializeBinary());
    EXPECT_FALSE(promoted.mNativeMeleeStates.begin()->second.mFirstPerson);
    EXPECT_EQ(promoted.mNativeMeleeStates.begin()->second.mReaction, legacy.mNativeMeleeStates.begin()->second.mReaction);
    EXPECT_EQ(promoted.mNativeAnimationClocks, legacy.mNativeAnimationClocks);
    EXPECT_NE(promoted.canonicalJson().find("\"first_person\":null"), std::string::npos);
}

namespace
{
    ESM4::RuntimeState firstPersonStrikeState()
    {
        auto state = firstPersonReactionState();
        state.mVersion = 58;
        auto& melee = state.mNativeMeleeStates.at(state.mPlayer.mReference);
        melee.mReaction.reset();
        melee.mFirstPerson->mReaction.reset();
        ESM4::RuntimeMeleeStrike body{3, ESM4::MeleeStrikeKind::Left, {}, "handtohandattackleft", .7f, .25f, true};
        body.mOrdinaryPhase = ESM4::OrdinaryMeleePhase::Contact;
        body.mSequenceTiming = ESM4::MeleeSequenceTiming{.mOffset = -.5f, .mEaseStart = .25f, .mEaseEnd = .5f,
            .mLastInput = .25f, .mWeightedTime = .25f, .mOutputTime = .25f};
        melee.mStrike = body;
        auto first = body;
        first.mAnimationTime = .125f;
        first.mOrdinaryPhase = ESM4::OrdinaryMeleePhase::Start;
        first.mSequenceTiming = ESM4::MeleeSequenceTiming{.mOffset = -.25f, .mEaseStart = .125f, .mEaseEnd = .25f,
            .mLastInput = .125f, .mWeightedTime = .125f, .mOutputTime = .125f};
        melee.mFirstPerson->mStrike = first;
        state.mNativeActorValues.back().mProcessAction = 3;
        return state;
    }
}

TEST(ESM4RuntimeState, Strike58RetainsIndependentViewHistoryAndCapturedPoseAfterBodyFinishes)
{
    for (unsigned phase = 0; phase < 6; ++phase)
    {
        auto state = firstPersonStrikeState();
        auto& melee = state.mNativeMeleeStates.at(state.mPlayer.mReference);
        if (phase == 1)
        {
            auto& strike = *melee.mFirstPerson->mStrike;
            strike.mSequenceTiming->mEasing = true;
            strike.mSequenceTiming->mEaseStart = .25f;
            strike.mSequenceTiming->mEaseEnd = .375f;
            strike.mPoseSnapshot = ESM4::RuntimeMeleePoseSnapshot{0, {{"Bip01", {1, 2, 3}, {1, 0, 0, 0}, 1, 9}}};
        }
        if (phase == 2 || phase == 3)
        {
            melee.mStrike.reset();
            state.mNativeActorValues.back().mProcessAction = -1;
        }
        if (phase == 3) melee.mFirstPerson->mStrike->mOrdinaryPhase = ESM4::OrdinaryMeleePhase::End;
        if (phase >= 4)
        {
            const auto id = state.mPhysicalActions.mNext++;
            state.mPhysicalActions.mPending.push_back(id);
            std::sort(state.mPhysicalActions.mPending.begin(), state.mPhysicalActions.mPending.end());
            state.mPhysicalActionOwners.emplace(id, state.mPlayer.mReference);
            state.mNativeActorValues.back().mProcessAction = 2;
            for (auto* strike : {&*melee.mStrike, &*melee.mFirstPerson->mStrike})
            {
                strike->mActionId = id;
                strike->mContactCommitted = false;
                strike->mAnimationTime = 0;
                strike->mOrdinaryPhase = ESM4::OrdinaryMeleePhase::Start;
                strike->mSequenceTiming = ESM4::MeleeSequenceTiming{.mEasing = true, .mOffset = 0.f, .mEaseEnd = .1f};
                strike->mPoseSnapshot = ESM4::RuntimeMeleePoseSnapshot{0,
                    {{"Bip01", {1, 2, 3}, {1, 0, 0, 0}, 1, 9}}};
            }
            if (phase == 5)
            {
                const auto old = firstPersonStrikeState();
                const auto& owners = old.mNativeMeleeStates.at(state.mPlayer.mReference);
                for (const bool first : {false, true})
                {
                    const auto& strike = first ? *owners.mFirstPerson->mStrike : *owners.mStrike;
                    ESM4::RuntimeRetiringMeleeSequence outgoing{3, "handtohandattackright", strike.mAnimationTime,
                        *strike.mSequenceTiming, {.125f, first ? .25f : .5f}};
                    (first ? melee.mFirstPerson->mRetiringSequences : melee.mRetiringSequences).push_back(outgoing);
                }
            }
        }
        const auto bytes = state.serializeBinary();
        const auto loaded = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(loaded.mNativeMeleeStates, state.mNativeMeleeStates);
        EXPECT_EQ(loaded.serializeBinary(), bytes);
        EXPECT_EQ(loaded.canonicalJson(), state.canonicalJson());
        std::string wire;
        constexpr char hex[] = "0123456789abcdef";
        for (auto byte : bytes) { wire += hex[byte >> 4]; wire += hex[byte & 15]; }
        RecordProperty("strike58_hex_" + std::to_string(phase), wire);
        RecordProperty("strike58_json_" + std::to_string(phase), state.canonicalJson());
    }
}

TEST(ESM4RuntimeState, Strike58RejectsSplitIdentityReplayMissingHistoryAndInvalidCapturedPose)
{
    const auto valid = firstPersonStrikeState();
    const auto reject = [&](auto mutate) {
        auto bad = valid;
        mutate(bad, bad.mNativeMeleeStates.at(bad.mPlayer.mReference));
        EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
    };
    reject([](auto&, auto& m) { m.mFirstPerson->mStrike->mActionId = 2; });
    reject([](auto&, auto& m) { m.mFirstPerson->mStrike->mPlaybackSpeed = 1; });
    reject([](auto&, auto& m) { m.mFirstPerson->mStrike->mSequenceTiming.reset(); });
    reject([](auto&, auto& m) { m.mFirstPerson->mStrike->mContactCommitted = false; });
    reject([](auto&, auto& m) { m.mFirstPerson->mReaction = ESM4::RuntimeMeleeReaction{3,
        ESM4::MeleeReactionKind::BlockHit, "blockhit"}; });
    reject([](auto&, auto& m) { m.mFirstPerson->mStrike->mPoseSnapshot = ESM4::RuntimeMeleePoseSnapshot{}; });
    reject([](auto& s, auto&) { s.mVersion = 57; });
    reject([](auto&, auto& m) { m.mStrike.reset(); m.mFirstPerson->mStrike->mContactCommitted = false; });
    reject([](auto&, auto& m) { m.mFirstPerson->mRetiringSequences.push_back({2, "old",
        .125f, {false, 0.f, 0.f, .125f, 0, .125f, .125f}, {.1f, {}}}); });
    auto legacy = firstPersonReactionState();
    const auto before = legacy.mNativeMeleeStates;
    legacy.mVersion = 58;
    const auto loaded = ESM4::RuntimeState::deserializeBinary(legacy.serializeBinary());
    EXPECT_EQ(loaded.mNativeMeleeStates, before); // No invented strike, clock or pose history.
}

TEST(ESM4RuntimeState, CapturedReactionRetainsRequestedOffsetBeforeBothViewAnchors)
{
    for (unsigned mask = 1; mask < 4; ++mask)
    {
        auto state = firstPersonReactionState();
        state.mVersion = 58;
        auto& melee = state.mNativeMeleeStates.at(state.mPlayer.mReference);
        const std::array<ESM4::RuntimeMeleeReaction*, 2> views{&*melee.mReaction, &*melee.mFirstPerson->mReaction};
        for (unsigned view = 0; view < views.size(); ++view)
            if (mask & (1u << view))
            {
                auto& reaction = *views[view];
                reaction.mAnimationTime = 0;
                reaction.mStarted = false;
                reaction.mSequenceTiming = {.mEasing = true, .mOffset = 0.f, .mEaseEnd = .125f};
                reaction.mPoseSnapshot = ESM4::RuntimeMeleePoseSnapshot{};
            }
        const auto bytes = state.serializeBinary();
        const auto loaded = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(loaded.canonicalJson(), state.canonicalJson());
        EXPECT_EQ(loaded.serializeBinary(), bytes);
        std::string wire;
        constexpr char hex[] = "0123456789abcdef";
        for (auto byte : bytes) { wire += hex[byte >> 4]; wire += hex[byte & 15]; }
        RecordProperty("reaction58_partial_hex_" + std::to_string(mask), wire);
        RecordProperty("reaction58_partial_json_" + std::to_string(mask), state.canonicalJson());
        for (unsigned view = 0; view < views.size(); ++view)
            if (mask & (1u << view))
            {
                const auto valid = *views[view];
                views[view]->mPoseSnapshot.reset();
                EXPECT_THROW(state.serializeBinary(), std::runtime_error);
                *views[view] = valid;
                views[view]->mSequenceTiming.mLastInput = 0;
                EXPECT_THROW(state.serializeBinary(), std::runtime_error);
                *views[view] = valid;
            }
    }
}

namespace
{
    ESM4::RuntimeState passiveSequenceState(unsigned protocol)
    {
        auto state = firstPersonStrikeState();
        state.mVersion = 59;
        auto& melee = state.mNativeMeleeStates.at(state.mPlayer.mReference);
        const auto body = *melee.mStrike;
        const auto first = *melee.mFirstPerson->mStrike;
        melee.mStrike.reset();
        melee.mFirstPerson->mStrike.reset();
        melee.mReaction = ESM4::RuntimeMeleeReaction{4, ESM4::MeleeReactionKind::Stagger, "stagger"};
        melee.mFirstPerson->mReaction = melee.mReaction;
        const auto passive = [&](const ESM4::RuntimeMeleeStrike& source) {
            if (protocol == 0 || protocol == 3)
            {
                auto strike = source;
                strike.mContactCommitted = false; // Canceled, not granted a new contact.
                if (protocol == 3)
                {
                    strike.mAnimationTime = 0;
                    strike.mOrdinaryPhase = ESM4::OrdinaryMeleePhase::Start;
                    strike.mSequenceTiming = ESM4::MeleeSequenceTiming{.mEasing = true, .mOffset = 0.f, .mEaseEnd = .125f};
                    strike.mPoseSnapshot = ESM4::RuntimeMeleePoseSnapshot{0,
                        {{"Bip01", {1, 2, 3}, {1, 0, 0, 0}, 1, 9}}};
                }
                return ESM4::RuntimePassiveMeleeSequence{strike};
            }
            const bool block = protocol == 1 || protocol == 4;
            ESM4::RuntimeMeleeReaction reaction{3, block ? ESM4::MeleeReactionKind::BlockHit
                : ESM4::MeleeReactionKind::Recoil, block ? "blockhit" : "recoil"};
            reaction.mStarted = true;
            reaction.mAnimationTime = source.mAnimationTime;
            reaction.mSequenceTiming = *source.mSequenceTiming;
            if (protocol == 4)
            {
                reaction.mStarted = false;
                reaction.mAnimationTime = 0;
                reaction.mSequenceTiming = {.mEasing = true, .mOffset = 0.f, .mEaseEnd = .125f};
                reaction.mPoseSnapshot = ESM4::RuntimeMeleePoseSnapshot{0,
                    {{"Bip01", {1, 2, 3}, {1, 0, 0, 0}, 1, 9}}};
            }
            return ESM4::RuntimePassiveMeleeSequence{reaction};
        };
        melee.mPassiveSequence = passive(body);
        melee.mFirstPerson->mPassiveSequence = passive(first);
        state.mNativeActorValues.back().mProcessAction = 8;
        return state;
    }
}

TEST(ESM4RuntimeState, Passive59PreservesSlot3ProtocolsIndependentOfStaggerAndRecovery)
{
    for (unsigned protocol = 0; protocol < 5; ++protocol)
        for (unsigned mask = 1; mask < 4; ++mask)
            for (bool recovered : {false, true})
            {
                auto state = passiveSequenceState(protocol);
                auto& melee = state.mNativeMeleeStates.at(state.mPlayer.mReference);
                if (!(mask & 1)) melee.mPassiveSequence.reset();
                if (!(mask & 2)) melee.mFirstPerson->mPassiveSequence.reset();
                if (recovered)
                {
                    melee.mReaction.reset();
                    melee.mFirstPerson->mReaction.reset();
                    state.mNativeActorValues.back().mProcessAction = -1;
                }
                const auto bytes = state.serializeBinary();
                auto loaded = ESM4::RuntimeState::deserializeBinary(bytes);
                EXPECT_EQ(loaded.mNativeMeleeStates, state.mNativeMeleeStates);
                EXPECT_EQ(loaded.mPhysicalActions, state.mPhysicalActions);
                EXPECT_EQ(loaded.mPhysicalActionOwners, state.mPhysicalActionOwners);
                std::string wire;
                constexpr char hex[] = "0123456789abcdef";
                for (auto byte : bytes) { wire += hex[byte >> 4]; wire += hex[byte & 15]; }
                const auto key = std::to_string(protocol) + "_" + std::to_string(mask) + "_" + std::to_string(recovered);
                RecordProperty("passive59_hex_" + key, wire);
                RecordProperty("passive59_json_" + key, state.canonicalJson());
            }
}

TEST(ESM4RuntimeState, Passive59RejectsPendingCausesForeignOwnershipAndSlotConflicts)
{
    const auto valid = passiveSequenceState(0);
    const auto reject = [&](auto change) {
        auto state = valid;
        auto& melee = state.mNativeMeleeStates.at(state.mPlayer.mReference);
        change(state, melee);
        EXPECT_THROW(state.serializeBinary(), std::runtime_error);
    };
    reject([](auto& s, auto&) { s.mVersion = 58; });
    reject([](auto& s, auto&) { s.mNativeAnimationClocks.clear(); });
    reject([](auto& s, auto&) { s.mNativeActorValues.back().mProcess = ESM4::ActorValueProcess::Low; });
    reject([](auto&, auto& m) { std::get<ESM4::RuntimeMeleeStrike>(m.mPassiveSequence->mSequence).mActionId = 2; });
    reject([](auto& s, auto& m) { std::get<ESM4::RuntimeMeleeStrike>(m.mPassiveSequence->mSequence).mActionId = s.mPhysicalActions.mNext; });
    reject([](auto&, auto& m) { std::get<ESM4::RuntimeMeleeStrike>(m.mFirstPerson->mPassiveSequence->mSequence).mActionId = 5; });
    reject([](auto&, auto& m) { std::get<ESM4::RuntimeMeleeStrike>(m.mPassiveSequence->mSequence).mSequenceTiming.reset(); });
    reject([](auto&, auto& m) { m.mPassiveSequence->mSequence = *m.mReaction; });
    reject([](auto&, auto& m) { m.mFirstPerson->mPassiveSequence->mSequence = *m.mFirstPerson->mReaction; });
    reject([](auto&, auto& m) { m.mStrike = std::get<ESM4::RuntimeMeleeStrike>(m.mPassiveSequence->mSequence); });
    reject([](auto&, auto& m) { m.mFirstPerson->mStrike = std::get<ESM4::RuntimeMeleeStrike>(m.mFirstPerson->mPassiveSequence->mSequence); });
    reject([](auto&, auto& m) { m.mReaction->mKind = ESM4::MeleeReactionKind::Recoil; });
    reject([](auto&, auto& m) { m.mFirstPerson->mReaction->mKind = ESM4::MeleeReactionKind::BlockHit; });
    const auto bytes = valid.serializeBinary();
    const std::array<std::uint8_t, 18> marker{1, 3,0,0,0,0,0,0,0, 0, 4,0,0,0, 'n','u','l','l'};
    unsigned tags = 0;
    for (auto it = std::search(bytes.begin(), bytes.end(), marker.begin(), marker.end()); it != bytes.end();
         it = std::search(it + 1, bytes.end(), marker.begin(), marker.end()))
    {
        auto corrupt = bytes;
        corrupt[it - bytes.begin()] = 3;
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(corrupt), std::runtime_error);
        ++tags;
    }
    EXPECT_EQ(tags, 2u);
    auto legacy = firstPersonStrikeState();
    const auto loaded = ESM4::RuntimeState::deserializeBinary(legacy.serializeBinary());
    EXPECT_FALSE(loaded.mNativeMeleeStates.begin()->second.mPassiveSequence);
    EXPECT_FALSE(loaded.mNativeMeleeStates.begin()->second.mFirstPerson->mPassiveSequence);
    EXPECT_EQ(loaded.canonicalJson().find("passive_sequence"), std::string::npos);
}

TEST(ESM4RuntimeState, Counter60RetainsThreeKeyViewsAndDistinctConsumedAction)
{
    for (const bool committed : {false, true})
    for (unsigned bodyPhase = 0; bodyPhase < 3; ++bodyPhase)
    for (unsigned firstPhase = 0; firstPhase < 3; ++firstPhase)
    {
        auto state = firstPersonStrikeState(); state.mVersion = 60;
        const auto actor = state.mPlayer.mReference;
        auto& melee = state.mNativeMeleeStates.at(actor);
        for (auto* strike : {&*melee.mStrike, &*melee.mFirstPerson->mStrike})
        {
            strike->mKind = ESM4::MeleeStrikeKind::Counter;
            strike->mAnimationGroup = "handtohandblockattack";
            strike->mPlaybackSpeed = 1;
            strike->mContactCommitted = committed;
            strike->mOrdinaryPhase = ESM4::OrdinaryMeleePhase::Start;
        }
        melee.mStrike->mPowerPhase = static_cast<ESM4::PowerMeleePhase>(bodyPhase);
        melee.mFirstPerson->mStrike->mPowerPhase = static_cast<ESM4::PowerMeleePhase>(firstPhase);
        if (bodyPhase == 2) melee.mStrike->mRetirement = ESM4::RuntimeReactionRetirement{.1f,
            state.mNativeAnimationClocks.at(actor)};
        if (firstPhase == 2) melee.mFirstPerson->mStrike->mRetirement = ESM4::RuntimeReactionRetirement{.1f,
            melee.mFirstPerson->mClock};
        state.mNativeActorValues.back().mProcessAction = committed ? 6 : 2;
        if (!committed)
        {
            state.mPhysicalActions.mPending.push_back(3);
            std::sort(state.mPhysicalActions.mPending.begin(), state.mPhysicalActions.mPending.end());
            state.mPhysicalActionOwners.emplace(3, actor);
        }
        const auto bytes = state.serializeBinary();
        auto loaded = ESM4::RuntimeState::deserializeBinary(bytes);
        EXPECT_EQ(loaded.serializeBinary(), bytes);
        EXPECT_EQ(loaded.mNativeMeleeStates, state.mNativeMeleeStates);
        EXPECT_EQ(melee.mStrike->processAction(), committed ? 6 : 2);
        auto bodyOnly = state;
        bodyOnly.mNativeMeleeStates.at(actor).mFirstPerson.reset();
        const auto bodyBytes = bodyOnly.serializeBinary();
        EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(bodyBytes).serializeBinary(), bodyBytes);
        auto legacy = state; legacy.mVersion = 59;
        auto& oldMelee = legacy.mNativeMeleeStates.at(actor);
        oldMelee.mStrike->mKind = oldMelee.mFirstPerson->mStrike->mKind = ESM4::MeleeStrikeKind::StandingPower;
        oldMelee.mStrike->mRetirement.reset(); oldMelee.mFirstPerson->mStrike->mRetirement.reset();
        legacy.mNativeActorValues.back().mProcessAction = committed ? 3 : 2;
        const auto oldBytes = legacy.serializeBinary();
        EXPECT_EQ(bytes.size(), oldBytes.size() + 4 + 9 * ((bodyPhase == 2) + (firstPhase == 2)));
        const auto reject = [&](auto mutate) {
            auto bad = state; mutate(bad, bad.mNativeMeleeStates.at(actor));
            EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
        };
        reject([](auto& x, auto&) { x.mVersion = 59; });
        reject([](auto& x, auto&) { x.mNativeActorValues.back().mProcessAction = 3; });
        reject([](auto&, auto& m) { m.mStrike->mPlaybackSpeed = .7f; });
        reject([](auto&, auto& m) { m.mStrike->mPowerPhase.reset(); });
        reject([](auto&, auto& m) { m.mStrike->mSequenceTiming.reset(); });
        reject([](auto&, auto& m) { m.mStrike->mKind = static_cast<ESM4::MeleeStrikeKind>(8); });
        reject([](auto&, auto& m) { m.mFirstPerson->mStrike->mPowerPhase.reset(); });
        if (committed)
        {
            loaded.mNativeMeleeStates.at(actor).mStrike.reset();
            loaded.mNativeActorValues.back().mProcessAction = -1;
            EXPECT_NO_THROW(loaded.serializeBinary());
            loaded.mVersion = 59;
            EXPECT_THROW(loaded.serializeBinary(), std::runtime_error); // FP-only tag7 is versioned too.
        }
    }
}

TEST(ESM4RuntimeState, Counter60InactiveIntervalsAndBothViewWireFlagsAreStrict)
{
    auto state = firstPersonStrikeState(); state.mVersion = 60;
    const auto actor = state.mPlayer.mReference;
    auto& melee = state.mNativeMeleeStates.at(actor);
    for (const bool first : {false, true})
    {
        auto& strike = first ? *melee.mFirstPerson->mStrike : *melee.mStrike;
        const float clock = first ? melee.mFirstPerson->mClock : state.mNativeAnimationClocks.at(actor);
        strike.mKind = ESM4::MeleeStrikeKind::Counter; strike.mPlaybackSpeed = 1;
        strike.mAnimationGroup = "handtohandblockattack";
        strike.mOrdinaryPhase = ESM4::OrdinaryMeleePhase::Start; strike.mPowerPhase = ESM4::PowerMeleePhase::End;
        strike.mRetirement = ESM4::RuntimeReactionRetirement{.1f, clock - .25f}; strike.mInactive = true;
    }
    state.mNativeActorValues.back().mProcessAction = 6;
    const auto bytes = state.serializeBinary();
    EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(bytes).mNativeMeleeStates, state.mNativeMeleeStates);
    for (const bool first : {false, true})
    {
        auto bad = state; auto& m = bad.mNativeMeleeStates.at(actor);
        auto& strike = first ? *m.mFirstPerson->mStrike : *m.mStrike;
        strike.mRetirement->mStartClock = first ? m.mFirstPerson->mClock : bad.mNativeAnimationClocks.at(actor);
        EXPECT_THROW(bad.serializeBinary(), std::runtime_error);
    }
    const std::string marker = "handtohandblockattack";
    auto from = bytes.begin(); unsigned views = 0;
    while (true)
    {
        const auto at = std::search(from, bytes.end(), marker.begin(), marker.end());
        if (at == bytes.end()) break;
        ++views; const auto tail = std::size_t(at - bytes.begin()) + marker.size();
        for (const auto flag : {42u, 47u, 52u})
        {
            ASSERT_EQ(bytes.at(tail + flag), 1);
            auto bad = bytes; bad.at(tail + flag) = 2;
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(bad), std::runtime_error);
        }
        from = at + marker.size();
    }
    EXPECT_EQ(views, 2u);
}
