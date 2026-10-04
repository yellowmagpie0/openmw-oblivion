#include <gtest/gtest.h>
#include <cmath>
#include <limits>
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
    class OblivionArrowLaunchTest : public testing::Test
    {
    protected:
        MWWorld::ESMStore mStore;
        const ESM::FormKey mBowKey = ESM::FormKey::content("bows.esm", 0x800);
        const ESM::FormKey mAmmoKey = ESM::FormKey::content("arrows.esm", 0x800);

        void SetUp() override
        {
            ESM4::Weapon bow{};
            bow.mId = {0x800, 3};
            bow.mData.type = 5;
            bow.mData.damage = 20;
            mStore.getWritable<ESM4::Weapon>().insertStatic(bow, mBowKey);
            ESM4::Ammunition arrow{};
            arrow.mId = {0x800, 7};
            arrow.mData.mDamage = 5;
            arrow.mData.mSpeed = 1;
            mStore.getWritable<ESM4::Ammunition>().insertStatic(arrow, mAmmoKey);
            // Match the independently reviewed S2 bow oracle operands;
            // these differ from compiled physical-combat defaults.
            unsigned id = 0x900;
            for (auto [name, value] : {std::pair{"fFatigueBase", 1.f},
                     {"fDamageWeaponMult", .5f}, {"fDamageSkillMult", 1.5f},
                     {"fDamageWeaponConditionBase", .5f}, {"fDamageWeaponConditionMult", .5f},
                     {"fDamageStrengthBase", .75f}, {"fDamageStrengthMult", .5f}})
            {
                ESM4::GameSetting setting{};
                setting.mId = {id++, 2};
                setting.mEditorId = name;
                setting.mData = value;
                mStore.getWritable<ESM4::GameSetting>().insertStatic(setting);
            }
        }

        MWWorld::OblivionArrowLaunchInput input() const
        {
            return {mBowKey, mAmmoKey, true, .625f, 50, 50, 50, 1, 1, 50};
        }

        MWWorld::OblivionArrowLaunch launch(const MWWorld::OblivionArrowLaunchInput& value) const
        {
            return MWWorld::resolveOblivionArrowLaunch(mStore, ESM::GameProfile::Oblivion, value);
        }
    };

    TEST_F(OblivionArrowLaunchTest, ResolvesDistinctNativeItemsAndSamplesPlayerDraw)
    {
        auto value = input();
        const auto half = launch(value);
        EXPECT_EQ(half.mBow, mBowKey);
        EXPECT_EQ(half.mAmmunition, mAmmoKey);
        EXPECT_FLOAT_EQ(half.mDrawFraction, .5f);
        EXPECT_FLOAT_EQ(half.mDamage, 5.9375f);
        EXPECT_FLOAT_EQ(half.mSpeed, 757.5f);
        EXPECT_NEAR(half.mGravityFactor, .975f, .000001f);
        EXPECT_EQ(half.mShotFatigueDebit, 0);
        value.mBowTimer = 100;
        const auto full = launch(value);
        EXPECT_EQ(full.mDrawFraction, 1);
        EXPECT_FLOAT_EQ(full.mDamage, 11.875f);
        EXPECT_EQ(full.mSpeed, 1500);
        EXPECT_NEAR(full.mGravityFactor, .2f, .000001f);
        value.mBowConditionRatio = .5f;
        value.mAttackBonus = 2;
        EXPECT_FLOAT_EQ(launch(value).mDamage, 13.5f);
    }

    TEST_F(OblivionArrowLaunchTest, NpcIgnoresPlayerTimerAndReturnsPostSamplingShotDebit)
    {
        auto value = input();
        value.mPlayer = false;
        value.mBowTimer = std::numeric_limits<float>::quiet_NaN();
        value.mMarksman = 5;
        value.mMasteryMarksman = 5;
        value.mAgility = 30;
        auto bow = *mStore.search<ESM4::Weapon>(mBowKey);
        auto arrow = *mStore.search<ESM4::Ammunition>(mAmmoKey);
        bow.mData.damage = 100;
        arrow.mData.mDamage = 20;
        mStore.getWritable<ESM4::Weapon>().insertStatic(bow, mBowKey);
        mStore.getWritable<ESM4::Ammunition>().insertStatic(arrow, mAmmoKey);
        const auto shot = launch(value);
        EXPECT_EQ(shot.mDrawFraction, 1);
        EXPECT_NEAR(shot.mDamage, 14.85f, .00001f);
        EXPECT_EQ(shot.mShotFatigueDebit, 5);
        EXPECT_EQ(value.mFatigueRatio, 1);
        value.mFatigueRatio = 135.f / 140.f;
        EXPECT_LT(launch(value).mDamage, shot.mDamage);
        EXPECT_NEAR(shot.mDamage, 14.85f, .00001f); // Captured snapshot remains unchanged.
        value.mPlayer = true;
        EXPECT_THROW(launch(value), std::invalid_argument);
        value.mPlayer = false;
        value.mMarksman = 24;
        value.mMasteryMarksman = 25;
        const auto apprentice = launch(value);
        EXPECT_EQ(apprentice.mShotFatigueDebit, 0);
        value.mMasteryMarksman = 24;
        const auto novice = launch(value);
        EXPECT_EQ(novice.mShotFatigueDebit, 5);
        EXPECT_EQ(novice.mDamage, apprentice.mDamage);
        EXPECT_EQ(novice.mGravityFactor, apprentice.mGravityFactor);
    }

    TEST_F(OblivionArrowLaunchTest, WinningNativeOverridesAreQueriedWithoutTes3Settings)
    {
        ESM::GameSetting legacy;
        legacy.mId = ESM::RefId::stringRefId("fArrowSpeedMult");
        legacy.mValue = ESM::Variant(999.f);
        mStore.getWritable<ESM::GameSetting>().insertStatic(legacy);
        auto value = input();
        value.mPlayer = false;
        EXPECT_EQ(launch(value).mSpeed, 1500);
        auto arrow = *mStore.search<ESM4::Ammunition>(mAmmoKey);
        arrow.mData.mDamage = 10;
        arrow.mData.mSpeed = 2;
        mStore.getWritable<ESM4::Ammunition>().insert(arrow, mAmmoKey);
        ESM4::GameSetting speed{};
        speed.mId = {0x900, 1};
        speed.mEditorId = "fArrowSpeedMult";
        speed.mData = 2000.f;
        mStore.getWritable<ESM4::GameSetting>().insertStatic(speed);
        speed.mData = 3000.f;
        mStore.getWritable<ESM4::GameSetting>().insert(speed);
        const auto shot = launch(value);
        EXPECT_FLOAT_EQ(shot.mDamage, 14.25f);
        EXPECT_EQ(shot.mSpeed, 6000);
        speed.mData = std::int32_t{3000};
        mStore.getWritable<ESM4::GameSetting>().insert(speed);
        EXPECT_THROW(launch(value), std::invalid_argument);
    }

    TEST_F(OblivionArrowLaunchTest, RejectsWrongProfileMissingDeletedAndMalformedInputs)
    {
        auto value = input();
        for (auto profile : {ESM::GameProfile::Auto, ESM::GameProfile::Morrowind})
            EXPECT_THROW(MWWorld::resolveOblivionArrowLaunch(mStore, profile, value), std::invalid_argument);
        value.mBow = {};
        EXPECT_THROW(launch(value), std::invalid_argument);
        value = input();
        value.mAmmunition = mBowKey;
        EXPECT_THROW(launch(value), std::invalid_argument);
        auto bow = *mStore.search<ESM4::Weapon>(mBowKey);
        bow.mData.type = 0;
        mStore.getWritable<ESM4::Weapon>().insertStatic(bow, mBowKey);
        EXPECT_THROW(launch(input()), std::invalid_argument);
        bow.mData.type = 5;
        mStore.getWritable<ESM4::Weapon>().insertStatic(bow, mBowKey);
        auto arrow = *mStore.search<ESM4::Ammunition>(mAmmoKey);
        for (float bad : {-1.f, .5f, 65536.f, std::numeric_limits<float>::infinity(),
                 std::numeric_limits<float>::quiet_NaN()})
        {
            arrow.mData.mDamage = bad;
            mStore.getWritable<ESM4::Ammunition>().insertStatic(arrow, mAmmoKey);
            EXPECT_THROW(launch(input()), std::invalid_argument);
        }
        arrow.mData.mDamage = 5;
        arrow.mData.mSpeed = -1;
        mStore.getWritable<ESM4::Ammunition>().insertStatic(arrow, mAmmoKey);
        EXPECT_THROW(launch(input()), std::invalid_argument);
        ASSERT_TRUE(mStore.getWritable<ESM4::Ammunition>().eraseStatic(mAmmoKey));
        EXPECT_THROW(launch(input()), std::invalid_argument);
    }

}
