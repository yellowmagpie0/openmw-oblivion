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
