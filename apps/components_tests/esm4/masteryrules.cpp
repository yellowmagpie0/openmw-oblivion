#include <components/esm4/masteryrules.hpp>
#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadgmst.hpp>
#include <gtest/gtest.h>
#include <array>
#include <random>
#include <stdexcept>
#include <cmath>
#include <limits>
#include <bit>

namespace
{
    const ESM4::CombatMasterySettings mastery{{25, 50, 75, 100}};
    const ESM4::MasteryProcSettings settings{5, 5, 5, 5, 5, 25};
    constexpr std::array attacks{ESM4::MasteryAttack::Normal, ESM4::MasteryAttack::Standing,
        ESM4::MasteryAttack::Forward, ESM4::MasteryAttack::Backward, ESM4::MasteryAttack::Left,
        ESM4::MasteryAttack::Right, ESM4::MasteryAttack::Arrow};
}

TEST(ESM4MasteryRules, ContactReactionsShareOneStrictDrawAndParalysisPrecedence)
{
    for (int skill : {0, 24, 25, 49, 50, 74, 75, 76, 99, 100, 101})
        for (auto attack : attacks)
            for (int speed : {-1, 0, 1, 50})
                for (bool initial : {false, true})
                    for (unsigned draw : {0, 4, 5, 6, 99})
                    {
                        const auto r = ESM4::attackMasteryReaction({skill, attack, speed, initial}, draw, settings, mastery);
                        const bool consumes = skill >= 75 && speed > 0;
                        const bool paralyze = consumes && skill >= 100 && draw < 5
                            && (attack == ESM4::MasteryAttack::Forward || attack == ESM4::MasteryAttack::Arrow);
                        const bool knock = consumes && draw < 5
                            && (attack == ESM4::MasteryAttack::Backward || attack == ESM4::MasteryAttack::Arrow);
                        EXPECT_EQ(r.mConsumesDraw, consumes);
                        EXPECT_EQ(r.mParalysis, paralyze);
                        EXPECT_EQ(r.mKnockdown, !paralyze && (initial || knock));
                    }
    auto different = settings;different.mKnockdown = 3;different.mParalysis = 10;
    const auto r = ESM4::attackMasteryReaction({100, ESM4::MasteryAttack::Arrow, 1, true}, 5, different, mastery);
    EXPECT_TRUE(r.mParalysis);EXPECT_FALSE(r.mKnockdown);EXPECT_TRUE(r.mConsumesDraw);
    different.mKnockdown = 10;different.mParalysis = 3;
    const auto k = ESM4::attackMasteryReaction({100, ESM4::MasteryAttack::Arrow, 1, false}, 5, different, mastery);
    EXPECT_FALSE(k.mParalysis);EXPECT_TRUE(k.mKnockdown);
}

TEST(ESM4MasteryRules, SideDisarmUsesJourneymanInclusiveDrawAndPostDrawItemGuards)
{
    for (int skill : {24, 25, 49, 50, 51, 74, 75, 99, 100, 101})
        for (auto attack : attacks)
            for (bool npc : {false, true})
                for (bool armed : {false, true})
                    for (bool quest : {false, true})
                        for (bool drawn : {false, true})
                            for (unsigned draw : {0, 4, 5, 6, 99})
                            {
                                const auto r = ESM4::sidePowerDisarm(skill, attack, npc, armed, quest, drawn, draw, settings, mastery);
                                const bool consumes = npc && armed && skill >= 50
                                    && (attack == ESM4::MasteryAttack::Left || attack == ESM4::MasteryAttack::Right);
                                EXPECT_EQ(r.mConsumesDraw, consumes);
                                EXPECT_EQ(r.mTriggered, consumes && !quest && drawn && draw <= 5);
                            }
}

TEST(ESM4MasteryRules, DefensiveStaggerSelectsBlockWithShieldOrUnarmedHandSkill)
{
    for (int block : {49, 50, 74, 75, 76, 99, 100, 101})
        for (int hand : {49, 50, 74, 75, 76, 99, 100, 101})
            for (bool weapon : {false, true})
                for (bool shield : {false, true})
                    for (unsigned draw : {0, 4, 5, 6, 99})
                    {
                        const auto r = ESM4::blockMasteryStagger({block, hand, weapon, shield}, draw, settings, mastery);
                        const bool consumes = weapon || shield ? shield && block >= 75 : hand >= 75;
                        EXPECT_EQ(r.mConsumesDraw, consumes);
                        EXPECT_EQ(r.mTriggered, consumes && draw <= 5);
                    }
}

