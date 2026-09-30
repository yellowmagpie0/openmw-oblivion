#include <components/esm4/ability.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadspel.hpp>

#include <gtest/gtest.h>
#include <array>
#include <bit>

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
