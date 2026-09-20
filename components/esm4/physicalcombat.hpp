#ifndef OPENMW_ESM4_PHYSICALCOMBAT_H
#define OPENMW_ESM4_PHYSICALCOMBAT_H

#include <cstdint>

namespace ESM4
{
    struct PhysicalCombatSettings
    {
        std::int32_t mLuckSkillBase;
        float mLuckSkillMultiplier;
        float mFatigueBase;
        float mFatigueMultiplier;
        float mWeaponMultiplier;
        float mSkillBase;
        float mSkillMultiplier;
        float mConditionBase;
        float mConditionMultiplier;
        float mAttributeBase;
        float mAttributeMultiplier;
    };

    struct WeaponDamageInput
    {
        std::int32_t mSkill;
        std::int32_t mLuck;
        std::int32_t mAttribute; // Governing attribute supplied by weapon policy.
        std::uint16_t mBaseDamage;
        float mConditionRatio; // Current / original maximum; may exceed one.
        float mFatigueRatio; // Supplied actor ratio; this helper does not clamp it.
        float mAttackMultiplier = 1.f;
        bool mIgnoreFatigue = false;
    };

    struct HandToHandSettings
    {
        float mSkillBase;
        float mSkillMultiplier;
        float mStrengthBase;
        float mStrengthMultiplier;
        float mHealthMinimum;
        float mHealthMaximum;
        float mFatigueBase;
        float mFatigueMultiplier;
    };

    struct AttackFatigueSettings
    {
        float mBase;
        float mWeightMultiplier;
        float mPowerMultiplier;
    };
    // Caller excludes staff/bow actions and supplies zero weight for unarmed.
    float attackFatigueCost(float weaponWeight, bool powerAttack, const AttackFatigueSettings& settings);
    void validateAttackFatigueSettings(const AttackFatigueSettings& settings);

    struct HandToHandInput
    {
        std::int32_t mSkill;
        std::int32_t mLuck;
        std::int32_t mStrength;
        float mFatigueRatio;
        bool mSuppressFatigueDamage = false;
    };

    struct HandToHandDamage
    {
        float mHealth;
        float mFatigue;
    };

    enum class BlockEquipment { Shield, Weapon, Unarmed };
    struct BlockSettings
    {
        float mSkillBase;
        float mSkillMultiplier;
        float mMaximum;
        float mWeaponMultiplier;
        float mUnarmedMultiplier;
    };
    struct BlockInput
    {
        std::int32_t mSkill;
        std::int32_t mLuck;
        float mFatigueRatio;
        BlockEquipment mEquipment;
    };

    struct ArmorRatingSettings
    {
        float mSkillBase;
        float mSkillMaximum;
        float mConditionBase;
        float mConditionMultiplier;
    };
    struct ArmorRatingInput
    {
        std::uint16_t mBaseHundredths;
        std::int32_t mSkill;
        std::int32_t mLuck;
        float mConditionRatio;
    };
    float armorRating(const ArmorRatingInput& input, const ArmorRatingSettings& settings,
        const PhysicalCombatSettings& physical);
    float capArmorRating(float total, float maximum);
    void validateArmorRatingSettings(const ArmorRatingSettings& settings);

    // The caller resolves identities first. Self-inflicted damage follows the
    // original victim-player branch; unknown sources select Unaffected.
    enum class PlayerDamageRole { Unaffected, Attacker, Victim };

    HandToHandDamage handToHandDamage(const HandToHandInput& input,
        const HandToHandSettings& settings, const PhysicalCombatSettings& physical);
    float blockFraction(const BlockInput& input, const BlockSettings& settings,
        const PhysicalCombatSettings& physical);
    float difficultyDamage(float damage, float difficulty, float multiplier, PlayerDamageRole role);
    void validateHandToHandSettings(const HandToHandSettings& settings);
    void validateBlockSettings(const BlockSettings& settings);

    // Pure original TES4 pre-mitigation arithmetic. No contact, mastery,
    // difficulty, armor, block, enchantment or actor mutation occurs here.
    float effectiveCombatSkill(std::int32_t skill, std::int32_t luck, const PhysicalCombatSettings& settings);
    float combatFatigueMultiplier(float ratio, const PhysicalCombatSettings& settings);
    float weaponDamage(const WeaponDamageInput& input, const PhysicalCombatSettings& settings);
    void validatePhysicalCombatSettings(const PhysicalCombatSettings& settings);
}

#endif
