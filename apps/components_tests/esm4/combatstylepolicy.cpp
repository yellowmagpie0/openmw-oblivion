#include <components/esm4/combatstylepolicy.hpp>
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace
{
    ESM4::CombatStyleStandard defaults()
    {
        ESM4::CombatStyleStandard result;
        result.mDodgeChance = 75;
        result.mAttackRecoilBonus = 5;
        result.mRangeMultipliers = std::array<float, 2>{2, 3};
        result.mSwitchDistances = std::array<float, 2>{250, 1000};
        result.mBuffStandoff = 325;
        result.mRangedGroupStandoff = std::array<float, 2>{500, 325};
        result.mRushChance = 25;
        result.mRushDistanceMultiplier = 1;
        result.mDoNotAcquire = true;
        return result;
    }
}

TEST(ESM4CombatStylePolicy, NullStyleAndHistoricalRecordHaveDifferentTailDefaults)
{
    const auto settings = defaults();
    const auto missing = ESM4::resolveCombatStyleStandard(nullptr, settings);
    EXPECT_EQ((*missing.mRangeMultipliers)[0], 2);
    EXPECT_EQ(missing.mDoNotAcquire, true);
    ESM4::CombatStyleStandard record;
    record.mDodgeChance = 15;
    record.mAttackRecoilBonus = 30;
    const auto historical = ESM4::resolveCombatStyleStandard(&record, settings);
    EXPECT_EQ(historical.mDodgeChance, 15);
    EXPECT_EQ(historical.mAttackRecoilBonus, 30);
    EXPECT_EQ(historical.mRangeMultipliers, (std::array<float, 2>{1, 1}));
    EXPECT_EQ(historical.mDoNotAcquire, false);
    EXPECT_EQ(historical.mSwitchDistances, settings.mSwitchDistances);
    EXPECT_EQ(historical.mBuffStandoff, 325);
    EXPECT_EQ(historical.mRangedGroupStandoff, settings.mRangedGroupStandoff);
    EXPECT_EQ(historical.mRushChance, 25);
    EXPECT_EQ(historical.mRushDistanceMultiplier, 1);
    EXPECT_FALSE(record.mSwitchDistances); // Resolution never edits source data.
}

TEST(ESM4CombatStylePolicy, ZeroCompatibilityDoesNotReplacePositiveValuesOrUnrelatedZeroes)
{
    auto record = defaults();
    record.mRangeMultipliers = std::array<float, 2>{0, 0};
    record.mSwitchDistances = std::array<float, 2>{0, 0};
    record.mBuffStandoff = 0;
    record.mRangedGroupStandoff = std::array<float, 2>{0, 0};
    record.mRushChance = 0;
    record.mRushDistanceMultiplier = 0;
    record.mDoNotAcquire = false;
    auto resolved = ESM4::resolveCombatStyleStandard(&record, defaults());
    EXPECT_EQ(resolved.mSwitchDistances, (std::array<float, 2>{250, 1000}));
    EXPECT_EQ(resolved.mRushChance, 25);
    EXPECT_EQ(resolved.mRushDistanceMultiplier, 1);
    EXPECT_EQ(resolved.mRangeMultipliers, record.mRangeMultipliers);
    EXPECT_EQ(resolved.mBuffStandoff, 0);
    EXPECT_EQ(resolved.mRangedGroupStandoff, record.mRangedGroupStandoff);
    EXPECT_EQ(resolved.mDoNotAcquire, false);
    const float next = std::nextafter(0.f, 1.f);
    record.mSwitchDistances = std::array<float, 2>{next, 50000};
    record.mRushChance = 1;
    record.mRushDistanceMultiplier = next;
    resolved = ESM4::resolveCombatStyleStandard(&record, defaults());
    EXPECT_EQ(resolved.mSwitchDistances, record.mSwitchDistances);
    EXPECT_EQ(resolved.mRushChance, 1);
    EXPECT_EQ(resolved.mRushDistanceMultiplier, next);
}

