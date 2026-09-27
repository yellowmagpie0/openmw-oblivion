#include <gtest/gtest.h>
#include <limits>
#include <utility>

#include <components/esm/records.hpp>
#include "apps/openmw/mwmechanics/oblivioncombat.hpp"
#include <components/esm4/actorvalues.hpp>
#include <components/esm3/statstate.hpp>
#include <components/esm3/npcstate.hpp>
#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwworld/player.hpp"
#include <components/esm3/readerscache.hpp>
#include "apps/openmw/mwbase/environment.hpp"
#include "apps/openmw/mwclass/esm4npc.hpp"
#include "apps/openmw/mwclass/esm4interactive.hpp"
#include "apps/openmw/mwworld/livecellref.hpp"
#include "apps/openmw/mwworld/worldmodel.hpp"

#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/oblivionactorstats.hpp"

namespace
{
    class OblivionActorStatsTest : public testing::Test
    {
    protected:
        MWWorld::ESMStore mStore;
        const ESM::FormKey mActorKey = ESM::FormKey::content("actors.esm", 0x800);
        ESM4::Npc mNpc{};
        std::array<ESM4::Skill, 21> mSkills{};

        void sharedStats()
        {
            for (int i = 0; i < 8; ++i)
            {
                ESM::Attribute attribute{};
                attribute.mId = *ESM::Attribute::indexToRefId(i).getIf<ESM::StringRefId>();
                mStore.getWritable<ESM::Attribute>().insertStatic(attribute);
            }
            for (auto id : MWWorld::oblivionSkillIds())
            {
                ESM::Skill skill{};
                skill.mId = *id.getIf<ESM::StringRefId>();
                // Deliberately different from TES4 canonical grouping.
                skill.mData.mSpecialization = ESM::Class::Stealth;
                mStore.getWritable<ESM::Skill>().insertStatic(skill);
            }
        }

        void autoNpc()
        {
            mNpc.mId = {0x800, 3};
            mNpc.mIsTES4 = true;
            mNpc.mBaseConfig.tes4.flags = ESM4::Npc::TES4_Female | ESM4::Npc::TES4_AutoCalcStats
                | ESM4::Npc::TES4_PCLevelOffset;
            mNpc.mBaseConfig.tes4.levelOrOffset = -1;
            mNpc.mData.attribs.personality = 53;
            ESM4::Race race{};
            race.mId = {0x100, 5};
            race.mAttribFemale = {36, 28, 71, 80, 56, 56, 35, 69};
            race.mTES4SkillBonuses = std::array<ESM4::Race::SkillBonus, 7>{{{16, 0}, {26, 15}, {17, 15}, {17, 4}, {31, 13}, {25, 1}, {21, 12}}};
            mStore.getWritable<ESM4::Race>().insertStatic(race, ESM::FormKey::content("race.esm", 0x100));
            ESM4::Class characterClass{};
            characterClass.mId = {0x200, 4};
            characterClass.mData.mFavoredAttributes = {6, 0};
            characterClass.mData.mMajorSkills = {12, 13, 15, 25, 18, 27, 28};
            characterClass.mData.mSpecialization = 0;
            mStore.getWritable<ESM4::Class>().insertStatic(characterClass, ESM::FormKey::content("classes.esm", 0x200));
            mNpc.mRace = race.mId;
            mNpc.mClass = characterClass.mId;
            const std::array<unsigned, 21> attributes{2, 6, 7, 5, 1, 2, 2, 3, 2, 3, 6, 3, 4, 2, 0, 3, 0, 5, 2, 0, 5};
            const std::array<unsigned, 21> specializations{1, 0, 1, 2, 1, 0, 1, 0, 2, 2, 0, 0, 2, 2, 1, 2, 2, 0, 0, 2, 0};
            for (unsigned i = 0; i < mSkills.size(); ++i)
            {
                auto& skill = mSkills[i];
                skill.mId = {0x1000 + i, 6};
                skill.mIndex = i + 12;
                skill.mData = ESM4::SkillData{i + 12, attributes[i], specializations[i], {1, 2}};
                mStore.getWritable<ESM4::Skill>().insertStatic(skill, ESM::FormKey::content("skills.esm", 0x1000 + i));
            }
            const std::array<std::pair<const char*, float>, 4> floats{{{"fAttributeClassPrimaryBonus", 5.5f},
                {"fAttributeClassSecondaryBonus", 6.5f}, {"fLowLevelNPCBaseHealthMult", .4f}, {"fNPCBaseMagickaMult", 1.5f}}};
            unsigned id = 0x2000;
            for (const auto& [name, value] : floats)
            {
                ESM4::GameSetting setting{};
                setting.mId = {id++, 6};
                setting.mEditorId = name;
                setting.mData = value;
                mStore.getWritable<ESM4::GameSetting>().insertStatic(setting);
            }
            ESM4::GameSetting threshold{};
            threshold.mId = {id, 6};
            threshold.mEditorId = "iLowLevelNPCMaxLevel";
            threshold.mData = std::int32_t{4};
            mStore.getWritable<ESM4::GameSetting>().insertStatic(threshold);
            mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        }
    };

    TEST_F(OblivionActorStatsTest, autoNpcMatchesOriginalFixtureThroughActualWinningStores)
    {
        autoNpc();
        const auto result = MWWorld::resolveOblivionActorBaseStats(mStore, mActorKey, 3);
        const std::array<std::uint8_t, 8> attributes{44, 28, 75, 82, 56, 57, 53, 69};
        const std::array<std::uint8_t, 21> skills{26, 32, 5, 26, 5, 30, 26, 11, 5, 17, 11, 11, 5, 27, 20, 26, 26, 11, 11, 18, 11};
        EXPECT_EQ(result.mLevel, 2);
        EXPECT_EQ(result.mAttributes, attributes);
        EXPECT_EQ(result.mSkills, skills);
        EXPECT_EQ(result.mHealth, 30);
        EXPECT_EQ(result.mMagicka, 70);
        EXPECT_EQ(result.mFatigue, 258);
        EXPECT_FALSE(result.mNaturalDamage);
        EXPECT_EQ(mStore.search<ESM4::Npc>(mActorKey)->mData.attribs.strength, 0); // No shared base mutation.
        auto skill = mSkills[0];
        skill.mData->mGoverningAttribute = 0;
        skill.mData->mSpecialization = 0;
        const auto key = ESM::FormKey::content("skills.esm", 0x1000);
        mStore.getWritable<ESM4::Skill>().insertStatic(skill, key);
        const auto modified = MWWorld::resolveOblivionActorBaseStats(mStore, mActorKey, 3);
        EXPECT_EQ(modified.mAttributes[0], 45);
        EXPECT_EQ(modified.mAttributes[2], 74);
        EXPECT_EQ(modified.mSkills[0], 32);
        ASSERT_TRUE(mStore.getWritable<ESM4::Skill>().eraseStatic(key));
        EXPECT_THROW(MWWorld::resolveOblivionActorBaseStats(mStore, mActorKey, 3), std::runtime_error);
    }

    TEST_F(OblivionActorStatsTest, fixedNpcPreservesAuthoredStatsWithoutAutoCalculationDependencies)
    {
        mNpc.mId = {0x800, 3};
        mNpc.mIsTES4 = true;
        mNpc.mBaseConfig.tes4.levelOrOffset = 10;
        mNpc.mBaseConfig.tes4.baseSpell = 99;
        mNpc.mBaseConfig.tes4.fatigue = 88;
        mNpc.mData.health = 123;
        mNpc.mData.attribs.strength = 37;
        mNpc.mData.skills.sneak = 173;
        mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        const auto result = MWWorld::resolveOblivionActorBaseStats(mStore, mActorKey, 200);
        EXPECT_EQ(result.mLevel, 10);
        EXPECT_EQ(result.mAttributes[0], 37);
        EXPECT_EQ(result.mSkills[19], 173);
        EXPECT_EQ(result.mHealth, 123);
        EXPECT_EQ(result.mMagicka, 99);
        EXPECT_EQ(result.mFatigue, 88);
        mNpc.mBaseConfig.tes4.levelOrOffset = 0;
        mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        EXPECT_THROW(MWWorld::resolveOblivionActorBaseStats(mStore, mActorKey, 200), std::runtime_error);
    }

    TEST_F(OblivionActorStatsTest, creatureKeepsAuthoredAttributesAndUsesNativeScaling)
    {
        ESM4::Creature creature{};
        creature.mId = {0x800, 3};
        creature.mFormKey = mActorKey;
        creature.mAttackReach = 64;
        creature.mBaseConfig.tes4.flags = ESM4::Creature::TES4_PCLevelOffset;
        creature.mBaseConfig.tes4.levelOrOffset = -1;
        creature.mBaseConfig.tes4.baseSpell = 6;
        creature.mBaseConfig.tes4.fatigue = 7;
        creature.mData.health = 5;
        creature.mData.damage = 40;
        creature.mData.combat = 10;
        creature.mData.magic = 20;
        creature.mData.stealth = 30;
        creature.mData.attribs.strength = 99;
        mStore.getWritable<ESM4::Creature>().insertStatic(creature, mActorKey);
        const auto result = MWWorld::resolveOblivionActorBaseStats(mStore, mActorKey, 11);
        EXPECT_EQ(result.mLevel, 10);
        EXPECT_EQ(result.mAttributes[0], 99);
        EXPECT_EQ(result.mSkills[0], 30);
        EXPECT_EQ(result.mSkills[7], 40);
        EXPECT_EQ(result.mSkills[20], 50);
        EXPECT_EQ(result.mHealth, 50);
        EXPECT_EQ(result.mMagicka, 60);
        EXPECT_EQ(result.mFatigue, 70);
        EXPECT_EQ(result.mNaturalDamage, 50);
    }

