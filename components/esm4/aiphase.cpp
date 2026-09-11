/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#include "aiphase.hpp"

#include <algorithm>

namespace ESM4
{
    PackageProcedure packageProcedure(AIPackageType type)
    {
        switch (type)
        {
            case AIPackageType::Find:
                return PackageProcedure::Find;
            case AIPackageType::Follow:
                return PackageProcedure::Follow;
            case AIPackageType::Escort:
                return PackageProcedure::Escort;
            case AIPackageType::Eat:
                return PackageProcedure::Eat;
            case AIPackageType::Sleep:
                return PackageProcedure::Sleep;
            case AIPackageType::Wander:
                return PackageProcedure::Wander;
            case AIPackageType::Travel:
                return PackageProcedure::Travel;
            case AIPackageType::Accompany:
                return PackageProcedure::Accompany;
            case AIPackageType::UseItemAt:
                return PackageProcedure::UseItemAt;
            case AIPackageType::Ambush:
                return PackageProcedure::Ambush;
            case AIPackageType::FleeNotCombat:
                return PackageProcedure::FleeNotCombat;
            case AIPackageType::CastMagic:
                return PackageProcedure::CastMagic;
            case AIPackageType::Pursue:
                return PackageProcedure::Pursue;
            case AIPackageType::Unknown:
                return PackageProcedure::None;
        }
        return PackageProcedure::None;
    }

    PackagePhaseState beginPackagePhase(const PackageSelection& selection, ESM::FormKey actor,
        ESM::FormKey base)
    {
        PackagePhaseState result;
        result.mActor = actor;
        result.mBase = base;
        result.mPackage = selection.mPackage;
        result.mSource = selection.mSource;
        result.mPackageType = selection.mType;
        result.mProcedure = packageProcedure(selection.mType);
        result.mPhase = selection.hasPackage() ? PackagePhase::Select : PackagePhase::Complete;
        result.mListIndex = static_cast<std::uint32_t>(selection.mListIndex);
        result.mSelectionGeneration = selection.mEvaluationGeneration;
        if (selection.mWindow)
            result.mDurationRemaining = static_cast<float>(selection.mWindow->mDurationHours);
        return result;
    }

