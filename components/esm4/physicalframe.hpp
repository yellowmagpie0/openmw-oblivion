#ifndef OPENMW_COMPONENTS_ESM4_PHYSICALFRAME_HPP
#define OPENMW_COMPONENTS_ESM4_PHYSICALFRAME_HPP

#include <cstdint>

namespace ESM4
{
    // Raw original image defaults; winning runtime configuration is separate.
    struct PhysicalFrameSettings
    {
        float mMaximumFrame = 166.6666717529297f;
        float mMinimumFrame = .008333333767950535f;
        float mFixedSubstep = .01666666753590107f;
        float mSmoothing = .05000000074505806f;
    };

    struct PhysicalFrameClock
    {
        float mRemaining = 0.f;
        float mSmoothed = .03200000151991844f;
        friend bool operator==(const PhysicalFrameClock&, const PhysicalFrameClock&) = default;
    };

    struct PhysicalFrameResult
    {
        float mInputDelta;
        float mFrameSeconds;
        float mSubstepSeconds;
        std::uint32_t mSubstepCount;
    };

    // Original889810 frame preparation. Mode0 uses fixed substeps,10 smooths;
    // other raw modes divide the prepared frame. Boolean limits to2 vs3 steps.
    // Stage clock changes atomically. Finite nonnegative input/state, positive
    // durations and a maximum/fixed-substep quotient below2^32 are supported.
    // This neither advances bodies nor resolves the engine's winning settings.
    PhysicalFrameResult preparePhysicalFrame(PhysicalFrameClock& clock,
        const PhysicalFrameSettings& settings, float delta, std::uint32_t rawMode, bool limitSubsteps);
}

#endif
