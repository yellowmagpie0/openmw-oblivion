#include <components/esm4/loadrefr.hpp>
#include <components/esm4/runtimestate.hpp>

#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>

#include <gtest/gtest.h>

#include <cmath>
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

}
