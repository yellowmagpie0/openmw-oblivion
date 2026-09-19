#ifndef OPENMW_COMPONENTS_ESM4_LOADCSTY_H
#define OPENMW_COMPONENTS_ESM4_LOADCSTY_H

#include <array>
#include <optional>
#include <span>

#include <components/esm/defs.hpp>

#include "loadrawrecord.hpp"

namespace ESM4
{
    enum class CombatStyleFlag : std::uint8_t
    {
        Advanced = 1, ChooseAttackChance = 2, IgnoreAllies = 4, WillYield = 8,
        RejectYields = 16, DisableFleeing = 32, PreferRanged = 64, MeleeAlert = 128
    };

    struct CombatStyleTimers
    {
        float mMinimum = 0;
        float mMaximum = 0;
    };

    struct CombatStyleStandard
    {
        std::uint8_t mDodgeChance = 0;
        std::uint8_t mLeftRightChance = 0;
        CombatStyleTimers mDodgeLeftRight;
        CombatStyleTimers mDodgeForward;
        CombatStyleTimers mDodgeBack;
        CombatStyleTimers mIdle;
        std::uint8_t mBlockChance = 0;
        std::uint8_t mAttackChance = 0;
        float mAttackRecoilBonus = 0;
        float mAttackUnconsciousBonus = 0;
        float mAttackUnarmedBonus = 0;
        std::uint8_t mPowerAttackChance = 0;
        float mPowerAttackRecoilBonus = 0;
        float mPowerAttackUnconsciousBonus = 0;
        // Normal, forward, back, left, right; relative weights, not a sum-to-100 constraint.
        std::array<std::uint8_t, 5> mPowerAttackDirections{};
        CombatStyleTimers mHold;
        std::uint8_t mFlags = 0;
        std::uint8_t mAcrobaticDodgeChance = 0;
        // Legacy records omit complete tail groups. Absence is explicit:
        // editor defaults are not evidence of original-game runtime defaults.
        std::optional<std::array<float, 2>> mRangeMultipliers;
        std::optional<std::array<float, 2>> mSwitchDistances;
        std::optional<float> mBuffStandoff;
        std::optional<std::array<float, 2>> mRangedGroupStandoff;
        std::optional<std::uint8_t> mRushChance;
        std::optional<float> mRushDistanceMultiplier;
        std::optional<bool> mDoNotAcquire;
        bool has(CombatStyleFlag flag) const { return (mFlags & static_cast<std::uint8_t>(flag)) != 0; }
    };

    struct CombatStyleAdvanced
    {
        float mDodgeFatigueMultiplier = 0;
        float mDodgeFatigueBase = 0;
        float mEncumberedSpeedBase = 0;
        float mEncumberedSpeedMultiplier = 0;
        float mDodgeUnderAttack = 0;
        float mDodgeNotUnderAttack = 0;
        float mBackDodgeUnderAttack = 0;
        float mBackDodgeNotUnderAttack = 0;
        float mForwardDodgeAttacking = 0;
        float mForwardDodgeNotAttacking = 0;
        float mBlockSkillMultiplier = 0;
        float mBlockSkillBase = 0;
        float mBlockUnderAttack = 0;
        float mBlockNotUnderAttack = 0;
        float mAttackSkillMultiplier = 0;
        float mAttackSkillBase = 0;
        float mAttackUnderAttack = 0;
        float mAttackNotUnderAttack = 0;
        float mAttackDuringBlock = 0;
        float mPowerAttackFatigueBase = 0;
        float mPowerAttackFatigueMultiplier = 0;
    };

    CombatStyleStandard decodeCombatStyleStandard(std::span<const std::uint8_t> bytes);
    CombatStyleAdvanced decodeCombatStyleAdvanced(std::span<const std::uint8_t> bytes);

    // Keep the lossless logical payload, padding and unknown subrecords alongside
    // semantic fields. Raw-record consumers retain their original information.
    struct CombatStyle : RawRecord
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_CSTY4;
        std::optional<CombatStyleStandard> mStandard;
        std::optional<CombatStyleAdvanced> mAdvanced;
        void load(Reader& reader);
    };
}
#endif
