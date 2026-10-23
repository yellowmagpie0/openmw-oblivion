#include <gtest/gtest.h>

#include "apps/openmw/nativeframeclock.hpp"
#include "apps/openmw/mwmechanics/oblivioncombat.hpp"

#include <array>
#include <bit>
#include <limits>

TEST(NativeFrameClock, ProcessTicksMatchOriginalConnectedTimerAndFrameAcrossWrapAndCaps)
{
    // Original full47D170 -> full889810, FPCW007F, stock fixed step.
    // Retained native-timer-oracle-02/cases.json mode0, limit=false.
    struct Row { std::uint32_t tick, delta, frame, step, count, remaining; };
    constexpr std::array<Row, 10> rows{{
        {4294967280u, 981668463, 0, 0, 0, 981668463},
        {4294967281u, 981668463, 0, 0, 0, 990057071},
        {1, 1015222895, 1015598704, 1015598704, 1, 984245456},
        {168, 1042938856, 1028470184, 1015598704, 3, 1015598704},
        {185, 1015759766, 1023987312, 1015598704, 2, 966609280},
        {185, 981668463, 0, 0, 0, 984245455},
        {218, 1023879938, 1023987312, 1015598704, 2, 980151808},
        {384, 1042938856, 1028470184, 1015598704, 3, 1015598704},
        {551, 1042938856, 1028470184, 1015598704, 3, 1015598704},
        {552, 981668463, 1015598704, 1015598704, 1, 981668464},
    }};
    OMW::NativeFrameClock clock(0xfffffff0u, .0167f, .05f, 0);
    for (const auto& row : rows)
    {
        SCOPED_TRACE(row.tick);
        const auto frame = clock.advance(row.tick, 1.f, false);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(frame.mInputDelta), row.delta);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(frame.mFrameSeconds), row.frame);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(frame.mSubstepSeconds), row.step);
        EXPECT_EQ(frame.mSubstepCount, row.count);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(clock.remaining()), row.remaining);
    }
}

TEST(NativeFrameClock, ConfiguredUpdateTypesKeepOriginalDividedAndSmoothedDurations)
{
    // Same original connected probe, third frame after ticks0,1,17ms.
    for (std::uint32_t mode : {1u, 10u, 0xffffffffu})
    {
        SCOPED_TRACE(mode);
        OMW::NativeFrameClock clock(0xfffffff0u, .0167f, .05f, mode);
        clock.advance(0xfffffff0u, 1.f, false);
        clock.advance(0xfffffff1u, 1.f, false);
        const auto frame = clock.advance(1, 1.f, false);
        EXPECT_EQ(frame.mSubstepCount, 1u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(frame.mInputDelta), 1015222895u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(frame.mSubstepSeconds),
            mode == 10 ? 1021841776u : 1016296637u);
        EXPECT_EQ(clock.remaining(), 0.f);
    }
}

TEST(NativeFrameClock, HiddenPollingPreservesCarryAndOwnsOnePauseTransition)
{
    OMW::NativeFrameClock once(0, .0167f, .05f, 0), polled = once;
    once.advance(1, 1.f, false);
    polled.advance(1, 1.f, false);
    const float remaining = once.remaining();
    once.advance(2, 1.f, true);
    for (std::uint32_t tick : {2, 3, 100, 200, 1000})
    {
        const auto frame = polled.advance(tick, 1.f, true);
        EXPECT_EQ(frame.mInputDelta, 0.f);
        EXPECT_EQ(frame.mSubstepCount, 0u);
        EXPECT_EQ(polled.remaining(), remaining);
    }
    const auto a = once.advance(1001, 1.f, false);
    const auto b = polled.advance(1001, 1.f, false);
    EXPECT_EQ(a.mInputDelta, b.mInputDelta);
    EXPECT_EQ(a.mSubstepSeconds, b.mSubstepSeconds);
    EXPECT_EQ(a.mSubstepCount, b.mSubstepCount);
    EXPECT_EQ(once.remaining(), polled.remaining());
    // Native outer resume rebasing can produce a capped first update.
    EXPECT_EQ(std::bit_cast<std::uint32_t>(b.mInputDelta), 1042938856u);
}

TEST(NativeFrameClock, RejectedScaleDoesNotConsumeHostTimeOrPreparedCarry)
{
    OMW::NativeFrameClock clock(100, .0167f, .05f, 0), uninterrupted = clock;
    clock.advance(101, 1.f, false);
    uninterrupted.advance(101, 1.f, false);
    EXPECT_THROW(clock.advance(130, std::numeric_limits<float>::quiet_NaN(), false), std::invalid_argument);
    EXPECT_THROW(clock.advance(130, -1.f, false), std::invalid_argument);
    EXPECT_EQ(clock.remaining(), uninterrupted.remaining());
    const auto a = clock.advance(140, .5f, false);
    const auto b = uninterrupted.advance(140, .5f, false);
    EXPECT_EQ(a.mInputDelta, b.mInputDelta);
    EXPECT_EQ(a.mSubstepSeconds, b.mSubstepSeconds);
    EXPECT_EQ(a.mSubstepCount, b.mSubstepCount);
    EXPECT_EQ(clock.remaining(), uninterrupted.remaining());
}

TEST(NativeFrameClock, ProcessSettingsRejectUnsupportedDurationsBeforeFirstFrame)
{
    for (float fixed : {0.f, -1.f, std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::denorm_min()})
        EXPECT_THROW((OMW::NativeFrameClock(0, fixed, .05f, 0)), std::invalid_argument);
    EXPECT_THROW((OMW::NativeFrameClock(0, .0167f, std::numeric_limits<float>::quiet_NaN(), 0)),
        std::invalid_argument);
    OMW::NativeFrameClock clock(0, 2.f, .05f, 0);
    const auto frame = clock.advance(1000, 1.f, false);
    EXPECT_EQ(frame.mSubstepCount, 0u); // capped timer delta is below half the capped1s fixed step.
}

TEST(NativeFrameClock, SceneComplexityIsConsumedBeforeScriptResetAndLimitsOnlyTheFollowingFrame)
{
    // Original40D8B0 + full889810 and40DBB9 reset/callback slice:
    // raw mode0, heavy delta, callbacks(None,1,None), then no setters.
    OMW::NativeFrameClock clock(0, .0167f, .05f, 0);
    MWMechanics::OblivionCombatService combat;
    const auto first = clock.advance(167, 1.f, false, combat.sceneIsComplex());
    EXPECT_EQ(first.mSubstepCount, 3u);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(first.mFrameSeconds), 1028470184u);
    combat.beginScriptActorFrame();
    combat.setSceneIsComplex(true);
    const auto second = clock.advance(334, 1.f, false, combat.sceneIsComplex());
    EXPECT_EQ(second.mSubstepCount, 2u);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(second.mFrameSeconds), 1023987312u);
    combat.beginScriptActorFrame();
    const auto third = clock.advance(501, 1.f, false, combat.sceneIsComplex());
    EXPECT_EQ(third.mSubstepCount, 3u);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(third.mFrameSeconds), 1028470184u);
    EXPECT_FALSE(combat.sceneIsComplex());
}
