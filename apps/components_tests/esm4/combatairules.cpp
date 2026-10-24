#include <components/esm4/combatairules.hpp>
#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadgmst.hpp>
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <random>
#include <array>

namespace
{
    const ESM4::FightScoreSettings installed{50, -1, -55, 1, 1, -.005f, -25, 1, 2};
    const ESM4::DispositionInput neutralDisposition{50, 50, 50, 0, 0, 0, 0, 0, -1, false, false, 0, false};
}

TEST(ESM4CombatAIRules, DispositionAveragesPersonalityBeforeTruncationAndLeavesWrapperClampToCaller)
{
    const auto settings = ESM4::buildDispositionSettings({});
    auto input = neutralDisposition;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 50);
    input.mActorPersonality = 51;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 50);
    input.mActorPersonality = -151;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), -50);
    input.mModifier = 200;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 150);
}

TEST(ESM4CombatAIRules, DispositionReputationTermsCapSeparatelyAndUseNativeRepeatedInfamyFactor)
{
    const auto settings = ESM4::buildDispositionSettings({});
    auto input = neutralDisposition;
    input.mTargetFame = 100;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 70);
    input.mTargetFame = -100;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 30);
    input.mTargetFame = 0;
    input.mTargetInfamy = 100;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 30);
    input.mTargetInfamy = 10;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 47); // Float term is -2.1 before final truncation.
    input.mActorResponsibility = 0;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 50); // (-.3) * (-.3 * 10), not (-1) * (-.3 * 10).
    input.mTargetInfamy = 0;
    input.mTargetBounty = 10000;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 70);
    input.mActorResponsibility = 100;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 30);
}

TEST(ESM4CombatAIRules, DispositionFactionMultiplierAppearsInBothFactors)
{
    auto settings = ESM4::buildDispositionSettings({});
    auto input = neutralDisposition;
    input.mFactionReaction = 10;
    input.mFactionRank = 2;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 80);
    settings.mFactionMultiplier = 2;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 150);
    input.mFactionRank = -1;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 30);
}

TEST(ESM4CombatAIRules, CreatureDispositionSkipsReputationButRetainsOtherTerms)
{
    const auto settings = ESM4::buildDispositionSettings({});
    auto input = neutralDisposition;
    input.mActorCreature = true;
    input.mTargetFame = 1000;
    input.mTargetInfamy = 1000;
    input.mTargetBounty = 10000;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 50);
    input.mRaceReaction = 10;
    input.mWeaponPenalty = true;
    input.mDiseasePenalty = true;
    input.mModifier = 7;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 47);
}

TEST(ESM4CombatAIRules, DispositionCapsRetainSignedNativeBranchOrder)
{
    auto settings = ESM4::buildDispositionSettings({});
    settings.mFameMaximum = -3;
    settings.mBountyMaximum = -3;
    EXPECT_EQ(ESM4::dispositionScore(neutralDisposition, settings), 44);
    auto input = neutralDisposition;
    input.mTargetFame = -100;
    input.mActorResponsibility = 100;
    input.mTargetBounty = 10000;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), 56);
    settings = ESM4::buildDispositionSettings({});
    settings.mInfamyMaximum = -3;
    EXPECT_EQ(ESM4::dispositionScore(neutralDisposition, settings), 47);
}

TEST(ESM4CombatAIRules, DispositionSettingsUseTypedWinningOverridesAndIgnoreUnreadSettings)
{
    ESM4::GameSetting actor{};
    actor.mEditorId = "fDispActorPerBase";
    actor.mData = 20.f;
    ESM4::GameSetting infamy{};
    infamy.mEditorId = "fDispActorInfamyBase";
    infamy.mData = 10000.f;
    ESM4::GameSetting rank{};
    rank.mEditorId = "fDispTargetFactionRankMult";
    rank.mData = 10000.f;
    const ESM4::GameSetting* values[]{&actor, &infamy, &rank};
    const auto settings = ESM4::buildDispositionSettings(values);
    EXPECT_EQ(ESM4::dispositionScore(neutralDisposition, settings), 60);
    actor.mData = std::int32_t{20};
    EXPECT_THROW(ESM4::buildDispositionSettings(values), std::invalid_argument);
    actor.mEditorId = "iDispFameMax";
    actor.mData = 20.f;
    EXPECT_THROW(ESM4::buildDispositionSettings(values), std::invalid_argument);
}

