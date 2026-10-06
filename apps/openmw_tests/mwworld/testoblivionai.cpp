/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#include <gtest/gtest.h>

#include <array>

#include <components/esm4/aipackagedata.hpp>
#include <components/esm4/aiphase.hpp>
#include <components/esm4/aiselection.hpp>
#include <components/esm4/runtimereferences.hpp>

#include "apps/openmw/mwmechanics/oblivionaidestination.hpp"
#include "apps/openmw/mwmechanics/oblivionaigait.hpp"
#include "apps/openmw/mwmechanics/obliviondoorstate.hpp"
#include "apps/openmw/mwmechanics/oblivionidle.hpp"
#include "apps/openmw/mwmechanics/oblivionpackageevents.hpp"

namespace
{
    using namespace ESM4;

    ESM::FormKey key(std::uint32_t id)
    {
        return ESM::FormKey::content("m14-test.esm", id);
    }

    PackageCandidate candidate(std::uint32_t id, AIPackageType type, std::size_t listIndex)
    {
        PackageCandidate result;
        result.mKey = key(id);
        result.mType = type;
        result.mListIndex = listIndex;
        result.mSchedule.mStartHour = -1;
        result.mSchedule.mDuration = 0;
        return result;
    }

    TEST(OblivionAiTest, LastPathgridNodeDoesNotReplaceTheActualDestination)
    {
        // The failing Telepe save had consumed the last node but remained
        // about 178 units from its follow destination. Repeated waiting and
        // reload cannot repair an arrival that was declared prematurely.
        const osg::Vec3f lastNodePosition(-673.5923f, 236.0759f, -158.1195f);
        const osg::Vec3f destination(-496.0045f, 241.5149f, -147.3725f);
        EXPECT_FALSE(MWMechanics::oblivionDestinationReached(key(1), lastNodePosition, key(1), destination, 64.f));
        EXPECT_TRUE(MWMechanics::oblivionRouteTailWithinDirectApproach(lastNodePosition, destination, 256.f));
        EXPECT_TRUE(MWMechanics::oblivionRouteTailWithinDirectApproach(
            osg::Vec3f(454.466f, -40.6367f, -60.102f),
            osg::Vec3f(539.108f, -14.8854f, -102.f), 256.f));
        EXPECT_FALSE(MWMechanics::oblivionRouteTailWithinDirectApproach(
            destination + osg::Vec3f(257.f, 0.f, 0.f), destination, 256.f));
        EXPECT_TRUE(MWMechanics::oblivionDestinationReached(
            key(1), destination + osg::Vec3f(64.f, 0.f, 0.f), key(1), destination, 64.f));
        EXPECT_FALSE(MWMechanics::oblivionDestinationReached(
            key(1), destination + osg::Vec3f(65.f, 0.f, 0.f), key(1), destination, 64.f));
        EXPECT_FALSE(MWMechanics::oblivionDestinationReached(key(1), destination, key(2), destination, 64.f));
        EXPECT_FALSE(MWMechanics::oblivionDestinationReached({}, destination, {}, destination, 64.f));
        EXPECT_FALSE(MWMechanics::oblivionDestinationReached(key(1), destination, key(1), destination, -1.f));
    }

    TEST(OblivionAiTest, NativePlayerReferenceAliasesRuntimePlayerButNotNpcBase)
    {
        const auto player = ESM::FormKey::dynamic("player", 1);
        EXPECT_EQ(ESM4::runtimeReferenceKey(ESM::FormKey::content("Oblivion.esm", 0x14)), player);
        EXPECT_EQ(ESM4::runtimeReferenceKey(player), player);
        for (const auto& other : { ESM::FormKey{}, ESM::FormKey::content("Oblivion.esm", 7),
                 ESM::FormKey::content("another.esp", 0x14), ESM::FormKey::dynamic("npc", 0x14) })
        {
            EXPECT_EQ(ESM4::runtimeReferenceKey(other), other);
            EXPECT_NE(ESM4::runtimeReferenceKey(other), player);
        }
    }

    TEST(OblivionAiTest, TravelHonorsAuthoredRadiusWithoutChangingOtherPackageArrival)
    {
        const osg::Vec3f destination(800.f, -100.f, -100.f);
        for (const float radius : { 120.f, 512.f })
        {
            EXPECT_TRUE(MWMechanics::oblivionDestinationReached(key(1),
                destination + osg::Vec3f(radius, 0.f, 0.f), key(1), destination, 64.f,
                AIPackageType::Travel, radius));
            EXPECT_FALSE(MWMechanics::oblivionDestinationReached(key(1),
                destination + osg::Vec3f(radius + 1.f, 0.f, 0.f), key(1), destination, 64.f,
                AIPackageType::Travel, radius));
            EXPECT_FALSE(MWMechanics::oblivionDestinationReached(key(1),
                destination + osg::Vec3f(radius, 0.f, 0.f), key(1), destination, 64.f,
                AIPackageType::Follow, radius));
        }
        EXPECT_FALSE(MWMechanics::oblivionDestinationReached(
            key(1), destination, key(2), destination, 64.f, AIPackageType::Travel, 120.f));
        EXPECT_FALSE(MWMechanics::oblivionDestinationReached(
            key(1), destination, key(1), destination, 64.f, AIPackageType::Travel, -1.f));
    }

