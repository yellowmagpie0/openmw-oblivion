#ifndef OPENMW_COMPONENTS_ESM4_ACTORMOVEMENTSTATE_H
#define OPENMW_COMPONENTS_ESM4_ACTORMOVEMENTSTATE_H

#include <array>
#include <cstdint>
#include <map>
#include <optional>

#include <components/esm/formkey.hpp>

namespace ESM4
{
    // Backend continuation state, not an original Havok controller snapshot.
    // Reference/presentation position is distinct from the two solver endpoints.
    struct RuntimeActorMovement
    {
        ESM::FormKey mBase;
        ESM::FormKey mStandingOn;
        std::array<double, 3> mPosition{}, mPreviousPosition{};
        std::array<float, 3> mSimulationPosition{}, mVelocity{}, mPositionOffset{}, mInertia{}, mLastStuckPosition{};
        std::uint32_t mStuckFrames = 0;
        bool mOnGround = false, mOnSlope = false, mWalkingOnWater = false;
        bool mSkipSimulation = true, mActive = false;
        bool mCollisionMode = true, mCollisionBody = true, mCanWaterWalk = false;
        // v63: mechanics owns initial grounding independently of the capsule.
        // Unknown in older or headless backend-only snapshots.
        std::optional<bool> mPositionAdjusted = std::nullopt;
        void validate() const;
        friend bool operator==(const RuntimeActorMovement&, const RuntimeActorMovement&) = default;
    };

    struct RuntimeMovementState
    {
        bool mDiscardMovements = false;
        std::map<ESM::FormKey, RuntimeActorMovement> mActors;
        friend bool operator==(const RuntimeMovementState&, const RuntimeMovementState&) = default;
    };

}

#endif
