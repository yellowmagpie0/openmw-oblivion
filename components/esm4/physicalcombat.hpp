#ifndef OPENMW_ESM4_PHYSICALCOMBAT_H
#define OPENMW_ESM4_PHYSICALCOMBAT_H

#include <cstdint>
#include <array>
#include <optional>

namespace ESM4
{
    enum class CombatMastery { Novice, Apprentice, Journeyman, Expert, Master };
    struct CombatMasterySettings
    {
        std::array<std::int32_t, 4> mMinimumSkill;
    };
    enum class PowerAttackDirection { Standing, Forward, Backward, Left, Right };
    struct PowerAttackSettings
    {
        float mBaseMultiplier;
        float mStandingMultiplier;
        float mSideMultiplier;
        float mBackwardMultiplier;
        float mForwardMultiplier;
    };
    CombatMastery combatMastery(std::int32_t skill, const CombatMasterySettings& settings);
    float powerAttackMultiplier(std::int32_t skill, PowerAttackDirection direction,
        const PowerAttackSettings& settings, const CombatMasterySettings& mastery);
    void validateCombatMasterySettings(const CombatMasterySettings& settings);
    void validatePowerAttackSettings(const PowerAttackSettings& settings);

    struct MeleeReachSettings
    {
        float mCombatDistance;
        float mHandMultiplier;
        float mGiantMultiplier;
    };
    // Base reach is rounded before applying resolved actor scale (including NPC race
    // height). Creature RNAM is already a distance; weapon reach is a
    // combat-distance factor.
    float weaponMeleeReach(float weaponReach, float scale, const MeleeReachSettings& settings);
    float unarmedMeleeReach(float scale, const MeleeReachSettings& settings);
    float creatureMeleeReach(std::uint8_t reach, std::uint8_t creatureType, float scale,
        const MeleeReachSettings& settings);
    void validateMeleeReachSettings(const MeleeReachSettings& settings);

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

    struct EssentialRecoverySettings
    {
        float mDelay;
        float mHealthFraction;
    };
    struct EssentialRecoveryHealth
    {
        float mTarget;
        float mAdjustment;
    };
    // Shared by entry into essential unconsciousness and its recovery. The
    // controller owns eligibility, the countdown and the actor-value mutation.
    EssentialRecoveryHealth essentialRecoveryHealth(std::int32_t baseHealth, float currentHealth,
        const EssentialRecoverySettings& settings);
    void validateEssentialRecoverySettings(const EssentialRecoverySettings& settings);

    struct KnockdownSettings
    {
        float mAgilityBase;
        float mAgilityMultiplier;
        float mDamageBase;
        float mDamageMultiplier;
        float mMaximumChance;
    };
    // Caller supplies truncated pre-armor contact damage and a draw in [0,99].
    // This is the random damage rule, not unconsciousness or mastery policy.
    bool damageKnockdown(std::int32_t agility, std::int32_t luck, float fatigueRatio,
        std::int32_t damage, unsigned draw, const KnockdownSettings& settings,
        const PhysicalCombatSettings& physical);
    void validateKnockdownSettings(const KnockdownSettings& settings);

    struct KnockbackSettings
    {
        float mAgilityBase;
        float mAgilityMultiplier;
        float mDamageBase;
        float mDamageMultiplier;
        float mMaximumForce;
        float mDuration;
    };
    // Signed force before caller direction/physics; only its upper bound is capped.
    float damageKnockback(std::int32_t agility, std::int32_t luck, float fatigueRatio,
        std::int32_t damage, const KnockbackSettings& settings, const PhysicalCombatSettings& physical);
    void validateKnockbackSettings(const KnockbackSettings& settings);
    // Mastery uses the base actor value, floored before rank lookup, not the
    // luck/effect-adjusted combat value. Fatigue divides current by this base.
    std::int32_t combatBaseValue(float value);
    float combatFatigueRatio(float current, std::int32_t base);

    struct BlockCostSettings
    {
        float mBase;
        float mMultiplier;
        float mSkillBase;
        float mSkillMultiplier;
    };
    struct BlockContactCosts
    {
        float mFatigueDebit;
        float mBlockingItemWear;
    };
    // Current integer Block enters fatigue arithmetic; base Block selects
    // mastery. Item wear still passes through the item condition mutation.
    BlockContactCosts blockContactCosts(std::int32_t baseBlock, std::int32_t currentBlock,
        float damage, float absorbedFraction, bool hasBlockingItem,
        const BlockCostSettings& settings, const CombatMasterySettings& mastery);
    void validateBlockCostSettings(const BlockCostSettings& settings);

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