    TEST(OblivionAiTest, TravelArrivalSeparatesStandingSlotFromVerticalMarkerOffset)
    {
        const osg::Vec3f destination(230.50577f, -91.31252f, -32.12009f);
        const osg::Vec3f premature(198.19360f, -41.13493f, -31.f);
        EXPECT_TRUE(MWMechanics::oblivionDestinationReached(key(1), premature, key(1), destination, 64.f));
        EXPECT_FALSE(MWMechanics::oblivionDestinationReached(
            key(1), premature, key(1), destination, 64.f, AIPackageType::Travel));
        for (const float vertical : { -64.f, 0.f, 64.f })
            EXPECT_TRUE(MWMechanics::oblivionDestinationReached(key(1),
                destination + osg::Vec3f(16.f, 0.f, vertical), key(1), destination, 64.f, AIPackageType::Travel));
        for (const auto offset : { osg::Vec3f(17.f, 0.f, 0.f), osg::Vec3f(0.f, 17.f, 0.f),
                 osg::Vec3f(0.f, 0.f, 65.f), osg::Vec3f(16.f, 16.f, 0.f) })
            EXPECT_FALSE(MWMechanics::oblivionDestinationReached(
                key(1), destination + offset, key(1), destination, 64.f, AIPackageType::Travel));
        EXPECT_FALSE(MWMechanics::oblivionDestinationReached(
            key(1), destination, key(2), destination, 64.f, AIPackageType::Travel));
        EXPECT_FALSE(MWMechanics::oblivionDestinationReached(
            {}, destination, {}, destination, 64.f, AIPackageType::Travel));
        EXPECT_FALSE(MWMechanics::oblivionDestinationReached(
            key(1), destination, key(1), destination, -1.f, AIPackageType::Travel));
    }

    TEST(OblivionAiTest, FollowCompletesWhenItsTargetReachesTheAuthoredDestination)
    {
        const osg::Vec3f destination(686.59314f, 80.28291f, -111.98434f);
        const osg::Vec3f targetAtDestination(755.79565f, 80.77766f, -100.87106f);
        EXPECT_TRUE(MWMechanics::oblivionFollowDestinationReached(
            key(1), targetAtDestination, key(1), destination, 70));
        EXPECT_FALSE(MWMechanics::oblivionFollowDestinationReached(
            key(1), targetAtDestination + osg::Vec3f(1.f, 0.f, 0.f), key(1), destination, 70));
        EXPECT_FALSE(MWMechanics::oblivionFollowDestinationReached(
            key(1), targetAtDestination, key(2), destination, 70));
        EXPECT_FALSE(MWMechanics::oblivionFollowDestinationReached(
            key(1), targetAtDestination, key(1), destination, -1));

        PackageSelection selection;
        selection.mSource = PackageSource::Base;
        selection.mPackage = key(10);
        selection.mType = AIPackageType::Follow;
        PackagePhaseState state = beginPackagePhase(selection, key(100), key(101));
        state.mPhase = PackagePhase::Wait;
        state.mDurationRemaining = 1.f;
        PackagePhaseInput input;
        input.mActionCompleted = MWMechanics::oblivionFollowDestinationReached(
            key(1), targetAtDestination, key(1), destination, 70);
        EXPECT_EQ(advancePackagePhase(state, input).mTo, PackagePhase::Complete);
    }

    TEST(OblivionAiTest, DoorRecoveryRequiresAnIdentifiedDoorInterruption)
    {
        RuntimeActorAiState state;
        state.mPhase = PackagePhase::Interrupted;
        state.mDoor = key(1);
        for (const auto* reason : { "door-unavailable", "door-locked" })
        {
            state.mInterruptionReason = reason;
            EXPECT_TRUE(MWMechanics::isOblivionDoorInterruption(state));
        }
        state.mDoor = {};
        EXPECT_FALSE(MWMechanics::isOblivionDoorInterruption(state));
        state.mDoor = key(1);
        state.mInterruptionReason = "target-unresolved";
        EXPECT_FALSE(MWMechanics::isOblivionDoorInterruption(state));
        state.mInterruptionReason = "door-unavailable";
        for (const auto phase : { PackagePhase::Path, PackagePhase::Door, PackagePhase::Stalled, PackagePhase::Complete })
        {
            state.mPhase = phase;
            EXPECT_FALSE(MWMechanics::isOblivionDoorInterruption(state));
        }
    }