    TEST_F(OblivionActorStatsTest, refusesPlayerWrongProfileAndMissingNativeInputs)
    {
        EXPECT_THROW(MWWorld::resolveOblivionActorBaseStats(mStore, {}, {}), std::runtime_error);
        EXPECT_THROW(MWWorld::resolveOblivionActorBaseStats(mStore, mActorKey, {}), std::runtime_error);
        autoNpc();
        mNpc.mIsTES4 = false;
        mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        EXPECT_THROW(MWWorld::resolveOblivionActorBaseStats(mStore, mActorKey, 3), std::runtime_error);
        mNpc.mIsTES4 = true;
        mNpc.mClass = {};
        mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        EXPECT_THROW(MWWorld::resolveOblivionActorBaseStats(mStore, mActorKey, 3), std::runtime_error);
        EXPECT_THROW(MWWorld::resolveOblivionActorBaseStats(mStore, ESM::FormKey::content("Oblivion.esm", 7), 3), std::runtime_error);
    }
    TEST_F(OblivionActorStatsTest, liveNpcClassExposesCalculatedStatsWithoutTes3WorldDependencies)
    {
        autoNpc();
        sharedStats();
        mNpc.mBaseConfig.tes4.flags &= ~ESM4::Npc::TES4_PCLevelOffset;
        mNpc.mBaseConfig.tes4.levelOrOffset = 2;
        mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(mStore, readers);
        environment.setWorldModel(model);
        MWClass::ESM4Npc::registerSelf();
        ESM4::ActorCharacter reference{};
        reference.mFormKey = ESM::FormKey::content("actors.esm", 0x900);
        MWWorld::LiveCellRef<ESM4::Npc> live(reference, mStore.search<ESM4::Npc>(mActorKey));
        MWWorld::Ptr ptr(&live);
        auto& stats = ptr.getClass().getCreatureStats(ptr);
        EXPECT_EQ(stats.getLevel(), 2);
        EXPECT_EQ(stats.getAttribute(ESM::Attribute::Strength).getBase(), 44);
        EXPECT_EQ(stats.getAttribute(ESM::Attribute::Intelligence).getBase(), 28);
        EXPECT_EQ(stats.getHealth().getBase(), 30);
        EXPECT_EQ(stats.getHealth().getCurrent(), 30);
        EXPECT_EQ(stats.getMagicka().getBase(), 70);
        EXPECT_EQ(stats.getFatigue().getBase(), 258);
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Athletics), 32);
        EXPECT_FALSE(stats.isDead());
        EXPECT_EQ(&ptr.getClass().getCreatureStats(ptr), &stats);
        EXPECT_EQ(mStore.search<ESM4::Npc>(mActorKey)->mData.attribs.strength, 0);
        EXPECT_THROW(stats.initializeOblivionBaseStats({}, {1, 2, 3}, 1), std::invalid_argument);
        EXPECT_EQ(stats.getHealth().getCurrent(), 30);
        MWMechanics::OblivionActorProjectionInput projection;
        projection.mAttributes.fill({40, {10, 5, -2}});
        projection.mSkills.fill({30, {2, 1, -4}});
        projection.mDynamic = {{{30, 45, 12}, {70, 80, 50}, {258, 280, -5}}};
        MWMechanics::OblivionActorProjection prepared(ptr.getClass().getNpcStats(ptr), projection);
        ASSERT_TRUE(prepared.commit());
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Athletics), 29);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 12);
        EXPECT_EQ(ptr.getClass().getNpcStats(ptr).getAttribute(ESM::Attribute::Strength).getModified(), 53);
        EXPECT_EQ(ptr.getClass().getCapacity(ptr), 265);
        EXPECT_EQ(mStore.search<ESM4::Npc>(mActorKey)->mData.attribs.strength, 0);
    }

    TEST_F(OblivionActorStatsTest, nativePlayerRecomputesFromRawInputsAndRestoresIntoActualPlayer)
    {
        sharedStats();
        ESM::NPC base{};
        base.blank();
        base.mId = ESM::RefId::stringRefId("Player");
        const auto* record = mStore.insertStatic(base);
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(mStore, readers);
        environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        MWWorld::Player player(record);
        const auto ptr = player.getPlayer();
        ESM::NpcState initial{};
        initial.blank();
        ptr.getClass().readAdditionalState(ptr, initial);
        auto& stats = ptr.getClass().getNpcStats(ptr);
        MWMechanics::OblivionCombatService service;
        EXPECT_THROW(service.getPlayerValue(8), std::invalid_argument);
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{7, 3, 9, -3}};
        values.mValues[0] = {50.75f, {.5f, -.25f, -.5f}};
        values.mValues[1] = {30, {.5f, .25f, {}}};
        values.mValues[2].mBase = 20;
        values.mValues[3].mBase = 10;
        values.mValues[5] = {40.75f, {std::nullopt, -.5f, std::nullopt}};
        values.mValues[8] = {999, {10.5f, .25f, -5.f}};
        values.mValues[9] = {999, {5.5f, .25f, -10.f}};
        values.mValues[40].mBase = 15;
        values.mValues[48] = {1.25f, {std::nullopt, .5f, std::nullopt}};
        values.mValues[28] = {20.5f, {2.f, .25f, -1.f}};
        ESM::GameSetting sharedSetting{};
        sharedSetting.mId = ESM::RefId::stringRefId("fPCBaseHealthMult");
        sharedSetting.mValue.setType(ESM::VT_Float);
        sharedSetting.mValue.setFloat(999.f);
        mStore.getWritable<ESM::GameSetting>().insertStatic(sharedSetting);
        ESM4::GameSetting nativeSetting{};
        nativeSetting.mId = {0x9e62f, 0};
        const auto settingKey = ESM::FormKey::content("oblivion.esm", 0x9e62f);
        nativeSetting.mEditorId = "fPCBaseMagickaMult";
        nativeSetting.mData = 1.f;
        mStore.getWritable<ESM4::GameSetting>().insertStatic(nativeSetting, settingKey);
        const auto settings = MWWorld::resolveOblivionPlayerDynamicBaseSettings(mStore);
        EXPECT_EQ(settings.mHealthMultiplier, 2); // TES3 aliases do not supply the native settings.
        EXPECT_EQ(settings.mMagickaMultiplier, 1);
        service.publishPlayerValues(player, values, settings);
        EXPECT_EQ(service.getPlayerValue(0), 50.5f);
        EXPECT_EQ(service.getPlayerIntegerValue(0), 49);
        EXPECT_EQ(stats.getAttribute(ESM::Attribute::Strength).getModified(), 50.5f);
        EXPECT_EQ(stats.getHealth().getBase(), 85);
        EXPECT_EQ(stats.getHealth().getModified(), 95.5f);
        EXPECT_EQ(stats.getHealth().getCurrent(), 90.75f);
        EXPECT_EQ(stats.getMagicka().getBase(), 94.5f);
        EXPECT_EQ(service.getPlayerBaseValue(9), 94);
        EXPECT_EQ(service.getPlayerBaseValue(11), 242);
        EXPECT_THROW(service.getPlayerBaseValue(72), std::invalid_argument);
        EXPECT_EQ(stats.getMagicka().getModified(), 99.5f);
        EXPECT_EQ(stats.getMagicka().getCurrent(), 90.25f);
        EXPECT_EQ(service.getPlayerIntegerValue(9), 89);
        EXPECT_EQ(stats.getFatigue().getBase(), 127);
        EXPECT_EQ(service.findActorValues(values.mActor)->mValues[11].mBase, 242);
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Marksman), 21.75f);
        EXPECT_EQ(service.getPlayerValue(48), 1.75f); // Player has no High-process cache branch.
        EXPECT_THROW(service.getPlayerValue(11), std::invalid_argument);
        EXPECT_THROW(service.getPlayerIntegerValue(72), std::invalid_argument);
        const auto first = *service.findActorValues(values.mActor);
        service.changePlayerValue(player, 8, ESM4::ActorValueModifier::Script,
            ESM4::forceActorValueDelta(100, service.getPlayerValue(8)), settings);
        EXPECT_EQ(stats.getHealth().getCurrent(), 100);
        EXPECT_EQ(stats.getHealth().getModified(), 95.5f); // Force does not replace maximum.
        service.publishPlayerValues(player, first, settings);
        EXPECT_EQ(*service.findActorValues(values.mActor), first);
        service.changePlayerValue(player, 5, ESM4::ActorValueModifier::Script, 1, settings);
        EXPECT_EQ(stats.getHealth().getBase(), 87);
        EXPECT_EQ(stats.getHealth().getCurrent(), 92.75f);
        EXPECT_EQ(stats.getFatigue().getBase(), 128);
        service.changePlayerValue(player, 40, ESM4::ActorValueModifier::Script, 5, settings);
        EXPECT_EQ(stats.getMagicka().getBase(), 126);
        EXPECT_EQ(stats.getMagicka().getCurrent(), 121.75f);
        service.changePlayerValue(player, 8, ESM4::ActorValueModifier::Damage, -1000, settings);
        EXPECT_EQ(stats.getHealth().getCurrent(), -907.25f);
        EXPECT_FALSE(stats.isDead()); // The scalar transaction does not own death policy.
        const auto before = *service.findActorValues(values.mActor);
        EXPECT_EQ(before.mPlayerFormValues, values.mPlayerFormValues);
        auto invalid = before;
        invalid.mPlayerFormValues.reset();
        EXPECT_THROW(service.publishPlayerValues(player, invalid, settings), std::invalid_argument);
        invalid = before;
        invalid.mBase = ESM::FormKey::content("oblivion.esm", 7);
        EXPECT_THROW(service.publishPlayerValues(player, invalid, settings), std::invalid_argument);
        EXPECT_THROW(service.changePlayerValue(player, 40, ESM4::ActorValueModifier::Script,
            std::numeric_limits<float>::max(), settings), std::invalid_argument);
        EXPECT_THROW(service.publishPlayerValues(player, before,
            {std::numeric_limits<float>::infinity(), 1, 5}), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(values.mActor), before);
        EXPECT_EQ(stats.getHealth().getCurrent(), -907.25f);
        EXPECT_EQ(stats.getMagicka().getCurrent(), 121.75f);
        EXPECT_THROW(stats.getSkill(ESM::Skill::Marksman).setBase(99), std::logic_error);
        ESM4::RuntimeActorLife life;
        life.mActor = values.mActor;
        life.mBase = values.mBase;
        life.mPhase = ESM4::ActorLifePhase::Dead;
        service.publishPlayerLife(player, life);
        EXPECT_TRUE(stats.isDead());
        EXPECT_EQ(stats.getHealth().getCurrent(), -907.25f);
        auto invalidLife = life;
        invalidLife.mActor = ESM::FormKey::dynamic("player", 2);
        EXPECT_THROW(service.publishPlayerLife(player, invalidLife), std::invalid_argument);
        EXPECT_EQ(*service.findActorLife(values.mActor), life);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = values.mActor;
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        service.capture(saved);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        MWWorld::Player fresh(record);
        const auto freshPtr = fresh.getPlayer();
        freshPtr.getClass().readAdditionalState(freshPtr, initial);
        restored.publishPlayerValues(fresh, *restored.findActorValues(values.mActor), settings);
        EXPECT_TRUE(freshPtr.getClass().getCreatureStats(freshPtr).isDead());
        EXPECT_FALSE(restored.takeNextDeathEvent());
        EXPECT_EQ(*restored.findActorValues(values.mActor), before);
        EXPECT_EQ(freshPtr.getClass().getCreatureStats(freshPtr).getMagicka().getCurrent(), 121.75f);
        restored.changePlayerValue(fresh, 40, ESM4::ActorValueModifier::Script, -5, settings);
        EXPECT_EQ(freshPtr.getClass().getCreatureStats(freshPtr).getMagicka().getBase(), 94.5f);
        EXPECT_EQ(stats.getMagicka().getBase(), 126); // Fresh instance owns fresh views.
        nativeSetting.mData = 2.f;
        mStore.getWritable<ESM4::GameSetting>().insertStatic(nativeSetting, settingKey);
        restored.publishPlayerValues(fresh, *restored.findActorValues(values.mActor),
            MWWorld::resolveOblivionPlayerDynamicBaseSettings(mStore));
        EXPECT_EQ(freshPtr.getClass().getCreatureStats(freshPtr).getMagicka().getBase(), 139.5f);
        EXPECT_EQ(freshPtr.getClass().getCreatureStats(freshPtr).getMagicka().getCurrent(), 135.25f);
        ASSERT_TRUE(mStore.getWritable<ESM4::GameSetting>().eraseStatic(settingKey));
        restored.publishPlayerValues(fresh, *restored.findActorValues(values.mActor),
            MWWorld::resolveOblivionPlayerDynamicBaseSettings(mStore));
        EXPECT_EQ(freshPtr.getClass().getCreatureStats(freshPtr).getMagicka().getBase(), 72);
        EXPECT_EQ(restored.findActorValues(values.mActor)->mPlayerFormValues, values.mPlayerFormValues);
        auto legacy = before;
        legacy.mPlayerFormValues.reset();
        saved.mNativeActorValues = {legacy};
        saved.mNativeActorLife.clear(); // Version 9 predates the lifecycle contract.
        saved.mVersion = 9;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        EXPECT_THROW(restored.getPlayerValue(8), std::invalid_argument);
        EXPECT_THROW(restored.publishPlayerValues(fresh, legacy, settings), std::invalid_argument);
    }

    TEST_F(OblivionActorStatsTest, nativeFatigueSettingsUseCurrentWinningTypedRecords)
    {
        ESM::GameSetting shared{};
        shared.mId = ESM::RefId::stringRefId("fFatigueReturnBase");
        shared.mValue.setType(ESM::VT_Float);
        shared.mValue.setFloat(999);
        mStore.getWritable<ESM::GameSetting>().insertStatic(shared);
        auto settings = MWWorld::resolveOblivionFatigueRegenerationSettings(mStore);
        EXPECT_EQ(settings.mBase, 10);
        EXPECT_EQ(settings.mEnduranceMultiplier, 0);
        ESM4::GameSetting native{};
        native.mId = {0x980, 3};
        native.mEditorId = "FFATIGUERETURNBASE";
        native.mData = 2.f;
        const auto key = ESM::FormKey::content("actors.esm", 0x980);
        mStore.getWritable<ESM4::GameSetting>().insertStatic(native, key);
        EXPECT_EQ(MWWorld::resolveOblivionFatigueRegenerationSettings(mStore).mBase, 2);
        const auto combined = MWWorld::resolveOblivionFatigueSettings(mStore);
        EXPECT_EQ(combined.mRegeneration.mBase, 2);
        EXPECT_EQ(combined.mMovement.mRunBase, 8);
        EXPECT_EQ(combined.mMastery.mMinimumSkill, (std::array<std::int32_t, 4>{25, 50, 75, 100}));
        native.mData = -3.f;
        mStore.getWritable<ESM4::GameSetting>().insertStatic(native, key);
        EXPECT_EQ(MWWorld::resolveOblivionFatigueRegenerationSettings(mStore).mBase, -3);
        native.mData = std::int32_t{2};
        mStore.getWritable<ESM4::GameSetting>().insertStatic(native, key);
        EXPECT_THROW(MWWorld::resolveOblivionFatigueRegenerationSettings(mStore), std::invalid_argument);
        ASSERT_TRUE(mStore.getWritable<ESM4::GameSetting>().eraseStatic(key));
        EXPECT_EQ(MWWorld::resolveOblivionFatigueRegenerationSettings(mStore).mBase, 10);
    }

    TEST_F(OblivionActorStatsTest, nativeFatigueRegenerationPreservesChannelsAndProcessRules)
    {
        autoNpc();
        sharedStats();
        mNpc.mFormKey = mActorKey;
        mNpc.mBaseConfig.tes4.flags &= ~ESM4::Npc::TES4_PCLevelOffset;
        mNpc.mBaseConfig.tes4.levelOrOffset = 2;
        mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        ESM::NPC playerBase{};
        playerBase.blank();
        playerBase.mId = ESM::RefId::stringRefId("Player");
        const auto* playerRecord = mStore.insertStatic(playerBase);
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(mStore, readers);
        environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        MWClass::ESM4Npc::registerSelf();
        MWWorld::Player player(playerRecord);
        const auto playerPtr = player.getPlayer();
        ESM::NpcState initial{};
        initial.blank();
        playerPtr.getClass().readAdditionalState(playerPtr, initial);
        ESM4::ActorCharacter reference{};
        reference.mFormKey = ESM::FormKey::content("actors.esm", 0x901);
        reference.mId = {0x901, 3};
        reference.mBaseKey = mActorKey;
        MWWorld::LiveCellRef<ESM4::Npc> live(reference, mStore.search<ESM4::Npc>(mActorKey));
        const MWWorld::Ptr npcPtr(&live);
        const auto baseSettings = MWWorld::resolveOblivionPlayerDynamicBaseSettings(mStore);
        struct Case
        {
            ESM4::ActorValueProcess mProcess;
            ESM4::ActorValueModifiers mModifiers;
            float mDuration;
            float mPlayerCurrent;
            float mNpcCurrent;
            std::optional<float> mPlayerDamage;
            std::optional<float> mNpcDamage;
        };
        using Process = ESM4::ActorValueProcess;
        // Base fatigue 40; current Endurance 20.75 but the regeneration query
        // is integer 20. Rate 2 + 20 * .5 = 12, independent of encumbrance.
        const Case cases[] = {
            {Process::Active, {2.f, -1.f, -2.f}, .125f, 40.5f, 40.5f, -.5f, -.5f},
            {Process::Low, {2.f, -1.f, -2.f}, .125f, 40.5f, 38.5f, -.5f, -.5f},
            {Process::Active, {2.f, std::nullopt, -1.f}, 1.f, 42.f, 42.f, 0.f, std::nullopt},
            {Process::Low, {2.f, std::nullopt, std::nullopt}, 1.f, 42.f, 40.f, std::nullopt, std::nullopt},
            {Process::Active, {std::nullopt, -1.f, std::nullopt}, 1.f, 39.f, 51.f, 0.f, 12.f},
            {Process::Active, {std::nullopt, -1.f, 0.f}, 1.f, 39.f, 39.f, 0.f, std::nullopt},
            {Process::Active, {std::nullopt, std::nullopt, -.25f}, 0.f, 39.75f, 39.75f, -.25f, -.25f},
            {Process::Active, {std::nullopt, .75f, std::nullopt}, 1.f, 40.75f, 40.75f, std::nullopt, std::nullopt},
            {Process::Active, {-.5f, std::nullopt, -.25f}, 1.f, 39.5f, 39.5f, 0.f, std::nullopt},
            {Process::Active, {std::nullopt, std::nullopt, -100.f}, 1.f, -48.f, -48.f, -88.f, -88.f},
        };
        for (const bool isPlayer : {false, true})
        {
            const auto ptr = isPlayer ? playerPtr : npcPtr;
            MWMechanics::OblivionCombatService service;
            ESM4::RuntimeActorValues values;
            values.mActor = isPlayer ? ESM::FormKey::dynamic("player", 1) : reference.mFormKey;
            values.mBase = isPlayer ? ESM::FormKey::dynamic("player-base", 1) : mActorKey;
            values.mOwner = isPlayer ? ESM4::ActorValueOwner::Player : ESM4::ActorValueOwner::NonPlayer;
            if (isPlayer)
                values.mPlayerFormValues = {{0, 0, 20, 0}};
            values.mValues[5] = {20, {.75f, {}, {}}};
            values.mValues[8].mBase = 100;
            values.mValues[10].mBase = 40;
            const auto publish = [&] {
                if (isPlayer)
                    service.publishPlayerValues(player, values, baseSettings);
                else
                    service.publishNonPlayerValues(ptr, values);
            };
            const auto regenerate = [&](float duration, ESM4::FatigueRegenerationSettings settings) {
                if (isPlayer)
                    service.regeneratePlayerFatigue(player, duration, settings, baseSettings);
                else
                    service.regenerateNonPlayerFatigue(ptr, duration, settings);
            };
            for (std::size_t i = 0; i < std::size(cases); ++i)
            {
                SCOPED_TRACE(testing::Message() << "player=" << isPlayer << " case=" << i);
                const auto& test = cases[i];
                values.mProcess = test.mProcess;
                values.mValues[10].mModifiers = test.mModifiers;
                publish();
                regenerate(test.mDuration, {2, .5f});
                const auto* result = service.findActorValues(values.mActor);
                ASSERT_NE(result, nullptr);
                EXPECT_EQ(result->mValues[10].mModifiers[0], test.mModifiers[0]);
                EXPECT_EQ(result->mValues[10].mModifiers[1], test.mModifiers[1]);
                EXPECT_EQ(result->mValues[10].mModifiers[2], isPlayer ? test.mPlayerDamage : test.mNpcDamage);
                EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getFatigue().getCurrent(),
                    isPlayer ? test.mPlayerCurrent : test.mNpcCurrent);
                EXPECT_FALSE(ptr.getClass().getCreatureStats(ptr).isDead());
            }
            const auto before = *service.findActorValues(values.mActor);
            const auto sharedBefore = ptr.getClass().getCreatureStats(ptr).getFatigue();
            EXPECT_THROW(regenerate(-1, {2, .5f}), std::invalid_argument);
            EXPECT_THROW(regenerate(std::numeric_limits<float>::infinity(), {2, .5f}), std::invalid_argument);
            EXPECT_THROW(regenerate(1, {std::numeric_limits<float>::quiet_NaN(), .5f}), std::invalid_argument);
            EXPECT_EQ(*service.findActorValues(values.mActor), before);
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getFatigue(), sharedBefore);
            regenerate(1, {-100, 0});
            EXPECT_EQ(*service.findActorValues(values.mActor), before);

            auto movementSettings = MWWorld::resolveOblivionFatigueSettings(mStore);
            movementSettings.mMovement.mJumpBase = 30;
            movementSettings.mMovement.mJumpMultiplier = 0;
            values.mProcess = Process::Active;
            values.mValues[0].mBase = 50;
            if (isPlayer)
                values.mPlayerFormValues = {{0, 0, -30, 0}}; // 50 Strength + 20 Endurance - 30 = 40.
            values.mValues[10].mModifiers = {};
            const auto update = [&](MWMechanics::OblivionFatigueUpdate input,
                                    const MWMechanics::OblivionFatigueSettings& settings) {
                if (isPlayer)
                    service.updatePlayerFatigue(player, input, settings);
                else
                    service.updateNonPlayerFatigue(ptr, input, settings);
            };
            const auto jump = [&](bool canSpend) {
                service.spendPlayerJumpFatigue(player, 100, canSpend, movementSettings);
            };
            const auto current = [&] { return ptr.getClass().getCreatureStats(ptr).getFatigue().getCurrent(); };
            publish();
            update({1, 100, true, true}, movementSettings);
            EXPECT_EQ(current(), 40); // Spend 8, then restore 10; opposite order would leave 32.
            values.mValues[10].mModifiers[2] = -20;
            publish();
            update({1, 100, true, true}, movementSettings);
            EXPECT_EQ(current(), 22);
            EXPECT_EQ(service.findActorValues(values.mActor)->mValues[10].mModifiers[2], -18);
            values.mValues[13] = {100, {std::nullopt, -100.f, std::nullopt}};
            publish();
            update({1, 100, true, true}, movementSettings);
            EXPECT_EQ(current(), 30); // Base Master Athletics, despite current skill zero.
            values.mValues[13] = {};
            publish();
            update({1, 100, true, false}, movementSettings);
            EXPECT_EQ(current(), 30); // Expenditure suppression does not suppress regeneration.
            publish();
            const auto stable = *service.findActorValues(values.mActor);
            auto invalidSettings = movementSettings;
            invalidSettings.mRegeneration.mBase = std::numeric_limits<float>::quiet_NaN();
            EXPECT_THROW(update({1, 100, true, true}, invalidSettings), std::invalid_argument);
            EXPECT_EQ(*service.findActorValues(values.mActor), stable); // No partial running debit.
            EXPECT_EQ(current(), 20);
            update({0, 100, true, true}, movementSettings);
            EXPECT_EQ(*service.findActorValues(values.mActor), stable);
            values.mValues[10].mModifiers = {};
            publish();
            if (isPlayer)
            {
                jump(false);
                EXPECT_EQ(current(), 40);
                jump(true);
                EXPECT_EQ(current(), 10);
                jump(true);
                EXPECT_EQ(current(), 0); // Movement cannot knock an actor out by crossing below zero.
                const auto emptyFatigue = *service.findActorValues(values.mActor);
                jump(true);
                EXPECT_EQ(*service.findActorValues(values.mActor), emptyFatigue);
                values.mValues[26] = {75, {std::nullopt, -75.f, std::nullopt}};
                publish();
                jump(true);
                EXPECT_EQ(current(), 25); // Base Expert Acrobatics, despite current skill zero.
            }
            else
            {
                service.changeNonPlayerValue(ptr, 10, ESM4::ActorValueModifier::Damage, -15);
                EXPECT_EQ(current(), 25);
            }

            ESM4::RuntimeState saved;
            saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
            saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
            saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
            if (!isPlayer)
            {
                ESM4::RuntimeReferenceState actor;
                actor.mKey = values.mActor;
                actor.mBase = values.mBase;
                actor.mCell = saved.mPlayer.mCell;
                saved.mReferences.push_back(actor);
            }
            service.capture(saved);
            MWMechanics::OblivionCombatService restored;
            restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
            EXPECT_EQ(*restored.findActorValues(values.mActor), *service.findActorValues(values.mActor));
            if (isPlayer)
                restored.spendPlayerJumpFatigue(player, 100, true, movementSettings);
            else
                restored.updateNonPlayerFatigue(ptr, {1, 100, true, true}, movementSettings);
            EXPECT_EQ(current(), isPlayer ? 10 : 27);
        }
    }

    TEST_F(OblivionActorStatsTest, nativeResourceRestorationCommitsAllChannelsTogether)
    {
        autoNpc();
        sharedStats();
        mNpc.mFormKey = mActorKey;
        mNpc.mBaseConfig.tes4.flags &= ~ESM4::Npc::TES4_PCLevelOffset;
        mNpc.mBaseConfig.tes4.levelOrOffset = 2;
        mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        ESM::NPC playerBase{};
        playerBase.blank();
        playerBase.mId = ESM::RefId::stringRefId("Player");
        const auto* playerRecord = mStore.insertStatic(playerBase);
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(mStore, readers);
        environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        MWClass::ESM4Npc::registerSelf();
        MWWorld::Player player(playerRecord);
        const auto playerPtr = player.getPlayer();
        ESM::NpcState initial{};
        initial.blank();
        playerPtr.getClass().readAdditionalState(playerPtr, initial);
        ESM4::ActorCharacter reference{};
        reference.mFormKey = ESM::FormKey::content("actors.esm", 0x902);
        reference.mId = {0x902, 3};
        reference.mBaseKey = mActorKey;
        MWWorld::LiveCellRef<ESM4::Npc> live(reference, mStore.search<ESM4::Npc>(mActorKey));
        const MWWorld::Ptr npcPtr(&live);
        const MWMechanics::OblivionRestorationSettings settings{
            {1, 0}, {2, 0}, MWWorld::resolveOblivionPlayerDynamicBaseSettings(mStore)};
        using Process = ESM4::ActorValueProcess;
        for (bool isPlayer : {false, true})
        {
            SCOPED_TRACE(testing::Message() << "player=" << isPlayer);
            MWMechanics::OblivionCombatService service;
            const auto ptr = isPlayer ? playerPtr : npcPtr;
            const auto& stats = ptr.getClass().getCreatureStats(ptr);
            ESM4::RuntimeActorValues values;
            values.mActor = isPlayer ? ESM::FormKey::dynamic("player", 1) : reference.mFormKey;
            values.mBase = isPlayer ? ESM::FormKey::dynamic("player-base", 1) : mActorKey;
            values.mOwner = isPlayer ? ESM4::ActorValueOwner::Player : ESM4::ActorValueOwner::NonPlayer;
            if (isPlayer)
                values.mPlayerFormValues = {{100, 100, -10, 0}};
            values.mValues[2] = {50, {.75f, {}, {}}}; // Integer Willpower 50, not 50.75.
            values.mValues[8].mBase = 100;
            values.mValues[9].mBase = 100;
            values.mValues[10].mBase = 40;
            for (int av : {8, 9, 10})
                values.mValues[av].mModifiers = {2.f, -1.f, -10.f};
            const auto publish = [&] {
                if (isPlayer)
                    service.publishPlayerValues(player, values, settings.mPlayerBase);
                else
                    service.publishNonPlayerValues(ptr, values);
            };
            const auto restore = [&](const MWMechanics::OblivionRestorationUpdate& input,
                                     const MWMechanics::OblivionRestorationSettings& config) {
                if (isPlayer)
                    service.restorePlayerResources(player, input, config);
                else
                    service.restoreNonPlayerResources(ptr, input, config);
            };
            for (Process process : {Process::Active, Process::Low})
            {
                values.mProcess = process;
                publish();
                restore({1, true, false}, settings);
                const bool maximumEligible = isPlayer || process == Process::Active;
                EXPECT_EQ(stats.getHealth().getCurrent(), maximumEligible ? 101 : 99);
                EXPECT_FLOAT_EQ(stats.getMagicka().getCurrent(), maximumEligible ? 92.02f : 90.f);
                EXPECT_EQ(stats.getFatigue().getCurrent(), maximumEligible ? 33 : 31);
                const auto* result = service.findActorValues(values.mActor);
                ASSERT_NE(result, nullptr);
                for (int av : {8, 9, 10})
                {
                    EXPECT_EQ(result->mValues[av].mModifiers[0], 2);
                    EXPECT_EQ(result->mValues[av].mModifiers[1], -1);
                }
                EXPECT_EQ(result->mValues[8].mModifiers[2], isPlayer ? std::optional<float>(0) : std::nullopt);
            }
            values.mProcess = Process::Active;
            publish();
            const auto unchanged = *service.findActorValues(values.mActor);
            restore({0, false, false}, settings);
            EXPECT_EQ(*service.findActorValues(values.mActor), unchanged);
            restore({0, true, false}, settings);
            EXPECT_EQ(stats.getHealth().getCurrent(), 101); // Health helper ignores duration.
            EXPECT_EQ(stats.getMagicka().getCurrent(), 91);
            EXPECT_EQ(stats.getFatigue().getCurrent(), 31);
            publish();
            restore({1, false, true}, settings);
            EXPECT_EQ(stats.getHealth().getCurrent(), 91); // No Health request.
            EXPECT_EQ(stats.getMagicka().getCurrent(), 91); // Active item suppresses only Magicka.
            EXPECT_EQ(stats.getFatigue().getCurrent(), 33);
            values.mValues[57].mBase = 1;
            publish();
            restore({1, true, false}, settings);
            EXPECT_EQ(stats.getHealth().getCurrent(), 101);
            EXPECT_EQ(stats.getMagicka().getCurrent(), 91);
            values.mValues[57].mBase = .75f;
            publish();
            restore({1, true, false}, settings);
            EXPECT_FLOAT_EQ(stats.getMagicka().getCurrent(), 92.02f); // Integer query truncates to zero.

            // A later invalid resource must not commit an earlier valid Health or Magicka request.
            publish();
            const auto before = *service.findActorValues(values.mActor);
            auto invalid = settings;
            invalid.mFatigue.mBase = std::numeric_limits<float>::quiet_NaN();
            EXPECT_THROW(restore({1, true, false}, invalid), std::invalid_argument);
            EXPECT_EQ(*service.findActorValues(values.mActor), before);
            EXPECT_EQ(stats.getHealth().getCurrent(), 91);
            EXPECT_EQ(stats.getMagicka().getCurrent(), 91);
            EXPECT_EQ(stats.getFatigue().getCurrent(), 31);
            EXPECT_THROW(restore({-1, true, false}, settings), std::invalid_argument);
            EXPECT_EQ(*service.findActorValues(values.mActor), before);

            // Native sparse storage distinguishes absent Damage from a stored zero.
            for (auto damage : {std::optional<float>{}, std::optional<float>{0}})
            {
                for (int av : {8, 9, 10})
                    values.mValues[av].mModifiers[2] = damage;
                publish();
                restore({1, true, false}, settings);
                const bool insertsPositive = !isPlayer && !damage;
                EXPECT_EQ(stats.getHealth().getCurrent(), insertsPositive ? 102 : 101);
                EXPECT_FLOAT_EQ(stats.getMagicka().getCurrent(), insertsPositive ? 102.02f : 101.f);
                EXPECT_EQ(stats.getFatigue().getCurrent(), insertsPositive ? 43 : 41);
            }
            for (int av : {8, 9, 10})
                values.mValues[av].mModifiers[2] = -10;
            publish();
            restore({1, true, false}, settings);
            ESM4::RuntimeState saved;
            saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
            saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
            saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
            if (!isPlayer)
            {
                ESM4::RuntimeReferenceState actor;
                actor.mKey = values.mActor;
                actor.mBase = values.mBase;
                actor.mCell = saved.mPlayer.mCell;
                saved.mReferences.push_back(actor);
            }
            service.capture(saved);
            MWMechanics::OblivionCombatService restored;
            restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
            EXPECT_EQ(*restored.findActorValues(values.mActor), *service.findActorValues(values.mActor));
            if (isPlayer)
                restored.restorePlayerResources(player, {1, true, false}, settings);
            else
                restored.restoreNonPlayerResources(ptr, {1, true, false}, settings);
            // NPC Health's absent Damage from the first clamped restore now admits +1.
            EXPECT_EQ(stats.getHealth().getCurrent(), isPlayer ? 101 : 102);
            EXPECT_FLOAT_EQ(stats.getMagicka().getCurrent(), 93.04f);
            EXPECT_EQ(stats.getFatigue().getCurrent(), 35);

            // Nonplayer current Magicka has an outer scale; its restoration maximum does not.
            if (!isPlayer)
            {
                values.mValues[40].mBase = 20;
                values.mValues[9].mModifiers[2] = -10;
                publish();
                restore({1, true, false}, settings);
                EXPECT_EQ(stats.getMagicka().getCurrent(), 182); // Above unscaled maximum: no request.
                values.mValues[40].mBase = 5;
                publish();
                restore({1, true, false}, settings);
                EXPECT_FLOAT_EQ(stats.getMagicka().getCurrent(), 46.01f); // Request 1.02, then scale .5.
            }
        }
    }

    TEST_F(OblivionActorStatsTest, nativeFatigueWritersLeaveDeadPlayerUnchanged)
    {
        sharedStats();
        ESM::NPC base{};
        base.blank();
        base.mId = ESM::RefId::stringRefId("Player");
        const auto* record = mStore.insertStatic(base);
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(mStore, readers);
        environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        MWWorld::Player player(record);
        const auto ptr = player.getPlayer();
        ESM::NpcState initial{};
        initial.blank();
        initial.mCreatureStats.mDead = true;
        ptr.getClass().readAdditionalState(ptr, initial);
        auto& stats = ptr.getClass().getCreatureStats(ptr);
        ASSERT_TRUE(stats.isDead());
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{100, 0, 20, 0}};
        values.mValues[10].mModifiers[2] = -5;
        const auto settings = MWWorld::resolveOblivionFatigueSettings(mStore);
        MWMechanics::OblivionCombatService service;
        service.publishPlayerValues(player, values, settings.mPlayerBase);
        const auto before = *service.findActorValues(values.mActor);
        EXPECT_EQ(stats.getFatigue().getCurrent(), 15);
        service.regeneratePlayerFatigue(player, 1, settings.mRegeneration, settings.mPlayerBase);
        service.updatePlayerFatigue(player, {1, 100, true, true}, settings);
        service.spendPlayerJumpFatigue(player, 100, true, settings);
        service.restorePlayerResources(player, {3600, true, false},
            {{.75f, .02f}, settings.mRegeneration, settings.mPlayerBase});
        EXPECT_EQ(*service.findActorValues(values.mActor), before);
        EXPECT_EQ(stats.getFatigue().getCurrent(), 15);
        EXPECT_TRUE(stats.isDead());
    }

    TEST_F(OblivionActorStatsTest, nativeServiceOwnsNpcValuesAcrossMutationAndReload)
    {
        autoNpc();
        mNpc.mFormKey = mActorKey;
        sharedStats();
        mNpc.mBaseConfig.tes4.flags &= ~ESM4::Npc::TES4_PCLevelOffset;
        mNpc.mBaseConfig.tes4.levelOrOffset = 2;
        mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(mStore, readers);
        environment.setWorldModel(model);
        MWClass::ESM4Npc::registerSelf();
        ESM4::ActorCharacter reference{};
        reference.mFormKey = ESM::FormKey::content("actors.esm", 0x900);
        reference.mId = {0x900, 3};
        reference.mBaseKey = mActorKey;
        mStore.getWritable<ESM4::ActorCharacter>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Npc> live(reference, mStore.search<ESM4::Npc>(mActorKey));
        MWWorld::Ptr ptr(&live);
        MWMechanics::OblivionCombatService service;
        EXPECT_THROW(service.getNonPlayerValue(ptr, 8), std::invalid_argument);
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        ASSERT_EQ(ptr.getCellRef().getFormKey(), values.mActor);
        ASSERT_EQ(ptr.get<ESM4::Npc>()->mBase->mFormKey, values.mBase);
        values.mValues[0] = {100, {0.5f, -0.5f, std::nullopt}};
        values.mValues[8] = {100.75f, {10.5f, -0.5f, -5.f}};
        values.mValues[9] = {50, {5, 0.75f, -10}};
        values.mValues[10] = {40, {2, -1, -2}};
        values.mValues[40].mBase = 15;
        for (std::size_t i = 12; i <= 32; ++i)
            values.mValues[i] = {30, {2, 0.5f, -1}};
        service.publishNonPlayerValues(ptr, values);
        EXPECT_EQ(service.getNonPlayerValue(ptr, 0), 100);
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptr, 0), 99);
        EXPECT_EQ(ptr.getClass().getCapacity(ptr), 500);
        EXPECT_EQ(service.getNonPlayerValue(ptr, 8), 105.75f);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 105.75f);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getModified(), 110.5f);
        EXPECT_EQ(service.getNonPlayerValue(ptr, 9), 68.625f);
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptr, 9), 67);
        const auto beforeForce = *service.findActorValues(values.mActor);
        service.changeNonPlayerValue(ptr, 9, ESM4::ActorValueModifier::Script,
            ESM4::forceActorValueDelta(100, service.getNonPlayerValue(ptr, 9)));
        // Native Force computes delta before the NPC outer multiplier. It does
        // not solve an inverse scaling equation to guarantee the requested AV.
        EXPECT_EQ(service.getNonPlayerValue(ptr, 9), 115.6875f);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getMagicka().getModified(), 55);
        service.publishNonPlayerValues(ptr, beforeForce);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getMagicka().getCurrent(), 68.625f);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getMagicka().getModified(), 55);
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Athletics), 31.5f);
        service.changeNonPlayerValue(ptr, 28, ESM4::ActorValueModifier::Script, 2);
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Marksman), 33.5f);
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Athletics), 31.5f);
        service.changeNonPlayerValue(ptr, 9, ESM4::ActorValueModifier::Maximum, 5);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getMagicka().getCurrent(), 76.125f);
        service.changeNonPlayerValue(ptr, 40, ESM4::ActorValueModifier::Script, 5);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getMagicka().getCurrent(), 101.5f);
        service.changeNonPlayerValue(ptr, 8, ESM4::ActorValueModifier::Damage, -200);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), -94.25f);
        EXPECT_FALSE(ptr.getClass().getCreatureStats(ptr).isDead());
        const auto before = *service.findActorValues(reference.mFormKey);
        auto invalid = before;
        invalid.mBase = ESM::FormKey::content("actors.esm", 0x999);
        EXPECT_THROW(service.publishNonPlayerValues(ptr, invalid), std::invalid_argument);
        invalid = before;
        invalid.mValues[71].mBase = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(service.publishNonPlayerValues(ptr, invalid), std::runtime_error);
        invalid = before;
        invalid.mValues[9] = {50, {3e38f, std::nullopt, std::nullopt}};
        // All stored values and ordinary composition are finite; the outer
        // Magicka scale overflows only while preparing complete shared views.
        EXPECT_NO_THROW(invalid.validate());
        EXPECT_THROW(service.publishNonPlayerValues(ptr, invalid), std::invalid_argument);
        EXPECT_THROW(service.changeNonPlayerValue(ptr, 8, ESM4::ActorValueModifier::Damage,
            std::numeric_limits<float>::infinity()), std::invalid_argument);
        EXPECT_THROW(service.getNonPlayerValue(ptr, 11), std::invalid_argument);
        EXPECT_THROW(service.getNonPlayerValue(ptr, 48), std::invalid_argument);
        EXPECT_THROW(service.getNonPlayerIntegerValue(ptr, 255), std::invalid_argument);
        EXPECT_THROW(service.publishNonPlayerValues(MWWorld::Ptr{}, before), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(reference.mFormKey), before);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), -94.25f);
        EXPECT_EQ(mStore.search<ESM4::Npc>(mActorKey)->mData.attribs.strength, 0);

        ESM4::RuntimeActorLife life;
        life.mActor = values.mActor;
        life.mBase = values.mBase;
        life.mPhase = ESM4::ActorLifePhase::Dead;
        service.publishNonPlayerLife(ptr, life);
        EXPECT_TRUE(ptr.getClass().getCreatureStats(ptr).isDead());
        EXPECT_EQ(service.getNonPlayerValue(ptr, 8), -94.25f);
        EXPECT_FALSE(service.takeNextDeathEvent()); // Loading a view does not dispatch death.
        auto invalidLife = life;
        invalidLife.mBase = ESM::FormKey::content("actors.esm", 0x999);
        EXPECT_THROW(service.publishNonPlayerLife(ptr, invalidLife), std::invalid_argument);
        invalidLife = life;
        invalidLife.mRecoveryRemaining = 1;
        EXPECT_THROW(service.publishNonPlayerLife(ptr, invalidLife), std::runtime_error);
        EXPECT_EQ(*service.findActorLife(values.mActor), life);

        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        ESM4::RuntimeReferenceState savedActor;
        savedActor.mKey = values.mActor;
        savedActor.mBase = values.mBase;
        savedActor.mCell = saved.mPlayer.mCell;
        saved.mReferences.push_back(savedActor);
        service.capture(saved);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()), mStore);
        EXPECT_EQ(restored.getNonPlayerValue(values.mActor, 9, mStore), 101.5f);
        EXPECT_EQ(restored.getNonPlayerIntegerValue(values.mActor, 9, mStore), 100);
        EXPECT_EQ(restored.getNonPlayerBaseValue(values.mActor, 8, mStore), 100);
        EXPECT_EQ(restored.getNonPlayerBaseValue(values.mActor, 9, mStore), 50);
        MWWorld::LiveCellRef<ESM4::Npc> newLive(reference, mStore.search<ESM4::Npc>(mActorKey));
        MWWorld::Ptr newPtr(&newLive);
        restored.publishNonPlayerValues(newPtr, *restored.findActorValues(reference.mFormKey));
        EXPECT_EQ(restored.getNonPlayerValue(newPtr, 8), service.getNonPlayerValue(ptr, 8));
        EXPECT_TRUE(newPtr.getClass().getCreatureStats(newPtr).isDead());
        EXPECT_FALSE(restored.takeNextDeathEvent());
        EXPECT_EQ(newPtr.getClass().getCreatureStats(newPtr).getHealth().getCurrent(), -94.25f);
        EXPECT_EQ(newPtr.getClass().getCreatureStats(newPtr).getMagicka().getCurrent(), 101.5f);
        EXPECT_EQ(newPtr.getClass().getSkill(newPtr, ESM::Skill::Athletics), 31.5f);
        EXPECT_THROW(newPtr.getClass().getNpcStats(newPtr).getSkill(ESM::Skill::Athletics).setBase(200),
            std::logic_error);
        auto low = *restored.findActorValues(reference.mFormKey);
        low.mProcess = ESM4::ActorValueProcess::Low;
        restored.publishNonPlayerValues(newPtr, low);
        EXPECT_EQ(restored.getNonPlayerValue(newPtr, 9), 81.5f);
        EXPECT_EQ(newPtr.getClass().getCreatureStats(newPtr).getMagicka().getModified(), 50);
    }

    TEST_F(OblivionActorStatsTest, sharedNpcBaseWritesReachResidentsUnloadedAndFutureActorsAtomically)
    {
        autoNpc();
        sharedStats();
        mNpc.mFormKey = mActorKey;
        mNpc.mBaseConfig.tes4.flags &= ~ESM4::Npc::TES4_PCLevelOffset;
        mNpc.mBaseConfig.tes4.levelOrOffset = 2;
        mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(mStore, readers);
        environment.setWorldModel(model);
        MWClass::ESM4Npc::registerSelf();
        std::array<ESM4::ActorCharacter, 4> refs{};
        std::array<std::unique_ptr<MWWorld::LiveCellRef<ESM4::Npc>>, 4> lives;
        std::array<MWWorld::Ptr, 4> ptrs;
        MWMechanics::OblivionCombatService service;
        ESM4::RuntimeActorValues original;
        original.mBase = mActorKey;
        original.mValues[0] = {30, {1, -.5f, -2}};
        original.mValues[8].mBase = 100;
        original.mValues[9].mBase = 50;
        original.mValues[10].mBase = 40;
        for (std::size_t i = 0; i < refs.size(); ++i)
        {
            auto& ref = refs[i];
            ref.mId = {static_cast<std::uint32_t>(0x900 + i), 3};
            ref.mFormKey = ESM::FormKey::content("actors.esm", 0x900 + i);
            ref.mBaseKey = mActorKey;
            mStore.getWritable<ESM4::ActorCharacter>().insertStatic(ref, ref.mFormKey);
            lives[i] = std::make_unique<MWWorld::LiveCellRef<ESM4::Npc>>(ref, mStore.search<ESM4::Npc>(mActorKey));
            ptrs[i] = MWWorld::Ptr(lives[i].get());
            original.mActor = ref.mFormKey;
            if (i < 3)
                service.publishNonPlayerValues(ptrs[i], original);
        }
        // Third reference unloads; the authority must not retain a Ptr to it.
        ptrs[2] = {};
        lives[2].reset();
        const std::span<const MWWorld::Ptr> residents(ptrs.data(), 2);
        const auto action = service.allocateAction();
        service.setNonPlayerBaseValue(ptrs[0], 0, 257, residents);
        for (std::size_t i = 0; i < 3; ++i)
        {
            const auto* state = service.findActorValues(refs[i].mFormKey);
            ASSERT_NE(state, nullptr);
            EXPECT_EQ(state->mValues[0].mBase, 1);
            EXPECT_EQ(state->mValues[0].mModifiers, original.mValues[0].mModifiers);
            EXPECT_EQ(service.getNonPlayerValue(refs[i].mFormKey, 0, mStore), -.5f);
        }
        for (const auto& ptr : residents)
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getAttribute(ESM::Attribute::Strength).getModified(), -.5f);
        EXPECT_EQ(mStore.search<ESM4::Npc>(mActorKey)->mData.attribs.strength, 0);
        service.setNonPlayerBaseValue(ptrs[0], 8, 16777217, residents);
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptrs[0], 8), 16777217);
        EXPECT_EQ(service.getNonPlayerValue(ptrs[0], 8), 16777216.f);
        EXPECT_EQ(service.getNonPlayerBaseValue(refs[2].mFormKey, 8, mStore), 16777216);
        EXPECT_EQ(service.getNonPlayerIntegerValue(refs[2].mFormKey, 8, mStore), 16777217);
        service.changeNonPlayerValue(ptrs[0], 8, ESM4::ActorValueModifier::Script, .5f);
        EXPECT_EQ(service.getNonPlayerValue(ptrs[0], 8), 16777218.f);
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptrs[0], 8), 16777217);
        EXPECT_EQ(ptrs[0].getClass().getCreatureStats(ptrs[0]).getHealth().getCurrent(), 16777218.f);
        service.changeNonPlayerValue(ptrs[0], 8, ESM4::ActorValueModifier::Damage, -.5f);
        EXPECT_EQ(service.getNonPlayerValue(ptrs[0], 8), 16777216.f);
        service.changeNonPlayerValue(ptrs[0], 8, ESM4::ActorValueModifier::Script, -.5f);
        service.changeNonPlayerValue(ptrs[0], 8, ESM4::ActorValueModifier::Damage, .5f);
        service.setNonPlayerBaseValue(ptrs[0], 9, -1, residents);
        EXPECT_EQ(service.getNonPlayerBaseValue(refs[0].mFormKey, 9, mStore), 65535);
        service.setNonPlayerBaseValue(ptrs[0], 71, 16777217, residents);
        EXPECT_EQ(service.getNonPlayerValue(ptrs[0], 71), 16777216.f);
        service.setNonPlayerBaseValue(ptrs[0], 71, 0, residents);
        EXPECT_EQ(service.getNonPlayerValue(ptrs[0], 71), 0);
        // A new reference's authored values cannot erase earlier shared writes.
        service.publishNonPlayerValues(ptrs[3], original);
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptrs[3], 8), 16777217);
        EXPECT_EQ(service.getNonPlayerValue(ptrs[3], 0), -.5f);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        for (const auto& ref : refs)
        {
            ESM4::RuntimeReferenceState savedActor;
            savedActor.mKey = ref.mFormKey;
            savedActor.mBase = ref.mBaseKey;
            savedActor.mCell = saved.mPlayer.mCell;
            saved.mReferences.push_back(savedActor);
        }
        service.capture(saved);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()), mStore);
        EXPECT_EQ(restored.getNonPlayerIntegerValue(refs[2].mFormKey, 8, mStore), 16777217);
        EXPECT_EQ(restored.getNonPlayerBaseValue(refs[2].mFormKey, 8, mStore), 16777216);
        EXPECT_TRUE(restored.isActionPending(action));
        auto conflicting = saved;
        conflicting.mNativeActorValues[0].mValues[8].mBase = 100;
        EXPECT_THROW(restored.restore(conflicting, mStore), std::invalid_argument);
        auto after = saved;
        restored.capture(after);
        EXPECT_EQ(after, saved);
        auto fractional = saved;
        for (auto& entry : fractional.mNativeActorBases[0].mValues)
            if (entry.mActorValue == 71)
                entry.mValue = -1.75f;
        for (auto& actor : fractional.mNativeActorValues)
            actor.mValues[71].mBase = -1;
        restored.restore(ESM4::RuntimeState::deserializeBinary(fractional.serializeBinary()), mStore);
        EXPECT_EQ(restored.getNonPlayerValue(refs[2].mFormKey, 71, mStore), -1);
        EXPECT_EQ(restored.getNonPlayerIntegerValue(refs[2].mFormKey, 71, mStore), -1);
        restored.capture(after);
        EXPECT_EQ(after, fractional);
        const auto unchanged = [&] {
            service.capture(after);
            EXPECT_EQ(after, saved);
            EXPECT_EQ(service.getNonPlayerIntegerValue(ptrs[0], 8), 16777217);
            EXPECT_EQ(ptrs[0].getClass().getCreatureStats(ptrs[0]).getHealth().getCurrent(), 16777216.f);
        };
        EXPECT_THROW(service.setNonPlayerBaseValue(ptrs[0], 8, 200, {}), std::invalid_argument);
        unchanged();
        const std::array duplicate{ptrs[0], ptrs[0]};
        EXPECT_THROW(service.setNonPlayerBaseValue(ptrs[0], 8, 200, duplicate), std::invalid_argument);
        unchanged();
        EXPECT_THROW(service.setNonPlayerBaseValue(ptrs[0], 48, 1, residents), std::invalid_argument);
        unchanged();
        EXPECT_THROW(service.setNonPlayerBaseValue(ptrs[0], 71, std::numeric_limits<std::int32_t>::max(), residents),
            std::invalid_argument);
        unchanged();
        // The final resident overflows only after changing the shared multiplier.
        auto dangerous = *service.findActorValues(refs[3].mFormKey);
        dangerous.mValues[9].mModifiers[0] = 3e38f;
        service.publishNonPlayerValues(ptrs[3], dangerous);
        service.capture(saved);
        const std::array allResidents{ptrs[0], ptrs[1], ptrs[3]};
        EXPECT_THROW(service.setNonPlayerBaseValue(ptrs[0], 40, 20, allResidents), std::invalid_argument);
        unchanged();
        EXPECT_EQ(ptrs[3].getClass().getCreatureStats(ptrs[3]).getMagicka().getCurrent(), 3e38f);
    }

    TEST_F(OblivionActorStatsTest, playerCommandsKeepBaseScriptAndConsoleDamageDistinct)
    {
        sharedStats();
        ESM::NPC base{};
        base.blank();
        base.mId = ESM::RefId::stringRefId("Player");
        const auto* record = mStore.insertStatic(base);
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(mStore, readers);
        environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        MWWorld::Player player(record);
        const auto ptr = player.getPlayer();
        ESM::NpcState initial{};
        initial.blank();
        ptr.getClass().readAdditionalState(ptr, initial);
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{7, 3, 9, 0}};
        values.mValues[5].mBase = 40;
        values.mValues[8].mModifiers = {10, 2, -5};
        const ESM4::PlayerDynamicBaseSettings settings{2, 2, 5};
        MWMechanics::OblivionCombatService service;
        service.publishPlayerValues(player, values, settings);
        using Command = ESM4::ActorValueCommand;
        using Source = ESM4::ActorValueCommandSource;
        const auto run = [&](std::uint8_t value, Command command, Source source, std::int32_t amount,
                             bool god = false) {
            return service.executePlayerValueCommand(player, value, command, source, amount, {god, false}, settings);
        };
        EXPECT_EQ(service.getPlayerValue(8), 94);
        const auto suppressed = run(8, Command::Mod, Source::Script, -4, true);
        EXPECT_FALSE(suppressed.mAccepted);
        EXPECT_FALSE(suppressed.mHealthReactionDelta);
        EXPECT_EQ(service.getPlayerValue(8), 94);
        const auto damage = run(8, Command::Mod, Source::Console, -4);
        EXPECT_TRUE(damage.mAccepted);
        EXPECT_EQ(damage.mHealthReactionDelta, -4);
        EXPECT_EQ(service.findActorValues(values.mActor)->mValues[8].mModifiers,
            (ESM4::ActorValueModifiers{10, 2, -9}));
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 90);
        const auto force = run(8, Command::Force, Source::Script, 100);
        EXPECT_TRUE(force.mAccepted);
        EXPECT_FALSE(force.mHealthReactionDelta);
        EXPECT_EQ(service.getPlayerValue(8), 100);
        EXPECT_EQ(service.getPlayerBaseValue(8), 87);
        EXPECT_EQ(service.findActorValues(values.mActor)->mValues[8].mModifiers,
            (ESM4::ActorValueModifiers{10, 12, -9}));
        const auto set = run(8, Command::Set, Source::Console, -1, true);
        EXPECT_TRUE(set.mAccepted); // Set is not suppressed by god mode.
        EXPECT_FALSE(set.mHealthReactionDelta);
        EXPECT_EQ((*service.findActorValues(values.mActor)->mPlayerFormValues)[0], -1);
        EXPECT_EQ(service.getPlayerBaseValue(8), 79);
        EXPECT_EQ(service.getPlayerValue(8), 92);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 92);
        const auto before = *service.findActorValues(values.mActor);
        EXPECT_THROW(run(8, static_cast<Command>(255), Source::Script, 0), std::invalid_argument);
        EXPECT_THROW(run(8, Command::Set, static_cast<Source>(255), 0), std::invalid_argument);
        EXPECT_THROW(service.executePlayerValueCommand(player, 8, Command::Mod, Source::Script, -1, {},
            (ESM4::PlayerDynamicBaseSettings{2, 2, std::numeric_limits<float>::infinity()})), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(values.mActor), before);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 92);
    }

    TEST_F(OblivionActorStatsTest, npcCommandsPreserveScaledForceFatigueEligibilityAndSharedBaseWrites)
    {
        autoNpc();
        sharedStats();
        mNpc.mFormKey = mActorKey;
        mNpc.mBaseConfig.tes4.flags &= ~ESM4::Npc::TES4_PCLevelOffset;
        mNpc.mBaseConfig.tes4.levelOrOffset = 2;
        mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(mStore, readers);
        environment.setWorldModel(model);
        MWClass::ESM4Npc::registerSelf();
        ESM4::ActorCharacter ref{};
        ref.mId = {0x900, 3};
        ref.mFormKey = ESM::FormKey::content("actors.esm", 0x900);
        ref.mBaseKey = mActorKey;
        mStore.getWritable<ESM4::ActorCharacter>().insertStatic(ref, ref.mFormKey);
        MWWorld::LiveCellRef<ESM4::Npc> live(ref, mStore.search<ESM4::Npc>(mActorKey));
        const MWWorld::Ptr ptr(&live);
        const std::array residents{ptr};
        ESM4::RuntimeActorValues values;
        values.mActor = ref.mFormKey;
        values.mBase = mActorKey;
        values.mValues[8] = {100, {10, 2, -5}};
        values.mValues[9] = {50, {0, -4.25f, std::nullopt}};
        values.mValues[10].mBase = 40;
        values.mValues[40].mBase = 15;
        MWMechanics::OblivionCombatService service;
        service.publishNonPlayerValues(ptr, values);
        using Command = ESM4::ActorValueCommand;
        using Source = ESM4::ActorValueCommandSource;
        const auto run = [&](std::uint8_t value, Command command, Source source, std::int32_t amount,
                             bool spend = true) {
            return service.executeNonPlayerValueCommand(ptr, value, command, source, amount, {true, spend}, residents);
        };
        EXPECT_EQ(service.getNonPlayerValue(ptr, 9), 68.625f);
        EXPECT_TRUE(run(9, Command::Force, Source::Script, 100).mAccepted);
        EXPECT_EQ(service.getNonPlayerValue(ptr, 9), 115.6875f); // Scale applies again after delta storage.
        EXPECT_EQ(service.findActorValues(values.mActor)->mValues[9].mModifiers[1], 27.125f);
        const auto before = *service.findActorValues(values.mActor);
        EXPECT_FALSE(run(10, Command::Mod, Source::Console, -1, false).mAccepted);
        EXPECT_EQ(*service.findActorValues(values.mActor), before);
        EXPECT_TRUE(run(10, Command::Mod, Source::Console, 1, false).mAccepted);
        EXPECT_EQ(service.getNonPlayerValue(ptr, 10), 41); // Absent sparse Damage accepts positive insertion.
        const auto health = run(8, Command::Force, Source::Console, 100);
        EXPECT_EQ(health.mHealthReactionDelta, -7);
        EXPECT_EQ(service.getNonPlayerValue(ptr, 8), 100);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 100);
        EXPECT_TRUE(run(8, Command::Set, Source::Script, 200).mAccepted);
        EXPECT_EQ(service.getNonPlayerValue(ptr, 8), 200);
        EXPECT_EQ(service.findActorValues(values.mActor)->mValues[8].mModifiers,
            (ESM4::ActorValueModifiers{10, 2, -12}));
        const auto final = *service.findActorValues(values.mActor);
        EXPECT_THROW(run(48, Command::Mod, Source::Script, 1), std::invalid_argument);
        EXPECT_THROW(run(8, static_cast<Command>(255), Source::Console, 1), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(values.mActor), final);
    }

    TEST_F(OblivionActorStatsTest, sharedPlayerBaseWritesPreserveRawContributionsAndRecomputeDerivedViews)
    {
        sharedStats();
        ESM::NPC base{};
        base.blank();
        base.mId = ESM::RefId::stringRefId("Player");
        const auto* record = mStore.insertStatic(base);
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(mStore, readers);
        environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        MWWorld::Player player(record);
        const auto ptr = player.getPlayer();
        ESM::NpcState initial{};
        initial.blank();
        ptr.getClass().readAdditionalState(ptr, initial);
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{7, 3, 9, -3}};
        values.mValues[5].mBase = 40;
        values.mValues[8].mModifiers = {10, 2, -5};
        const ESM4::PlayerDynamicBaseSettings settings{2, 2, 5};
        MWMechanics::OblivionCombatService service;
        service.publishPlayerValues(player, values, settings);
        service.setPlayerBaseValue(player, 8, 16777217, settings);
        EXPECT_EQ((*service.findActorValues(values.mActor)->mPlayerFormValues)[0], 16777217);
        EXPECT_EQ(service.getPlayerBaseValue(8), 16777296);
        EXPECT_EQ(service.getPlayerIntegerValue(8), 16777303);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 16777304.f);
        service.setPlayerBaseValue(player, 5, 257, settings);
        EXPECT_EQ(service.getPlayerBaseValue(5), 1);
        EXPECT_EQ(service.getPlayerBaseValue(8), 16777220);
        EXPECT_EQ(service.findActorValues(values.mActor)->mValues[8].mModifiers, values.mValues[8].mModifiers);
        service.setPlayerBaseValue(player, 9, -1, settings);
        EXPECT_EQ((*service.findActorValues(values.mActor)->mPlayerFormValues)[1], 65535);
        EXPECT_EQ(service.getPlayerBaseValue(9), 65535);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = values.mActor;
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        service.capture(saved);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()), mStore);
        restored.publishPlayerValues(player, *restored.findActorValues(values.mActor), settings);
        EXPECT_EQ(restored.getPlayerBaseValue(8), 16777220);
        EXPECT_EQ(restored.getPlayerBaseValue(9), 65535);
        const auto before = *service.findActorValues(values.mActor);
        const auto health = ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent();
        EXPECT_THROW(service.setPlayerBaseValue(player, 0, 1,
            (ESM4::PlayerDynamicBaseSettings{2, 2, std::numeric_limits<float>::infinity()})), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(values.mActor), before);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), health);
        auto captured = saved;
        service.capture(captured);
        EXPECT_EQ(captured, saved);
        service.publishPlayerValues(player, values, settings);
        EXPECT_EQ((*service.findActorValues(values.mActor)->mPlayerFormValues)[0], 16777217);
        EXPECT_EQ(service.getPlayerBaseValue(5), 1);
    }

    TEST_F(OblivionActorStatsTest, creatureCommandsAliasRuntimeSkillsAndPersistModifierOwnership)
    {
        sharedStats();
        ESM4::Creature creature{};
        creature.mId = {0x800, 3};
        creature.mFormKey = mActorKey;
        creature.mAttackReach = 64;
        creature.mBaseConfig.tes4.levelOrOffset = 4;
        mStore.getWritable<ESM4::Creature>().insertStatic(creature, mActorKey);
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(mStore, readers);
        environment.setWorldModel(model);
        MWClass::ESM4Creature::registerSelf();
        ESM4::ActorCreature reference{};
        reference.mFormKey = ESM::FormKey::content("actors.esm", 0x900);
        reference.mId = {0x900, 3};
        reference.mBaseKey = mActorKey;
        mStore.getWritable<ESM4::ActorCreature>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Creature> live(reference, mStore.search<ESM4::Creature>(mActorKey));
        const MWWorld::Ptr ptr(&live);
        const std::array residents{ptr};
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[8].mBase = 100;
        values.mValues[10].mBase = 40;
        for (std::size_t i = 12; i <= 32; ++i)
            values.mValues[i].mBase = i <= 18 ? 21 : i <= 25 ? 22 : 23;
        MWMechanics::OblivionCombatService service;
        service.publishNonPlayerValues(ptr, values);
        using Command = ESM4::ActorValueCommand;
        using Source = ESM4::ActorValueCommandSource;
        const auto run = [&](std::uint8_t value, Command command, Source source, std::int32_t amount) {
            return service.executeNonPlayerValueCommand(ptr, value, command, source, amount, {false, false}, residents);
        };
        EXPECT_TRUE(run(28, Command::Mod, Source::Script, 2).mAccepted);
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Marksman), 23);
        EXPECT_EQ(service.findActorValues(values.mActor)->mValues[12].mModifiers[1], 2);
        EXPECT_FALSE(service.findActorValues(values.mActor)->mValues[28].mModifiers[1]);
        EXPECT_TRUE(run(28, Command::Force, Source::Console, 20).mAccepted);
        EXPECT_EQ(service.findActorValues(values.mActor)->mValues[12].mModifiers[2], -3);
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Marksman), 20);
        EXPECT_TRUE(run(28, Command::Set, Source::Console, 30).mAccepted);
        EXPECT_EQ(service.getNonPlayerBaseValue(values.mActor, 28, mStore), 23); // Raw Marksman remains Stealth.
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Marksman), 29);
        EXPECT_FALSE(run(10, Command::Force, Source::Script, 0).mAccepted);
        EXPECT_EQ(service.getNonPlayerValue(ptr, 10), 40);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        ESM4::RuntimeReferenceState savedActor;
        savedActor.mKey = values.mActor;
        savedActor.mBase = values.mBase;
        savedActor.mCell = saved.mPlayer.mCell;
        saved.mReferences.push_back(savedActor);
        service.capture(saved);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()), mStore);
        restored.publishNonPlayerValues(ptr, *restored.findActorValues(values.mActor));
        EXPECT_EQ(restored.getNonPlayerValue(ptr, 28), 29);
        auto resaved = saved;
        restored.capture(resaved);
        EXPECT_EQ(resaved, saved);
    }

    TEST_F(OblivionActorStatsTest, liveCreatureClassUsesCanonicalNativeSkillGroups)
    {
        sharedStats();
        ESM4::Creature creature{};
        creature.mId = {0x800, 3};
        creature.mFormKey = mActorKey;
        creature.mAttackReach = 64;
        creature.mBaseConfig.tes4.levelOrOffset = 4;
        creature.mBaseConfig.tes4.baseSpell = 17;
        creature.mBaseConfig.tes4.fatigue = 18;
        creature.mData.health = 19;
        creature.mData.damage = 20;
        creature.mData.combat = 21;
        creature.mData.magic = 22;
        creature.mData.stealth = 23;
        creature.mData.attribs.intelligence = 24;
        mStore.getWritable<ESM4::Creature>().insertStatic(creature, mActorKey);
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(mStore, readers);
        environment.setWorldModel(model);
        MWClass::ESM4Creature::registerSelf();
        ESM4::ActorCreature reference{};
        reference.mFormKey = ESM::FormKey::content("actors.esm", 0x900);
        reference.mId = {0x900, 3};
        reference.mBaseKey = mActorKey;
        mStore.getWritable<ESM4::ActorCreature>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Creature> live(reference, mStore.search<ESM4::Creature>(mActorKey));
        MWWorld::Ptr ptr(&live);
        auto& stats = ptr.getClass().getCreatureStats(ptr);
        EXPECT_EQ(stats.getLevel(), 4);
        EXPECT_EQ(stats.getAttribute(ESM::Attribute::Intelligence).getBase(), 24);
        EXPECT_EQ(stats.getHealth().getCurrent(), 19);
        EXPECT_EQ(stats.getMagicka().getBase(), 17);
        EXPECT_EQ(stats.getFatigue().getBase(), 18);
        const auto& ids = MWWorld::oblivionSkillIds();
        for (std::size_t i = 0; i < ids.size(); ++i)
            EXPECT_EQ(ptr.getClass().getSkill(ptr, ids[i]), i < 7 || i == 16 ? 21 : i < 14 ? 22 : 23);
        // Base-record Marksman remains Stealth; the live getter aliases Combat.
        EXPECT_EQ((*live.mData.getCustomData()->asESM4CreatureCustomData().getNativeSkills())[16], 23);
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Marksman), 21);
        EXPECT_EQ(live.mData.getCustomData()->asESM4CreatureCustomData().mNativeDamage, 20);
        EXPECT_THROW(ptr.getClass().getSkill(ptr, ESM::Skill::Spear), std::invalid_argument);

        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        for (std::size_t i = 0; i < 8; ++i)
            values.mValues[i].mBase = stats.getAttribute(ESM::Attribute::indexToRefId(i)).getBase();
        for (std::size_t i = 0; i < 21; ++i)
            values.mValues[12 + i].mBase = i < 7 ? 21 : i < 14 ? 22 : 23;
        values.mValues[8].mBase = 19;
        values.mValues[9].mBase = 17;
        values.mValues[10].mBase = 18;
        MWMechanics::OblivionCombatService service;
        service.publishNonPlayerValues(ptr, values);
        service.changeNonPlayerValue(ptr, 28, ESM4::ActorValueModifier::Script, 0.75f);
        service.changeNonPlayerValue(ptr, 22, ESM4::ActorValueModifier::Damage, -1.25f);
        service.changeNonPlayerValue(ptr, 31, ESM4::ActorValueModifier::Maximum, 2.5f);
        for (std::size_t i = 0; i < ids.size(); ++i)
        {
            const float expected = i < 7 || i == 16 ? 21.75f : i < 14 ? 20.75f : 25.5f;
            EXPECT_EQ(ptr.getClass().getSkill(ptr, ids[i]), expected);
            EXPECT_EQ(service.getNonPlayerValue(ptr, static_cast<std::uint8_t>(12 + i)), expected);
        }
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptr, 28), 21);
        const auto& stored = *service.findActorValues(reference.mFormKey);
        EXPECT_EQ(stored.mValues[12].mModifiers[1], 0.75f);
        EXPECT_EQ(stored.mValues[19].mModifiers[2], -1.25f);
        EXPECT_EQ(stored.mValues[26].mModifiers[0], 2.5f);
        EXPECT_FALSE(stored.mValues[28].mModifiers[1]);
        EXPECT_EQ(stored.mValues[28].mBase, 23);
        EXPECT_EQ(mStore.search<ESM4::Creature>(mActorKey)->mData.combat, 21);
        const auto before = stored;
        auto invalid = before;
        invalid.mValues[12].mModifiers[1] = std::numeric_limits<float>::infinity();
        EXPECT_THROW(service.publishNonPlayerValues(ptr, invalid), std::runtime_error);
        EXPECT_EQ(*service.findActorValues(reference.mFormKey), before);
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Marksman), 21.75f);
        auto low = before;
        low.mProcess = ESM4::ActorValueProcess::Low;
        service.publishNonPlayerValues(ptr, low);
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Sneak), 23);
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Marksman), 21.75f);
        service.publishNonPlayerValues(ptr, before);

        ESM4::RuntimeActorLife life;
        life.mActor = values.mActor;
        life.mBase = values.mBase;
        life.mPhase = ESM4::ActorLifePhase::EssentialUnconscious;
        life.mRecoveryRemaining = 2.5f;
        service.publishNonPlayerLife(ptr, life);
        EXPECT_FALSE(stats.isDead());
        EXPECT_TRUE(stats.getKnockedDown());
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        ESM4::RuntimeReferenceState savedActor;
        savedActor.mKey = values.mActor;
        savedActor.mBase = values.mBase;
        savedActor.mCell = saved.mPlayer.mCell;
        saved.mReferences.push_back(savedActor);
        service.capture(saved);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()), mStore);
        EXPECT_EQ(restored.getNonPlayerValue(values.mActor, 28, mStore), 21.75f);
        EXPECT_EQ(restored.getNonPlayerIntegerValue(values.mActor, 28, mStore), 21);
        EXPECT_EQ(restored.getNonPlayerBaseValue(values.mActor, 28, mStore), 23);
        MWWorld::LiveCellRef<ESM4::Creature> newLive(reference, mStore.search<ESM4::Creature>(mActorKey));
        MWWorld::Ptr newPtr(&newLive);
        restored.publishNonPlayerValues(newPtr, *restored.findActorValues(reference.mFormKey));
        EXPECT_FALSE(newPtr.getClass().getCreatureStats(newPtr).isDead());
        EXPECT_TRUE(newPtr.getClass().getCreatureStats(newPtr).getKnockedDown());
        EXPECT_EQ(restored.findActorLife(values.mActor)->mRecoveryRemaining, 2.5f);
        life.mPhase = ESM4::ActorLifePhase::Alive;
        life.mRecoveryRemaining = 0;
        restored.publishNonPlayerLife(newPtr, life);
        EXPECT_FALSE(newPtr.getClass().getCreatureStats(newPtr).getKnockedDown());
        for (auto id : ids)
            EXPECT_EQ(newPtr.getClass().getSkill(newPtr, id), ptr.getClass().getSkill(ptr, id));
        EXPECT_EQ(newLive.mData.getCustomData()->asESM4CreatureCustomData().mNativeDamage, 20);
        restored.changeNonPlayerValue(newPtr, 10, ESM4::ActorValueModifier::Damage, -2.5f);
        restored.regenerateNonPlayerFatigue(newPtr, .125f, {10, 0});
        EXPECT_EQ(newPtr.getClass().getCreatureStats(newPtr).getFatigue().getCurrent(), 16.75f);
        EXPECT_EQ(restored.findActorValues(values.mActor)->mValues[10].mModifiers[2], -1.25f);
        restored.regenerateNonPlayerFatigue(newPtr, 1.f, {10, 0});
        EXPECT_EQ(newPtr.getClass().getCreatureStats(newPtr).getFatigue().getCurrent(), 18);
        EXPECT_FALSE(restored.findActorValues(values.mActor)->mValues[10].mModifiers[2]);
        for (std::uint8_t av : {8, 9, 10})
            restored.changeNonPlayerValue(newPtr, av, ESM4::ActorValueModifier::Damage, -2);
        restored.restoreNonPlayerResources(newPtr, {1, true, false},
            {{1, 0}, {2, 0}, MWWorld::resolveOblivionPlayerDynamicBaseSettings(mStore)});
        const auto& restoredStats = newPtr.getClass().getCreatureStats(newPtr);
        EXPECT_EQ(restoredStats.getHealth().getCurrent(), 19);
        EXPECT_FLOAT_EQ(restoredStats.getMagicka().getCurrent(), 15.17f);
        EXPECT_EQ(restoredStats.getFatigue().getCurrent(), 18);
        EXPECT_EQ(newPtr.getClass().getSkill(newPtr, ESM::Skill::Marksman), 21.75f);
        const std::array residents{newPtr};
        restored.setNonPlayerBaseValue(newPtr, 28, 257, residents);
        EXPECT_EQ(restored.getNonPlayerValue(newPtr, 28), 1.75f);
        EXPECT_EQ(restored.getNonPlayerBaseValue(values.mActor, 28, mStore), 23);
        for (std::size_t i = 12; i <= 18; ++i)
            EXPECT_EQ(restored.findActorValues(values.mActor)->mValues[i].mBase, 1);
        restored.setNonPlayerBaseValue(newPtr, 31, -1, residents);
        EXPECT_EQ(restored.getNonPlayerBaseValue(values.mActor, 28, mStore), 255);
        EXPECT_EQ(restored.getNonPlayerValue(newPtr, 28), 1.75f);
        EXPECT_EQ(newPtr.getClass().getSkill(newPtr, ESM::Skill::Sneak), 257.5f);
        restored.setNonPlayerBaseValue(newPtr, 22, 258, residents);
        EXPECT_EQ(newPtr.getClass().getSkill(newPtr, ESM::Skill::Conjuration), .75f);
        restored.capture(saved);
        MWMechanics::OblivionCombatService reloaded;
        reloaded.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()), mStore);
        EXPECT_EQ(reloaded.getNonPlayerValue(values.mActor, 28, mStore), 1.75f);
        EXPECT_EQ(reloaded.getNonPlayerBaseValue(values.mActor, 28, mStore), 255);
        EXPECT_EQ(reloaded.getNonPlayerValue(values.mActor, 22, mStore), .75f);
    }

    TEST_F(OblivionActorStatsTest, nativeInitializationValidatesBeforeChangingSharedStats)
    {
        sharedStats();
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        MWMechanics::CreatureStats stats;
        const std::array<std::uint8_t, 8> attributes{1, 2, 3, 4, 5, 6, 7, 8};
        EXPECT_THROW(stats.initializeOblivionBaseStats(attributes, {1, -1, 2}, 1), std::invalid_argument);
        EXPECT_THROW(stats.initializeOblivionBaseStats(attributes, {1, 2, 3}, 0), std::invalid_argument);
        EXPECT_EQ(stats.getLevel(), 0);
        EXPECT_EQ(stats.getAttribute(ESM::Attribute::Strength).getBase(), 0);
        stats.initializeOblivionBaseStats(attributes, {1, 2, 3}, 1);
        EXPECT_EQ(stats.getAttribute(ESM::Attribute::Intelligence).getBase(), 2);
        EXPECT_EQ(stats.getHealth().getCurrent(), 1);
        EXPECT_EQ(stats.getMagicka().getCurrent(), 2);
        EXPECT_EQ(stats.getFatigue().getCurrent(), 3);
    }

    TEST_F(OblivionActorStatsTest, preparedNativeViewsCommitTogetherAndCannotReplay)
    {
        sharedStats();
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        MWMechanics::NpcStats stats;
        stats.initializeOblivionBaseStats({1, 2, 3, 4, 5, 6, 7, 8}, {100, 50, 42}, 1);
        stats.getSkill(ESM::Skill::Athletics).setProgress(0.375f);
        MWMechanics::OblivionActorProjectionInput input;
        input.mAttributes.fill({40, {10, 5, -2}});
        input.mSkills.fill({30, {2, 1, -4}});
        input.mDynamic = {{{100, 120, -5}, {50, 75, 25}, {42, 60, -10}}};
        const auto* strengthView = &stats.getAttribute(ESM::Attribute::Strength);
        const auto* athleticsView = &stats.getSkill(ESM::Skill::Athletics);
        MWMechanics::OblivionActorProjection prepared(stats, input);
        EXPECT_EQ(stats.getAttribute(ESM::Attribute::Strength).getModified(), 1);
        EXPECT_EQ(stats.getHealth().getCurrent(), 100);
        EXPECT_TRUE(prepared.commit());
        for (int i = 0; i < 8; ++i)
            EXPECT_EQ(stats.getAttribute(ESM::Attribute::indexToRefId(i)).getModified(), 53);
        for (auto id : MWWorld::oblivionSkillIds())
            EXPECT_EQ(stats.getSkill(id).getModified(), 29);
        EXPECT_EQ(stats.getSkill(ESM::Skill::Athletics).getProgress(), 0.375f);
        EXPECT_EQ(strengthView, &stats.getAttribute(ESM::Attribute::Strength));
        EXPECT_EQ(athleticsView, &stats.getSkill(ESM::Skill::Athletics));
        EXPECT_EQ(strengthView->getModified(), 53);
        EXPECT_EQ(athleticsView->getModified(), 29);
        EXPECT_EQ(stats.getHealth().getCurrent(), -5);
        EXPECT_EQ(stats.getMagicka().getModified(), 75);
        EXPECT_EQ(stats.getFatigue().getCurrent(), -10);
        EXPECT_FALSE(stats.isDead()); // Death is a separate native transition.
        EXPECT_EQ(stats.getLevel(), 1);
        EXPECT_FALSE(prepared.commit());
        EXPECT_EQ(stats.getHealth().getCurrent(), -5);
        EXPECT_THROW(stats.setHealth(MWMechanics::DynamicStat<float>(200)), std::logic_error);
        EXPECT_THROW(stats.getSkill(ESM::Skill::Athletics).setBase(200), std::logic_error);
        input.mProcess = ESM4::ActorValueProcess::Low;
        input.mDynamic[0][2] = 90;
        MWMechanics::OblivionActorProjection refresh(stats, input);
        EXPECT_TRUE(refresh.commit());
        EXPECT_EQ(stats.getAttribute(ESM::Attribute::Strength).getModified(), 43);
        EXPECT_EQ(stats.getSkill(ESM::Skill::Athletics).getModified(), 27);
        EXPECT_EQ(stats.getHealth().getCurrent(), 90);
        EXPECT_FALSE(prepared.commit()); // Cannot roll back a later transaction.
        EXPECT_EQ(stats.getHealth().getCurrent(), 90);
    }

    TEST_F(OblivionActorStatsTest, nativeLifeProjectionPreservesHealthAndControlsSharedLifecycle)
    {
        sharedStats();
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        ESM::NPC base{};
        base.blank();
        base.mId = ESM::RefId::stringRefId("lifecycle-test");
        mStore.insertStatic(base);
        MWMechanics::CreatureStats stats;
        stats.getSpells().setSpells(base.mId);
        MWMechanics::OblivionActorProjectionInput input;
        input.mDynamic = {{{100, 100, -5}, {50, 50, 50}, {40, 40, 40}}};
        input.mLife = ESM4::ActorLifePhase::Alive;
        ASSERT_TRUE(MWMechanics::OblivionActorProjection(stats, input).commit());
        EXPECT_FALSE(stats.isDead());
        EXPECT_EQ(stats.getHealth().getCurrent(), -5);
        EXPECT_THROW(stats.resurrect(), std::logic_error);
        stats.setKnockedDown(true);
        ASSERT_TRUE(MWMechanics::OblivionActorProjection(stats, input).commit());
        EXPECT_TRUE(stats.getKnockedDown());
        input.mLife = ESM4::ActorLifePhase::EssentialUnconscious;
        ASSERT_TRUE(MWMechanics::OblivionActorProjection(stats, input).commit());
        stats.setKnockedDown(false);
        EXPECT_TRUE(stats.getKnockedDown());
        EXPECT_FALSE(stats.isDead());
        ESM::CreatureStats essentialSave{};
        stats.writeState(essentialSave);
        EXPECT_FALSE(essentialSave.mDead);
        EXPECT_TRUE(essentialSave.mKnockdown);
        stats.setKnockedDownOneFrame(true);
        stats.setKnockedDownOverOneFrame(true);
        input.mLife = ESM4::ActorLifePhase::Alive;
        ASSERT_TRUE(MWMechanics::OblivionActorProjection(stats, input).commit());
        EXPECT_FALSE(stats.getKnockedDown());
        EXPECT_FALSE(stats.getKnockedDownOneFrame());
        EXPECT_FALSE(stats.getKnockedDownOverOneFrame());
        stats.setDeathAnimationFinished(true);
        input.mLife = ESM4::ActorLifePhase::Dead;
        ASSERT_TRUE(MWMechanics::OblivionActorProjection(stats, input).commit());
        EXPECT_TRUE(stats.isDead());
        EXPECT_FALSE(stats.isDeathAnimationFinished());
        ESM::CreatureStats deadSave{};
        stats.writeState(deadSave);
        EXPECT_TRUE(deadSave.mDead);
        EXPECT_FALSE(deadSave.mKnockdown);
        EXPECT_EQ(stats.getHealth().getCurrent(), -5);
        stats.setDeathAnimationFinished(true);
        ASSERT_TRUE(MWMechanics::OblivionActorProjection(stats, input).commit());
        EXPECT_TRUE(stats.isDeathAnimationFinished());
        input.mLife = static_cast<ESM4::ActorLifePhase>(255);
        EXPECT_THROW((MWMechanics::OblivionActorProjection(stats, input)), std::invalid_argument);
        EXPECT_TRUE(stats.isDead());
        EXPECT_TRUE(stats.isDeathAnimationFinished());
        input.mLife.reset();
        ASSERT_TRUE(MWMechanics::OblivionActorProjection(stats, input).commit());
        EXPECT_TRUE(stats.isDead());
        input.mLife = ESM4::ActorLifePhase::Alive;
        ASSERT_TRUE(MWMechanics::OblivionActorProjection(stats, input).commit());
        EXPECT_FALSE(stats.isDead());
        EXPECT_FALSE(stats.isDeathAnimationFinished());
        MWMechanics::CreatureStats legacy;
        legacy.getSpells().setSpells(base.mId);
        ESM::CreatureStats legacySave{};
        legacySave.blank();
        legacySave.mDead = true;
        legacy.readState(legacySave);
        EXPECT_TRUE(legacy.isDead());
        EXPECT_NO_THROW(legacy.resurrect());
        EXPECT_FALSE(legacy.isDead());
    }

    TEST_F(OblivionActorStatsTest, failedOrAbandonedPreparationLeavesAllNativeViewsIntact)
    {
        sharedStats();
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        MWMechanics::NpcStats stats;
        MWMechanics::OblivionActorProjectionInput input;
        input.mAttributes.fill({40, {}});
        input.mSkills.fill({30, {}});
        input.mDynamic = {{{100, 120, 90}, {50, 75, 25}, {42, 60, -10}}};
        MWMechanics::OblivionActorProjection initial(stats, input);
        ASSERT_TRUE(initial.commit());
        const auto attributes = stats.getAttributes();
        const auto skills = stats.getSkills();
        const auto health = stats.getHealth();
        const auto magicka = stats.getMagicka();
        const auto fatigue = stats.getFatigue();
        input.mAttributes[0].mBase = 200;
        {
            MWMechanics::OblivionActorProjection abandoned(stats, input);
        }
        input.mSkills.back().mModifiers[2] = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW((MWMechanics::OblivionActorProjection(stats, input)), std::invalid_argument);
        input.mSkills.back().mModifiers[2].reset();
        input.mDynamic.back()[2] = std::numeric_limits<float>::infinity();
        EXPECT_THROW((MWMechanics::OblivionActorProjection(stats, input)), std::invalid_argument);
        EXPECT_EQ(stats.getAttributes(), attributes);
        EXPECT_EQ(stats.getSkills(), skills);
        EXPECT_EQ(stats.getHealth(), health);
        EXPECT_EQ(stats.getMagicka(), magicka);
        EXPECT_EQ(stats.getFatigue(), fatigue);
    }

    TEST_F(OblivionActorStatsTest, creatureProjectionNeedsNoNpcSkillStorage)
    {
        sharedStats();
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        MWMechanics::CreatureStats stats;
        MWMechanics::OblivionActorProjectionInput input;
        input.mAttributes.fill({12, {1, 2, -3}});
        input.mDynamic = {{{10, 20, 9}, {15, 18, 12}, {30, 40, -5}}};
        MWMechanics::OblivionActorProjection prepared(stats, input);
        ASSERT_TRUE(prepared.commit());
        EXPECT_EQ(stats.getAttribute(ESM::Attribute::Strength).getModified(), 12);
        EXPECT_EQ(stats.getHealth().getCurrent(), 9);
        EXPECT_EQ(stats.getFatigue().getCurrent(), -5);
        EXPECT_FALSE(stats.isDead());
    }

    TEST_F(OblivionActorStatsTest, nativeViewsCannotEnterLegacyActorSetters)
    {
        sharedStats();
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        MWMechanics::NpcStats stats;
        stats.initializeOblivionBaseStats({1, 2, 3, 4, 5, 6, 7, 8}, {100, 50, 42}, 1);
        MWMechanics::AttributeValue attribute;
        attribute.setNativeProjection({90, {}}, ESM4::ActorValueOwner::NonPlayer,
            ESM4::ActorValueProcess::Active);
        EXPECT_THROW(stats.setAttribute(ESM::Attribute::Intelligence, attribute), std::logic_error);
        EXPECT_EQ(stats.getAttribute(ESM::Attribute::Intelligence).getBase(), 2);
        EXPECT_EQ(stats.getMagicka().getCurrent(), 50);
        EXPECT_EQ(stats.getFatigue().getCurrent(), 42);
        MWMechanics::DynamicStat<float> health;
        health.setNativeProjection(100, 100, -1);
        EXPECT_THROW(stats.setHealth(health), std::logic_error);
        EXPECT_EQ(stats.getHealth().getCurrent(), 100);
        EXPECT_FALSE(stats.isDead());
        MWMechanics::SkillValue skill;
        skill.setNativeProjection({90, {}}, ESM4::ActorValueOwner::NonPlayer,
            ESM4::ActorValueProcess::Active);
        EXPECT_THROW(stats.setSkill(ESM::Skill::Athletics, skill), std::logic_error);
        EXPECT_EQ(stats.getSkill(ESM::Skill::Athletics).getBase(), 0);
        // The explicit native projection entry point can establish a view;
        // ordinary assignment through the historical mutable reference cannot
        // subsequently replace it with an independently writable value.
        auto& liveSkill = stats.getSkill(ESM::Skill::Athletics);
        liveSkill.setNativeProjection({90, {}}, ESM4::ActorValueOwner::NonPlayer,
            ESM4::ActorValueProcess::Active);
        EXPECT_THROW(liveSkill = MWMechanics::SkillValue{}, std::logic_error);
        EXPECT_THROW(stats.setSkill(ESM::Skill::Athletics, MWMechanics::SkillValue{}), std::logic_error);
        EXPECT_EQ(stats.getSkill(ESM::Skill::Athletics).getModified(), 90);
    }
}

