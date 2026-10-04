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

TEST(ESM4ProjectileRules, LifetimeStartsFadingStrictlyAfterMaximumStoredAge)
{
    const ESM4::ArrowLifetimeSettings settings{90};
    const auto exact = ESM4::advanceArrowLifetime({89, 1, false}, 1, settings);
    EXPECT_EQ(exact.mState.mAge, 90);
    EXPECT_EQ(exact.mState.mOpacity, 1);
    EXPECT_FALSE(exact.mState.mFading);
    EXPECT_FALSE(exact.mRemove);
    // Age is stored before comparison: an increment below half a float step
    // does not start fading. The full crossing tick also advances the fade.
    const auto plateau = ESM4::advanceArrowLifetime(exact.mState, 0.000001f, settings);
    EXPECT_EQ(plateau.mState.mAge, 90);
    EXPECT_FALSE(plateau.mState.mFading);
    const auto crossed = ESM4::advanceArrowLifetime(exact.mState, .75f, settings);
    EXPECT_EQ(crossed.mState.mAge, 90.75f);
    EXPECT_EQ(crossed.mState.mOpacity, .75f);
    EXPECT_TRUE(crossed.mState.mFading);
    EXPECT_FALSE(crossed.mRemove);
    const auto adjacent = ESM4::advanceArrowLifetime({std::nextafter(90.f, 100.f), 1, false}, 0, settings);
    EXPECT_TRUE(adjacent.mState.mFading);
    EXPECT_EQ(adjacent.mState.mOpacity, 1);
    EXPECT_FALSE(ESM4::advanceArrowLifetime({std::nextafter(90.f, 0.f), 1, false}, 0, settings).mState.mFading);
    EXPECT_FALSE(ESM4::advanceArrowLifetime({}, 0, {0}).mState.mFading);
    EXPECT_TRUE(ESM4::advanceArrowLifetime({}, 1, {0}).mState.mFading);
}

TEST(ESM4ProjectileRules, LifetimeFadeTakesThreeSecondsAndRemovesAtZero)
{
    const auto early = ESM4::advanceArrowLifetime({1, 1, true}, .75f, {90});
    EXPECT_EQ(early.mState.mAge, 1.75f);
    EXPECT_EQ(early.mState.mOpacity, .75f);
    EXPECT_TRUE(early.mState.mFading);
    EXPECT_FALSE(early.mRemove);
    for (float duration : {2.25f, std::nextafter(2.25f, 3.f), 1000.f})
    {
        const auto removed = ESM4::advanceArrowLifetime(early.mState, duration, {90});
        EXPECT_EQ(removed.mState.mOpacity, 0);
        EXPECT_TRUE(removed.mState.mFading);
        EXPECT_TRUE(removed.mRemove);
    }
    const auto before = ESM4::advanceArrowLifetime(early.mState, std::nextafter(2.25f, 0.f), {90});
    EXPECT_GT(before.mState.mOpacity, 0);
    EXPECT_FALSE(before.mRemove);
    // Opacity alone does not put a live arrow into the fading lifecycle.
    EXPECT_FALSE(ESM4::advanceArrowLifetime({0, 0, false}, 0, {90}).mRemove);
    EXPECT_TRUE(ESM4::advanceArrowLifetime({0, 0, true}, 0, {90}).mRemove);
    const auto longTick = ESM4::advanceArrowLifetime({89, 1, false}, 3, {90});
    EXPECT_TRUE(longTick.mRemove);
    EXPECT_EQ(longTick.mState.mOpacity, 0);
}

