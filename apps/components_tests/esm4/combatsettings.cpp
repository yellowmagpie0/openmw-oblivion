#include <components/esm4/combatsettings.hpp>
#include <components/esm4/combatstylepolicy.hpp>
#include <components/esm4/loadgmst.hpp>
#include <gtest/gtest.h>
#include <array>
#include <limits>
#include <stdexcept>

TEST(ESM4CombatSettings, NativeInitializersAreOverriddenByTypedWinningContent)
{
    const auto compiled = ESM4::buildCombatStyleDefaults({});
    EXPECT_EQ(compiled.mStandard.mDodgeChance, 75);
    EXPECT_EQ(compiled.mStandard.mAttackRecoilBonus, 5);
    EXPECT_EQ(compiled.mStandard.mIdle.mMaximum, 1.5);
    EXPECT_EQ(compiled.mAdvanced.mAttackSkillBase, 0);
    EXPECT_EQ(compiled.mAdvanced.mAttackSkillMultiplier, 20);
    EXPECT_EQ(compiled.mAdvanced.mDodgeFatigueMultiplier, -20);
    ESM4::GameSetting bonus{};
    bonus.mEditorId = "fAIDefaultAttackDuringRecoilStaggerBonus";
    bonus.mData = 30.f; // Independently observed in original 1.2.0416 with Oblivion.esm.
    std::array<const ESM4::GameSetting*, 1> settings{&bonus};
    EXPECT_EQ(ESM4::buildCombatStyleDefaults(settings).mStandard.mAttackRecoilBonus, 30);
    EXPECT_EQ(compiled.mStandard.mAttackRecoilBonus, 5); // Immutable earlier result.
}

TEST(ESM4CombatSettings, CaseInsensitiveFlagsAndRangesRetainNativeDefaultSemantics)
{
    ESM4::GameSetting range{}, flag{};
    range.mEditorId = "FAIDEFAULTOPTIMALRANGEMULT";
    range.mData = 2.f;
    flag.mEditorId = "iAIDefaultDoNotAcquire";
    flag.mData = std::int32_t{-1}; // Original flag getters test nonzero.
    std::array<const ESM4::GameSetting*, 2> settings{&range, &flag};
    const auto native = ESM4::buildCombatStyleDefaults(settings);
    EXPECT_EQ((*native.mStandard.mRangeMultipliers)[0], 2);
    EXPECT_EQ(native.mStandard.mDoNotAcquire, true);
    ESM4::CombatStyleStandard historical;
    const auto authored = ESM4::resolveCombatStyleStandard(&historical, native.mStandard);
    EXPECT_EQ((*authored.mRangeMultipliers)[0], 1);
    EXPECT_EQ(authored.mDoNotAcquire, false);
    flag.mEditorId = "iAIDefaultYieldEnabled";
    EXPECT_TRUE(ESM4::buildCombatStyleDefaults(settings).mStandard.has(ESM4::CombatStyleFlag::WillYield));
}

TEST(ESM4CombatSettings, InvalidOverridesCannotHideBehindCompiledFallbacks)
{
    ESM4::GameSetting value{};
    value.mEditorId = "iAIDefaultDodgeChance";
    std::array<const ESM4::GameSetting*, 1> settings{&value};
    for (const auto chance : {-1, 0, 1, 100, 101})
    {
        value.mData = std::int32_t{chance};
        if (chance >= 0 && chance <= 100)
            EXPECT_EQ(ESM4::buildCombatStyleDefaults(settings).mStandard.mDodgeChance, chance);
        else
            EXPECT_THROW(ESM4::buildCombatStyleDefaults(settings), std::invalid_argument);
    }
    value.mData = 50.f;
    EXPECT_THROW(ESM4::buildCombatStyleDefaults(settings), std::invalid_argument);
    value.mEditorId = "fAIDefaultIdleMaxTime";
    for (const auto invalid : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        value.mData = invalid;
        EXPECT_THROW(ESM4::buildCombatStyleDefaults(settings), std::invalid_argument);
    }
    value.mData = std::int32_t{2};
    EXPECT_THROW(ESM4::buildCombatStyleDefaults(settings), std::invalid_argument);
    value.mEditorId = "fAIDefaultDodgeFatigueMult";
    value.mData = -30.f;
    EXPECT_EQ(ESM4::buildCombatStyleDefaults(settings).mAdvanced.mDodgeFatigueMultiplier, -30);
    value.mData = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(ESM4::buildCombatStyleDefaults(settings), std::invalid_argument);
}

