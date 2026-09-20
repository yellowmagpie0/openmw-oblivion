#include <gtest/gtest.h>

#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadgmst.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>

TEST(ESM4StealthRules, SneakMultipliersUseBaseRankAndNativeWeaponEligibility)
{
    const auto settings = ESM4::buildSneakAttackSettings({});
    const auto mastery = ESM4::buildCombatMasterySettings({});
    for (int skill : {-1, 0, 1, 24, 25, 26, 49, 50, 51, 74, 75, 76, 99, 100, 101})
        for (int weapon = -1; weapon <= 5; ++weapon)
        {
            const auto result = ESM4::sneakAttack({skill, weapon, 0, true, true, false, false}, settings, mastery);
            const float expected = weapon == 5 ? (skill < 25 ? 6 : 8)
                : weapon == -1 || weapon == 0 || weapon == 2 ? (skill < 25 ? 4 : 6) : 1;
            EXPECT_EQ(result.mMultiplier, expected);
            EXPECT_EQ(result.mBypassArmor, skill >= 100 && expected > 1);
            EXPECT_EQ(result.mBypassBlock, result.mBypassArmor);
        }
    EXPECT_EQ(ESM4::sneakAttack({10, 0, 0, true, true, false, false}, settings, {{10,20,30,40}}).mMultiplier, 6);
    EXPECT_TRUE(ESM4::sneakAttack({40, 0, 0, true, true, false, false}, settings, {{10,20,30,40}}).mBypassArmor);
}

TEST(ESM4StealthRules, SneakEligibilityUsesVictimAwarenessCombatThresholdAndPosture)
{
    const auto settings = ESM4::buildSneakAttackSettings({});
    const auto mastery = ESM4::buildCombatMasterySettings({});
    for (int detection : {std::numeric_limits<int>::min(), -51, -50, -49, -1, 0, 1, 100})
        for (bool combat : {false, true})
            for (bool npc : {false, true})
                for (bool sneak : {false, true})
                    for (bool swim : {false, true})
                    {
                        const auto result = ESM4::sneakAttack({100, 0, detection, npc, sneak, swim, combat}, settings, mastery);
                        const bool eligible = npc && sneak && !swim && detection <= 0 && (!combat || detection <= -50);
                        EXPECT_EQ(result.mMultiplier, eligible ? 6 : 1);
                        EXPECT_EQ(result.mBypassArmor, eligible);
                        EXPECT_EQ(result.mBypassBlock, eligible);
                    }
}

TEST(ESM4StealthRules, ArmorBypassRequiresStrictlyGreaterThanOneMultiplier)
{
    auto settings = ESM4::buildSneakAttackSettings({});
    const auto mastery = ESM4::buildCombatMasterySettings({});
    for (float mult : {0.f, std::nextafter(1.f, 0.f), 1.f, std::nextafter(1.f, 2.f), 2.f})
    {
        settings.mMeleeMultipliers[4] = mult;
        const auto result = ESM4::sneakAttack({100, 0, 0, true, true, false, false}, settings, mastery);
        EXPECT_EQ(result.mMultiplier, mult);
        EXPECT_EQ(result.mBypassArmor, mult > 1);
    }
}

TEST(ESM4StealthRules, SettingsUseEveryRankOverrideAndRejectMalformedInput)
{
    const auto mastery = ESM4::buildCombatMasterySettings({});
    const auto defaults = ESM4::buildSneakAttackSettings({});
    const std::array<std::string, 5> ranks{"Novice", "Apprentice", "Journeyman", "Expert", "Master"};
    const std::array<int, 5> skills{0,25,50,75,100};
    ESM4::GameSetting value{};
    const std::array<const ESM4::GameSetting*, 1> values{&value};
    for (std::size_t i = 0; i < ranks.size(); ++i)
        for (bool bow : {false, true})
        {
            value.mEditorId = std::string("fPerkSneakAttack") + (bow ? "Marksman" : "Melee") + ranks[i] + "Mult";
            value.mData = 2.f + float(i);
            const auto settings = ESM4::buildSneakAttackSettings(values);
            EXPECT_EQ(ESM4::sneakAttack({skills[i], bow ? 5 : 0, 0, true, true, false, false}, settings, mastery).mMultiplier, 2.f + float(i));
            value.mData = std::int32_t{2};
            EXPECT_THROW(ESM4::buildSneakAttackSettings(values), std::invalid_argument);
        }
    value.mEditorId = "iAICombatMinDetection"; value.mData = std::int32_t{-20};
    EXPECT_EQ(ESM4::buildSneakAttackSettings(values).mCombatMinimumDetection, -20);
    for (int bad : {-2, 6, 100})
        EXPECT_THROW(ESM4::sneakAttack({100, bad, 0, false, false, false, false}, defaults, mastery), std::invalid_argument);
    for (float bad : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        auto invalid = defaults; invalid.mMeleeMultipliers[0] = bad;
        EXPECT_THROW(ESM4::sneakAttack({100, 0, 0, false, false, false, false}, invalid, mastery), std::invalid_argument);
        invalid = defaults; invalid.mMarksmanMultipliers[4] = bad;
        EXPECT_THROW(ESM4::validateSneakAttackSettings(invalid), std::invalid_argument);
    }
}
