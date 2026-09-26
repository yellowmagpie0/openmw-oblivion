#include "projectilerules.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    namespace
    {
        void nonnegative(float value)
        {
            if (!std::isfinite(value) || value < 0)
                throw std::invalid_argument("invalid native projectile input");
        }
        void fraction(float value)
        {
            nonnegative(value);
            if (value > 1)
                throw std::invalid_argument("invalid native projectile draw fraction");
        }
        float rounded(double value)
        {
            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
                throw std::invalid_argument("native projectile arithmetic overflow");
            return static_cast<float>(value);
        }
    }

    void validateArrowRecoverySettings(const ArrowRecoverySettings& settings)
    {
        if (settings.mInventoryChance < 0 || settings.mInventoryChance > 100)
            throw std::invalid_argument("invalid native arrow recovery percentage");
    }

    ArrowInventoryRecoveryResult arrowInventoryRecovery(bool arrowEnchanted, unsigned draw,
        const ArrowRecoverySettings& settings)
    {
        validateArrowRecoverySettings(settings);
        if (draw >= 100)
            throw std::invalid_argument("invalid native arrow recovery draw");
        if (arrowEnchanted)
            return {};
        return {true, draw < static_cast<unsigned>(settings.mInventoryChance)};
    }

    void validateArrowLifetimeSettings(const ArrowLifetimeSettings& settings)
    {
        nonnegative(settings.mMaximumAge);
    }

    ArrowLifetimeChange advanceArrowLifetime(const ArrowLifetimeState& state, float duration,
        const ArrowLifetimeSettings& settings)
    {
        validateArrowLifetimeSettings(settings);
        nonnegative(state.mAge);
        fraction(state.mOpacity);
        nonnegative(duration);
        ArrowLifetimeChange result{state, false};
        result.mState.mAge = rounded(double(state.mAge) + duration);
        result.mState.mFading = state.mFading || result.mState.mAge > settings.mMaximumAge;
        if (result.mState.mFading)
        {
            // The original fade divisor is a double constant, not a GMST.
            result.mState.mOpacity = std::max(0.f, rounded(state.mOpacity - double(duration) / 3.0));
            result.mRemove = result.mState.mOpacity == 0;
        }
        return result;
    }

    void validateProjectileSettings(const ProjectileSettings& settings)
    {
        for (float value : {settings.mBowTimerBase, settings.mBowTimerMultiplier, settings.mSpeedMultiplier,
                 settings.mWeakSpeed, settings.mGravityBase, settings.mGravityMultiplier, settings.mWeakGravity})
            nonnegative(value);
    }

    void validateBowFatigueSettings(const BowFatigueSettings& settings)
    {
        nonnegative(settings.mHoldPerSecond);
        nonnegative(settings.mPerShot);
    }

    float bowHoldFatigue(std::int32_t marksman, bool player, bool holding, float duration,
        const BowFatigueSettings& settings, const CombatMasterySettings& mastery)
    {
        validateBowFatigueSettings(settings);
        nonnegative(duration);
        const auto rank = combatMastery(marksman, mastery);
        return player && holding && rank == CombatMastery::Novice
            ? rounded(double(settings.mHoldPerSecond) * duration) : 0.f;
    }

    float bowShotFatigue(std::int32_t marksman, const BowFatigueSettings& settings,
        const CombatMasterySettings& mastery)
    {
        validateBowFatigueSettings(settings);
        return combatMastery(marksman, mastery) == CombatMastery::Novice ? settings.mPerShot : 0.f;
    }

    float bowDrawFraction(float timer, const ProjectileSettings& settings)
    {
        validateProjectileSettings(settings);
        nonnegative(timer);
        return std::min(1.f, rounded(settings.mBowTimerBase + double(timer) * settings.mBowTimerMultiplier));
    }

    float arrowLaunchDamage(const ArrowDamageInput& input, const PhysicalCombatSettings& settings)
    {
        fraction(input.mDrawFraction);
        const WeaponDamageInput bow{input.mMarksman, input.mLuck, input.mAgility, input.mBowDamage,
            input.mBowConditionRatio, input.mFatigueRatio};
        const WeaponDamageInput ammo{input.mMarksman, input.mLuck, input.mAgility, input.mAmmoDamage,
            1.f, input.mFatigueRatio};
        const float bowDamage = rounded(double(weaponDamage(bow, settings)) + input.mAttackBonus);
        const float ammoDamage = rounded(double(weaponDamage(ammo, settings)) + input.mAttackBonus);
        const float combined = rounded(double(bowDamage) + ammoDamage);
        return rounded(double(combined) * input.mDrawFraction);
    }

    float arrowLaunchSpeed(float ammoSpeed, float drawFraction, const ProjectileSettings& settings)
    {
        validateProjectileSettings(settings);
        nonnegative(ammoSpeed);
        fraction(drawFraction);
        const float fullSpeed = rounded(double(ammoSpeed) * settings.mSpeedMultiplier);
        return rounded(double(fullSpeed) * (drawFraction + (1.0 - drawFraction) * settings.mWeakSpeed));
    }

    float arrowGravityFactor(std::int32_t marksman, std::int32_t luck, float drawFraction,
        const ProjectileSettings& settings, const PhysicalCombatSettings& physical)
    {
        validateProjectileSettings(settings);
        fraction(drawFraction);
        const float skill = effectiveCombatSkill(marksman, luck, physical);
        const float fullGravity = rounded(settings.mGravityBase - double(skill) * settings.mGravityMultiplier);
        return std::max(0.f, rounded(double(fullGravity) * drawFraction + (1.0 - drawFraction) * settings.mWeakGravity));
    }
}