TEST(ESM4CombatSettings, AmbiguousOrMissingRecordsFailAndUnrelatedSettingsStaySeparate)
{
    ESM4::GameSetting first{}, second{};
    first.mEditorId = "fAIDefaultIdleMaxTime";
    first.mData = 2.f;
    second.mEditorId = "faidefaultidlemaxtime";
    second.mData = 3.f;
    std::array<const ESM4::GameSetting*, 2> settings{&first, &second};
    EXPECT_THROW(ESM4::buildCombatStyleDefaults(settings), std::invalid_argument);
    settings[1] = nullptr;
    EXPECT_THROW(ESM4::buildCombatStyleDefaults(settings), std::invalid_argument);
    second.mEditorId.clear();
    settings[1] = &second;
    EXPECT_THROW(ESM4::buildCombatStyleDefaults(settings), std::invalid_argument);
    second.mEditorId = "sUnrelatedDisplayText";
    second.mData = std::string("native text");
    EXPECT_EQ(ESM4::buildCombatStyleDefaults(settings).mStandard.mIdle.mMaximum, 2);
}

TEST(ESM4CombatSettings, ReachDefaultsAndTypedOverrides)
{
    const auto defaults = ESM4::buildMeleeReachSettings({});
    EXPECT_EQ(defaults.mCombatDistance, 128);
    EXPECT_EQ(defaults.mHandMultiplier, .5f);
    EXPECT_EQ(defaults.mGiantMultiplier, 2);
    ESM4::GameSetting record{};
    const std::array<const ESM4::GameSetting*, 1> settings{&record};
    for (const auto* name : {"fCombatDistance", "fHandReachMult", "fCombatGiantCreatureReachMult"})
    {
        record.mEditorId = name;
        for (float valid : {0.f, 1.f, .6f, 2.2f})
        {
            record.mData = valid;
            const auto result = ESM4::buildMeleeReachSettings(settings);
            EXPECT_EQ(std::string_view(name) == "fCombatDistance" ? result.mCombatDistance
                : std::string_view(name) == "fHandReachMult" ? result.mHandMultiplier : result.mGiantMultiplier, valid);
        }
        record.mData = std::int32_t{1};
        EXPECT_THROW(ESM4::buildMeleeReachSettings(settings), std::invalid_argument);
        for (float invalid : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            record.mData = invalid;
            EXPECT_THROW(ESM4::buildMeleeReachSettings(settings), std::invalid_argument);
        }
    }
}

TEST(ESM4CombatSettings, HitConeAngleUsesNativeDefaultAndTypedOverride)
{
    EXPECT_EQ(ESM4::buildCombatHitConeAngle({}), 20);
    ESM4::GameSetting value{};
    value.mEditorId = "fCombatHitConeAngle";
    const std::array<const ESM4::GameSetting*, 1> settings{&value};
    for (float degrees : {0.f, 1.f, 35.f, 180.f, 360.f})
    {
        value.mData = degrees;
        EXPECT_EQ(ESM4::buildCombatHitConeAngle(settings), degrees);
    }
    value.mData = std::int32_t{35};
    EXPECT_THROW(ESM4::buildCombatHitConeAngle(settings), std::invalid_argument);
    for (float invalid : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        value.mData = invalid;
        EXPECT_THROW(ESM4::buildCombatHitConeAngle(settings), std::invalid_argument);
    }
}

TEST(ESM4CombatSettings, TrespassTimerUsesNativeDefaultAndTypedWinningOverride)
{
    EXPECT_EQ(ESM4::buildTrespassWarningSettings({}).mTimerLimit, 10);
    ESM4::GameSetting setting;
    setting.mEditorId = "fAITrespassWarningTimer";
    setting.mData = 30.f; // Installed Oblivion.esm:005682.
    const std::array<const ESM4::GameSetting*, 1> settings{&setting};
    EXPECT_EQ(ESM4::buildTrespassWarningSettings(settings).mTimerLimit, 30);
    setting.mData = 0.f;
    EXPECT_EQ(ESM4::buildTrespassWarningSettings(settings).mTimerLimit, 0);
    setting.mData = std::int32_t{30};
    EXPECT_THROW(ESM4::buildTrespassWarningSettings(settings), std::invalid_argument);
    for (float invalid : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        setting.mData = invalid;
        EXPECT_THROW(ESM4::buildTrespassWarningSettings(settings), std::invalid_argument);
    }
}

TEST(ESM4CombatSettings, AlarmRecipientDistanceUsesNativeIntegerDefaultAndOverride)
{
    EXPECT_EQ(ESM4::buildCrimeAlarmSettings({}).mRecipientDistance, 10000);
    ESM4::GameSetting setting;
    setting.mEditorId = "iCrimeAlarmRecDistance";
    setting.mData = std::int32_t{4000}; // Installed Oblivion.esm:02be94.
    const std::array<const ESM4::GameSetting*, 1> settings{&setting};
    EXPECT_EQ(ESM4::buildCrimeAlarmSettings(settings).mRecipientDistance, 4000);
    setting.mData = std::int32_t{0};
    EXPECT_EQ(ESM4::buildCrimeAlarmSettings(settings).mRecipientDistance, 0);
    setting.mData = 4000.f;
    EXPECT_THROW(ESM4::buildCrimeAlarmSettings(settings), std::invalid_argument);
    setting.mData = std::int32_t{-1};
    EXPECT_THROW(ESM4::buildCrimeAlarmSettings(settings), std::invalid_argument);
}

