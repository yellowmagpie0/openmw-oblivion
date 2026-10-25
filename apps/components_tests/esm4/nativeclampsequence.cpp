#include <components/esm4/nativesequencedeactivation.hpp>
#include <components/esm4/nativeclampsequence.hpp>
#include <components/esm4/nativeclampframe.hpp>

#include <gtest/gtest.h>

#include <bit>
#include <limits>
#include <stdexcept>

namespace
{
    ESM4::NativeClampSequenceState tick(const ESM4::NativeClampSequenceState& state, float clock,
        float retirement = .25f, bool freeze = false)
    {
        return ESM4::advanceNativeClampSequence(state, clock, 1, 0, 2, {0, 1}, retirement, freeze);
    }
}

TEST(ESM4NativeClampSequence, GetUpEndDetachesBeforeRetainedSequenceBecomesInactive)
{
    const ESM4::NativeClampSequenceState fresh;
    const auto initialized = tick(fresh, .5f);
    EXPECT_EQ(fresh, ESM4::NativeClampSequenceState{});
    EXPECT_FALSE(initialized.mStarted);
    EXPECT_EQ(initialized.mTiming.mOffset, -.5f);
    const auto playing = tick(initialized, 1);
    EXPECT_TRUE(playing.mStarted);
    EXPECT_FALSE(playing.mRetirement);
    const auto end = tick(playing, 1.5f);
    ASSERT_TRUE(end.mRetirement);
    EXPECT_EQ(end.mRetirement->mStartClock, 1.5f);
    EXPECT_FALSE(end.mInactive);
    EXPECT_EQ(end.mTiming.mOutputTime, 1);
    const auto tail = tick(end, 1.625f);
    EXPECT_FALSE(tail.mInactive);
    EXPECT_EQ(tail.mTiming.mOutputTime, 1.125f);
    const auto done = tick(tail, 1.75f);
    EXPECT_TRUE(done.mInactive);
    EXPECT_EQ(done.mTiming, tail.mTiming);
    EXPECT_EQ(tick(done, 100), done);
}

TEST(ESM4NativeClampSequence, ClampedOutputDoesNotSubstituteForEndKeyOrFrequency)
{
    auto state = ESM4::advanceNativeClampSequence({}, 0, 2, 0, 1, {0, 2}, .25f);
    state = ESM4::advanceNativeClampSequence(state, 1, 2, 0, 1, {0, 2}, .25f);
    EXPECT_EQ(state.mTiming.mOutputTime, 1);
    EXPECT_FALSE(state.mRetirement);
    EXPECT_FALSE(state.mInactive);
    state = ESM4::advanceNativeClampSequence(state, 2, 2, 0, 1, {0, 2}, .25f);
    EXPECT_TRUE(state.mRetirement);
}

TEST(ESM4NativeClampSequence, PoseCaptureEasesThenUsesItsResolvedOffsetForEnd)
{
    ESM4::NativeClampSequenceState state;
    state.mTiming.mEasing = true;
    state.mTiming.mOffset = 0;
    state.mTiming.mEaseEnd = .25f;
    state = ESM4::advanceNativeClampSequence(state, .5f, 1, 0, 2, {0, 1}, .25f, false, 0);
    EXPECT_TRUE(state.mTiming.mEasing);
    EXPECT_EQ(state.mTiming.mOutputTime, 0);
    EXPECT_FALSE(state.mStarted);
    state = ESM4::advanceNativeClampSequence(state, .75f, 1, 0, 2, {0, 1}, .25f, false, 0);
    EXPECT_FALSE(state.mTiming.mEasing);
    EXPECT_EQ(state.mTiming.mOffset, -.75f);
    state = tick(state, 1.75f);
    EXPECT_TRUE(state.mRetirement);
    EXPECT_FALSE(state.mInactive);
}

TEST(ESM4NativeClampSequence, FreezeSkipsKeysButManagerAndRetirementStillAdvance)
{
    auto state = tick({}, 0);
    state = tick(state, 1.5f, .25f, true);
    EXPECT_FALSE(state.mStarted);
    EXPECT_FALSE(state.mRetirement);
    EXPECT_EQ(state.mTiming.mOutputTime, 1.5f);
    state = tick(state, 1.5f);
    ASSERT_TRUE(state.mRetirement);
    EXPECT_TRUE(tick(state, 1.75f, .25f, true).mInactive);
    // Nonpositive signed blend durations deactivate immediately at End.
    for (float duration : {0.f, -1.f})
    {
        const auto done = tick(tick({}, 0), 1, duration);
        EXPECT_TRUE(done.mInactive);
        EXPECT_FALSE(done.mRetirement);
    }
}