TEST(OblivionStatProjection, NativeCurrentPreservesRoundingAndNegativeValues)
{
    using Owner = ESM4::ActorValueOwner;
    using Process = ESM4::ActorValueProcess;
    MWMechanics::AttributeValue npc;
    MWMechanics::AttributeValue player;
    const ESM4::ActorValueState state{1, {-1, 0x1p-24f, 0}};
    npc.setNativeProjection(state, Owner::NonPlayer, Process::Active);
    player.setNativeProjection(state, Owner::Player, Process::Active);
    EXPECT_EQ(npc.getModified(), 0);
    EXPECT_EQ(player.getModified(), 0x1p-24f);
    EXPECT_NE(npc, player);
    npc.setNativeProjection({1, {0, 0, -10}}, Owner::NonPlayer, Process::Active);
    EXPECT_EQ(npc.getModified(), -9);
    EXPECT_EQ(npc.getBase(), 1);
    EXPECT_EQ(npc.getDamage(), 10);
    const auto before = npc;
    EXPECT_THROW(npc.setNativeProjection({std::numeric_limits<float>::infinity(), {}},
        Owner::NonPlayer, Process::Active), std::invalid_argument);
    EXPECT_EQ(npc, before);
}

TEST(OblivionStatProjection, LegacyMutationsCannotOverwriteNativeViews)
{
    MWMechanics::SkillValue native;
    native.setNativeProjection({40, {10, 5, -2}}, ESM4::ActorValueOwner::NonPlayer,
        ESM4::ActorValueProcess::Active);
    const auto before = native;
    EXPECT_THROW(native.setBase(100), std::logic_error);
    EXPECT_THROW(native.setModifier(20), std::logic_error);
    EXPECT_THROW(native.damage(10), std::logic_error);
    EXPECT_THROW(native.restore(10), std::logic_error);
    ESM::StatState<float> serialized{};
    EXPECT_THROW(native.readState(serialized), std::logic_error);
    EXPECT_EQ(native, before);
    EXPECT_EQ(native.getModified(), 53);
    auto differentScript = native;
    differentScript.setNativeProjection({40, {10, 6, -2}}, ESM4::ActorValueOwner::NonPlayer,
        ESM4::ActorValueProcess::Active);
    EXPECT_NE(native, differentScript);
    MWMechanics::AttributeValue legacy;
    legacy.setBase(40);
    legacy.setModifier(-10);
    EXPECT_FALSE(legacy.isNativeProjection());
    EXPECT_EQ(legacy.getModified(), 30);
    legacy.restore(5);
    EXPECT_EQ(legacy.getModified(), 35);
    legacy.damage(100);
    EXPECT_EQ(legacy.getModified(), 0);
}

