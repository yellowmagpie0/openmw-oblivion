#ifndef OPENMW_COMPONENTS_ESM4_PHYSICALFRAME_HPP
#define OPENMW_COMPONENTS_ESM4_PHYSICALFRAME_HPP

#include <cstdint>

namespace Nif { enum class NativeTransformPrecision; }

namespace ESM4
{
    // Original47D150/47D170 timer state. Tick arithmetic wraps at32 bits;
    // the pause byte is supplied by the timer's separate lifecycle owner.
    struct PhysicalTimerState
    {
        std::uint8_t mPauseCount = 0;
        float mOverrideMilliseconds = 0.f;
        float mFractionalMilliseconds = 0.f;
        float mDeltaSeconds = 0.f;
        std::uint32_t mLastElapsed = 0;
        std::uint32_t mOriginTick;
        explicit PhysicalTimerState(std::uint32_t originTick = 0) : mOriginTick(originTick) {}
        friend bool operator==(const PhysicalTimerState&, const PhysicalTimerState&) = default;
    };

    // Full original47D120/47D0F0 nested transitions. Only the outer
    // transition rebases tick fields; preserve raw uint8/uint32 wrapping.
    // Host time sampling and balanced pause-source ownership are external.
    void pausePhysicalTimer(PhysicalTimerState& timer, std::uint32_t tick) noexcept;
    void resumePhysicalTimer(PhysicalTimerState& timer, std::uint32_t tick) noexcept;

    // Original shared counters rotate even when this timer is paused. These
    // are not the capped/overridden interval or the physical frame remainder.
    struct PhysicalTimerCounters
    {
        std::uint32_t mElapsed = 0, mPreviousElapsed = 0;
        friend bool operator==(const PhysicalTimerCounters&, const PhysicalTimerCounters&) = default;
    };

    // Full timer update, with host tick/configuration supplied by the caller.
    // Stage timer/counters atomically. Unpaused override/scale must be finite
    // and nonnegative; consumed fractional sums must fit signed64 truncation.
    // Pause entry/resume methods and process clock ownership are separate.
    float advancePhysicalTimer(PhysicalTimerState& timer, PhysicalTimerCounters& counters,
        std::uint32_t tick, float scale, Nif::NativeTransformPrecision precision);

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
    // Stage clock changes atomically. Finite nonnegative input/smoothing, signed
    // finite remainder, positive durations and a maximum/fixed-substep quotient
    // below2^32 are supported.
    // This neither advances bodies nor resolves the engine's winning settings.
    PhysicalFrameResult preparePhysicalFrame(PhysicalFrameClock& clock,
        const PhysicalFrameSettings& settings, float delta, std::uint32_t rawMode, bool limitSubsteps);
    // The original API retains Double intermediates. Select Single explicitly
    // for the observed live main caller's007F arithmetic. Rounded quotients can
    // generate negative carry; a nonpositive accumulated frame takes no steps
    // and retains the clock, including that signed carry.
    PhysicalFrameResult preparePhysicalFrame(PhysicalFrameClock& clock,
        const PhysicalFrameSettings& settings, float delta, std::uint32_t rawMode, bool limitSubsteps,
        Nif::NativeTransformPrecision precision);
}

#endif
