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
