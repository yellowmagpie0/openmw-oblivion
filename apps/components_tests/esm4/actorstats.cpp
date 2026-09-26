#include <components/esm4/actorstats.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <array>
#include <stdexcept>

TEST(ESM4ActorStats, FixedLevelBypassesOffsetMinimumMaximumAndFloor)
{
    ESM4::ACBS_TES4 input{};
    input.calcMin = 10;
    input.calcMax = 20;
    for (std::int16_t level : {-32768, -1, 0, 1, 5, 10, 20, 21, 32767})
    {
        input.levelOrOffset = level;
        EXPECT_EQ(ESM4::resolveActorLevel(input, 100), level);
        EXPECT_EQ(ESM4::resolveActorLevel(input, std::nullopt), level);
    }
}

TEST(ESM4ActorStats, OffsetAddsPlayerLevelAndChecksMinimumBeforeMaximum)
{
    ESM4::ACBS_TES4 input{};
    input.flags = 0x80;
    input.levelOrOffset = -5;
    EXPECT_EQ(ESM4::resolveActorLevel(input, 10), 5);
    EXPECT_EQ(ESM4::resolveActorLevel(input, 5), 1);
    EXPECT_EQ(ESM4::resolveActorLevel(input, 1), 1);
    EXPECT_EQ(ESM4::resolveActorLevel(input, std::nullopt), 1);
    input.levelOrOffset = 5;
    EXPECT_EQ(ESM4::resolveActorLevel(input, std::nullopt), 5);
    input.calcMin = 10;
    input.calcMax = 20;
    EXPECT_EQ(ESM4::resolveActorLevel(input, 4), 10);
    EXPECT_EQ(ESM4::resolveActorLevel(input, 5), 10);
    EXPECT_EQ(ESM4::resolveActorLevel(input, 6), 11);
    EXPECT_EQ(ESM4::resolveActorLevel(input, 14), 19);
    EXPECT_EQ(ESM4::resolveActorLevel(input, 15), 20);
    EXPECT_EQ(ESM4::resolveActorLevel(input, 16), 20);
    // Preserve the native raw-record decision instead of sorting bad bounds.
    input.calcMin = 20;
    input.calcMax = 10;
    EXPECT_EQ(ESM4::resolveActorLevel(input, 5), 20);
    EXPECT_EQ(ESM4::resolveActorLevel(input, 15), 10);
}

TEST(ESM4ActorStats, OffsetPreservesNativeWordAdditionAndSignedResult)
{
    ESM4::ACBS_TES4 input{};
    input.flags = 0x80;
    input.levelOrOffset = 32767;
    EXPECT_EQ(ESM4::resolveActorLevel(input, 0), 32767);
    EXPECT_EQ(ESM4::resolveActorLevel(input, 1), 1);
    input.calcMin = 10;
    EXPECT_EQ(ESM4::resolveActorLevel(input, 1), 10);
    input.calcMin = std::numeric_limits<std::uint16_t>::max();
    EXPECT_EQ(ESM4::resolveActorLevel(input, 1), -1);
    input.calcMin = 0;
    input.levelOrOffset = 1;
    EXPECT_EQ(ESM4::resolveActorLevel(input, 65535), 1);
    input.levelOrOffset = -32768;
    EXPECT_EQ(ESM4::resolveActorLevel(input, 65535), 32767);
}

