#include <gtest/gtest.h>

#include <limits>
#include <cmath>
#include <stdexcept>
#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadgmst.hpp>

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

namespace
{
    ESM4::NativeDetectionSettings installedDetectionSettings()
    {
        return {1500, 2, 14, 1, 25, 1.3f, 1, 1.6f, -5, 1.4f, .5f, 100, .5f, -10, -25};
    }
}

TEST(ESM4Detection, NativeAwarenessMatchesIndependentOriginalInstructionCases)
{
    struct Case { ESM4::NativeDetectionInput mInput; int mExpected; };
    // Original 005463F0 machine instructions, independent emulation with both
    // 53/64-bit x87 precision controls; see M15 native rule provenance.
    const std::array cases{
        Case{{60, 20, true, 300, 0, 50, 0, 0, false, true, false, false, false, false, false, false}, 39}, // baseline
        Case{{60, 20, false, 300, 0, 50, 0, 0, false, true, false, false, false, false, false, false}, -11}, // no-los
        Case{{60, 20, true, 0, 0, 50, 0, 0, false, true, false, false, false, false, false, false}, 58}, // near
        Case{{60, 20, true, 1500, 0, 50, 0, 0, false, true, false, false, false, false, false, false}, -35}, // at-limit
        Case{{60, 20, true, 1501, 0, 50, 0, 0, false, true, false, false, false, false, false, false}, 0}, // beyond-limit
        Case{{60, 20, true, 300, 100, 50, 0, 0, false, true, false, false, false, false, false, false}, -11}, // blind
        Case{{60, 20, true, 300, 0, 0, 0, 0, false, true, false, false, false, false, false, false}, -11}, // dark
        Case{{60, 20, true, 300, 0, 50, 50, 0, false, true, false, false, false, false, false, false}, 14}, // chameleon
        Case{{60, 20, true, 300, 0, 50, 0, 0, false, false, false, false, false, false, false, false}, 49}, // not-sneaking
        Case{{60, 20, true, 300, 0, 50, 0, 0, false, true, true, false, false, false, false, false}, 139}, // attacking
        Case{{60, 20, true, 300, 0, 50, 0, 0, false, true, false, true, false, false, false, false}, 71}, // in-combat
        Case{{60, 20, true, 300, 0, 50, 0, 0, false, true, false, false, false, true, false, false}, 14}, // underwater-observer
        Case{{60, 20, true, 300, 0, 50, 0, 0, false, true, false, false, false, false, true, false}, -21}, // sleeping-observer
        Case{{60, 20, true, 300, 0, 50, 0, 0, false, true, false, false, false, false, false, true}, 48}, // exterior
        Case{{60, 100, true, 300, 0, 50, 0, 0, false, true, false, false, false, false, false, false}, 0}, // high-target-skill
        Case{{100, 20, true, 300, 0, 50, 0, 0, false, true, false, false, false, false, false, false}, 55}, // high-observer-skill
        Case{{60, 20, true, 300, 0, 50, 0, 10, true, true, false, false, false, false, false, false}, 70}, // moving
        Case{{60, 20, true, 300, 0, 50, 0, 10, true, true, false, false, true, false, false, false}, 79}, // running
        Case{{60, 20, false, 300, 0, 50, 0, 10, true, true, false, false, true, false, false, false}, 28}, // audible-no-los
        Case{{60, 20, true, 2000, 0, 50, 0, 0, false, true, true, false, false, false, false, false}, 55}, // attacking-far
    };
    for (const auto& c : cases)
        EXPECT_EQ(ESM4::nativeDetectionAwareness(c.mInput, installedDetectionSettings()), c.mExpected);
}