TEST(ESM4NativeClampSequence, ValidatesPartialAndRetiredStateWithoutMutatingInput)
{
    auto state = tick({}, 0);
    const auto old = state;
    for (float value : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        EXPECT_THROW(tick(state, value), std::invalid_argument);
    EXPECT_THROW(ESM4::advanceNativeClampSequence(state, 1, 0, 0, 2, {0, 1}, .1f), std::invalid_argument);
    EXPECT_THROW(ESM4::advanceNativeClampSequence(state, 1, 1, 2, 1, {0, 1}, .1f), std::invalid_argument);
    EXPECT_THROW(ESM4::advanceNativeClampSequence(state, 1, 1, 0, 2, {1, 0}, .1f), std::invalid_argument);
    EXPECT_EQ(state, old);
    state.mTiming.mLastInput.reset();
    EXPECT_THROW(tick(state, 1), std::invalid_argument);
    state = tick(old, 1);
    ASSERT_TRUE(state.mRetirement);
    state.mRetirement->mDuration = 0;
    EXPECT_THROW(tick(state, 2), std::invalid_argument);
    state = tick(old, 1);
    state.mInactive = true;
    EXPECT_THROW(tick(state, 1), std::invalid_argument);
}

TEST(ESM4NativeClampSequence, InactiveAndImmediateEndDoNotAdvanceOverflowingManagerTime)
{
    auto state = tick({}, 0);
    state.mInactive = true;
    // Original6CA950 state0 returns without touching any sequence field.
    // Advancing this finite clock at frequency2 would overflow a float store.
    for (float clock : {std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()})
        EXPECT_EQ(ESM4::advanceNativeClampSequence(state, clock, 2, 0, 2, {0, 1}, 0), state);
    state.mInactive = false;
    const auto done = ESM4::advanceNativeClampSequence(state, std::numeric_limits<float>::max(),
        2, 0, 2, {0, 1}, 0);
    EXPECT_TRUE(done.mInactive);
    EXPECT_EQ(done.mTiming, state.mTiming);
}

TEST(ESM4NativeClampSequence, DeactivationRoundsPC24WithExtendedExponentBeforeFinalStore)
{
    ESM4::MeleeSequenceTiming timing;
    timing.mOffset = -.5f; timing.mLastInput = 9.5f;
    timing.mWeightedTime = std::bit_cast<float>(std::uint32_t{1087687885});
    const auto old = timing;
    EXPECT_EQ(std::bit_cast<std::uint32_t>(*ESM4::deactivateNativeSequenceTiming(timing, .7f).mOffset),
        3204448256u);
    EXPECT_EQ(timing, old);
    timing.mOffset.reset(); timing.mLastInput = std::numeric_limits<float>::max();
    timing.mWeightedTime = std::numeric_limits<float>::max();
    // Original registers retain values above FLT_MAX before cancellation.
    EXPECT_EQ(*ESM4::deactivateNativeSequenceTiming(timing, .5f).mOffset, 0.f);
    timing.mOffset = 0;
    EXPECT_EQ(*ESM4::deactivateNativeSequenceTiming(timing, .5f).mOffset, std::numeric_limits<float>::max());
    timing.mOffset.reset();
    EXPECT_THROW(ESM4::deactivateNativeSequenceTiming(timing, .25f), std::invalid_argument);
    EXPECT_THROW(ESM4::deactivateNativeSequenceTiming(timing, 0), std::invalid_argument);
    timing.mLastInput.reset();
    EXPECT_EQ(ESM4::deactivateNativeSequenceTiming(timing, .5f), timing);
}

TEST(ESM4NativeClampSequence, ColdExternalRetirementAnchorsOnlyAtFirstManagerUpdate)
{
    const auto retired = ESM4::retireNativeClampSequence({}, 1, .1f);
    ASSERT_TRUE(retired.mRetirement);
    EXPECT_FALSE(retired.mRetirement->mStartClock);
    EXPECT_FALSE(retired.mTiming.mOffset);
    EXPECT_FALSE(retired.mTiming.mLastInput);
    EXPECT_FALSE(retired.mInactive);
    const auto first = ESM4::advanceNativeClampSequence(retired, .5f, 1, 0, 2, {0, 1}, .1f, true);
    EXPECT_EQ(first.mTiming.mOffset, -.5f);
    EXPECT_EQ(first.mRetirement->mStartClock, .5f);
    EXPECT_EQ(first.mTiming.mEaseEnd, .6f);
    EXPECT_FALSE(first.mInactive);
    EXPECT_TRUE(ESM4::advanceNativeClampSequence(first, .6f, 1, 0, 2, {0, 1}, .1f, true).mInactive);
}

TEST(ESM4NativeClampSequence, ExternalPoseRetirementKeepsDestinationUntilInactive)
{
    ESM4::NativeClampSequenceState pose;
    pose.mTiming = {true, 0.f, std::nullopt, .1f, std::nullopt,
        -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};
    auto state = ESM4::retireNativeClampSequence(pose, 1, .1f, 0);
    EXPECT_FALSE(state.mTiming.mEasing);
    EXPECT_FALSE(state.mTiming.mLastInput);
    state = ESM4::advanceNativeClampSequence(state, .5f, 1, 0, 2, {0, 1}, .1f, true, 0);
    EXPECT_EQ(state.mTiming.mOutputTime, 0);
    state = ESM4::advanceNativeClampSequence(state, .55f, 1, 0, 2, {0, 1}, .1f, true, 0);
    EXPECT_EQ(state.mTiming.mOutputTime, 0);
    EXPECT_EQ(state.mTiming.mOffset, 0);
    state = ESM4::advanceNativeClampSequence(state, .6f, 1, 0, 2, {0, 1}, .1f, true, 0);
    EXPECT_TRUE(state.mInactive);
    EXPECT_THROW(ESM4::validateNativeClampSequenceState(state, 0, .6f), std::invalid_argument);
    EXPECT_NO_THROW(ESM4::validateNativeClampSequenceState(state, std::nullopt, .6f));
}

TEST(ESM4NativeClampSequence, ImmediatePoseCancellationRetainsUninitializedHistoryWithoutManagerArithmetic)
{
    ESM4::NativeClampSequenceState pose;
    pose.mTiming = {true, 0.f, std::nullopt, .1f, std::nullopt,
        -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};
    const auto stopped = ESM4::retireNativeClampSequence(pose, 1, 0, 0);
    EXPECT_TRUE(stopped.mInactive);
    EXPECT_FALSE(stopped.mTiming.mEasing);
    EXPECT_FALSE(stopped.mTiming.mLastInput);
    EXPECT_FALSE(stopped.mTiming.mEaseStart);
    EXPECT_EQ(ESM4::advanceNativeClampSequence(stopped, std::numeric_limits<float>::max(),
        2, 0, 2, {0, 1}, 0, true), stopped);
}

TEST(ESM4NativeClampSequence, CachedActivationRetainsPriorInputAndOutputUntilManagerUpdate)
{
    ESM4::NativeClampSequenceState cached;
    cached.mTiming = {false, std::nullopt, std::nullopt, .1f, 0.f, 0.f, 0.f};
    const auto frame = ESM4::advanceNativeClampSequence(cached, .525f, 1, 0, 2, {0, 1}, .1f, true);
    EXPECT_EQ(frame.mTiming.mOffset, -.525f);
    EXPECT_EQ(frame.mTiming.mLastInput, 0.f);
    EXPECT_EQ(frame.mTiming.mEaseStart, 0.f);
    EXPECT_EQ(frame.mTiming.mOutputTime, 0.f);
    ESM4::NativeClampSequenceState firstOutput;
    firstOutput.mTiming = {false, -.5f, std::nullopt, -std::numeric_limits<float>::max(), 0.f, 0.f, 0.f};
    EXPECT_NO_THROW(ESM4::validateNativeClampSequenceState(firstOutput));
}

TEST(ESM4NativeClampSequence, LargeFirstRetirementClockExpiresBeforeInputUpdate)
{
    for (float clock : {1e8f, std::numeric_limits<float>::max()})
    {
        const auto state = ESM4::advanceNativeClampSequence(
            ESM4::retireNativeClampSequence({}, 2, .1f), clock, 2, 0, 2, {0, 1}, .1f, true);
        EXPECT_TRUE(state.mInactive);
        EXPECT_EQ(state.mTiming.mOffset, -clock);
        EXPECT_FALSE(state.mTiming.mLastInput);
        EXPECT_EQ(state.mTiming.mEaseStart, clock);
        EXPECT_EQ(state.mTiming.mEaseEnd, clock);
        EXPECT_NO_THROW(ESM4::validateNativeClampSequenceState(state, std::nullopt, clock));
    }
}

TEST(ESM4NativeClampSequence, ResidentQueryPrimesConstructorHistoryBeforeManagerWithoutPublishingInfinity)
{
    const float sentinel = -std::numeric_limits<float>::max();
    for (float frequency : {.7f, 1.f, 2.f})
        for (bool literalSentinels : {false, true})
        {
            ESM4::NativeClampSequenceState state;
            state.mTiming.mEaseEnd = state.mTiming.mWeightedTime = state.mTiming.mOutputTime = sentinel;
            if (literalSentinels)
                state.mTiming.mOffset = state.mTiming.mEaseStart = state.mTiming.mLastInput = sentinel;
            const auto old = state;
            const float clock = std::bit_cast<float>(std::uint32_t{1057244228});
            const auto frame = ESM4::advanceNativeResidentClampSequence(state, .5f, clock,
                frequency, 0, 2, {0, 1}, .1f);
            EXPECT_FALSE(frame.mStarted); EXPECT_FALSE(frame.mInactive); EXPECT_FALSE(frame.mRetirement);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(*frame.mTiming.mOffset), 3204727876u);
            EXPECT_EQ(frame.mTiming.mEaseStart, 0.f);
            EXPECT_EQ(frame.mTiming.mLastInput, 0.f);
            EXPECT_EQ(frame.mTiming.mWeightedTime, 0.f);
            EXPECT_EQ(frame.mTiming.mOutputTime, 0.f);
            EXPECT_EQ(frame.mTiming.mEaseEnd, sentinel);
            EXPECT_EQ(state, old);
        }
}

