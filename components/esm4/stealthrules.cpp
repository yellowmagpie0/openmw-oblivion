#include "stealthrules.hpp"

#include <cmath>
#include <stdexcept>

namespace ESM4
{
    void validateSneakAttackSettings(const SneakAttackSettings& settings)
    {
        for (const auto* values : {&settings.mMeleeMultipliers, &settings.mMarksmanMultipliers})
            for (float value : *values)
                if (!std::isfinite(value) || value < 0)
                    throw std::invalid_argument("invalid native sneak attack multiplier");
    }

    SneakAttackResult sneakAttack(const SneakAttackInput& input, const SneakAttackSettings& settings,
        const CombatMasterySettings& mastery)
    {
        validateSneakAttackSettings(settings);
        const auto rank = combatMastery(input.mBaseSneak, mastery);
        if (input.mWeaponType < -1 || input.mWeaponType > 5)
            throw std::invalid_argument("invalid native sneak attack weapon type");
        if (!input.mNpcAttacker || !input.mSneaking || input.mSwimming || input.mVictimDetection > 0
            || (input.mVictimCombatTarget && input.mVictimDetection > settings.mCombatMinimumDetection))
            return {};
        const auto index = static_cast<std::size_t>(rank);
        float multiplier = 1.f;
        if (input.mWeaponType == -1 || input.mWeaponType == 0 || input.mWeaponType == 2)
            multiplier = settings.mMeleeMultipliers[index];
        else if (input.mWeaponType == 5)
            multiplier = settings.mMarksmanMultipliers[index];
        const bool bypass = rank == CombatMastery::Master && multiplier > 1.f;
        return {multiplier, bypass, bypass};
    }
}