TEST(ESM4ProjectileRules, LifetimeRejectsInvalidInputsBeforeReturningChange)
{
    for (float bad : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        EXPECT_THROW(ESM4::advanceArrowLifetime({bad, 1, false}, 0, {90}), std::invalid_argument);
        EXPECT_THROW(ESM4::advanceArrowLifetime({0, bad, false}, 0, {90}), std::invalid_argument);
        EXPECT_THROW(ESM4::advanceArrowLifetime({}, bad, {90}), std::invalid_argument);
        EXPECT_THROW(ESM4::advanceArrowLifetime({}, 0, {bad}), std::invalid_argument);
    }
    EXPECT_THROW(ESM4::advanceArrowLifetime({0, std::nextafter(1.f, 2.f), false}, 0, {90}), std::invalid_argument);
    const float maximum = std::numeric_limits<float>::max();
    EXPECT_THROW(ESM4::advanceArrowLifetime({maximum, 1, false}, maximum, {maximum}), std::invalid_argument);
}

TEST(ESM4ProjectileRules, LifetimeSettingsUseTypedDefaultAndOverrides)
{
    EXPECT_EQ(ESM4::buildArrowLifetimeSettings({}).mMaximumAge, 90);
    ESM4::GameSetting value{};
    value.mEditorId = "fArrowAgeMax";
    value.mData = 30.f;
    const std::array<const ESM4::GameSetting*, 1> values{&value};
    EXPECT_EQ(ESM4::buildArrowLifetimeSettings(values).mMaximumAge, 30);
    value.mData = 0.f;
    EXPECT_EQ(ESM4::buildArrowLifetimeSettings(values).mMaximumAge, 0);
    value.mData = std::int32_t{30};
    EXPECT_THROW(ESM4::buildArrowLifetimeSettings(values), std::invalid_argument);
    value.mData = -1.f;
    EXPECT_THROW(ESM4::buildArrowLifetimeSettings(values), std::invalid_argument);
}

TEST(ESM4ProjectileRules, InventoryRecoveryRollIsStrictAndArrowEnchantmentSuppressesDraw)
{
    for (int chance : {0, 1, 49, 50, 99, 100})
        for (unsigned draw = 0; draw < 100; ++draw)
            for (bool enchanted : {false, true})
            {
                SCOPED_TRACE(chance);
                SCOPED_TRACE(draw);
                SCOPED_TRACE(enchanted);
                const auto result = ESM4::arrowInventoryRecovery(enchanted, draw, {chance});
                EXPECT_EQ(result.mConsumesDraw, !enchanted);
                EXPECT_EQ(result.mRecover, !enchanted && draw < static_cast<unsigned>(chance));
            }
}

TEST(ESM4ProjectileRules, InventoryRecoveryRejectsInvalidProbabilityAndDraw)
{
    for (int chance : {-1, 101, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()})
        for (bool enchanted : {false, true})
            EXPECT_THROW(ESM4::arrowInventoryRecovery(enchanted, 0, {chance}), std::invalid_argument);
    for (unsigned draw : {100u, std::numeric_limits<unsigned>::max()})
        for (bool enchanted : {false, true})
            EXPECT_THROW(ESM4::arrowInventoryRecovery(enchanted, draw, {50}), std::invalid_argument);
}

TEST(ESM4ProjectileRules, InventoryRecoveryFixedSeedDistribution)
{
    // Predeclared 100,000 draws, native 15-bit samples modulo 100, seed 0x4d15a3.
    // Allow +/-600 around nominal 50%; deterministic boundaries are tested above.
    std::uint32_t random = 0x4d15a3;
    unsigned recovered = 0;
    for (unsigned i = 0; i < 100000; ++i)
    {
        random = random * 0x343FDu + 0x269EC3u;
        const unsigned draw = ((random >> 16) & 0x7fff) % 100;
        recovered += ESM4::arrowInventoryRecovery(false, draw, {50}).mRecover;
    }
    EXPECT_GE(recovered, 49400u);
    EXPECT_LE(recovered, 50600u);
}

TEST(ESM4ProjectileRules, InventoryRecoverySettingsUseTypedDefaultAndOverrides)
{
    EXPECT_EQ(ESM4::buildArrowRecoverySettings({}).mInventoryChance, 50);
    ESM4::GameSetting value{};
    value.mEditorId = "iArrowInventoryChance";
    value.mData = std::int32_t{25};
    const std::array<const ESM4::GameSetting*, 1> values{&value};
    EXPECT_EQ(ESM4::buildArrowRecoverySettings(values).mInventoryChance, 25);
    value.mData = 25.f;
    EXPECT_THROW(ESM4::buildArrowRecoverySettings(values), std::invalid_argument);
    value.mData = std::int32_t{101};
    EXPECT_THROW(ESM4::buildArrowRecoverySettings(values), std::invalid_argument);
}