TEST(ESM4CombatStylePolicy, MissingDefaultsAndInvalidValuesFailRatherThanUsingLegacyRules)
{
    auto incomplete = defaults();
    incomplete.mBuffStandoff.reset();
    EXPECT_THROW(ESM4::resolveCombatStyleStandard(nullptr, incomplete), std::invalid_argument);
    for (const float invalid : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        auto record = defaults();
        record.mSwitchDistances = std::array<float, 2>{invalid, 100};
        EXPECT_THROW(ESM4::resolveCombatStyleStandard(&record, defaults()), std::invalid_argument);
        EXPECT_THROW(ESM4::resolveCombatStyleStandard(nullptr, record), std::invalid_argument);
    }
    auto record = defaults();
    record.mRushChance = 101;
    EXPECT_THROW(ESM4::resolveCombatStyleStandard(&record, defaults()), std::invalid_argument);
    record = defaults();
    record.mIdle = {2, 1};
    EXPECT_THROW(ESM4::resolveCombatStyleStandard(&record, defaults()), std::invalid_argument);
    record = defaults();
    record.mAttackRecoilBonus = -25;
    EXPECT_NO_THROW(ESM4::resolveCombatStyleStandard(&record, defaults()));
}

TEST(ESM4CombatStylePolicy, ResolvesEveryHistoricalPayloadWithoutConflatingAbsenceAndZero)
{
    for (const std::size_t size : {84, 92, 104, 112, 120, 124})
    {
        SCOPED_TRACE(size);
        const auto record = ESM4::decodeCombatStyleStandard(std::vector<std::uint8_t>(size, 0));
        const auto value = ESM4::resolveCombatStyleStandard(&record, defaults());
        EXPECT_EQ((*value.mRangeMultipliers)[0], size >= 92 ? 0 : 1);
        EXPECT_EQ(value.mSwitchDistances, (std::array<float, 2>{250, 1000}));
        EXPECT_EQ(value.mBuffStandoff, size >= 104 ? 0 : 325);
        EXPECT_EQ((*value.mRangedGroupStandoff)[0], size >= 112 ? 0 : 500);
        EXPECT_EQ(value.mRushChance, 25);
        EXPECT_EQ(value.mRushDistanceMultiplier, 1);
        EXPECT_EQ(value.mDoNotAcquire, false);
    }
}

TEST(ESM4CombatStylePolicy, AdvancedFlagSelectsRecordOrLiveDefaults)
{
    ESM4::CombatStyleAdvanced global;
    global.mAttackSkillBase = 0;
    global.mAttackSkillMultiplier = 20;
    global.mDodgeFatigueMultiplier = -20;
    EXPECT_EQ(ESM4::resolveCombatStyleAdvanced(nullptr, global).mAttackSkillMultiplier, 20);
    ESM4::CombatStyle record;
    record.mStandard = defaults();
    record.mAdvanced = global;
    record.mAdvanced->mAttackSkillMultiplier = 40;
    EXPECT_EQ(ESM4::resolveCombatStyleAdvanced(&record, global).mAttackSkillMultiplier, 20);
    record.mStandard->mFlags |= static_cast<std::uint8_t>(ESM4::CombatStyleFlag::Advanced);
    EXPECT_EQ(ESM4::resolveCombatStyleAdvanced(&record, global).mAttackSkillMultiplier, 40);
    record.mAdvanced.reset();
    EXPECT_THROW(ESM4::resolveCombatStyleAdvanced(&record, global), std::invalid_argument);
}

TEST(ESM4CombatStylePolicy, AdvancedModifiersRemainSignedAndRequireFiniteAuthoritativeInputs)
{
    ESM4::CombatStyleAdvanced global;
    global.mPowerAttackFatigueMultiplier = -10;
    EXPECT_EQ(ESM4::resolveCombatStyleAdvanced(nullptr, global).mPowerAttackFatigueMultiplier, -10);
    ESM4::CombatStyle record;
    EXPECT_THROW(ESM4::resolveCombatStyleAdvanced(&record, global), std::invalid_argument);
    record.mStandard = defaults();
    record.mStandard->mFlags = static_cast<std::uint8_t>(ESM4::CombatStyleFlag::Advanced);
    record.mAdvanced = global;
    record.mAdvanced->mPowerAttackFatigueMultiplier = std::numeric_limits<float>::infinity();
    EXPECT_THROW(ESM4::resolveCombatStyleAdvanced(&record, global), std::invalid_argument);
    global.mAttackDuringBlock = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(ESM4::resolveCombatStyleAdvanced(nullptr, global), std::invalid_argument);
}