TEST(OblivionStatProjection, DynamicViewsPreserveExactMaximumAndCurrent)
{
    MWMechanics::DynamicStat<float> view;
    // Computing a modifier and adding it back would round this maximum away.
    view.setNativeProjection(1.f, 0x1p-25f, -3.f);
    EXPECT_EQ(view.getBase(), 1.f);
    EXPECT_EQ(view.getModified(), 0x1p-25f);
    EXPECT_EQ(view.getModified(false), 0x1p-25f);
    EXPECT_EQ(view.getCurrent(), -3.f);
    EXPECT_EQ(view.getRatio(), -0x1.8p26f);
    auto copy = view;
    EXPECT_EQ(copy, view);
    copy.setNativeProjection(1.f, 0x1p-26f, -3.f);
    EXPECT_NE(copy, view);
    view.setNativeProjection(1.f, -2.f, 10.f);
    EXPECT_EQ(view.getModified(), -2.f);
    EXPECT_EQ(view.getCurrent(), 10.f);
    view.setNativeProjection(1.f, 0.f, -3.f);
    EXPECT_EQ(view.getRatio(), 0.f);
    EXPECT_EQ(view.getRatio(false), 1.f);
}

TEST(OblivionStatProjection, DynamicViewsRejectInvalidProjectionAndLegacyWrites)
{
    MWMechanics::DynamicStat<float> view;
    view.setNativeProjection(10.f, 12.f, -1.f);
    const auto before = view;
    EXPECT_THROW(view.setBase(20), std::logic_error);
    EXPECT_THROW(view.setModifier(20), std::logic_error);
    EXPECT_THROW(view.setCurrent(20, true, true), std::logic_error);
    ESM::StatState<float> state{};
    EXPECT_THROW(view.readState(state), std::logic_error);
    const float invalid = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(view.setNativeProjection(invalid, 12, -1), std::invalid_argument);
    EXPECT_THROW(view.setNativeProjection(10, invalid, -1), std::invalid_argument);
    EXPECT_THROW(view.setNativeProjection(10, 12, invalid), std::invalid_argument);
    EXPECT_THROW(view.setNativeProjection(-std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(), 0), std::invalid_argument);
    EXPECT_EQ(view, before);
    MWMechanics::DynamicStat<int> integer;
    EXPECT_THROW(integer.setNativeProjection(std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max(), 0), std::invalid_argument);
    EXPECT_FALSE(integer.isNativeProjection());
}

