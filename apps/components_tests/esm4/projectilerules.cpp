#include <components/esm4/projectilerules.hpp>
#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadgmst.hpp>
#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
    const ESM4::ProjectileSettings projectile{.25f, .4f, 1500, .01f, .3f, .002f, 1.75f};
    const ESM4::PhysicalCombatSettings physical{-20, .4f, 1, .5f, .5f, .2f, 1.5f, .5f, .5f, .75f, .5f};
}

TEST(ESM4ProjectileRules, DrawUsesPlayerTimerAndCapsAtFull)
{
    for (const auto& c : {std::pair{0.f, .25f}, {1.f, .65f}, {1.875f, 1.f}, {2.f, 1.f}, {1000.f, 1.f}})
        EXPECT_NEAR(ESM4::bowDrawFraction(c.first, projectile), c.second, .000001f);
    auto settings = projectile;settings.mBowTimerBase = 0;settings.mBowTimerMultiplier = 1;
    EXPECT_EQ(ESM4::bowDrawFraction(0, settings), 0);
    EXPECT_LT(ESM4::bowDrawFraction(std::nextafter(1.f, 0.f), settings), 1);
    EXPECT_EQ(ESM4::bowDrawFraction(std::nextafter(1.f, 2.f), settings), 1);
}

TEST(ESM4ProjectileRules, DamageCombinesSeparatelyScaledBowAndAmmoAtLaunch)
{
    ESM4::ArrowDamageInput input{50, 50, 50, 20, 5, 1, 1, 1};
    // Full: (20*.5*.95*1) + (5*.5*.95*1) = 11.875.
    EXPECT_NEAR(ESM4::arrowLaunchDamage(input, physical), 11.875f, .00001f);
    input.mBowConditionRatio = .5f;
    EXPECT_NEAR(ESM4::arrowLaunchDamage(input, physical), 9.5f, .00001f);
    input.mDrawFraction = .5f;
    EXPECT_NEAR(ESM4::arrowLaunchDamage(input, physical), 4.75f, .00001f);
    input.mAttackBonus = 2; // Original entry damage adds this AV to each item.
    EXPECT_NEAR(ESM4::arrowLaunchDamage(input, physical), 6.75f, .00001f);
    input.mFatigueRatio = 0;
    EXPECT_NEAR(ESM4::arrowLaunchDamage(input, physical), 4.375f, .00001f);
    input.mDrawFraction = 0;
    EXPECT_EQ(ESM4::arrowLaunchDamage(input, physical), 0);
    input = {50, 50, 50, 0, 0, 1, 1, 1};
    EXPECT_EQ(ESM4::arrowLaunchDamage(input, physical), 0);
    input.mBowDamage = 1;input.mAmmoDamage = 1;
    EXPECT_NEAR(ESM4::arrowLaunchDamage(input, physical), .95f, .000001f);
    input.mBowDamage = 65535;input.mAmmoDamage = 65535;
    EXPECT_NEAR(ESM4::arrowLaunchDamage(input, physical), 62258.25f, .01f);
}

TEST(ESM4ProjectileRules, SpeedInterpolatesWeakAndFullDraw)
{
    for (const auto& c : {std::pair{0.f, 15.f}, {.25f, 386.25f}, {.5f, 757.5f}, {1.f, 1500.f}})
        EXPECT_NEAR(ESM4::arrowLaunchSpeed(1, c.first, projectile), c.second, .0001f);
    EXPECT_EQ(ESM4::arrowLaunchSpeed(0, 1, projectile), 0);
    EXPECT_EQ(ESM4::arrowLaunchSpeed(2, 1, projectile), 3000);
    EXPECT_LT(ESM4::arrowLaunchSpeed(1, std::nextafter(1.f, 0.f), projectile), 1500);
}

