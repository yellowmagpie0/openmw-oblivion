#include <components/esm4/actorclock.hpp>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

TEST(ESM4ActorClock, ManagerSetterMatchesOriginalInstructionBits)
{
    // 00673B10 in the hash-pinned original 1.2.0416 image, including its real
    // CRT checks. Expected bits recorded independently with both x87 controls.
    constexpr std::array<std::array<std::uint32_t, 2>, 12> cases{{
        {0x00000000, 0x00000000}, {0x80000000, 0x80000000},
        {0x3f800000, 0x3f800000}, {0xbf800000, 0xbf800000},
        {0x47c34fff, 0x47c34fff}, {0x47c35000, 0x47c35000},
        {0x47c35001, 0x00000000}, {0x7f7fffff, 0x00000000},
        {0xff7fffff, 0xff7fffff}, {0x7f800000, 0x00000000},
        {0xff800000, 0x00000000}, {0x7fc00000, 0x00000000}}};
    for (const auto& [input, expected] : cases)
    {
        SCOPED_TRACE(input);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(ESM4::normalizeActorManagerTime(std::bit_cast<float>(input))), expected);
    }
}

TEST(ESM4ActorClock, RewindAndInitializationUseStrictSmallTimeBoundary)
{
    // Original common Player/Character/Creature virtual +368, 005FAAE0 clock
    // prefix. The .3 constant is binary32 0x3e99999a, not a double threshold.
    constexpr float threshold = std::bit_cast<float>(0x3e99999au);
    const float below = std::nextafter(threshold, 0.f);
    const float above = std::nextafter(threshold, 1.f);
    for (const float previous : {-1.f, 100000.f})
    {
        EXPECT_EQ(ESM4::actorUpdateDuration(0.f, previous), 0.f);
        EXPECT_FALSE(std::signbit(ESM4::actorUpdateDuration(-0.f, previous)));
        EXPECT_EQ(ESM4::actorUpdateDuration(.1f, previous), .1f);
        EXPECT_EQ(ESM4::actorUpdateDuration(below, previous), below);
        EXPECT_EQ(ESM4::actorUpdateDuration(threshold, previous), 0.f);
        EXPECT_EQ(ESM4::actorUpdateDuration(above, previous), 0.f);
        EXPECT_EQ(ESM4::actorUpdateDuration(1.f, previous), 0.f);
        EXPECT_EQ(ESM4::actorUpdateDuration(-1.f, previous), 0.f);
    }
}

TEST(ESM4ActorClock, OrdinaryUpdatesRetainFloatStoreAndZeroSign)
{
    EXPECT_EQ(ESM4::actorUpdateDuration(100000.f, 99999.f), 1.f);
    EXPECT_EQ(ESM4::actorUpdateDuration(100000.f, 100000.f), 0.f);
    EXPECT_EQ(ESM4::actorUpdateDuration(.3f, 0.f), .3f);
    EXPECT_EQ(ESM4::actorUpdateDuration(1.f, .3f), .7f);
    EXPECT_TRUE(std::signbit(ESM4::actorUpdateDuration(-0.f, 0.f)));
    EXPECT_FALSE(std::signbit(ESM4::actorUpdateDuration(0.f, -0.f)));
    EXPECT_EQ(ESM4::actorUpdateDuration(std::numeric_limits<float>::max(), 0.f),
        std::numeric_limits<float>::max());
    for (const float bad : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::quiet_NaN()})
    {
        EXPECT_THROW(ESM4::actorUpdateDuration(bad, 0.f), std::invalid_argument);
        EXPECT_THROW(ESM4::actorUpdateDuration(0.f, bad), std::invalid_argument);
    }
}

TEST(ESM4ActorClock, FrameAndHourCallersRetainNativeFloatStoresAndReset)
{
    EXPECT_EQ(ESM4::advanceActorManagerTime(99999.f, 1.f), 100000.f);
    EXPECT_EQ(ESM4::advanceActorManagerTime(100000.f, .016f), 0.f);
    EXPECT_EQ(ESM4::advanceActorManagerTime(100000.f, 100000.f), 0.f);
    EXPECT_EQ(ESM4::advanceActorManagerTime(-1.f, .125f), -.875f);
    EXPECT_EQ(ESM4::actorManagerTimeAfterHour(.125f, 30.f), 120.125f);
    EXPECT_EQ(ESM4::actorManagerTimeAfterHour(.125f, -30.f), -119.875f);
    EXPECT_EQ(ESM4::actorManagerTimeAfterHour(99999.f, 30.f), 0.f);
    for (const float scale : {0.f, -0.f, 1e-40f})
        EXPECT_EQ(ESM4::actorManagerTimeAfterHour(.125f, scale), 0.f);
    // A finite division can also exceed the manager limit and reset.
    EXPECT_EQ(std::bit_cast<std::uint32_t>(ESM4::actorManagerTimeAfterHour(-1.f, .001f)), 0u);
}
