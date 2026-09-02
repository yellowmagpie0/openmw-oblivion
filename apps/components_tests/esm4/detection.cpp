#include <gtest/gtest.h>

#include <limits>

#include <components/esm4/detection.hpp>

namespace
{
    ESM4::DetectionInput baseInput()
    {
        ESM4::DetectionInput input;
        input.mObserverSneak = 60;
        input.mObserverAgility = 60;
        input.mObserverLuck = 50;
        input.mTargetSneak = 20;
        input.mDistance = 300;
        input.mMaximumDistance = 3000;
        input.mViewAngleRadians = 0.1;
        input.mViewConeRadians = 1.2;
        input.mAmbientLight = 0.8;
        input.mTargetIllumination = 0.8;
        input.mMovementSpeed = 30;
        input.mRandomSample = 0.0;
        return input;
    }
}

TEST(ESM4Detection, FixedVectorIsStableAndBounded)
{
    const ESM4::DetectionResult result = ESM4::calculateDetection(baseInput());
    EXPECT_TRUE(result.mValid);
    EXPECT_DOUBLE_EQ(result.mLevel, ESM4::calculateDetection(baseInput()).mLevel);
    EXPECT_GE(result.mLevel, 0.0);
    EXPECT_LE(result.mLevel, 100.0);
}

TEST(ESM4Detection, LineOfSightIsAHardGateAndInvalidInputsDoNotEscape)
{
    auto input = baseInput();
    input.mLineOfSight = false;
    EXPECT_EQ(ESM4::calculateDetection(input), (ESM4::DetectionResult{ 0.0, false, true }));

    input = baseInput();
    input.mDistance = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE(ESM4::calculateDetection(input).mValid);
}

TEST(ESM4Detection, AbilityLightAndDistanceAreMonotonic)
{
    auto input = baseInput();
    const double baseline = ESM4::calculateDetection(input).mLevel;

    input.mObserverAgility += 20;
    EXPECT_GE(ESM4::calculateDetection(input).mLevel, baseline);
    input = baseInput();
    input.mTargetSneak += 50;
    EXPECT_LE(ESM4::calculateDetection(input).mLevel, baseline);
    input = baseInput();
    input.mDistance += 600;
    EXPECT_LE(ESM4::calculateDetection(input).mLevel, baseline);
    input = baseInput();
    input.mAmbientLight = 0.1;
    EXPECT_LE(ESM4::calculateDetection(input).mLevel, baseline);
}

TEST(ESM4Detection, RandomSampleOnlyAffectsThresholdDecision)
{
    auto input = baseInput();
    input.mRandomSample = 0.0;
    const auto visible = ESM4::calculateDetection(input);
    input.mRandomSample = 1.0;
    const auto unlikely = ESM4::calculateDetection(input);
    EXPECT_DOUBLE_EQ(visible.mLevel, unlikely.mLevel);
    EXPECT_GE(visible.mDetected, unlikely.mDetected);
}
