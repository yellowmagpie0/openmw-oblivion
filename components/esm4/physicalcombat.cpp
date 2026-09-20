#include "physicalcombat.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    namespace
    {
        void finite(float value)
        {
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native physical combat input");
        }
        void nonnegative(float value)
        {
            finite(value);
            if (value < 0)
                throw std::invalid_argument("negative native physical combat factor");
        }
        float rounded(double value)
        {
            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
                throw std::invalid_argument("native physical combat arithmetic overflow");
            return static_cast<float>(value);
        }
        float skillValue(std::int32_t skill, std::int32_t luck, const PhysicalCombatSettings& settings)
        {
            const float luckTerm = rounded(double(luck) * settings.mLuckSkillMultiplier);
            const float adjustment = rounded(double(luckTerm) + settings.mLuckSkillBase);
            return rounded(std::clamp(double(skill) + adjustment, 0.0, 100.0));
        }
        float fatigueValue(float ratio, const PhysicalCombatSettings& settings)
        {
            finite(ratio);
            return rounded(settings.mFatigueBase - (1.0 - ratio) * settings.mFatigueMultiplier);
        }
    }

    void validateDurabilitySettings(const DurabilitySettings& settings)
    {
        nonnegative(settings.mWeaponDamageMultiplier);
        nonnegative(settings.mArmorDamageMultiplier);
    }

    float weaponWear(std::uint16_t baseDamage, const DurabilitySettings& settings)
    {
        validateDurabilitySettings(settings);
        return rounded(double(baseDamage) * settings.mWeaponDamageMultiplier);
    }

    float armorWear(float incomingDamage, float absorbedFraction, const DurabilitySettings& settings)
    {
        validateDurabilitySettings(settings);
        nonnegative(incomingDamage);
        nonnegative(absorbedFraction);
        if (absorbedFraction > 1.f)
            throw std::invalid_argument("invalid native absorbed damage fraction");
        return rounded(double(incomingDamage) * absorbedFraction * settings.mArmorDamageMultiplier);
    }

    void validateCombatMasterySettings(const CombatMasterySettings& settings)
    {
        if (settings.mMinimumSkill.front() < 0
            || !std::is_sorted(settings.mMinimumSkill.begin(), settings.mMinimumSkill.end()))
            throw std::invalid_argument("invalid native combat mastery thresholds");
    }
    void validatePowerAttackSettings(const PowerAttackSettings& settings)
    {
        for (const float value : {settings.mBaseMultiplier, settings.mStandingMultiplier,
                 settings.mSideMultiplier, settings.mBackwardMultiplier, settings.mForwardMultiplier})
            nonnegative(value);
    }
    CombatMastery combatMastery(std::int32_t skill, const CombatMasterySettings& settings)
    {
        validateCombatMasterySettings(settings);
        return static_cast<CombatMastery>(std::upper_bound(settings.mMinimumSkill.begin(),
            settings.mMinimumSkill.end(), skill) - settings.mMinimumSkill.begin());
    }
    float powerAttackMultiplier(std::int32_t skill, PowerAttackDirection direction,
        const PowerAttackSettings& settings, const CombatMasterySettings& mastery)
    {
        validatePowerAttackSettings(settings);
        const auto rank = combatMastery(skill, mastery);
        switch (direction)
        {
            case PowerAttackDirection::Standing:
                return rank >= CombatMastery::Apprentice ? settings.mStandingMultiplier : settings.mBaseMultiplier;
            case PowerAttackDirection::Left:
            case PowerAttackDirection::Right:
                return rank >= CombatMastery::Journeyman ? settings.mSideMultiplier : settings.mBaseMultiplier;
            case PowerAttackDirection::Backward:
                return rank >= CombatMastery::Expert ? settings.mBackwardMultiplier : settings.mBaseMultiplier;
            case PowerAttackDirection::Forward:
                return rank >= CombatMastery::Master ? settings.mForwardMultiplier : settings.mBaseMultiplier;
        }
        throw std::invalid_argument("invalid native power attack direction");
    }

    void validatePhysicalCombatSettings(const PhysicalCombatSettings& settings)
    {
        for (const float value : { settings.mLuckSkillMultiplier, settings.mFatigueBase,
                 settings.mFatigueMultiplier, settings.mWeaponMultiplier, settings.mSkillBase,
                 settings.mSkillMultiplier, settings.mConditionBase, settings.mConditionMultiplier,
                 settings.mAttributeBase, settings.mAttributeMultiplier })
            nonnegative(value);
    }

    void validateAttackFatigueSettings(const AttackFatigueSettings& settings)
    {
        nonnegative(settings.mBase);
        nonnegative(settings.mWeightMultiplier);
        nonnegative(settings.mPowerMultiplier);
    }

    float attackFatigueCost(float weaponWeight, bool powerAttack, const AttackFatigueSettings& settings)
    {
        validateAttackFatigueSettings(settings);
        nonnegative(weaponWeight);
        const float cost = rounded(settings.mBase + double(weaponWeight) * settings.mWeightMultiplier);
        return powerAttack ? rounded(double(cost) * settings.mPowerMultiplier) : cost;
    }

    float effectiveCombatSkill(std::int32_t skill, std::int32_t luck, const PhysicalCombatSettings& settings)
    {
        validatePhysicalCombatSettings(settings);
        return skillValue(skill, luck, settings);
    }

    float combatFatigueMultiplier(float ratio, const PhysicalCombatSettings& settings)
    {
        validatePhysicalCombatSettings(settings);
        return fatigueValue(ratio, settings);
    }

    float weaponDamage(const WeaponDamageInput& input, const PhysicalCombatSettings& settings)
    {
        validatePhysicalCombatSettings(settings);
        nonnegative(input.mConditionRatio);
        finite(input.mFatigueRatio);
        nonnegative(input.mAttackMultiplier);
        if (input.mAttribute < 0)
            throw std::invalid_argument("negative governing combat attribute");
        // Original x87 routines explicitly store each of these terms as float.
        // Use wider intermediates and those same rounding boundaries; do not
        // round each individual multiplication to float or truncate to integer.
        constexpr double percent = static_cast<double>(0.01f);
        const float base = rounded(double(input.mBaseDamage) * settings.mWeaponMultiplier);
        const float condition = rounded(settings.mConditionBase
            + double(input.mConditionRatio) * settings.mConditionMultiplier);
        const float skill = rounded(settings.mSkillBase
            + skillValue(input.mSkill, input.mLuck, settings) * percent * settings.mSkillMultiplier);
        const float attribute = rounded(settings.mAttributeBase
            + std::min(input.mAttribute, 100) * percent * settings.mAttributeMultiplier);
        const float fatigue = input.mIgnoreFatigue ? 1.f : fatigueValue(input.mFatigueRatio, settings);
        const float subtotal = rounded(double(condition) * base * skill * attribute * fatigue);
        return rounded(double(subtotal) * input.mAttackMultiplier);
    }
    void validateHandToHandSettings(const HandToHandSettings& settings)
    {
        for (float value : { settings.mSkillBase, settings.mSkillMultiplier, settings.mStrengthBase,
                 settings.mStrengthMultiplier, settings.mHealthMinimum, settings.mHealthMaximum,
                 settings.mFatigueBase, settings.mFatigueMultiplier })
            nonnegative(value);
        if (settings.mHealthMinimum > settings.mHealthMaximum)
            throw std::invalid_argument("reversed native hand damage range");
    }

    void validateBlockSettings(const BlockSettings& settings)
    {
        for (float value : { settings.mSkillBase, settings.mSkillMultiplier, settings.mMaximum,
                 settings.mWeaponMultiplier, settings.mUnarmedMultiplier })
            nonnegative(value);
        if (settings.mMaximum > 1.f)
            throw std::invalid_argument("native maximum block fraction exceeds one");
    }

    HandToHandDamage handToHandDamage(const HandToHandInput& input, const HandToHandSettings& settings,
        const PhysicalCombatSettings& physical)
    {
        validatePhysicalCombatSettings(physical);
        validateHandToHandSettings(settings);
        if (input.mStrength < 0)
            throw std::invalid_argument("negative native hand damage strength");
        constexpr double percent = static_cast<double>(0.01f);
        const float skill = rounded(settings.mSkillBase
            + skillValue(input.mSkill, input.mLuck, physical) * percent * settings.mSkillMultiplier);
        const float strength = rounded(settings.mStrengthBase
            + std::min(input.mStrength, 100) * percent * settings.mStrengthMultiplier);
        const float factor = std::min(1.f, rounded(double(strength) * skill * fatigueValue(input.mFatigueRatio, physical)));
        HandToHandDamage result;
        result.mHealth = rounded(settings.mHealthMinimum
            + (double(settings.mHealthMaximum) - settings.mHealthMinimum) * factor);
        result.mFatigue = input.mSuppressFatigueDamage ? 0.f
            : rounded(double(result.mHealth) * settings.mFatigueMultiplier + settings.mFatigueBase);
        return result;
    }

    float blockFraction(const BlockInput& input, const BlockSettings& settings, const PhysicalCombatSettings& physical)
    {
        validatePhysicalCombatSettings(physical);
        validateBlockSettings(settings);
        float equipment;
        switch (input.mEquipment)
        {
            case BlockEquipment::Shield: equipment = 1.f; break;
            case BlockEquipment::Weapon: equipment = settings.mWeaponMultiplier; break;
            case BlockEquipment::Unarmed: equipment = settings.mUnarmedMultiplier; break;
            default: throw std::invalid_argument("invalid native block equipment");
        }
        constexpr double percent = static_cast<double>(0.01f);
        const float skill = rounded(settings.mSkillBase
            + skillValue(input.mSkill, input.mLuck, physical) * percent * settings.mSkillMultiplier);
        return std::min(settings.mMaximum,
            rounded(double(fatigueValue(input.mFatigueRatio, physical)) * skill * equipment));
    }

    float difficultyDamage(float damage, float difficulty, float multiplier, PlayerDamageRole role)
    {
        nonnegative(damage);
        finite(difficulty);
        nonnegative(multiplier);
        if (difficulty < -1.f || difficulty > 1.f)
            throw std::invalid_argument("native difficulty outside normalized slider domain");
        if (role != PlayerDamageRole::Unaffected && role != PlayerDamageRole::Victim
            && role != PlayerDamageRole::Attacker)
            throw std::invalid_argument("invalid player damage role");
        if (role == PlayerDamageRole::Unaffected || difficulty == 0.f)
            return damage;
        const float scaled = rounded(double(difficulty) * multiplier);
        const float factor = difficulty < 0.f ? rounded(1.0 / rounded(1.0 - scaled)) : rounded(1.0 + scaled);
        return role == PlayerDamageRole::Victim ? rounded(double(damage) * factor) : rounded(double(damage) / factor);
    }
    void validateArmorRatingSettings(const ArmorRatingSettings& settings)
    {
        for (float value : { settings.mSkillBase, settings.mSkillMaximum,
                 settings.mConditionBase, settings.mConditionMultiplier })
            nonnegative(value);
        if (settings.mSkillBase > settings.mSkillMaximum)
            throw std::invalid_argument("reversed native armor skill range");
    }

    float armorRating(const ArmorRatingInput& input, const ArmorRatingSettings& settings,
        const PhysicalCombatSettings& physical)
    {
        validateArmorRatingSettings(settings);
        validatePhysicalCombatSettings(physical);
        nonnegative(input.mConditionRatio);
        // The native caller converts hundredths to integral armor units first.
        const auto base = input.mBaseHundredths / 100;
        const float range = rounded(double(settings.mSkillMaximum) - settings.mSkillBase);
        const float skill = skillValue(input.mSkill, input.mLuck, physical);
        const float scaled = rounded((settings.mSkillBase + double(skill) / 100.0 * range) * base);
        const float floored = std::max(1.f, std::floor(scaled));
        const float condition = rounded(settings.mConditionBase
            + double(input.mConditionRatio) * settings.mConditionMultiplier);
        return rounded(double(floored) * condition);
    }

    float capArmorRating(float total, float maximum)
    {
        nonnegative(total);
        nonnegative(maximum);
        return maximum == 0.f ? total : std::min(total, maximum);
    }
}
