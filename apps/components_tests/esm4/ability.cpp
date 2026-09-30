#include <components/esm4/ability.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadspel.hpp>

#include <gtest/gtest.h>
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>

TEST(ESM4AbilityInputs, ReconcilesAuthoredFieldsAgainstIndependentOriginalLoaderCases)
{
    struct Case {std::uint32_t previous, authored, flags, data;};
    // Captured original 41617B..416229 cases, not the production expression.
    const std::array<Case, 9> cases{{
        {0x00000000, 0xffffffff, 0x0fc03c00, 123},
        {0x00000002, 0xffffffff, 0x0fc03c02, 123},
        {0x00000004, 0xffffffff, 0x0fc03c04, 123},
        {0x00000080, 0xffffffff, 0x0fc03c80, 123},
        {0x00000100, 0xffffffff, 0x0fc03d00, 123},
        {0x00100072, 0xffffffff, 0x0fd03c72, 123},
        {0x01000072, 0xffffffff, 0x0fc03c72, 55},
        {0xffffffff, 0x00000000, 0xf11fc3ff, 55},
        {0xffffffff, 0x00200000, 0xf11fc3ff, 55},
    }};
    for (const auto& row : cases)
    {
        ESM4::EffectSettingData authored;
        authored.mFlags=row.authored;authored.mAssociatedData=123;
        const auto actual=ESM4::mergeLoadedEffectSetting({row.previous,55},authored);
        EXPECT_EQ(actual.mFlags,row.flags);EXPECT_EQ(actual.mData,row.data);
        EXPECT_EQ(authored.mFlags,row.authored);EXPECT_EQ(authored.mAssociatedData,123);
    }
}

TEST(ESM4AbilityInputs, ConstructorUsesStaticOrItemActorValueAndOriginalSignedQuantityConversions)
{
    ESM4::SpellEffect effect;
    effect.mActorValue=9;effect.mMagnitude=0x80000000;effect.mDuration=0xffffffff;
    auto actual=ESM4::resolveValueModifierEffectInputs(effect,{0,40});
    EXPECT_EQ(actual.mActorValue,9);EXPECT_EQ(actual.mMagnitude,-2147483648.f);EXPECT_EQ(actual.mDuration,-1.f);
    actual=ESM4::resolveValueModifierEffectInputs(effect,{0x1000000,40});
    EXPECT_EQ(actual.mActorValue,40);EXPECT_EQ(actual.mMagnitude,-2147483648.f);
    effect.mMagnitude=16777217;effect.mDuration=60;
    actual=ESM4::resolveValueModifierEffectInputs(effect,{0,40});
    EXPECT_EQ(actual.mMagnitude,16777216.f);EXPECT_EQ(actual.mDuration,60.f);
    EXPECT_EQ(effect.mMagnitude,16777217);EXPECT_EQ(effect.mDuration,60);
}

TEST(ESM4AbilityInputs, ConstructorFlagsOverrideMagnitudeAndDurationWithoutApplyingDetrimentalSign)
{
    ESM4::SpellEffect effect;effect.mActorValue=61;effect.mMagnitude=25;effect.mDuration=60;
    auto actual=ESM4::resolveValueModifierEffectInputs(effect,{0x104,40});
    EXPECT_EQ(actual.mActorValue,61);EXPECT_EQ(actual.mMagnitude,1.f);EXPECT_EQ(actual.mDuration,60.f);
    actual=ESM4::resolveValueModifierEffectInputs(effect,{0x84,40});
    EXPECT_EQ(actual.mMagnitude,25.f);EXPECT_EQ(actual.mDuration,0.f);
    actual=ESM4::resolveValueModifierEffectInputs(effect,{0x1000184,57});
    EXPECT_EQ(actual.mActorValue,57);EXPECT_EQ(actual.mMagnitude,1.f);EXPECT_EQ(actual.mDuration,0.f);
    effect.mMagnitude=0;effect.mDuration=0;
    actual=ESM4::resolveValueModifierEffectInputs(effect,{4,40});
    EXPECT_EQ(std::bit_cast<std::uint32_t>(actual.mMagnitude),0);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(actual.mDuration),0);
}

