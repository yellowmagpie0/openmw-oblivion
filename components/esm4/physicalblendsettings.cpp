#include "physicalblendsettings.hpp"

#include <cmath>
#include <stdexcept>
#include <limits>

namespace ESM4
{
    namespace
    {
        void validate(float value)
        {
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native physical blend duration");
        }
    }

    PhysicalBlendDurationTables resolvePhysicalBlendDurationTables(
        const PhysicalBlendDurationTables& previous, const PhysicalBlendDurationSettings& settings)
    {
        validate(settings.mGetUpTime);
        validate(settings.mKnockdownTime);
        auto result = previous;
        for (std::size_t i = 0; i < result.mGetUp.size(); ++i)
        {
            validate(previous.mGetUp[i]);
            validate(previous.mKnockdown[i]);
            if (previous.mGetUp[i] >= 0.f)
                result.mGetUp[i] = settings.mGetUpTime;
            if (previous.mKnockdown[i] >= 0.f)
                result.mKnockdown[i] = settings.mKnockdownTime;
        }
        return result;
    }

    PhysicalKnockdownBlend preparePhysicalKnockdownBlend(PhysicalBlendGains current,
        float duration, float startKey, std::uint16_t controllerFlags, PhysicalBlendClock previousClock)
    {
        validate(current.mHierarchy);
        validate(current.mVelocity);
        validate(duration);
        validate(startKey);
        validate(previousClock.mElapsed);
        if (duration < 0.f)
            throw std::invalid_argument("disabled native knockdown blend duration");
        constexpr float sentinel = -std::numeric_limits<float>::max();
        previousClock.mStartTime = sentinel;
        previousClock.mPreviousTime = sentinel;
        // Setup ORs0xc5; NiTimeController::Start adds active bit0x8.
        return {{{{0.f, current}, {duration, {0.f, 0.f}}}}, startKey, duration,
            static_cast<std::uint16_t>((controllerFlags & 0xfef5u) | 0xcdu), previousClock};
    }

    PhysicalBlendKeyBounds resolvePhysicalBlendKeyBounds(
        PhysicalBlendKeyBounds previous, std::span<const PhysicalBlendKey> keys)
    {
        if (keys.empty())
            return {0.f, 0.f};
        validate(previous.mStartKey);
        validate(previous.mStopKey);
        validate(keys.front().mTime);
        validate(keys.back().mTime);
        constexpr float sentinel = std::numeric_limits<float>::max();
        if (keys.front().mTime < previous.mStartKey || previous.mStartKey == sentinel)
            previous.mStartKey = keys.front().mTime;
        if (keys.back().mTime < previous.mStopKey || previous.mStopKey == -sentinel)
            previous.mStopKey = keys.back().mTime;
        return previous;
    }

    PhysicalBlendControllerUpdate advancePhysicalBlendController(const PhysicalBlendControllerState& controller,
        const PhysicalBlendTimeCache& timeCache, bool hasTarget, std::optional<PhysicalBlendGains> targetGains,
        bool hasVelocityController, float inputTime)
    {
        PhysicalBlendControllerUpdate result{controller, timeCache, targetGains, false};
        auto& next = result.mController;
        if (!hasTarget || !(next.mTiming.mFlags & 8) || next.mKeys.empty())
            return result;
        validate(inputTime);
        if (!targetGains)
        {
            next.mClock.mPreviousTime = inputTime;
            return result;
        }
        validate(targetGains->mHierarchy);
        validate(targetGains->mVelocity);
        validate(next.mCachedGains.mHierarchy);
        validate(next.mCachedGains.mVelocity);
        const float keyTime = advancePhysicalBlendClock(next.mClock, result.mTimeCache, next.mTiming, inputTime);
        const auto evaluated = evaluatePhysicalBlendKeys(next.mKeys, keyTime, next.mCursor);
        next.mCursor = evaluated.mCursor;
        if (evaluated.mGains)
        {
            if (next.mCachedGains.mHierarchy < 0.f)
                next.mCachedGains = *targetGains;
            result.mTargetGains = evaluated.mGains;
        }
        if (keyTime == next.mTiming.mStopKey && (next.mTiming.mFlags & 6) == 4)
        {
            // Reset precedes restoration in Original8AAD60. Retain the cached
            // segment cursor even when its key array becomes empty.
            if (next.mTiming.mFlags & 0x40)
            {
                next.mKeys.clear();
                next.mCachedGains = {-1.f, -1.f};
                next.mSetupState = 0;
            }
            result.mRemoveVelocityController = (next.mTiming.mFlags & 0x80) && hasVelocityController;
            if ((next.mTiming.mFlags & 0x100) && next.mCachedGains.mHierarchy >= 0.f)
            {
                result.mTargetGains = next.mCachedGains;
                next.mCachedGains = {-1.f, -1.f};
            }
            next.mTiming.mFlags &= 0xfff7u;
            constexpr float sentinel = -std::numeric_limits<float>::max();
            next.mClock.mPreviousTime = sentinel;
            if (next.mTiming.mFlags & 1)
                next.mClock.mStartTime = sentinel;
        }
        return result;
    }

    float physicalBlendDurationForFilter(
        const PhysicalBlendDurationTables& tables, std::uint32_t filter, bool getUp)
    {
        const auto index = (filter >> 8) & 31u;
        const float duration = getUp ? tables.mGetUp[index] : tables.mKnockdown[index];
        validate(duration);
        return duration;
    }
}