    TEST(OblivionAiTest, DoorAvailabilityUsesResidentThenSavedThenAuthoredState)
    {
        using MWMechanics::OblivionDoorState;
        using MWMechanics::resolveOblivionDoorState;
        const OblivionDoorState authored{ false, true, ESM::RefId(ESM::FormId{ 10, 0 }), key(20) };
        EXPECT_FALSE(resolveOblivionDoorState(authored, nullptr, std::nullopt).mAvailable);
        EXPECT_TRUE(resolveOblivionDoorState(authored, nullptr, std::nullopt).mLocked);

        RuntimeReferenceState saved;
        saved.mEnabled = true;
        saved.mLockLevel = 0;
        const auto enabled = resolveOblivionDoorState(authored, &saved, std::nullopt);
        EXPECT_TRUE(enabled.mAvailable);
        EXPECT_FALSE(enabled.mLocked);
        EXPECT_EQ(enabled.mKey, authored.mKey);
        EXPECT_TRUE(enabled.mOwner.isNull());
        saved.mDeleted = true;
        EXPECT_FALSE(resolveOblivionDoorState(authored, &saved, std::nullopt).mAvailable);
        saved.mDeleted = false;
        saved.mEnabled = false;
        saved.mLockLevel = 50;
        saved.mOwner = key(30);
        const auto disabled = resolveOblivionDoorState(authored, &saved, std::nullopt);
        EXPECT_FALSE(disabled.mAvailable);
        EXPECT_TRUE(disabled.mLocked);
        EXPECT_EQ(disabled.mOwner, key(30));

        OblivionDoorState resident{ true, false, {}, {} };
        const auto live = resolveOblivionDoorState(authored, &saved, resident);
        EXPECT_TRUE(live.mAvailable);
        EXPECT_FALSE(live.mLocked);
        EXPECT_TRUE(live.mKey.empty());
        EXPECT_TRUE(live.mOwner.isNull());
        saved.mEnabled = true;
        resident.mAvailable = false;
        EXPECT_FALSE(resolveOblivionDoorState(authored, &saved, resident).mAvailable);
        saved.mCustomState["locked"] = false;
        EXPECT_FALSE(resolveOblivionDoorState(authored, &saved, std::nullopt).mLocked);
        saved.mLockLevel = 0;
        saved.mCustomState["locked"] = true;
        EXPECT_TRUE(resolveOblivionDoorState(authored, &saved, std::nullopt).mLocked);
        saved.mCustomState["locked"] = std::int64_t(1);
        EXPECT_THROW(resolveOblivionDoorState(authored, &saved, std::nullopt), std::runtime_error);
        // A stale snapshot cannot override or invalidate resident state.
        EXPECT_FALSE(resolveOblivionDoorState(authored, &saved, resident).mAvailable);
    }

    TEST(OblivionAiTest, NativeDoorOpenStateUsesDistinctQueryAndCommandValues)
    {
        using MWWorld::DoorState;
        EXPECT_EQ(MWMechanics::oblivionDoorOpenState(DoorState::Idle, 0.f, 0.f), 3);
        EXPECT_EQ(MWMechanics::oblivionDoorOpenState(DoorState::Idle, 1.f, 0.f), 1);
        EXPECT_EQ(MWMechanics::oblivionDoorOpenState(DoorState::Opening, 0.f, 0.f), 2);
        EXPECT_EQ(MWMechanics::oblivionDoorOpenState(DoorState::Closing, 1.f, 0.f), 4);
        EXPECT_EQ(MWMechanics::oblivionDoorTransition(0), DoorState::Closing);
        EXPECT_EQ(MWMechanics::oblivionDoorTransition(1), DoorState::Opening);
        EXPECT_FALSE(MWMechanics::oblivionDoorTransition(-1));
        EXPECT_FALSE(MWMechanics::oblivionDoorTransition(2));
    }

    TEST(OblivionAiTest, ResidentPackageActorsObserveSharedPhysicalDoorTransitions)
    {
        RuntimeActorAiState observer;
        observer.mActor = key(1);
        observer.mCell = key(10);
        observer.mPackage = key(20);
        observer.mTier = ProcessTier::High;
        EXPECT_TRUE(MWMechanics::observesOblivionPhysicalDoorTransition(
            observer, key(2), key(10)));

        observer.mActor = key(2);
        EXPECT_FALSE(MWMechanics::observesOblivionPhysicalDoorTransition(
            observer, key(2), key(10)));
        observer.mActor = key(1);
        observer.mTier = ProcessTier::Low;
        EXPECT_FALSE(MWMechanics::observesOblivionPhysicalDoorTransition(
            observer, key(2), key(10)));
        observer.mTier = ProcessTier::High;
        observer.mPackage = {};
        EXPECT_FALSE(MWMechanics::observesOblivionPhysicalDoorTransition(
            observer, key(2), key(10)));
        observer.mPackage = key(20);
        EXPECT_FALSE(MWMechanics::observesOblivionPhysicalDoorTransition(
            observer, key(2), key(11)));
    }

    TEST(OblivionAiTest, NativeIdleAnimationMustBelongToTheActorsSkeletonFamily)
    {
        IdleAnimation idle;
        idle.mModel = "Characters\\_Male\\IdleAnims\\FollowMe.kf";
        idle.mAnimationGroup = 4;
        EXPECT_EQ(MWMechanics::oblivionIdleAnimationGroup(idle, "meshes/characters/_male/skeleton.nif"),
            std::optional<std::string>("followme"));
        EXPECT_EQ(MWMechanics::oblivionPickIdleAnimationGroup(
                      idle, "meshes/characters/_male/skeleton.nif"),
            std::optional<std::string>("followme"));
        EXPECT_FALSE(MWMechanics::oblivionIdleAnimationGroup(
            idle, "meshes/creatures/horse/skeleton.nif"));

        idle.mAnimationGroup = 5;
        EXPECT_FALSE(MWMechanics::oblivionPickIdleAnimationGroup(
            idle, "meshes/characters/_male/skeleton.nif"));
        idle.mModel = "Characters\\_Male\\IdleAnims";
        EXPECT_FALSE(MWMechanics::oblivionIdleAnimationGroup(
            idle, "meshes/characters/_male/skeleton.nif"));
    }