TEST(ESM4Detection, NativeAwarenessPreservesSignedRangeAndPositiveSubunitDetection)
{
    const ESM4::NativeDetectionInput input{};
    auto settings = installedDetectionSettings();
    settings.mLightOffset = 0;
    for (float base : {-1000.f, -1.f, std::nextafter(-1.f, 0.f), -.5f, 0.f,
             std::nextafter(0.f, 1.f), .5f, std::nextafter(1.f, 0.f), 1.f, 1000.f})
    {
        settings.mBase = base;
        const int expected = base > 0 && base < 1 ? 1 : static_cast<int>(base);
        EXPECT_EQ(ESM4::nativeDetectionAwareness(input, settings), expected);
    }
    settings = installedDetectionSettings();
    ESM4::NativeDetectionInput distance{};
    distance.mDistance = 1500;
    EXPECT_EQ(ESM4::nativeDetectionAwareness(distance, settings), -25);
    distance.mDistance = std::nextafter(1500.f, 2000.f);
    EXPECT_EQ(ESM4::nativeDetectionAwareness(distance, settings), 0);
    distance.mTargetAttacking = true;
    EXPECT_EQ(ESM4::nativeDetectionAwareness(distance, settings), 75);
    distance.mTargetAttacking = false; distance.mExterior = true;
    EXPECT_EQ(ESM4::nativeDetectionAwareness(distance, settings), -25);
}

TEST(ESM4Detection, SneakMasteryChangesBootAndMovementInputsAtBaseThresholds)
{
    const auto mastery = ESM4::buildCombatMasterySettings({});
    for (int skill : {0, 1, 24, 25, 49, 50, 51, 74, 75, 76, 99, 100, 101})
        for (bool sneaking : {false, true})
            for (bool moving : {false, true})
                for (bool running : {false, true})
                {
                    const auto noise = ESM4::sneakDetectionNoise(skill, sneaking, 10, moving, running, mastery);
                    EXPECT_EQ(noise.mBootWeight, sneaking && skill >= 50 ? 0 : 10);
                    EXPECT_EQ(noise.mMoving, moving && !(sneaking && skill >= 75));
                    EXPECT_EQ(noise.mRunning, running && !(sneaking && skill >= 75));
                }
    EXPECT_EQ(ESM4::sneakDetectionNoise(20, true, 10, true, true, {{10,20,30,40}}).mBootWeight, 0);
    EXPECT_FALSE(ESM4::sneakDetectionNoise(30, true, 10, true, true, {{10,20,30,40}}).mMoving);
    EXPECT_THROW(ESM4::sneakDetectionNoise(0, false, -1, false, false, mastery), std::invalid_argument);
}

TEST(ESM4Detection, NativeSettingsAndInvalidAwarenessInputsAreChecked)
{
    const auto settings = ESM4::buildNativeDetectionSettings({});
    EXPECT_EQ(settings.mBootWeightBase, 7); EXPECT_EQ(settings.mSkillMultiplier, .75f);
    EXPECT_EQ(settings.mSoundWithoutLosMultiplier, .5f); EXPECT_EQ(settings.mLightMultiplier, 1.2f);
    EXPECT_EQ(settings.mSleepBonus, -25); EXPECT_EQ(settings.mMaximumDistance, 1500);
    for (float bad : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        ESM4::NativeDetectionInput input{}; input.mDistance = bad;
        EXPECT_THROW(ESM4::nativeDetectionAwareness(input, settings), std::invalid_argument);
    }
    auto invalid = settings; invalid.mMaximumDistance = 0;
    EXPECT_THROW(ESM4::nativeDetectionAwareness({}, invalid), std::invalid_argument);
    invalid = settings; invalid.mExteriorDistanceMultiplier = 0;
    EXPECT_THROW(ESM4::nativeDetectionAwareness({}, invalid), std::invalid_argument);
    invalid = settings; invalid.mBase = std::numeric_limits<float>::max();
    EXPECT_THROW(ESM4::nativeDetectionAwareness({}, invalid), std::invalid_argument);
    ESM4::GameSetting value{}; value.mEditorId = "fSneakBootWeightBase"; value.mData = 14.f;
    const std::array<const ESM4::GameSetting*, 1> values{&value};
    EXPECT_EQ(ESM4::buildNativeDetectionSettings(values).mBootWeightBase, 14);
    value.mData = std::int32_t{14};
    EXPECT_THROW(ESM4::buildNativeDetectionSettings(values), std::invalid_argument);
}
