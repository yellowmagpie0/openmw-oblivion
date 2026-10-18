#include "physicalregistration.hpp"

#include <bit>
#include <cmath>
#include <stdexcept>

namespace ESM4
{
    PhysicalBodyRegistrationPlan resolvePhysicalBodyRegistration(PhysicalBodyWorldMembership membership,
        const std::optional<PhysicalBodyBlendRegistration>& blend)
    {
        if (membership != PhysicalBodyWorldMembership::Absent && membership != PhysicalBodyWorldMembership::Desired
            && membership != PhysicalBodyWorldMembership::Other)
            throw std::invalid_argument("Invalid physical body World membership");
        if (blend && (!std::isfinite(blend->mHierarchyGain) || !std::isfinite(blend->mVelocityGain)))
            throw std::invalid_argument("Nonfinite physical body registration gains");
        const bool deferred = blend && blend->mHierarchyGain >= 1.f && blend->mVelocityGain >= 1.f
            && blend->mCounter <= 0 && !blend->mRegistrationOverride;
        if (deferred)
            return {membership != PhysicalBodyWorldMembership::Absent, false, true, true, true};
        return {membership == PhysicalBodyWorldMembership::Other, membership != PhysicalBodyWorldMembership::Desired,
            blend.has_value(), false, membership != PhysicalBodyWorldMembership::Desired};
    }

    PhysicalBlendCounterStep resolvePhysicalBlendCounterStep(PhysicalBlendCounterState state,
        PhysicalBlendCounterOperation operation, const PhysicalBlendCounterContext& context)
    {
        if (operation != PhysicalBlendCounterOperation::Increment && operation != PhysicalBlendCounterOperation::Decrement)
            throw std::invalid_argument("Invalid physical blend counter operation");
        const auto oldCounter = state.mCounter;
        const auto word = std::bit_cast<std::uint32_t>(oldCounter);
        state.mCounter = std::bit_cast<std::int32_t>(
            operation == PhysicalBlendCounterOperation::Increment ? word + 1u : word - 1u);
        PhysicalBlendCounterStep result{state, PhysicalBlendCounterSceneOperation::None, false};
        if (operation == PhysicalBlendCounterOperation::Increment)
        {
            if (oldCounter != 0) return result;
            if (context.mRawMode != 3)
            {
                if (state.mCollisionFlags & 0x40)
                {
                    if (context.mRawMode != 2)
                    {
                        result.mSceneOperation = PhysicalBlendCounterSceneOperation::DirectSync;
                        result.mState.mCollisionFlags &= ~std::uint16_t(0x40);
                    }
                }
                else if (state.mCollisionFlags & 1)
                    result.mSceneOperation = PhysicalBlendCounterSceneOperation::Virtual64Update;
            }
        }
        else
        {
            if (state.mCounter > 0) return result;
            result.mState.mCounter = 0;
        }
        result.mRequestNormalBodyRegistration = context.mHasBodyWrapper && context.mHasNativeBody
            && (context.mPackedFilter & 0x3f) == 8 && context.mHasRememberedWorldOwner;
        return result;
    }
}
