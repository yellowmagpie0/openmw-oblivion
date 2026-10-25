#include <components/esm4/nativeclampsequence.hpp>

#include <gtest/gtest.h>

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
