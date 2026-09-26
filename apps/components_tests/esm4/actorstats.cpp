#include <components/esm4/actorstats.hpp>

#include <gtest/gtest.h>

#include <limits>

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