TEST(ESM4ActorStats, NpcHealthUsesLevelsGainedAndLowLevelScaling)
{
    const ESM4::NpcDynamicStatsSettings settings{.5f, 4, 4, .4f, 1.5f};
    ESM4::NpcDynamicStatsInput input{49, 49, 37, 23, 50, 1, false, 0};
    // Independently executed original 00547F80, installed GMST values.
    const std::array<unsigned, 6> expected{19, 29, 41, 54, 69, 144};
    const std::array<std::int16_t, 6> levels{1, 2, 3, 4, 5, 20};
    for (std::size_t i = 0; i < levels.size(); ++i)
    {
        input.mLevel = levels[i];
        const auto result = ESM4::calculateNpcDynamicStats(input, settings);
        EXPECT_EQ(result.mHealth, expected[i]) << levels[i];
        EXPECT_EQ(result.mMagicka, 122); // Truncate 49 * 1.5 + 49.
        EXPECT_EQ(result.mFatigue, 159);
    }
    input.mLevel = 5;
    input.mFavoredEndurance = true;
    EXPECT_EQ(ESM4::calculateNpcDynamicStats(input, settings).mHealth, 73);
    input.mSpecialization = 1;
    EXPECT_EQ(ESM4::calculateNpcDynamicStats(input, settings).mHealth, 65);
    input.mSpecialization = 2;
    EXPECT_EQ(ESM4::calculateNpcDynamicStats(input, settings).mHealth, 69);
}

TEST(ESM4ActorStats, NpcDynamicStatBoundariesAndCompiledDefaults)
{
    ESM4::NpcDynamicStatsSettings settings{.5f, 4, 3, .25f, .2f};
    ESM4::NpcDynamicStatsInput input{0, 0, 0, 0, 0, 1, false, 0};
    auto result = ESM4::calculateNpcDynamicStats(input, settings);
    EXPECT_EQ(result.mHealth, 0);
    EXPECT_EQ(result.mMagicka, 0);
    EXPECT_EQ(result.mFatigue, 0);
    input = {255, 255, 255, 255, 255, 32767, true, 0};
    result = ESM4::calculateNpcDynamicStats(input, settings);
    EXPECT_EQ(result.mHealth, 196851);
    EXPECT_EQ(result.mMagicka, 306);
    EXPECT_EQ(result.mFatigue, 1020);
    input = {49, 49, 37, 23, 50, 1, false, 0};
    EXPECT_EQ(ESM4::calculateNpcDynamicStats(input, settings).mHealth, 12);
    settings.mLowLevelMaximum = 0; // Disabled branch, no division by zero.
    EXPECT_EQ(ESM4::calculateNpcDynamicStats(input, settings).mHealth, 49);
}

TEST(ESM4ActorStats, NpcDynamicStatsRejectUnsupportedDomainsAndOverflow)
{
    const ESM4::NpcDynamicStatsSettings valid{.5f, 4, 4, .4f, 1.5f};
    ESM4::NpcDynamicStatsInput input{100, 100, 100, 100, 100, 5, false, 0};
    auto settings = valid;
    for (float invalid : {-1.f, std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::quiet_NaN()})
    {
        settings = valid;
        settings.mAttributeHealthMultiplier = invalid;
        EXPECT_THROW(ESM4::calculateNpcDynamicStats(input, settings), std::invalid_argument);
        settings = valid;
        settings.mMagickaMultiplier = invalid;
        EXPECT_THROW(ESM4::calculateNpcDynamicStats(input, settings), std::invalid_argument);
        settings = valid;
        settings.mLowLevelHealthMultiplier = invalid;
        EXPECT_THROW(ESM4::calculateNpcDynamicStats(input, settings), std::invalid_argument);
    }
    settings = valid;
    settings.mLowLevelHealthMultiplier = 1.01f;
    EXPECT_THROW(ESM4::calculateNpcDynamicStats(input, settings), std::invalid_argument);
    settings = valid;
    settings.mLowLevelMaximum = -1;
    EXPECT_THROW(ESM4::calculateNpcDynamicStats(input, settings), std::invalid_argument);
    settings = valid;
    settings.mPerLevelHealthMultiplier = 0;
    EXPECT_THROW(ESM4::calculateNpcDynamicStats(input, settings), std::invalid_argument);
    settings.mPerLevelHealthMultiplier = std::numeric_limits<std::int32_t>::max();
    EXPECT_THROW(ESM4::calculateNpcDynamicStats(input, settings), std::invalid_argument);
    settings = valid;
    settings.mAttributeHealthMultiplier = std::numeric_limits<float>::max();
    EXPECT_THROW(ESM4::calculateNpcDynamicStats(input, settings), std::invalid_argument);
    settings = valid;
    settings.mMagickaMultiplier = 1000;
    EXPECT_THROW(ESM4::calculateNpcDynamicStats(input, settings), std::invalid_argument);
    input.mLevel = 0;
    EXPECT_THROW(ESM4::calculateNpcDynamicStats(input, valid), std::invalid_argument);
    input.mLevel = 1;
    input.mSpecialization = 3;
    EXPECT_THROW(ESM4::calculateNpcDynamicStats(input, valid), std::invalid_argument);
}

