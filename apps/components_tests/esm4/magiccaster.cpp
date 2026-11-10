#include <components/esm4/magiccaster.hpp>
#include <components/esm/defs.hpp>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <limits>
#include <stdexcept>

namespace
{
#include "magiccaster_expected.inc"
}

TEST(ESM4MagicProjectile, SweptEndpointPreservesFlightAndStopsAtRange)
{
    ESM4::MagicProjectileState state;
    state.mCaster = ESM::FormKey::dynamic("player", 1);
    state.mSource = ESM::FormKey::content("Oblivion.esm", 0xa97df);
    state.mCell = ESM::FormKey::content("Oblivion.esm", 0x380bd);
    state.mEffectCode = ESM::fourCC("FIDG"); state.mMagnitude = 6;
    state.mPosition = {10, 20, 30}; state.mVelocity = {0, 1000, 0};
    state.mModel = "MagicEffects/fireball.nif";
    auto next = ESM4::advanceMagicProjectile(state, .25f);
    EXPECT_EQ(next.mPosition, (std::array<float, 3>{10, 270, 30}));
    EXPECT_FLOAT_EQ(next.mDistance, 250);
    EXPECT_EQ(state.mPosition, (std::array<float, 3>{10, 20, 30}));
    next = ESM4::advanceMagicProjectile(next, 20);
    EXPECT_FLOAT_EQ(next.mPosition[1], 10020);
    EXPECT_FLOAT_EQ(next.mDistance, next.mMaximumDistance);
    EXPECT_EQ(ESM4::advanceMagicProjectile(next, 1), next);
    EXPECT_THROW(ESM4::advanceMagicProjectile(state, -1), std::invalid_argument);
    EXPECT_THROW(ESM4::advanceMagicProjectile(state, std::numeric_limits<float>::infinity()), std::invalid_argument);
    auto invalid = state; invalid.mVelocity = {};
    EXPECT_THROW(invalid.validate(), std::invalid_argument);
    invalid = state; invalid.mMagnitude = -1;
    EXPECT_THROW(invalid.validate(), std::invalid_argument);
    invalid = state; invalid.mEffectCode = ESM::fourCC("REHE");
    EXPECT_THROW(invalid.validate(), std::invalid_argument);
}

TEST(ESM4CombatSpellPower, ReplaysOriginalPlayerCasterPowerAndRejectsUnrepresentableOutputs)
{
    unsigned finite = 0, rejected = 0;
    const auto replay = [&](const auto& fatigues, const auto& coverages, const auto& skills,
                            const auto& settings, const auto& thresholds, const auto& expected) {
        ASSERT_EQ(expected.size(), fatigues.size() * coverages.size() * skills.size()
                * settings.size() * thresholds.size());
        std::size_t n = 0;
        for (const auto& fatigue : fatigues)
        for (const auto& coverage : coverages)
        for (const auto& skill : skills)
        for (const auto& s : settings)
        for (const auto threshold : thresholds)
        {
            SCOPED_TRACE(n);
            const ESM4::PlayerCombatSpellPower input{
                fatigue[0], fatigue[1], skill[0], skill[1], coverage[0], coverage[1]};
            const ESM4::CombatSpellPowerSettings parameters{std::bit_cast<float>(s[0]), std::bit_cast<float>(s[1]),
                std::bit_cast<float>(s[2]), std::bit_cast<float>(s[3]), threshold};
            if ((expected[n] & 0x7f800000u) == 0x7f800000u)
            {
                EXPECT_THROW(ESM4::playerCombatSpellPower(input, parameters), std::invalid_argument);
                ++rejected;
            }
            else
            {
                EXPECT_EQ(std::bit_cast<std::uint32_t>(ESM4::playerCombatSpellPower(input, parameters)), expected[n]);
                ++finite;
            }
            ++n;
        }
    };
    replay(sFatigue2, sCoverage2, sSkills2, sSettings2, sThresholds2, sExpected2);
    replay(sFatigue4, sCoverage4, sSkills4, sSettings4, sThresholds4, sExpected4);
    replay(sFatigue5, sCoverage5, sSkills5, sSettings5, sThresholds5, sExpected5);
    EXPECT_EQ(finite, 4512u);
    EXPECT_EQ(rejected, 220u);
}

TEST(ESM4CombatSpellPower, IntegerRatioMidpointUsesOriginalPrecisionBeforeFloatStore)
{
    // Original PC24 arithmetic at declared integer-getter boundaries. This
    // synthetic base lies outside 5F1910's normal f32/floor image; it protects
    // the typed adapter rather than establishing a reachable vanilla bug.
    const ESM4::PlayerCombatSpellPower input{2125114407, 2125114027, 0, 0, 0, 0};
    const ESM4::CombatSpellPowerSettings settings{0, 1, .05f, .3f, 50};
    EXPECT_EQ(std::bit_cast<std::uint32_t>(ESM4::playerCombatSpellPower(input, settings)), 0x3f800001u);
}

