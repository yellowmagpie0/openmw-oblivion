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

    void validatePhysicalCombatSettings(const PhysicalCombatSettings& settings)
    {
        for (const float value : { settings.mLuckSkillMultiplier, settings.mFatigueBase,
                 settings.mFatigueMultiplier, settings.mWeaponMultiplier, settings.mSkillBase,
                 settings.mSkillMultiplier, settings.mConditionBase, settings.mConditionMultiplier,
                 settings.mAttributeBase, settings.mAttributeMultiplier })
            nonnegative(value);
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
}
