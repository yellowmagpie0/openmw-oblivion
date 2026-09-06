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

#include "apps/openmw/mwmechanics/oblivionaidestination.hpp"
#include "apps/openmw/mwmechanics/oblivionaigait.hpp"

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

    TEST(OblivionAiTest, CatchUpHonorsSneakAndRunFlagsWithoutChangingOtherPackageGaits)
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
        EXPECT_TRUE(MWMechanics::oblivionPackageShouldRun(AIPackageType::Travel, {}, 0.f, std::nullopt, false));
        EXPECT_TRUE(MWMechanics::oblivionPackageShouldRun(AIPackageType::Pursue, {}, 0.f, std::nullopt, false));
        EXPECT_TRUE(MWMechanics::oblivionPackageShouldRun(AIPackageType::FleeNotCombat, {}, 0.f, std::nullopt, false));
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
}
