#ifndef OPENMW_ESM4_COMBATAIRULES_H
#define OPENMW_ESM4_COMBATAIRULES_H

#include <cstdint>

namespace ESM4
{
    struct FightScoreSettings
    {
        float mDispositionBase;
        float mDispositionMultiplier;
        float mAggressionBase;
        float mAggressionMultiplier;
        float mDistanceBase;
        float mDistanceMultiplier;
        float mFriendDispositionBase;
        float mFriendDispositionMultiplier;
        float mResponsibilityMultiplier;
    };
    struct FightScoreInput
    {
        std::int32_t mTargetDisposition;
        std::int32_t mFriendDisposition;
        std::int32_t mAggression;
        float mDistance;
        bool mApplyFriendDisposition;
        bool mGateFriendByResponsibility;
        std::int32_t mResponsibility;
    };
    // Native numeric score shared by AI and alarm response. Callers resolve
    // dispositions and select the friend/responsibility gates. This does not
    // start combat or decide actor/incident eligibility. Negative scores remain
    // negative; aggression <= 0 returns zero and only the upper bound is capped.
    std::int32_t fightScore(const FightScoreInput& input, const FightScoreSettings& settings);
    void validateFightScoreSettings(const FightScoreSettings& settings);

    struct YieldSettings
    {
        float mBase;
        float mMultiplier;
        float mDurationBase;
        float mDurationMultiplier;
        std::int32_t mMaxHitCount;
    };
    struct YieldAcceptanceInput
    {
        bool mReceiverNpc;
        bool mReceiverPlayer;
        bool mParalyzed;
        bool mTargetPlayer;
        bool mTargetEscapedJail;
        bool mRejectYields;
        std::int32_t mFightScore; // Resolved native score at distance 100, friend gates off.
        bool mHasHostileEffectFromTarget; // Exact target MagicCaster identity; M16 data boundary.
    };
    bool acceptsYield(const YieldAcceptanceInput& input);
    // Native signed int32 disposition-aggression subtraction precedes scaling.
    float yieldScore(float fight, std::int32_t aggression, std::int32_t disposition,
        const YieldSettings& settings);
    // Original 15-bit raw draw modulo ten; consumes one draw, not a new RNG.
    float yieldDuration(unsigned draw, const YieldSettings& settings);
    bool yieldInterruptedByHits(std::int32_t combatState, std::int8_t hitCount, const YieldSettings& settings);
    void validateYieldSettings(const YieldSettings& settings);
}

#endif