TEST(ESM4MasteryRules, DefensiveDisarmRequiresMasterShieldOrUnarmedHandAndRealWeapon)
{
    for (int block : {74, 75, 99, 100, 101})
        for (int hand : {74, 75, 99, 100, 101})
            for (bool weapon : {false, true})
                for (bool shield : {false, true})
                    for (bool npc : {false, true})
                        for (bool targetWeapon : {false, true})
                            for (bool quest : {false, true})
                                for (bool drawn : {false, true})
                                    for (unsigned draw : {0, 5, 6, 99})
                                    {
                                        const auto r = ESM4::blockMasteryDisarm({block, hand, weapon, shield}, npc,
                                            targetWeapon, quest, drawn, draw, settings, mastery);
                                        const bool rank = weapon || shield ? shield && block >= 100 : hand >= 100;
                                        const bool consumes = npc && targetWeapon && rank;
                                        EXPECT_EQ(r.mConsumesDraw, consumes);
                                        EXPECT_EQ(r.mTriggered, consumes && !quest && drawn && draw <= 5);
                                    }
}

TEST(ESM4MasteryRules, ProcChanceEndpointsCustomThresholdsAndInvalidInputs)
{
    for (int chance : {0, 1, 99, 100})
    {
        auto s = settings;s.mKnockdown = chance;s.mParalysis = chance;
        s.mAttackDisarm = chance;s.mBlockDisarm = chance;s.mBlockStagger = chance;
        for (unsigned draw = 0; draw != 100; ++draw)
        {
            EXPECT_EQ(ESM4::attackMasteryReaction({75, ESM4::MasteryAttack::Arrow, 1, false}, draw, s, mastery).mKnockdown,
                draw < static_cast<unsigned>(chance));
            EXPECT_EQ(ESM4::sidePowerDisarm(50, ESM4::MasteryAttack::Left, true, true, false, true, draw, s, mastery).mTriggered,
                draw <= static_cast<unsigned>(chance));
            EXPECT_EQ(ESM4::attackMasteryReaction({100, ESM4::MasteryAttack::Forward, 1, false}, draw, s, mastery).mParalysis,
                draw < static_cast<unsigned>(chance));
            EXPECT_EQ(ESM4::blockMasteryStagger({75, 0, false, true}, draw, s, mastery).mTriggered,
                draw <= static_cast<unsigned>(chance));
            EXPECT_EQ(ESM4::blockMasteryDisarm({100, 0, false, true}, true, true, false, true, draw, s, mastery).mTriggered,
                draw <= static_cast<unsigned>(chance));
        }
    }
    const ESM4::CombatMasterySettings custom{{10, 20, 30, 40}};
    EXPECT_FALSE(ESM4::attackMasteryReaction({29, ESM4::MasteryAttack::Backward, 1, false}, 0, settings, custom).mConsumesDraw);
    EXPECT_TRUE(ESM4::attackMasteryReaction({30, ESM4::MasteryAttack::Backward, 1, false}, 0, settings, custom).mKnockdown);
    EXPECT_TRUE(ESM4::attackMasteryReaction({40, ESM4::MasteryAttack::Forward, 1, false}, 0, settings, custom).mParalysis);
    EXPECT_TRUE(ESM4::blockMasteryDisarm({40, 0, false, true}, true, true, false, true, 0, settings, custom).mTriggered);
    for (int bad : {-1, 101})
        for (std::int32_t ESM4::MasteryProcSettings::* member : {&ESM4::MasteryProcSettings::mAttackDisarm,
                 &ESM4::MasteryProcSettings::mBlockDisarm, &ESM4::MasteryProcSettings::mBlockStagger,
                 &ESM4::MasteryProcSettings::mKnockdown, &ESM4::MasteryProcSettings::mParalysis,
                 &ESM4::MasteryProcSettings::mHandBlockRecoil})
        {
            auto s = settings;s.*member = bad;
            EXPECT_THROW(ESM4::validateMasteryProcSettings(s), std::invalid_argument);
        }
    EXPECT_THROW(ESM4::attackMasteryReaction({100, static_cast<ESM4::MasteryAttack>(99), 1, false}, 0, settings, mastery), std::invalid_argument);
    EXPECT_THROW(ESM4::blockMasteryStagger({100, 100, true, true}, 100, settings, mastery), std::invalid_argument);
    EXPECT_THROW(ESM4::sidePowerDisarm(100, ESM4::MasteryAttack::Left, true, true, false, true, 100, settings, mastery), std::invalid_argument);
    EXPECT_THROW(ESM4::blockMasteryDisarm({100, 100, true, true}, true, true, false, true, 100, settings, mastery), std::invalid_argument);
    EXPECT_THROW(ESM4::blockMasteryStagger({100, 100, true, true}, 0, settings, {{25, 20, 75, 100}}), std::invalid_argument);
}

