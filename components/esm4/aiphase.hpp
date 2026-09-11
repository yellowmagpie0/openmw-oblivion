/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#ifndef OPENMW_COMPONENTS_ESM4_AIPHASE_H
#define OPENMW_COMPONENTS_ESM4_AIPHASE_H

#include <cstdint>
#include <string>

#include <components/esm/formkey.hpp>

#include "aipackagedata.hpp"
#include "aiselection.hpp"

namespace ESM4
{
    enum class PackageProcedure : std::uint16_t
    {
        None = 0,
        Find,
        Follow,
        Escort,
        Eat,
        Sleep,
        Wander,
        Travel,
        Accompany,
        UseItemAt,
        Ambush,
        FleeNotCombat,
        CastMagic,
        Pursue,
    };

    enum class PackagePhase : std::uint8_t
    {
        Select,
        Resolve,
        Path,
        Door,
        Arrive,
        Act,
        Wait,
        Complete,
        Interrupted,
        ReadyForDialogue,
        WaitingForM16Action,
        ReadyForM15Combat,
        Stalled,
    };

    enum class PhaseBoundary : std::uint8_t
    {
        None,
        WaitingForM16Action,
        ReadyForM15Combat,
        ReadyForDialogue,
    };

    enum class ProcessTier : std::uint8_t
    {
        High,
        Low,
    };

    struct PackagePhaseState
    {
        ESM::FormKey mActor;
        ESM::FormKey mBase;
        ESM::FormKey mPackage;
        ESM::FormKey mTarget;
        ESM::FormKey mCell;
        ESM::FormKey mPathgrid;
        ESM::FormKey mDoor;
        PackageSource mSource = PackageSource::None;
        AIPackageType mPackageType = AIPackageType::Unknown;
        PackageProcedure mProcedure = PackageProcedure::None;
        PackagePhase mPhase = PackagePhase::Select;
        ProcessTier mTier = ProcessTier::High;
        PhaseBoundary mBoundary = PhaseBoundary::None;
        std::uint32_t mListIndex = 0;
        std::uint64_t mSelectionGeneration = 0;
        std::uint64_t mRouteGeneration = 0;
        std::uint64_t mTransitionGeneration = 0;
        std::uint32_t mPathNode = 0;
        std::uint32_t mRepathAttempts = 0;
        float mActionTimer = 0.0f;
        float mDurationRemaining = 0.0f;
        float mNoProgressSeconds = 0.0f;
        bool mRestrained = false;
        bool mActionReserved = false;
        std::string mInterruptionReason;
    };

    struct PackagePhaseInput
    {
        float mElapsedSeconds = 0.0f;
        bool mResolved = false;
        bool mRouteAvailable = false;
        bool mDestinationReached = false;
        bool mDoorRequired = false;
        bool mDoorAvailable = false;
        bool mDoorLocked = false;
        bool mActionPending = false;
        bool mActionCompleted = false;
        bool mM16ActionRequired = false;
        bool mInterrupted = false;
        bool mMadeProgress = true;
        // A successful door action can leave more route nodes in the
        // destination cell. Keep the route in Path instead of treating the
        // first foreign edge as the final arrival.
        bool mContinueAfterDoor = false;
        // Low process uses a complete, stable pathgrid/door index. A missing
        // route there is structural until the graph changes, so retrying it
        // every fixed step cannot produce a different result.
        bool mPermanentRouteFailure = false;
    };

    struct PackagePhaseTransition
    {
        PackagePhase mFrom;
        PackagePhase mTo;
        PhaseBoundary mBoundary = PhaseBoundary::None;
        std::string mReason;
    };

    PackageProcedure packageProcedure(AIPackageType type);
    PackagePhaseState beginPackagePhase(const PackageSelection& selection, ESM::FormKey actor,
        ESM::FormKey base);
    PackagePhaseTransition advancePackagePhase(PackagePhaseState& state, const PackagePhaseInput& input);
}

#endif
