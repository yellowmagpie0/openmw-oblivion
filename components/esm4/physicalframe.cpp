#include "physicalframe.hpp"

#include <components/nif/nativetransformmath.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace ESM4
{
    float advancePhysicalTimer(PhysicalTimerState& timer, PhysicalTimerCounters& counters,
        std::uint32_t tick, float scale, Nif::NativeTransformPrecision precision)
    {
        const Nif::Detail::NativeTransformMath math(precision);
        if (!math.valid()) throw std::invalid_argument("invalid native timer precision");
        auto updated = timer;
        const std::uint32_t elapsed = tick - timer.mOriginTick;
        const PhysicalTimerCounters updatedCounters{elapsed, counters.mElapsed};
        updated.mDeltaSeconds = 0.f;
        if (timer.mPauseCount == 0)
        {
            if (!std::isfinite(scale) || scale < 0.f || !std::isfinite(timer.mOverrideMilliseconds)
                || timer.mOverrideMilliseconds < 0.f)
                throw std::invalid_argument("invalid native timer input");
            double milliseconds;
            if (timer.mOverrideMilliseconds != 0.f)
            {
                const float sum = static_cast<float>(math.add(
                    timer.mFractionalMilliseconds, timer.mOverrideMilliseconds));
                if (!std::isfinite(sum) || double(sum) < -0x1p63 || double(sum) >= 0x1p63)
                    throw std::invalid_argument("unsupported native timer truncation");
                const auto truncated = static_cast<std::int64_t>(sum);
                const auto low = static_cast<std::uint32_t>(static_cast<std::uint64_t>(truncated));
                // Original reloads the low word as signed, then adds2^32 under
                // the selected x87 precision before subtracting it from sum.
                const double unsignedLow = low <= 0x7fffffffu ? double(low)
                    : math.add(std::bit_cast<std::int32_t>(low), 0x1p32);
                updated.mFractionalMilliseconds = static_cast<float>(math.add(sum, -unsignedLow));
                updated.mLastElapsed += low;
                milliseconds = timer.mOverrideMilliseconds;
            }
            else
            {
                auto interval = elapsed - timer.mLastElapsed;
                if (interval == 0) interval = 1;
                else if (interval > 166) interval = 166;
                milliseconds = interval;
                updated.mLastElapsed = elapsed;
            }
            // The original double constant contains the promoted binary32
            // value0.001f. Preserve both explicit binary32 stores around scale.
            updated.mDeltaSeconds = static_cast<float>(math.multiply(milliseconds, double(.001f)));
            updated.mDeltaSeconds = static_cast<float>(math.multiply(updated.mDeltaSeconds, scale));
            if (!std::isfinite(updated.mDeltaSeconds))
                throw std::invalid_argument("nonrepresentable native timer output");
        }
        timer = updated;
        counters = updatedCounters;
        return timer.mDeltaSeconds;
    }

    PhysicalFrameResult preparePhysicalFrame(PhysicalFrameClock& clock,
        const PhysicalFrameSettings& settings, float delta, std::uint32_t rawMode, bool limitSubsteps)
    {
        return preparePhysicalFrame(clock, settings, delta, rawMode, limitSubsteps,
            Nif::NativeTransformPrecision::Double);
    }

    PhysicalFrameResult preparePhysicalFrame(PhysicalFrameClock& clock,
        const PhysicalFrameSettings& settings, float delta, std::uint32_t rawMode, bool limitSubsteps,
        Nif::NativeTransformPrecision precision)
    {
        const Nif::Detail::NativeTransformMath math(precision);
        const auto nonnegative = [](float value) { return std::isfinite(value) && value >= 0.f; };
        const auto positive = [](float value) { return std::isfinite(value) && value > 0.f; };
        if (!math.valid() || !nonnegative(delta) || !std::isfinite(clock.mRemaining) || !nonnegative(clock.mSmoothed)
            || !positive(settings.mMaximumFrame) || !positive(settings.mMinimumFrame)
            || !positive(settings.mFixedSubstep) || !std::isfinite(settings.mSmoothing)
            || math.divide(settings.mMaximumFrame, settings.mFixedSubstep) >= 0x1p32)
            throw std::invalid_argument("Invalid or unsupported native physical frame input");
        auto updated = clock;
        // Original stores the accumulated sum as binary32 before capping it.
        // A finite-input sum can overflow that store; the cap still applies.
        const float accumulated = static_cast<float>(math.add(delta, clock.mRemaining));
        const float frame = std::min(accumulated, settings.mMaximumFrame);
        PhysicalFrameResult result{delta, frame, 0.f, 0};
        if (frame <= 0.f)
            return result;
        const std::uint32_t maximumSteps = limitSubsteps ? 2 : 3;
        if (rawMode == 10)
        {
            // No intermediate float store in the original x87 smoothing path.
            updated.mSmoothed = static_cast<float>(math.add(clock.mSmoothed,
                math.multiply(math.add(frame, -double(clock.mSmoothed)), settings.mSmoothing)));
            result.mSubstepSeconds = updated.mSmoothed;
            result.mSubstepCount = 1;
            updated.mRemaining = 0.f;
        }
        else if (rawMode != 0)
        {
            if (frame < settings.mMinimumFrame)
            {
                updated.mRemaining = frame;
                result.mFrameSeconds = 0.f;
            }
            else
            {
                const auto count = static_cast<std::uint32_t>(math.divide(frame, settings.mFixedSubstep));
                result.mSubstepCount = std::min(count, maximumSteps);
                if (result.mSubstepCount == 0)
                    result.mSubstepCount = 1;
                result.mSubstepSeconds = static_cast<float>(math.divide(frame, result.mSubstepCount));
                updated.mRemaining = 0.f;
            }
        }
        else
        {
            const auto count = static_cast<std::uint32_t>(math.divide(frame, settings.mFixedSubstep));
            result.mSubstepCount = std::min(count, maximumSteps);
            if (result.mSubstepCount != 0)
            {
                result.mSubstepSeconds = settings.mFixedSubstep;
                // Preserve the remainder store before subtracting it again.
                updated.mRemaining = static_cast<float>(math.add(frame,
                    -math.multiply(result.mSubstepCount, settings.mFixedSubstep)));
                result.mFrameSeconds = static_cast<float>(math.add(frame, -double(updated.mRemaining)));
                updated.mRemaining = std::min(updated.mRemaining, settings.mFixedSubstep);
            }
            else if (double(frame) >= math.multiply(settings.mFixedSubstep, .5))
            {
                result.mSubstepCount = 1;
                result.mSubstepSeconds = frame;
                updated.mRemaining = 0.f;
            }
            else
            {
                updated.mRemaining = frame;
                result.mFrameSeconds = 0.f;
            }
        }
        if (!std::isfinite(updated.mRemaining) || !nonnegative(updated.mSmoothed)
            || !nonnegative(result.mFrameSeconds) || !nonnegative(result.mSubstepSeconds))
            throw std::invalid_argument("Nonrepresentable native physical frame output");
        clock = updated;
        return result;
    }
}
