#include "physicalvelocitycontroller.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    PhysicalVelocityControllerState preparePhysicalVelocityController(
        const std::optional<PhysicalVelocityControllerState>& previous, const std::array<float, 4>& sourceVector,
        float duration, bool hasPhysicalBody, float inverseMass, float linearDamping)
    {
        const auto finite = [](float value) {
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native velocity controller setup input/result");
        };
        finite(duration);
        if (duration < 0.f)
            throw std::invalid_argument("negative native velocity controller Down duration");
        auto next = previous.value_or(PhysicalVelocityControllerState{});
        if (!previous)
        {
            next.mTiming.mFlags = 0xcu; // Native NiTimeController constructor.
            next.mFrameDelta = 0.f; // Native velocity-controller constructor.
        }
        float mass = 0.f;
        float dampingWeight = 0.f;
        if (hasPhysicalBody)
        {
            finite(inverseMass);
            finite(linearDamping);
            if (inverseMass < 0.f || linearDamping < 0.f)
                throw std::invalid_argument("negative native velocity controller mass/damping coefficient");
            // Original535AC0 stores the89DA90 mass getter as binary32. On a
            // keyframed motion this reciprocal belongs to its archived motion.
            mass = inverseMass == 0.f ? 0.f : float(1.0 / double(inverseMass));
            dampingWeight = float(double(linearDamping) * 0.75);
            finite(mass);
            finite(dampingWeight);
        }
        for (std::size_t i = 0; i < sourceVector.size(); ++i)
        {
            finite(sourceVector[i]);
            if (hasPhysicalBody)
            {
                // Preserve the two SSE product stores before their addition.
                const float massProduct = float(double(mass) * double(sourceVector[i]));
                const float dampingProduct = float(double(dampingWeight) * double(sourceVector[i]));
                finite(massProduct);
                finite(dampingProduct);
                next.mForceVector[i] = float(double(massProduct) + double(dampingProduct));
                finite(next.mForceVector[i]);
            }
            else
                next.mForceVector[i] = sourceVector[i];
        }
        next.mTiming = {static_cast<std::uint16_t>((next.mTiming.mFlags & 0xfff5u) | 0xdu),
            1.f, 0.f, 0.f, duration};
        // Start(-FLT_MAX) resets these sentinels and retains elapsed/delta on
        // reuse. Target identity and list insertion belong to the owner.
        next.mClock.mStartTime = -std::numeric_limits<float>::max();
        next.mClock.mPreviousTime = -std::numeric_limits<float>::max();
        return next;
    }

    PhysicalVelocityControllerState preparePhysicalHitVelocityController(
        const std::optional<PhysicalVelocityControllerState>& previous, const std::array<float, 4>& sourceVector,
        bool hasPhysicalBody, float inverseMass, float linearDamping, float resolvedMassMultiplier)
    {
        // Constructor/timing/Start agree with Down, but HIT always uses its
        // compiled interval and applies damping to the mass-scaled vector.
        auto next = preparePhysicalVelocityController(previous, sourceVector, .2f, false, 0.f, 0.f);
        if (!hasPhysicalBody)
            return next;
        const auto finite = [](float value) {
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native HIT velocity setup input/result");
        };
        finite(inverseMass);
        finite(linearDamping);
        finite(resolvedMassMultiplier);
        if (inverseMass < 0.f || linearDamping < 0.f)
            throw std::invalid_argument("negative native HIT velocity mass/damping coefficient");
        const float mass = inverseMass == 0.f ? 0.f : float(1.0 / double(inverseMass));
        finite(mass);
        const float scaledMass = float(double(mass) * double(resolvedMassMultiplier));
        const float dampingWeight = float(double(linearDamping) * .75);
        finite(scaledMass);
        finite(dampingWeight);
        for (std::size_t axis = 0; axis < sourceVector.size(); ++axis)
        {
            const float massProduct = float(double(scaledMass) * double(sourceVector[axis]));
            const float dampingProduct = float(double(dampingWeight) * double(massProduct));
            finite(massProduct);
            finite(dampingProduct);
            next.mForceVector[axis] = float(double(dampingProduct) + double(massProduct));
            finite(next.mForceVector[axis]);
        }
        return next;
    }

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
