#include "combatairules.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    namespace
    {
        std::int32_t dispositionInteger(double value, ActorValueConversionMode mode)
        {
            if (!std::isfinite(value)
                || (mode != ActorValueConversionMode::NonSse && mode != ActorValueConversionMode::Sse))
                throw std::invalid_argument("invalid native disposition conversion");
            if (mode == ActorValueConversionMode::Sse)
                return value < -0x1p31 || value >= 0x1p31
                    ? std::numeric_limits<std::int32_t>::min() : static_cast<std::int32_t>(value);
            if (value < -0x1p63 || value >= 0x1p63) return 0;
            return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(
                static_cast<std::int64_t>(value))));
        }
    }

    std::int32_t modifyDispositionModifier(std::optional<std::int32_t> prior,
        std::int32_t currentDisposition, std::int32_t delta, ActorValueConversionMode mode)
    {
        if (currentDisposition < 0 || currentDisposition > 100)
            throw std::invalid_argument("invalid current native disposition");
        float change = static_cast<float>(delta); // 503AC0 FILD, 503AC7 FLOATSTORE.
        if (!prior) return dispositionInteger(change, mode);
        const double requested = double(currentDisposition) + change;
        if (requested < 0)
            change = static_cast<float>(double(change) - requested);
        else if (requested > 100)
        {
            const auto excessInteger = std::bit_cast<std::int32_t>(
                static_cast<std::uint32_t>(dispositionInteger(requested, mode)) - 100u);
            const float excess = static_cast<float>(excessInteger);
            change = excess < change ? static_cast<float>(double(change) - excess) : 0.f;
        }
        // FILD old integer + FLOAT delta, without a further FLOATSTORE.
        return dispositionInteger(double(*prior) + change, mode);
    }

    void validateDispositionSettings(const DispositionSettings& settings)
    {
        for (float value : {settings.mActorPersonalityBase, settings.mActorPersonalityMultiplier,
                 settings.mTargetPersonalityBase, settings.mTargetPersonalityMultiplier,
                 settings.mTargetFameMultiplier, settings.mTargetInfamyMultiplier, settings.mActorInfamyMultiplier,
                 settings.mTargetBountyMultiplier, settings.mActorBountyBase, settings.mActorBountyMultiplier,
                 settings.mRaceMultiplier, settings.mFactionMultiplier, settings.mFactionRankBase,
                 settings.mWeaponPenalty, settings.mDiseasePenalty, settings.mInfamyMaximum})
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native disposition setting");
    }

    std::int32_t dispositionScore(const DispositionInput& input, const DispositionSettings& settings,
        ActorValueConversionMode mode)
    {
        validateDispositionSettings(settings);
        const auto integer = [](double value) {
            const double result = std::trunc(value);
            if (!std::isfinite(result) || result < std::numeric_limits<std::int32_t>::min()
                || result > std::numeric_limits<std::int32_t>::max())
                throw std::overflow_error("native disposition conversion overflow");
            return static_cast<std::int32_t>(result);
        };
        const auto capped = [](std::int32_t value, std::int32_t maximum) {
            if (value > maximum)
                return maximum;
            const auto minimum = std::bit_cast<std::int32_t>(0u - static_cast<std::uint32_t>(maximum));
            return value < minimum ? minimum : value;
        };
        const auto personality = integer(((double(input.mTargetPersonality) * settings.mTargetPersonalityMultiplier
                                             + settings.mTargetPersonalityBase)
            + (double(input.mActorPersonality) * settings.mActorPersonalityMultiplier
                + settings.mActorPersonalityBase)) * .5);
        std::int32_t fame = 0;
        std::int32_t bounty = 0;
        float infamy = 0;
        if (!input.mActorCreature)
        {
            fame = integer(double(input.mTargetFame) * settings.mTargetFameMultiplier);
            // Native uses TargetInfamyMult twice, leaving ActorInfamyBase
            // unread. The product is stored as float before capping/summing.
            const double value = (double(input.mActorResponsibility) * settings.mActorInfamyMultiplier
                + settings.mTargetInfamyMultiplier)
                * (double(settings.mTargetInfamyMultiplier) * input.mTargetInfamy);
            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
                throw std::overflow_error("native disposition infamy overflow");
            infamy = static_cast<float>(value);
            bounty = integer((double(input.mActorResponsibility) * settings.mActorBountyMultiplier
                + settings.mActorBountyBase) * (double(input.mTargetBounty) * settings.mTargetBountyMultiplier));
        }
        const auto race = integer(double(input.mRaceReaction) * settings.mRaceMultiplier);
        // Likewise TargetFactionMult is used for both factors; the separate
        // TargetFactionRankMult setting is not read by the original scorer.
        const auto faction = integer((double(input.mFactionRank) * settings.mFactionMultiplier
            + settings.mFactionRankBase) * (double(settings.mFactionMultiplier) * input.mFactionReaction));
        const auto weapon = integer(double(input.mWeaponPenalty) * settings.mWeaponPenalty);
        const auto disease = integer(double(input.mDiseasePenalty) * settings.mDiseasePenalty);
        if (infamy > settings.mInfamyMaximum)
            infamy = settings.mInfamyMaximum;
        else if (infamy < -settings.mInfamyMaximum)
            infamy = -settings.mInfamyMaximum;
        fame = capped(fame, settings.mFameMaximum);
        bounty = capped(bounty, settings.mBountyMaximum);
        const auto base = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(personality)
            + static_cast<std::uint32_t>(fame));
        return dispositionInteger((((((double(base) + infamy) + bounty) + race) + faction) + weapon)
            + disease + input.mModifier, mode);
    }

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