TEST(ESM4CombatSpellPower, ValidatesTypedCoverageAndNonfiniteSettings)
{
    const ESM4::PlayerCombatSpellPower input{50, 100, 0, 0, 0, 0};
    const ESM4::CombatSpellPowerSettings defaults;
    for (const auto coverage : {-1, 101, std::numeric_limits<std::int32_t>::max()})
    {
        auto malformed = input;
        malformed.mLightCoverage = coverage;
        EXPECT_THROW(ESM4::playerCombatSpellPower(malformed, defaults), std::invalid_argument);
        malformed = input;
        malformed.mHeavyCoverage = coverage;
        EXPECT_THROW(ESM4::playerCombatSpellPower(malformed, defaults), std::invalid_argument);
    }
    for (const auto value : {std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity()})
    for (const auto member : {&ESM4::CombatSpellPowerSettings::mFatigueBase,
             &ESM4::CombatSpellPowerSettings::mFatigueMultiplier,&ESM4::CombatSpellPowerSettings::mArmorPenaltyMinimum,
             &ESM4::CombatSpellPowerSettings::mArmorPenaltyMaximum})
    {
        auto malformed = defaults;
        malformed.*member = value;
        EXPECT_THROW(ESM4::playerCombatSpellPower(input, malformed), std::invalid_argument);
    }
}

TEST(ESM4CombatSpellPower, IntegerProductsRoundOnceAtOriginalPrecisionBeforeFloatStore)
{
    // Full typed-int32 getter boundaries, not observed vanilla gameplay.
    // Double-rounding a 54-bit product through double loses these native bits.
    const ESM4::CombatSpellPowerSettings settings{1, 0, .05f, .3f, 50};
    std::size_t n = 0;
    unsigned checked = 0;
    for (const auto& row : sProductCases)
    {
        SCOPED_TRACE(n++);
        for (const bool heavy : {false, true})
        {
            SCOPED_TRACE(heavy);
            ESM4::PlayerCombatSpellPower input{100, 100, 50, 50, 0, 0};
            (heavy ? input.mHeavyArmor : input.mLightArmor) = row.mSkill;
            (heavy ? input.mHeavyCoverage : input.mLightCoverage) = row.mCoverage;
            EXPECT_EQ(std::bit_cast<std::uint32_t>(ESM4::playerCombatSpellPower(input, settings)),
                row.mExpected[heavy]);
            ++checked;
        }
    }
    EXPECT_EQ(checked, 76u);
}

TEST(ESM4SpellCost, TutorialHealMatchesOriginalCostRoutine)
{
    ESM4::SpellCostSettings settings;
    settings.mSkillBase = .2f;
    settings.mMagnitudeExponent = 1.28f;
    const ESM4::SpellEffectCostInput heal{10.f, 8, 0, 0, false};
    // Hash-pinned original SpellItem virtual41D320, including original math
    // and effect-list traversal; only the integer actor AV getter is stubbed.
    struct Case { int skill; int luck; float cost; };
    for (const auto& row : std::array<Case, 6>{{{5, 50, 19}, {25, 50, 15}, {50, 50, 11},
             {100, 50, 2}, {5, 0, 20}, {5, 100, 15}}})
        EXPECT_FLOAT_EQ(ESM4::spellEffectMagickaCost(heal, row.skill, row.luck, settings), row.cost);
    auto ranged = heal;
    ranged.mTarget = true;
    EXPECT_FLOAT_EQ(ESM4::spellEffectMagickaCost(ranged, 5, 50, settings), 28.f);
    EXPECT_FLOAT_EQ(ESM4::manualSpellMagickaCost(0, 5, 50, settings), 0.f);
    EXPECT_NEAR(ESM4::manualSpellMagickaCost(1, 5, 50, settings), 1.34f, .0001f);
    EXPECT_NEAR(ESM4::manualSpellMagickaCost(24, 5, 50, settings), 32.16f, .0001f);
}

TEST(ESM4SpellCost, ValidatesCostInputsAndRetainsCalculatedMinimum)
{
    ESM4::SpellCostSettings settings;
    const ESM4::SpellEffectCostInput empty{0.f, 0, 0, 0, false};
    EXPECT_FLOAT_EQ(ESM4::spellEffectMagickaCost(empty, 5, 50, settings), 1.f);
    auto invalid = empty;
    invalid.mBaseCost = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(ESM4::spellEffectMagickaCost(invalid, 5, 50, settings), std::invalid_argument);
    invalid = empty;
    invalid.mMagnitude = std::numeric_limits<std::uint32_t>::max();
    EXPECT_THROW(ESM4::spellEffectMagickaCost(invalid, 5, 50, settings), std::invalid_argument);
    EXPECT_THROW(ESM4::manualSpellMagickaCost(std::numeric_limits<std::uint32_t>::max(), 5, 50, settings),
        std::invalid_argument);
    settings.mSkillMultiplier = -1.f;
    EXPECT_THROW(ESM4::manualSpellMagickaCost(24, 5, 50, settings), std::invalid_argument);
    settings = {};
    settings.mMagnitudeExponent = std::numeric_limits<float>::max();
    EXPECT_THROW(ESM4::spellEffectMagickaCost({10.f, 8, 0, 0, false}, 5, 50, settings), std::invalid_argument);
}

