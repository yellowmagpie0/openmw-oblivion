#ifndef OPENMW_ESM4_MASTERYRULES_H
#define OPENMW_ESM4_MASTERYRULES_H

#include "physicalcombat.hpp"

namespace ESM4
{
    struct MasteryProcSettings
    {
        std::int32_t mAttackDisarm;
        std::int32_t mBlockDisarm;
        std::int32_t mBlockStagger;
        std::int32_t mKnockdown;
        std::int32_t mParalysis;
        std::int32_t mHandBlockRecoil;
    };
    enum class MasteryAttack { Normal, Standing, Forward, Backward, Left, Right, Arrow };
    struct AttackMasteryInput
    {
        std::int32_t mBaseSkill;
        MasteryAttack mAttack;
        std::int32_t mTargetBaseSpeed;
        bool mDamageKnockdown;
    };
    struct AttackMasteryResult
    {
        bool mConsumesDraw;
        bool mKnockdown;
        bool mParalysis;
    };
    struct MasteryProcResult
    {
        bool mConsumesDraw;
        bool mTriggered;
    };
    struct BlockMasteryInput
    {
        std::int32_t mBlock;
        std::int32_t mHandToHand;
        bool mHasWeapon;
        bool mHasShield;
    };
    // Decisions only: callers own contact eligibility, actual RNG advancement,
    // real inventory drops, effect application, controller state and saves.
    AttackMasteryResult attackMasteryReaction(const AttackMasteryInput& input, unsigned draw,
        const MasteryProcSettings& settings, const CombatMasterySettings& mastery);
    MasteryProcResult sidePowerDisarm(std::int32_t baseSkill, MasteryAttack attack, bool isNpc,
        bool targetHasWeapon, bool targetWeaponQuestItem, bool attackerWeaponDrawn, unsigned draw,
        const MasteryProcSettings& settings, const CombatMasterySettings& mastery);
    MasteryProcResult blockMasteryStagger(const BlockMasteryInput& input, unsigned draw,
        const MasteryProcSettings& settings, const CombatMasterySettings& mastery);
    MasteryProcResult blockMasteryDisarm(const BlockMasteryInput& input, bool isNpc,
        bool targetHasWeapon, bool targetWeaponQuestItem, bool defenderWeaponDrawn, unsigned draw,
        const MasteryProcSettings& settings, const CombatMasterySettings& mastery);
    void validateMasteryProcSettings(const MasteryProcSettings& settings);
}

#endif