TEST(ESM4MasteryRules, ProcSettingsUseNativeTypesDefaultsAndOverrides)
{
    const auto s = ESM4::buildMasteryProcSettings({});
    EXPECT_EQ(s.mAttackDisarm, 5);EXPECT_EQ(s.mBlockDisarm, 5);EXPECT_EQ(s.mBlockStagger, 5);
    EXPECT_EQ(s.mKnockdown, 5);EXPECT_EQ(s.mParalysis, 5);EXPECT_EQ(s.mHandBlockRecoil, 25);
    ESM4::GameSetting value{};value.mEditorId = "iPerkMarksmanParalyzeChance";value.mData = std::int32_t{12};
    const std::array<const ESM4::GameSetting*, 1> values{&value};
    EXPECT_EQ(ESM4::buildMasteryProcSettings(values).mParalysis, 12);
    value.mData = 12.f;
    EXPECT_THROW(ESM4::buildMasteryProcSettings(values), std::invalid_argument);
    for (const auto& [name, member] : {
             std::pair{"iPerkAttackDisarmChance", &ESM4::MasteryProcSettings::mAttackDisarm},
             {"iPerkBlockDisarmChance", &ESM4::MasteryProcSettings::mBlockDisarm},
             {"iPerkBlockStaggerChance", &ESM4::MasteryProcSettings::mBlockStagger},
             {"iPerkMarksmanKnockdownChance", &ESM4::MasteryProcSettings::mKnockdown},
             {"iPerkMarksmanParalyzeChance", &ESM4::MasteryProcSettings::mParalysis},
             {"iPerkHandToHandBlockRecoilChance", &ESM4::MasteryProcSettings::mHandBlockRecoil}})
    {
        value.mEditorId = name;value.mData = std::int32_t{17};
        EXPECT_EQ(ESM4::buildMasteryProcSettings(values).*member, 17);
    }
    value.mEditorId = "iPerkBlockStaggerChance";value.mData = std::int32_t{25}; // installed override
    const auto native = ESM4::buildMasteryProcSettings(values);
    EXPECT_TRUE(ESM4::blockMasteryStagger({75, 0, false, true}, 25, native, mastery).mTriggered);
    EXPECT_FALSE(ESM4::blockMasteryStagger({75, 0, false, true}, 26, native, mastery).mTriggered);
}

TEST(ESM4MasteryRules, FixedSeedInclusiveAndStrictProcDistributions)
{
    std::mt19937 random(0x4d1532);
    unsigned disarm = 0, paralysis = 0, knockdown = 0, blockDisarm = 0, stagger = 0;
    auto native = settings;native.mBlockStagger = 25;
    for (unsigned i = 0; i < 100000; ++i)
    {
        const unsigned draw = random() % 100;
        disarm += ESM4::sidePowerDisarm(50, ESM4::MasteryAttack::Left, true, true, false, true, draw, settings, mastery).mTriggered;
        const auto r = ESM4::attackMasteryReaction({100, ESM4::MasteryAttack::Arrow, 1, false}, draw, settings, mastery);
        paralysis += r.mParalysis;knockdown += r.mKnockdown;
        blockDisarm += ESM4::blockMasteryDisarm({100, 0, false, true}, true, true, false, true, draw, native, mastery).mTriggered;
        stagger += ESM4::blockMasteryStagger({75, 0, false, true}, draw, native, mastery).mTriggered;
    }
    EXPECT_NEAR(disarm, 6000, 500); // inclusive 0..5
    EXPECT_NEAR(paralysis, 5000, 500); // strict 0..4, same draw clears knockdown
    EXPECT_EQ(knockdown, 0);
    EXPECT_NEAR(blockDisarm, 6000, 500);
    EXPECT_NEAR(stagger, 26000, 700);
}