TEST(ESM4ActorStats, NpcAutoCalculationMatchesOriginalAttributeAndSkillFixture)
{
    // Original 005222D0 fixture with duplicate signed racial skill bonuses.
    const ESM4::NpcAutoStatsInput input{2, {36, 28, 71, 80, 56, 56, 35, 69}, 53,
        {6, 0}, {12, 13, 15, 25, 18, 27, 28}, 0,
        {{{2, 1}, {6, 0}, {7, 1}, {5, 2}, {1, 1}, {2, 0}, {2, 1},
            {3, 0}, {2, 2}, {3, 2}, {6, 0}, {3, 0}, {4, 2}, {2, 2},
            {0, 1}, {3, 2}, {0, 2}, {5, 0}, {2, 0}, {0, 2}, {5, 0}}},
        {{{16, 0}, {26, 15}, {17, 15}, {17, 4}, {31, 13}, {25, 1}, {21, 12}}}};
    const auto result = ESM4::calculateNpcAutoStats(input, {5.5f, 6.5f});
    const std::array<std::uint8_t, 8> attributes{44, 28, 75, 82, 56, 57, 53, 69};
    const std::array<std::uint8_t, 21> skills{
        26, 32, 5, 26, 5, 30, 26, 11, 5, 17, 11, 11, 5, 27, 20, 26, 26, 11, 11, 18, 11};
    EXPECT_EQ(result.mAttributes, attributes);
    EXPECT_EQ(result.mSkills, skills);
    const auto& a = result.mAttributes;
    const auto dynamic = ESM4::calculateNpcDynamicStats(
        {a[0], a[1], a[2], a[3], a[5], input.mLevel, false, input.mSpecialization}, {.5f, 4, 4, .4f, 1.5f});
    EXPECT_EQ(dynamic.mHealth, 30);
    EXPECT_EQ(dynamic.mMagicka, 70);
    EXPECT_EQ(dynamic.mFatigue, 258);
}

namespace
{
    ESM4::NpcAutoStatsInput autoStatsFixture()
    {
        ESM4::NpcAutoStatsInput input{};
        input.mLevel = 1;
        input.mRaceAttributes.fill(40);
        input.mAuthoredPersonality = 217;
        input.mFavoredAttributes = {0, 0};
        input.mMajorSkills = {12, 13, 14, 15, 16, 17, 18};
        input.mSpecialization = 0;
        input.mSkills.fill({7, 1});
        input.mSkills[0] = {0, 0};
        return input;
    }
}

