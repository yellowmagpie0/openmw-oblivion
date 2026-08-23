#include <gtest/gtest.h>

#include <set>

#include <components/esm4/loadbsgn.hpp>
#include <components/esm4/loadclas.hpp>
#include <components/esm4/loadrace.hpp>
#include <components/esm4/playermechanics.hpp>

namespace
{
    ESM4::Race makeRace()
    {
        ESM4::Race race;
        race.mAttribMale = { 40, 40, 30, 30, 40, 40, 50, 50 };
        race.mAttribFemale = { 30, 50, 40, 40, 40, 30, 50, 50 };
        race.mSkillBonus[ESM4::Race::Skill_Blade] = 10;
        return race;
    }

    ESM4::Class makeClass()
    {
        ESM4::Class value;
        value.mData.mFavoredAttributes = { 0, 5 };
        value.mData.mSpecialization = 0;
        value.mData.mMajorSkills = { 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12 };
        return value;
    }
}

TEST(ESM4PlayerMechanics, BuildsOfficialStartingStats)
{
    ESM4::BirthSign warrior;
    warrior.mEditorId = "Warrior";
    const ESM4::PlayerCharacterStats stats = ESM4::buildPlayerCharacterStats(makeRace(), makeClass(), &warrior, false);
    EXPECT_FLOAT_EQ(stats.mAttributes[0], 55.f);
    EXPECT_FLOAT_EQ(stats.mAttributes[5], 55.f);
    EXPECT_FLOAT_EQ(stats.mSkills[2], 40.f); // 5 base + 10 race + 5 specialization + 20 major
    EXPECT_FLOAT_EQ(stats.mHealth, 110.f);
    EXPECT_FLOAT_EQ(stats.mMagicka, 80.f);
    EXPECT_FLOAT_EQ(stats.mFatigue, 170.f);
    EXPECT_FLOAT_EQ(stats.mCapacity, 275.f);
    EXPECT_FLOAT_EQ(stats.mBreathTime, 20.5f);
}

TEST(ESM4PlayerMechanics, AppliesSexAndBirthsignMagic)
{
    ESM4::BirthSign atronach;
    atronach.mEditorId = "Atronach";
    const ESM4::PlayerCharacterStats stats = ESM4::buildPlayerCharacterStats(makeRace(), makeClass(), &atronach, true);
    EXPECT_FLOAT_EQ(stats.mAttributes[0], 35.f);
    EXPECT_FLOAT_EQ(stats.mAttributes[5], 35.f);
    EXPECT_FLOAT_EQ(stats.mHealth, 70.f);
    EXPECT_FLOAT_EQ(stats.mMagicka, 250.f);
    EXPECT_TRUE(stats.mStuntedMagicka);
}

TEST(ESM4PlayerMechanics, MovementFormulaBoundariesAndMonotonicity)
{
    EXPECT_FLOAT_EQ(ESM4::playerWalkSpeed(0.f, 0.f, 200.f, false), 90.f);
    EXPECT_FLOAT_EQ(ESM4::playerWalkSpeed(100.f, 0.f, 200.f, false), 130.f);
    EXPECT_FLOAT_EQ(ESM4::playerRunSpeed(50.f, 0.f, 200.f), 330.f);
    EXPECT_FLOAT_EQ(ESM4::playerWalkSpeed(50.f, 200.f, 200.f, true), 39.6f);
    EXPECT_FLOAT_EQ(ESM4::playerWalkSpeed(50.f, 201.f, 200.f, false), 0.f);
    EXPECT_FLOAT_EQ(ESM4::playerRunSpeed(50.f, 201.f, 200.f), 0.f);
    EXPECT_FLOAT_EQ(ESM4::playerJumpHeight(50.f, 201.f, 200.f), 0.f);
    EXPECT_GT(ESM4::playerJumpVelocity(5.f, 4.f, 225.f), 0.f);
    EXPECT_FLOAT_EQ(ESM4::playerJumpVelocity(50.f, 201.f, 200.f), 0.f);
    for (int encumbrance = 0; encumbrance < 200; ++encumbrance)
    {
        EXPECT_GE(ESM4::playerWalkSpeed(50.f, encumbrance, 200.f, false),
            ESM4::playerWalkSpeed(50.f, encumbrance + 1.f, 200.f, false));
        EXPECT_GE(ESM4::playerJumpHeight(50.f, encumbrance, 200.f),
            ESM4::playerJumpHeight(50.f, encumbrance + 1.f, 200.f));
    }
    EXPECT_FLOAT_EQ(ESM4::playerRunFatigueCost(2.f), 16.f);
    EXPECT_FLOAT_EQ(ESM4::playerJumpFatigueCost(), 30.f);
    EXPECT_FLOAT_EQ(ESM4::playerFatigueRecovery(2.f), 20.f);
}

TEST(ESM4PlayerMechanics, MapsExactlyTheTwentyOneNativeSkills)
{
    std::set<std::string_view> names;
    for (std::uint32_t nativeSkill = ESM4::Race::Skill_Armorer;
         nativeSkill <= ESM4::Race::Skill_Speechcraft; ++nativeSkill)
    {
        const int index = ESM4::playerSkillIndex(nativeSkill);
        ASSERT_GE(index, 0);
        EXPECT_EQ(index, static_cast<int>(nativeSkill - ESM4::Race::Skill_Armorer));
        EXPECT_FALSE(ESM4::playerSkillName(index).empty());
        names.insert(ESM4::playerSkillName(index));
    }
    EXPECT_EQ(names.size(), 21u);
    EXPECT_EQ(ESM4::playerSkillIndex(ESM4::Race::Skill_Armorer - 1), -1);
    EXPECT_EQ(ESM4::playerSkillIndex(ESM4::Race::Skill_Speechcraft + 1), -1);
    EXPECT_TRUE(ESM4::playerSkillName(21).empty());
}