TEST(ESM4NativeClampSequence, ResidentQueryReplacesCachedHistoryBeforeFirstManagerUpdate)
{
    // Original full-frame04: same-resource request1, three prior frames, frequency.7.
    const auto value = [](std::uint32_t bits) { return std::bit_cast<float>(bits); };
    ESM4::NativeClampSequenceState state;
    state.mTiming = {false, std::nullopt, std::nullopt, .1f,
        value(1023969408), value(1019159936), value(1019159936)};
    const auto frame = ESM4::advanceNativeResidentClampSequence(state, value(1057803468),
        value(1058083088), .7f, 0, 2, {0, 1}, .1f);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(*frame.mTiming.mOffset), 3205566736u);
    EXPECT_EQ(frame.mTiming.mEaseStart, 0.f);
    EXPECT_EQ(frame.mTiming.mLastInput, 0.f);
    EXPECT_EQ(frame.mTiming.mWeightedTime, 0.f);
    EXPECT_EQ(frame.mTiming.mOutputTime, 0.f);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(frame.mTiming.mEaseEnd), 1059760810u);
}

TEST(ESM4NativeClampSequence, PositiveRetirementKeepsOffsetUntilImmediateDeactivation)
{
    const auto value = [](std::uint32_t bits) { return std::bit_cast<float>(bits); };
    ESM4::NativeClampSequenceState state;
    state.mTiming = {false, value(3229334446), value(1059928730), -std::numeric_limits<float>::max(),
        value(1065073800), value(1060124467), value(1060124467)};
    const auto old = state;
    const auto positive = ESM4::retireNativeClampSequence(state, .7f, .1f);
    EXPECT_FALSE(positive.mInactive); ASSERT_TRUE(positive.mRetirement);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(*positive.mTiming.mOffset), 3229334446u);
    EXPECT_FALSE(positive.mTiming.mEaseStart);
    const auto immediate = ESM4::retireNativeClampSequence(state, .7f, 0);
    EXPECT_TRUE(immediate.mInactive);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(*immediate.mTiming.mOffset), 3229334444u);
    EXPECT_EQ(state, old);
    EXPECT_THROW(ESM4::advanceNativeResidentClampSequence(positive, 5, 6, .7f, 0, 2, {0, 1}, .1f),
        std::invalid_argument);
    EXPECT_THROW(ESM4::advanceNativeResidentClampSequence(immediate, 5, 6, .7f, 0, 2, {0, 1}, .1f),
        std::invalid_argument);
}
