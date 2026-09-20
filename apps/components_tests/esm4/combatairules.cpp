#include <components/esm4/combatairules.hpp>
#include <components/esm4/combatsettings.hpp>
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
    const ESM4::FightScoreSettings installed{50, -1, -55, 1, 1, -.005f, -25, 1, 2};
}

TEST(ESM4CombatAIRules, AggressionGateUpperCapAndNegativeScores)
{
    ESM4::FightScoreInput input{50, 100, 0, 0, true, false, 100};
    EXPECT_EQ(ESM4::fightScore(input, installed), 0);
    input.mAggression = -1;
    EXPECT_EQ(ESM4::fightScore(input, installed), 0);
    input.mAggression = 1;
    input.mApplyFriendDisposition = false;
    EXPECT_EQ(ESM4::fightScore(input, installed), -54);
    input.mAggression = 100;
    input.mApplyFriendDisposition = true;
    EXPECT_EQ(ESM4::fightScore(input, installed), 100);
}

TEST(ESM4CombatAIRules, FriendAndResponsibilityComparisonsAreStrict)
{
    ESM4::FightScoreInput input{50, 100, 50, 0, true, true, 50};
    EXPECT_EQ(ESM4::fightScore(input, installed), -5); // Responsibility equality excludes friend term.
    input.mResponsibility = 49;
    EXPECT_EQ(ESM4::fightScore(input, installed), 70);
    input.mTargetDisposition = 100;
    EXPECT_EQ(ESM4::fightScore(input, installed), -55); // Disposition equality also excludes it.
    input = {0, 170, 50, 0, true, true, 100};
    auto settings = installed;
    settings.mResponsibilityMultiplier = 1.7f;
    EXPECT_EQ(ESM4::fightScore(input, settings), 45); // Unrounded product is greater than 170.
    input.mFriendDisposition = 171;
    EXPECT_EQ(ESM4::fightScore(input, settings), 100);
    input.mApplyFriendDisposition = false;
    EXPECT_EQ(ESM4::fightScore(input, settings), 45);
}

TEST(ESM4CombatAIRules, DistanceClampAndFinalSumDoNotAddAnExtraFloatStore)
{
    ESM4::FightScoreInput input{50, 0, 50, 0, false, false, 0};
    EXPECT_EQ(ESM4::fightScore(input, installed), -5);
    input.mDistance = 199;
    EXPECT_EQ(ESM4::fightScore(input, installed), -5); // Positive distance term is clamped to zero.
    input.mDistance = 200;
    EXPECT_EQ(ESM4::fightScore(input, installed), -5); // Slightly positive distance term is still clamped.
    input.mDistance = 201;
    EXPECT_EQ(ESM4::fightScore(input, installed), -5);
    input.mDistance = 1500;
    EXPECT_EQ(ESM4::fightScore(input, installed), -11);
    auto settings = installed;
    settings.mDispositionBase = 0;
    settings.mDispositionMultiplier = 0;
    settings.mAggressionBase = -1;
    settings.mAggressionMultiplier = 0;
    settings.mDistanceBase = -std::nextafter(1.f, 0.f);
    settings.mDistanceMultiplier = 0;
    EXPECT_EQ(ESM4::fightScore(input, settings), -1);
    settings.mAggressionBase = -16777216.f;
    settings.mDistanceBase = -1.5f;
    EXPECT_EQ(ESM4::fightScore(input, settings), -16777217);
}

TEST(ESM4CombatAIRules, InvalidInputsAndOverflowAreDiagnosed)
{
    ESM4::FightScoreInput input{0, 0, 1, 0, false, false, 0};
    for (float invalid : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        input.mDistance = invalid;
        EXPECT_THROW(ESM4::fightScore(input, installed), std::invalid_argument);
    }
    input.mDistance = 0;
    auto settings = installed;
    settings.mAggressionMultiplier = std::numeric_limits<float>::infinity();
    EXPECT_THROW(ESM4::fightScore(input, settings), std::invalid_argument);
    settings.mAggressionMultiplier = std::numeric_limits<float>::max();
    input.mAggression = 2;
    EXPECT_THROW(ESM4::fightScore(input, settings), std::overflow_error);
    settings = installed;
    settings.mAggressionBase = 2147483648.f;
    EXPECT_THROW(ESM4::fightScore(input, settings), std::overflow_error);
}
