#include "physicalframe.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ESM4
{
    PhysicalFrameResult preparePhysicalFrame(PhysicalFrameClock& clock,
        const PhysicalFrameSettings& settings, float delta, std::uint32_t rawMode, bool limitSubsteps)
    {
        const auto nonnegative = [](float value) { return std::isfinite(value) && value >= 0.f; };
        const auto positive = [](float value) { return std::isfinite(value) && value > 0.f; };
        if (!nonnegative(delta) || !nonnegative(clock.mRemaining) || !nonnegative(clock.mSmoothed)
            || !positive(settings.mMaximumFrame) || !positive(settings.mMinimumFrame)
            || !positive(settings.mFixedSubstep) || !std::isfinite(settings.mSmoothing)
            || double(settings.mMaximumFrame) / settings.mFixedSubstep >= 0x1p32)
            throw std::invalid_argument("Invalid or unsupported native physical frame input");
        auto updated = clock;
        // Original stores the accumulated sum as binary32 before capping it.
        // A finite-input sum can overflow that store; the cap still applies.
        const float accumulated = delta + clock.mRemaining;
        const float frame = std::min(accumulated, settings.mMaximumFrame);
        PhysicalFrameResult result{delta, frame, 0.f, 0};
        if (frame == 0.f)
            return result;
        const std::uint32_t maximumSteps = limitSubsteps ? 2 : 3;
        if (rawMode == 10)
        {
            // No intermediate float store in the original x87 smoothing path.
            updated.mSmoothed = static_cast<float>(double(clock.mSmoothed)
                + (double(frame) - clock.mSmoothed) * settings.mSmoothing);
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
                const auto count = static_cast<std::uint32_t>(double(frame) / settings.mFixedSubstep);
                result.mSubstepCount = std::min(count, maximumSteps);
                if (result.mSubstepCount == 0)
                    result.mSubstepCount = 1;
                result.mSubstepSeconds = static_cast<float>(double(frame) / result.mSubstepCount);
                updated.mRemaining = 0.f;
            }
        }
        else
        {
            const auto count = static_cast<std::uint32_t>(double(frame) / settings.mFixedSubstep);
            result.mSubstepCount = std::min(count, maximumSteps);
            if (result.mSubstepCount != 0)
            {
                result.mSubstepSeconds = settings.mFixedSubstep;
                // Preserve the remainder store before subtracting it again.
                updated.mRemaining = static_cast<float>(double(frame)
                    - double(result.mSubstepCount) * settings.mFixedSubstep);
                result.mFrameSeconds = static_cast<float>(double(frame) - updated.mRemaining);
                updated.mRemaining = std::min(updated.mRemaining, settings.mFixedSubstep);
            }
            else if (double(frame) >= double(settings.mFixedSubstep) * .5)
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
        if (!nonnegative(updated.mRemaining) || !nonnegative(updated.mSmoothed)
            || !nonnegative(result.mFrameSeconds) || !nonnegative(result.mSubstepSeconds))
            throw std::invalid_argument("Nonrepresentable native physical frame output");
        clock = updated;
        return result;
    }
}