TEST(ESM4ActorStats, NpcAutoCalculationUsesNearestEvenAndPreservesPersonality)
{
    auto input = autoStatsFixture();
    auto result = ESM4::calculateNpcAutoStats(input, {5.5f, 90.f});
    EXPECT_EQ(result.mAttributes[0], 46); // Only the first favored bonus.
    EXPECT_EQ(result.mAttributes[6], 217); // No race replacement or cap.
    EXPECT_EQ(result.mSkills[0], 30);
    input.mLevel = 2;
    result = ESM4::calculateNpcAutoStats(input, {5.5f, 90.f});
    EXPECT_EQ(result.mAttributes[0], 46); // 46.5 -> 46.
    EXPECT_EQ(result.mSkills[0], 32); // 31.5 -> 32.
    input.mLevel = 4;
    EXPECT_EQ(ESM4::calculateNpcAutoStats(input, {5, 5}).mSkills[0], 34); // 34.5 -> 34.
    input.mLevel = 32767;
    result = ESM4::calculateNpcAutoStats(input, {5, 5});
    EXPECT_EQ(result.mSkills[0], 100);
    EXPECT_EQ(result.mSkills[20], 100);
    EXPECT_EQ(result.mAttributes[0], 100);
    EXPECT_EQ(result.mAttributes[7], 100);
    EXPECT_EQ(result.mAttributes[6], 217);
}

TEST(ESM4ActorStats, NpcAutoCalculationRetainsSignedDuplicateRaceBonusesAndByteStorage)
{
    auto input = autoStatsFixture();
    input.mRaceBonuses[0] = {19, -5};
    input.mRaceBonuses[1] = {19, -1};
    input.mRaceBonuses[2] = {-1, 127}; // Unused pair does not match.
    input.mRaceBonuses[3] = {12, 127};
    const auto result = ESM4::calculateNpcAutoStats(input, {5, 5});
    EXPECT_EQ(result.mSkills[7], 255); // 5 - 5 - 1, stored as a byte.
    EXPECT_EQ(result.mSkills[0], 100); // Upper clamp applies before storage.
    EXPECT_EQ(result.mSkills[8], 5);
}

TEST(ESM4ActorStats, NpcAutoCalculationRejectsUnresolvedDefinitionsAndOverflow)
{
    const auto valid = autoStatsFixture();
    auto input = valid;
    input.mLevel = 0;
    EXPECT_THROW(ESM4::calculateNpcAutoStats(input, {5, 5}), std::invalid_argument);
    input = valid;
    input.mSpecialization = 3;
    EXPECT_THROW(ESM4::calculateNpcAutoStats(input, {5, 5}), std::invalid_argument);
    input = valid;
    input.mSkills[20].mGoverningAttribute = 8;
    EXPECT_THROW(ESM4::calculateNpcAutoStats(input, {5, 5}), std::invalid_argument);
    input = valid;
    input.mSkills[0].mSpecialization = 3;
    EXPECT_THROW(ESM4::calculateNpcAutoStats(input, {5, 5}), std::invalid_argument);
    input = valid;
    input.mMajorSkills[0] = 11;
    EXPECT_THROW(ESM4::calculateNpcAutoStats(input, {5, 5}), std::invalid_argument);
    input = valid;
    input.mFavoredAttributes[1] = 8;
    EXPECT_THROW(ESM4::calculateNpcAutoStats(input, {5, 5}), std::invalid_argument);
    input = valid;
    input.mRaceBonuses[0].mSkill = 0;
    EXPECT_THROW(ESM4::calculateNpcAutoStats(input, {5, 5}), std::invalid_argument);
    EXPECT_THROW(ESM4::calculateNpcAutoStats(valid, {std::numeric_limits<float>::quiet_NaN(), 5}),
        std::invalid_argument);
    EXPECT_THROW(ESM4::calculateNpcAutoStats(valid, {5, std::numeric_limits<float>::infinity()}),
        std::invalid_argument);
    EXPECT_THROW(ESM4::calculateNpcAutoStats(valid, {-std::numeric_limits<float>::max(), 5}),
        std::invalid_argument);
}

