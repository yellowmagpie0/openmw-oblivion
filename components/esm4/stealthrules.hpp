#ifndef OPENMW_ESM4_STEALTHRULES_H
#define OPENMW_ESM4_STEALTHRULES_H

#include "physicalcombat.hpp"

#include <optional>

namespace ESM4
{
    enum class PickpocketDirection { Take, Place };
    enum class PickpocketItemDecision { Allowed, NonPlayable, Equipped, Bound, QuestItem, DrawnWeapon, PositiveWeight };
    struct PickpocketItemInput
    {
        PickpocketDirection mDirection;
        bool mPlayable; // Native biped flag for ARMO/CLOT; true for other supported items.
        bool mAnyInstanceWorn; // Either worn marker in the selected native entry.
        bool mFirstInstanceBound; // ExtraBoundArmor on the entry's first extra-data instance.
        bool mQuestItem;
        bool mDrawnEquippedWeapon;
        float mBaseWeight;
    };
    // Normal live-NPC pickpocket menu policy; corpse/container transfers differ.
    // Does not transfer inventory or decide the taking success roll. A transfer
    // click clears untouched-session state before even a rejected attempt.
    PickpocketItemDecision pickpocketItemDecision(const PickpocketItemInput& input);

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
    enum class PickpocketOperation { Take, Place, Exit };
    struct PickpocketCheckPlan
    {
        std::optional<PickpocketCheck> mCheck;
        bool mFailureCanBeDetected = false;
    };
    // After item eligibility for Take/Place. A transfer click clears untouched
    // before eligibility, even if rejected. Taking still rolls against a knocked
    // target, but that failed roll does not trigger the caught branch.
    PickpocketCheckPlan planPickpocketCheck(
        PickpocketOperation operation, bool untouched, bool targetKnocked);
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
