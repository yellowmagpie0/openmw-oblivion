#include "masteryrules.hpp"

#include <stdexcept>

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
}