    TEST(OblivionAiTest, PackageDoneRecordsOnlyNewCompletionsAndPreservesCallbackSave)
    {
        MWMechanics::OblivionPackageDoneQueue queue;
        queue.record(PackagePhase::Act, PackagePhase::Complete, key(1), key(10));
        queue.record(PackagePhase::Wait, PackagePhase::Complete, key(2), key(20));
        queue.record(PackagePhase::Complete, PackagePhase::Complete, key(2), key(20));
        queue.record(PackagePhase::Path, PackagePhase::Interrupted, key(3), key(30));
        queue.record(PackagePhase::Path, PackagePhase::Stalled, key(3), key(30));
        queue.record(PackagePhase::Act, PackagePhase::Complete, key(3), {});
        std::vector<RuntimePackageDoneEvent> saved;
        std::vector<RuntimePackageDoneEvent> delivered;
        queue.dispatch([&](const auto& event) {
            delivered.push_back(event);
            if (event.mActor == key(1))
                saved = queue.capture();
            queue.dispatch([&](const auto&) { FAIL() << "Reentrant dispatch"; });
        });
        ASSERT_EQ(delivered.size(), 2u);
        EXPECT_EQ(delivered[0].mPackage, key(10));
        ASSERT_EQ(saved.size(), 1u);
        EXPECT_EQ(saved.front().mActor, key(2));
        EXPECT_TRUE(queue.capture().empty());
        queue.restore(saved);
        queue.dispatch([&](const auto& event) { EXPECT_EQ(event, delivered[1]); });
        queue.dispatch([&](const auto&) { FAIL() << "Completion repeated"; });
    }

    TEST(OblivionAiTest, CompletedPackageRemainsTerminalWhenEvaluationSelectsTheSameWinner)
    {
        RuntimeActorAiState state;
        state.mSource = PackageSource::Base;
        state.mPackage = key(10);
        state.mPackageType = AIPackageType::Travel;
        state.mPhase = PackagePhase::Complete;

        PackageSelection selection;
        selection.mSource = PackageSource::Base;
        selection.mPackage = key(10);
        selection.mType = AIPackageType::Travel;
        EXPECT_TRUE(MWMechanics::oblivionPackageSelectionMatches(state, std::nullopt, selection, {}));

        selection.mPackage = key(11);
        EXPECT_FALSE(MWMechanics::oblivionPackageSelectionMatches(state, std::nullopt, selection, {}));
    }

    TEST(OblivionAiTest, PackageDoneDefersCallbackGeneratedEventsAndStopsAtReload)
    {
        MWMechanics::OblivionPackageDoneQueue queue;
        queue.record(PackagePhase::Act, PackagePhase::Complete, key(1), key(10));
        queue.dispatch([&](const auto&) {
            queue.record(PackagePhase::Act, PackagePhase::Complete, key(2), key(20));
        });
        ASSERT_EQ(queue.capture().size(), 1u);
        EXPECT_EQ(queue.capture().front().mActor, key(2));
        queue.record(PackagePhase::Act, PackagePhase::Complete, key(3), key(30));
        const std::vector<RuntimePackageDoneEvent> restored{ { key(4), key(40) } };
        unsigned count = 0;
        queue.dispatch([&](const auto&) { ++count; queue.restore(restored); });
        EXPECT_EQ(count, 1u);
        EXPECT_EQ(queue.capture(), restored);
        EXPECT_THROW(queue.dispatch([](const auto&) { throw std::runtime_error("callback"); }), std::runtime_error);
        queue.record(PackagePhase::Act, PackagePhase::Complete, key(5), key(50));
        queue.dispatch([&](const auto& event) { EXPECT_EQ(event.mActor, key(5)); });
        EXPECT_TRUE(queue.capture().empty());
    }

    TEST(OblivionAiTest, OverlayInvalidatesOnlyAffectedActiveRoutesIncludingForeignGraphs)
    {
        const std::array<PathgridNodeKey, 2> route{ PathgridNodeKey{ key(100), 0 }, { key(101), 1 } };
        EXPECT_TRUE(MWMechanics::oblivionRouteAffectedByOverlay(PackagePhase::Path, key(100), route, { key(100) }));
        EXPECT_TRUE(MWMechanics::oblivionRouteAffectedByOverlay(PackagePhase::Path, key(100), route, { key(101) }));
        EXPECT_FALSE(MWMechanics::oblivionRouteAffectedByOverlay(PackagePhase::Path, key(100), route, { key(102) }));
        EXPECT_TRUE(MWMechanics::oblivionRouteAffectedByOverlay(PackagePhase::Path, key(100), {}, { key(100) }));
        for (const auto phase : { PackagePhase::Wait, PackagePhase::Act, PackagePhase::Door,
                 PackagePhase::Complete, PackagePhase::Interrupted, PackagePhase::Stalled })
            EXPECT_FALSE(MWMechanics::oblivionRouteAffectedByOverlay(phase, key(100), route, { key(100), key(101) }));
    }