TEST(ESM4AbilityInputs, CompiledPassiveDefinitionsRetainOriginalFlagsAndStaticActorValues)
{
    struct Expected {const char code[5]; std::uint32_t flags, av;};
    const std::array<Expected, 14> original{{
        {"WABR",0x1000172,55}, {"WKFI",0x100007f,61}, {"WKFR",0x100007f,62},
        {"WKSH",0x100007f,68}, {"WKMA",0x100007f,64}, {"FOSP",0x1000072,9},
        {"SABS",0x1000072,52}, {"STMA",0x1000112,57}, {"FOAT",0x100072,0},
        {"RSFI",0x100007a,61}, {"RSPO",0x100007a,67}, {"RSDI",0x100007a,63},
        {"RSMA",0x100007a,64}, {"RSFR",0x100007a,62}
    }};
    for (const auto& row : original)
    {
        const auto definition = ESM4::compiledPassiveValueModifierDefinition(ESM::fourCC(row.code));
        ASSERT_TRUE(definition) << row.code;
        EXPECT_EQ(definition->mFlags,row.flags);
        EXPECT_EQ(definition->mData,row.av);
    }
    EXPECT_FALSE(ESM4::compiledPassiveValueModifierDefinition(ESM::fourCC("SEFF")));
    EXPECT_FALSE(ESM4::compiledPassiveValueModifierDefinition(ESM::fourCC("SHLD")));
    EXPECT_FALSE(ESM4::compiledPassiveValueModifierDefinition(ESM::fourCC("ZZZZ")));
}

TEST(ESM4AbilityInputs, NativeClampBranchesQueryOnlyRequiredCurrentAndPreserveSignedZero)
{
    const auto foat = ESM::fourCC("FOAT");
    EXPECT_TRUE(ESM4::valueModifierRequiresCurrent(0,foat,-50));
    EXPECT_EQ(ESM4::clampValueModifierDelta(0,foat,-50,10),-10);
    EXPECT_EQ(ESM4::clampValueModifierDelta(0,foat,0,-5),5);
    EXPECT_EQ(ESM4::clampValueModifierDelta(32,foat,-50,10),-10);
    for (const auto av : {10,33,64,71})
    {
        EXPECT_FALSE(ESM4::valueModifierRequiresCurrent(av,foat,-50));
        EXPECT_EQ(ESM4::clampValueModifierDelta(av,foat,-50,{}),-50);
    }
    EXPECT_EQ(ESM4::clampValueModifierDelta(8,ESM::fourCC("ABHE"),-50,{}),-50);
    EXPECT_FALSE(ESM4::valueModifierRequiresCurrent(0,foat,1));
    EXPECT_EQ(ESM4::clampValueModifierDelta(0,foat,1,{}),1);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(ESM4::clampValueModifierDelta(0,foat,-0.f,1)),0x80000000u);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(ESM4::clampValueModifierDelta(0,foat,0.f,-0x1p-149f)),1u);
}

TEST(ESM4AbilityInputs, RecoverableRemovalKeepsInitialDamageWritePresenceSeparateFromBaseInverse)
{
    const auto foat = ESM::fourCC("FOAT");
    EXPECT_EQ(ESM4::initialValueModifierRemovalDamage(0,foat,50,10),40.f);
    const auto zero = ESM4::initialValueModifierRemovalDamage(0,foat,50,100);
    ASSERT_TRUE(zero);
    EXPECT_EQ(*zero,0);
    EXPECT_FALSE(ESM4::initialValueModifierRemovalDamage(0,foat,0,{}));
    EXPECT_FALSE(ESM4::initialValueModifierRemovalDamage(0,foat,-50,{}));
    const auto extra = ESM4::initialValueModifierRemovalDamage(64,ESM::fourCC("RSMA"),50,{});
    ASSERT_TRUE(extra);
    EXPECT_EQ(*extra,0);
    // The eventual base inverse remains-50 even where initial Damage is40.
    // This helper intentionally does not replace it with the clamped-10.
}

