#ifndef OPENMW_ESM4_COMBATAIRULES_H
#define OPENMW_ESM4_COMBATAIRULES_H

#include "actorvalues.hpp"

#include <cstdint>
#include <optional>

namespace ESM4
{
    struct DispositionSettings
    {
        float mActorPersonalityBase, mActorPersonalityMultiplier;
        float mTargetPersonalityBase, mTargetPersonalityMultiplier;
        float mTargetFameMultiplier;
        float mTargetInfamyMultiplier, mActorInfamyMultiplier;
        float mTargetBountyMultiplier, mActorBountyBase, mActorBountyMultiplier;
        float mRaceMultiplier, mFactionMultiplier, mFactionRankBase;
        float mWeaponPenalty, mDiseasePenalty;
        std::int32_t mFameMaximum, mBountyMaximum;
        float mInfamyMaximum;
    };
    struct DispositionInput
    {
        std::int32_t mActorPersonality, mTargetPersonality, mActorResponsibility;
        std::int32_t mTargetFame, mTargetInfamy, mTargetBounty;
        std::int32_t mRaceReaction, mFactionReaction, mFactionRank;
        bool mWeaponPenalty, mDiseasePenalty;
        std::int32_t mModifier;
        bool mActorCreature;
    };
    // Original 546260 numeric score, before the actor wrapper's [0,100] clamp.
    // Caller resolves ownership/leveled bases, factions, process flags and
    // per-target modifiers. Intermediate terms outside int32 remain unsupported;
    // the final conversion retains the selected native CPU overflow behavior.
    std::int32_t dispositionScore(const DispositionInput& input, const DispositionSettings& settings,
        ActorValueConversionMode mode = ActorValueConversionMode::NonSse);
    // Original 503A50 integer-to-float request and 5E2070 modifier update.
    // An absent entry is distinct from an existing zero entry.
    std::int32_t modifyDispositionModifier(std::optional<std::int32_t> prior,
        std::int32_t currentDisposition, std::int32_t delta, ActorValueConversionMode mode);
    enum class DispositionArithmeticPrecision : std::uint8_t { Double, Extended };
    // Original 5E2070 accepts a binary32 delta directly. The command's integer
    // request conversion and per-frame result cache are separate. Select x87
    // PC53/PC64 arithmetic independently of 9828C0's CPU conversion branch;
    // very large finite deltas can distinguish both precision modes.
    std::int32_t modifyDispositionModifierFloat(std::optional<std::int32_t> prior,
        std::int32_t currentDisposition, float delta, ActorValueConversionMode mode,
        DispositionArithmeticPrecision precision = DispositionArithmeticPrecision::Double);
    void validateDispositionSettings(const DispositionSettings& settings);

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