    TEST(OblivionAiTest, MovingDestinationAccumulatesSubThresholdMotion)
    {
        RuntimeActorAiState state;
        ASSERT_TRUE(MWMechanics::updateOblivionMovingDestination(state, key(100), { 0.f, 0.f, 0.f }));
        for (int step = 1; step <= 96; ++step)
        {
            EXPECT_FALSE(MWMechanics::updateOblivionMovingDestination(
                state, key(100), { static_cast<float>(step), 0.f, 0.f }));
            EXPECT_FLOAT_EQ(state.mDestinationPosition.pos[0], 0.f);
        }
        EXPECT_TRUE(MWMechanics::updateOblivionMovingDestination(state, key(100), { 97.f, 0.f, 0.f }));
        EXPECT_FLOAT_EQ(state.mDestinationPosition.pos[0], 97.f);
        EXPECT_FALSE(MWMechanics::updateOblivionMovingDestination(state, key(100), { 98.f, 0.f, 0.f }));
    }

    TEST(OblivionAiTest, AbstractPositionSurvivesUnchangedResidentPositionButNotExternalMoves)
    {
        const osg::Vec3f origin(0.f, 0.f, 0.f);
        // The logical cell is deliberately not an input: crossing a PGRI edge
        // must not be undone merely because the resident Ptr is still behind.
        EXPECT_TRUE(MWMechanics::preserveOblivionAbstractPosition(true, true, key(1), key(1), origin, origin));
        EXPECT_TRUE(MWMechanics::preserveOblivionAbstractPosition(true, false, key(1), {}, origin, origin));
        EXPECT_FALSE(MWMechanics::preserveOblivionAbstractPosition(false, false, key(1), {}, origin, origin));
        EXPECT_FALSE(MWMechanics::preserveOblivionAbstractPosition(true, true, key(2), key(1), origin, origin));
        EXPECT_FALSE(MWMechanics::preserveOblivionAbstractPosition(
            true, true, key(1), key(1), { 65.f, 0.f, 0.f }, origin));
    }

    TEST(OblivionAiTest, MovingDestinationUsesPersistedIntentAcrossTiersAndReload)
    {
        RuntimeActorAiState state;
        state.mActor = key(10);
        state.mBase = key(11);
        state.mCell = key(100);
        ASSERT_TRUE(MWMechanics::updateOblivionMovingDestination(state, key(100), { 0.f, 0.f, 0.f }));
        EXPECT_FALSE(MWMechanics::updateOblivionMovingDestination(state, key(100), { 60.f, 0.f, 0.f }));
        RuntimeState save;
        save.mPlayer.mReference = key(1);
        save.mPlayer.mCell = key(100);
        save.mPlayer.mRace = key(2);
        save.mPlayer.mClass = key(3);
        save.mActorAi.push_back(state);
        RuntimeState loaded = RuntimeState::deserializeBinary(save.serializeBinary());
        ASSERT_EQ(loaded.mActorAi.size(), 1u);
        RuntimeActorAiState& restored = loaded.mActorAi.front();
        restored.mTier = ProcessTier::Low;
        EXPECT_TRUE(MWMechanics::updateOblivionMovingDestination(restored, key(100), { 100.f, 0.f, 0.f }));
        EXPECT_TRUE(MWMechanics::updateOblivionMovingDestination(restored, key(101), { 100.f, 0.f, 0.f }));
        EXPECT_EQ(restored.mDestinationCell, key(101));
    }

    TEST(OblivionAiTest, FollowersRunToRecoverPackageSpacing)
    {
        for (const auto type : { AIPackageType::Follow, AIPackageType::Accompany })
        {
            EXPECT_FALSE(MWMechanics::oblivionPackageShouldRun(type, {}, 300.f, 300.f * 300.f, false));
            EXPECT_FALSE(MWMechanics::oblivionPackageShouldRun(type, {}, 300.f, 396.f * 396.f, false));
            EXPECT_TRUE(MWMechanics::oblivionPackageShouldRun(type, {}, 300.f, 397.f * 397.f, false));
            EXPECT_TRUE(MWMechanics::oblivionPackageShouldRun(type, {}, 300.f, 0.f, true));
            EXPECT_FALSE(MWMechanics::oblivionPackageShouldRun(type, {}, 300.f, std::nullopt, false));
            EXPECT_FALSE(MWMechanics::oblivionPackageShouldRun(type, {}, 0.f, 224.f * 224.f, false));
            EXPECT_TRUE(MWMechanics::oblivionPackageShouldRun(type, {}, 0.f, 225.f * 225.f, false));
        }
    }

