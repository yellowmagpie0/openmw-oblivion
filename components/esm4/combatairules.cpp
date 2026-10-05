#include "combatairules.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    void validateYieldSettings(const YieldSettings& settings)
    {
        for (float value : {settings.mBase, settings.mMultiplier, settings.mDurationBase, settings.mDurationMultiplier})
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native yield setting");
    }

    bool acceptsYield(const YieldAcceptanceInput& input)
    {
        if (input.mParalyzed)
            return false;
        if (input.mReceiverPlayer)
            return true;
        return input.mReceiverNpc && !(input.mTargetPlayer && input.mTargetEscapedJail)
            && !input.mRejectYields && input.mFightScore <= 0 && !input.mHasHostileEffectFromTarget;
    }

    float yieldScore(float fight, std::int32_t aggression, std::int32_t disposition, const YieldSettings& settings)
    {
        validateYieldSettings(settings);
        if (!std::isfinite(fight))
            throw std::invalid_argument("nonfinite native yield fight score");
        const auto difference = std::bit_cast<std::int32_t>(std::uint32_t(disposition) - std::uint32_t(aggression));
        const double result = double(difference) * settings.mMultiplier + (double(settings.mBase) + fight);
        if (std::abs(result) > std::numeric_limits<float>::max())
            throw std::overflow_error("native yield score overflow");
        return static_cast<float>(result);
    }

    float yieldDuration(unsigned draw, const YieldSettings& settings)
    {
        validateYieldSettings(settings);
        if (draw >= 32768)
            throw std::invalid_argument("invalid native yield raw draw");
        const double result = double(draw % 10) / 10 * settings.mDurationMultiplier + settings.mDurationBase;
        if (std::abs(result) > std::numeric_limits<float>::max())
            throw std::overflow_error("native yield duration overflow");
        return static_cast<float>(result);
    }

    bool yieldInterruptedByHits(std::int32_t combatState, std::int8_t hitCount, const YieldSettings& settings)
    {
        validateYieldSettings(settings);
        return combatState == 6 && hitCount > settings.mMaxHitCount;
    }

    void validateFightScoreSettings(const FightScoreSettings& settings)
    {
        for (float value : {settings.mDispositionBase, settings.mDispositionMultiplier, settings.mAggressionBase,
                 settings.mAggressionMultiplier, settings.mDistanceBase, settings.mDistanceMultiplier,
                 settings.mFriendDispositionBase, settings.mFriendDispositionMultiplier,
                 settings.mResponsibilityMultiplier})
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native fight score setting");
    }

    std::int32_t fightScore(const FightScoreInput& input, const FightScoreSettings& settings)
    {
        validateFightScoreSettings(settings);
        if (!std::isfinite(input.mDistance) || input.mDistance < 0)
            throw std::invalid_argument("invalid native fight score distance");
        if (input.mAggression <= 0)
            return 0;
        const auto term = [](double value, float multiplier, float base) {
            const double result = value * multiplier + base;
            if (!std::isfinite(result) || std::abs(result) > std::numeric_limits<float>::max())
                throw std::overflow_error("native fight score term overflow");
            return static_cast<float>(result);
        };
        const float disposition = term(input.mTargetDisposition, settings.mDispositionMultiplier, settings.mDispositionBase);
        const float aggression = term(input.mAggression, settings.mAggressionMultiplier, settings.mAggressionBase);
        const float distance = std::min(0.f, term(input.mDistance, settings.mDistanceMultiplier, settings.mDistanceBase));
        float friendDisposition = 0;
        if (input.mApplyFriendDisposition && input.mTargetDisposition < input.mFriendDisposition
            && (!input.mGateFriendByResponsibility
                || static_cast<double>(input.mResponsibility) * settings.mResponsibilityMultiplier
                    < input.mFriendDisposition))
            friendDisposition = term(input.mFriendDisposition, settings.mFriendDispositionMultiplier,
                settings.mFriendDispositionBase);
        // Native stores each term, then adds them on x87 without another float
        // store before truncation. Its conversion precedes the upper cap.
        const double result = std::trunc(static_cast<double>(aggression) + disposition + distance + friendDisposition);
        if (result < std::numeric_limits<std::int32_t>::min() || result > std::numeric_limits<std::int32_t>::max())
            throw std::overflow_error("native fight score integer overflow");
        return std::min(std::int32_t{100}, static_cast<std::int32_t>(result));
    }
}