    PackagePhaseTransition advancePackagePhase(PackagePhaseState& state, const PackagePhaseInput& input)
    {
        const PackagePhase from = state.mPhase;
        auto transition = [&state, from](PackagePhase to, std::string reason,
                              PhaseBoundary boundary = PhaseBoundary::None) {
            state.mPhase = to;
            state.mBoundary = boundary;
            return PackagePhaseTransition{ from, to, boundary, std::move(reason) };
        };

        if (input.mInterrupted && state.mPhase != PackagePhase::Complete)
        {
            state.mInterruptionReason = "interrupted";
            state.mActionReserved = false;
            return transition(PackagePhase::Interrupted, "interrupted");
        }
        if (state.mRestrained)
            return { from, from, PhaseBoundary::None, "restrained" };

        const float elapsed = std::max(0.0f, input.mElapsedSeconds);
        if (!input.mMadeProgress && state.mPhase == PackagePhase::Path)
            state.mNoProgressSeconds += elapsed;
        else if (input.mMadeProgress)
            state.mNoProgressSeconds = 0.0f;

        switch (state.mPhase)
        {
            case PackagePhase::Select:
                return transition(PackagePhase::Resolve, "package-selected");
            case PackagePhase::Resolve:
                if (!input.mResolved)
                {
                    state.mInterruptionReason = "target-unresolved";
                    return transition(PackagePhase::Interrupted, "target-unresolved");
                }
                return transition(PackagePhase::Path, "intent-resolved");
            case PackagePhase::Path:
                if (input.mPermanentRouteFailure)
                {
                    state.mInterruptionReason = "route-unavailable";
                    return transition(PackagePhase::Stalled, "route-unavailable");
                }
                if (state.mNoProgressSeconds > 30.0f
                    || (state.mRepathAttempts >= 8 && !input.mRouteAvailable))
                {
                    state.mInterruptionReason = "bounded-repath-exhausted";
                    return transition(PackagePhase::Stalled, "bounded-repath-exhausted");
                }
                if (!input.mRouteAvailable)
                {
                    ++state.mRepathAttempts;
                    return { from, from, PhaseBoundary::None, "repath-requested" };
                }
                state.mRepathAttempts = 0;
                if (input.mDestinationReached)
                    return transition(input.mDoorRequired ? PackagePhase::Door : PackagePhase::Arrive,
                        input.mDoorRequired ? "door-edge-reached" : "destination-reached");
                return { from, from, PhaseBoundary::None, "following-route" };
            case PackagePhase::Door:
                if (input.mDoorLocked || !input.mDoorAvailable)
                {
                    state.mInterruptionReason = input.mDoorLocked ? "door-locked" : "door-unavailable";
                    return transition(PackagePhase::Interrupted, state.mInterruptionReason);
                }
                ++state.mTransitionGeneration;
                return transition(input.mContinueAfterDoor ? PackagePhase::Path : PackagePhase::Arrive,
                    input.mContinueAfterDoor ? "door-transition-complete-continue-route"
                                              : "door-transition-complete");
            case PackagePhase::Arrive:
                if (state.mPackageType == AIPackageType::Ambush)
                    return transition(PackagePhase::ReadyForM15Combat, "ambush-location-reached",
                        PhaseBoundary::ReadyForM15Combat);
                if (state.mPackageType == AIPackageType::Follow || state.mPackageType == AIPackageType::Escort
                    || state.mPackageType == AIPackageType::Accompany || state.mPackageType == AIPackageType::Wander
                    || state.mPackageType == AIPackageType::FleeNotCombat
                    || state.mPackageType == AIPackageType::Pursue)
                    return transition(PackagePhase::Wait, "locomotion-intent-active");
                return transition(PackagePhase::Act, "arrival-complete");
            case PackagePhase::Act:
                if (state.mPackageType == AIPackageType::CastMagic || input.mM16ActionRequired)
                    return transition(PackagePhase::WaitingForM16Action, "effect-action-deferred-to-m16",
                        PhaseBoundary::WaitingForM16Action);
                if (state.mPackageType == AIPackageType::Follow || state.mPackageType == AIPackageType::Escort
                    || state.mPackageType == AIPackageType::Accompany || state.mPackageType == AIPackageType::Wander
                    || state.mPackageType == AIPackageType::Pursue)
                    return transition(PackagePhase::Wait, "follow-active");
                if (input.mActionPending)
                {
                    state.mActionReserved = true;
                    return transition(PackagePhase::Wait, "action-started");
                }
                if (input.mActionCompleted)
                    return transition(PackagePhase::Complete, "action-complete");
                return { from, from, PhaseBoundary::None, "action-ready" };
            case PackagePhase::Wait:
                state.mActionTimer = std::max(0.0f, state.mActionTimer - elapsed);
                state.mDurationRemaining = std::max(0.0f, state.mDurationRemaining - elapsed / 3600.0f);
                // Low-process Eat/UseItemAt keeps an action reservation as a
                // commit token until a resident InventoryStore can consume
                // the exact stack. Do not silently complete and discard that
                // token merely because the schedule window expired.
                if (input.mActionCompleted
                    || (state.mActionTimer == 0.0f && state.mDurationRemaining == 0.0f
                        && !state.mActionReserved))
                {
                    state.mActionReserved = false;
                    return transition(PackagePhase::Complete, "wait-complete");
                }
                return { from, from, PhaseBoundary::None, "waiting" };
            case PackagePhase::Complete:
                return { from, from, PhaseBoundary::None, "complete" };
            case PackagePhase::Interrupted:
                state.mActionReserved = false;
                return { from, from, PhaseBoundary::None, "interrupted" };
            case PackagePhase::ReadyForDialogue:
            case PackagePhase::WaitingForM16Action:
            case PackagePhase::ReadyForM15Combat:
            case PackagePhase::Stalled:
                return { from, from, state.mBoundary, "boundary" };
        }
        return { from, from, PhaseBoundary::None, "invalid-phase" };
    }
}