    TEST(OblivionAiTest, CatchUpAndTravelHonorAuthoredGaitFlags)
    {
        const auto run = decodePackageFlags(static_cast<std::uint32_t>(PackageFlag::AlwaysRun));
        const auto sneak = decodePackageFlags(run.mRaw | static_cast<std::uint32_t>(PackageFlag::AlwaysSneak));
        for (const auto type : { AIPackageType::Follow, AIPackageType::Accompany, AIPackageType::Travel,
                 AIPackageType::Wander, AIPackageType::Escort, AIPackageType::FleeNotCombat, AIPackageType::Pursue })
        {
            EXPECT_FALSE(MWMechanics::oblivionPackageShouldRun(type, sneak, 0.f, 1000000.f, true));
            EXPECT_TRUE(MWMechanics::oblivionPackageShouldRun(type, run, 0.f, 0.f, false));
        }
        EXPECT_FALSE(MWMechanics::oblivionPackageShouldRun(AIPackageType::Escort, {}, 0.f, 1000000.f, true));
        EXPECT_FALSE(MWMechanics::oblivionPackageShouldRun(AIPackageType::Wander, {}, 0.f, 1000000.f, true));
        EXPECT_FALSE(MWMechanics::oblivionPackageShouldRun(AIPackageType::Travel, {}, 0.f, std::nullopt, false));
        EXPECT_TRUE(MWMechanics::oblivionPackageShouldRun(AIPackageType::Pursue, {}, 0.f, std::nullopt, false));
        EXPECT_TRUE(MWMechanics::oblivionPackageShouldRun(AIPackageType::FleeNotCombat, {}, 0.f, std::nullopt, false));
    }

    TEST(OblivionAiTest, ProactiveYieldingIsLimitedToGenuinelyIdleNativeActors)
    {
        RuntimeActorAiState state;
        state.mPhase = PackagePhase::Complete;
        EXPECT_TRUE(MWMechanics::oblivionActorCanProactivelyYield(state));

        state.mRestrained = true;
        EXPECT_FALSE(MWMechanics::oblivionActorCanProactivelyYield(state));
        state.mRestrained = false;
        state.mActionReserved = true;
        EXPECT_FALSE(MWMechanics::oblivionActorCanProactivelyYield(state));
        state.mActionReserved = false;

        state.mPhase = PackagePhase::Wait;
        state.mPackage = {};
        EXPECT_TRUE(MWMechanics::oblivionActorCanProactivelyYield(state));
        state.mPackage = key(1);
        EXPECT_FALSE(MWMechanics::oblivionActorCanProactivelyYield(state));
        state.mPhase = PackagePhase::Path;
        state.mPackage = {};
        EXPECT_FALSE(MWMechanics::oblivionActorCanProactivelyYield(state));

        state.mHasDestination = true;
        EXPECT_TRUE(MWMechanics::oblivionActorHasMovingIntent(state));
        state.mRestrained = true;
        EXPECT_FALSE(MWMechanics::oblivionActorHasMovingIntent(state));
        state.mRestrained = false;
        state.mPhase = PackagePhase::Wait;
        EXPECT_FALSE(MWMechanics::oblivionActorHasMovingIntent(state));

        const auto direction = MWMechanics::oblivionProactiveYieldDirection(
            osg::Vec3f(653.f, -21.f, -109.f), osg::Vec3f(594.f, 29.f, -104.f),
            osg::Vec3f(755.f, 80.f, -104.f), osg::Vec3f(762.f, -6.f, -106.f));
        ASSERT_TRUE(direction);
        EXPECT_GT(direction->x(), 0.f);
        EXPECT_LT(direction->y(), -0.6f);
        EXPECT_FALSE(MWMechanics::oblivionProactiveYieldDirection(
            osg::Vec3f(1.f, 2.f, 3.f), osg::Vec3f(1.f, 2.f, 4.f), std::nullopt, std::nullopt));
    }

    TEST(OblivionAiTest, UnflaggedTravelLeaderWalksWhileSeparatedCompanionRuns)
    {
        // Released MS92RoxyAricTelepe and MS92AlonzoFollowRoxyAric flags.
        const auto leader = decodePackageFlags(0x00000002);
        const auto follower = decodePackageFlags(0x00400000);
        EXPECT_FALSE(MWMechanics::oblivionPackageShouldRun(
            AIPackageType::Travel, leader, 0.f, std::nullopt, false));
        EXPECT_TRUE(MWMechanics::oblivionPackageShouldRun(
            AIPackageType::Accompany, follower, 300.f, 800.f * 800.f, false));
        EXPECT_FALSE(MWMechanics::oblivionPackageShouldRun(
            AIPackageType::Accompany, follower, 300.f, 300.f * 300.f, false));
    }

    TEST(OblivionAiTest, MapsEveryNativePackageTypeToItsProcedure)
    {
        constexpr std::array types{
            AIPackageType::Find, AIPackageType::Follow, AIPackageType::Escort, AIPackageType::Eat,
            AIPackageType::Sleep, AIPackageType::Wander, AIPackageType::Travel, AIPackageType::Accompany,
            AIPackageType::UseItemAt, AIPackageType::Ambush, AIPackageType::FleeNotCombat,
            AIPackageType::CastMagic, AIPackageType::Pursue,
        };
        for (const AIPackageType type : types)
            EXPECT_NE(packageProcedure(type), PackageProcedure::None);
        EXPECT_EQ(packageProcedure(AIPackageType::Unknown), PackageProcedure::None);
    }