TEST(ESM4AbilityInputs, ValueModifierPreparationsRejectInvalidQueriedInputsAndArithmeticOverflow)
{
    const auto foat = ESM::fourCC("FOAT");
    EXPECT_THROW(ESM4::clampValueModifierDelta(72,foat,1,{}),std::invalid_argument);
    EXPECT_THROW(ESM4::clampValueModifierDelta(0,foat,-1,{}),std::invalid_argument);
    EXPECT_THROW(ESM4::clampValueModifierDelta(0,foat,-1,std::numeric_limits<float>::quiet_NaN()),std::invalid_argument);
    EXPECT_THROW(ESM4::valueModifierRequiresCurrent(0,foat,std::numeric_limits<float>::infinity()),std::invalid_argument);
    EXPECT_THROW(ESM4::initialValueModifierRemovalDamage(0,foat,std::numeric_limits<float>::quiet_NaN(),{}),std::invalid_argument);
    EXPECT_THROW(ESM4::clampValueModifierDelta(0,foat,-std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max()),std::invalid_argument);
    EXPECT_EQ(ESM4::clampValueModifierDelta(0,foat,1,std::numeric_limits<float>::quiet_NaN()),1);
}

TEST(ESM4AbilityInputs, PassiveComparisonKeysMatchOriginalAsciiFormattingAndFieldOrder)
{
    const auto key = [](unsigned flags, unsigned school, std::string_view name, float mag, float duration) {
        return ESM4::passiveEffectComparisonKey(flags, school, name, mag, duration);
    };
    const std::string letters = "ceadfbz";
    for (unsigned i = 0; i < letters.size(); ++i)
        EXPECT_EQ(key(0, i, "Name", 10, 0), std::string(1, letters[i]) + "name10.00.0");
    EXPECT_EQ(key(0, 2, "FoRtIfY Attribute", .25f, 0), "afortify attribute0.30.0");
    EXPECT_EQ(key(0, 2, "Name", -.25f, 0), "aname-0.30.0");
    EXPECT_EQ(key(0, 2, "Name", -.0f, -.0f), "aname-0.0-0.0");
    EXPECT_EQ(key(0, 2, "Name", .35f, 0), "aname0.30.0");
    EXPECT_EQ(key(0, 2, "Name", 100, 10), "aname100.010.0");
    EXPECT_LT(key(0, 2, "Name", 10, 100), key(0, 2, "Name", 100, 0));
    EXPECT_EQ(key(0x80, 2, "Name", 10, 0), "aname10.01000.0");
    EXPECT_EQ(key(0x100, 2, "Name", 10, 0), "aname1000.00.0");
    EXPECT_EQ(key(0x180, 7, "Name", std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity()), "zname1000.01000.0");
    EXPECT_EQ(key(0, 2, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdLEFT", 10, 0),
        key(0, 2, "abcdefghijklmnopqrstuvwxyzabcdRIGHT", 10, 0));
    EXPECT_EQ(key(0, 2, std::string_view("Name\0ignored", 12), 10, 0), "aname10.00.0");
    EXPECT_EQ(key(0, 2, "Name", 4294967296.f, 0), "aname4294967296.00.0");
    EXPECT_EQ(key(0, 2, "Name", 1.e20f, 0), "aname100000002004087730000.00.0");
    EXPECT_EQ(key(0, 2, "Name", std::numeric_limits<float>::max(), 0),
        "aname340282346638528860000000000000000000000.00.0");
}

TEST(ESM4AbilityInputs, PassiveComparisonKeysRejectUnreadableAsciiOrNonfiniteQueriedQuantities)
{
    for (float value : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        EXPECT_THROW(ESM4::passiveEffectComparisonKey(0, 2, "Name", value, 0), std::invalid_argument);
        EXPECT_THROW(ESM4::passiveEffectComparisonKey(0, 2, "Name", 0, value), std::invalid_argument);
    }
    EXPECT_THROW(ESM4::passiveEffectComparisonKey(0, 2, "\xc3\xa9", 1, 0), std::invalid_argument);
    const std::string prefix(30, 'A');
    EXPECT_EQ(ESM4::passiveEffectComparisonKey(0, 2, prefix + "\xff", 1, 0),
        "a" + std::string(30, 'a') + "1.00.0");
}
