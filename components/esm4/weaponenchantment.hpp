#ifndef OPENMW_ESM4_WEAPONENCHANTMENT_H
#define OPENMW_ESM4_WEAPONENCHANTMENT_H

#include <cstdint>
#include <optional>
#include <span>

namespace ESM4
{
    struct GameSetting;

    struct WeaponEnchantmentCostSettings
    {
        float mAreaMultiplier = .15f;
        float mDurationMultiplier = .1f;
        float mMagnitudeExponent = 1.25f;
        float mTargetMultiplier = 1.5f;
    };

    WeaponEnchantmentCostSettings buildWeaponEnchantmentCostSettings(
        std::span<const GameSetting* const> settings);

    // Caller resolves the winning MGEF flags/base cost. These are cost inputs,
    // not an assertion that the effect can execute in the current runtime.
    struct WeaponEnchantmentCostEffect
    {
        float mBaseCost;
        std::uint32_t mMagnitude;
        std::uint32_t mArea;
        std::uint32_t mDuration;
        std::uint32_t mRange;
        bool mNoMagnitude = false;
        bool mNoArea = false;
        bool mNoDuration = false;
        bool mNoCost = false;
    };

    // ENCH's cost getter ignores its actor argument. Manual cost bypasses the
    // effect list; autocalc sums per-effect integer costs. Ordinary numerical
    // behavior is reproduced with host pow, without a bit-identical CRT claim.
    float weaponEnchantmentCost(std::optional<std::uint32_t> manualCost,
        std::span<const WeaponEnchantmentCostEffect> effects,
        const WeaponEnchantmentCostSettings& settings);

    struct WeaponEnchantmentCharge
    {
        bool mAvailable;
        // Unavailable effects preserve an absent (-1) or fractional extra.
        // An admitted effect publishes current (or WEAP maximum) minus cost.
        std::optional<float> mAfter;
    };
    WeaponEnchantmentCharge weaponEnchantmentCharge(float current, float maximum, float cost);
}
#endif