TEST(ESM4MasteryRules, UnarmedRecoilUsesZeroAbsorptionBlockRankAndInclusiveDraw)
{
    // Original5FFEDF..5FFF52, independently executed in closure-mastery-oracle-03.
    for (int block : {49, 50, 51})
        for (int hand : {49, 50, 51})
            for (bool active : {false, true})
                for (bool unarmed : {false, true})
                    for (bool projectile : {false, true})
                        for (bool weapon : {false, true})
                            for (float absorbed : {-1.f, -0.f, 0.f, std::nextafter(0.f, 1.f), 1.f})
                                for (unsigned draw : {0, 24, 25, 26, 99})
                                {
                                    const auto result = ESM4::unarmedBlockRecoil(
                                        {block, hand, absorbed, unarmed, active, projectile, weapon}, draw, settings, mastery);
                                    const bool eligible = block >= 50 && hand < 50 && active && unarmed
                                        && !projectile && weapon && absorbed <= 0;
                                    EXPECT_EQ(result.mConsumesDraw, eligible);
                                    EXPECT_EQ(result.mTriggered, eligible && draw <= 25);
                                }
    auto thresholds = mastery;
    thresholds.mMinimumSkill = {5, 10, 15, 20};
    EXPECT_TRUE(ESM4::unarmedBlockRecoil({10, 9, 0, true, true, false, true}, 25, settings, thresholds).mTriggered);
    EXPECT_FALSE(ESM4::unarmedBlockRecoil({9, 9, 0, true, true, false, true}, 25, settings, thresholds).mConsumesDraw);
    for (float value : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        EXPECT_THROW(ESM4::unarmedBlockRecoil({50, 49, value, true, true, false, true}, 25, settings, mastery),
            std::invalid_argument);
    EXPECT_THROW(ESM4::unarmedBlockRecoil({50, 49, 0, true, true, false, true}, 100, settings, mastery),
        std::invalid_argument);
}

TEST(ESM4MasteryRules, UnarmedRecoilDistributionIncludesTheChanceEndpoint)
{
    std::mt19937 random(0x4d1552);
    unsigned triggered = 0;
    // Declared tolerance: 26000 +/-600 for100000 uniform percentile draws.
    std::uniform_int_distribution<unsigned> percent(0, 99);
    for (unsigned i = 0; i < 100000; ++i)
        triggered += ESM4::unarmedBlockRecoil({50, 49, 0, true, true, false, true},
            percent(random), settings, mastery).mTriggered;
    EXPECT_GE(triggered, 25400u);
    EXPECT_LE(triggered, 26600u);
}

TEST(ESM4MasteryRules, BowZoomMatchesOriginalDelayFovStoresAndRestoration)
{
    const ESM4::BowZoomSettings zoom{30, .25f, 2.75f};
    ESM4::BowZoomInput input{50, 5, 2.75f, 75, 75, 75, .125f, true, false, false, true, true};
    // Literal original666670 observations; equality at the delay passes.
    EXPECT_EQ(ESM4::bowZoomFov(input, zoom, mastery), 75.f);
    input.mElapsed = std::nextafter(2.75f, 0.f);
    EXPECT_FALSE(ESM4::bowZoomFov(input, zoom, mastery));
    input.mElapsed = std::nextafter(2.75f, 3.f);
    const auto adjacent = ESM4::bowZoomFov(input, zoom, mastery);
    ASSERT_TRUE(adjacent);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(*adjacent), 1117126650u);
    input.mElapsed = 2.875f;
    EXPECT_EQ(ESM4::bowZoomFov(input, zoom, mastery), 52.5f);
    input.mElapsed = 3;
    EXPECT_EQ(ESM4::bowZoomFov(input, zoom, mastery), 30.f);
    input.mElapsed = 3.5f;
    EXPECT_EQ(ESM4::bowZoomFov(input, zoom, mastery), 30.f);
    input.mCurrentFov = 30;
    EXPECT_FALSE(ESM4::bowZoomFov(input, zoom, mastery));
    input.mCurrentFov = 45;
    input.mBaseMarksman = 49;
    EXPECT_FALSE(ESM4::bowZoomFov(input, zoom, mastery));
    input.mBaseMarksman = 50;
    input.mBlockHeld = false;
    EXPECT_EQ(ESM4::bowZoomFov(input, zoom, mastery), 67.5f);
    input.mDuration = .25f;
    EXPECT_EQ(ESM4::bowZoomFov(input, zoom, mastery), 75.f);
    input.mDuration = 0;
    EXPECT_EQ(ESM4::bowZoomFov(input, zoom, mastery), 45.f);
    input.mBlockHeld = true;
    input.mProcessAction = 4;
    EXPECT_FALSE(ESM4::bowZoomFov(input, zoom, mastery));
    input.mThirdPerson = true;
    EXPECT_FALSE(ESM4::bowZoomFov(input, zoom, mastery));
    input.mSceneFov = 45;
    EXPECT_EQ(ESM4::bowZoomFov(input, zoom, mastery), 75.f);
    input.mEnabled = false;
    EXPECT_FALSE(ESM4::bowZoomFov(input, zoom, mastery));
}