TEST(ESM4ProjectileRules, CleanupStartsAboveLimitAndPrefersFirstPoolOverOlderFallback)
{
    const std::array candidates{ESM4::ArrowCleanupCandidate{80, true, false},
        ESM4::ArrowCleanupCandidate{1, true, true}, ESM4::ArrowCleanupCandidate{90, false, true}};
    for (int count : {0, 1, 14, 15})
        EXPECT_FALSE(ESM4::selectArrowForCleanup(count, candidates, {15}));
    EXPECT_EQ(ESM4::selectArrowForCleanup(16, candidates, {15}), 1u);
    EXPECT_EQ(ESM4::selectArrowForCleanup(100, candidates, {15}), 1u);
    EXPECT_FALSE(ESM4::selectArrowForCleanup(0, candidates, {0}));
    EXPECT_EQ(ESM4::selectArrowForCleanup(1, candidates, {0}), 1u);
    EXPECT_FALSE(ESM4::selectArrowForCleanup(100, {}, {15}));
}

TEST(ESM4ProjectileRules, CleanupSelectsOldestPositiveAgeSettledArrowWithStableTies)
{
    std::array candidates{ESM4::ArrowCleanupCandidate{50, false, true},
        ESM4::ArrowCleanupCandidate{0, true, true}, ESM4::ArrowCleanupCandidate{2, true, false},
        ESM4::ArrowCleanupCandidate{3, true, false}, ESM4::ArrowCleanupCandidate{3, true, false}};
    EXPECT_EQ(ESM4::selectArrowForCleanup(16, candidates, {15}), 3u);
    candidates[4].mAge = std::nextafter(3.f, 4.f);
    EXPECT_EQ(ESM4::selectArrowForCleanup(16, candidates, {15}), 4u);
    candidates[1].mAge = std::numeric_limits<float>::denorm_min();
    EXPECT_EQ(ESM4::selectArrowForCleanup(16, candidates, {15}), 1u);
    candidates[0].mSettled = true;
    EXPECT_EQ(ESM4::selectArrowForCleanup(16, candidates, {15}), 0u);
    for (auto& candidate : candidates)
        candidate.mSettled = false;
    EXPECT_FALSE(ESM4::selectArrowForCleanup(100, candidates, {15}));
}

TEST(ESM4ProjectileRules, CleanupRejectsMalformedCountsAndAges)
{
    EXPECT_THROW(ESM4::selectArrowForCleanup(-1, {}, {15}), std::invalid_argument);
    EXPECT_THROW(ESM4::selectArrowForCleanup(0, {}, {-1}), std::invalid_argument);
    for (float bad : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        const std::array candidates{ESM4::ArrowCleanupCandidate{bad, false, false}};
        EXPECT_THROW(ESM4::selectArrowForCleanup(0, candidates, {15}), std::invalid_argument);
    }
}

