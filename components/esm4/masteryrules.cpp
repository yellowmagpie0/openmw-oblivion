#include "masteryrules.hpp"

#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace ESM4
{
    namespace
    {
        void validate(unsigned draw, const MasteryProcSettings& settings, const CombatMasterySettings& mastery)
        {
            validateMasteryProcSettings(settings);
            validateCombatMasterySettings(mastery);
            if (draw >= 100)
                throw std::invalid_argument("invalid native mastery percentile");
        }
        void validateAttack(MasteryAttack attack)
        {
            switch (attack)
            {
                case MasteryAttack::Normal:
                case MasteryAttack::Standing:
                case MasteryAttack::Forward:
                case MasteryAttack::Backward:
                case MasteryAttack::Left:
                case MasteryAttack::Right:
                case MasteryAttack::Arrow:
                    return;
            }
            throw std::invalid_argument("invalid native mastery attack");
        }
        bool defensiveRank(const BlockMasteryInput& input, CombatMastery minimum, const CombatMasterySettings& mastery)
        {
            if (input.mHasWeapon || input.mHasShield)
                return input.mHasShield && combatMastery(input.mBlock, mastery) >= minimum;
            return combatMastery(input.mHandToHand, mastery) >= minimum;
        }
    }

    void validateMasteryProcSettings(const MasteryProcSettings& settings)
    {
        for (int value : {settings.mAttackDisarm, settings.mBlockDisarm, settings.mBlockStagger,
                 settings.mKnockdown, settings.mParalysis, settings.mHandBlockRecoil})
            if (value < 0 || value > 100)
                throw std::invalid_argument("invalid native mastery proc percentage");
    }

    AttackMasteryResult attackMasteryReaction(const AttackMasteryInput& input, unsigned draw,
        const MasteryProcSettings& settings, const CombatMasterySettings& mastery)
    {
        validate(draw, settings, mastery);
        validateAttack(input.mAttack);
        AttackMasteryResult result{false, input.mDamageKnockdown, false};
        const auto rank = combatMastery(input.mBaseSkill, mastery);
        if (rank < CombatMastery::Expert || input.mTargetBaseSpeed <= 0)
            return result;
        result.mConsumesDraw = true; // Original also rolls for non-proc attack directions.
        if ((input.mAttack == MasteryAttack::Arrow || input.mAttack == MasteryAttack::Backward)
            && draw < static_cast<unsigned>(settings.mKnockdown))
            result.mKnockdown = true;
        if (rank == CombatMastery::Master
            && (input.mAttack == MasteryAttack::Arrow || input.mAttack == MasteryAttack::Forward)
            && draw < static_cast<unsigned>(settings.mParalysis))
        {
            result.mParalysis = true;
            result.mKnockdown = false;
        }
        return result;
    }

    MasteryProcResult sidePowerDisarm(std::int32_t baseSkill, MasteryAttack attack, bool isNpc,
        bool targetHasWeapon, bool targetWeaponQuestItem, bool attackerWeaponDrawn, unsigned draw,
        const MasteryProcSettings& settings, const CombatMasterySettings& mastery)
    {
        validate(draw, settings, mastery);
        validateAttack(attack);
        if (!isNpc || !targetHasWeapon || (attack != MasteryAttack::Left && attack != MasteryAttack::Right)
            || combatMastery(baseSkill, mastery) < CombatMastery::Journeyman)
            return {};
        return {true, draw <= static_cast<unsigned>(settings.mAttackDisarm)
            && !targetWeaponQuestItem && attackerWeaponDrawn};
    }

    MasteryProcResult blockMasteryStagger(const BlockMasteryInput& input, unsigned draw,
        const MasteryProcSettings& settings, const CombatMasterySettings& mastery)
    {
        validate(draw, settings, mastery);
        if (!defensiveRank(input, CombatMastery::Expert, mastery))
            return {};
        return {true, draw <= static_cast<unsigned>(settings.mBlockStagger)};
    }

    MasteryProcResult blockMasteryDisarm(const BlockMasteryInput& input, bool isNpc,
        bool targetHasWeapon, bool targetWeaponQuestItem, bool defenderWeaponDrawn, unsigned draw,
        const MasteryProcSettings& settings, const CombatMasterySettings& mastery)
    {
        validate(draw, settings, mastery);
        if (!isNpc || !targetHasWeapon || !defensiveRank(input, CombatMastery::Master, mastery))
            return {};
        return {true, draw <= static_cast<unsigned>(settings.mBlockDisarm)
            && !targetWeaponQuestItem && defenderWeaponDrawn};
    }

    MasteryProcResult unarmedBlockRecoil(const UnarmedBlockRecoilInput& input, unsigned draw,
        const MasteryProcSettings& settings, const CombatMasterySettings& mastery)
    {
        validate(draw, settings, mastery);
        if (!std::isfinite(input.mAbsorbedFraction))
            throw std::invalid_argument("nonfinite native block absorption");
        if (input.mAbsorbedFraction > 0 || !input.mUnarmed || !input.mActiveBlock || input.mProjectile
            || !input.mAttackerHasWeapon
            || combatMastery(input.mBaseHandToHand, mastery) >= CombatMastery::Journeyman
            || combatMastery(input.mBaseBlock, mastery) < CombatMastery::Journeyman)
            return {};
        return {true, draw <= static_cast<unsigned>(settings.mHandBlockRecoil)};
    }

    void validateBowZoomSettings(const BowZoomSettings& settings)
    {
        if (!std::isfinite(settings.mZoomFov) || settings.mZoomFov <= 0
            || !std::isfinite(settings.mTimeChange) || settings.mTimeChange <= 0
            || !std::isfinite(settings.mTimeStart) || settings.mTimeStart < 0)
            throw std::invalid_argument("invalid native bow FOV settings");
    }

    std::optional<float> bowZoomFov(const BowZoomInput& input,
        const BowZoomSettings& settings, const CombatMasterySettings& mastery)
    {
        validateBowZoomSettings(settings);
        validateCombatMasterySettings(mastery);
        for (float value : {input.mElapsed, input.mCurrentFov, input.mNormalFov, input.mSceneFov, input.mDuration})
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native bow FOV input");
        if (input.mDuration < 0)
            throw std::invalid_argument("negative native bow FOV duration");
        if (!input.mEnabled)
            return {};
        if (input.mThirdPerson || input.mCameraRestricted)
            return input.mSceneFov == input.mNormalFov ? std::nullopt : std::optional(input.mNormalFov);
        if (input.mProcessAction == 5 && input.mBlockHeld)
        {
            if (!input.mAiming || combatMastery(input.mBaseMarksman, mastery) < CombatMastery::Journeyman
                || input.mElapsed < settings.mTimeStart || input.mCurrentFov <= settings.mZoomFov)
                return {};
            const float candidate = static_cast<float>(double(input.mNormalFov)
                + (double(input.mElapsed) - settings.mTimeStart) / settings.mTimeChange
                    * (double(settings.mZoomFov) - input.mNormalFov));
            const float result = std::max(candidate, settings.mZoomFov);
            if (!std::isfinite(result))
                throw std::overflow_error("native bow FOV overflow");
            return result;
        }
        if (input.mBlockHeld && input.mAiming)
            return {};
        if (input.mNormalFov <= input.mCurrentFov)
            return {};
        const float candidate = static_cast<float>(double(input.mCurrentFov)
            + double(input.mDuration) / settings.mTimeChange * (double(input.mNormalFov) - settings.mZoomFov));
        const float result = std::min(candidate, input.mNormalFov);
        if (!std::isfinite(result))
            throw std::overflow_error("native bow FOV overflow");
        return result;
    }

    std::uint8_t requestedDodgeGroup(std::uint32_t movementFlags)
    {
        for (unsigned bit = 0; bit < 4; ++bit)
            if (movementFlags & (1u << bit))
                return static_cast<std::uint8_t>(11 + bit);
        return 0xff;
    }

    bool blockingDodgeAllowed(const BlockingDodgeInput& input, const CombatMasterySettings& mastery)
    {
        validateCombatMasterySettings(mastery);
        return input.mBlockHeld && !input.mInterfacePreventsDodge && !input.mAnimationBusy
            && input.mAnimationDataPresent && input.mProcessPresent
            && combatMastery(input.mBaseAcrobatics, mastery) >= CombatMastery::Journeyman
            && input.mResolvedAnimationGroup && *input.mResolvedAnimationGroup >= 11
            && *input.mResolvedAnimationGroup <= 14;
    }
}