TEST(ESM4CombatAIRules, DispositionInvalidSettingsAndUnsupportedConversionsFailExplicitly)
{
    auto settings = ESM4::buildDispositionSettings({});
    settings.mActorPersonalityBase = std::numeric_limits<float>::infinity();
    EXPECT_THROW(ESM4::dispositionScore(neutralDisposition, settings), std::invalid_argument);
    settings = ESM4::buildDispositionSettings({});
    settings.mActorPersonalityMultiplier = std::numeric_limits<float>::max();
    EXPECT_THROW(ESM4::dispositionScore(neutralDisposition, settings), std::overflow_error);
    settings = ESM4::buildDispositionSettings({});
    auto input = neutralDisposition;
    input.mTargetPersonality = 0;
    input.mActorPersonality = 0;
    input.mModifier = std::numeric_limits<std::int32_t>::max();
    EXPECT_EQ(ESM4::dispositionScore(input, settings), input.mModifier);
    input.mActorPersonality = 50;
    EXPECT_EQ(ESM4::dispositionScore(input, settings), -2147483624);
    EXPECT_EQ(ESM4::dispositionScore(input, settings, ESM4::ActorValueConversionMode::Sse),
        std::numeric_limits<std::int32_t>::min());
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

TEST(ESM4CombatAIRules, YieldAcceptanceUsesReceiverTypeAndNativeExclusions)
{
    ESM4::YieldAcceptanceInput input{true, false, false, true, false, false, 0, false};
    EXPECT_TRUE(ESM4::acceptsYield(input));
    input.mFightScore = -1;
    EXPECT_TRUE(ESM4::acceptsYield(input));
    input.mFightScore = 1;
    EXPECT_FALSE(ESM4::acceptsYield(input));
    input.mFightScore = 0;
    input.mHasHostileEffectFromTarget = true;
    EXPECT_FALSE(ESM4::acceptsYield(input));
    input.mHasHostileEffectFromTarget = false;
    input.mRejectYields = true;
    EXPECT_FALSE(ESM4::acceptsYield(input));
    input.mRejectYields = false;
    input.mTargetEscapedJail = true;
    EXPECT_FALSE(ESM4::acceptsYield(input));
    input.mTargetPlayer = false;
    EXPECT_TRUE(ESM4::acceptsYield(input));
    input.mReceiverNpc = false;
    EXPECT_FALSE(ESM4::acceptsYield(input)); // Creature virtual always returns false.
    input.mReceiverPlayer = true;
    EXPECT_TRUE(ESM4::acceptsYield(input)); // Player ignores NPC acceptance policy.
    input.mParalyzed = true;
    EXPECT_FALSE(ESM4::acceptsYield(input));
}

TEST(ESM4CombatAIRules, YieldScoreAndDurationPreserveNativeArithmetic)
{
    auto settings = ESM4::buildYieldSettings({});
    EXPECT_FLOAT_EQ(ESM4::yieldScore(10, 50, 75, settings), 35);
    settings.mBase = -20; // Winning Oblivion.esm override.
    EXPECT_FLOAT_EQ(ESM4::yieldScore(10, 50, 75, settings), 15);
    EXPECT_FLOAT_EQ(ESM4::yieldScore(0, 75, 50, settings), -45);
    const auto limit = std::numeric_limits<std::int32_t>::max();
    EXPECT_FLOAT_EQ(ESM4::yieldScore(0, -1, limit, settings), -2147483648.f);
    EXPECT_FLOAT_EQ(ESM4::yieldDuration(0, settings), 1);
    EXPECT_FLOAT_EQ(ESM4::yieldDuration(9, settings), 3.7f);
    EXPECT_FLOAT_EQ(ESM4::yieldDuration(10, settings), 1);
    EXPECT_FLOAT_EQ(ESM4::yieldDuration(99, settings), 3.7f);
    EXPECT_FLOAT_EQ(ESM4::yieldDuration(100, settings), 1);
    EXPECT_FLOAT_EQ(ESM4::yieldDuration(32767, settings), 3.1f);
    EXPECT_THROW(ESM4::yieldDuration(32768, settings), std::invalid_argument);
    EXPECT_THROW(ESM4::yieldScore(std::numeric_limits<float>::quiet_NaN(), 50, 50, settings), std::invalid_argument);
    settings.mDurationMultiplier = std::numeric_limits<float>::infinity();
    EXPECT_THROW(ESM4::yieldDuration(0, settings), std::invalid_argument);
}

TEST(ESM4CombatAIRules, YieldHitInterruptUsesSignedByteAndStrictThreshold)
{
    const auto settings = ESM4::buildYieldSettings({});
    for (int state : {5, 6, 7})
        for (int hits : {-128, -1, 0, 1, 2, 3, 127})
            EXPECT_EQ(ESM4::yieldInterruptedByHits(state, static_cast<std::int8_t>(hits), settings),
                state == 6 && hits > 2);
}

TEST(ESM4CombatAIRules, YieldDefaultsAndTypedOverrides)
{
    const auto defaults = ESM4::buildYieldSettings({});
    EXPECT_EQ(defaults.mBase, 0);
    EXPECT_EQ(defaults.mMultiplier, 1);
    EXPECT_EQ(defaults.mDurationBase, 1);
    EXPECT_EQ(defaults.mDurationMultiplier, 3);
    EXPECT_EQ(defaults.mMaxHitCount, 2);
    ESM4::GameSetting base{};
    base.mEditorId = "fAIYieldBase";
    base.mData = -20.f;
    const ESM4::GameSetting* values[]{&base};
    EXPECT_EQ(ESM4::buildYieldSettings(values).mBase, -20);
    base.mData = std::int32_t{-20};
    EXPECT_THROW(ESM4::buildYieldSettings(values), std::invalid_argument);
}

TEST(ESM4CombatAIRules, YieldDurationFixedSeedDistribution)
{
    const auto settings = ESM4::buildYieldSettings({});
    std::mt19937 random(0x4d1552);
    std::uniform_int_distribution<unsigned> draw(0, 32767);
    std::array<unsigned, 10> counts{};
    // Declared before sampling: 100000 uniform 15-bit draws. Residues 0..7
    // have 3277/32768 probability, 8/9 have 3276/32768; tolerance 600 per bin.
    for (unsigned i = 0; i < 100000; ++i)
    {
        const auto sample = draw(random);
        EXPECT_EQ(ESM4::yieldDuration(sample, settings), ESM4::yieldDuration(sample % 10, settings));
        ++counts[sample % 10];
    }
    for (std::size_t i = 0; i < counts.size(); ++i)
        EXPECT_NEAR(counts[i], (i < 8 ? 3277. : 3276.) / 32768 * 100000, 600);
}

TEST(ESM4CombatAIRules, DispositionModifierPreservesMissingAndZeroEntryDifference)
{
    using Mode = ESM4::ActorValueConversionMode;
    EXPECT_EQ(ESM4::modifyDispositionModifier(std::nullopt, 50, 200, Mode::Sse), 200);
    EXPECT_EQ(ESM4::modifyDispositionModifier(0, 50, 200, Mode::Sse), 50);
    EXPECT_EQ(ESM4::modifyDispositionModifier(0, 50, -200, Mode::Sse), -50);
    EXPECT_EQ(ESM4::modifyDispositionModifier(200, 100, -7, Mode::Sse), 193);
    EXPECT_EQ(ESM4::modifyDispositionModifier(-200, 0, 7, Mode::Sse), -193);
    EXPECT_EQ(ESM4::modifyDispositionModifier(std::nullopt, 50, 0, Mode::Sse), 0);
}

TEST(ESM4CombatAIRules, DispositionModifierRetainsCommandFloatStoreAndCpuConversions)
{
    using Mode = ESM4::ActorValueConversionMode;
    const auto maximum = std::numeric_limits<std::int32_t>::max();
    EXPECT_EQ(ESM4::modifyDispositionModifier(std::nullopt, 50, maximum, Mode::Sse),
        std::numeric_limits<std::int32_t>::min());
    EXPECT_EQ(ESM4::modifyDispositionModifier(0, 50, maximum, Mode::Sse), 128);
    EXPECT_EQ(ESM4::modifyDispositionModifier(0, 50, maximum, Mode::NonSse), 0);
    EXPECT_EQ(ESM4::modifyDispositionModifier(16777217, 50, 1, Mode::Sse), 16777218);
    EXPECT_THROW(ESM4::modifyDispositionModifier(0, -1, 0, Mode::Sse), std::invalid_argument);
    EXPECT_THROW(ESM4::modifyDispositionModifier(0, 101, 0, Mode::Sse), std::invalid_argument);
    EXPECT_THROW(ESM4::modifyDispositionModifier(0, 50, 0, static_cast<Mode>(9)), std::invalid_argument);
}