    TEST(OblivionAiTest, ScriptPackagePrecedesBaseListAndBaseOrderIsStable)
    {
        PackageSelectionRequest request;
        request.mNow = { 433, 6, 26, 12.0 };
        request.mEvaluationGeneration = 17;
        request.mBasePackages = { candidate(1, AIPackageType::Wander, 4),
            candidate(2, AIPackageType::Eat, 9) };
        request.mScriptPackage = candidate(3, AIPackageType::Travel, 0);

        PackageSelection selected = selectPackage(request);
        ASSERT_TRUE(selected.hasPackage());
        EXPECT_EQ(selected.mSource, PackageSource::Script);
        EXPECT_EQ(selected.mPackage, key(3));
        EXPECT_EQ(selected.mType, AIPackageType::Travel);
        EXPECT_EQ(selected.mEvaluationGeneration, 17u);

        request.mScriptPackage.reset();
        selected = selectPackage(request);
        ASSERT_TRUE(selected.hasPackage());
        EXPECT_EQ(selected.mSource, PackageSource::Base);
        EXPECT_EQ(selected.mPackage, key(1));
        EXPECT_EQ(selected.mListIndex, 4u);
    }

    TEST(OblivionAiTest, ScheduleWindowHandlesMidnightAndZeroDuration)
    {
        PackageSchedule schedule;
        schedule.mStartHour = 22;
        schedule.mDuration = 4;

        const auto active = schedule.activeWindow({ 433, 6, 27, 1.5 });
        ASSERT_TRUE(active.has_value());
        EXPECT_EQ(active->mStart, (CalendarInstant{ 433, 6, 26, 22.0 }));
        EXPECT_EQ(active->mEnd, (CalendarInstant{ 433, 6, 27, 2.0 }));

        EXPECT_FALSE(schedule.activeWindow({ 433, 6, 27, 2.0 }).has_value());

        schedule.mStartHour = -1;
        schedule.mDuration = 0;
        const auto allDay = schedule.activeWindow({ 433, 6, 27, 23.5 });
        ASSERT_TRUE(allDay.has_value());
        EXPECT_DOUBLE_EQ(allDay->mDurationHours, 24.0);
        EXPECT_EQ(allDay->mStart, (CalendarInstant{ 433, 6, 27, 23.5 }));
    }

    TEST(OblivionAiTest, DoorTransitionPreservesRouteAndIncrementsGeneration)
    {
        PackageSelection selection;
        selection.mSource = PackageSource::Base;
        selection.mPackage = key(10);
        selection.mType = AIPackageType::Travel;
        selection.mListIndex = 2;
        selection.mEvaluationGeneration = 4;
        selection.mWindow = ScheduleWindow{ { 433, 6, 26, 0.0 }, { 433, 6, 27, 0.0 }, 24.0 };

        PackagePhaseState state = beginPackagePhase(selection, key(100), key(101));
        EXPECT_EQ(state.mPhase, PackagePhase::Select);
        EXPECT_EQ(advancePackagePhase(state, { 0.f, true, true, false, false, true, false, false, false, false,
                              false, true })
                      .mTo,
            PackagePhase::Resolve);
        EXPECT_EQ(advancePackagePhase(state, { 0.f, true, true, false, false, true, false, false, false, false,
                              false, true })
                      .mTo,
            PackagePhase::Path);

        const PackagePhaseTransition reachedDoor
            = advancePackagePhase(state, { 0.f, true, true, true, true, false, false, false, false, false, false,
                true });
        EXPECT_EQ(reachedDoor.mTo, PackagePhase::Door);

        const PackagePhaseTransition crossed
            = advancePackagePhase(state, { 0.f, true, true, true, true, true, false, false, false, false, false,
                true, true });
        EXPECT_EQ(crossed.mTo, PackagePhase::Path);
        EXPECT_EQ(state.mTransitionGeneration, 1u);

        const PackagePhaseTransition arrived
            = advancePackagePhase(state, { 0.f, true, true, true, false, true, false, false, false, false, false,
                true });
        EXPECT_EQ(arrived.mTo, PackagePhase::Arrive);
        PackagePhaseInput actionFinished;
        actionFinished.mResolved = true;
        actionFinished.mRouteAvailable = true;
        actionFinished.mActionCompleted = true;
        EXPECT_EQ(advancePackagePhase(state, actionFinished).mTo, PackagePhase::Act);
        EXPECT_EQ(advancePackagePhase(state, actionFinished).mTo, PackagePhase::Complete);
    }

