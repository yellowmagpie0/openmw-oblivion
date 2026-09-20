#include <components/esm4/masteryrules.hpp>
#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadgmst.hpp>
#include <gtest/gtest.h>
#include <array>
#include <random>
#include <stdexcept>

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