TEST(ESM4ProjectileRules, CleanupSettingsUseTypedDefaultAndOverrides)
{
    EXPECT_EQ(ESM4::buildArrowCleanupSettings({}).mMaximumReferences, 15);
    ESM4::GameSetting value{};
    value.mEditorId = "iArrowMaxRefCount";
    value.mData = std::int32_t{0};
    const std::array<const ESM4::GameSetting*, 1> values{&value};
    EXPECT_EQ(ESM4::buildArrowCleanupSettings(values).mMaximumReferences, 0);
    value.mData = std::numeric_limits<std::int32_t>::max();
    EXPECT_EQ(ESM4::buildArrowCleanupSettings(values).mMaximumReferences, std::numeric_limits<std::int32_t>::max());
    value.mData = 15.f;
    EXPECT_THROW(ESM4::buildArrowCleanupSettings(values), std::invalid_argument);
    value.mData = std::int32_t{-1};
    EXPECT_THROW(ESM4::buildArrowCleanupSettings(values), std::invalid_argument);
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

TEST(ESM4ProjectileRules, BowKeysUseAllFiveStockPhasesAndIgnoreSoundDispatch)
{
    const std::array<ESM4::MeleeTextKey, 7> keys{{
        {0, "start"}, {.03333330154418945f, "Sound: WPNBowDraw"},
        {.2666666507720947f, "Attach"}, {1.3666667938232422f, "Hold"},
        {1.433333396911621f, "Release"}, {1.4333434104919434f, "Sound: bowShoot"},
        {1.9666666984558105f, "end"}}};
    const auto result = ESM4::bowAnimationKeyTimes(keys);
    EXPECT_EQ(result.mMatchedCount, 5);
    EXPECT_EQ(result.mTimes, (std::array<float, 5>{0, .2666666507720947f,
        1.3666667938232422f, 1.433333396911621f, 1.9666666984558105f}));
}

TEST(ESM4ProjectileRules, BowKeysKeepAuthoredOrderAndPrefixMatching)
{
    const std::array<ESM4::MeleeTextKey, 8> keys{{
        {.1f, "Attach"}, {.2f, " START"}, {.3f, "STARTer"}, {.4f, "Start"},
        {.5f, "aTtAcH"}, {.6f, "Hold"}, {.7f, "Release"}, {.8f, "End"}}};
    const auto result = ESM4::bowAnimationKeyTimes(keys);
    EXPECT_EQ(result.mMatchedCount, 5);
    EXPECT_EQ(result.mTimes, (std::array<float, 5>{.3f, .5f, .6f, .7f, .8f}));
}

TEST(ESM4ProjectileRules, BowKeysFollowNativeLineAndNulBoundaries)
{
    const std::array<ESM4::MeleeTextKey, 1> lines{{
        {.25f, "\r\nStart\r\nAttach\nHold\nRelease\r\nEnd"}}};
    EXPECT_EQ(ESM4::bowAnimationKeyTimes(lines).mMatchedCount, 5);
    const std::array<ESM4::MeleeTextKey, 1> bare{{
        {.25f, "Start\rAttach\rHold\rRelease\rEnd"}}};
    EXPECT_EQ(ESM4::bowAnimationKeyTimes(bare).mMatchedCount, 1);
    const std::array<ESM4::MeleeTextKey, 1> nul{{
        {.25f, std::string_view("Start\0\nAttach", 13)}}};
    EXPECT_EQ(ESM4::bowAnimationKeyTimes(nul).mMatchedCount, 1);
    EXPECT_EQ(ESM4::bowAnimationKeyTimes({}).mMatchedCount, 0);
}

TEST(ESM4ProjectileRules, BowKeysAdvanceUnstoredTimesAndPreserveSignedZero)
{
    const std::array<ESM4::MeleeTextKey, 5> keys{{
        {-2, "Start"}, {-1, "Attach"}, {std::nextafter(-1.f, 0.f), "Hold"},
        {-0.f, "Release"}, {std::numeric_limits<float>::denorm_min(), "End"}}};
    const auto result = ESM4::bowAnimationKeyTimes(keys);
    EXPECT_EQ(result.mMatchedCount, 5);
    EXPECT_EQ(result.mTimes[0], 0);
    EXPECT_EQ(result.mTimes[1], 0);
    EXPECT_EQ(result.mTimes[2], keys[2].mTime);
    EXPECT_TRUE(std::signbit(result.mTimes[3]));
    EXPECT_EQ(result.mTimes[4], keys[4].mTime);
}

TEST(ESM4ProjectileRules, BowKeysRejectNonfiniteAndUnsupportedPostEndText)
{
    const std::array<ESM4::MeleeTextKey, 1> invalid{{
        {std::numeric_limits<float>::infinity(), "unrelated"}}};
    EXPECT_THROW(ESM4::bowAnimationKeyTimes(invalid), std::invalid_argument);
    const std::array<ESM4::MeleeTextKey, 1> trailing{{
        {0, "Start\nAttach\nHold\nRelease\nEnd\nEnd"}}};
    EXPECT_THROW(ESM4::bowAnimationKeyTimes(trailing), std::invalid_argument);
    const std::array<ESM4::MeleeTextKey, 1> sound{{
        {0, "Start\nAttach\nHold\nRelease\nEnd\nSound: bowShoot"}}};
    EXPECT_EQ(ESM4::bowAnimationKeyTimes(sound).mMatchedCount, 5);
}

TEST(ESM4ProjectileRules, BowPhaseAdvancesStrictlyPastOneKeyPerCall)
{
    using Phase = ESM4::BowAnimationPhase;
    const std::array<float, 5> keys{0, .25f, 1.25f, 1.5f, 2};
    EXPECT_EQ(ESM4::advanceBowAnimation({}, .25f, keys).mPhase, Phase::Start);
    EXPECT_EQ(ESM4::advanceBowAnimation({}, std::nextafter(.25f, 1.f), keys).mPhase, Phase::Attach);
    EXPECT_EQ(ESM4::advanceBowAnimation({}, 100, keys).mPhase, Phase::Attach);
    EXPECT_EQ(ESM4::advanceBowAnimation({Phase::End, 0}, 100, keys).mPhase, Phase::End);
}

TEST(ESM4ProjectileRules, BowPhaseHoldChangesUpperBodyOffsetOnlyOnEntry)
{
    using Phase = ESM4::BowAnimationPhase;
    const std::array<float, 5> keys{0, .25f, 1.25f, 1.5f, 2};
    const ESM4::BowAnimationProgress previous{Phase::Attach, .1f};
    const auto upper = ESM4::advanceBowAnimation(previous, 1.5f, keys);
    EXPECT_EQ(upper.mPhase, Phase::Hold);
    EXPECT_EQ(upper.mSequenceOffset, -.25f);
    const auto lower = ESM4::advanceBowAnimation(previous, 1.5f, keys, false);
    EXPECT_EQ(lower.mPhase, Phase::Hold);
    EXPECT_EQ(lower.mSequenceOffset, .1f);
    const auto held = ESM4::advanceBowAnimation(upper, 1.25f, keys);
    EXPECT_EQ(held.mPhase, Phase::Hold);
    EXPECT_EQ(held.mSequenceOffset, -.25f);
    EXPECT_EQ(previous.mSequenceOffset, .1f);
}

TEST(ESM4ProjectileRules, BowPhaseRoundsCombinedTimeBeforeComparing)
{
    using Phase = ESM4::BowAnimationPhase;
    const std::array<float, 5> keys{0, 1, 2, 3, 4};
    const auto plateau = ESM4::advanceBowAnimation({Phase::Start, 1}, 0x1p-24f, keys);
    EXPECT_EQ(plateau.mPhase, Phase::Start);
    const auto crossed = ESM4::advanceBowAnimation({Phase::Start, 1}, 0x1.8p-24f, keys);
    EXPECT_EQ(crossed.mPhase, Phase::Attach);
}

TEST(ESM4ProjectileRules, BowPhaseRejectsInvalidStateAndStoredOverflow)
{
    using Phase = ESM4::BowAnimationPhase;
    std::array<float, 5> keys{0, .25f, 1.25f, 1.5f, 2};
    EXPECT_THROW(ESM4::advanceBowAnimation({static_cast<Phase>(5), 0}, 0, keys), std::invalid_argument);
    EXPECT_THROW(ESM4::advanceBowAnimation({}, std::numeric_limits<float>::infinity(), keys),
        std::invalid_argument);
    const float maximum = std::numeric_limits<float>::max();
    EXPECT_THROW(ESM4::advanceBowAnimation({Phase::Start, maximum}, maximum, keys),
        std::invalid_argument);
    keys[4] = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(ESM4::advanceBowAnimation({}, 0, keys), std::invalid_argument);
}
