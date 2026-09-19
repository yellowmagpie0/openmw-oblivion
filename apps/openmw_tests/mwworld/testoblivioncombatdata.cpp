#include <gtest/gtest.h>
#include <components/esm/records.hpp>
#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/oblivioncombatdata.hpp"

namespace
{
    class OblivionCombatDataTest : public testing::Test
    {
    protected:
        MWWorld::ESMStore mStore;
        const ESM::FormKey mActorKey = ESM::FormKey::content("actors.esm", 0x800);
        const ESM::FormKey mStyleKey = ESM::FormKey::content("styles.esm", 0x900);

        void npc(ESM::FormId style = {}, bool native = true)
        {
            ESM4::Npc actor{};
            actor.mId = { 0x800, 3 };
            actor.mIsTES4 = native;
            actor.mCombatStyle = style;
            mStore.getWritable<ESM4::Npc>().insertStatic(actor, mActorKey);
        }

        void style(unsigned char chance)
        {
            ESM4::CombatStyle record{};
            record.mId = { 0x900, 7 };
            record.mStandard.emplace();
            record.mStandard->mAttackChance = chance;
            mStore.getWritable<ESM4::CombatStyle>().insertStatic(record, mStyleKey);
        }
    };

    TEST_F(OblivionCombatDataTest, nativeWinningSettingsOverrideCompiledDefaultsWithoutTes3Leakage)
    {
        ESM::GameSetting legacy;
        legacy.mId = ESM::RefId::stringRefId("fAIDefaultAttackDuringRecoilStaggerBonus");
        legacy.mValue = ESM::Variant(999.f);
        mStore.getWritable<ESM::GameSetting>().insertStatic(legacy);
        EXPECT_FLOAT_EQ(MWWorld::buildOblivionCombatDefaults(mStore).mStandard.mAttackRecoilBonus, 5.f);
        ESM4::GameSetting setting{};
        setting.mId = { 0x100, 0 };
        setting.mEditorId = "fAIDefaultAttackDuringRecoilStaggerBonus";
        setting.mData = 20.f;
        mStore.getWritable<ESM4::GameSetting>().insertStatic(setting);
        setting.mData = 30.f;
        mStore.getWritable<ESM4::GameSetting>().insertStatic(setting);
        npc();
        const auto defaults = MWWorld::buildOblivionCombatDefaults(mStore);
        const auto policy = MWWorld::resolveOblivionCombatPolicy(mStore, mActorKey, defaults);
        EXPECT_EQ(policy.mActorBase, mActorKey);
        EXPECT_TRUE(policy.mStyle.isNull());
        EXPECT_FLOAT_EQ(policy.mStandard.mAttackRecoilBonus, 30.f);
        EXPECT_EQ(policy.mStandard.mAttackChance, 40);
        setting.mData = 80.f;
        mStore.getWritable<ESM4::GameSetting>().insert(setting);
        EXPECT_FLOAT_EQ(MWWorld::buildOblivionCombatDefaults(mStore).mStandard.mAttackRecoilBonus, 80.f);
    }

    TEST_F(OblivionCombatDataTest, nativeActorsResolveWinningStylesAndRejectDeletedReferences)
    {
        const auto defaults = MWWorld::buildOblivionCombatDefaults(mStore);
        style(25);
        npc({ 0x900, 7 });
        auto policy = MWWorld::resolveOblivionCombatPolicy(mStore, mActorKey, defaults);
        EXPECT_EQ(policy.mStyle, mStyleKey);
        EXPECT_EQ(policy.mStandard.mAttackChance, 25);
        EXPECT_EQ((*policy.mStandard.mSwitchDistances)[0], 250.f);
        style(75);
        policy = MWWorld::resolveOblivionCombatPolicy(mStore, mActorKey, defaults);
        EXPECT_EQ(policy.mStandard.mAttackChance, 75);
        EXPECT_FALSE(mStore.search<ESM4::CombatStyle>(mStyleKey)->mStandard->mSwitchDistances.has_value());
        ASSERT_TRUE(mStore.getWritable<ESM4::CombatStyle>().eraseStatic(mStyleKey));
        EXPECT_THROW(MWWorld::resolveOblivionCombatPolicy(mStore, mActorKey, defaults), std::runtime_error);
    }

    TEST_F(OblivionCombatDataTest, creatureUsesNativeReachAndStableStyleIdentity)
    {
        const auto defaults = MWWorld::buildOblivionCombatDefaults(mStore);
        ESM4::Creature actor{};
        actor.mId = { 0x800, 3 };
        actor.mAttackReach = 64;
        actor.mCombatStyle = { 0x900, 7 };
        style(60);
        mStore.getWritable<ESM4::Creature>().insertStatic(actor, mActorKey);
        const auto policy = MWWorld::resolveOblivionCombatPolicy(mStore, mActorKey, defaults);
        EXPECT_EQ(policy.mStyle, mStyleKey);
        EXPECT_EQ(policy.mStandard.mAttackChance, 60);
        actor.mAttackReach.reset();
        mStore.getWritable<ESM4::Creature>().insertStatic(actor, mActorKey);
        EXPECT_THROW(MWWorld::resolveOblivionCombatPolicy(mStore, mActorKey, defaults), std::runtime_error);
    }

    TEST_F(OblivionCombatDataTest, unresolvedOrWrongProfileInputsFailExplicitly)
    {
        const auto defaults = MWWorld::buildOblivionCombatDefaults(mStore);
        EXPECT_THROW(MWWorld::resolveOblivionCombatPolicy(mStore, {}, defaults), std::runtime_error);
        EXPECT_THROW(MWWorld::resolveOblivionCombatPolicy(mStore, mActorKey, defaults), std::runtime_error);
        npc({}, false);
        EXPECT_THROW(MWWorld::resolveOblivionCombatPolicy(mStore, mActorKey, defaults), std::runtime_error);
        npc({ 0x800, 3 }); // NPC identity is not a combat style.
        EXPECT_THROW(MWWorld::resolveOblivionCombatPolicy(mStore, mActorKey, defaults), std::runtime_error);
        npc({ 0x900, 7 });
        ESM4::CombatStyle invalid{};
        invalid.mId = { 0x900, 7 };
        mStore.getWritable<ESM4::CombatStyle>().insertStatic(invalid, mStyleKey);
        EXPECT_THROW(MWWorld::resolveOblivionCombatPolicy(mStore, mActorKey, defaults), std::runtime_error);
    }
}
