#include "combatstylepolicy.hpp"

#include <cmath>
#include <stdexcept>

namespace ESM4
{
    namespace
    {
        void validate(const CombatStyleStandard& value)
        {
            if (!value.mRangeMultipliers || !value.mSwitchDistances || !value.mBuffStandoff
                || !value.mRangedGroupStandoff || !value.mRushChance || !value.mRushDistanceMultiplier
                || !value.mDoNotAcquire)
                throw std::invalid_argument("native combat policy requires complete default inputs");
            for (const auto chance : { value.mDodgeChance, value.mLeftRightChance, value.mBlockChance,
                     value.mAttackChance, value.mPowerAttackChance, value.mAcrobaticDodgeChance, *value.mRushChance })
                if (chance > 100)
                    throw std::invalid_argument("native combat policy percentage outside domain");
            for (const auto chance : value.mPowerAttackDirections)
                if (chance > 100)
                    throw std::invalid_argument("native combat policy directional weight outside domain");
            for (const auto timer : { value.mDodgeLeftRight, value.mDodgeForward, value.mDodgeBack,
                     value.mIdle, value.mHold })
                if (!std::isfinite(timer.mMinimum) || !std::isfinite(timer.mMaximum)
                    || timer.mMinimum < 0 || timer.mMinimum > timer.mMaximum)
                    throw std::invalid_argument("native combat policy has invalid timer interval");
            for (const float bonus : { value.mAttackRecoilBonus, value.mAttackUnconsciousBonus,
                     value.mAttackUnarmedBonus, value.mPowerAttackRecoilBonus, value.mPowerAttackUnconsciousBonus })
                if (!std::isfinite(bonus))
                    throw std::invalid_argument("native combat policy has nonfinite attack bonus");
            for (const float distance : { (*value.mRangeMultipliers)[0], (*value.mRangeMultipliers)[1],
                     (*value.mSwitchDistances)[0], (*value.mSwitchDistances)[1], *value.mBuffStandoff,
                     (*value.mRangedGroupStandoff)[0], (*value.mRangedGroupStandoff)[1], *value.mRushDistanceMultiplier })
                if (!std::isfinite(distance) || distance < 0)
                    throw std::invalid_argument("native combat policy has invalid range/distance input");
        }
    }

    CombatStyleStandard resolveCombatStyleStandard(
        const CombatStyleStandard* record, const CombatStyleStandard& defaults)
    {
        validate(defaults);
        if (!record)
            return defaults;

        CombatStyleStandard result = *record;
        // Original TES4's CSTD loader clears the record then seeds these tails
        // before copying the payload. Range multipliers are literal one and
        // the secondary acquisition flag stays zero, unlike DefaultCombatStyle.
        if (!result.mRangeMultipliers)
            result.mRangeMultipliers = std::array<float, 2>{ 1, 1 };
        if (!result.mSwitchDistances)
            result.mSwitchDistances = defaults.mSwitchDistances;
        if (!result.mBuffStandoff)
            result.mBuffStandoff = defaults.mBuffStandoff;
        if (!result.mRangedGroupStandoff)
            result.mRangedGroupStandoff = defaults.mRangedGroupStandoff;
        if (!result.mRushChance)
            result.mRushChance = defaults.mRushChance;
        if (!result.mRushDistanceMultiplier)
            result.mRushDistanceMultiplier = defaults.mRushDistanceMultiplier;
        if (!result.mDoNotAcquire)
            result.mDoNotAcquire = false;

        // Reject invalid caller input before applying the native zero-value
        // compatibility rule. The binary decoder likewise rejects bad domains.
        validate(result);
        for (std::size_t i = 0; i < 2; ++i)
            if ((*result.mSwitchDistances)[i] == 0)
                (*result.mSwitchDistances)[i] = (*defaults.mSwitchDistances)[i];
        if (*result.mRushChance == 0)
            result.mRushChance = defaults.mRushChance;
        if (*result.mRushDistanceMultiplier == 0)
            result.mRushDistanceMultiplier = defaults.mRushDistanceMultiplier;
        return result;
    }
    CombatStyleAdvanced resolveCombatStyleAdvanced(const CombatStyle* record, const CombatStyleAdvanced& defaults)
    {
        const auto check = [](const CombatStyleAdvanced& value) {
            for (const float field : { value.mDodgeFatigueMultiplier, value.mDodgeFatigueBase,
                     value.mEncumberedSpeedBase, value.mEncumberedSpeedMultiplier,
                     value.mDodgeUnderAttack, value.mDodgeNotUnderAttack,
                     value.mBackDodgeUnderAttack, value.mBackDodgeNotUnderAttack,
                     value.mForwardDodgeAttacking, value.mForwardDodgeNotAttacking,
                     value.mBlockSkillMultiplier, value.mBlockSkillBase, value.mBlockUnderAttack,
                     value.mBlockNotUnderAttack, value.mAttackSkillMultiplier, value.mAttackSkillBase,
                     value.mAttackUnderAttack, value.mAttackNotUnderAttack, value.mAttackDuringBlock,
                     value.mPowerAttackFatigueBase, value.mPowerAttackFatigueMultiplier })
                if (!std::isfinite(field))
                    throw std::invalid_argument("native advanced combat policy has nonfinite modifier");
        };
        check(defaults);
        if (!record)
            return defaults;
        if (!record->mStandard)
            throw std::invalid_argument("native combat style lacks standard data");
        if (!record->mStandard->has(CombatStyleFlag::Advanced))
            return defaults;
        if (!record->mAdvanced)
            throw std::invalid_argument("native advanced combat style lacks modifier data");
        check(*record->mAdvanced);
        return *record->mAdvanced;
    }

}
