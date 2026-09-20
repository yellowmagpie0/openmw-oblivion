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
}

#endif
