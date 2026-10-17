#include <components/esm4/firstpersonview.hpp>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <limits>

namespace
{
    struct OriginalSample
    {
        ESM4::FirstPersonRootInput mInput;
        unsigned mFollow;
        std::array<unsigned, 9> mRotation;
    };
    const OriginalSample samples[]{
#include "firstpersonview_expected.inc"
    };

    TEST(ESM4FirstPersonView, MatchesOriginalFullRootRotationAndFollowCache)
    {
        // Deterministic selection from 15,120 independent full-function PC007F
        // observations; high group bits, recovery, overshoot, yaw and both pitches.
        for (const auto& sample : samples)
        {
            const auto result = ESM4::updateFirstPersonRootRotation(sample.mInput);
            ASSERT_TRUE(result);
            EXPECT_EQ(std::bit_cast<unsigned>(result->mFollow), sample.mFollow);
            for (unsigned i = 0; i < 9; ++i)
                EXPECT_EQ(std::bit_cast<unsigned>(result->mRotation.mRotation.mValues[i / 3][i % 3]),
                    sample.mRotation[i]);
        }
    }

    TEST(ESM4FirstPersonView, RejectsInvalidConsumedInputs)
    {
        using Input = ESM4::FirstPersonRootInput;
        for (float invalid : {std::numeric_limits<float>::infinity(),
                 -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            for (auto field : {&Input::mPreviousFollow, &Input::mElapsed, &Input::mPitch,
                     &Input::mYaw, &Input::mSecondaryYaw})
            {
                Input input;
                input.*field = invalid;
                EXPECT_FALSE(ESM4::updateFirstPersonRootRotation(input));
            }
            EXPECT_FALSE(ESM4::updateFirstPersonRootRotation({}, {invalid, 2.f}));
            EXPECT_FALSE(ESM4::updateFirstPersonRootRotation({}, {.75f, invalid}));
        }
        Input input;
        input.mElapsed = -1.f;
        EXPECT_FALSE(ESM4::updateFirstPersonRootRotation(input));
        EXPECT_FALSE(ESM4::updateFirstPersonRootRotation({}, {.75f, 0.f}));
        EXPECT_FALSE(ESM4::updateFirstPersonRootRotation({}, {.75f, -1.f}));
        for (float angle : {-0x1p63f, 0x1p63f, std::numeric_limits<float>::max()})
        {
            input = {};
            input.mPitch = angle;
            EXPECT_FALSE(ESM4::updateFirstPersonRootRotation(input));
            input = {};
            input.mYaw = angle;
            EXPECT_FALSE(ESM4::updateFirstPersonRootRotation(input));
        }
    }
}
