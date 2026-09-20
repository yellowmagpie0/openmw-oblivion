#ifndef OPENMW_ESM4_STEALTHRULES_H
#define OPENMW_ESM4_STEALTHRULES_H

#include "physicalcombat.hpp"

namespace ESM4
{
    struct PickpocketSettings
    {
        float mActorSkillBase;
        float mActorSkillMultiplier;
        float mTargetSkillBase;
        float mTargetSkillMultiplier;
        float mAmountBase;
        float mAmountMultiplier;
        float mMinimumChance;
        float mMaximumChance;
    };
    enum class PickpocketCheck { Transfer, UntouchedMenuExit };
    float pickpocketAmount(std::int32_t itemValue, std::uint32_t count);
    std::int32_t pickpocketChance(std::int32_t actorSkill, std::int32_t targetSkill, float amount,
        const PickpocketSettings& settings);
    bool pickpocketCheckSucceeds(PickpocketCheck check, std::int32_t chance, unsigned draw);
    void validatePickpocketSettings(const PickpocketSettings& settings);

    struct SneakAttackSettings
    {
        std::array<float, 5> mMeleeMultipliers;
        std::array<float, 5> mMarksmanMultipliers;
        std::int32_t mCombatMinimumDetection;
    };
    struct SneakAttackInput
    {
        std::int32_t mBaseSneak;
        std::int32_t mWeaponType; // Native WEAP type 0..5, or -1 for unarmed.
        std::int32_t mVictimDetection; // Signed contact-time score for this attacker.
        bool mNpcAttacker;
        bool mSneaking;
        bool mSwimming;
        bool mVictimCombatTarget; // Attacker is in victim controller's target list.
    };
    struct SneakAttackResult
    {
        float mMultiplier = 1.f;
        bool mBypassArmor = false;
        bool mBypassBlock = false;
    };
    // Contact-time rule only. The existing detector supplies actor-pair awareness;
    // world execution owns damage ordering, animation, events and persistence.
    SneakAttackResult sneakAttack(const SneakAttackInput& input,
        const SneakAttackSettings& settings, const CombatMasterySettings& mastery);
    void validateSneakAttackSettings(const SneakAttackSettings& settings);
}

#endif
