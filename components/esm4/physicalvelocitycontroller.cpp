#include "physicalvelocitycontroller.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    PhysicalVelocityControllerUpdate advancePhysicalVelocityController(
        const PhysicalVelocityControllerState& controller, const PhysicalBlendTimeCache& timeCache,
        bool hasTarget, std::optional<float> hierarchyGain, bool hasPhysicalBody, float inputTime)
    {
        PhysicalVelocityControllerUpdate result{controller, timeCache, std::nullopt};
        auto& next = result.mController;
        next.mFrameDelta = 0.016f; // Original A96CFC, even on early exits.
        if (!(next.mTiming.mFlags & 8) || !hasTarget)
            return result;
        const auto finite = [](float value) {
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native velocity controller input/result");
        };
        finite(inputTime);
        finite(next.mClock.mPreviousTime);
        if (next.mClock.mPreviousTime != -std::numeric_limits<float>::max())
        {
            const float delta = float(double(inputTime) - double(next.mClock.mPreviousTime));
            finite(delta);
            if (delta >= 0.f)
                next.mFrameDelta = delta;
        }
        const float keyTime = advancePhysicalBlendClock(next.mClock, result.mTimeCache, next.mTiming, inputTime);
        if (hierarchyGain)
        {
            finite(*hierarchyGain);
            if (*hierarchyGain < 1.f && hasPhysicalBody)
            {
                std::array<float, 4> force;
                for (std::size_t i = 0; i < force.size(); ++i)
                {
                    finite(next.mForceVector[i]);
                    force[i] = next.mForceVector[i] * 100.f; // Original SSE8B83EA.
                    finite(force[i]);
                }
                result.mForce = force;
            }
        }
        // Velocity completion clears active directly. It does not call Stop
        // or depend on the cycle mode, unlike the blend-controller finish.
        if (keyTime == next.mTiming.mStopKey)
            next.mTiming.mFlags &= 0xfff7u;
        return result;
    }
}