TEST(ESM4CombatSettings, FightScoreUsesTypedNativeDefaultsAndWinningOverrides)
{
    const auto compiled = ESM4::buildFightScoreSettings({});
    EXPECT_EQ(compiled.mDispositionBase, 50);
    EXPECT_EQ(compiled.mDispositionMultiplier, -1);
    EXPECT_EQ(compiled.mAggressionBase, -80);
    EXPECT_EQ(compiled.mAggressionMultiplier, 1);
    EXPECT_EQ(compiled.mDistanceBase, 1);
    EXPECT_EQ(compiled.mDistanceMultiplier, -.005f);
    EXPECT_EQ(compiled.mFriendDispositionBase, -50);
    EXPECT_EQ(compiled.mFriendDispositionMultiplier, 1);
    EXPECT_EQ(compiled.mResponsibilityMultiplier, 1.7f);
    ESM4::GameSetting aggression, friendBase, responsibility;
    aggression.mEditorId = "fFightAggrBase";
    aggression.mData = -55.f;
    friendBase.mEditorId = "fFightFriendDispBase";
    friendBase.mData = -25.f;
    responsibility.mEditorId = "fCrimeAlarmRespMult";
    responsibility.mData = 2.f;
    const std::array<const ESM4::GameSetting*, 3> settings{&aggression, &friendBase, &responsibility};
    const auto installed = ESM4::buildFightScoreSettings(settings);
    EXPECT_EQ(installed.mAggressionBase, -55);
    EXPECT_EQ(installed.mFriendDispositionBase, -25);
    EXPECT_EQ(installed.mResponsibilityMultiplier, 2);
    aggression.mData = std::int32_t{-55};
    EXPECT_THROW(ESM4::buildFightScoreSettings(settings), std::invalid_argument);
    aggression.mData = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(ESM4::buildFightScoreSettings(settings), std::invalid_argument);
}

TEST(ESM4CombatSettings, CrimeInfamyThresholdUsesCompiledDefaultAndTypedOverride)
{
    EXPECT_EQ(ESM4::buildCrimeInfamySettings({}).mBountyThreshold, 2000);
    ESM4::GameSetting setting;
    setting.mEditorId = "fInfamyBountyMod";
    setting.mData = 500.f; // Oblivion.esm:06c64b.
    const std::array<const ESM4::GameSetting*, 1> settings{&setting};
    EXPECT_EQ(ESM4::buildCrimeInfamySettings(settings).mBountyThreshold, 500);
    setting.mData = std::int32_t{500};
    EXPECT_THROW(ESM4::buildCrimeInfamySettings(settings), std::invalid_argument);
    for (float value : {0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        setting.mData = value;
        EXPECT_THROW(ESM4::buildCrimeInfamySettings(settings), std::invalid_argument);
    }
}

TEST(ESM4CombatSettings, NpcDynamicStatsUseTypedCompiledDefaultsAndInstalledOverrides)
{
    const auto compiled = ESM4::buildNpcDynamicStatsSettings({});
    EXPECT_EQ(compiled.mAttributeHealthMultiplier, .5f);
    EXPECT_EQ(compiled.mPerLevelHealthMultiplier, 4);
    EXPECT_EQ(compiled.mLowLevelMaximum, 3);
    EXPECT_EQ(compiled.mLowLevelHealthMultiplier, .25f);
    EXPECT_EQ(compiled.mMagickaMultiplier, .2f);
    ESM4::GameSetting lowLevel, health, magicka;
    lowLevel.mEditorId = "iLowLevelNPCMaxLevel";
    lowLevel.mData = std::int32_t{4};
    health.mEditorId = "fLowLevelNPCBaseHealthMult";
    health.mData = .4f;
    magicka.mEditorId = "fNPCBaseMagickaMult";
    magicka.mData = 1.5f;
    const std::array<const ESM4::GameSetting*, 3> inputs{&lowLevel, &health, &magicka};
    const auto installed = ESM4::buildNpcDynamicStatsSettings(inputs);
    EXPECT_EQ(installed.mLowLevelMaximum, 4);
    EXPECT_EQ(installed.mLowLevelHealthMultiplier, .4f);
    EXPECT_EQ(installed.mMagickaMultiplier, 1.5f);
    lowLevel.mData = 4.f;
    EXPECT_THROW(ESM4::buildNpcDynamicStatsSettings(inputs), std::invalid_argument);
    lowLevel.mData = std::int32_t{4};
    health.mData = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(ESM4::buildNpcDynamicStatsSettings(inputs), std::invalid_argument);
}
