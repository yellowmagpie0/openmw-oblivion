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

TEST(ESM4ProjectileRules, BowFatigueUsesNoviceRankAndPlayerHoldAction)
{
    const ESM4::BowFatigueSettings settings{15, 5};
    const ESM4::CombatMasterySettings mastery{{25, 50, 75, 100}};
    for (int skill : {-1, 0, 5, 19, 20, 24, 25, 49, 50, 74, 75, 99, 100, 101})
    {
        SCOPED_TRACE(skill);
        const bool novice = skill < 25;
        EXPECT_EQ(ESM4::bowShotFatigue(skill, settings, mastery), novice ? 5 : 0);
        for (bool player : {false, true})
            for (bool holding : {false, true})
                for (float duration : {0.f, .5f, 1.f, 4.f})
                    EXPECT_EQ(ESM4::bowHoldFatigue(skill, player, holding, duration, settings, mastery),
                        novice && player && holding ? 15 * duration : 0);
    }
    // Uses mastery thresholds, not the unused iMarksmanFatigueBurnPerSecondSkill (20).
    const ESM4::CombatMasterySettings custom{{10, 30, 60, 90}};
    EXPECT_EQ(ESM4::bowHoldFatigue(9, true, true, 2, settings, custom), 30);
    EXPECT_EQ(ESM4::bowHoldFatigue(10, true, true, 2, settings, custom), 0);
    EXPECT_EQ(ESM4::bowShotFatigue(9, settings, custom), 5);
    EXPECT_EQ(ESM4::bowShotFatigue(10, settings, custom), 0);
}

TEST(ESM4ProjectileRules, BowFatigueRejectsInvalidInputsAndOverflow)
{
    const ESM4::CombatMasterySettings mastery{{25, 50, 75, 100}};
    for (float bad : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        EXPECT_THROW(ESM4::bowHoldFatigue(0, true, true, bad, {15, 5}, mastery), std::invalid_argument);
        for (const auto settings : {ESM4::BowFatigueSettings{bad, 5}, ESM4::BowFatigueSettings{15, bad}})
        {
            EXPECT_THROW(ESM4::validateBowFatigueSettings(settings), std::invalid_argument);
            EXPECT_THROW(ESM4::bowShotFatigue(100, settings, mastery), std::invalid_argument);
            EXPECT_THROW(ESM4::bowHoldFatigue(100, false, false, 0, settings, mastery), std::invalid_argument);
        }
    }
    EXPECT_THROW(ESM4::bowHoldFatigue(0, true, true, std::numeric_limits<float>::max(), {15, 5}, mastery),
        std::invalid_argument);
    EXPECT_THROW(ESM4::bowShotFatigue(0, {15, 5}, {{25, 20, 75, 100}}), std::invalid_argument);
    EXPECT_EQ(ESM4::bowShotFatigue(0, {0, 0}, mastery), 0);
    EXPECT_EQ(ESM4::bowHoldFatigue(0, true, true, 1, {0, 0}, mastery), 0);
}

TEST(ESM4ProjectileRules, BowFatigueSettingsUseTypedDefaultsAndOverrides)
{
    const auto defaults = ESM4::buildBowFatigueSettings({});
    EXPECT_EQ(defaults.mHoldPerSecond, 15);
    EXPECT_EQ(defaults.mPerShot, 5);
    ESM4::GameSetting value{};value.mEditorId = "fMarksmanFatigueBurnPerSecond";value.mData = 3.f;
    const std::array<const ESM4::GameSetting*, 1> values{&value};
    EXPECT_EQ(ESM4::buildBowFatigueSettings(values).mHoldPerSecond, 3);
    value.mEditorId = "fMarksmanFatigueBurnPerShot";
    EXPECT_EQ(ESM4::buildBowFatigueSettings(values).mPerShot, 3);
    value.mData = std::int32_t{3};
    EXPECT_THROW(ESM4::buildBowFatigueSettings(values), std::invalid_argument);
}

TEST(ESM4ProjectileRules, OriginalFirstArrowUsesFatigueAfterHold)
{
    // Original-14/BOW-02: default Imperial, normal pickup/equip, fresh target.
    // Paused draw fatigue is 119.58/140, observed health 500 -> 486.26.
    // The .05 bound was declared before release for readback/transition time;
    // exact release fatigue and frame timing were not measured.
    ESM4::ArrowDamageInput input{5, 50, 30, 100, 20, 1, 119.58f / 140.f, 1};
    EXPECT_NEAR(500 - ESM4::arrowLaunchDamage(input, physical), 486.26f, .05f);
    input.mFatigueRatio = 1;
    EXPECT_GT(std::abs(500 - ESM4::arrowLaunchDamage(input, physical) - 486.26f), 1.f);
    input.mFatigueRatio = (119.58f - 5) / 140.f;
    EXPECT_GT(std::abs(500 - ESM4::arrowLaunchDamage(input, physical) - 486.26f), .2f);
}
