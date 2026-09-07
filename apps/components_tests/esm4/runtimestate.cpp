#include <components/esm4/loadrefr.hpp>
#include <components/esm4/runtimestate.hpp>

#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>

#include <gtest/gtest.h>

#include <cmath>
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
        EXPECT_NE(json.find("\"schema_version\":6"), std::string::npos);
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
}
