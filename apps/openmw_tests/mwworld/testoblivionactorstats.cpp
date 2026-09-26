#include <gtest/gtest.h>

#include <components/esm/records.hpp>
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
    }

    TEST_F(OblivionActorStatsTest, liveCreatureClassUsesCanonicalNativeSkillGroups)
    {
        sharedStats();
        ESM4::Creature creature{};
        creature.mId = {0x800, 3};
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
        ESM4::ActorCharacter reference{};
        reference.mFormKey = ESM::FormKey::content("actors.esm", 0x900);
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
            EXPECT_EQ(ptr.getClass().getSkill(ptr, ids[i]), i < 7 ? 21 : i < 14 ? 22 : 23);
        EXPECT_EQ(live.mData.getCustomData()->asESM4CreatureCustomData().mNativeDamage, 20);
        EXPECT_THROW(ptr.getClass().getSkill(ptr, ESM::Skill::Spear), std::invalid_argument);
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

}