TEST(ESM4SpellCast, ReleaseOccursOnceAcrossFramePartitioning)
{
    ESM4::SpellCastState initial{ESM::FormKey::content("Oblivion.esm", 0x136),
        "onehandcastself", 19.f, 0.f, .36666667f, 1.3f, false};
    auto small = initial;
    int releases = 0;
    for (int i = 0; i < 200; ++i)
        releases += ESM4::advanceSpellCast(small, .01f);
    EXPECT_EQ(releases, 1);
    EXPECT_EQ(small.mElapsed, small.mDuration);
    auto large = initial;
    EXPECT_TRUE(ESM4::advanceSpellCast(large, 10.f));
    EXPECT_EQ(large, small);
    EXPECT_FALSE(ESM4::advanceSpellCast(large, 0.f));
    EXPECT_FALSE(ESM4::advanceSpellCast(large, 10.f));
    // Failed candidate validation cannot partially advance the caller's state.
    const auto before = small;
    EXPECT_THROW(ESM4::advanceSpellCast(small, -1.f), std::invalid_argument);
    EXPECT_THROW(ESM4::advanceSpellCast(small, std::numeric_limits<float>::infinity()), std::invalid_argument);
    EXPECT_EQ(small, before);
}

TEST(ESM4ScrollChameleon, LifetimeStopsAtAuthoredDurationAndRejectsReplay)
{
    ESM4::ScrollChameleonState effect{ESM::FormKey::content("Oblivion.esm", 0x84aeb),
        ESM::FormKey::content("Oblivion.esm", 0x5b044), 25.f, 12.f};
    EXPECT_NO_THROW(effect.validate());
    const auto paused = ESM4::advanceScrollChameleon(effect.mElapsed, 0.f);
    EXPECT_FLOAT_EQ(paused.mElapsed, 12.f);
    EXPECT_FALSE(paused.mExpired);
    const auto remaining = ESM4::advanceScrollChameleon(effect.mElapsed, 100.f);
    EXPECT_FLOAT_EQ(remaining.mElapsed, 30.f);
    EXPECT_TRUE(remaining.mExpired);
    EXPECT_THROW(ESM4::advanceScrollChameleon(30.f, 0.f), std::invalid_argument);
    EXPECT_THROW(ESM4::advanceScrollChameleon(0.f, -1.f), std::invalid_argument);
    EXPECT_THROW(ESM4::advanceScrollChameleon(0.f, std::numeric_limits<float>::infinity()), std::invalid_argument);
    effect.mMagnitude = 24.f;
    EXPECT_THROW(effect.validate(), std::invalid_argument);
}

TEST(ESM4ScrollChameleon, SameEnchantmentRefreshesWithoutAnAdditionalContribution)
{
    ESM4::ScrollChameleonState effect{ESM::FormKey::content("Oblivion.esm", 0x84aeb),
        ESM::FormKey::content("Oblivion.esm", 0x5b044), 25.f, 0.f};
    std::vector<ESM4::ScrollChameleonState> effects;
    EXPECT_FLOAT_EQ(ESM4::applyScrollChameleon(effects, effect), 25.f);
    effects.front().mElapsed = 12.f;
    EXPECT_FLOAT_EQ(ESM4::applyScrollChameleon(effects, effect), 0.f);
    ASSERT_EQ(effects.size(), 1);
    EXPECT_FLOAT_EQ(effects.front().mElapsed, 0.f);
    const auto before = effects;
    effect.mElapsed = 1.f;
    EXPECT_THROW(ESM4::applyScrollChameleon(effects, effect), std::invalid_argument);
    EXPECT_EQ(effects, before);
}

TEST(ESM4GreaterPower, ElapsedGameHoursOwnDailyUseAcrossMidnightAndBackwardClock)
{
    ESM4::PowerUseState state{ESM::FormKey::content("Oblivion.esm", 0x47ade), 23.75};
    EXPECT_FALSE(state.available(24.));
    EXPECT_FALSE(state.available(47.749));
    EXPECT_TRUE(state.available(47.75));
    EXPECT_TRUE(state.available(100.));
    EXPECT_FALSE(state.available(1.));
    EXPECT_THROW(state.available(std::numeric_limits<double>::infinity()), std::invalid_argument);
    state.mUsedAt = -1.;
    EXPECT_THROW(state.validate(), std::invalid_argument);
    state.mUsedAt = 0.; state.mSpell = ESM::FormKey::dynamic("spell", 1);
    EXPECT_THROW(state.validate(), std::invalid_argument);
}
