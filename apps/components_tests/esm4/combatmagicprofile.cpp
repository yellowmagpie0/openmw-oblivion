#include <components/esm4/combatmagicprofile.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadspel.hpp>

#include <gtest/gtest.h>

#include <limits>

namespace
{
    ESM4::EffectSetting paralysis(std::uint32_t flags = 0x01000d73)
    {
        ESM4::EffectSetting result;
        result.mEffectCode = ESM::fourCC("PARA");
        result.mData = ESM4::EffectSettingData{flags, 475.f, 48, 3, 66, 0, 0, {}};
        return result;
    }

    ESM4::Spell defaultSpell()
    {
        ESM4::Spell result;
        result.mData = ESM4::SpellData{0, 0, 0, 1, {}};
        result.mEffects.push_back({ESM::fourCC("PARA"), 0, 0, 10, 0, 48, {}});
        return result;
    }
}

TEST(ESM4CombatMagicProfile, AcceptsIndependentlyVerifiedCompiledAndWinningParalysisProfiles)
{
    for (const auto flags : {0x01000173u, 0x01000d73u})
    {
        const auto effect = paralysis(flags);
        EXPECT_TRUE(ESM4::supportsDefaultCombatParalysisProfile(nullptr, &effect));
        const auto spell = defaultSpell();
        EXPECT_TRUE(ESM4::supportsDefaultCombatParalysisProfile(&spell, &effect));
    }
    EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(nullptr, nullptr));
}

TEST(ESM4CombatMagicProfile, UsesReadyStaticFlagsAndIgnoresNativeSuppressedQuantityFields)
{
    auto effect = paralysis();
    effect.mData->mFlags = 0; // Noneditable Recover/NoMagnitude/static AV bits survive original loading.
    effect.mData->mAssociatedData = std::numeric_limits<std::uint32_t>::max();
    auto spell = defaultSpell();
    for (auto quantity : {0u, 48u, std::numeric_limits<std::uint32_t>::max()})
    {
        spell.mEffects[0].mMagnitude = quantity;
        spell.mEffects[0].mActorValue = quantity;
        EXPECT_TRUE(ESM4::supportsDefaultCombatParalysisProfile(&spell, &effect));
    }
}

TEST(ESM4CombatMagicProfile, RejectsAuthoredSpellShapesOutsideTheOwnedDefaultContract)
{
    const auto effect = paralysis();
    const auto original = defaultSpell();
    auto spell = original;
    spell.mData.reset();
    EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(&spell, &effect));
    for (const auto member : {&ESM4::SpellData::mType, &ESM4::SpellData::mCost, &ESM4::SpellData::mLevel})
    {
        spell = original;
        spell.mData.value().*member = 1;
        EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(&spell, &effect));
    }
    spell = original; spell.mData->mFlags = 0;
    EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(&spell, &effect));
    spell = original; spell.mEffects.clear();
    EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(&spell, &effect));
    spell = original; spell.mEffects.push_back(spell.mEffects[0]);
    EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(&spell, &effect));
    for (const auto member : {&ESM4::SpellEffect::mId, &ESM4::SpellEffect::mArea,
             &ESM4::SpellEffect::mDuration, &ESM4::SpellEffect::mRange})
    {
        spell = original;
        spell.mEffects[0].*member = 1;
        EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(&spell, &effect));
    }
    spell = original; spell.mEffects[0].mScriptEffect.emplace();
    EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(&spell, &effect));
    spell = original; spell.mFlags = ESM4::Rec_Deleted;
    EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(&spell, &effect));
}

TEST(ESM4CombatMagicProfile, RejectsMissingOrUnsupportedEffectDefinitions)
{
    auto effect = paralysis(); effect.mData.reset();
    EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(nullptr, &effect));
    effect = paralysis(); effect.mEffectCode = ESM::fourCC("FOAT");
    EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(nullptr, &effect));
    effect = paralysis(); effect.mFlags = ESM4::Rec_Deleted;
    EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(nullptr, &effect));
    effect = paralysis(); effect.mData->mSchool = 2;
    EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(nullptr, &effect));
    effect = paralysis(); effect.mData->mResistanceActorValue = 64;
    EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(nullptr, &effect));
    effect = paralysis(); effect.mData->mFlags |= 0x1000; // Unverified authored flag behavior stays guarded.
    EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(nullptr, &effect));
    effect = paralysis(); effect.mData->mAssociatedForm = ESM::FormKey::content("test.esm", 1);
    EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(nullptr, &effect));
    for (const auto value : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity()})
    {
        effect = paralysis(); effect.mData->mBaseCost = value;
        EXPECT_FALSE(ESM4::supportsDefaultCombatParalysisProfile(nullptr, &effect));
    }
}
