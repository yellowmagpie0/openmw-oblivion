#ifndef OPENMW_ESM4_PROJECTILERULES_H
#define OPENMW_ESM4_PROJECTILERULES_H

#include "physicalcombat.hpp"

namespace ESM4
{
    struct ProjectileSettings
    {
        float mBowTimerBase;
        float mBowTimerMultiplier;
        float mSpeedMultiplier;
        float mWeakSpeed;
        float mGravityBase;
        float mGravityMultiplier;
        float mWeakGravity;
    };

    struct ArrowDamageInput
    {
        std::int32_t mMarksman;
        std::int32_t mLuck;
        std::int32_t mAgility;
        std::uint16_t mBowDamage;
        std::uint16_t mAmmoDamage;
        float mBowConditionRatio;
        float mFatigueRatio;
        float mDrawFraction;
        std::int32_t mAttackBonus = 0; // Caller-resolved AV; no effect execution here.
    };

    // Original launch arithmetic only. The controller owns timer accumulation,
    // release eligibility, ammo consumption, launch transform and collisions.
    float bowDrawFraction(float timer, const ProjectileSettings& settings);
    float arrowLaunchDamage(const ArrowDamageInput& input, const PhysicalCombatSettings& settings);
    float arrowLaunchSpeed(float ammoSpeed, float drawFraction, const ProjectileSettings& settings);
    float arrowGravityFactor(std::int32_t marksman, std::int32_t luck, float drawFraction,
        const ProjectileSettings& settings, const PhysicalCombatSettings& physical);
    void validateProjectileSettings(const ProjectileSettings& settings);
}

#endif