TEST(OblivionStatProjection, DynamicLegacyClampsAndSerializationRemainUnchanged)
{
    MWMechanics::DynamicStat<float> legacy(10);
    EXPECT_FALSE(legacy.isNativeProjection());
    legacy.setCurrent(20);
    EXPECT_EQ(legacy.getCurrent(), 10);
    legacy.setCurrent(-5);
    EXPECT_EQ(legacy.getCurrent(), 0);
    legacy.setCurrent(-5, true);
    EXPECT_EQ(legacy.getCurrent(), -5);
    legacy.setModifier(-20);
    EXPECT_EQ(legacy.getModified(), 0);
    EXPECT_EQ(legacy.getModified(false), -10);
    ESM::StatState<float> state{};
    legacy.writeState(state);
    MWMechanics::DynamicStat<float> restored;
    restored.readState(state);
    EXPECT_EQ(restored, legacy);
    EXPECT_FALSE(restored.isNativeProjection());
}

TEST(OblivionStatProjection, WholeValueAssignmentCannotReplaceNativeViews)
{
    MWMechanics::AttributeValue attribute;
    attribute.setNativeProjection({40, {10, 5, -2}}, ESM4::ActorValueOwner::Player,
        ESM4::ActorValueProcess::Active);
    const auto beforeAttribute = attribute;
    MWMechanics::AttributeValue replacement;
    replacement.setBase(200);
    EXPECT_THROW(attribute = replacement, std::logic_error);
    EXPECT_THROW(attribute = std::move(replacement), std::logic_error);
    EXPECT_EQ(attribute, beforeAttribute);
    auto& sameAttribute = attribute;
    EXPECT_NO_THROW(attribute = sameAttribute);
    replacement = beforeAttribute; // Copying a view into a new value preserves its mode.
    EXPECT_TRUE(replacement.isNativeProjection());
    EXPECT_THROW(replacement.setBase(30), std::logic_error);

    MWMechanics::DynamicStat<float> dynamic;
    dynamic.setNativeProjection(100, 120, -5);
    const auto beforeDynamic = dynamic;
    MWMechanics::DynamicStat<float> other(200);
    EXPECT_THROW(dynamic = other, std::logic_error);
    EXPECT_THROW(dynamic = std::move(other), std::logic_error);
    EXPECT_EQ(dynamic, beforeDynamic);
    auto& sameDynamic = dynamic;
    EXPECT_NO_THROW(dynamic = sameDynamic);
    other = beforeDynamic;
    EXPECT_TRUE(other.isNativeProjection());
    EXPECT_THROW(other.setCurrent(10), std::logic_error);
    MWMechanics::DynamicStat<float> legacy(20);
    legacy = MWMechanics::DynamicStat<float>(30);
    EXPECT_EQ(legacy.getCurrent(), 30);
    EXPECT_FALSE(legacy.isNativeProjection());
}