TEST(ESM4ActorStats, CreatureScalingUsesResolvedLevelAndDistinctSkillGroups)
{
    ESM4::CreatureBaseStatsInput input{true, 10, 10, 20, 30, 5, 6, 7, 40};
    const ESM4::CreatureBaseStatsSettings settings{2, 3, 4, 1};
    auto result = ESM4::calculateCreatureBaseStats(input, settings);
    EXPECT_EQ(result.mCombat, 30);
    EXPECT_EQ(result.mMagic, 50);
    EXPECT_EQ(result.mStealth, 70);
    EXPECT_EQ(result.mHealth, 50);
    EXPECT_EQ(result.mMagicka, 60);
    EXPECT_EQ(result.mFatigue, 70);
    EXPECT_EQ(result.mDamage, 50);
    input.mResolvedLevel = -5; // These native getters apply their own floor.
    result = ESM4::calculateCreatureBaseStats(input, settings);
    EXPECT_EQ(result.mCombat, 12);
    EXPECT_EQ(result.mMagic, 23);
    EXPECT_EQ(result.mStealth, 34);
    EXPECT_EQ(result.mHealth, 5);
    EXPECT_EQ(result.mDamage, 41);
    input.mPlayerLevelOffset = false;
    input.mResolvedLevel = 32767;
    result = ESM4::calculateCreatureBaseStats(input, settings);
    EXPECT_EQ(result.mCombat, 10);
    EXPECT_EQ(result.mMagic, 20);
    EXPECT_EQ(result.mStealth, 30);
    EXPECT_EQ(result.mHealth, 5);
    EXPECT_EQ(result.mMagicka, 6);
    EXPECT_EQ(result.mFatigue, 7);
    EXPECT_EQ(result.mDamage, 40);
}

TEST(ESM4ActorStats, CreatureGettersTruncateThenWrapNativeByteAndWordResults)
{
    ESM4::CreatureBaseStatsInput input{true, 2, 255, 250, 0, 65535, 32768, 0, 65535};
    auto result = ESM4::calculateCreatureBaseStats(input, {2, 2, .9f, 1});
    EXPECT_EQ(result.mCombat, 3);
    EXPECT_EQ(result.mMagic, 254);
    EXPECT_EQ(result.mStealth, 1); // Truncate 1.8, do not round to nearest.
    EXPECT_EQ(result.mHealth, 65534);
    EXPECT_EQ(result.mMagicka, 0);
    EXPECT_EQ(result.mFatigue, 0);
    EXPECT_EQ(result.mDamage, 1);
    input = {true, 1, 0, 0, 0, 0, 0, 0, 0};
    result = ESM4::calculateCreatureBaseStats(input, {-1.25f, -.9f, 1.9f, -1.25f});
    EXPECT_EQ(result.mCombat, 255);
    EXPECT_EQ(result.mMagic, 0);
    EXPECT_EQ(result.mStealth, 1);
    EXPECT_EQ(result.mDamage, 65535);
}

TEST(ESM4ActorStats, CreatureSettingsAndArithmeticRejectNonfiniteAndIntegerOverflow)
{
    const ESM4::CreatureBaseStatsInput input{true, 32767, 255, 255, 255, 65535, 65535, 65535, 65535};
    for (float value : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        EXPECT_THROW(ESM4::calculateCreatureBaseStats(input, {value, 2, 2, 1}), std::invalid_argument);
        EXPECT_THROW(ESM4::calculateCreatureBaseStats(input, {2, value, 2, 1}), std::invalid_argument);
        EXPECT_THROW(ESM4::calculateCreatureBaseStats(input, {2, 2, value, 1}), std::invalid_argument);
        EXPECT_THROW(ESM4::calculateCreatureBaseStats(input, {2, 2, 2, value}), std::invalid_argument);
    }
    EXPECT_THROW(ESM4::calculateCreatureBaseStats(input, {std::numeric_limits<float>::max(), 2, 2, 1}),
        std::invalid_argument);
    EXPECT_THROW(ESM4::calculateCreatureBaseStats(input, {2, 2, 2, -std::numeric_limits<float>::max()}),
        std::invalid_argument);
    // Largest signed native conversion result is still valid before word storage.
    EXPECT_EQ(ESM4::calculateCreatureBaseStats(input, {2, 2, 2, 65536}).mDamage, 65535);
}