TEST(ESM4ProjectileRules, GravityUsesLuckAdjustedMarksmanAndDraw)
{
    for (const auto& c : {std::pair{0, .3f}, {1, .298f}, {50, .2f}, {99, .102f}, {100, .1f}, {101, .1f}})
        EXPECT_NEAR(ESM4::arrowGravityFactor(c.first, 50, 1, projectile, physical), c.second, .000001f);
    EXPECT_NEAR(ESM4::arrowGravityFactor(50, 0, 1, projectile, physical), .24f, .000001f);
    EXPECT_NEAR(ESM4::arrowGravityFactor(50, 100, 1, projectile, physical), .16f, .000001f);
    EXPECT_EQ(ESM4::arrowGravityFactor(50, 50, 0, projectile, physical), 1.75f);
    EXPECT_NEAR(ESM4::arrowGravityFactor(50, 50, .5f, projectile, physical), .975f, .000001f);
    auto settings = projectile;settings.mGravityBase = 0;
    EXPECT_EQ(ESM4::arrowGravityFactor(50, 50, 1, settings, physical), 0);
    // Float storage creates a one-step plateau: the first smaller draw still
    // rounds to .975f; two steps down cross the next gravity float midpoint.
    const float below = std::nextafter(.5f, 0.f);
    EXPECT_EQ(ESM4::arrowGravityFactor(50, 50, below, projectile, physical), .975f);
    EXPECT_GT(ESM4::arrowGravityFactor(50, 50, std::nextafter(below, 0.f), projectile, physical), .975f);
}

TEST(ESM4ProjectileRules, RejectsInvalidInputsAndOverflow)
{
    ESM4::ArrowDamageInput input{50, 50, 50, 20, 5, 1, 1, 1};
    for (float bad : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        EXPECT_THROW(ESM4::bowDrawFraction(bad, projectile), std::invalid_argument);
        EXPECT_THROW(ESM4::arrowLaunchSpeed(bad, 1, projectile), std::invalid_argument);
        EXPECT_THROW(ESM4::arrowLaunchSpeed(1, bad, projectile), std::invalid_argument);
        EXPECT_THROW(ESM4::arrowGravityFactor(50, 50, bad, projectile, physical), std::invalid_argument);
        input.mDrawFraction = bad;
        EXPECT_THROW(ESM4::arrowLaunchDamage(input, physical), std::invalid_argument);
        for (float ESM4::ProjectileSettings::* member : {&ESM4::ProjectileSettings::mBowTimerBase,
                 &ESM4::ProjectileSettings::mBowTimerMultiplier, &ESM4::ProjectileSettings::mSpeedMultiplier,
                 &ESM4::ProjectileSettings::mWeakSpeed, &ESM4::ProjectileSettings::mGravityBase,
                 &ESM4::ProjectileSettings::mGravityMultiplier, &ESM4::ProjectileSettings::mWeakGravity})
        {
            auto settings = projectile;settings.*member = bad;
            EXPECT_THROW(ESM4::validateProjectileSettings(settings), std::invalid_argument);
        }
    }
    const float above = std::nextafter(1.f, 2.f);
    EXPECT_THROW(ESM4::arrowLaunchSpeed(1, above, projectile), std::invalid_argument);
    EXPECT_THROW(ESM4::arrowGravityFactor(50, 50, above, projectile, physical), std::invalid_argument);
    input.mDrawFraction = above;
    EXPECT_THROW(ESM4::arrowLaunchDamage(input, physical), std::invalid_argument);
    EXPECT_THROW(ESM4::arrowLaunchSpeed(std::numeric_limits<float>::max(), 1, projectile), std::invalid_argument);
}

TEST(ESM4ProjectileRules, SettingsUseTypedNativeDefaultsAndOverrides)
{
    const auto defaults = ESM4::buildProjectileSettings({});
    EXPECT_EQ(defaults.mBowTimerBase, .25f);
    EXPECT_EQ(defaults.mBowTimerMultiplier, .4f);
    EXPECT_EQ(defaults.mSpeedMultiplier, 1500);
    EXPECT_EQ(defaults.mWeakSpeed, .01f);
    EXPECT_EQ(defaults.mGravityBase, .3f);
    EXPECT_EQ(defaults.mGravityMultiplier, .002f);
    EXPECT_EQ(defaults.mWeakGravity, 1.75f);
    ESM4::GameSetting value{};value.mEditorId = "fArrowSpeedMult";value.mData = 2000.f;
    const std::array<const ESM4::GameSetting*, 1> values{&value};
    EXPECT_EQ(ESM4::buildProjectileSettings(values).mSpeedMultiplier, 2000);
    value.mData = std::int32_t{2000};
    EXPECT_THROW(ESM4::buildProjectileSettings(values), std::invalid_argument);
}
