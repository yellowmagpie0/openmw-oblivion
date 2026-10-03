#ifndef OPENMW_ESM4_PHYSICALVELOCITYCONTROLLER_H
#define OPENMW_ESM4_PHYSICALVELOCITYCONTROLLER_H

#include "physicalcombat.hpp"

#include <array>
#include <optional>

namespace ESM4
{
    struct PhysicalVelocityControllerState
    {
        PhysicalBlendTiming mTiming{0, 1.f, 0.f, 0.f, 0.f};
        PhysicalBlendClock mClock;
        std::array<float, 4> mForceVector{};
        float mFrameDelta = 0.016f;
    };

    struct PhysicalVelocityControllerUpdate
    {
        PhysicalVelocityControllerState mController;
        PhysicalBlendTimeCache mTimeCache;
        // Original5377B0 arguments, in native force units. Caller performs
        // actual body activation/force application with mFrameDelta.
        std::optional<std::array<float, 4>> mForce;
    };

    // Original8B8770 plus8B8380. Target/blend/body identity and stored force
    // vector are caller-resolved. Stage clocks and optional force intent;
    // inactive/missing-target paths still reset delta to native default. This
    // does not create/attach/remove controllers or apply physical forces.
    PhysicalVelocityControllerUpdate advancePhysicalVelocityController(
        const PhysicalVelocityControllerState& controller, const PhysicalBlendTimeCache& timeCache,
        bool hasTarget, std::optional<float> hierarchyGain, bool hasPhysicalBody, float inputTime);
}

#endif
