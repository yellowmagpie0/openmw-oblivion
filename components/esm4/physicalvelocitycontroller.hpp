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
        friend bool operator==(const PhysicalVelocityControllerState&, const PhysicalVelocityControllerState&) = default;
    };

    // Original8B8590 setup after the caller admits a nonnegative Down duration.
    // A missing prior controller uses native constructor clock/delta/flags.
    // Body mass is the reciprocal read from current or archived native motion;
    // linear damping belongs to the current motion. Target/list ownership is
    // deliberately caller-owned and is not retargeted by reuse.
    PhysicalVelocityControllerState preparePhysicalVelocityController(
        const std::optional<PhysicalVelocityControllerState>& previous, const std::array<float, 4>& sourceVector,
        float duration, bool hasPhysicalBody, float inverseMass, float linearDamping);

    // Original8B8410 HIT setup. Always replace vector/timing and restart,
    // retaining elapsed/delta on reuse. Duration is the compiled0.2f. The
    // caller resolves B2F0F8 independently; mass scaling precedes damping.
    // Absent physical bodies copy all four source lanes and ignore coefficients.
    // Target identity and existing attachment/list position stay caller-owned.
    PhysicalVelocityControllerState preparePhysicalHitVelocityController(
        const std::optional<PhysicalVelocityControllerState>& previous, const std::array<float, 4>& sourceVector,
        bool hasPhysicalBody, float inverseMass, float linearDamping, float resolvedMassMultiplier);

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