TEST(ESM4MasteryRules, BowZoomSettingsAreTypedAndMalformedInputsDiagnose)
{
    const auto defaults = ESM4::buildBowZoomSettings({});
    EXPECT_FLOAT_EQ(defaults.mZoomFov, 30);
    EXPECT_FLOAT_EQ(defaults.mTimeChange, .25f);
    EXPECT_FLOAT_EQ(defaults.mTimeStart, 2.75f);
    ESM4::GameSetting zoom{}, time{}, delay{};
    zoom.mEditorId = "fArrowFOVZoom"; zoom.mData = 40.f;
    time.mEditorId = "fArrowFOVTimeChange"; time.mData = .5f;
    delay.mEditorId = "fArrowFOVTimeStart"; delay.mData = 1.f;
    const std::array<const ESM4::GameSetting*, 3> records{&zoom, &time, &delay};
    const auto custom = ESM4::buildBowZoomSettings(records);
    EXPECT_FLOAT_EQ(custom.mZoomFov, 40);
    EXPECT_FLOAT_EQ(custom.mTimeChange, .5f);
    EXPECT_FLOAT_EQ(custom.mTimeStart, 1);
    ESM4::BowZoomInput input{50, 5, 1.25f, 75, 75, 75, .125f, true, false, false, true, true};
    EXPECT_EQ(ESM4::bowZoomFov(input, custom, mastery), 57.5f);
    zoom.mData = 40;
    EXPECT_THROW(ESM4::buildBowZoomSettings(records), std::invalid_argument);
    for (float bad : {-1.f, 0.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        EXPECT_THROW(ESM4::validateBowZoomSettings({30, bad, 2.75f}), std::invalid_argument);
        EXPECT_THROW(ESM4::validateBowZoomSettings({bad, .25f, 2.75f}), std::invalid_argument);
    }
    input.mDuration = -1;
    EXPECT_THROW(ESM4::bowZoomFov(input, defaults, mastery), std::invalid_argument);
    input.mDuration = .125f;
    input.mElapsed = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(ESM4::bowZoomFov(input, defaults, mastery), std::invalid_argument);
}

TEST(ESM4MasteryRules, BlockingDodgeSelectsMovementPriorityAndResolvedAnimation)
{
    constexpr std::array<std::uint8_t, 16> expected{255,11,12,11,13,11,12,11,14,11,12,11,13,11,12,11};
    for (unsigned flags = 0; flags < 256; ++flags)
        EXPECT_EQ(ESM4::requestedDodgeGroup(flags), expected[flags & 15]);
    for (int skill : {49, 50, 51})
        for (bool held : {false, true})
            for (bool blocked : {false, true})
                for (bool busy : {false, true})
                    for (bool animation : {false, true})
                        for (bool process : {false, true})
                            for (std::optional<std::uint16_t> group : {std::optional<std::uint16_t>{},
                                     {10}, {11}, {12}, {13}, {14}, {15}, {0x10b}, {255}})
                            {
                                const bool allowed = skill >= 50 && held && !blocked && !busy && animation
                                    && process && group && *group >= 11 && *group <= 14;
                                EXPECT_EQ(ESM4::blockingDodgeAllowed({skill, group, held, blocked, busy,
                                              animation, process}, mastery), allowed);
                            }
}

TEST(ESM4MasteryRules, BowZoomClampsLargeFiniteTimeAndDiagnosesOutputOverflow)
{
    ESM4::BowZoomInput input{50, 5, std::numeric_limits<float>::max(), 75, 75, 75,
        std::numeric_limits<float>::max(), true, false, false, true, true};
    EXPECT_EQ(ESM4::bowZoomFov(input, {30, .25f, 2.75f}, mastery), 30.f);
    input.mCurrentFov = std::numeric_limits<float>::max();
    input.mNormalFov = 1;
    EXPECT_THROW(ESM4::bowZoomFov(input, {100, std::numeric_limits<float>::min(), 0}, mastery), std::overflow_error);
}
