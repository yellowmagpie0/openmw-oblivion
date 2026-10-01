#include <components/esm4/loadrefr.hpp>
#include <components/esm4/runtimestate.hpp>

#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>

#include <gtest/gtest.h>

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
                                 "\"remaining_usage_time\":812.5}]"),
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
        state.mPlayer.mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x18baa), 2 } };
        state.mReferences[0].mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x400), -1 } };
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
        state.mPlayer.mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x18baa), 2 } };
        state.mReferences[0].mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x400), -1 } };
        const ESM4::RuntimeState loaded = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        EXPECT_EQ(loaded.mVersion, 2u);
        EXPECT_TRUE(loaded.mPlayer.mRace.isNull());
        EXPECT_EQ(loaded.canonicalJson().find("character_generation_flags"), std::string::npos);
    }

    TEST(ESM4RuntimeState, versionThreePayloadMigratesWithoutM13ItemMetadata)
    {
        auto state = makeState();
        state.mVersion = 3;
        state.mPlayer.mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x18baa), 2 } };
        state.mReferences[0].mInventory = { { ESM::FormKey::content("Oblivion.esm", 0x400), -1 } };
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
        auto state = engagedState();
        const auto actor = state.mNativeActorValues.front().mActor;
        state.mNativeActorManagerTime = 99999.5f;
        state.mNativeActorUpdateTimes = {{actor, 99999.f}};
        const auto bytes = state.serializeBinary();
        const auto entrySize = 8 + actor.serialize().size();
        const auto countOffset = bytes.size() - entrySize - 4;
        auto invalid = bytes;
        invalid[countOffset] = 2;
        invalid.insert(invalid.end(), bytes.begin() + countOffset + 4, bytes.end());
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
        invalid = bytes;
        std::fill_n(invalid.begin() + countOffset, 4, 0xff);
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
        invalid = bytes;
        invalid[countOffset + 8] = 'C';
        EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
        for (const auto offset : {bytes.size() - 4, countOffset - 4})
        {
            invalid = bytes;
            const std::array<std::uint8_t, 4> infinity{0, 0, 0x80, 0x7f};
            std::copy(infinity.begin(), infinity.end(), invalid.begin() + offset);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
        }
        for (std::size_t cut = 1; cut <= entrySize + 8; ++cut)
        {
            SCOPED_TRACE(cut);
            invalid.assign(bytes.begin(), bytes.end() - cut);
            EXPECT_THROW(ESM4::RuntimeState::deserializeBinary(invalid), std::runtime_error);
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
            EXPECT_NE(restored.canonicalJson().find("\"condition\":-0.0"), std::string::npos);
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