    TEST(OblivionAiTest, RepathBudgetEndsInTypedStalledBoundary)
    {
        PackageSelection selection;
        selection.mSource = PackageSource::Base;
        selection.mPackage = key(20);
        selection.mType = AIPackageType::Travel;
        PackagePhaseState state = beginPackagePhase(selection, key(200), key(201));
        static_cast<void>(advancePackagePhase(state, { 0.f, true, true, false, false, true, false, false, false,
            false, false, true }));
        static_cast<void>(advancePackagePhase(state, { 0.f, true, true, false, false, true, false, false, false,
            false, false, true }));
        ASSERT_EQ(state.mPhase, PackagePhase::Path);

        for (int attempt = 0; attempt < 8; ++attempt)
            EXPECT_EQ(advancePackagePhase(state, { 0.25f, true, false, false, false, false, false, false, false,
                                  false, false, false })
                          .mTo,
                PackagePhase::Path);
        const PackagePhaseTransition stalled
            = advancePackagePhase(state, { 0.25f, true, false, false, false, false, false, false, false, false,
                false, false });
        EXPECT_EQ(stalled.mTo, PackagePhase::Stalled);
        EXPECT_EQ(stalled.mBoundary, PhaseBoundary::None);
        EXPECT_EQ(state.mInterruptionReason, "bounded-repath-exhausted");
    }

    TEST(OblivionAiTest, AvailableRouteResetsTheConsecutiveRepathBudget)
    {
        PackageSelection selection;
        selection.mSource = PackageSource::Base;
        selection.mPackage = key(20);
        selection.mType = AIPackageType::Follow;
        PackagePhaseState state = beginPackagePhase(selection, key(200), key(201));
        state.mPhase = PackagePhase::Path;

        for (int attempt = 0; attempt < 8; ++attempt)
            EXPECT_EQ(advancePackagePhase(state, { 0.25f, true, false }).mTo,
                PackagePhase::Path);
        ASSERT_EQ(state.mRepathAttempts, 8u);
        EXPECT_EQ(advancePackagePhase(state, { 0.25f, true, true }).mTo,
            PackagePhase::Path);
        EXPECT_EQ(state.mRepathAttempts, 0u);
        EXPECT_TRUE(state.mInterruptionReason.empty());
    }

    TEST(OblivionAiTest, PermanentLowProcessRouteFailureDoesNotRetry)
    {
        PackageSelection selection;
        selection.mSource = PackageSource::Base;
        selection.mPackage = key(20);
        selection.mType = AIPackageType::Travel;
        PackagePhaseState state = beginPackagePhase(selection, key(200), key(201));
        state.mPhase = PackagePhase::Path;

        PackagePhaseInput input;
        input.mResolved = true;
        input.mPermanentRouteFailure = true;
        const PackagePhaseTransition transition = advancePackagePhase(state, input);
        EXPECT_EQ(transition.mTo, PackagePhase::Stalled);
        EXPECT_EQ(transition.mReason, "route-unavailable");
        EXPECT_EQ(state.mInterruptionReason, "route-unavailable");
        EXPECT_EQ(state.mRepathAttempts, 0u);
    }
}

TEST(OblivionAiTest, PreparedPackageQueueReplacementInvalidatesCallbackBatchAndKeepsDispatchGuard)
{
    MWMechanics::OblivionPackageDoneQueue queue;
    queue.record(ESM4::PackagePhase::Act, ESM4::PackagePhase::Complete, key(1), key(10));
    queue.record(ESM4::PackagePhase::Act, ESM4::PackagePhase::Complete, key(2), key(20));
    std::deque<ESM4::RuntimePackageDoneEvent> prepared{{key(3), key(30)}, {key(4), key(40)}};
    int calls = 0;
    queue.dispatch([&](const auto& event) {
        ++calls;
        EXPECT_EQ(event.mActor, key(1));
        queue.installPrepared(prepared);
        queue.dispatch([](const auto&) { ADD_FAILURE() << "recursive dispatch crossed restored queue epoch"; });
    });
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(queue.capture(), (std::vector<ESM4::RuntimePackageDoneEvent>{{key(3), key(30)}, {key(4), key(40)}}));
    std::vector<ESM4::RuntimePackageDoneEvent> delivered;
    queue.dispatch([&](const auto& event) { delivered.push_back(event); });
    EXPECT_EQ(delivered, (std::vector<ESM4::RuntimePackageDoneEvent>{{key(3), key(30)}, {key(4), key(40)}}));
    EXPECT_TRUE(queue.capture().empty());
}

TEST(OblivionAiTest, EmptyPackageQueueRestoreRetiresCallbackBatchAndAllowsFreshEvents)
{
    MWMechanics::OblivionPackageDoneQueue queue;
    queue.record(ESM4::PackagePhase::Act, ESM4::PackagePhase::Complete, key(1), key(10));
    queue.record(ESM4::PackagePhase::Act, ESM4::PackagePhase::Complete, key(2), key(20));
    int calls = 0;
    queue.dispatch([&](const auto& event) {
        ++calls;
        EXPECT_EQ(event.mActor, key(1));
        queue.restore({});
        queue.record(ESM4::PackagePhase::Act, ESM4::PackagePhase::Complete, key(3), key(30));
        queue.dispatch([](const auto&) { ADD_FAILURE() << "recursive dispatch crossed cleared queue epoch"; });
    });
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(queue.capture(), (std::vector<ESM4::RuntimePackageDoneEvent>{{key(3), key(30)}}));
    queue.dispatch([&](const auto& event) {
        ++calls;
        EXPECT_EQ(event.mActor, key(3));
    });
    EXPECT_EQ(calls, 2);
    EXPECT_TRUE(queue.capture().empty());
}