    struct DurabilitySettings
    {
        float mWeaponDamageMultiplier;
        float mArmorDamageMultiplier;
    };
    // Pure wear amounts. Contact eligibility, armor-piece selection and
    // condition mutation are caller responsibilities.
    float weaponWear(std::uint16_t baseDamage, const DurabilitySettings& settings);
    float armorWear(float incomingDamage, float absorbedFraction, const DurabilitySettings& settings);
    void validateDurabilitySettings(const DurabilitySettings& settings);

    enum class ArmorWeight { Light, Heavy };
    struct ArmorWearMasterySettings
    {
        float mLightNoviceMultiplier;
        float mHeavyNoviceMultiplier;
        float mLightJourneymanMultiplier;
        float mHeavyJourneymanMultiplier;
    };
    float armorWearMasteryMultiplier(std::int32_t skill, ArmorWeight weight,
        const ArmorWearMasterySettings& settings, const CombatMasterySettings& mastery);
    void validateArmorWearMasterySettings(const ArmorWearMasterySettings& settings);
    // Caller supplies final wear after mastery/block policy. Positive wear
    // snaps remaining condition below one to zero; repaired excess is retained.
    float conditionAfterWear(float current, float wear);

    struct ArmorMitigation
    {
        float mHealthDamage;
        float mAbsorbedFraction;
    };
    // Total rating already includes the actor's aggregation/mastery/cap policy.
    // The caller resolves armor bypass eligibility; this helper only applies it.
    ArmorMitigation mitigateArmor(float damage, float rating, float maximumFraction, bool bypass);

    enum class ArmorWearSlot { Head, Hair, UpperBody, LowerBody, Hands, Feet, Shield };
    struct ArmorWearSelectionSettings
    {
        std::int32_t mHeadChance;
        std::int32_t mUpperBodyChance;
        std::int32_t mLowerBodyChance;
        std::int32_t mHandsChance;
        std::int32_t mFeetChance;
    };
    // One original selection attempt, using a supplied [0,99] draw. Missing
    // candidates fall forward through thresholds; feet do not fall to shield.
    // The world retries at most seven draws, stopping on the first selection.
    inline constexpr unsigned ArmorWearSelectionAttempts = 7;
    std::optional<ArmorWearSlot> selectArmorWearSlot(unsigned draw,
        const std::array<bool, 7>& available, const ArmorWearSelectionSettings& settings);
    void validateArmorWearSelectionSettings(const ArmorWearSelectionSettings& settings);

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

    struct ArmorMasterySettings
    {
        // Head, hair, upper body, lower body, hands, feet, active shield.
        std::array<std::int32_t, 7> mCoverage;
        std::int32_t mLightMasterMinimum;
        float mLightMasterRatingMultiplier;
        float mHeavyExpertWeightMultiplier;
        float mHeavyMasterWeightMultiplier;
        float mLightExpertWeightMultiplier;
    };
    std::int32_t armorCoverage(const std::array<bool, 7>& matchingSlots,
        const ArmorMasterySettings& settings);
    float masteryArmorRating(float itemRating, float otherRating, std::int32_t baseLightArmor,
        std::int32_t lightCoverage, std::int32_t heavyCoverage, float maximum,
        const ArmorMasterySettings& settings, const CombatMasterySettings& mastery);
    // Reduction applies to one worn instance, not other items in its stack.
    float wornArmorWeight(float weight, bool heavy, std::int32_t baseSkill, bool worn,
        const ArmorMasterySettings& settings, const CombatMasterySettings& mastery);
    void validateArmorMasterySettings(const ArmorMasterySettings& settings);

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
    // Creature natural attacks use base attack damage and fatigue, followed by
    // truncation to signed integer in the original actor virtual method.
    std::int32_t creatureNaturalDamage(std::uint16_t baseDamage, float fatigueRatio,
        const PhysicalCombatSettings& settings);
    void validatePhysicalCombatSettings(const PhysicalCombatSettings& settings);
}

#endif
