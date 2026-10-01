#include <gtest/gtest.h>
#include <limits>
#include <utility>

#include <components/esm/records.hpp>
#include "apps/openmw/mwmechanics/oblivioncombat.hpp"
#include "apps/openmw/mwmechanics/spellcasting.hpp"
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
#include "apps/openmw/mwlua/context.hpp"
#include "apps/openmw/mwlua/object.hpp"
#include "apps/openmw/mwlua/stats.hpp"
#include "apps/openmw/mwlua/luamanagerimp.hpp"
#include "apps/openmw/mwlua/localscripts.hpp"
#include <components/vfs/manager.hpp>

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

    TEST_F(OblivionActorStatsTest, freshPlayerUsesWinningRawFormAndDenseConstructorStorage)
    {
        sharedStats();
        const auto key = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc native{};
        native.mId = {7, 4}; // Resolved load-order index is not persistent identity.
        native.mFormKey = key;
        native.mIsTES4 = true;
        native.mEditorId = "Player";
        native.mData.attribs = {50, 50, 30, 30, 40, 40, 50, 50};
        native.mData.skills = {5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25};
        native.mData.health = 45;
        native.mBaseConfig.tes4.baseSpell = 0;
        native.mBaseConfig.tes4.fatigue = 150;
        native.mAIData = {1, 2, 3, 4, 0, 0, 0, 0};
        mStore.getWritable<ESM4::Npc>().insertStatic(native, key);
        const auto raw = MWWorld::resolveOblivionInitialPlayerValues(mStore);
        EXPECT_EQ(raw.mActor, ESM::FormKey::dynamic("player", 1));
        EXPECT_EQ(raw.mBase, ESM::FormKey::dynamic("player-base", 1));
        EXPECT_EQ(raw.mOwner, ESM4::ActorValueOwner::Player);
        EXPECT_EQ(raw.mProcess, ESM4::ActorValueProcess::Active);
        EXPECT_EQ(raw.mPlayerFormValues, (std::optional<std::array<std::int32_t, 4>>{{45, 0, 150, 0}}));
        EXPECT_FALSE(raw.mNonPlayerFormHealth);
        for (std::size_t av = 0; av < 72; ++av)
        {
            EXPECT_EQ(raw.mValues[av].mModifiers, (ESM4::ActorValueModifiers{0.f, 0.f, 0.f}));
            if (av >= 12 && av <= 32)
            {
                EXPECT_EQ(raw.mValues[av].mBase, av - 7);
            }
            if (av >= 33 && av <= 36)
            {
                EXPECT_EQ(raw.mValues[av].mBase, av - 32);
            }
            if (av >= 37)
            {
                EXPECT_EQ(raw.mValues[av].mBase, 0);
            }
        }
        ESM::NPC facade{};
        facade.blank();
        facade.mId = ESM::RefId::stringRefId("Player");
        facade.mNpdt.mHealth = 999; // Shared telemetry cannot supply raw forms.
        const auto* record = mStore.insertStatic(facade);
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
        MWMechanics::OblivionCombatService service;
        ESM4::GameSetting magickaSetting{};
        magickaSetting.mId = {0x9e62f, 4};
        magickaSetting.mEditorId = "fPCBaseMagickaMult";
        magickaSetting.mData = 1.f;
        mStore.getWritable<ESM4::GameSetting>().insertStatic(magickaSetting,
            ESM::FormKey::content("Oblivion.esm", 0x9e62f));
        const auto settings = MWWorld::resolveOblivionPlayerDynamicBaseSettings(mStore);
        EXPECT_EQ(settings.mMagickaMultiplier, 1.f);
        service.publishPlayerValues(player, raw, settings);
        EXPECT_EQ(service.getPlayerBaseValue(8), 125); // Form45 + current Endurance40 *2.
        EXPECT_EQ(service.getPlayerBaseValue(9), 100);
        EXPECT_EQ(service.getPlayerBaseValue(10), 300); // Form150 + four current attributes.
        EXPECT_EQ(service.getPlayerBaseValue(11), 250);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 125);
        EXPECT_EQ(ptr.getClass().getNpcStats(ptr).getSkill(ESM::Skill::Marksman).getBase(), 21);
        const std::uint32_t authoredHealth = mStore.search<ESM4::Npc>(key)->mData.health;
        EXPECT_EQ(authoredHealth, 45);
        EXPECT_EQ(raw.mValues[8].mBase, 45); // Publication leaves detached raw inputs unchanged.
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = raw.mActor;
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        service.capture(saved);
        const auto restarted = ESM4::RuntimeState::deserializeBinary(saved.serializeBinary());
        EXPECT_EQ(restarted.mNativeActorValues, saved.mNativeActorValues);
    }

    TEST_F(OblivionActorStatsTest, freshPlayerRejectsForeignFormsAndPreservesWinningRawPrecision)
    {
        EXPECT_THROW(MWWorld::resolveOblivionInitialPlayerValues(mStore), std::invalid_argument);
        const auto key = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc native{};
        native.mId = {7, 5};
        native.mFormKey = key;
        native.mIsTES4 = true;
        native.mEditorId = "RenamedPlayer"; // Editor names do not replace stable identity.
        native.mData.health = 16777217;
        native.mBaseConfig.tes4.baseSpell = 65535;
        native.mBaseConfig.tes4.fatigue = 65535;
        mStore.getWritable<ESM4::Npc>().insertStatic(native, key);
        auto values = MWWorld::resolveOblivionInitialPlayerValues(mStore);
        EXPECT_EQ((*values.mPlayerFormValues)[0], 16777217);
        EXPECT_EQ(values.mValues[8].mBase, 16777216.f);
        EXPECT_EQ((*values.mPlayerFormValues)[1], 65535);
        EXPECT_EQ((*values.mPlayerFormValues)[2], 65535);
        // A later winning override must be read anew, not cached by EditorID.
        native.mData.health = 0xffffffff;
        native.mData.attribs.endurance = 255;
        mStore.getWritable<ESM4::Npc>().insertStatic(native, key);
        values = MWWorld::resolveOblivionInitialPlayerValues(mStore);
        EXPECT_EQ((*values.mPlayerFormValues)[0], -1);
        EXPECT_EQ(values.mValues[5].mBase, 255);
        for (const auto flags : {ESM4::Npc::TES4_AutoCalcStats, ESM4::Npc::TES4_PCLevelOffset})
        {
            native.mBaseConfig.tes4.flags = flags;
            mStore.getWritable<ESM4::Npc>().insertStatic(native, key);
            EXPECT_THROW(MWWorld::resolveOblivionInitialPlayerValues(mStore), std::invalid_argument);
        }
        native.mBaseConfig.tes4.flags = 0;
        native.mIsTES4 = false;
        mStore.getWritable<ESM4::Npc>().insertStatic(native, key);
        EXPECT_THROW(MWWorld::resolveOblivionInitialPlayerValues(mStore), std::invalid_argument);
        native.mIsTES4 = true;
        native.mFormKey = ESM::FormKey::content("foreign.esm", 7);
        mStore.getWritable<ESM4::Npc>().insertStatic(native, key);
        EXPECT_THROW(MWWorld::resolveOblivionInitialPlayerValues(mStore), std::invalid_argument);
    }

    TEST_F(OblivionActorStatsTest, freshPlayerCharacterChoiceStagesConstructorAndPassivesBeforePublication)
    {
        sharedStats();
        const auto actor = ESM::FormKey::dynamic("player", 1);
        const auto baseKey = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc native{}; native.mId = {7, 4}; native.mFormKey = baseKey; native.mIsTES4 = true;
        native.mData.attribs = {50, 50, 30, 30, 40, 40, 50, 50};
        native.mData.health = 45;
        native.mBaseConfig.tes4.baseSpell = 350;
        native.mBaseConfig.tes4.fatigue = 150;
        mStore.getWritable<ESM4::Npc>().insertStatic(native, baseKey);
        ESM::NPC facade{}; facade.blank(); facade.mId = ESM::RefId::stringRefId("Player");
        const auto* record = mStore.insertStatic(facade);
        MWBase::Environment environment; environment.setESMStore(mStore);
        ESM::ReadersCache readers; MWWorld::WorldModel model(mStore, readers); environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        MWWorld::Player player(record); ESM::NpcState initial{}; initial.blank();
        const auto ptr = player.getPlayer(); ptr.getClass().readAdditionalState(ptr, initial);
        const auto oldHealth = ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent();
        MWMechanics::OblivionCombatService service;
        const auto action = service.allocateAction();
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = actor;
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        const auto snapshot = [&] { service.capture(saved); return saved.serializeBinary(); };
        const auto before = snapshot();
        ESM4::ActorCharacterBaseStats character{}; character.mAttributes.fill(40); character.mSkills.fill(5);
        const auto spell = ESM::FormKey::content("abilities.esp", 0x123);
        std::array abilities{ESM4::PassiveAbilityInput{spell, {
            {7, ESM::fourCC("FOAT"), 0x100072, {5, 10, 0}},
            {2, ESM::fourCC("FOSP"), 0x1000072, {9, 150, 0}}}}};
        const auto settings = MWWorld::resolveOblivionPlayerDynamicBaseSettings(mStore);
        auto invalid = abilities;
        invalid[0].mEffects.back().mValues.mMagnitude = std::numeric_limits<float>::infinity();
        EXPECT_THROW(service.initializePlayerCharacter(player, mStore, character, invalid, {}, settings), std::runtime_error);
        EXPECT_EQ(snapshot(), before);
        EXPECT_EQ(service.findActorValues(actor), nullptr);
        EXPECT_EQ(service.findActorLife(actor), nullptr);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), oldHealth);
        auto badSettings = settings; badSettings.mHealthMultiplier = std::numeric_limits<float>::infinity();
        EXPECT_THROW(service.initializePlayerCharacter(player, mStore, character, abilities, {}, badSettings), std::invalid_argument);
        EXPECT_EQ(snapshot(), before);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), oldHealth);
        service.initializePlayerCharacter(player, mStore, character, abilities, {}, settings);
        ASSERT_NE(service.findActorValues(actor), nullptr);
        ASSERT_NE(service.findActorLife(actor), nullptr);
        EXPECT_EQ(service.findActorLife(actor)->mPhase, ESM4::ActorLifePhase::Alive);
        EXPECT_EQ(service.findActorValues(actor)->mPlayerFormValues,
            (std::optional<std::array<std::int32_t, 4>>{{0, 150, 0, 0}}));
        EXPECT_EQ(service.getPlayerValue(5), 50);
        EXPECT_EQ(service.getPlayerValue(8), 100);
        EXPECT_EQ(service.getPlayerValue(9), 210);
        EXPECT_EQ(service.getPlayerValue(10), 170);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 100);
        EXPECT_FALSE(service.takeNextDeathEvent());
        EXPECT_EQ(service.allocateAction(), action + 1);
        service.changePlayerValue(player, 8, ESM4::ActorValueModifier::Damage, -7, settings);
        const auto accepted = snapshot();
        std::array order{MWMechanics::OblivionPassiveEffectIdentity{spell, 7},
            MWMechanics::OblivionPassiveEffectIdentity{spell, 2}};
        EXPECT_THROW(service.initializePlayerCharacter(player, mStore, character, invalid, order, settings), std::runtime_error);
        EXPECT_EQ(snapshot(), accepted);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 93);
        native.mIsTES4 = false;
        mStore.getWritable<ESM4::Npc>().insertStatic(native, baseKey);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        MWWorld::Player fresh(record);
        const auto freshPtr = fresh.getPlayer(); freshPtr.getClass().readAdditionalState(freshPtr, initial);
        restored.initializePlayerCharacter(fresh, mStore, character, abilities, order, settings);
        EXPECT_EQ(*restored.findActorValues(actor), *service.findActorValues(actor));
        EXPECT_EQ(*restored.findActorLife(actor), *service.findActorLife(actor));
        EXPECT_EQ(freshPtr.getClass().getCreatureStats(freshPtr).getHealth().getCurrent(), 93);
        EXPECT_FALSE(restored.takeNextDeathEvent());
    }

    TEST_F(OblivionActorStatsTest, playerConstructorPublishesLifeAndPrefersRestoredAuthorityWithoutEvents)
    {
        sharedStats();
        const auto baseKey = ESM::FormKey::content("Oblivion.esm", 7);
        ESM4::Npc native{};
        native.mId = {7, 4};
        native.mFormKey = baseKey;
        native.mIsTES4 = true;
        native.mData.attribs = {50, 50, 30, 30, 40, 40, 50, 50};
        native.mData.health = 45;
        native.mBaseConfig.tes4.baseSpell = 350;
        native.mBaseConfig.tes4.fatigue = 150;
        mStore.getWritable<ESM4::Npc>().insertStatic(native, baseKey);
        ESM::NPC facade{};
        facade.blank();
        facade.mId = ESM::RefId::stringRefId("Player");
        const auto* record = mStore.insertStatic(facade);
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(mStore, readers);
        environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        ESM::NpcState initial{};
        initial.blank();
        MWWorld::Player player(record);
        auto ptr = player.getPlayer();
        ptr.getClass().readAdditionalState(ptr, initial);
        MWMechanics::OblivionCombatService service;
        const auto actor = ESM::FormKey::dynamic("player", 1);
        service.initializePlayerActor(player, mStore);
        EXPECT_EQ(service.getPlayerValue(8), 80);
        EXPECT_EQ(service.getPlayerValue(9), 75);
        EXPECT_EQ(service.getPlayerValue(10), 150);
        EXPECT_EQ(service.findActorValues(actor)->mPlayerFormValues,
            (std::optional<std::array<std::int32_t, 4>>{{0, 0, 0, 0}}));
        ASSERT_NE(service.findActorLife(actor), nullptr);
        EXPECT_EQ(service.findActorLife(actor)->mPhase, ESM4::ActorLifePhase::Alive);
        EXPECT_FALSE(ptr.getClass().getCreatureStats(ptr).isDead());
        EXPECT_FALSE(service.takeNextDeathEvent());
        service.changePlayerValue(player, 8, ESM4::ActorValueModifier::Damage, -10,
            MWWorld::resolveOblivionPlayerDynamicBaseSettings(mStore));
        const auto before = *service.findActorValues(actor);
        const auto life = *service.findActorLife(actor);
        native.mData.health = 900;
        native.mIsTES4 = false; // Restored values do not depend on a fresh form resolver.
        mStore.getWritable<ESM4::Npc>().insertStatic(native, baseKey);
        service.initializePlayerActor(player, mStore);
        EXPECT_EQ(*service.findActorValues(actor), before);
        EXPECT_EQ(*service.findActorLife(actor), life);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 70);
        EXPECT_THROW(service.initializePlayerActor(player, mStore, true), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(actor), before);
        EXPECT_EQ(*service.findActorLife(actor), life);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = actor;
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        service.capture(saved);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        MWWorld::Player fresh(record);
        ptr = fresh.getPlayer();
        ptr.getClass().readAdditionalState(ptr, initial);
        restored.initializePlayerActor(fresh, mStore);
        EXPECT_EQ(*restored.findActorValues(actor), before);
        EXPECT_EQ(*restored.findActorLife(actor), life);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 70);
        EXPECT_FALSE(restored.takeNextDeathEvent());
        restored.capture(saved);
        EXPECT_TRUE(saved.mNativeDeathCounts.empty());
        EXPECT_EQ(saved.mNextDeathEvent, 1);
    }

    TEST_F(OblivionActorStatsTest, playerConstructionPreflightsSettingsAndAdoptsExplicitLegacyDeath)
    {
        sharedStats();
        const auto baseKey = ESM::FormKey::content("Oblivion.esm", 7);
        const auto actor = ESM::FormKey::dynamic("player", 1);
        ESM4::Npc native{};
        native.mId = {7, 4};
        native.mFormKey = baseKey;
        native.mIsTES4 = true;
        native.mData.health = 0;
        mStore.getWritable<ESM4::Npc>().insertStatic(native, baseKey);
        ESM::NPC facade{};
        facade.blank();
        facade.mId = ESM::RefId::stringRefId("Player");
        const auto* record = mStore.insertStatic(facade);
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(mStore, readers);
        environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        ESM::NpcState initial{};
        initial.blank();
        ESM4::GameSetting setting{};
        setting.mId = {0x9e62f, 4};
        setting.mEditorId = "fPCBaseHealthMult";
        setting.mData = std::numeric_limits<float>::quiet_NaN();
        mStore.getWritable<ESM4::GameSetting>().insertStatic(setting);
        MWWorld::Player player(record);
        const auto ptr = player.getPlayer();
        ptr.getClass().readAdditionalState(ptr, initial);
        const auto health = ptr.getClass().getCreatureStats(ptr).getHealth();
        MWMechanics::OblivionCombatService failed;
        EXPECT_THROW(failed.initializePlayerActor(player, mStore), std::invalid_argument);
        EXPECT_EQ(failed.findActorValues(actor), nullptr);
        EXPECT_EQ(failed.findActorLife(actor), nullptr);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth(), health);
        setting.mData = 2.f;
        mStore.getWritable<ESM4::GameSetting>().insertStatic(setting);
        for (const std::optional<bool> dead : {std::optional<bool>{}, std::optional<bool>{false}, std::optional<bool>{true}})
        {
            MWWorld::Player fresh(record);
            const auto freshPtr = fresh.getPlayer();
            freshPtr.getClass().readAdditionalState(freshPtr, initial);
            MWMechanics::OblivionCombatService service;
            service.initializePlayerActor(fresh, mStore, dead);
            EXPECT_EQ(service.getPlayerValue(8), 0);
            ASSERT_NE(service.findActorLife(actor), nullptr);
            EXPECT_EQ(service.findActorLife(actor)->mPhase,
                dead.value_or(false) ? ESM4::ActorLifePhase::Dead : ESM4::ActorLifePhase::Alive);
            EXPECT_EQ(freshPtr.getClass().getCreatureStats(freshPtr).isDead(), dead.value_or(false));
            EXPECT_FALSE(service.takeNextDeathEvent());
            ESM4::RuntimeState saved;
            service.capture(saved);
            EXPECT_TRUE(saved.mNativeDeathCounts.empty());
            EXPECT_EQ(saved.mNextDeathEvent, 1);
        }
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

    TEST_F(OblivionActorStatsTest, playerLifeTransitionsOwnEventsAcrossCallbacksRestartAndExhaustion)
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
        auto& stats = ptr.getClass().getCreatureStats(ptr);
        MWMechanics::OblivionCombatService service;
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{10, 10, 10, 0}};
        service.publishPlayerValues(player, values, {});
        ESM4::RuntimeActorLife alive;
        alive.mActor = values.mActor;
        alive.mBase = values.mBase;
        auto dead = alive;
        dead.mPhase = ESM4::ActorLifePhase::Dead;
        dead.mKiller = values.mActor;
        EXPECT_THROW(service.transitionPlayerLife(player, dead), std::invalid_argument);
        service.publishPlayerLife(player, alive);
        EXPECT_FALSE(service.transitionPlayerLife(player, alive));
        EXPECT_TRUE(service.killPlayer(player, dead.mKiller, false, {}));
        EXPECT_EQ(service.getDeadCount(values.mBase), 1);
        EXPECT_TRUE(stats.isDead());
        EXPECT_EQ(stats.getHealth().getCurrent(), 10); // Policy owns Health changes separately.
        auto repeated = dead;
        repeated.mKiller = {};
        EXPECT_FALSE(service.killPlayer(player, {}, false, {}));
        EXPECT_EQ(*service.findActorLife(values.mActor), dead);
        EXPECT_EQ(service.getDeadCount(values.mBase), 1);
        const auto first = service.takeNextDeathEvent();
        ASSERT_TRUE(first);
        EXPECT_EQ(first->mId, 1);
        EXPECT_EQ(first->mKiller, values.mActor);
        EXPECT_FALSE(service.takeNextDeathEvent());
        // A callback can revive and kill again, then save: the consumed event
        // stays consumed while its newly issued successor remains pending.
        EXPECT_TRUE(service.transitionPlayerLife(player, alive));
        EXPECT_FALSE(stats.isDead());
        EXPECT_TRUE(service.transitionPlayerLife(player, dead));
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = values.mActor;
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        service.capture(saved);
        EXPECT_EQ(saved.mNextDeathEvent, 3);
        EXPECT_EQ(saved.mNativeDeathCounts.at(values.mBase), 2);
        ASSERT_EQ(saved.mPendingDeathEvents.size(), 1);
        EXPECT_EQ(saved.mPendingDeathEvents.front().mId, 2);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        EXPECT_EQ(restored.takeNextDeathEvent()->mId, 2);
        EXPECT_FALSE(restored.takeNextDeathEvent());
        EXPECT_TRUE(restored.transitionPlayerLife(player, alive));
        auto essential = alive;
        essential.mPhase = ESM4::ActorLifePhase::EssentialUnconscious;
        essential.mRecoveryRemaining = 10;
        EXPECT_TRUE(restored.transitionPlayerLife(player, essential));
        EXPECT_EQ(restored.getDeadCount(values.mBase), 2);
        EXPECT_FALSE(stats.isDead());
        EXPECT_TRUE(stats.getKnockedDown());
        EXPECT_FALSE(restored.takeNextDeathEvent());
        repeated = essential;
        repeated.mRecoveryRemaining = 20;
        EXPECT_FALSE(restored.transitionPlayerLife(player, repeated));
        EXPECT_EQ(restored.findActorLife(values.mActor)->mRecoveryRemaining, 10);
        EXPECT_TRUE(restored.transitionPlayerLife(player, alive));
        restored.capture(saved);
        service.restore(saved);
        service.setPlayerBaseValue(player, 8, 1, {});
        EXPECT_EQ(stats.getHealth().getCurrent(), 1);
        EXPECT_FALSE(service.reactPlayerHealth(player, {}, false, {}));
        service.changePlayerValue(player, 8, ESM4::ActorValueModifier::Damage, -0x1p-24f, {});
        EXPECT_LT(stats.getHealth().getCurrent(), 1);
        const auto beforeReaction = *service.findActorValues(values.mActor);
        EXPECT_THROW(service.reactPlayerHealth(player, {}, true, {-1, .3f}), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(values.mActor), beforeReaction);
        EXPECT_EQ(*service.findActorLife(values.mActor), alive);
        EXPECT_TRUE(service.reactPlayerHealth(player, {}, true, {2.5f, .3f}));
        EXPECT_FALSE(stats.isDead());
        EXPECT_TRUE(stats.getKnockedDown());
        EXPECT_FLOAT_EQ(stats.getHealth().getCurrent(), .3f);
        EXPECT_EQ(service.findActorLife(values.mActor)->mRecoveryRemaining, 2.5f);
        EXPECT_FALSE(service.takeNextDeathEvent());
        service.changePlayerValue(player, 8, ESM4::ActorValueModifier::Damage, -1, {});
        EXPECT_FALSE(service.reactPlayerHealth(player, {}, true, {10, 1}));
        EXPECT_EQ(service.findActorLife(values.mActor)->mRecoveryRemaining, 2.5f);
        EXPECT_TRUE(service.transitionPlayerLife(player, alive));
        EXPECT_TRUE(service.reactPlayerHealth(player, values.mActor, false, {}));
        EXPECT_TRUE(stats.isDead());
        EXPECT_LT(stats.getHealth().getCurrent(), 0);
        EXPECT_FALSE(service.reactPlayerHealth(player, {}, false, {}));
        const auto reactionEvent = service.takeNextDeathEvent();
        ASSERT_TRUE(reactionEvent);
        EXPECT_EQ(reactionEvent->mId, 3);
        EXPECT_EQ(reactionEvent->mKiller, values.mActor);
        EXPECT_FALSE(service.takeNextDeathEvent());
        EXPECT_TRUE(service.transitionPlayerLife(player, alive));
        service.publishPlayerValues(player, saved.mNativeActorValues.front(), {});
        service.changePlayerValue(player, 8, ESM4::ActorValueModifier::Script, -20, {});
        EXPECT_EQ(stats.getHealth().getCurrent(), -19);
        EXPECT_TRUE(service.reactPlayerHealth(player, {}, true, {10, .3f}));
        // Player positive Damage is capped at zero, so a Script deficit is
        // not erased by assigning the desired recovery target to current.
        EXPECT_EQ(stats.getHealth().getCurrent(), -19);
        EXPECT_TRUE(stats.getKnockedDown());
        EXPECT_FALSE(stats.isDead());
        EXPECT_FALSE(service.takeNextDeathEvent());
        EXPECT_TRUE(service.transitionPlayerLife(player, alive));
        service.publishPlayerValues(player, saved.mNativeActorValues.front(), {});
        EXPECT_EQ(stats.getHealth().getCurrent(), 1);
        EXPECT_TRUE(service.killPlayer(player, {}, true, {4, .3f}));
        EXPECT_FLOAT_EQ(stats.getHealth().getCurrent(), .3f);
        EXPECT_TRUE(stats.getKnockedDown());
        EXPECT_FALSE(stats.isDead());
        EXPECT_EQ(service.findActorLife(values.mActor)->mRecoveryRemaining, 4);
        EXPECT_FALSE(service.takeNextDeathEvent());
        // Return the shared view to the saved alive state before the exhausted
        // namespace rollback check on the independent restored authority.
        restored.publishPlayerValues(player, *restored.findActorValues(values.mActor), {});
        saved.mNextDeathEvent = std::numeric_limits<std::uint64_t>::max();
        restored.restore(saved);
        EXPECT_THROW(restored.transitionPlayerLife(player, dead), std::overflow_error);
        EXPECT_FALSE(stats.isDead());
        EXPECT_EQ(*restored.findActorLife(values.mActor), alive);
        EXPECT_FALSE(restored.takeNextDeathEvent());
        auto after = saved;
        restored.capture(after);
        EXPECT_EQ(after, saved);
        for (const auto count : {32767u, 65535u})
        {
            saved.mNextDeathEvent = 1;
            saved.mNativeDeathCounts[values.mBase] = count;
            restored.restore(saved);
            EXPECT_TRUE(restored.transitionPlayerLife(player, dead));
            EXPECT_EQ(restored.getDeadCount(values.mBase), count == 32767 ? -32768 : 0);
            auto wrapped = saved;
            restored.capture(wrapped);
            EXPECT_EQ(wrapped.mNativeDeathCounts.at(values.mBase), count == 32767 ? 32768 : 0);
            EXPECT_EQ(wrapped.mNextDeathEvent, 2);
        }

    }

    TEST_F(OblivionActorStatsTest, playerEssentialRecoveryHonorsGodModeAndClearsAttribution)
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
        auto& stats = ptr.getClass().getCreatureStats(ptr);
        MWMechanics::OblivionCombatService service;
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{100, 0, 0, 0}};
        service.publishPlayerValues(player, values, {});
        EXPECT_THROW(service.advancePlayerEssentialRecovery(player, 1, 1, true, {10, .3f}), std::invalid_argument);
        ESM4::RuntimeActorLife alive{values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}};
        service.publishPlayerLife(player, alive);
        EXPECT_TRUE(service.killPlayer(player, values.mActor, true, {10, .3f}, true));
        EXPECT_EQ(stats.getHealth().getCurrent(), 100); // God Mode rejects the negative Damage adjustment.
        EXPECT_TRUE(stats.getKnockedDown());
        EXPECT_FALSE(service.advancePlayerEssentialRecovery(player, 100, 0, true, {10, .3f}, true));
        EXPECT_EQ(service.findActorLife(values.mActor)->mRecoveryRemaining, 10);
        EXPECT_FALSE(service.advancePlayerEssentialRecovery(player, 4, 1, true, {10, .3f}, true));
        EXPECT_EQ(service.findActorLife(values.mActor)->mRecoveryRemaining, 6);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = values.mActor;
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        service.capture(saved);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        restored.publishPlayerValues(player, *restored.findActorValues(values.mActor), {});
        EXPECT_TRUE(restored.advancePlayerEssentialRecovery(player, 6, 3, true, {10, .3f}, true));
        EXPECT_EQ(stats.getHealth().getCurrent(), 100);
        EXPECT_EQ(*restored.findActorLife(values.mActor), alive);
        EXPECT_FALSE(stats.isDead());
        EXPECT_FALSE(stats.getKnockedDown());
        EXPECT_FALSE(restored.takeNextDeathEvent());
        EXPECT_EQ(restored.getDeadCount(values.mBase), 0);
        EXPECT_TRUE(restored.killPlayer(player, {}, true, {1, .3f}));
        EXPECT_FLOAT_EQ(stats.getHealth().getCurrent(), 30);
        EXPECT_TRUE(restored.advancePlayerEssentialRecovery(player, 1, 1, true, {4, 0}));
        EXPECT_EQ(restored.findActorLife(values.mActor)->mPhase, ESM4::ActorLifePhase::EssentialUnconscious);
        EXPECT_EQ(restored.findActorLife(values.mActor)->mRecoveryRemaining, 4);
        EXPECT_EQ(stats.getHealth().getCurrent(), 0);
        EXPECT_FALSE(restored.takeNextDeathEvent());
        // Zero recovery delta does not invoke the negative-Health callback.
        EXPECT_TRUE(restored.advancePlayerEssentialRecovery(player, 4, 3, true, {4, 0}));
        EXPECT_EQ(*restored.findActorLife(values.mActor), alive);
        EXPECT_EQ(stats.getHealth().getCurrent(), 0);
        EXPECT_FALSE(stats.isDead());
    }

    TEST_F(OblivionActorStatsTest, npcEssentialRecoveryRestartsAndCommitsHealthReentryOrDeathAtomically)
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
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[8].mBase = 100;
        service.publishNonPlayerValues(ptr, values);
        ESM4::RuntimeActorLife life{values.mActor, values.mBase, ESM4::ActorLifePhase::EssentialUnconscious,
            .125f, ESM::FormKey::dynamic("player", 1)};
        service.publishNonPlayerLife(ptr, life);
        EXPECT_THROW(service.advanceNonPlayerEssentialRecovery(ptr, -1, 1, true, {4, .5f}), std::invalid_argument);
        EXPECT_FALSE(service.advanceNonPlayerEssentialRecovery(ptr, .0625f, 1, true, {4, .5f}));
        EXPECT_EQ(service.findActorLife(values.mActor)->mRecoveryRemaining, .0625f);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 100);
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
        MWWorld::LiveCellRef<ESM4::Npc> newLive(reference, mStore.search<ESM4::Npc>(mActorKey));
        MWWorld::Ptr newPtr(&newLive);
        restored.publishNonPlayerValues(newPtr, *restored.findActorValues(values.mActor));
        auto& stats = newPtr.getClass().getCreatureStats(newPtr);
        EXPECT_TRUE(stats.getKnockedDown());
        EXPECT_THROW(restored.advanceNonPlayerEssentialRecovery(newPtr, .0625f, 3, true,
            {4, std::numeric_limits<float>::infinity()}), std::invalid_argument);
        auto unchanged = saved;
        restored.capture(unchanged);
        EXPECT_EQ(unchanged, saved);
        EXPECT_EQ(stats.getHealth().getCurrent(), 100);
        EXPECT_TRUE(restored.advanceNonPlayerEssentialRecovery(newPtr, .0625f, 3, true, {4, .5f}));
        EXPECT_EQ(stats.getHealth().getCurrent(), 50);
        EXPECT_FALSE(stats.getKnockedDown());
        EXPECT_FALSE(stats.isDead());
        EXPECT_EQ(restored.findActorLife(values.mActor)->mPhase, ESM4::ActorLifePhase::Alive);
        EXPECT_TRUE(restored.findActorLife(values.mActor)->mKiller.isNull());
        EXPECT_FALSE(restored.takeNextDeathEvent());
        // Negative recovery adjustment leaves sub-one Health and reenters
        // essential state through the ordinary Health reaction gate.
        values.mValues[8] = {1, {std::nullopt, 1, std::nullopt}};
        restored.publishNonPlayerValues(newPtr, values);
        life.mRecoveryRemaining = 0;
        restored.publishNonPlayerLife(newPtr, life);
        EXPECT_TRUE(restored.advanceNonPlayerEssentialRecovery(newPtr, 0, 1, true, {4, .3f}));
        EXPECT_EQ(restored.findActorLife(values.mActor)->mPhase, ESM4::ActorLifePhase::EssentialUnconscious);
        EXPECT_EQ(restored.findActorLife(values.mActor)->mRecoveryRemaining, 4);
        EXPECT_TRUE(restored.findActorLife(values.mActor)->mKiller.isNull());
        EXPECT_FALSE(restored.takeNextDeathEvent());
        EXPECT_EQ(restored.getDeadCount(values.mBase), 0);
        // Removing the essential flag while unconscious makes that same
        // negative recovery write terminal. Exhaustion must roll back timer,
        // Health, life, event and count together.
        restored.publishNonPlayerValues(newPtr, values);
        life.mRecoveryRemaining = .125f;
        restored.publishNonPlayerLife(newPtr, life);
        restored.capture(saved);
        saved.mNextDeathEvent = std::numeric_limits<std::uint64_t>::max();
        restored.restore(saved, mStore);
        EXPECT_THROW(restored.advanceNonPlayerEssentialRecovery(newPtr, .125f, 1, false, {4, .3f}), std::overflow_error);
        unchanged = saved;
        restored.capture(unchanged);
        EXPECT_EQ(unchanged, saved);
        EXPECT_EQ(stats.getHealth().getCurrent(), 2);
        EXPECT_TRUE(stats.getKnockedDown());
        saved.mNextDeathEvent = 1;
        restored.restore(saved, mStore);
        EXPECT_TRUE(restored.advanceNonPlayerEssentialRecovery(newPtr, .125f, 1, false, {4, .3f}));
        EXPECT_TRUE(stats.isDead());
        EXPECT_FALSE(stats.getKnockedDown());
        EXPECT_EQ(restored.getDeadCount(values.mBase), 1);
        const auto event = restored.takeNextDeathEvent();
        ASSERT_TRUE(event);
        EXPECT_TRUE(event->mKiller.isNull());
        EXPECT_EQ(event->mId, 1);
        EXPECT_FALSE(restored.advanceNonPlayerEssentialRecovery(newPtr, 1, 1, false, {4, .3f}));
        // Positive restoration to sub-one Health does not call the negative
        // writer reaction, so it can recover Alive below one.
        values.mValues[8] = {1, {std::nullopt, std::nullopt, -1}};
        restored.publishNonPlayerValues(newPtr, values);
        life.mRecoveryRemaining = 0;
        restored.publishNonPlayerLife(newPtr, life);
        EXPECT_TRUE(restored.advanceNonPlayerEssentialRecovery(newPtr, 0, 3, true, {4, .3f}));
        EXPECT_EQ(restored.findActorLife(values.mActor)->mPhase, ESM4::ActorLifePhase::Alive);
        EXPECT_LT(stats.getHealth().getCurrent(), 1);
        EXPECT_FALSE(stats.isDead());
        EXPECT_FALSE(restored.takeNextDeathEvent());
        EXPECT_EQ(restored.getDeadCount(values.mBase), 1);
    }

    TEST_F(OblivionActorStatsTest, creatureEssentialRecoveryPublishesWithoutChangingOtherValues)
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
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[8].mBase = 19;
        values.mValues[9].mBase = 17;
        values.mValues[10].mBase = 18;
        values.mValues[12] = {21, {2, 3, -1}};
        MWMechanics::OblivionCombatService service;
        service.publishNonPlayerValues(ptr, values);
        ESM4::RuntimeActorLife life{values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}};
        service.publishNonPlayerLife(ptr, life);
        EXPECT_TRUE(service.killNonPlayer(ptr, ESM::FormKey::dynamic("player", 1), true, {2, .5f}));
        EXPECT_EQ(stats.getHealth().getCurrent(), 9.5f);
        EXPECT_TRUE(stats.getKnockedDown());
        // A second Kill cannot restart the countdown or replace attribution.
        EXPECT_FALSE(service.advanceNonPlayerEssentialRecovery(ptr, 1, 1, true, {2, .5f}));
        EXPECT_FALSE(service.killNonPlayer(ptr, {}, true, {2, .5f}));
        EXPECT_EQ(service.findActorLife(values.mActor)->mRecoveryRemaining, 1);
        EXPECT_EQ(service.findActorLife(values.mActor)->mKiller, ESM::FormKey::dynamic("player", 1));
        EXPECT_TRUE(service.advanceNonPlayerEssentialRecovery(ptr, 1.25f, 3, true, {2, 1}));
        EXPECT_EQ(*service.findActorLife(values.mActor), life);
        EXPECT_EQ(stats.getHealth().getCurrent(), 19);
        EXPECT_FALSE(stats.getKnockedDown());
        EXPECT_FALSE(stats.isDead());
        EXPECT_EQ(stats.getMagicka().getCurrent(), 17);
        EXPECT_EQ(stats.getFatigue().getCurrent(), 18);
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Marksman), 25);
        const auto* after = service.findActorValues(values.mActor);
        ASSERT_TRUE(after);
        for (std::size_t i = 0; i < values.mValues.size(); ++i)
        {
            if (i != 8)
            {
                EXPECT_EQ(after->mValues[i], values.mValues[i]);
            }
        }
        EXPECT_FALSE(after->mValues[8].mModifiers[2]); // Native nonplayer zero removes the sparse entry.
        // Dedicated Magicka/Fatigue slots clamp positive Damage even when
        // an old staged snapshot omitted the constructor's zero entries.
        for (std::uint8_t av : {9, 10})
        {
            service.changeNonPlayerValue(ptr, av, ESM4::ActorValueModifier::Damage, 5);
            EXPECT_EQ(service.findActorValues(values.mActor)->mValues[av].mModifiers[2], 0);
            service.changeNonPlayerValue(ptr, av, ESM4::ActorValueModifier::Damage, -5);
            service.changeNonPlayerValue(ptr, av, ESM4::ActorValueModifier::Damage, 5);
            EXPECT_EQ(service.findActorValues(values.mActor)->mValues[av].mModifiers[2], 0);
        }
        EXPECT_EQ(stats.getMagicka().getCurrent(), 17);
        EXPECT_EQ(stats.getFatigue().getCurrent(), 18);
        EXPECT_EQ(service.getDeadCount(values.mBase), 0);
        EXPECT_FALSE(service.takeNextDeathEvent());
        EXPECT_TRUE(service.killNonPlayer(ptr, {}, false, {}));
        service.resetNonPlayerForResurrection(ptr);
        EXPECT_FALSE(stats.isDead());
        EXPECT_EQ(stats.getHealth().getCurrent(), 19);
        EXPECT_EQ(stats.getMagicka().getCurrent(), 17);
        EXPECT_EQ(stats.getFatigue().getCurrent(), 18);
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Marksman), 21);
        EXPECT_EQ(service.findActorValues(values.mActor)->mProcess, ESM4::ActorValueProcess::Low);
        EXPECT_EQ(service.getDeadCount(values.mBase), 1);
        EXPECT_TRUE(service.takeNextDeathEvent());
        EXPECT_FALSE(service.takeNextDeathEvent());
    }

    TEST_F(OblivionActorStatsTest, playerResurrectionResetPreservesModifiersAndHistoricalDeathEvents)
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
        auto& stats = ptr.getClass().getCreatureStats(ptr);
        MWMechanics::OblivionCombatService service;
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{100, 30, 40, 0}};
        values.mValues[0] = {50, {1, 2, -3}};
        values.mValues[5].mBase = 20;
        values.mValues[8].mModifiers = {10, 7, -120};
        values.mValues[9].mModifiers = {4, 6, -20};
        values.mValues[10].mModifiers = {5, -7, -50};
        values.mValues[12] = {20, {4, 5, -6}};
        service.publishPlayerValues(player, values, {});
        const auto before = *service.findActorValues(values.mActor);
        EXPECT_THROW(service.resetPlayerForResurrection(player), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(values.mActor), before);
        const ESM4::RuntimeActorLife alive{values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}};
        service.publishPlayerLife(player, alive);
        EXPECT_TRUE(service.killPlayer(player, values.mActor, false, {}));
        stats.setDeathAnimationFinished(true);
        service.resetPlayerForResurrection(player);
        EXPECT_EQ(*service.findActorLife(values.mActor), alive);
        EXPECT_FALSE(stats.isDead());
        EXPECT_FALSE(stats.isDeathAnimationFinished());
        EXPECT_EQ(stats.getHealth().getCurrent(), 117);
        EXPECT_EQ(stats.getMagicka().getCurrent(), 40);
        EXPECT_EQ(stats.getFatigue().getCurrent(), 108);
        auto expected = before;
        for (std::uint8_t av : {8, 9, 10})
            expected.mValues[av].mModifiers[2] = 0;
        EXPECT_EQ(*service.findActorValues(values.mActor), expected);
        EXPECT_EQ(service.getDeadCount(values.mBase), 1);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = values.mActor;
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        service.capture(saved);
        ASSERT_EQ(saved.mPendingDeathEvents.size(), 1);
        EXPECT_EQ(saved.mPendingDeathEvents[0].mKiller, values.mActor);
        service.resetPlayerForResurrection(player);
        auto repeated = saved;
        service.capture(repeated);
        EXPECT_EQ(repeated, saved);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        restored.publishPlayerValues(player, *restored.findActorValues(values.mActor), {});
        EXPECT_EQ(stats.getHealth().getCurrent(), 117);
        EXPECT_FALSE(stats.isDead());
        EXPECT_EQ(restored.takeNextDeathEvent(), saved.mPendingDeathEvents[0]);
        EXPECT_FALSE(restored.takeNextDeathEvent());
        values = *restored.findActorValues(values.mActor);
        (*values.mPlayerFormValues)[0] = -100;
        restored.publishPlayerValues(player, values, {});
        EXPECT_TRUE(restored.killPlayer(player, {}, true, {4, .3f}));
        EXPECT_TRUE(stats.getKnockedDown());
        restored.resetPlayerForResurrection(player);
        EXPECT_EQ(stats.getHealth().getCurrent(), -83);
        EXPECT_EQ(*restored.findActorLife(values.mActor), alive);
        EXPECT_FALSE(stats.isDead());
        EXPECT_FALSE(stats.getKnockedDown());
        EXPECT_FALSE(restored.takeNextDeathEvent());
        EXPECT_EQ(restored.getDeadCount(values.mBase), 1);
    }

    TEST_F(OblivionActorStatsTest, npcResurrectionResetRestoresChannelsAndPreservesDeathHistoryAcrossRestart)
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
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[0] = {50, {1, 2, -3}};
        values.mValues[8] = {100, {10, 7, -120}};
        values.mValues[9] = {30, {4, 6, -20}};
        values.mValues[10] = {40, {5, -7, -50}};
        values.mValues[12] = {20, {4, 5, -6}};
        service.publishNonPlayerValues(ptr, values);
        EXPECT_THROW(service.resetNonPlayerForResurrection(ptr), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(values.mActor), values);
        const ESM4::RuntimeActorLife alive{values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}};
        service.publishNonPlayerLife(ptr, alive);
        const auto killer = ESM::FormKey::dynamic("player", 1);
        EXPECT_TRUE(service.killNonPlayer(ptr, killer, false, {}));
        auto& stats = ptr.getClass().getCreatureStats(ptr);
        stats.setDeathAnimationFinished(true);
        service.resetNonPlayerForResurrection(ptr);
        EXPECT_EQ(*service.findActorLife(values.mActor), alive);
        EXPECT_FALSE(stats.isDead());
        EXPECT_FALSE(stats.isDeathAnimationFinished());
        EXPECT_EQ(stats.getHealth().getCurrent(), 100);
        EXPECT_EQ(stats.getMagicka().getCurrent(), 36);
        EXPECT_EQ(stats.getFatigue().getCurrent(), 33);
        EXPECT_EQ(ptr.getClass().getSkill(ptr, ESM::Skill::Armorer), 20);
        const auto* reset = service.findActorValues(values.mActor);
        ASSERT_TRUE(reset);
        EXPECT_EQ(reset->mProcess, ESM4::ActorValueProcess::Low);
        EXPECT_EQ(reset->mValues[0], (ESM4::ActorValueState{50, {}}));
        EXPECT_EQ(reset->mValues[8], (ESM4::ActorValueState{100, {}}));
        EXPECT_EQ(reset->mValues[9], (ESM4::ActorValueState{30, {std::nullopt, 6, 0}}));
        EXPECT_EQ(reset->mValues[10], (ESM4::ActorValueState{40, {std::nullopt, -7, 0}}));
        EXPECT_EQ(service.getDeadCount(values.mBase), 1);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = killer;
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        ESM4::RuntimeReferenceState savedActor;
        savedActor.mKey = values.mActor;
        savedActor.mBase = values.mBase;
        savedActor.mCell = saved.mPlayer.mCell;
        saved.mReferences.push_back(savedActor);
        service.capture(saved);
        ASSERT_EQ(saved.mPendingDeathEvents.size(), 1);
        EXPECT_EQ(saved.mPendingDeathEvents[0].mKiller, killer);
        service.resetNonPlayerForResurrection(ptr); // Resetting a live actor is also valid.
        auto repeated = saved;
        service.capture(repeated);
        EXPECT_EQ(repeated, saved);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()), mStore);
        MWWorld::LiveCellRef<ESM4::Npc> newLive(reference, mStore.search<ESM4::Npc>(mActorKey));
        MWWorld::Ptr newPtr(&newLive);
        restored.publishNonPlayerValues(newPtr, *restored.findActorValues(values.mActor));
        EXPECT_EQ(newPtr.getClass().getCreatureStats(newPtr).getMagicka().getCurrent(), 36);
        EXPECT_FALSE(newPtr.getClass().getCreatureStats(newPtr).isDead());
        EXPECT_EQ(restored.takeNextDeathEvent(), saved.mPendingDeathEvents[0]);
        EXPECT_FALSE(restored.takeNextDeathEvent());
        EXPECT_EQ(restored.getDeadCount(values.mBase), 1);
        // Reset is a storage operation, not a negative Health writer callback.
        values.mValues[8].mBase = -100;
        restored.publishNonPlayerValues(newPtr, values);
        restored.resetNonPlayerForResurrection(newPtr);
        EXPECT_EQ(newPtr.getClass().getCreatureStats(newPtr).getHealth().getCurrent(), -100);
        EXPECT_FALSE(newPtr.getClass().getCreatureStats(newPtr).isDead());
        EXPECT_FALSE(restored.takeNextDeathEvent());
    }

    TEST_F(OblivionActorStatsTest, preservedResurrectionKeepsStateAndCommitsNewHealthDeathEvenWhenPreviouslyDead)
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
        using Phase = ESM4::ActorLifePhase;
        struct Case
        {
            ESM4::ActorValueState mHealth;
            bool mEssential;
            float mExpectedHealth;
            float mExpectedDamage;
            Phase mExpectedPhase;
        };
        const Case cases[] = {
            {{100, {10, 7, -120}}, false, 100, -17, Phase::Alive},
            {{100, {10, 7, 0}}, false, 100, -17, Phase::Alive},
            {{0, {std::nullopt, 2, std::nullopt}}, false, 0, -2, Phase::Dead},
            {{0, {std::nullopt, 2, std::nullopt}}, true, 0, -2, Phase::EssentialUnconscious},
            {{0, {std::nullopt, -2, std::nullopt}}, false, 0, 2, Phase::Alive},
            {{0, {std::nullopt, 2, -2}}, false, 0, -2, Phase::Alive},
        };
        const auto killer = ESM::FormKey::dynamic("player", 1);
        ESM4::RuntimeState saveTemplate;
        saveTemplate.mPlayer.mReference = killer;
        saveTemplate.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saveTemplate.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saveTemplate.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        ESM4::RuntimeReferenceState savedActor;
        savedActor.mKey = reference.mFormKey;
        savedActor.mBase = mActorKey;
        savedActor.mCell = saveTemplate.mPlayer.mCell;
        saveTemplate.mReferences.push_back(savedActor);
        for (auto prior : {Phase::Alive, Phase::Dead, Phase::EssentialUnconscious})
            for (std::size_t index = 0; index < std::size(cases); ++index)
            {
                SCOPED_TRACE(testing::Message() << "prior=" << static_cast<int>(prior) << " case=" << index);
                const auto& test = cases[index];
                MWMechanics::OblivionCombatService service;
                ESM4::RuntimeActorValues values;
                values.mActor = reference.mFormKey;
                values.mBase = mActorKey;
                values.mValues[8] = test.mHealth;
                values.mValues[9] = {30, {4, 6, -20}};
                values.mValues[10] = {40, {5, -7, -50}};
                values.mValues[12] = {20, {4, 5, -6}};
                service.publishNonPlayerValues(ptr, values);
                EXPECT_THROW(service.reviveNonPlayerPreservingState(ptr, test.mEssential, {4, .3f}),
                    std::invalid_argument);
                service.publishNonPlayerLife(ptr, {values.mActor, values.mBase, prior,
                    prior == Phase::EssentialUnconscious ? 2.f : 0.f, prior == Phase::Alive ? ESM::FormKey{} : killer});
                ptr.getClass().getCreatureStats(ptr).setDeathAnimationFinished(prior == Phase::Dead);
                auto before = saveTemplate;
                service.capture(before);
                before.mNativeDeathCounts[values.mBase] = 17;
                before.mPendingDeathEvents.push_back({7, values.mActor, killer});
                before.mNextDeathEvent = 9;
                service.restore(before, mStore);
                if (test.mExpectedPhase == Phase::Dead)
                {
                    auto exhausted = before;
                    exhausted.mNextDeathEvent = std::numeric_limits<std::uint64_t>::max();
                    service.restore(exhausted, mStore);
                    const auto healthBefore = ptr.getClass().getCreatureStats(ptr).getHealth();
                    EXPECT_THROW(service.reviveNonPlayerPreservingState(ptr, false, {}), std::overflow_error);
                    auto afterFailure = exhausted;
                    service.capture(afterFailure);
                    EXPECT_EQ(afterFailure, exhausted);
                    EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth(), healthBefore);
                    EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).isDeathAnimationFinished(), prior == Phase::Dead);
                    service.restore(before, mStore);
                }
                if (test.mEssential)
                {
                    EXPECT_THROW(service.reviveNonPlayerPreservingState(ptr, true,
                        {std::numeric_limits<float>::quiet_NaN(), .3f}), std::invalid_argument);
                    auto afterFailure = before;
                    service.capture(afterFailure);
                    EXPECT_EQ(afterFailure, before);
                }
                service.reviveNonPlayerPreservingState(ptr, test.mEssential, {4, .3f});
                auto after = saveTemplate;
                service.capture(after);
                auto expectedValues = values;
                expectedValues.mValues[8].mModifiers[2] = test.mExpectedDamage;
                EXPECT_EQ(*service.findActorValues(values.mActor), expectedValues);
                const auto* life = service.findActorLife(values.mActor);
                ASSERT_TRUE(life);
                EXPECT_EQ(life->mPhase, test.mExpectedPhase);
                EXPECT_TRUE(life->mKiller.isNull());
                EXPECT_EQ(life->mRecoveryRemaining, test.mExpectedPhase == Phase::EssentialUnconscious ? 4 : 0);
                const auto& stats = ptr.getClass().getCreatureStats(ptr);
                EXPECT_EQ(stats.getHealth().getCurrent(), test.mExpectedHealth);
                EXPECT_EQ(stats.isDead(), test.mExpectedPhase == Phase::Dead);
                EXPECT_FALSE(stats.isDeathAnimationFinished());
                EXPECT_EQ(stats.getKnockedDown(), test.mExpectedPhase == Phase::EssentialUnconscious);
                EXPECT_EQ(service.getDeadCount(values.mBase), test.mExpectedPhase == Phase::Dead ? 18 : 17);
                EXPECT_EQ(after.mNextDeathEvent, test.mExpectedPhase == Phase::Dead ? 10 : 9);
                EXPECT_EQ(service.takeNextDeathEvent(), before.mPendingDeathEvents[0]);
                if (test.mExpectedPhase == Phase::Dead)
                {
                    const auto event = service.takeNextDeathEvent();
                    ASSERT_TRUE(event);
                    EXPECT_EQ(event->mId, 9);
                    EXPECT_EQ(event->mActor, values.mActor);
                    EXPECT_TRUE(event->mKiller.isNull());
                }
                EXPECT_FALSE(service.takeNextDeathEvent());
                MWMechanics::OblivionCombatService restored;
                restored.restore(ESM4::RuntimeState::deserializeBinary(after.serializeBinary()), mStore);
                auto again = saveTemplate;
                restored.capture(again);
                EXPECT_EQ(again, after);
                // A zero delta still visits the native writer. An existing
                // positive sparse Health Damage entry clamps back to zero.
                if (index == 4)
                {
                    restored.reviveNonPlayerPreservingState(ptr, false, {});
                    EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), -2);
                    EXPECT_FALSE(ptr.getClass().getCreatureStats(ptr).isDead());
                    EXPECT_EQ(restored.getDeadCount(values.mBase), 17);
                }
            }
    }

    TEST_F(OblivionActorStatsTest, essentialRecoverySettingsFollowCurrentWinningNativeRecords)
    {
        auto settings = MWWorld::resolveOblivionEssentialRecoverySettings(mStore);
        EXPECT_EQ(settings.mDelay, 10);
        EXPECT_EQ(settings.mHealthFraction, .3f);
        ESM4::GameSetting delay{};
        delay.mId = {0x981, 3};
        delay.mEditorId = "fEssentialDeathTime";
        delay.mData = 3.125f;
        const auto key = ESM::FormKey::content("actors.esm", 0x981);
        mStore.getWritable<ESM4::GameSetting>().insertStatic(delay, key);
        EXPECT_EQ(MWWorld::resolveOblivionEssentialRecoverySettings(mStore).mDelay, 3.125f);
        delay.mData = 5.f;
        mStore.getWritable<ESM4::GameSetting>().insertStatic(delay, key);
        EXPECT_EQ(MWWorld::resolveOblivionEssentialRecoverySettings(mStore).mDelay, 5);
        delay.mData = -1.f;
        mStore.getWritable<ESM4::GameSetting>().insertStatic(delay, key);
        EXPECT_THROW(MWWorld::resolveOblivionEssentialRecoverySettings(mStore), std::invalid_argument);
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
            {Process::Active, {2.f, std::nullopt, -1.f}, 1.f, 42.f, 42.f, 0.f, 0.f},
            {Process::Low, {2.f, std::nullopt, std::nullopt}, 1.f, 42.f, 40.f, std::nullopt, std::nullopt},
            {Process::Active, {std::nullopt, -1.f, std::nullopt}, 1.f, 39.f, 39.f, 0.f, 0.f},
            {Process::Active, {std::nullopt, -1.f, 0.f}, 1.f, 39.f, 39.f, 0.f, 0.f},
            {Process::Active, {std::nullopt, std::nullopt, -.25f}, 0.f, 39.75f, 39.75f, -.25f, -.25f},
            {Process::Active, {std::nullopt, .75f, std::nullopt}, 1.f, 40.75f, 40.75f, std::nullopt, std::nullopt},
            {Process::Active, {-.5f, std::nullopt, -.25f}, 1.f, 39.5f, 39.5f, 0.f, 0.f},
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

    TEST_F(OblivionActorStatsTest, statModifierRequestsPreserveAuthorityChannelsAndRejectInvalidWrites)
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
        const MWWorld::Ptr npc(&live);
        const auto settings = MWWorld::resolveOblivionPlayerDynamicBaseSettings(mStore);
        MWMechanics::OblivionCombatService service;
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[8] = {100, {10.f, 5.f, -75.f}};
        values.mValues[9] = {100, {10.f, 5.f, -75.f}};
        values.mValues[10] = {100, {10.f, 5.f, -75.f}};
        values.mValues[40].mBase = 15;
        service.publishNonPlayerValues(npc, values);
        service.publishNonPlayerLife(npc, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        const auto npcKey = values.mActor;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{100, 100, 100, 0}};
        service.publishPlayerValues(player, values, settings);
        service.publishPlayerLife(player, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        const auto playerKey = values.mActor;
        for (bool isPlayer : {false, true})
        {
            const auto key = isPlayer ? playerKey : npcKey;
            const auto ptr = isPlayer ? playerPtr : npc;
            const auto request = [&](std::uint8_t av, ESM4::ActorValueModifier modifier, float wanted) {
                if (isPlayer)
                    service.requestPlayerStatModifier(player, av, modifier, wanted, settings);
                else
                    service.requestNonPlayerStatModifier(npc, av, modifier, wanted);
            };
            for (auto process : {ESM4::ActorValueProcess::Active, ESM4::ActorValueProcess::Low})
                for (std::uint8_t av = 0; av <= 32; ++av)
                {
                    if (av >= 8 && av < 12)
                        continue;
                    SCOPED_TRACE(static_cast<int>(av));
                    SCOPED_TRACE(isPlayer);
                    SCOPED_TRACE(static_cast<int>(process));
                    auto baseline = *service.findActorValues(key);
                    baseline.mProcess = process;
                    baseline.mValues[av] = {50, {3.f, 1.25f, -1.f}};
                    if (isPlayer)
                        service.publishPlayerValues(player, baseline, settings);
                    else
                        service.publishNonPlayerValues(npc, baseline);
                    request(av, ESM4::ActorValueModifier::Maximum, 7.5f);
                    request(av, ESM4::ActorValueModifier::Damage, -2.25f);
                    const auto& result = service.findActorValues(key)->mValues[av];
                    const bool ignoresMaximum = !isPlayer && process == ESM4::ActorValueProcess::Low;
                    EXPECT_EQ(result.mBase, 50);
                    EXPECT_EQ(result.mModifiers,
                        (ESM4::ActorValueModifiers{ignoresMaximum ? 3.f : 7.5f, 1.25f, -2.25f}));
                    const auto& stats = ptr.getClass().getNpcStats(ptr);
                    const MWMechanics::AttributeValue& view = av < 8
                        ? stats.getAttribute(ESM::Attribute::indexToRefId(av))
                        : stats.getSkill(MWWorld::oblivionSkillIds()[av - 12]);
                    EXPECT_EQ(view.getModifier(), ignoresMaximum ? 3.f : 7.5f);
                    EXPECT_EQ(view.getDamage(), 2.25f);
                    EXPECT_EQ(view.getModified(), !isPlayer && process == ESM4::ActorValueProcess::Low ? 49.f : 56.5f);
                    request(av, ESM4::ActorValueModifier::Damage, 0.f);
                    request(av, ESM4::ActorValueModifier::Maximum, 0.f);
                    const auto& cleared = service.findActorValues(key)->mValues[av];
                    EXPECT_EQ(cleared.mModifiers[1], 1.25f);
                    EXPECT_EQ(cleared.mModifiers[0], ignoresMaximum ? std::optional<float>(3.f)
                        : isPlayer ? std::optional<float>(0.f) : std::nullopt);
                    EXPECT_EQ(cleared.mModifiers[2], isPlayer ? std::optional<float>(0.f) : std::nullopt);
                    if (ignoresMaximum)
                    {
                        auto active = *service.findActorValues(key);
                        active.mProcess = ESM4::ActorValueProcess::Active;
                        service.publishNonPlayerValues(npc, active);
                        EXPECT_EQ(view.getModified(), 54.25f); // Original3, not either ignored request.
                    }
                }
            auto ai = *service.findActorValues(key);
            ai.mValues[33] = {13, {0.5f, -20.f, -1.25f}};
            if (isPlayer)
                service.publishPlayerValues(player, ai, settings);
            else
                service.publishNonPlayerValues(npc, ai);
            request(33, ESM4::ActorValueModifier::Maximum, 2.5f);
            request(33, ESM4::ActorValueModifier::Damage, -2.25f);
            EXPECT_EQ(service.findActorValues(key)->mValues[33].mModifiers,
                (ESM4::ActorValueModifiers{2.5f, -20.f, -2.25f}));
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getAiSetting(MWMechanics::AiSetting::Fight)
                .getModified(), -6);
            const auto before = *service.findActorValues(key);
            const auto life = *service.findActorLife(key);
            for (std::uint8_t invalid : {8, 9, 10, 11, 37, 71, 72, 255})
                EXPECT_THROW(request(invalid, ESM4::ActorValueModifier::Maximum, 2.f), std::invalid_argument);
            EXPECT_THROW(request(4, ESM4::ActorValueModifier::Script, 2.f), std::invalid_argument);
            EXPECT_THROW(request(4, ESM4::ActorValueModifier::Maximum,
                std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
            EXPECT_THROW(request(4, ESM4::ActorValueModifier::Damage,
                std::numeric_limits<float>::infinity()), std::invalid_argument);
            EXPECT_EQ(*service.findActorValues(key), before);
            EXPECT_EQ(*service.findActorLife(key), life);
        }
    }

    TEST_F(OblivionActorStatsTest, queuedStatReadsRetainNativeScriptProcessAndFloatStores)
    {
        MWMechanics::AttributeValue legacy;
        EXPECT_EQ(legacy.getModifiedWithOverrides(1, 2, 5), 0);
        for (auto owner : {ESM4::ActorValueOwner::Player, ESM4::ActorValueOwner::NonPlayer})
            for (auto process : {ESM4::ActorValueProcess::Active, ESM4::ActorValueProcess::Low})
            {
                MWMechanics::AttributeValue view;
                view.setNativeProjection({40, {10.f, 5.f, -2.f}}, owner, process);
                EXPECT_EQ(view.getModifiedWithOverrides(40, 3.5f, 1.25f),
                    owner == ESM4::ActorValueOwner::NonPlayer && process == ESM4::ActorValueProcess::Low ? 43.75f : 47.25f);
                EXPECT_EQ(view.getModifier(), 10);
                EXPECT_EQ(view.getDamage(), 2);
                MWMechanics::AttributeValue copied(view);
                EXPECT_EQ(copied.getModifiedWithOverrides(1, 2, 10),
                    owner == ESM4::ActorValueOwner::NonPlayer && process == ESM4::ActorValueProcess::Low ? -4.f : -2.f);
            }
        MWMechanics::AttributeValue player, npc;
        player.setNativeProjection({16777216, {0.f, 1.f, 0.f}},
            ESM4::ActorValueOwner::Player, ESM4::ActorValueProcess::Active);
        npc.setNativeProjection({16777216, {0.f, 1.f, 0.f}},
            ESM4::ActorValueOwner::NonPlayer, ESM4::ActorValueProcess::Active);
        EXPECT_EQ(player.getModifiedWithOverrides(16777216, -16777216, 0), 1);
        EXPECT_EQ(npc.getModifiedWithOverrides(16777216, -16777216, 0), 0);
    }

    TEST_F(OblivionActorStatsTest, creatureAttributeModifierRequestsRejectNpcSkillAliases)
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
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[8].mBase = 19;
        values.mValues[9].mBase = 17;
        values.mValues[10].mBase = 18;
        values.mValues[12] = {21, {2, 3, -1}};
        MWMechanics::OblivionCombatService service;
        service.publishNonPlayerValues(ptr, values);
        ESM4::RuntimeActorLife life{values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}};
        service.publishNonPlayerLife(ptr, life);
        for (std::uint8_t av = 0; av < 8; ++av)
        {
            service.requestNonPlayerStatModifier(ptr, av, ESM4::ActorValueModifier::Maximum, 7.5f);
            service.requestNonPlayerStatModifier(ptr, av, ESM4::ActorValueModifier::Damage, -1.25f);
            const auto& view = stats.getAttribute(ESM::Attribute::indexToRefId(av));
            EXPECT_EQ(view.getModifier(), 7.5f);
            EXPECT_EQ(view.getDamage(), 1.25f);
            EXPECT_EQ(view.getModified(), values.mValues[av].mBase + 6.25f);
        }
        auto before = *service.findActorValues(values.mActor);
        EXPECT_THROW(service.requestNonPlayerStatModifier(ptr, 14, ESM4::ActorValueModifier::Maximum, 2.f),
            std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(values.mActor), before);
        EXPECT_EQ(*service.findActorLife(values.mActor), life);
        before.mValues[4].mModifiers[0] = 3e38f;
        service.publishNonPlayerValues(ptr, before);
        EXPECT_THROW(service.requestNonPlayerStatModifier(ptr, 4, ESM4::ActorValueModifier::Maximum, -3e38f),
            std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(values.mActor), before);
        EXPECT_EQ(stats.getAttribute(ESM::Attribute::Speed).getModifier(), 3e38f);
        // Actual native LowProcess +278 is RET12: even a finite request that
        // would overflow an active map addition must preserve all storage.
        before.mProcess = ESM4::ActorValueProcess::Low;
        service.publishNonPlayerValues(ptr, before);
        for (std::uint8_t av = 0; av <= 10; ++av)
        {
            EXPECT_NO_THROW(service.changeNonPlayerValue(ptr, av, ESM4::ActorValueModifier::Maximum, 3e38f));
            EXPECT_EQ(*service.findActorValues(values.mActor), before);
            EXPECT_EQ(*service.findActorLife(values.mActor), life);
        }
        EXPECT_THROW(service.changeNonPlayerValue(ptr, 4, ESM4::ActorValueModifier::Maximum,
            std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(values.mActor), before);
    }

    TEST_F(OblivionActorStatsTest, resourceCurrentRequestsPreserveChannelsScalingAndHealthTransactions)
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
        const MWWorld::Ptr npc(&live);
        const auto settings = MWWorld::resolveOblivionPlayerDynamicBaseSettings(mStore);
        MWMechanics::OblivionCombatService service;
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[8] = {100, {10.f, 5.f, -75.f}};
        values.mValues[9] = {100, {10.f, 5.f, -75.f}};
        values.mValues[10] = {100, {10.f, 5.f, -75.f}};
        values.mValues[40].mBase = 15;
        service.publishNonPlayerValues(npc, values);
        service.publishNonPlayerLife(npc, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        const auto npcKey = values.mActor;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{100, 100, 100, 0}};
        service.publishPlayerValues(player, values, settings);
        service.publishPlayerLife(player, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        const auto playerKey = values.mActor;
        for (bool isPlayer : {false, true})
        {
            SCOPED_TRACE(isPlayer);
            const auto key = isPlayer ? playerKey : npcKey;
            const auto ptr = isPlayer ? playerPtr : npc;
            const auto request = [&](std::uint8_t av, float wanted, bool blocked = false, bool essential = false) {
                return isPlayer
                    ? service.requestPlayerResourceCurrent(player, av, wanted, blocked, essential, {10, .3f}, settings)
                    : service.requestNonPlayerResourceCurrent(npc, av, wanted, {true, !blocked}, essential, {10, .3f});
            };
            const auto current = [&](int index) { return ptr.getClass().getCreatureStats(ptr).getDynamic(index).getCurrent(); };
            EXPECT_FLOAT_EQ(current(0), 40);
            // Player scales the raw100 base first:150+10+5-75=90.
            // NPC scales the completed process value:(100+10+5-75)*1.5=60.
            EXPECT_FLOAT_EQ(current(1), isPlayer ? 90 : 60);
            const auto beforeInvalid = *service.findActorValues(key);
            for (float bad : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                     -std::numeric_limits<float>::infinity()})
                EXPECT_THROW(request(8, bad), std::invalid_argument);
            EXPECT_THROW(request(11, 1), std::invalid_argument);
            EXPECT_EQ(*service.findActorValues(key), beforeInvalid);
            EXPECT_TRUE(request(8, 25));
            EXPECT_FLOAT_EQ(current(0), 25);
            EXPECT_EQ(service.findActorValues(key)->mValues[8].mModifiers, (ESM4::ActorValueModifiers{10, 5, -90}));
            EXPECT_TRUE(request(8, 500)); // Native restoration cannot create positive Damage here.
            EXPECT_FLOAT_EQ(current(0), 115);
            EXPECT_EQ(service.findActorValues(key)->mValues[8].mModifiers, (ESM4::ActorValueModifiers{10, 5, isPlayer ? std::optional<float>{0} : std::nullopt}));
            EXPECT_TRUE(request(9, 30));
            EXPECT_FLOAT_EQ(current(1), 30);
            EXPECT_EQ(service.findActorValues(key)->mValues[9].mModifiers,
                (ESM4::ActorValueModifiers{10, 5, isPlayer ? -135.f : -95.f}));
            EXPECT_TRUE(request(10, 5));
            EXPECT_FLOAT_EQ(current(2), 5);
            EXPECT_FALSE(request(10, 0, true));
            EXPECT_FLOAT_EQ(current(2), 5);
            if (isPlayer)
            {
                EXPECT_FALSE(request(8, 0, true));
                EXPECT_FALSE(request(9, 0, true));
                EXPECT_FLOAT_EQ(current(0), 115);
                EXPECT_FLOAT_EQ(current(1), 30);
            }
            EXPECT_TRUE(request(8, 0, false, !isPlayer));
            EXPECT_EQ(service.findActorLife(key)->mPhase, isPlayer
                ? ESM4::ActorLifePhase::Dead : ESM4::ActorLifePhase::EssentialUnconscious);
            EXPECT_FLOAT_EQ(current(0), isPlayer ? 0 : 30);
            EXPECT_EQ(service.findActorLife(key)->mRecoveryRemaining, isPlayer ? 0 : 10);
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).isDead(), isPlayer);
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getKnockedDown(), !isPlayer);
            EXPECT_EQ(service.findActorValues(key)->mValues[8].mModifiers[0], 10);
            EXPECT_EQ(service.findActorValues(key)->mValues[8].mModifiers[1], 5);
        }
        EXPECT_EQ(service.getDeadCount(mActorKey), 0);
        EXPECT_EQ(service.getDeadCount(ESM::FormKey::dynamic("player-base", 1)), 1);
        ASSERT_TRUE(service.takeNextDeathEvent());
        EXPECT_FALSE(service.takeNextDeathEvent());
    }

    TEST_F(OblivionActorStatsTest, resourceBatchRestoresResidentPlayerAndUnloadedActorsAtomically)
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
        const MWWorld::Ptr npc(&live);
        const std::array residents{npc};
        const MWMechanics::OblivionRestorationSettings settings{
            {.1f, 0}, {2, 0}, MWWorld::resolveOblivionPlayerDynamicBaseSettings(mStore)};
        MWMechanics::OblivionCombatService service;
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[8] = {100, {std::nullopt, std::nullopt, -75.f}};
        values.mValues[9] = {1000, {std::nullopt, std::nullopt, -900.f}};
        values.mValues[10] = {100, {std::nullopt, std::nullopt, -90.f}};
        service.publishNonPlayerValues(npc, values);
        service.publishNonPlayerLife(npc, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        const auto npcKey = values.mActor;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{100, 1000, 100, 0}};
        service.publishPlayerValues(player, values, settings.mPlayerBase);
        service.publishPlayerLife(player, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        const auto playerKey = values.mActor;
        ESM4::RuntimeState before;
        before.mProfile = ESM::GameProfile::Oblivion;
        service.capture(before);
        const auto unloadedKey = ESM::FormKey::content("actors.esm", 0x903);
        mStore.getWritable<ESM4::ActorCharacter>().insertStatic(reference, npcKey);
        auto unloadedReference = reference;
        unloadedReference.mId = {0x903, 3};
        unloadedReference.mFormKey = unloadedKey;
        mStore.getWritable<ESM4::ActorCharacter>().insertStatic(unloadedReference, unloadedKey);
        values = *service.findActorValues(npcKey);
        values.mActor = unloadedKey;
        values.mProcess = ESM4::ActorValueProcess::Low;
        before.mNativeActorValues.push_back(values);
        before.mNativeActorLife.push_back({values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        before.mPlayer.mReference = playerKey;
        before.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        before.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        before.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        for (const auto& key : {npcKey, unloadedKey})
        {
            ESM4::RuntimeReferenceState referenceState;
            referenceState.mKey = key;
            referenceState.mBase = mActorKey;
            referenceState.mCell = before.mPlayer.mCell;
            before.mReferences.push_back(referenceState);
        }
        service.restore(before);
        service.capture(before); // Canonical identity order.

        const std::vector<MWMechanics::OblivionActorRestoration> updates{
            {npcKey, {{120, false, false}, {2, true, false}}},
            {unloadedKey, {{120, false, false}}},
            {playerKey, {{3600, true, false}}}};
        service.restoreResourceBatch(player, updates, residents, settings, 120.f);
        EXPECT_FLOAT_EQ(service.getNonPlayerValue(npc, 8), 100);
        EXPECT_FLOAT_EQ(service.getNonPlayerValue(npc, 9), 222);
        EXPECT_FLOAT_EQ(service.getNonPlayerValue(unloadedKey, 8, mStore), 25);
        EXPECT_FLOAT_EQ(service.getNonPlayerValue(unloadedKey, 9, mStore), 220);
        EXPECT_FLOAT_EQ(service.getPlayerValue(8), 100);
        EXPECT_FLOAT_EQ(service.getPlayerValue(9), 1000);
        EXPECT_FLOAT_EQ(npc.getClass().getCreatureStats(npc).getMagicka().getCurrent(), 222);
        EXPECT_FLOAT_EQ(playerPtr.getClass().getCreatureStats(playerPtr).getMagicka().getCurrent(), 1000);
        auto after = before;
        after.mProfile = ESM::GameProfile::Oblivion;
        service.capture(after);
        EXPECT_EQ(after.mNativeActorLife, before.mNativeActorLife);
        EXPECT_EQ(after.mNativeActorManagerTime, 120.f);
        for (const auto& key : {npcKey, unloadedKey, playerKey})
            EXPECT_EQ(after.mNativeActorUpdateTimes.at(key), 120.f);
        MWMechanics::OblivionCombatService restored;
        restored.restore(after);
        EXPECT_FLOAT_EQ(restored.getNonPlayerValue(unloadedKey, 9, mStore), 220);

        // Fail after earlier candidates have already been computed. Neither the
        // owned state nor any resident projection may expose partial restoration.
        for (int failure = 0; failure != 9; ++failure)
        {
            SCOPED_TRACE(failure);
            auto invalid = updates;
            float completedTime = 240.f;
            auto invalidResidents = std::vector<MWWorld::Ptr>{npc};
            if (failure == 0)
                invalid.back().mUpdates.back().mDuration = std::numeric_limits<float>::quiet_NaN();
            else if (failure == 1)
                invalid.push_back(updates.front());
            else if (failure == 2)
                invalid.back().mActor = ESM::FormKey::content("actors.esm", 0x999);
            else if (failure == 3)
                invalidResidents.push_back(npc);
            else if (failure == 4)
                invalidResidents.push_back(MWWorld::Ptr{});
            else if (failure == 5)
                invalid.front().mUpdates.clear();
            else if (failure == 6)
                completedTime = std::numeric_limits<float>::quiet_NaN();
            else if (failure == 7)
                completedTime = std::numeric_limits<float>::infinity();
            else
                completedTime = 100001.f;
            EXPECT_THROW(service.restoreResourceBatch(player, invalid, invalidResidents, settings, completedTime), std::exception);
            auto result = after;
            result.mProfile = ESM::GameProfile::Oblivion;
            service.capture(result);
            EXPECT_EQ(result.mNativeActorValues, after.mNativeActorValues);
            EXPECT_EQ(result.mNativeActorLife, after.mNativeActorLife);
            EXPECT_EQ(result.mNativeActorManagerTime, after.mNativeActorManagerTime);
            EXPECT_EQ(result.mNativeActorUpdateTimes, after.mNativeActorUpdateTimes);
            EXPECT_FLOAT_EQ(npc.getClass().getCreatureStats(npc).getMagicka().getCurrent(), 222);
            EXPECT_FLOAT_EQ(playerPtr.getClass().getCreatureStats(playerPtr).getMagicka().getCurrent(), 1000);
        }
        // Retain separate float stores: two 0.00003 increments each round
        // away at Damage=-900; one combined 0.00006 increment would not.
        service.restore(before);
        service.publishNonPlayerValues(npc, *service.findActorValues(npcKey));
        const std::array tinyUpdates{MWMechanics::OblivionActorRestoration{
            npcKey, {{.00003f, false, false}, {.00003f, false, false}}}};
        service.restoreResourceBatch(player, tinyUpdates, residents, settings);
        auto withoutClock = before;
        service.capture(withoutClock);
        EXPECT_EQ(withoutClock.mNativeActorManagerTime, before.mNativeActorManagerTime);
        EXPECT_EQ(withoutClock.mNativeActorUpdateTimes, before.mNativeActorUpdateTimes);
        EXPECT_EQ(service.findActorValues(npcKey)->mValues[9].mModifiers[2], -900.f);
        EXPECT_FLOAT_EQ(npc.getClass().getCreatureStats(npc).getMagicka().getCurrent(), 100);

        // Terminal lifecycle suppresses restoration even with positive stored
        // Health. No HP-based inference may resurrect an unloaded actor.
        auto terminal = before;
        for (auto& life : terminal.mNativeActorLife)
            life.mPhase = ESM4::ActorLifePhase::Dead;
        service.restore(terminal);
        service.restoreResourceBatch(player, updates, residents, settings);
        auto terminalResult = terminal;
        service.capture(terminalResult);
        EXPECT_EQ(terminalResult.mNativeActorValues, terminal.mNativeActorValues);
        EXPECT_EQ(terminalResult.mNativeActorLife, terminal.mNativeActorLife);
        EXPECT_TRUE(npc.getClass().getCreatureStats(npc).isDead());
        EXPECT_TRUE(playerPtr.getClass().getCreatureStats(playerPtr).isDead());

        auto missingLife = before;
        missingLife.mNativeActorLife.clear();
        service.restore(missingLife);
        EXPECT_THROW(service.restoreResourceBatch(player, updates, residents, settings), std::invalid_argument);
        auto missingResult = missingLife;
        service.capture(missingResult);
        EXPECT_EQ(missingResult, missingLife);
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

            // Health uses sparse Damage; Magicka/Fatigue have permanent zero slots.
            for (auto damage : {std::optional<float>{}, std::optional<float>{0}})
            {
                for (int av : {8, 9, 10})
                    values.mValues[av].mModifiers[2] = damage;
                publish();
                restore({1, true, false}, settings);
                const bool insertsPositive = !isPlayer && !damage;
                EXPECT_EQ(stats.getHealth().getCurrent(), insertsPositive ? 102 : 101);
                EXPECT_FLOAT_EQ(stats.getMagicka().getCurrent(), 101.f);
                EXPECT_EQ(stats.getFatigue().getCurrent(), 41);
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

    TEST_F(OblivionActorStatsTest, freshNativeValuesResolveWinningFormsAndPermanentSlotsWithoutPublishing)
    {
        autoNpc();
        mNpc.mFormKey = mActorKey;
        mNpc.mAIData = {13, 27, 39, 101, 0, 0, 0, 0};
        mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        const auto actor = ESM::FormKey::content("actors.esm", 0x900);
        const auto values = MWWorld::resolveOblivionInitialNonPlayerValues(
            mStore, actor, mActorKey, 3, ESM4::ActorValueProcess::Low);
        EXPECT_EQ(values.mActor, actor);
        EXPECT_EQ(values.mBase, mActorKey);
        EXPECT_EQ(values.mOwner, ESM4::ActorValueOwner::NonPlayer);
        EXPECT_EQ(values.mProcess, ESM4::ActorValueProcess::Low);
        EXPECT_EQ(values.mValues[0].mBase, 44);
        EXPECT_EQ(values.mValues[8].mBase, 30);
        EXPECT_EQ(values.mNonPlayerFormHealth, 30);
        EXPECT_EQ(values.mValues[9].mBase, 70);
        EXPECT_EQ(values.mValues[10].mBase, 258);
        EXPECT_EQ(values.mValues[12].mBase, 26);
        EXPECT_EQ(values.mValues[28].mBase, 26);
        const std::array<float, 4> ai{13, 27, 39, 101};
        for (std::size_t av = 0; av < values.mValues.size(); ++av)
        {
            if (av >= 33 && av <= 36)
            {
                EXPECT_EQ(values.mValues[av].mBase, ai[av - 33]);
            }
            if (av == 11 || av >= 37)
            {
                EXPECT_EQ(values.mValues[av].mBase, 0);
            }
            for (std::size_t channel = 0; channel < values.mValues[av].mModifiers.size(); ++channel)
                EXPECT_EQ(values.mValues[av].mModifiers[channel],
                    (av == 9 || av == 10) && channel != 0 ? std::optional(0.f) : std::nullopt);
        }
        const std::uint32_t authoredHealth = mStore.search<ESM4::Npc>(mActorKey)->mData.health;
        EXPECT_EQ(authoredHealth, 0);
        EXPECT_THROW(MWWorld::resolveOblivionInitialNonPlayerValues(
            mStore, {}, mActorKey, 3, ESM4::ActorValueProcess::Active), std::invalid_argument);
        EXPECT_THROW(MWWorld::resolveOblivionInitialNonPlayerValues(mStore,
            ESM::FormKey::content("Oblivion.esm", 0x14), mActorKey, 3, ESM4::ActorValueProcess::Active),
            std::invalid_argument);
        EXPECT_THROW(MWWorld::resolveOblivionInitialNonPlayerValues(
            mStore, actor, mActorKey, 3, static_cast<ESM4::ActorValueProcess>(255)), std::runtime_error);
        mNpc.mBaseConfig.tes4.flags = 0;
        mNpc.mBaseConfig.tes4.levelOrOffset = 2;
        mNpc.mData.health = 16777217;
        mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        const auto fixed = MWWorld::resolveOblivionInitialNonPlayerValues(
            mStore, actor, mActorKey, {}, ESM4::ActorValueProcess::Active);
        EXPECT_EQ(fixed.mValues[8].mBase, 16777216.f);
        EXPECT_EQ(fixed.mNonPlayerFormHealth, 16777217);
        for (const auto av : {9, 10})
            EXPECT_EQ(fixed.mValues[av].mModifiers, (ESM4::ActorValueModifiers{0.f, 0.f, 0.f}));

        const auto creatureKey = ESM::FormKey::content("actors.esm", 0x801);
        ESM4::Creature creature{};
        creature.mId = {0x801, 3};
        creature.mFormKey = creatureKey;
        creature.mAttackReach = 64;
        creature.mBaseConfig.tes4.levelOrOffset = 2;
        creature.mData.health = 99;
        creature.mData.combat = 10;
        creature.mData.magic = 20;
        creature.mData.stealth = 30;
        creature.mAIData = mNpc.mAIData;
        mStore.getWritable<ESM4::Creature>().insertStatic(creature, creatureKey);
        const auto initialCreature = MWWorld::resolveOblivionInitialNonPlayerValues(
            mStore, actor, creatureKey, {}, ESM4::ActorValueProcess::Active);
        EXPECT_EQ(initialCreature.mNonPlayerFormHealth, 99);
        EXPECT_EQ(initialCreature.mValues[12].mBase, 10);
        EXPECT_EQ(initialCreature.mValues[19].mBase, 20);
        EXPECT_EQ(initialCreature.mValues[28].mBase, 30); // Form getter; runtime aliases are separate.
        EXPECT_EQ(initialCreature.mValues[36].mBase, 101);
        const auto lowCreature = MWWorld::resolveOblivionInitialNonPlayerValues(
            mStore, actor, creatureKey, {}, ESM4::ActorValueProcess::Low);
        for (const auto av : {9, 10})
            EXPECT_EQ(lowCreature.mValues[av].mModifiers,
                (ESM4::ActorValueModifiers{std::nullopt, 0.f, 0.f}));
    }

    TEST_F(OblivionActorStatsTest, nonPlayerRawHealthRetainsFormPrecisionAcrossQueriesWritesAndRestart)
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
        ESM4::ActorCharacter reference{};
        reference.mId = {0x900, 3};
        reference.mFormKey = ESM::FormKey::content("actors.esm", 0x900);
        reference.mBaseKey = mActorKey;
        mStore.getWritable<ESM4::ActorCharacter>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Npc> live(reference, mStore.search<ESM4::Npc>(mActorKey));
        MWWorld::Ptr ptr(&live);
        MWMechanics::OblivionCombatService service;
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mNonPlayerFormHealth = 16777217;
        values.mValues[8] = {16777216.f, {.5f, .75f, -.25f}};
        values.mValues[9].mBase = 20;
        values.mValues[10].mBase = 40;
        for (const auto process : {ESM4::ActorValueProcess::Low, ESM4::ActorValueProcess::Active})
        {
            values.mProcess = process;
            service.publishNonPlayerValues(ptr, values);
            // Recorded original form/process outputs, not a float-base query.
            EXPECT_EQ(service.getNonPlayerValue(ptr, 8), 16777218.f);
            EXPECT_EQ(service.getNonPlayerIntegerValue(ptr, 8), 16777217);
            EXPECT_EQ(service.getNonPlayerBaseValue(values.mActor, 8, mStore), 16777216);
            EXPECT_EQ(service.getScriptActorValue(values.mActor, 8, false, true, mStore), 16777217);
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 16777218.f);
        }
        ESM4::RuntimeState saved;
        ESM4::RuntimeReferenceState savedRef;
        saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 0x100);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        savedRef.mKey = values.mActor;
        savedRef.mBase = values.mBase;
        savedRef.mCell = ESM::FormKey::content("actors.esm", 0x100);
        saved.mReferences.push_back(savedRef);
        service.capture(saved);
        const auto restored = ESM4::RuntimeState::deserializeBinary(saved.serializeBinary());
        service.clear();
        service.restore(restored, mStore);
        service.publishNonPlayerValues(ptr, *service.findActorValues(values.mActor));
        EXPECT_EQ(service.getNonPlayerIntegerValue(values.mActor, 8, mStore), 16777217);
        EXPECT_EQ(service.getNonPlayerValue(values.mActor, 8, mStore), 16777218.f);
        auto older = saved;
        older.mVersion = 16;
        EXPECT_THROW(service.capture(older), std::invalid_argument);
        EXPECT_EQ(older.mNativeActorValues, saved.mNativeActorValues);
        const std::array residents{ptr};
        service.setNonPlayerBaseValue(ptr, 8, 16777219, residents);
        EXPECT_EQ(service.findActorValues(values.mActor)->mNonPlayerFormHealth, 16777219);
        EXPECT_EQ(service.getNonPlayerIntegerValue(ptr, 8), 16777219);
        EXPECT_EQ(service.getScriptActorValue(values.mActor, 8, false, true, mStore), 16777219);

        values.mNonPlayerFormHealth = std::numeric_limits<std::int32_t>::max();
        values.mValues[8] = {2147483648.f, {}};
        MWMechanics::OblivionCombatService boundary;
        boundary.publishNonPlayerValues(ptr, values);
        EXPECT_EQ(boundary.getNonPlayerValue(ptr, 8), 2147483648.f);
        EXPECT_EQ(boundary.getNonPlayerIntegerValue(ptr, 8), std::numeric_limits<std::int32_t>::max());
        EXPECT_EQ(boundary.getNonPlayerBaseValue(values.mActor, 8, mStore), std::numeric_limits<std::int32_t>::min());
        EXPECT_EQ(boundary.getScriptActorValue(values.mActor, 8, false, true, mStore), 2147483647.);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getModified(), -2147483648.f);

        // Legacy absence keeps the old rounded interpretation and is not
        // silently reconstructed from today's winning content or a float.
        values.mNonPlayerFormHealth.reset();
        values.mValues[8] = {16777216.f, {.5f, .75f, -.25f}};
        MWMechanics::OblivionCombatService legacy;
        legacy.publishNonPlayerValues(ptr, values);
        EXPECT_EQ(legacy.getNonPlayerIntegerValue(ptr, 8), 16777216);
        EXPECT_EQ(legacy.getNonPlayerValue(ptr, 8), 16777216.f);
        const auto before = *legacy.findActorValues(values.mActor);
        auto conflict = values;
        conflict.mNonPlayerFormHealth = 16777219;
        EXPECT_THROW(legacy.publishNonPlayerValues(ptr, conflict), std::runtime_error);
        EXPECT_EQ(*legacy.findActorValues(values.mActor), before);
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
        auto aliveLife = life;
        aliveLife.mPhase = ESM4::ActorLifePhase::Alive;
        EXPECT_TRUE(service.transitionNonPlayerLife(ptr, aliveLife));
        EXPECT_TRUE(service.killNonPlayer(ptr, {}, false, {}));
        EXPECT_EQ(service.getDeadCount(values.mBase), 1);
        EXPECT_FALSE(service.transitionNonPlayerLife(ptr, life));
        const auto deathEvent = service.takeNextDeathEvent();
        ASSERT_TRUE(deathEvent);
        EXPECT_EQ(deathEvent->mId, 1);
        EXPECT_EQ(deathEvent->mActor, values.mActor);
        EXPECT_FALSE(service.takeNextDeathEvent());
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
        EXPECT_TRUE(restored.transitionNonPlayerLife(newPtr, aliveLife));
        const auto beforeReaction = *restored.findActorValues(values.mActor);
        EXPECT_THROW(restored.reactNonPlayerHealth(newPtr, {}, true,
            {10, std::numeric_limits<float>::infinity()}), std::invalid_argument);
        EXPECT_EQ(*restored.findActorValues(values.mActor), beforeReaction);
        EXPECT_FALSE(newPtr.getClass().getCreatureStats(newPtr).isDead());
        EXPECT_TRUE(restored.reactNonPlayerHealth(newPtr, saved.mPlayer.mReference, true, {3.125f, .3f}));
        EXPECT_EQ(newPtr.getClass().getCreatureStats(newPtr).getHealth().getCurrent(), 30);
        EXPECT_TRUE(newPtr.getClass().getCreatureStats(newPtr).getKnockedDown());
        EXPECT_EQ(restored.findActorLife(values.mActor)->mRecoveryRemaining, 3.125f);
        EXPECT_FALSE(restored.takeNextDeathEvent());
        restored.changeNonPlayerValue(newPtr, 8, ESM4::ActorValueModifier::Damage, -31);
        EXPECT_FALSE(restored.reactNonPlayerHealth(newPtr, {}, true, {10, 1}));
        EXPECT_TRUE(restored.transitionNonPlayerLife(newPtr, aliveLife));
        EXPECT_TRUE(restored.reactNonPlayerHealth(newPtr, saved.mPlayer.mReference, false, {}));
        EXPECT_TRUE(newPtr.getClass().getCreatureStats(newPtr).isDead());
        EXPECT_EQ(newPtr.getClass().getCreatureStats(newPtr).getHealth().getCurrent(), -1);
        const auto reactionEvent = restored.takeNextDeathEvent();
        ASSERT_TRUE(reactionEvent);
        EXPECT_EQ(reactionEvent->mId, 2);
        EXPECT_EQ(reactionEvent->mKiller, saved.mPlayer.mReference);
        EXPECT_TRUE(restored.transitionNonPlayerLife(newPtr, aliveLife));
        auto scriptDeficit = *restored.findActorValues(values.mActor);
        scriptDeficit.mValues[8] = {100, {std::nullopt, -200, std::nullopt}};
        restored.publishNonPlayerValues(newPtr, scriptDeficit);
        EXPECT_EQ(restored.getNonPlayerValue(newPtr, 8), -100);
        EXPECT_TRUE(restored.reactNonPlayerHealth(newPtr, {}, true, {10, .3f}));
        // Unlike the player array, an absent nonplayer sparse Damage entry
        // permits the initial positive value. Keep that native distinction.
        EXPECT_EQ(restored.findActorValues(values.mActor)->mValues[8].mModifiers[2], 130);
        EXPECT_EQ(restored.getNonPlayerValue(newPtr, 8), 30);


    }

    TEST_F(OblivionActorStatsTest, unloadedCommandsCommitValuesLifeAndResidentSharedBaseAtomically)
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
        MWMechanics::OblivionCombatService service;
        std::array<ESM4::ActorCharacter, 2> refs{};
        std::array<std::unique_ptr<MWWorld::LiveCellRef<ESM4::Npc>>, 2> lives;
        std::array<MWWorld::Ptr, 2> ptrs;
        for (std::size_t i = 0; i < refs.size(); ++i)
        {
            auto& ref = refs[i];
            ref.mId = {static_cast<std::uint32_t>(0x900 + i), 3};
            ref.mFormKey = ESM::FormKey::content("actors.esm", 0x900 + i);
            ref.mBaseKey = mActorKey;
            mStore.getWritable<ESM4::ActorCharacter>().insertStatic(ref, ref.mFormKey);
            lives[i] = std::make_unique<MWWorld::LiveCellRef<ESM4::Npc>>(ref, mStore.search<ESM4::Npc>(mActorKey));
            ptrs[i] = MWWorld::Ptr(lives[i].get());
            ESM4::RuntimeActorValues values;
            values.mActor = ref.mFormKey;
            values.mBase = mActorKey;
            values.mValues[8].mBase = 100;
            values.mValues[9].mBase = 50;
            values.mValues[10].mBase = 40;
            service.publishNonPlayerValues(ptrs[i], values);
            ESM4::RuntimeActorLife life;
            life.mActor = values.mActor;
            life.mBase = values.mBase;
            service.publishNonPlayerLife(ptrs[i], life);
        }
        using Command = ESM4::ActorValueCommand;
        using Source = ESM4::ActorValueCommandSource;
        const auto target = refs[0].mFormKey;
        const auto run = [&](std::uint8_t av, Command command, Source source, int requested,
                             std::span<const MWWorld::Ptr> residents = {}) {
            return service.executeUnloadedValueCommand(target, mStore, av, command, source,
                requested, {false, true}, residents);
        };
        EXPECT_THROW(run(8, Command::Mod, Source::Script, -1, ptrs), std::invalid_argument);
        ptrs[0] = {};
        lives[0].reset(); // The target really has no live actor or shared stats.
        EXPECT_TRUE(run(8, Command::Mod, Source::Script, -25).mAccepted);
        EXPECT_EQ(service.getNonPlayerValue(target, 8, mStore), 75);
        EXPECT_EQ(run(8, Command::Force, Source::Console, 50).mHealthReactionDelta, -25);
        EXPECT_EQ(service.getNonPlayerValue(target, 8, mStore), 50);
        const auto before = *service.findActorValues(target);
        const auto lifeBefore = *service.findActorLife(target);
        EXPECT_THROW(run(8, static_cast<Command>(255), Source::Script, 0), std::invalid_argument);
        EXPECT_THROW(run(8, Command::Set, static_cast<Source>(255), 0), std::invalid_argument);
        const std::array duplicate{ptrs[1], ptrs[1]};
        EXPECT_THROW(run(8, Command::Set, Source::Script, 200, duplicate), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(target), before);
        EXPECT_EQ(*service.findActorLife(target), lifeBefore);
        EXPECT_EQ(ptrs[1].getClass().getCreatureStats(ptrs[1]).getHealth().getCurrent(), 100);
        const std::array residents{ptrs[1]};
        EXPECT_TRUE(run(8, Command::Set, Source::Script, 200, residents).mAccepted);
        EXPECT_EQ(service.getNonPlayerValue(target, 8, mStore), 150);
        EXPECT_EQ(ptrs[1].getClass().getCreatureStats(ptrs[1]).getHealth().getCurrent(), 200);
        EXPECT_TRUE(service.engage(target, refs[1].mFormKey));
        EXPECT_TRUE(run(8, Command::Mod, Source::Script, -200).mAccepted);
        EXPECT_EQ(service.findActorLife(target)->mPhase, ESM4::ActorLifePhase::Dead);
        EXPECT_FALSE(service.isInCombat(target));
        EXPECT_FALSE(service.isInCombat(refs[1].mFormKey));
        EXPECT_EQ(service.getDeadCount(mActorKey), 1);
        EXPECT_TRUE(run(8, Command::Mod, Source::Script, -1).mAccepted);
        EXPECT_EQ(service.getDeadCount(mActorKey), 1);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        for (const auto& ref : refs)
        {
            ESM4::RuntimeReferenceState state;
            state.mKey = ref.mFormKey;
            state.mBase = ref.mBaseKey;
            state.mCell = saved.mPlayer.mCell;
            saved.mReferences.push_back(state);
        }
        service.capture(saved);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()), mStore);
        EXPECT_EQ(restored.getNonPlayerValue(target, 8, mStore), -51);
        EXPECT_EQ(restored.getDeadCount(mActorKey), 1);
        const auto event = restored.takeNextDeathEvent();
        ASSERT_TRUE(event);
        EXPECT_FALSE(restored.takeNextDeathEvent());
        EXPECT_EQ(service.takeNextDeathEvent(), event);
        EXPECT_FALSE(service.takeNextDeathEvent());
        // Later residency publishes the same committed values and terminal life.
        lives[0] = std::make_unique<MWWorld::LiveCellRef<ESM4::Npc>>(refs[0], mStore.search<ESM4::Npc>(mActorKey));
        ptrs[0] = MWWorld::Ptr(lives[0].get());
        service.publishNonPlayerValues(ptrs[0], *service.findActorValues(target));
        EXPECT_EQ(ptrs[0].getClass().getCreatureStats(ptrs[0]).getHealth().getCurrent(), -51);
        EXPECT_TRUE(ptrs[0].getClass().getCreatureStats(ptrs[0]).isDead());
        // A missing or changed winning reference cannot mutate the restored actor.
        const auto prior = *restored.findActorValues(target);
        refs[0].mBaseKey = ESM::FormKey::content("actors.esm", 0x999);
        mStore.getWritable<ESM4::ActorCharacter>().insertStatic(refs[0], target);
        EXPECT_THROW(restored.executeUnloadedValueCommand(target, mStore, 8, Command::Mod,
            Source::Script, 1, {}, {}), std::invalid_argument);
        EXPECT_EQ(*restored.findActorValues(target), prior);
    }

    TEST_F(OblivionActorStatsTest, sharedBaseRosterRefreshesDisabledAndDeletedResidentProjections)
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
        auto& cell = model.getDraftCell();
        MWMechanics::OblivionCombatService service;
        std::array<MWWorld::Ptr, 3> ptrs;
        for (std::size_t i = 0; i < ptrs.size(); ++i)
        {
            ESM4::ActorCharacter reference{};
            reference.mId = {static_cast<std::uint32_t>(0x980 + i), 3};
            reference.mFormKey = ESM::FormKey::content("actors.esm", 0x980 + i);
            reference.mBaseKey = mActorKey;
            mStore.getWritable<ESM4::ActorCharacter>().insertStatic(reference, reference.mFormKey);
            MWWorld::LiveCellRef<ESM4::Npc> live(reference, mStore.search<ESM4::Npc>(mActorKey));
            if (i == 1)
                live.mData.disable();
            if (i == 2)
                live.mData.setDeletedByContentFile(true);
            ptrs[i] = MWWorld::Ptr(cell.insert(&live), &cell);
            ESM4::RuntimeActorValues values;
            values.mActor = reference.mFormKey;
            values.mBase = mActorKey;
            values.mValues[0] = {30, {1, 2, -3}};
            values.mValues[8].mBase = 100;
            values.mValues[9].mBase = 50;
            values.mValues[10].mBase = 40;
            service.publishNonPlayerValues(ptrs[i], values);
        }
        model.registerPtr(ptrs[0]);
        ASSERT_TRUE(model.getPtr(ptrs[1].getCellRef().getRefNum()).isEmpty());
        ASSERT_TRUE(model.getPtr(ptrs[2].getCellRef().getRefNum()).isEmpty());
        // Normal rendering/script iteration can prime the cache first. A
        // later transaction must still discover content-deleted residents.
        std::size_t accessible = 0;
        cell.forEachConst([&](const MWWorld::ConstPtr&) { ++accessible; return true; });
        ASSERT_EQ(accessible, 2);
        const auto residents = model.getResidentPtrs();
        ASSERT_EQ(residents.size(), 3);
        // Once upgraded, ordinary visitors must still hide deleted entries.
        accessible = 0;
        cell.forEachConst([&](const MWWorld::ConstPtr&) { ++accessible; return true; });
        EXPECT_EQ(accessible, 2);
        accessible = 0;
        cell.forEachType<ESM4::Npc>([&](const MWWorld::Ptr&) { ++accessible; return true; });
        EXPECT_EQ(accessible, 2);
        accessible = 0;
        cell.forEachType<ESM4::Npc>([&](const MWWorld::Ptr&) { ++accessible; return true; }, true);
        EXPECT_EQ(accessible, 3);
        service.setNonPlayerBaseValue(ptrs[0], 0, 257, residents);
        for (const auto& ptr : ptrs)
        {
            const auto* values = service.findActorValues(ptr.getCellRef().getFormKey());
            ASSERT_NE(values, nullptr);
            EXPECT_EQ(values->mValues[0].mBase, 1);
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getAttribute(ESM::Attribute::Strength).getBase(), 1);
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getAttribute(ESM::Attribute::Strength).getModified(), 1);
        }
        EXPECT_FALSE(ptrs[1].getRefData().isEnabled());
        EXPECT_TRUE(ptrs[2].mRef->isDeleted());
        EXPECT_TRUE(model.getPtr(ptrs[1].getCellRef().getRefNum()).isEmpty());
        EXPECT_TRUE(model.getPtr(ptrs[2].getCellRef().getRefNum()).isEmpty());
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
        service.publishPlayerLife(player, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
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
        service.publishNonPlayerLife(ptr, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
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
        EXPECT_EQ(service.getNonPlayerValue(ptr, 10), 40); // Permanent Fatigue Damage clamps positive writes.
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

    TEST_F(OblivionActorStatsTest, playerLifeAdoptionRollsBackCommitsAndPreservesExistingAuthority)
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
        values.mPlayerFormValues = {{100, 20, 40, 0}};
        MWMechanics::OblivionCombatService service;
        service.publishPlayerValues(player, values, {});
        const auto before = *service.findActorValues(values.mActor);
        auto& stats = ptr.getClass().getCreatureStats(ptr);
        ESM4::RuntimeActorLife life{values.mActor, values.mBase, ESM4::ActorLifePhase::Dead, 0, {}};
        EXPECT_THROW(service.guardLifeAdoption({}, &player), std::invalid_argument);
        {
            auto adoption = service.guardLifeAdoption(ptr, &player);
            service.publishPlayerLife(player, life);
            EXPECT_TRUE(stats.isDead());
        }
        EXPECT_EQ(service.findActorLife(values.mActor), nullptr);
        EXPECT_FALSE(stats.isDead());
        EXPECT_EQ(*service.findActorValues(values.mActor), before);
        life.mPhase = ESM4::ActorLifePhase::Alive;
        {
            auto adoption = service.guardLifeAdoption(ptr, &player);
            service.publishPlayerLife(player, life);
            adoption.commit();
        }
        ASSERT_NE(service.findActorLife(values.mActor), nullptr);
        EXPECT_EQ(*service.findActorLife(values.mActor), life);
        {
            auto adoption = service.guardLifeAdoption(ptr, &player); // Existing life is not initial adoption.
            life.mPhase = ESM4::ActorLifePhase::Dead;
            service.publishPlayerLife(player, life);
        }
        EXPECT_EQ(*service.findActorLife(values.mActor), life);
        EXPECT_TRUE(stats.isDead());
        EXPECT_EQ(*service.findActorValues(values.mActor), before);
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
        EXPECT_EQ(restored.findActorValues(values.mActor)->mValues[10].mModifiers[2], 0);
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

    TEST_F(OblivionActorStatsTest, fatigueKnockoutRespectsNativeProjectionAndLegacyZeroPool)
    {
        sharedStats();
        MWBase::Environment environment;
        environment.setESMStore(mStore);
        for (const float base : {0.f, 100.f})
        for (const float maximum : {0.f, 100.f})
        for (const float current : {-1.f, -std::numeric_limits<float>::denorm_min(), -0.f, 0.f, 1.f})
        {
            SCOPED_TRACE(base);
            SCOPED_TRACE(maximum);
            SCOPED_TRACE(current);
            MWMechanics::CreatureStats legacy;
            legacy.setFatigue(MWMechanics::DynamicStat<float>(base, maximum, current));
            const auto before = legacy.getFatigue();
            EXPECT_EQ(legacy.isFatigueKnockedOut(), current < 0 || base == 0);
            EXPECT_EQ(legacy.getFatigue(), before);
            for (const auto owner : {ESM4::ActorValueOwner::Player, ESM4::ActorValueOwner::NonPlayer})
            for (const auto process : {ESM4::ActorValueProcess::Low, ESM4::ActorValueProcess::Active})
            {
                MWMechanics::CreatureStats native;
                MWMechanics::OblivionActorProjectionInput input;
                input.mOwner = owner;
                input.mProcess = process;
                input.mDynamic = {{{100, 100, 100}, {0, 0, 0}, {base, maximum, current}}};
                input.mLife = ESM4::ActorLifePhase::Alive;
                MWMechanics::OblivionActorProjection prepared(native, input);
                ASSERT_TRUE(prepared.commit());
                const auto projected = native.getFatigue();
                ASSERT_TRUE(projected.isNativeProjection());
                EXPECT_EQ(native.isFatigueKnockedOut(), current < 0);
                EXPECT_EQ(native.getFatigue(), projected);
                EXPECT_FALSE(native.getKnockedDown());
                EXPECT_FALSE(native.isDead());
                // Essential unconsciousness remains an independent reaction.
                input.mLife = ESM4::ActorLifePhase::EssentialUnconscious;
                MWMechanics::OblivionActorProjection unconscious(native, input);
                ASSERT_TRUE(unconscious.commit());
                EXPECT_TRUE(native.getKnockedDown());
                EXPECT_EQ(native.isFatigueKnockedOut(), current < 0);
                EXPECT_EQ(native.getFatigue(), projected);
            }
        }
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

namespace
{
    TEST_F(OblivionActorStatsTest, luaNativeBaseCacheNormalizesFieldsWithoutPublishingEarly)
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
        model.registerPtr(ptr);
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[0] = {40, {7, 13, -3}};
        values.mValues[14] = {30, {5, 17, -2}};
        values.mValues[8].mBase = 100;
        values.mValues[9].mBase = 30;
        values.mValues[10].mBase = 40;
        MWMechanics::OblivionCombatService service;
        VFS::Manager vfs;
        LuaUtil::ScriptsConfiguration configuration;
        LuaUtil::LuaState luaState(&vfs, &configuration);
        MWLua::LuaManager manager(&vfs, {});
        MWLua::Context context{MWLua::Context::Local};
        context.mLuaManager = &manager;
        context.mLua = &luaState;
        sol::state_view lua = luaState.unsafeState();
        sol::table actor(lua, sol::create), npc(lua, sol::create);
        MWLua::addActorStatsBindings(actor, context);
        npc["baseType"] = actor;
        MWLua::addNpcStatsBindings(npc, context);
        lua["Actor"] = actor;
        lua["NPC"] = npc;
        lua.new_usertype<MWLua::SelfObject>("SelfObject", sol::no_constructor);
        MWLua::SelfObject self{MWLua::LObject(ptr)};
        lua["target"] = &self;
        for (auto process : {ESM4::ActorValueProcess::Active, ESM4::ActorValueProcess::Low})
        {
            values.mProcess = process;
            service.publishNonPlayerValues(ptr, values);
            auto result = lua.safe_script(
                "local attribute = Actor.stats.attributes.strength(target); "
                "local skill = NPC.stats.skills.longblade(target); "
                "attribute.base = -1.75; skill.base = 257.75; "
                "return attribute.base, skill.base, attribute.modified, skill.modified",
                sol::script_pass_on_error);
            ASSERT_TRUE(result.valid()) << sol::error(result).what();
            EXPECT_EQ(result.get<float>(0), 255.f);
            EXPECT_EQ(result.get<float>(1), 1.f);
            EXPECT_EQ(result.get<float>(2), process == ESM4::ActorValueProcess::Active ? 272.f : 265.f);
            EXPECT_EQ(result.get<float>(3), process == ESM4::ActorValueProcess::Active ? 21.f : 16.f);
            EXPECT_EQ(*service.findActorValues(values.mActor), values);
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getAttribute(ESM::Attribute::Strength).getBase(), 40.f);
            EXPECT_EQ(ptr.getClass().getNpcStats(ptr).getSkill(ESM::Skill::LongBlade).getBase(), 30.f);
            result = lua.safe_script("Actor.stats.attributes.strength(target).base = 0/0",
                sol::script_pass_on_error);
            EXPECT_FALSE(result.valid());
            result = lua.safe_script("return Actor.stats.attributes.strength(target).base",
                sol::script_pass_on_error);
            ASSERT_TRUE(result.valid()) << sol::error(result).what();
            EXPECT_EQ(result.get<float>(), 255.f); // Rejected input did not replace the queued write.
            result = lua.safe_script("Actor.stats.attributes.strength(target).base = 4294967296; "
                "return Actor.stats.attributes.strength(target).base", sol::script_pass_on_error);
            ASSERT_TRUE(result.valid()) << sol::error(result).what();
            EXPECT_EQ(result.get<float>(), 0.f); // SSE overflow sentinel wraps at the native byte field.
            EXPECT_EQ(*service.findActorValues(values.mActor), values);
        }
    }

    TEST_F(OblivionActorStatsTest, luaModifiedQueriesIncludeNativeScriptAndProcessOwnership)
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
        model.registerPtr(ptr);
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[0] = {40, {7, 13, -3}};
        values.mValues[14] = {30, {5, 17, -2}};
        values.mValues[8].mBase = 100;
        values.mValues[9].mBase = 30;
        values.mValues[10].mBase = 40;
        MWMechanics::OblivionCombatService service;
        VFS::Manager vfs;
        LuaUtil::ScriptsConfiguration configuration;
        LuaUtil::LuaState luaState(&vfs, &configuration);
        MWLua::Context context{MWLua::Context::Global};
        context.mLua = &luaState;
        sol::state_view lua = luaState.unsafeState();
        sol::table actor(lua, sol::create), npc(lua, sol::create);
        MWLua::addActorStatsBindings(actor, context);
        npc["baseType"] = actor;
        MWLua::addNpcStatsBindings(npc, context);
        lua["Actor"] = actor;
        lua["NPC"] = npc;
        lua["target"] = MWLua::LObject(ptr);
        for (auto process : {ESM4::ActorValueProcess::Active, ESM4::ActorValueProcess::Low})
        {
            SCOPED_TRACE(static_cast<int>(process));
            values.mProcess = process;
            service.publishNonPlayerValues(ptr, values);
            auto result = lua.safe_script("return Actor.stats.attributes.strength(target).modified, "
                "NPC.stats.skills.longblade(target).modified", sol::script_pass_on_error);
            ASSERT_TRUE(result.valid()) << sol::error(result).what();
            EXPECT_FLOAT_EQ(result.get<float>(0), process == ESM4::ActorValueProcess::Active ? 57.f : 50.f);
            EXPECT_FLOAT_EQ(result.get<float>(1), process == ESM4::ActorValueProcess::Active ? 50.f : 45.f);
            auto damaged = values;
            damaged.mValues[0].mModifiers[2] = -70;
            damaged.mValues[14].mModifiers[2] = -70;
            service.publishNonPlayerValues(ptr, damaged);
            result = lua.safe_script("return Actor.stats.attributes.strength(target).modified, "
                "NPC.stats.skills.longblade(target).modified", sol::script_pass_on_error);
            ASSERT_TRUE(result.valid()) << sol::error(result).what();
            EXPECT_FLOAT_EQ(result.get<float>(0), process == ESM4::ActorValueProcess::Active ? -10.f : -17.f);
            EXPECT_FLOAT_EQ(result.get<float>(1), process == ESM4::ActorValueProcess::Active ? -18.f : -23.f);
        }
    }

    TEST_F(OblivionActorStatsTest, luaModifiedQueriesPreserveLegacyAndNativePlayerComposition)
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
        model.registerPtr(ptr);
        ESM::NpcState initial{};
        initial.blank();
        ptr.getClass().readAdditionalState(ptr, initial);
        auto& stats = ptr.getClass().getNpcStats(ptr);
        VFS::Manager vfs;
        LuaUtil::ScriptsConfiguration configuration;
        LuaUtil::LuaState luaState(&vfs, &configuration);
        MWLua::Context context{MWLua::Context::Global};
        context.mLua = &luaState;
        sol::state_view lua = luaState.unsafeState();
        sol::table actor(lua, sol::create), npc(lua, sol::create);
        MWLua::addActorStatsBindings(actor, context);
        npc["baseType"] = actor;
        MWLua::addNpcStatsBindings(npc, context);
        lua["Actor"] = actor;
        lua["NPC"] = npc;
        lua["target"] = MWLua::LObject(ptr);
        for (float damage : {3.f, 70.f})
        {
            SCOPED_TRACE(damage);
            MWMechanics::AttributeValue attribute;
            attribute.setBase(40);
            attribute.setModifier(7);
            attribute.damage(damage);
            stats.setAttribute(ESM::Attribute::Strength, attribute);
            MWMechanics::SkillValue skill;
            skill.setBase(30);
            skill.setModifier(5);
            skill.damage(damage);
            stats.setSkill(ESM::Skill::LongBlade, skill);
            auto result = lua.safe_script("return Actor.stats.attributes.strength(target).modified, "
                "NPC.stats.skills.longblade(target).modified", sol::script_pass_on_error);
            ASSERT_TRUE(result.valid()) << sol::error(result).what();
            EXPECT_FLOAT_EQ(result.get<float>(0), damage == 3 ? 44.f : 0.f);
            EXPECT_FLOAT_EQ(result.get<float>(1), damage == 3 ? 32.f : 0.f);
        }
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{0, 0, 0, 0}};
        values.mValues[0] = {40, {7, 13, -3}};
        values.mValues[14] = {30, {5, 17, -2}};
        MWMechanics::OblivionCombatService service;
        service.publishPlayerValues(player, values, {});
        auto result = lua.safe_script("return Actor.stats.attributes.strength(target).modified, "
            "NPC.stats.skills.longblade(target).modified", sol::script_pass_on_error);
        ASSERT_TRUE(result.valid()) << sol::error(result).what();
        EXPECT_FLOAT_EQ(result.get<float>(0), 57.f);
        EXPECT_FLOAT_EQ(result.get<float>(1), 50.f);
    }
}

namespace
{
    void verifyHealthCommandTransactions(MWMechanics::OblivionCombatService& service,
        const MWWorld::Ptr& ptr, const ESM4::RuntimeActorValues& values, MWWorld::Player* player = nullptr)
    {
        using Phase = ESM4::ActorLifePhase;
        using Command = ESM4::ActorValueCommand;
        using Source = ESM4::ActorValueCommandSource;
        const auto execute = [&](Command command, Source source, std::int32_t requested, bool essential,
                                 const ESM4::EssentialRecoverySettings& settings,
                                 const ESM4::ActorValueCommandPolicy& policy = {}) {
            return player
                ? service.executePlayerValueCommand(*player, 8, command, source, requested, policy, {}, essential, settings)
                : service.executeNonPlayerValueCommand(ptr, 8, command, source, requested, policy, {}, essential, settings);
        };
        const auto initial = [&](Phase phase, bool damagePresent = false) {
            service.clear();
            auto initialValues = values;
            if (damagePresent)
                initialValues.mValues[8].mModifiers[2] = 0;
            if (player)
                service.publishPlayerValues(*player, initialValues, {});
            else
                service.publishNonPlayerValues(ptr, initialValues);
            ESM4::RuntimeActorLife life{values.mActor, values.mBase, phase,
                phase == Phase::EssentialUnconscious ? 3.f : 0.f, {}};
            if (player)
                service.publishPlayerLife(*player, life);
            else
                service.publishNonPlayerLife(ptr, life);
            ESM4::RuntimeState saved;
            saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
            saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
            saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
            if (!player)
            {
                ESM4::RuntimeReferenceState referenceState;
                referenceState.mKey = values.mActor;
                referenceState.mBase = values.mBase;
                referenceState.mCell = saved.mPlayer.mCell;
                saved.mReferences.push_back(referenceState);
            }
            service.capture(saved);
            return saved;
        };
        for (auto phase : {Phase::Alive, Phase::Dead, Phase::EssentialUnconscious})
        for (bool essential : {false, true})
        for (auto source : {Source::Script, Source::Console})
        for (auto command : {Command::Mod, Command::Force})
        for (bool damagePresent : {false, true})
        {
            SCOPED_TRACE(static_cast<int>(phase));
            SCOPED_TRACE(essential);
            SCOPED_TRACE(static_cast<int>(source));
            SCOPED_TRACE(static_cast<int>(command));
            SCOPED_TRACE(damagePresent);
            const auto before = initial(phase, damagePresent);
            const int requested = command == Command::Mod ? -200 : -100;
            const auto result = execute(command, source, requested, essential, {3, .5f});
            EXPECT_TRUE(result.mAccepted);
            EXPECT_EQ(result.mHealthReactionDelta, -200);
            const bool entered = phase == Phase::Alive;
            const auto expectedLife = entered
                ? essential ? Phase::EssentialUnconscious : Phase::Dead : phase;
            const float expectedHealth = entered && essential && (source == Source::Console || (!player && !damagePresent)) ? 50.f : -100.f;
            EXPECT_EQ(service.findActorLife(values.mActor)->mPhase, expectedLife);
            EXPECT_FLOAT_EQ((player ? service.getPlayerValue(8) : service.getNonPlayerValue(ptr, 8)), expectedHealth);
            EXPECT_FLOAT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), expectedHealth);
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).isDead(), expectedLife == Phase::Dead);
            EXPECT_EQ(service.getDeadCount(values.mBase), entered && !essential ? 1 : 0);
            auto after = before;
            service.capture(after);
            EXPECT_EQ(after.mPendingDeathEvents.size(), entered && !essential ? 1u : 0u);
            EXPECT_EQ(after.mNextDeathEvent, entered && !essential ? 2u : 1u);
            EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(after.serializeBinary()), after);
            const auto counts = after.mNativeDeathCounts;
            execute(command, source, requested, essential, {3, .5f});
            auto repeated = before;
            service.capture(repeated);
            EXPECT_EQ(repeated.mPendingDeathEvents, after.mPendingDeathEvents);
            EXPECT_EQ(repeated.mNativeDeathCounts, counts);
        }
        for (bool essential : {false, true})
        {
            auto before = initial(Phase::Alive);
            before.mNextDeathEvent = std::numeric_limits<std::uint64_t>::max();
            service.restore(before);
            const auto rejected = [&]() {
                execute(Command::Mod, Source::Script, -200, essential,
                    {essential ? std::numeric_limits<float>::infinity() : 3.f, .5f});
            };
            if (essential)
                EXPECT_THROW(rejected(), std::invalid_argument);
            else
                EXPECT_THROW(rejected(), std::overflow_error);
            auto after = before;
            service.capture(after);
            EXPECT_EQ(after, before);
            EXPECT_FLOAT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 100);
            EXPECT_FALSE(ptr.getClass().getCreatureStats(ptr).isDead());
        }
        auto missingLife = initial(Phase::Alive);
        missingLife.mNativeActorLife.clear();
        service.restore(missingLife);
        EXPECT_THROW(execute(Command::Mod, Source::Script, -1, false, {}), std::invalid_argument);
        auto afterMissingLife = missingLife;
        service.capture(afterMissingLife);
        EXPECT_EQ(afterMissingLife, missingLife);
        EXPECT_FLOAT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 100);
        if (player)
        {
            const auto beforeGodMode = initial(Phase::Alive);
            const auto suppressed = execute(Command::Mod, Source::Script, -200, false, {}, {true, true});
            EXPECT_FALSE(suppressed.mAccepted);
            auto afterGodMode = beforeGodMode;
            service.capture(afterGodMode);
            EXPECT_EQ(afterGodMode, beforeGodMode);
            // Native eligibility checks positive int32 input before its float round trip.
            // INT_MAX subsequently converts to INT_MIN on the original command path.
            const auto overflow = execute(Command::Mod, Source::Script,
                std::numeric_limits<std::int32_t>::max(), false, {}, {true, true});
            EXPECT_TRUE(overflow.mAccepted);
            EXPECT_LT(service.getPlayerValue(8), 0);
            EXPECT_EQ(service.findActorLife(values.mActor)->mPhase, Phase::Dead);
            EXPECT_EQ(service.getDeadCount(values.mBase), 1);
        }
    }

    void verifyFloatingHealthTransactions(MWMechanics::OblivionCombatService& service,
        const MWWorld::Ptr& ptr, ESM4::RuntimeActorValues values, MWWorld::Player* player = nullptr)
    {
        using Phase = ESM4::ActorLifePhase;
        const auto source = ESM::FormKey::dynamic("player", 1);
        const auto change = [&](float delta, bool essential = false,
                                ESM4::EssentialRecoverySettings settings = {3, .5f}, bool godMode = false) {
            return player ? service.changePlayerHealth(*player, delta, source, essential, settings, {}, godMode)
                          : service.changeNonPlayerHealth(ptr, delta, source, essential, settings);
        };
        const auto health = [&]() { return player ? service.getPlayerValue(8) : service.getNonPlayerValue(ptr, 8); };
        const auto initial = [&](std::int32_t base = 2) {
            service.clear();
            values.mValues[8].mBase = base;
            if (player)
            {
                values.mPlayerFormValues = {{base, 30, 40, 0}};
                service.publishPlayerValues(*player, values, {});
                service.publishPlayerLife(*player, {values.mActor, values.mBase, Phase::Alive, 0, {}});
            }
            else
            {
                service.publishNonPlayerValues(ptr, values);
                service.publishNonPlayerLife(ptr, {values.mActor, values.mBase, Phase::Alive, 0, {}});
            }
            ESM4::RuntimeState saved;
            saved.mPlayer.mReference = source;
            saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
            saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
            saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
            if (!player)
            {
                ESM4::RuntimeReferenceState reference;
                reference.mKey = values.mActor;
                reference.mBase = values.mBase;
                reference.mCell = saved.mPlayer.mCell;
                saved.mReferences.push_back(reference);
            }
            service.capture(saved);
            return saved;
        };
        auto saved = initial();
        ASSERT_TRUE(change(-.5f));
        EXPECT_FLOAT_EQ(health(), 1.5f);
        EXPECT_EQ(service.findActorLife(values.mActor)->mPhase, Phase::Alive);
        EXPECT_TRUE(service.findActorLife(values.mActor)->mKiller.isNull());
        ASSERT_TRUE(change(.25f));
        EXPECT_FLOAT_EQ(health(), 1.75f);
        ASSERT_TRUE(change(-1.f));
        EXPECT_FLOAT_EQ(health(), .75f);
        EXPECT_EQ(service.findActorLife(values.mActor)->mPhase, Phase::Dead);
        EXPECT_EQ(service.findActorLife(values.mActor)->mKiller, source);
        EXPECT_TRUE(ptr.getClass().getCreatureStats(ptr).isDead());
        service.capture(saved);
        ASSERT_EQ(saved.mPendingDeathEvents.size(), 1u);
        EXPECT_EQ(saved.mNextDeathEvent, 2u);
        EXPECT_EQ(service.getDeadCount(values.mBase), 1);
        EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()), saved);
        const auto events = saved.mPendingDeathEvents;
        ASSERT_TRUE(change(1.f));
        ASSERT_TRUE(change(0.f));
        ASSERT_TRUE(change(-1.f));
        EXPECT_EQ(service.findActorLife(values.mActor)->mPhase, Phase::Dead);
        service.capture(saved);
        EXPECT_EQ(saved.mPendingDeathEvents, events);
        EXPECT_EQ(service.getDeadCount(values.mBase), 1);

        initial();
        ASSERT_TRUE(change(-1.25f, true));
        EXPECT_FLOAT_EQ(health(), 1.f);
        EXPECT_EQ(service.findActorLife(values.mActor)->mPhase, Phase::EssentialUnconscious);
        EXPECT_EQ(service.findActorLife(values.mActor)->mKiller, source);
        EXPECT_FLOAT_EQ(service.findActorLife(values.mActor)->mRecoveryRemaining, 3.f);
        EXPECT_EQ(service.getDeadCount(values.mBase), 0);
        service.capture(saved);
        EXPECT_TRUE(saved.mPendingDeathEvents.empty());

        for (bool essential : {false, true})
        {
            auto before = initial();
            before.mNextDeathEvent = std::numeric_limits<std::uint64_t>::max();
            service.restore(before);
            if (essential)
                EXPECT_THROW(change(-1.25f, true, {std::numeric_limits<float>::infinity(), .5f}),
                    std::invalid_argument);
            else
                EXPECT_THROW(change(-1.25f), std::overflow_error);
            auto after = before;
            service.capture(after);
            EXPECT_EQ(after, before);
            EXPECT_FLOAT_EQ(health(), 2.f);
            EXPECT_FLOAT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 2.f);
            EXPECT_FALSE(ptr.getClass().getCreatureStats(ptr).isDead());
        }
        auto before = initial();
        for (float invalid : {std::numeric_limits<float>::infinity(),
                 -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            EXPECT_THROW(change(invalid, false, {}, true), std::invalid_argument);
            auto after = before;
            service.capture(after);
            EXPECT_EQ(after, before);
        }
        before.mNativeActorLife.clear();
        service.restore(before);
        EXPECT_THROW(change(-.25f), std::invalid_argument);
        auto after = before;
        service.capture(after);
        EXPECT_EQ(after, before);

        initial(1.f);
        ASSERT_TRUE(change(-0x1p-25f));
        EXPECT_FLOAT_EQ(health(), 1.f);
        EXPECT_EQ(service.findActorLife(values.mActor)->mPhase, Phase::Alive);
        initial(1.f);
        ASSERT_TRUE(change(-0x1p-24f));
        EXPECT_LT(health(), 1.f);
        EXPECT_EQ(service.findActorLife(values.mActor)->mPhase, Phase::Dead);
        if (player)
        {
            before = initial();
            EXPECT_FALSE(change(-1.25f, false, {}, true));
            after = before;
            service.capture(after);
            EXPECT_EQ(after, before);
            ASSERT_TRUE(change(-.5f));
            ASSERT_TRUE(change(.25f, false, {}, true));
            EXPECT_FLOAT_EQ(health(), 1.75f);
        }
    }

    TEST_F(OblivionActorStatsTest, nativeHealthCommandsCommitValuesLifeAndEventsTogether)
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
        model.registerPtr(ptr);
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[0] = {40, {7, 13, -3}};
        values.mValues[14] = {30, {5, 17, -2}};
        values.mValues[8].mBase = 100;
        values.mValues[9].mBase = 30;
        values.mValues[10].mBase = 40;
        MWMechanics::OblivionCombatService service;
        verifyHealthCommandTransactions(service, ptr, values);
    }

    TEST_F(OblivionActorStatsTest, creatureHealthCommandsCommitValuesLifeAndEventsTogether)
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
        MWWorld::Ptr ptr(&live);
        model.registerPtr(ptr);
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[0] = {40, {7, 13, -3}};
        values.mValues[14] = {30, {5, 17, -2}};
        values.mValues[8].mBase = 100;
        values.mValues[9].mBase = 30;
        values.mValues[10].mBase = 40;
        MWMechanics::OblivionCombatService service;
        verifyHealthCommandTransactions(service, ptr, values);
    }

    TEST_F(OblivionActorStatsTest, playerHealthCommandsCommitValuesLifeAndEventsTogether)
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
        ESM::NpcState initialState{};
        initialState.blank();
        ptr.getClass().readAdditionalState(ptr, initialState);
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{100, 30, 40, 0}};
        MWMechanics::OblivionCombatService service;
        verifyHealthCommandTransactions(service, ptr, values, &player);
    }

    TEST_F(OblivionActorStatsTest, nativeFloatingHealthTransactions)
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
        model.registerPtr(ptr);
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[0] = {40, {7, 13, -3}};
        values.mValues[14] = {30, {5, 17, -2}};
        values.mValues[8].mBase = 100;
        values.mValues[9].mBase = 30;
        values.mValues[10].mBase = 40;
        MWMechanics::OblivionCombatService service;
        verifyFloatingHealthTransactions(service, ptr, values);
    }

    TEST_F(OblivionActorStatsTest, creatureFloatingHealthTransactions)
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
        MWWorld::Ptr ptr(&live);
        model.registerPtr(ptr);
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[0] = {40, {7, 13, -3}};
        values.mValues[14] = {30, {5, 17, -2}};
        values.mValues[8].mBase = 100;
        values.mValues[9].mBase = 30;
        values.mValues[10].mBase = 40;
        MWMechanics::OblivionCombatService service;
        verifyFloatingHealthTransactions(service, ptr, values);
    }

    TEST_F(OblivionActorStatsTest, playerFloatingHealthTransactions)
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
        ESM::NpcState initialState{};
        initialState.blank();
        ptr.getClass().readAdditionalState(ptr, initialState);
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{100, 30, 40, 0}};
        MWMechanics::OblivionCombatService service;
        verifyFloatingHealthTransactions(service, ptr, values, &player);
    }


    TEST_F(OblivionActorStatsTest, creatureMovementCapabilitiesUseNativeFlags)
    {
        MWClass::ESM4Creature::registerSelf();
        ESM4::Creature creature{};
        creature.mId = {0x800, 3};
        creature.mFormKey = mActorKey;
        creature.mAttackReach = 64;
        ESM4::ActorCreature reference{};
        reference.mFormKey = ESM::FormKey::content("actors.esm", 0x900);
        reference.mId = {0x900, 3};
        reference.mBaseKey = mActorKey;
        MWWorld::LiveCellRef<ESM4::Creature> live(reference, &creature);
        MWWorld::Ptr ptr(&live);
        // Independent native predicate outcomes, including biped-implied swim/walk.
        // Tuple: flags, biped, swim, walk, fly, pure aquatic, pure flying, pure land.
        const std::array<std::tuple<unsigned, bool, bool, bool, bool, bool, bool, bool>, 10> cases{{
            {0x00, false, false, false, false, false, false, false},
            {0x01, true, true, true, false, false, false, false},
            {0x10, false, true, false, false, true, false, false},
            {0x20, false, false, false, true, false, true, false},
            {0x40, false, false, true, false, false, false, true},
            {0x50, false, true, true, false, false, false, false},
            {0x11, true, true, true, false, false, false, false},
            {0x30, false, true, false, true, false, false, false},
            {0x71, true, true, true, true, false, false, false},
            {0x92, false, true, false, false, true, false, false},
        }};
        for (const auto& [flags, biped, swim, walk, fly, aquatic, flying, land] : cases)
        {
            SCOPED_TRACE(flags);
            creature.mBaseConfig.tes4.flags = flags;
            EXPECT_EQ(ptr.getClass().isBipedal(ptr), biped);
            EXPECT_EQ(ptr.getClass().canSwim(ptr), swim);
            EXPECT_EQ(ptr.getClass().canWalk(ptr), walk);
            EXPECT_EQ(ptr.getClass().canFly(ptr), fly);
            EXPECT_EQ(ptr.getClass().isPureWaterCreature(ptr), aquatic);
            EXPECT_EQ(ptr.getClass().isPureFlyingCreature(ptr), flying);
            EXPECT_EQ(ptr.getClass().isPureLandCreature(ptr), land);
        }
    }


    void verifyBreathTransactions(MWMechanics::OblivionCombatService& service,
        const MWWorld::Ptr& ptr, ESM4::RuntimeActorValues values, MWWorld::Player* player = nullptr)
    {
        using Phase = ESM4::ActorLifePhase;
        const auto initial = [&](std::optional<float> remaining = .25f, float waterBreathing = 0.f) {
            service.clear();
            values.mValues[5].mBase = 50;
            values.mValues[55].mBase = waterBreathing;
            if (player)
            {
                service.publishPlayerValues(*player, values, {});
                service.publishPlayerLife(*player, {values.mActor, values.mBase, Phase::Alive, 0, {}});
            }
            else
            {
                service.publishNonPlayerValues(ptr, values);
                service.publishNonPlayerLife(ptr, {values.mActor, values.mBase, Phase::Alive, 0, {}});
            }
            ESM4::RuntimeState saved;
            saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
            saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
            saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
            if (!player)
            {
                ESM4::RuntimeReferenceState reference;
                reference.mKey = values.mActor;
                reference.mBase = values.mBase;
                reference.mCell = saved.mPlayer.mCell;
                saved.mReferences.push_back(reference);
            }
            service.capture(saved);
            if (remaining)
                saved.mNativeActorBreath.emplace(values.mActor, *remaining);
            service.restore(saved);
            return saved;
        };
        const auto update = [&](float duration, bool needsAir = true, bool essential = false,
                                ESM4::EssentialRecoverySettings recovery = {3, .5f},
                                ESM4::SwimBreathSettings settings = {4, .3f, .2f}, bool godMode = false) {
            return player ? service.updatePlayerBreath(*player, duration, needsAir, essential,
                                settings, recovery, {}, godMode)
                          : service.updateNonPlayerBreath(ptr, duration, needsAir, essential, settings, recovery);
        };
        const auto health = [&]() { return player ? service.getPlayerValue(8) : service.getNonPlayerValue(ptr, 8); };
        auto saved = initial(std::nullopt);
        auto result = update(.25f, true, false, {}, {40, 0, .2f});
        ASSERT_TRUE(result);
        EXPECT_EQ(result->mRemaining, 19.75f); // Native constructor default20.
        EXPECT_EQ(result->mMaximum, 40.f);
        EXPECT_FALSE(result->mDrowning);
        saved = initial();
        result = update(.25f);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->mRemaining, 0.f);
        EXPECT_FALSE(result->mDrowning);
        EXPECT_EQ(health(), 100.f);
        result = update(.25f);
        ASSERT_TRUE(result);
        EXPECT_TRUE(result->mDrowning);
        EXPECT_EQ(result->mDamage, 5.f);
        EXPECT_EQ(health(), 95.f);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 95.f);
        EXPECT_EQ(service.findActorBreath(values.mActor), 0.f);
        service.capture(saved);
        EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()), saved);
        EXPECT_TRUE(saved.mPendingDeathEvents.empty());
        result = update(30, false);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->mRemaining, 19.f);
        EXPECT_FALSE(result->mDrowning);
        EXPECT_EQ(health(), 95.f);

        for (float water : {1.f, -1.f})
        {
            initial(.25f, water);
            result = update(.5f);
            ASSERT_TRUE(result);
            EXPECT_EQ(result->mRemaining, .75f);
            EXPECT_FALSE(result->mDrowning);
        }
        initial(.25f, .5f); // Native integer query floors to0.
        result = update(.5f);
        ASSERT_TRUE(result);
        EXPECT_TRUE(result->mDrowning);
        EXPECT_EQ(health(), 90.f);

        // Damage derives from base Health, excluding all current modifiers.
        values.mValues[8].mModifiers[1] = 25.f;
        initial();
        result = update(.5f);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->mDamage, 10.f);
        EXPECT_EQ(health(), 115.f);
        values.mValues[8].mModifiers[1].reset();

        for (bool essential : {false, true})
        for (bool existingTimer : {false, true})
        {
            auto before = initial(existingTimer ? std::optional(.25f) : std::nullopt);
            before.mNextDeathEvent = std::numeric_limits<std::uint64_t>::max();
            service.restore(before);
            if (essential)
                EXPECT_THROW(update(30, true, true, {std::numeric_limits<float>::infinity(), .5f}),
                    std::invalid_argument);
            else
                EXPECT_THROW(update(30), std::overflow_error);
            auto after = before;
            service.capture(after);
            EXPECT_EQ(after, before);
            EXPECT_EQ(health(), 100.f);
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 100.f);
            EXPECT_FALSE(ptr.getClass().getCreatureStats(ptr).isDead());
        }
        saved = initial();
        result = update(6);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->mDamage, 120.f);
        EXPECT_EQ(health(), -20.f);
        EXPECT_EQ(service.findActorLife(values.mActor)->mPhase, Phase::Dead);
        EXPECT_TRUE(service.findActorLife(values.mActor)->mKiller.isNull());
        EXPECT_EQ(service.getDeadCount(values.mBase), 1);
        service.capture(saved);
        ASSERT_EQ(saved.mPendingDeathEvents.size(), 1u);
        EXPECT_EQ(saved.mNativeActorBreath.at(values.mActor), 0.f);
        EXPECT_FALSE(update(1));
        auto afterDead = saved;
        service.capture(afterDead);
        EXPECT_EQ(afterDead, saved);

        initial();
        result = update(6, true, true);
        ASSERT_TRUE(result);
        EXPECT_EQ(health(), 50.f);
        EXPECT_EQ(service.findActorLife(values.mActor)->mPhase, Phase::EssentialUnconscious);
        EXPECT_FLOAT_EQ(service.findActorLife(values.mActor)->mRecoveryRemaining, 3.f);
        EXPECT_EQ(service.getDeadCount(values.mBase), 0);
        EXPECT_EQ(service.findActorBreath(values.mActor), 0.f);
        initial();
        result = update(6, true, false, {}, {4, .3f, -.2f});
        ASSERT_TRUE(result);
        EXPECT_TRUE(result->mDrowning);
        EXPECT_EQ(result->mDamage, 0.f);
        EXPECT_EQ(health(), 100.f);

        saved = initial(std::nullopt);
        for (float bad : {-1.f, std::numeric_limits<float>::infinity(),
                 std::numeric_limits<float>::quiet_NaN()})
        {
            EXPECT_THROW(update(bad, false), std::invalid_argument);
            auto after = saved;
            service.capture(after);
            EXPECT_EQ(after, saved);
        }
        auto missingLife = saved;
        missingLife.mNativeActorLife.clear();
        service.restore(missingLife);
        EXPECT_THROW(update(.25f), std::invalid_argument);
        auto afterMissingLife = missingLife;
        service.capture(afterMissingLife);
        EXPECT_EQ(afterMissingLife, missingLife);
        if (player)
        {
            initial();
            result = update(30, true, true, {3, .5f}, {4, .3f, .2f}, true);
            ASSERT_TRUE(result);
            EXPECT_EQ(result->mRemaining, 19.f);
            EXPECT_FALSE(result->mDrowning);
            EXPECT_EQ(health(), 100.f);
            EXPECT_EQ(service.findActorLife(values.mActor)->mPhase, Phase::Alive);
        }
    }

    TEST_F(OblivionActorStatsTest, nativeBreathTransactions)
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
        model.registerPtr(ptr);
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[0] = {40, {7, 13, -3}};
        values.mValues[14] = {30, {5, 17, -2}};
        values.mValues[8].mBase = 100;
        values.mValues[9].mBase = 30;
        values.mValues[10].mBase = 40;
        MWMechanics::OblivionCombatService service;
        verifyBreathTransactions(service, ptr, values);
    }

    TEST_F(OblivionActorStatsTest, creatureBreathTransactions)
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
        MWWorld::Ptr ptr(&live);
        model.registerPtr(ptr);
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[0] = {40, {7, 13, -3}};
        values.mValues[14] = {30, {5, 17, -2}};
        values.mValues[8].mBase = 100;
        values.mValues[9].mBase = 30;
        values.mValues[10].mBase = 40;
        MWMechanics::OblivionCombatService service;
        verifyBreathTransactions(service, ptr, values);
    }

    TEST_F(OblivionActorStatsTest, playerBreathTransactions)
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
        ESM::NpcState initialState{};
        initialState.blank();
        ptr.getClass().readAdditionalState(ptr, initialState);
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{100, 30, 40, 0}};
        MWMechanics::OblivionCombatService service;
        verifyBreathTransactions(service, ptr, values, &player);
    }

    TEST_F(OblivionActorStatsTest, nativeFrameSettingsUseWinningNativeRecords)
    {
        const auto defaults = MWWorld::resolveOblivionFrameSettings(mStore);
        EXPECT_EQ(defaults.mMagicka.mBase, .75f);
        EXPECT_EQ(defaults.mMagicka.mWillpowerMultiplier, .02f);
        EXPECT_EQ(defaults.mFatigue.mRegeneration.mBase, 10.f);
        ESM::GameSetting shared{};
        shared.mId = ESM::RefId::stringRefId("fMagickaReturnBase");
        shared.mValue.setType(ESM::VT_Float);
        shared.mValue.setFloat(999);
        mStore.getWritable<ESM::GameSetting>().insertStatic(shared);
        EXPECT_EQ(MWWorld::resolveOblivionFrameSettings(mStore).mMagicka.mBase, .75f);
        ESM4::GameSetting native{};
        native.mId = {0x980, 3};
        native.mEditorId = "FMAGICKARETURNBASE";
        native.mData = 2.f;
        const auto key = ESM::FormKey::content("actors.esm", 0x980);
        mStore.getWritable<ESM4::GameSetting>().insertStatic(native, key);
        EXPECT_EQ(MWWorld::resolveOblivionFrameSettings(mStore).mMagicka.mBase, 2.f);
        native.mData = std::int32_t{2};
        mStore.getWritable<ESM4::GameSetting>().insertStatic(native, key);
        EXPECT_THROW(MWWorld::resolveOblivionFrameSettings(mStore), std::invalid_argument);
    }

    void verifyMagickaFrameTransactions(MWMechanics::OblivionCombatService& service,
        const MWWorld::Ptr& ptr, ESM4::RuntimeActorValues values, MWWorld::Player* player = nullptr)
    {
        using Phase = ESM4::ActorLifePhase;
        const ESM4::MagickaRegenerationSettings settings{.75f, .02f};
        values.mValues[2] = {50, {.75f, {}, {}}}; // Native integer Willpower50.
        values.mValues[8] = {100, {std::nullopt, std::nullopt, -10}};
        values.mValues[9] = {100, {std::nullopt, std::nullopt, -80}};
        values.mValues[10] = {200, {std::nullopt, std::nullopt, -50}};
        if (player)
            values.mPlayerFormValues = {{100, 100, 200, 0}};
        const auto publish = [&] {
            if (player)
            {
                service.publishPlayerValues(*player, values, {});
                service.publishPlayerLife(*player, {values.mActor, values.mBase, Phase::Alive, 0, {}});
            }
            else
            {
                service.publishNonPlayerValues(ptr, values);
                service.publishNonPlayerLife(ptr, {values.mActor, values.mBase, Phase::Alive, 0, {}});
            }
        };
        const auto update = [&](float duration, bool casting = false,
                                ESM4::MagickaRegenerationSettings config = {.75f, .02f}) {
            if (player)
                service.regeneratePlayerMagicka(*player, duration, casting, config, {});
            else
                service.regenerateNonPlayerMagicka(ptr, duration, casting, config);
        };
        const auto current = [&] { return ptr.getClass().getCreatureStats(ptr).getMagicka().getCurrent(); };
        publish();
        // No World/Lua/effect subsystem is installed. Native rejection must occur
        // before legacy record lookup, item removal, notifications or effects.
        const auto beforeMagic = *service.findActorValues(values.mActor);
        MWMechanics::CastSpell cast(ptr, ptr);
        EXPECT_FALSE(cast.cast(ESM::RefId::stringRefId("not-a-tes3-spell")));
        EXPECT_FALSE(cast.cast(static_cast<const ESM::Spell*>(nullptr)));
        EXPECT_FALSE(cast.cast(static_cast<const ESM::Potion*>(nullptr)));
        EXPECT_FALSE(cast.cast(static_cast<const ESM::Ingredient*>(nullptr)));
        EXPECT_FALSE(cast.cast(MWWorld::Ptr{}));
        EXPECT_NO_THROW(cast.inflict(ptr, {}, ESM::RT_Self));
        EXPECT_FALSE(ptr.getClass().consume({}, ptr));
        EXPECT_EQ(*service.findActorValues(values.mActor), beforeMagic);
        auto frameSettings = MWWorld::resolveOblivionFrameSettings(*MWBase::Environment::get().getESMStore());
        frameSettings.mFatigue.mRegeneration = {2, 0};
        frameSettings.mFatigue.mPlayerBase = {};
        const auto frame = [&](bool casting, const MWMechanics::OblivionFrameSettings& config) {
            const MWMechanics::OblivionFatigueUpdate input{.5f, 0, false, false};
            if (player)
                service.updatePlayerFrameResources(*player, input, casting, config);
            else
                service.updateNonPlayerFrameResources(ptr, input, casting, config);
        };
        const auto initialFatigue = ptr.getClass().getCreatureStats(ptr).getFatigue().getCurrent();
        const auto initialHealth = ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent();
        frame(false, frameSettings);
        EXPECT_FLOAT_EQ(current(), 20.875f);
        EXPECT_FLOAT_EQ(ptr.getClass().getCreatureStats(ptr).getFatigue().getCurrent(), initialFatigue + 1);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), initialHealth);
        frame(true, frameSettings);
        EXPECT_FLOAT_EQ(current(), 20.875f);
        EXPECT_FLOAT_EQ(ptr.getClass().getCreatureStats(ptr).getFatigue().getCurrent(), initialFatigue + 2);
        const auto beforeInvalidFrame = *service.findActorValues(values.mActor);
        auto badFrame = frameSettings;
        badFrame.mFatigue.mRegeneration.mBase = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(frame(false, badFrame), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(values.mActor), beforeInvalidFrame);
        EXPECT_FLOAT_EQ(current(), 20.875f);
        EXPECT_FLOAT_EQ(ptr.getClass().getCreatureStats(ptr).getFatigue().getCurrent(), initialFatigue + 2);
        badFrame = frameSettings;
        badFrame.mMagicka.mBase = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(frame(false, badFrame), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(values.mActor), beforeInvalidFrame);
        publish();
        const auto before = *service.findActorValues(values.mActor);
        update(.5f);
        EXPECT_FLOAT_EQ(current(), 20.875f);
        auto expected = before;
        expected.mValues[9].mModifiers[2] = -79.125f;
        EXPECT_EQ(*service.findActorValues(values.mActor), expected); // Only Magicka Damage changes.
        EXPECT_EQ(service.findActorLife(values.mActor)->mPhase, Phase::Alive);
        update(.5f, true);
        update(0);
        EXPECT_EQ(*service.findActorValues(values.mActor), expected);
        for (float duration : {-1.f, std::numeric_limits<float>::quiet_NaN(),
                 std::numeric_limits<float>::infinity()})
        {
            EXPECT_THROW(update(duration), std::invalid_argument);
            EXPECT_EQ(*service.findActorValues(values.mActor), expected);
            EXPECT_FLOAT_EQ(current(), 20.875f);
        }
        auto invalid = settings;
        invalid.mBase = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(update(1, false, invalid), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(values.mActor), expected);
        for (float stunted : {1.f, .75f, 0.f, -1.f})
        {
            values.mValues[57].mBase = stunted;
            publish();
            update(1);
            EXPECT_FLOAT_EQ(current(), stunted == 1 ? 20.f : 21.75f);
        }
        values.mValues[57].mBase = 0;
        for (auto process : {ESM4::ActorValueProcess::Active, ESM4::ActorValueProcess::Low})
        {
            values.mProcess = process;
            values.mValues[9].mModifiers = {10, -1, -80};
            publish();
            update(1);
            const bool maximum = player || process == ESM4::ActorValueProcess::Active;
            EXPECT_FLOAT_EQ(current(), maximum ? 30.925f : 20.75f);
            const auto& result = service.findActorValues(values.mActor)->mValues[9];
            EXPECT_EQ(result.mModifiers[0], 10);
            EXPECT_EQ(result.mModifiers[1], -1);
        }
        values.mProcess = ESM4::ActorValueProcess::Active;
        values.mValues[9].mModifiers = {std::nullopt, std::nullopt, -.25f};
        publish();
        update(1); // Truncated current99 admits restoration; Damage clamps at zero.
        EXPECT_FLOAT_EQ(current(), 100.f);
        const auto full = *service.findActorValues(values.mActor);
        update(1);
        EXPECT_EQ(*service.findActorValues(values.mActor), full);
        for (auto damage : {std::optional<float>{}, std::optional<float>{0}})
        {
            values.mValues[9].mModifiers = {std::nullopt, -1, damage};
            publish();
            update(1);
            update(1);
            EXPECT_FLOAT_EQ(current(), 99.f); // Magicka's permanent Damage slot cannot offset Script.
            EXPECT_EQ(service.findActorValues(values.mActor)->mValues[9].mModifiers[2], 0.f);
        }
        values.mValues[9].mModifiers = {std::nullopt, std::nullopt, -80};
        publish();
        update(.5f);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        if (!player)
        {
            ESM4::RuntimeReferenceState reference;
            reference.mKey = values.mActor;
            reference.mBase = values.mBase;
            reference.mCell = saved.mPlayer.mCell;
            saved.mReferences.push_back(reference);
        }
        service.capture(saved);
        service.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        update(.5f);
        EXPECT_FLOAT_EQ(current(), 21.75f);
        if (player)
        {
            values.mValues[1].mBase = 50;
            publish(); // Magicka base150 with the initial zero setting.
            EXPECT_FLOAT_EQ(current(), 70.f);
            const auto health = ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent();
            const auto fatigue = ptr.getClass().getCreatureStats(ptr).getFatigue().getCurrent();
            service.regeneratePlayerMagicka(*player, 1, false, settings, {0, 1, 0});
            EXPECT_FLOAT_EQ(current(), 123.5f); // New base200 gives a3.5 request, not the stale2.625.
            EXPECT_EQ(service.findActorValues(values.mActor)->mValues[9].mBase, 200);
            EXPECT_EQ((*service.findActorValues(values.mActor)->mPlayerFormValues)[1], 100);
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), health);
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getFatigue().getCurrent(), fatigue);
            auto invalidBase = ESM4::PlayerDynamicBaseSettings{0, std::numeric_limits<float>::quiet_NaN(), 0};
            const auto beforeInvalid = *service.findActorValues(values.mActor);
            EXPECT_THROW(service.regeneratePlayerMagicka(*player, 1, false, settings, invalidBase), std::invalid_argument);
            EXPECT_EQ(*service.findActorValues(values.mActor), beforeInvalid);
            EXPECT_FLOAT_EQ(current(), 123.5f);
            publish();
            service.restorePlayerResources(*player, {1, false, false}, {settings, {2, 0}, {0, 1, 0}});
            EXPECT_FLOAT_EQ(current(), 123.5f); // Rest must use the new base before calculating its request.
            for (int entry = 0; entry < 6; ++entry)
            {
                SCOPED_TRACE(testing::Message() << "player resource entry=" << entry);
                publish();
                const auto beforeRefresh = *service.findActorValues(values.mActor);
                const auto refresh = [&](const ESM4::PlayerDynamicBaseSettings& base) {
                    auto config = frameSettings;
                    config.mFatigue.mPlayerBase = base;
                    switch (entry)
                    {
                        case 0: service.regeneratePlayerMagicka(*player, 0, true, settings, base); break;
                        case 1: service.regeneratePlayerFatigue(*player, 0, {2, 0}, base); break;
                        case 2: service.restorePlayerResources(*player, {0, false, true}, {settings, {2, 0}, base}); break;
                        case 3: service.updatePlayerFatigue(*player, {0, 0, false, false}, config.mFatigue); break;
                        case 4: service.updatePlayerFrameResources(*player, {0, 0, false, false}, true, config); break;
                        case 5: service.spendPlayerJumpFatigue(*player, 0, false, config.mFatigue); break;
                    }
                };
                refresh({0, 1, 0});
                auto expectedRefresh = beforeRefresh;
                expectedRefresh.mValues[9].mBase = 200;
                EXPECT_EQ(*service.findActorValues(values.mActor), expectedRefresh);
                EXPECT_FLOAT_EQ(current(), 120.f); // Refresh even when the resource operation itself is a no-op.
                EXPECT_THROW(refresh(invalidBase), std::invalid_argument);
                EXPECT_EQ(*service.findActorValues(values.mActor), expectedRefresh);
                EXPECT_FLOAT_EQ(current(), 120.f);
            }
            values.mValues[1].mBase = 0;
        }
        else
        {
            for (float multiplier : {5.f, 20.f})
            {
                values.mValues[40].mBase = multiplier;
                publish();
                update(1);
                EXPECT_FLOAT_EQ(current(), multiplier == 5 ? 10.875f : 43.5f);
                EXPECT_FLOAT_EQ(*service.findActorValues(values.mActor)->mValues[9].mModifiers[2], -78.25f);
            }
            values.mValues[40].mBase = 0;
        }
        publish();
        update(1);
        if (player)
            service.publishPlayerLife(*player, {values.mActor, values.mBase, Phase::Dead, 0, {}});
        else
            service.publishNonPlayerLife(ptr, {values.mActor, values.mBase, Phase::Dead, 0, {}});
        const auto dead = *service.findActorValues(values.mActor);
        update(10);
        EXPECT_EQ(*service.findActorValues(values.mActor), dead);
        EXPECT_FLOAT_EQ(current(), 21.75f);
    }

    void verifyClockFrameTransactions(const MWWorld::Ptr& ptr,
        ESM4::RuntimeActorValues values, MWWorld::Player* player = nullptr)
    {
        using Phase = ESM4::ActorLifePhase;
        MWMechanics::OblivionCombatService service;
        values.mValues[2] = {50, {}};
        values.mValues[8] = {100, {std::nullopt, std::nullopt, -10}};
        values.mValues[9] = {100, {std::nullopt, std::nullopt, -80}};
        values.mValues[10] = {200, {std::nullopt, std::nullopt, -50}};
        if (player)
            values.mPlayerFormValues = {{100, 100, 150, 0}}; // Willpower adds50 to the native Fatigue base.
        auto settings = MWWorld::resolveOblivionFrameSettings(*MWBase::Environment::get().getESMStore());
        settings.mMagicka = {.75f, .02f};
        settings.mFatigue.mRegeneration = {2, 0};
        settings.mFatigue.mPlayerBase = {};
        const auto life = [&](Phase phase) {
            if (player)
                service.publishPlayerLife(*player, {values.mActor, values.mBase, phase, 0, {}});
            else
                service.publishNonPlayerLife(ptr, {values.mActor, values.mBase, phase, 0, {}});
        };
        const auto publish = [&] {
            if (player)
                service.publishPlayerValues(*player, values, {});
            else
                service.publishNonPlayerValues(ptr, values);
            life(Phase::Alive);
        };
        const auto update = [&](const MWMechanics::OblivionFrameSettings& config) {
            if (player)
                service.updatePlayerFrameResourcesFromClock(*player, {0, false, false}, false, config);
            else
                service.updateNonPlayerFrameResourcesFromClock(ptr, {0, false, false}, false, config);
        };
        ESM4::RuntimeState identity;
        identity.mProfile = ESM::GameProfile::Oblivion;
        identity.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        identity.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        identity.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        identity.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        if (!player)
        {
            ESM4::RuntimeReferenceState ref;
            ref.mKey = values.mActor;
            ref.mBase = values.mBase;
            ref.mCell = identity.mPlayer.mCell;
            identity.mReferences.push_back(ref);
        }
        const auto capture = [&] {
            auto state = identity;
            service.capture(state);
            return state;
        };
        const auto fatigue = [&] { return ptr.getClass().getCreatureStats(ptr).getFatigue().getCurrent(); };
        publish();
        EXPECT_EQ(service.elapsedSinceActorUpdate(values.mActor, .125f), .125f);
        EXPECT_EQ(service.elapsedSinceActorUpdate(values.mActor, .3f), 0.f);
        EXPECT_THROW(service.elapsedSinceActorUpdate(values.mActor, 100001.f), std::invalid_argument);
        for (float bad : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            EXPECT_THROW(service.advanceFrameClock(bad), std::invalid_argument);
            EXPECT_EQ(service.actorManagerTime(), 0.f);
        }
        service.advanceFrameClock(.125f);
        update(settings); // Missing constructor time -1 permits this small initial duration.
        EXPECT_FLOAT_EQ(fatigue(), 150.25f);
        EXPECT_FLOAT_EQ(ptr.getClass().getCreatureStats(ptr).getMagicka().getCurrent(), 20.21875f);
        const auto first = capture();
        EXPECT_EQ(first.mNativeActorUpdateTimes.at(values.mActor), .125f);
        update(settings); // A second writer in the same tick cannot regenerate twice.
        EXPECT_EQ(capture().mNativeActorValues, first.mNativeActorValues);
        service.restore(ESM4::RuntimeState::deserializeBinary(first.serializeBinary()));
        service.advanceFrameClock(.5f);
        service.advanceFrameClock(.25f); // Actor skipped two manager ticks.
        auto bad = settings;
        bad.mMagicka.mBase = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(update(bad), std::invalid_argument);
        EXPECT_EQ(capture().mNativeActorValues, first.mNativeActorValues);
        EXPECT_EQ(capture().mNativeActorUpdateTimes, first.mNativeActorUpdateTimes);
        EXPECT_EQ(service.actorManagerTime(), .875f);
        update(settings);
        EXPECT_FLOAT_EQ(fatigue(), 151.75f);
        EXPECT_FLOAT_EQ(ptr.getClass().getCreatureStats(ptr).getMagicka().getCurrent(), 21.53125f);
        EXPECT_EQ(capture().mNativeActorUpdateTimes.at(values.mActor), .875f);
        life(Phase::Dead);
        const auto dead = capture();
        service.advanceFrameClock(1.f);
        update(settings);
        EXPECT_EQ(capture().mNativeActorValues, dead.mNativeActorValues);
        EXPECT_EQ(capture().mNativeActorUpdateTimes.at(values.mActor), 1.875f);
        life(Phase::Alive);
        service.advanceFrameClock(.125f);
        update(settings);
        EXPECT_FLOAT_EQ(fatigue(), 152.f); // Dead interval never replays after revival.
        service.advanceFrameClock(100000.f);
        EXPECT_EQ(service.actorManagerTime(), 0.f);
        update(settings);
        EXPECT_FLOAT_EQ(fatigue(), 152.f); // Rewind is not elapsed=100000.
        EXPECT_EQ(capture().mNativeActorUpdateTimes.at(values.mActor), 0.f);
        service.advanceFrameClock(.125f);
        update(settings);
        EXPECT_FLOAT_EQ(fatigue(), 152.25f);
        service.clear();
        publish();
        service.advanceFrameClock(.3f);
        update(settings);
        EXPECT_FLOAT_EQ(fatigue(), 150.f); // Strict initialization threshold.
        EXPECT_EQ(capture().mNativeActorUpdateTimes.at(values.mActor), .3f);
    }

    TEST_F(OblivionActorStatsTest, nativeMagickaFrameTransactions)
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
        model.registerPtr(ptr);
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[0] = {40, {7, 13, -3}};
        values.mValues[14] = {30, {5, 17, -2}};
        values.mValues[8].mBase = 100;
        values.mValues[9].mBase = 30;
        values.mValues[10].mBase = 40;
        MWMechanics::OblivionCombatService service;
        verifyMagickaFrameTransactions(service, ptr, values);
        verifyClockFrameTransactions(ptr, values);
    }

    TEST_F(OblivionActorStatsTest, creatureMagickaFrameTransactions)
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
        MWWorld::Ptr ptr(&live);
        model.registerPtr(ptr);
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey;
        values.mBase = mActorKey;
        values.mValues[0] = {40, {7, 13, -3}};
        values.mValues[14] = {30, {5, 17, -2}};
        values.mValues[8].mBase = 100;
        values.mValues[9].mBase = 30;
        values.mValues[10].mBase = 40;
        MWMechanics::OblivionCombatService service;
        verifyMagickaFrameTransactions(service, ptr, values);
        verifyClockFrameTransactions(ptr, values);
    }

    TEST_F(OblivionActorStatsTest, playerMagickaFrameTransactions)
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
        ESM::NpcState initialState{};
        initialState.blank();
        ptr.getClass().readAdditionalState(ptr, initialState);
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{100, 30, 40, 0}};
        MWMechanics::OblivionCombatService service;
        verifyMagickaFrameTransactions(service, ptr, values, &player);
        verifyClockFrameTransactions(ptr, values, &player);
    }



    TEST_F(OblivionActorStatsTest, terminalHealthCommitRemovesCombatMembershipBeforeCallbackCapture)
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
        ESM4::RuntimeState state;
        state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        state.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        state.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        state.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        ESM4::RuntimeActorValues values;
        values.mActor = state.mPlayer.mReference;
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{100, 100, 200, 0}};
        MWMechanics::OblivionCombatService service;
        service.publishPlayerValues(player, values, {});
        service.publishPlayerLife(player, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        service.capture(state);
        for (std::uint32_t i : {10, 11})
        {
            ESM4::RuntimeReferenceState ref;
            ref.mKey = ESM::FormKey::content("actors.esm", i);
            ref.mBase = ESM::FormKey::content("actors.esm", i + 100);
            ref.mCell = state.mPlayer.mCell;
            state.mReferences.push_back(ref);
            ESM4::RuntimeActorValues opponent;
            opponent.mActor = ref.mKey;
            opponent.mBase = ref.mBase;
            state.mNativeActorValues.push_back(opponent);
            state.mNativeActorLife.push_back({ref.mKey, ref.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        }
        service.restore(state);
        const auto a = state.mReferences[0].mKey, b = state.mReferences[1].mKey;
        ASSERT_TRUE(service.engage(values.mActor, a));
        ASSERT_TRUE(service.engage(a, b));
        service.publishPlayerLife(player,
            {values.mActor, values.mBase, ESM4::ActorLifePhase::EssentialUnconscious, 1, a});
        EXPECT_TRUE(service.isInCombatWith(values.mActor, a));
        service.publishPlayerLife(player, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        ESM4::EssentialRecoverySettings invalid{};
        invalid.mDelay = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(service.changePlayerHealth(player, -101, a, true, invalid, {}), std::invalid_argument);
        EXPECT_TRUE(service.isInCombatWith(values.mActor, a));
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 100);
        EXPECT_TRUE(service.changePlayerHealth(player, -101, a, false, {}, {}));
        EXPECT_TRUE(ptr.getClass().getCreatureStats(ptr).isDead());
        EXPECT_FALSE(service.isInCombat(values.mActor));
        EXPECT_TRUE(service.isInCombatWith(a, b));
        const auto event = service.takeNextDeathEvent();
        ASSERT_TRUE(event);
        service.capture(state); // A save inside the callback must already see cleanup.
        EXPECT_EQ(state.mNativeCombatEngagements,
            (std::set<std::pair<ESM::FormKey, ESM::FormKey>>{{a, b}}));
        EXPECT_NO_THROW(ESM4::RuntimeState::deserializeBinary(state.serializeBinary()));
        service.publishPlayerLife(player, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        EXPECT_FALSE(service.isInCombat(values.mActor)); // Revival cannot recreate old opponents.
        ASSERT_TRUE(service.engage(values.mActor, a));
        service.publishPlayerLife(player, {values.mActor, values.mBase, ESM4::ActorLifePhase::Dead, 0, a});
        EXPECT_FALSE(service.isInCombat(values.mActor));
        EXPECT_EQ(service.combatOpponents(a), (std::vector<ESM::FormKey>{b}));
    }

    TEST_F(OblivionActorStatsTest, passiveInputsUseWinningStableSpellsAndPreparedDefinitionHistory)
    {
        const auto key = ESM::FormKey::content("abilities.esp", 0x123);
        ESM4::Spell spell{};
        spell.mId = {0x123, 2};
        spell.mData = ESM4::SpellData{4, 0, 0, 0, {}};
        spell.mEffects = {{ESM::fourCC("FOAT"), 25, 0, 0, 0, 5, {}}};
        mStore.getWritable<ESM4::Spell>().insertStatic(spell, key);
        ESM4::EffectSetting prior{};
        prior.mId = {0x800, 3};
        prior.mEffectCode = ESM::fourCC("FOAT");
        prior.mData = ESM4::EffectSettingData{};
        prior.mData->mFlags = 1 << 24;
        prior.mData->mAssociatedData = 40;
        prior.preparePassiveValueModifierDefinition();
        auto winning = prior;
        winning.mData->mFlags = 0;
        winning.mData->mAssociatedData = 55;
        winning.preparePassiveValueModifierDefinition(&prior);
        const auto definitionKey = ESM::FormKey::content("effects.esm", 0x800);
        mStore.getWritable<ESM4::EffectSetting>().insertStatic(winning, definitionKey);
        const std::array requested{key, key};
        const auto resolved = MWWorld::resolveOblivionPassiveAbilityInputs(mStore, requested);
        ASSERT_EQ(resolved.size(), 1);
        EXPECT_EQ(resolved[0].mSpell, key);
        ASSERT_EQ(resolved[0].mEffects.size(), 1);
        EXPECT_EQ(resolved[0].mEffects[0].mValues.mActorValue, 40);
        EXPECT_EQ(resolved[0].mEffects[0].mValues.mMagnitude, 25);
        EXPECT_EQ(resolved[0].mEffects[0].mValues.mDuration, 0);
        EXPECT_EQ(winning.mData->mAssociatedData, 55);
        spell.mEffects[0].mMagnitude = 75;
        mStore.getWritable<ESM4::Spell>().insertStatic(spell, key);
        EXPECT_EQ(MWWorld::resolveOblivionPassiveAbilityInputs(mStore, requested)[0].mEffects[0].mValues.mMagnitude, 75);
        ASSERT_TRUE(mStore.getWritable<ESM4::EffectSetting>().eraseStatic(definitionKey));
        EXPECT_THROW(MWWorld::resolveOblivionPassiveAbilityInputs(mStore, requested), std::invalid_argument);
    }

    TEST_F(OblivionActorStatsTest, passiveInputsRejectUnadmittedAmbiguousAndMalformedWinningRecords)
    {
        const auto key = ESM::FormKey::content("abilities.esp", 0x123);
        const std::array requested{key};
        ESM4::Spell spell{};
        spell.mId = {0x123, 2};
        spell.mData = ESM4::SpellData{4, 0, 0, 0, {}};
        spell.mEffects = {{ESM::fourCC("FOAT"), 25, 0, 0, 0, 5, {}}};
        ESM4::EffectSetting definition{};
        definition.mId = {0x800, 3};
        definition.mEffectCode = ESM::fourCC("FOAT");
        definition.mData = ESM4::EffectSettingData{};
        definition.preparePassiveValueModifierDefinition();
        const auto definitionKey = ESM::FormKey::content("effects.esm", 0x800);
        mStore.getWritable<ESM4::EffectSetting>().insertStatic(definition, definitionKey);
        const auto reject = [&](const ESM4::Spell& bad) {
            mStore.getWritable<ESM4::Spell>().insertStatic(bad, key);
            EXPECT_THROW(MWWorld::resolveOblivionPassiveAbilityInputs(mStore, requested), std::invalid_argument);
        };
        auto bad = spell; bad.mData->mType = 0; reject(bad);
        bad = spell; bad.mData.reset(); reject(bad);
        bad = spell; bad.mEffects.clear(); reject(bad);
        bad = spell; bad.mEffects[0].mRange = 1; reject(bad);
        bad = spell; bad.mEffects[0].mArea = 1; reject(bad);
        bad = spell; bad.mEffects[0].mScriptEffect.emplace(); reject(bad);
        bad = spell; bad.mEffects[0].mId = ESM::fourCC("SEFF"); reject(bad);
        bad = spell; bad.mEffects[0].mActorValue = 72; reject(bad);
        bad = spell; bad.mEffects[0].mActorValue = 8; reject(bad);
        bad = spell; bad.mEffects[0].mDuration = 1; reject(bad);
        mStore.getWritable<ESM4::Spell>().insertStatic(spell, key);
        EXPECT_NO_THROW(MWWorld::resolveOblivionPassiveAbilityInputs(mStore, requested));
        auto second = definition; second.mId = {0x801, 3};
        const auto secondKey = ESM::FormKey::content("effects.esm", 0x801);
        mStore.getWritable<ESM4::EffectSetting>().insertStatic(second, secondKey);
        EXPECT_THROW(MWWorld::resolveOblivionPassiveAbilityInputs(mStore, requested), std::invalid_argument);
        ASSERT_TRUE(mStore.getWritable<ESM4::EffectSetting>().eraseStatic(secondKey));
        definition.mPassiveValueModifierDefinition.reset();
        mStore.getWritable<ESM4::EffectSetting>().insertStatic(definition, definitionKey);
        EXPECT_THROW(MWWorld::resolveOblivionPassiveAbilityInputs(mStore, requested), std::invalid_argument);
    }

    TEST_F(OblivionActorStatsTest, playerSpellSourcesUseWinningDeclarationsStableKeysAndPreserveUnexecutedPowers)
    {
        ESM4::Npc player{}; player.mId = {7, 3}; player.mIsTES4 = true;
        const auto playerKey = ESM::FormKey::content("Oblivion.esm", 7);
        player.mFormKey = playerKey;
        ESM4::Race race{}; race.mId = {0x100, 5};
        const auto raceKey = ESM::FormKey::content("race.esm", 0x100);
        ESM4::BirthSign sign{}; sign.mId = {0x110, 4};
        const auto signKey = ESM::FormKey::content("signs.esm", 0x110);
        ESM4::EffectSetting definition{}; definition.mId = {0x812, 3};
        definition.mEffectCode = ESM::fourCC("FOAT"); definition.mData.emplace();
        definition.preparePassiveValueModifierDefinition();
        mStore.getWritable<ESM4::EffectSetting>().insertStatic(definition, ESM::FormKey::content("effects.esm", 0x812));
        std::array<ESM4::Spell, 5> spells{};
        const std::array<int, 5> slots{2, 4, 1, 6, 9};
        const std::array<unsigned, 5> types{4, 0, 2, 4, 4};
        std::array<ESM::FormKey, 5> keys;
        for (unsigned i = 0; i < spells.size(); ++i)
        {
            auto& spell = spells[i]; spell.mId = {0x200 + i, slots[i]};
            spell.mData = ESM4::SpellData{types[i], 0, 0, 0, {}};
            spell.mEffects = {{ESM::fourCC("FOAT"), 25 + i, 0, 0, 0, i, {}}};
            keys[i] = ESM::FormKey::content("spells" + std::to_string(i) + ".esm", 0x200 + i);
            mStore.getWritable<ESM4::Spell>().insertStatic(spell, keys[i]);
        }
        // Power effects are retained as declarations without entering the
        // narrow passive adapter, which would reject this scripted class.
        spells[2].mEffects[0].mId = ESM::fourCC("SEFF");
        spells[2].mEffects[0].mScriptEffect.emplace();
        mStore.getWritable<ESM4::Spell>().insertStatic(spells[2], keys[2]);
        player.mSpell = {spells[0].mId, spells[1].mId};
        race.mBonusSpells = {spells[2].mId, spells[3].mId, spells[0].mId};
        sign.mSpells = {spells[3].mId, spells[4].mId};
        mStore.getWritable<ESM4::Npc>().insertStatic(player, playerKey);
        mStore.getWritable<ESM4::Race>().insertStatic(race, raceKey);
        mStore.getWritable<ESM4::BirthSign>().insertStatic(sign, signKey);
        const auto result = MWWorld::resolveOblivionPlayerSpellInputs(mStore, race.mId, sign.mId);
        EXPECT_EQ(result.mDeclaredSpells, (std::vector<ESM::FormKey>(keys.begin(), keys.end())));
        ASSERT_EQ(result.mPassiveAbilities.size(), 3);
        EXPECT_EQ(result.mPassiveAbilities[0].mSpell, keys[0]);
        EXPECT_EQ(result.mPassiveAbilities[1].mSpell, keys[3]);
        EXPECT_EQ(result.mPassiveAbilities[2].mSpell, keys[4]);
        EXPECT_EQ(result.mPassiveAbilities[2].mEffects[0].mValues.mMagnitude, 29);
        const auto noSign = MWWorld::resolveOblivionPlayerSpellInputs(mStore, race.mId, {});
        EXPECT_EQ(noSign.mDeclaredSpells, (std::vector<ESM::FormKey>{keys[0], keys[1], keys[2], keys[3]}));
        EXPECT_EQ(noSign.mPassiveAbilities.size(), 2);
        race.mBonusSpells = {spells[4].mId, spells[0].mId};
        mStore.getWritable<ESM4::Race>().insertStatic(race, raceKey);
        spells[4].mEffects[0].mMagnitude = 75;
        mStore.getWritable<ESM4::Spell>().insertStatic(spells[4], keys[4]);
        const auto winning = MWWorld::resolveOblivionPlayerSpellInputs(mStore, race.mId, {});
        EXPECT_EQ(winning.mDeclaredSpells, (std::vector<ESM::FormKey>{keys[0], keys[1], keys[4]}));
        EXPECT_EQ(winning.mPassiveAbilities[1].mEffects[0].mValues.mMagnitude, 75);
        EXPECT_EQ(result.mPassiveAbilities[2].mEffects[0].mValues.mMagnitude, 29);
    }

    TEST_F(OblivionActorStatsTest, playerSpellSourcesRejectMissingMalformedDeletedAndUnadmittedSources)
    {
        ESM4::Npc player{}; player.mId = {7, 3}; player.mIsTES4 = true;
        const auto playerKey = ESM::FormKey::content("Oblivion.esm", 7);
        player.mFormKey = playerKey;
        ESM4::Race race{}; race.mId = {0x100, 5};
        const auto raceKey = ESM::FormKey::content("race.esm", 0x100);
        ESM4::BirthSign sign{}; sign.mId = {0x110, 4};
        const auto signKey = ESM::FormKey::content("signs.esm", 0x110);
        ESM4::Spell spell{}; spell.mId = {0x200, 2};
        const auto spellKey = ESM::FormKey::content("spells.esm", 0x200);
        spell.mData = ESM4::SpellData{0, 0, 0, 0, {}};
        race.mBonusSpells = {spell.mId};
        mStore.getWritable<ESM4::Race>().insertStatic(race, raceKey);
        mStore.getWritable<ESM4::BirthSign>().insertStatic(sign, signKey);
        mStore.getWritable<ESM4::Spell>().insertStatic(spell, spellKey);
        const auto resolve = [&] { return MWWorld::resolveOblivionPlayerSpellInputs(mStore, race.mId, sign.mId); };
        EXPECT_THROW(resolve(), std::invalid_argument); // Missing native Player.
        player.mIsTES4 = false;
        mStore.getWritable<ESM4::Npc>().insertStatic(player, playerKey);
        EXPECT_THROW(resolve(), std::invalid_argument);
        player.mIsTES4 = true;
        mStore.getWritable<ESM4::Npc>().insertStatic(player, playerKey);
        EXPECT_THROW(MWWorld::resolveOblivionPlayerSpellInputs(mStore, {}, sign.mId), std::invalid_argument);
        EXPECT_THROW(MWWorld::resolveOblivionPlayerSpellInputs(mStore, race.mId,
            ESM::RefId::stringRefId("missing-sign")), std::invalid_argument);
        race.mBonusSpells.push_back({});
        mStore.getWritable<ESM4::Race>().insertStatic(race, raceKey);
        EXPECT_THROW(resolve(), std::invalid_argument);
        race.mBonusSpells.pop_back();
        mStore.getWritable<ESM4::Race>().insertStatic(race, raceKey);
        for (const unsigned type : {1u, 5u, 0xffffffffu, 4u})
        {
            spell.mData->mType = type; // Disease, invalid kinds, empty Ability.
            mStore.getWritable<ESM4::Spell>().insertStatic(spell, spellKey);
            EXPECT_THROW(resolve(), std::invalid_argument);
        }
        spell.mData.reset();
        mStore.getWritable<ESM4::Spell>().insertStatic(spell, spellKey);
        EXPECT_THROW(resolve(), std::invalid_argument);
        EXPECT_TRUE(mStore.getWritable<ESM4::Spell>().eraseStatic(spellKey));
        EXPECT_THROW(resolve(), std::invalid_argument);
        // Empty declarations are legitimate and don't invent active effects.
        race.mBonusSpells.clear();
        mStore.getWritable<ESM4::Race>().insertStatic(race, raceKey);
        EXPECT_TRUE(resolve().mDeclaredSpells.empty());
        EXPECT_TRUE(resolve().mPassiveAbilities.empty());
    }

    TEST_F(OblivionActorStatsTest, playerPassiveGrantOwnsClampedBaseWritesAtomicallyAndSurvivesRestart)
    {
        sharedStats();
        ESM::NPC base{}; base.blank(); base.mId = ESM::RefId::stringRefId("Player");
        const auto* record = mStore.insertStatic(base);
        MWBase::Environment environment; environment.setESMStore(mStore);
        ESM::ReadersCache readers; MWWorld::WorldModel model(mStore, readers); environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        MWWorld::Player player(record); ESM::NpcState initial{}; initial.blank();
        const auto ptr = player.getPlayer(); ptr.getClass().readAdditionalState(ptr, initial);
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{100, 30, 40, 0}};
        values.mPassiveAbilities.emplace();
        for (auto& value : values.mValues) value.mModifiers = {0, 0, 0};
        for (unsigned av = 0; av < 8; ++av) values.mValues[av].mBase = 50;
        values.mValues[0].mModifiers = {7, 13, -60}; // Current10, raw form50.
        values.mValues[8].mModifiers[2] = -10;
        const ESM4::PlayerDynamicBaseSettings settings{2, 1.5f, 5};
        MWMechanics::OblivionCombatService service;
        service.publishPlayerValues(player, values, settings);
        service.publishPlayerLife(player, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        const auto spell = ESM::FormKey::content("abilities.esp", 0x123);
        EXPECT_EQ(service.getPlayerValue(9), 155); // Raw30 + Intelligence50 * (1 + GMST1.5).
        std::array abilities{ESM4::PassiveAbilityInput{spell, {
            {0, ESM::fourCC("FOAT"), 0x100072, {0, -25, 0}},
            {1, ESM::fourCC("WKFI"), 0x100007f, {61, 25, 0}},
            {2, ESM::fourCC("FOSP"), 0x1000072, {9, 150, 0}},
            {3, ESM::fourCC("STMA"), 0x1000112, {57, 1, 0}},
            {4, ESM::fourCC("WABR"), 0x1000172, {55, 1, 0}}}}};
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = values.mActor;
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        const auto snapshot = [&] {service.capture(saved); return saved.serializeBinary();};
        const auto before = snapshot();
        auto invalidSettings = settings; invalidSettings.mHealthMultiplier = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(service.grantPlayerPassiveAbilities(player, abilities, invalidSettings), std::invalid_argument);
        EXPECT_EQ(snapshot(), before);
        EXPECT_EQ(service.getPlayerBaseValue(0), 50);
        auto invalid = abilities; invalid[0].mEffects.back().mValues.mMagnitude = std::numeric_limits<float>::infinity();
        EXPECT_THROW(service.grantPlayerPassiveAbilities(player, invalid, settings), std::runtime_error);
        EXPECT_EQ(snapshot(), before);
        service.grantPlayerPassiveAbilities(player, abilities, settings);
        const auto committed = *service.findActorValues(values.mActor);
        ASSERT_TRUE(committed.mPassiveAbilities);
        ASSERT_EQ(committed.mPassiveAbilities->size(), 1);
        EXPECT_EQ(committed.mPassiveAbilities->front().mEffects.front().mStoredMagnitude, -10);
        EXPECT_EQ(committed.mPassiveAbilities->front().mEffects.front().mInitialMagnitude, -25);
        EXPECT_EQ(committed.mPassiveAbilities->front().mEffects[1].mInitialMagnitude, 25);
        auto downgrade = saved; downgrade.mVersion = 18;
        const auto beforeDowngrade = downgrade.canonicalJson();
        EXPECT_THROW(service.capture(downgrade), std::invalid_argument);
        EXPECT_EQ(downgrade.canonicalJson(), beforeDowngrade);
        EXPECT_EQ(committed.mValues[0].mBase, 40);
        EXPECT_EQ(service.getPlayerValue(0), 0);
        EXPECT_EQ(committed.mValues[61].mBase, -25);
        EXPECT_EQ((*committed.mPlayerFormValues)[1], 180);
        EXPECT_EQ(service.getPlayerValue(9), 305);
        EXPECT_EQ(committed.mValues[57].mBase, 1);
        EXPECT_EQ(committed.mValues[55].mBase, 1);
        for (std::size_t av = 0; av < 72; ++av) EXPECT_EQ(committed.mValues[av].mModifiers, values.mValues[av].mModifiers);
        EXPECT_FALSE(service.takeNextDeathEvent());
        const auto bytes = snapshot();
        abilities[0].mEffects[2].mValues.mMagnitude = 900;
        service.grantPlayerPassiveAbilities(player, abilities, settings);
        EXPECT_EQ(snapshot(), bytes); // Deduplicate saved active ownership, not fresh winning magnitudes.
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(bytes));
        MWWorld::Player fresh(record); const auto freshPtr = fresh.getPlayer();
        freshPtr.getClass().readAdditionalState(freshPtr, initial);
        restored.publishPlayerValues(fresh, *restored.findActorValues(values.mActor), settings);
        restored.grantPlayerPassiveAbilities(fresh, abilities, settings);
        EXPECT_EQ(*restored.findActorValues(values.mActor), committed);
        EXPECT_EQ(freshPtr.getClass().getCreatureStats(freshPtr).getMagicka().getCurrent(), 305);
        auto legacy = committed; legacy.mPassiveAbilities.reset();
        restored.publishPlayerValues(fresh, legacy, settings);
        EXPECT_THROW(restored.grantPlayerPassiveAbilities(fresh, abilities, settings), std::invalid_argument);
        EXPECT_EQ(*restored.findActorValues(values.mActor), legacy);
        auto older = saved; older.mVersion = 17; older.mNativeActorValues.clear(); older.mNativeActorBases.clear();
        const auto oldBytes = older.serializeBinary();
        EXPECT_THROW(service.capture(older), std::invalid_argument);
        EXPECT_EQ(older.serializeBinary(), oldBytes); // Capture schema preflight precedes any publication.
    }

    TEST_F(OblivionActorStatsTest, playerPassiveEnduranceReactionStagesLifeEventsAndEssentialRecovery)
    {
        sharedStats();
        ESM::NPC base{}; base.blank(); base.mId = ESM::RefId::stringRefId("Player");
        const auto* record = mStore.insertStatic(base);
        MWBase::Environment environment; environment.setESMStore(mStore);
        ESM::ReadersCache readers; MWWorld::WorldModel model(mStore, readers); environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        MWWorld::Player player(record); ESM::NpcState initial{}; initial.blank();
        const auto ptr = player.getPlayer(); ptr.getClass().readAdditionalState(ptr, initial);
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{100, 0, 0, 0}};
        values.mPassiveAbilities.emplace();
        for (auto& value : values.mValues) value.mModifiers = {0, 0, 0};
        for (unsigned av = 0; av < 8; ++av) values.mValues[av].mBase = 50;
        values.mValues[8].mModifiers[2] = -250;
        const ESM4::PlayerDynamicBaseSettings settings{2, 1.5f, 5};
        std::array abilities{ESM4::PassiveAbilityInput{ESM::FormKey::content("abilities.esp", 0x123),
            {{0, ESM::fourCC("FOAT"), 0x100072, {5, 5, 0}}}}};
        for (const float magnitude : {-5.f, 0.f, 5.f})
        for (const float damage : {-250.f, -189.f, -188.f})
        for (const bool essential : {false, true})
        {
            SCOPED_TRACE(magnitude);
            SCOPED_TRACE(damage);
            SCOPED_TRACE(essential);
            abilities[0].mEffects[0].mValues.mMagnitude = magnitude;
            values.mValues[8].mModifiers[2] = damage;
            const float baseHealth = 200 + 2 * magnitude;
            const float currentHealth = baseHealth + damage;
            const bool reacts = magnitude < 0 && currentHealth <= 1;
            MWMechanics::OblivionCombatService service;
            service.publishPlayerValues(player, values, settings);
            service.publishPlayerLife(player, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
            const auto ownedAction = service.allocateAction(values.mActor);
            if (essential && reacts)
            {
                const auto before = *service.findActorValues(values.mActor);
                EXPECT_THROW(service.grantPlayerPassiveAbilities(player, abilities, settings, true,
                    {std::numeric_limits<float>::quiet_NaN(), .1f}), std::invalid_argument);
                EXPECT_EQ(*service.findActorValues(values.mActor), before);
                EXPECT_EQ(service.findActorLife(values.mActor)->mPhase, ESM4::ActorLifePhase::Alive);
                EXPECT_FALSE(service.takeNextDeathEvent());
                EXPECT_TRUE(service.isActionPending(ownedAction, values.mActor));
            }
            service.grantPlayerPassiveAbilities(player, abilities, settings, essential, {10, .1f});
            EXPECT_EQ(service.getPlayerBaseValue(5), 50 + magnitude);
            const auto* life = service.findActorLife(values.mActor);
            ASSERT_NE(life, nullptr);
            EXPECT_EQ(life->mPhase, !reacts ? ESM4::ActorLifePhase::Alive : essential ? ESM4::ActorLifePhase::EssentialUnconscious : ESM4::ActorLifePhase::Dead);
            EXPECT_EQ(life->mKiller, reacts ? values.mActor : ESM::FormKey{});
            EXPECT_EQ(service.isActionPending(ownedAction, values.mActor), !reacts);
            EXPECT_EQ(service.isActionConsumed(ownedAction), reacts);
            EXPECT_EQ(service.getPlayerValue(8), essential && reacts ? 19 : currentHealth);
            if (!reacts || essential)
            {
                EXPECT_EQ(life->mRecoveryRemaining, reacts ? 10 : 0);
                EXPECT_EQ(service.getDeadCount(values.mBase), 0);
                EXPECT_FALSE(service.takeNextDeathEvent());
            }
            else
            {
                EXPECT_EQ(service.getDeadCount(values.mBase), 1);
                const auto event = service.takeNextDeathEvent();
                ASSERT_TRUE(event);
                EXPECT_EQ(event->mActor, values.mActor);
                EXPECT_EQ(event->mKiller, values.mActor);
            }
            const auto committed = *service.findActorValues(values.mActor);
            service.grantPlayerPassiveAbilities(player, abilities, settings, essential, {10, .1f});
            EXPECT_EQ(*service.findActorValues(values.mActor), committed);
            EXPECT_FALSE(service.takeNextDeathEvent());
        }
    }

    TEST_F(OblivionActorStatsTest, playerPassiveRemovalUsesSavedMagnitudePreservesOtherWritesAndRollsBack)
    {
        sharedStats();
        ESM::NPC base{}; base.blank(); base.mId = ESM::RefId::stringRefId("Player");
        const auto* record = mStore.insertStatic(base);
        MWBase::Environment environment; environment.setESMStore(mStore);
        ESM::ReadersCache readers; MWWorld::WorldModel model(mStore, readers); environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        MWWorld::Player player(record); ESM::NpcState initial{}; initial.blank();
        const auto ptr = player.getPlayer(); ptr.getClass().readAdditionalState(ptr, initial);
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{100, 30, 40, 0}};
        values.mPassiveAbilities.emplace();
        for (auto& value : values.mValues) value.mModifiers = {0, 0, 0};
        for (unsigned av = 0; av < 8; ++av) values.mValues[av].mBase = 50;
        values.mValues[0].mModifiers[2] = -80;
        const ESM4::PlayerDynamicBaseSettings settings{2, 1.5f, 5};
        MWMechanics::OblivionCombatService service;
        service.publishPlayerValues(player, values, settings);
        service.publishPlayerLife(player, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        const auto spell = ESM::FormKey::content("abilities.esp", 0x123);
        std::array abilities{ESM4::PassiveAbilityInput{spell, {
            {7, ESM::fourCC("FOAT"), 0x100072, {0, 25, 0}},
            {2, ESM::fourCC("FOSP"), 0x1000072, {9, 150, 0}}}}};
        service.grantPlayerPassiveAbilities(player, abilities, settings);
        service.setPlayerBaseValue(player, 0, 82, settings); // Independent +7 persists.
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = values.mActor;
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        service.capture(saved);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        MWWorld::Player fresh(record); fresh.getPlayer().getClass().readAdditionalState(fresh.getPlayer(), initial);
        restored.publishPlayerValues(fresh, *restored.findActorValues(values.mActor), settings);
        const auto snapshot = [&] { restored.capture(saved); return saved.serializeBinary(); };
        const auto before = snapshot();
        auto invalid = settings; invalid.mHealthMultiplier = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(restored.removePlayerPassiveEffect(fresh, spell, 7, invalid), std::invalid_argument);
        EXPECT_EQ(snapshot(), before);
        EXPECT_THROW(restored.removePlayerPassiveEffect(fresh, {}, 7, settings), std::invalid_argument);
        EXPECT_EQ(snapshot(), before);
        restored.removePlayerPassiveEffect(fresh, spell, 999, settings);
        EXPECT_EQ(snapshot(), before);
        restored.removePlayerPassiveEffect(fresh, spell, 7, settings);
        EXPECT_EQ(restored.getPlayerBaseValue(0), 57);
        EXPECT_EQ(restored.getPlayerValue(0), 0);
        EXPECT_EQ(restored.findActorValues(values.mActor)->mValues[0].mModifiers[2], -57);
        ASSERT_EQ(restored.findActorValues(values.mActor)->mPassiveAbilities->front().mEffects.size(), 1);
        EXPECT_EQ(restored.findActorValues(values.mActor)->mPassiveAbilities->front().mEffects.front().mEffectIndex, 2);
        const auto removed = snapshot();
        restored.removePlayerPassiveEffect(fresh, spell, 7, settings);
        EXPECT_EQ(snapshot(), removed);
        restored.removePlayerPassiveEffect(fresh, spell, 2, settings);
        EXPECT_EQ(restored.getPlayerBaseValue(9), 155);
        EXPECT_EQ((*restored.findActorValues(values.mActor)->mPlayerFormValues)[1], 30);
        EXPECT_TRUE(restored.findActorValues(values.mActor)->mPassiveAbilities->empty());
        abilities[0].mEffects[0].mValues.mMagnitude = 5;
        restored.grantPlayerPassiveAbilities(fresh, abilities, settings);
        EXPECT_EQ(restored.getPlayerBaseValue(0), 62);
        EXPECT_EQ(restored.findActorValues(values.mActor)->mPassiveAbilities->front().mEffects.front().mStoredMagnitude, 5);
        auto unknown = *restored.findActorValues(values.mActor); unknown.mPassiveAbilities.reset();
        restored.publishPlayerValues(fresh, unknown, settings);
        const auto legacy = snapshot();
        EXPECT_THROW(restored.removePlayerPassiveEffect(fresh, spell, 7, settings), std::invalid_argument);
        EXPECT_EQ(snapshot(), legacy);
        EXPECT_FALSE(restored.takeNextDeathEvent());
    }

    TEST_F(OblivionActorStatsTest, playerPassiveRemovalCompensatesDamageAndTerminalEnduranceWithoutNewDeath)
    {
        sharedStats();
        ESM::NPC base{}; base.blank(); base.mId = ESM::RefId::stringRefId("Player");
        const auto* record = mStore.insertStatic(base);
        MWBase::Environment environment; environment.setESMStore(mStore);
        ESM::ReadersCache readers; MWWorld::WorldModel model(mStore, readers); environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        MWWorld::Player player(record); ESM::NpcState initial{}; initial.blank();
        const auto ptr = player.getPlayer(); ptr.getClass().readAdditionalState(ptr, initial);
        const auto spell = ESM::FormKey::content("abilities.esp", 0x123);
        const ESM4::PlayerDynamicBaseSettings settings{2, 1.5f, 5};
        for (const auto phase : {ESM4::ActorLifePhase::Alive, ESM4::ActorLifePhase::Dead,
                ESM4::ActorLifePhase::EssentialUnconscious})
        for (const float stored : {-25.f, -0.f, 0.f, 25.f})
        for (const bool godMode : {false, true})
        {
            SCOPED_TRACE(static_cast<int>(phase));
            SCOPED_TRACE(stored);
            SCOPED_TRACE(godMode);
            ESM4::RuntimeActorValues values;
            values.mActor = ESM::FormKey::dynamic("player", 1);
            values.mBase = ESM::FormKey::dynamic("player-base", 1);
            values.mOwner = ESM4::ActorValueOwner::Player;
            values.mPlayerFormValues = {{100, 0, 0, 0}};
            values.mPassiveAbilities = std::vector<ESM4::RuntimePassiveAbility>{
                {spell, {{3, ESM::fourCC("FOAT"), 5, stored}}}};
            for (auto& value : values.mValues) value.mModifiers = {0, 0, 0};
            for (unsigned av = 0; av < 8; ++av) values.mValues[av].mBase = 50;
            values.mValues[5].mBase = 50 + stored;
            values.mValues[5].mModifiers[2] = -80;
            values.mValues[8].mModifiers[2] = -250;
            MWMechanics::OblivionCombatService service;
            service.publishPlayerValues(player, values, settings);
            service.publishPlayerLife(player, {values.mActor, values.mBase, phase, 0, {}});
            const auto* life = service.findActorLife(values.mActor);
            const auto beforeLife = *life;
            while (service.takeNextDeathEvent()) {}
            service.removePlayerPassiveEffect(player, spell, 3, settings, godMode);
            const auto& after = *service.findActorValues(values.mActor);
            EXPECT_EQ(service.getPlayerBaseValue(5), 50);
            EXPECT_EQ(after.mValues[5].mModifiers[2], stored > 0 ? -50 : -80);
            // Derived Health follows current integer Endurance, including its
            // remaining Damage. The native integer getter can be negative.
            const float baseHealth = stored > 0 ? 100.f : 40.f;
            EXPECT_EQ(after.mValues[8].mBase, baseHealth);
            EXPECT_EQ(after.mValues[8].mModifiers[2], phase != ESM4::ActorLifePhase::Alive && !godMode
                ? -250 - baseHealth : -250);
            EXPECT_EQ(*service.findActorLife(values.mActor), beforeLife);
            EXPECT_TRUE(after.mPassiveAbilities->empty());
            EXPECT_FALSE(service.takeNextDeathEvent());
        }
    }

    TEST_F(OblivionActorStatsTest, playerCharacterReplacementCombinesRemovalBaseResetAndGrantAtomically)
    {
        sharedStats();
        ESM::NPC base{}; base.blank(); base.mId = ESM::RefId::stringRefId("Player");
        const auto* record = mStore.insertStatic(base);
        MWBase::Environment environment; environment.setESMStore(mStore);
        ESM::ReadersCache readers; MWWorld::WorldModel model(mStore, readers); environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        MWWorld::Player player(record); ESM::NpcState initial{}; initial.blank();
        const auto ptr = player.getPlayer(); ptr.getClass().readAdditionalState(ptr, initial);
        ESM4::RuntimeActorValues values;
        values.mActor = ESM::FormKey::dynamic("player", 1);
        values.mBase = ESM::FormKey::dynamic("player-base", 1);
        values.mOwner = ESM4::ActorValueOwner::Player;
        values.mPlayerFormValues = {{100, 30, 40, 0}};
        values.mPassiveAbilities.emplace();
        for (auto& value : values.mValues) value.mModifiers = {0, 0, 0};
        for (unsigned av = 0; av < 8; ++av) values.mValues[av].mBase = 50;
        values.mValues[0].mModifiers[2] = -80;
        values.mValues[8].mModifiers[2] = -20;
        const ESM4::PlayerDynamicBaseSettings settings{2, 1.5f, 5};
        MWMechanics::OblivionCombatService service;
        service.publishPlayerValues(player, values, settings);
        service.publishPlayerLife(player, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        const auto spell = ESM::FormKey::content("abilities.esp", 0x123);
        std::array abilities{ESM4::PassiveAbilityInput{spell, {
            {7, ESM::fourCC("FOAT"), 0x100072, {0, 25, 0}},
            {2, ESM::fourCC("FOSP"), 0x1000072, {9, 150, 0}}}}};
        service.grantPlayerPassiveAbilities(player, abilities, settings);
        const auto action = service.allocateAction();
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = values.mActor;
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        const auto snapshot = [&] {service.capture(saved); return saved.serializeBinary();};
        const auto before = snapshot();
        const auto oldView = ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent();
        ESM4::ActorCharacterBaseStats character{}; character.mAttributes.fill(40); character.mSkills.fill(5);
        std::array order{MWMechanics::OblivionPassiveEffectIdentity{spell, 2},
            MWMechanics::OblivionPassiveEffectIdentity{spell, 7}};
        abilities[0].mEffects[0].mValues.mMagnitude = 5;
        abilities[0].mEffects[1].mValues.mMagnitude = 100;
        auto invalid = abilities; invalid[0].mEffects.back().mValues.mMagnitude = std::numeric_limits<float>::infinity();
        EXPECT_THROW(service.replacePlayerCharacter(player, character, invalid, order, settings), std::runtime_error);
        EXPECT_EQ(snapshot(), before);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), oldView);
        auto missing = std::span(order).first(1);
        EXPECT_THROW(service.replacePlayerCharacter(player, character, abilities, missing, settings), std::invalid_argument);
        EXPECT_EQ(snapshot(), before);
        auto duplicate = order; duplicate[1] = duplicate[0];
        EXPECT_THROW(service.replacePlayerCharacter(player, character, abilities, duplicate, settings), std::invalid_argument);
        EXPECT_EQ(snapshot(), before);
        auto wrong = order; wrong[1].mEffectIndex = 999;
        EXPECT_THROW(service.replacePlayerCharacter(player, character, abilities, wrong, settings), std::invalid_argument);
        EXPECT_EQ(snapshot(), before);
        service.replacePlayerCharacter(player, character, abilities, order, settings);
        const auto& after = *service.findActorValues(values.mActor);
        EXPECT_EQ(service.getPlayerBaseValue(0), 45);
        EXPECT_EQ(after.mValues[0].mModifiers[2], -50);
        EXPECT_EQ(service.getPlayerValue(0), -5);
        EXPECT_EQ(service.getPlayerBaseValue(12), 5);
        EXPECT_EQ((*after.mPlayerFormValues)[1], 130);
        EXPECT_EQ(service.getPlayerValue(9), 230);
        EXPECT_EQ(service.getPlayerValue(8), 160);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), 160);
        EXPECT_EQ(after.mPassiveAbilities->front().mEffects.front().mStoredMagnitude, 5);
        EXPECT_EQ(after.mPassiveAbilities->front().mEffects.back().mStoredMagnitude, 100);
        EXPECT_TRUE(service.isActionPending(action));
        EXPECT_FALSE(service.takeNextDeathEvent());
        service.capture(saved);
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()));
        EXPECT_EQ(*restored.findActorValues(values.mActor), after);
        EXPECT_TRUE(restored.isActionPending(action));
        // A final removal-only choice empties ownership and preserves raw30.
        service.replacePlayerCharacter(player, character, {}, order, settings);
        EXPECT_TRUE(service.findActorValues(values.mActor)->mPassiveAbilities->empty());
        EXPECT_EQ((*service.findActorValues(values.mActor)->mPlayerFormValues)[1], 30);
        auto unknown = *service.findActorValues(values.mActor); unknown.mPassiveAbilities.reset();
        service.publishPlayerValues(player, unknown, settings);
        const auto legacy = snapshot();
        EXPECT_THROW(service.replacePlayerCharacter(player, character, {}, {}, settings), std::invalid_argument);
        EXPECT_EQ(snapshot(), legacy);
        unknown.mPassiveAbilities.emplace();
        MWMechanics::OblivionCombatService noLife;
        noLife.publishPlayerValues(player, unknown, settings);
        EXPECT_THROW(noLife.replacePlayerCharacter(player, character, {}, {}, settings), std::invalid_argument);
        EXPECT_EQ(*noLife.findActorValues(values.mActor), unknown);
    }

    TEST_F(OblivionActorStatsTest, playerCharacterReplacementCommitsLifeOnceAndRollsBackLaterGrantFailure)
    {
        sharedStats();
        ESM::NPC base{}; base.blank(); base.mId = ESM::RefId::stringRefId("Player");
        const auto* record = mStore.insertStatic(base);
        MWBase::Environment environment; environment.setESMStore(mStore);
        ESM::ReadersCache readers; MWWorld::WorldModel model(mStore, readers); environment.setWorldModel(model);
        MWClass::Npc::registerSelf();
        MWWorld::Player player(record); ESM::NpcState initial{}; initial.blank();
        const auto ptr = player.getPlayer(); ptr.getClass().readAdditionalState(ptr, initial);
        const ESM4::PlayerDynamicBaseSettings settings{2, 1.5f, 5};
        const auto spell = ESM::FormKey::content("abilities.esp", 0x123);
        const auto later = ESM::FormKey::content("abilities.esp", 0x124);
        for (const bool essential : {false, true})
        for (const bool godMode : {false, true})
        for (const int rawHealth : {-69, 100})
        {
            SCOPED_TRACE(essential);
            SCOPED_TRACE(godMode);
            SCOPED_TRACE(rawHealth);
            ESM4::RuntimeActorValues values;
            values.mActor = ESM::FormKey::dynamic("player", 1);
            values.mBase = ESM::FormKey::dynamic("player-base", 1);
            values.mOwner = ESM4::ActorValueOwner::Player;
            values.mPlayerFormValues = {{rawHealth, 0, 0, 0}};
            values.mPassiveAbilities.emplace();
            for (auto& value : values.mValues) value.mModifiers = {0, 0, 0};
            for (unsigned av = 0; av < 8; ++av) values.mValues[av].mBase = 50;
            values.mValues[8].mModifiers[2] = rawHealth < 0 ? -.5f : -250.f;
            MWMechanics::OblivionCombatService service;
            service.publishPlayerValues(player, values, settings);
            service.publishPlayerLife(player, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
            std::array abilities{ESM4::PassiveAbilityInput{spell,
                {{3, ESM::fourCC("FOAT"), 0x100072, {5, 25, 0}}}}};
            service.grantPlayerPassiveAbilities(player, abilities, settings);
            ESM4::RuntimeState saved;
            saved.mPlayer.mReference = values.mActor;
            saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
            saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
            saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
            const auto snapshot = [&] {service.capture(saved); return saved.serializeBinary();};
            const auto before = snapshot();
            const auto oldView = ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent();
            ESM4::ActorCharacterBaseStats character{}; character.mAttributes.fill(40); character.mSkills.fill(5);
            std::array order{MWMechanics::OblivionPassiveEffectIdentity{spell, 3}};
            abilities[0].mEffects[0].mValues.mMagnitude = -5;
            std::array failed{abilities[0], ESM4::PassiveAbilityInput{later,
                {{0, ESM::fourCC("STMA"), 0x1000112, {57, 1, 0}}}}};
            // The first new spell stages terminal entry. The second cannot
            // grant to that life state; neither entry nor facade can leak.
            EXPECT_THROW(service.replacePlayerCharacter(player, character, failed, order,
                settings, essential, {10, .1f}, godMode), std::invalid_argument);
            EXPECT_EQ(snapshot(), before);
            EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), oldView);
            EXPECT_FALSE(service.takeNextDeathEvent());
            service.replacePlayerCharacter(player, character, abilities, order,
                settings, essential, {10, .1f}, godMode);
            EXPECT_EQ(service.findActorLife(values.mActor)->mPhase,
                essential ? ESM4::ActorLifePhase::EssentialUnconscious : ESM4::ActorLifePhase::Dead);
            EXPECT_EQ(service.findActorLife(values.mActor)->mKiller, values.mActor);
            EXPECT_EQ(service.getDeadCount(values.mBase), essential ? 0 : 1);
            if (essential)
            {
                EXPECT_FLOAT_EQ(service.getPlayerValue(8), rawHealth > 0 ? 17.f : godMode ? .5f : .1f);
                EXPECT_EQ(service.findActorLife(values.mActor)->mRecoveryRemaining, 10);
            }
            else
                EXPECT_FLOAT_EQ(service.getPlayerValue(8), rawHealth > 0 ? -80.f : .5f);
            const auto event = service.takeNextDeathEvent();
            EXPECT_EQ(event.has_value(), !essential);
            if (event)
            {
                EXPECT_EQ(event->mKiller, values.mActor);
            }
            EXPECT_FALSE(service.takeNextDeathEvent());
        }
    }

    TEST_F(OblivionActorStatsTest, playerCharacterBaseUsesWinningNativeSexSkillsOrderedBonusesAndFullClassId)
    {
        autoNpc();
        const auto female = MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, mNpc.mClass, true, 1);
        EXPECT_EQ(female.mAttributes,
            (std::array<std::uint8_t, 8>{42, 28, 71, 80, 56, 56, 40, 69}));
        EXPECT_EQ(female.mSkills[0], 25);
        EXPECT_EQ(female.mSkills[1], 30);
        EXPECT_EQ(female.mSkills[5], 29); // Both signed ordered bonuses match AV17.
        auto race = *mStore.get<ESM4::Race>().find(mNpc.mRace);
        race.mSkillBonus[ESM4::Race::Skill_HandToHand] = 200; // Legacy lossy map cannot supply native values.
        race.mAttribMale = {60, 60, 60, 60, 60, 60, 60, 60};
        mStore.getWritable<ESM4::Race>().insertStatic(race, ESM::FormKey::content("race.esm", 0x100));
        EXPECT_EQ(MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, mNpc.mClass, true, 1), female);
        const auto male = MWWorld::resolveOblivionPlayerCharacterBaseStats(mStore, mNpc.mRace, mNpc.mClass, false, 1);
        EXPECT_EQ(male.mAttributes[0], 66); // Secondary6.5 rounds66.5 to even66.
        EXPECT_EQ(male.mAttributes[6], 66); // Primary5.5 rounds65.5 to even66.
        ESM4::GameSetting temporary{};
        temporary.mId = {0x3333, 6};
        temporary.mEditorId = "iClassCharactergenClass";
        temporary.mData = std::int32_t(0x200); // Same local ID, different content index.
        mStore.getWritable<ESM4::GameSetting>().insertStatic(temporary);
        EXPECT_EQ(MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, mNpc.mClass, true, 1), female);
        temporary.mData = std::int32_t(mNpc.mClass.toUint32());
        mStore.getWritable<ESM4::GameSetting>().insertStatic(temporary);
        const auto initial = MWWorld::resolveOblivionPlayerCharacterBaseStats(mStore, mNpc.mRace, mNpc.mClass, true, 1);
        EXPECT_EQ(initial.mAttributes, (std::array<std::uint8_t, 8>{36, 28, 71, 80, 56, 56, 35, 69}));
        EXPECT_EQ(initial.mSkills[0], 5);
        EXPECT_EQ(initial.mSkills[1], 5);
        EXPECT_EQ(initial.mSkills[5], 24);
        auto skill = mSkills[0];
        skill.mData->mSpecialization = 0;
        mStore.getWritable<ESM4::Skill>().insertStatic(skill, ESM::FormKey::content("skills.esm", 0x1000));
        temporary.mData = std::int32_t(0);
        mStore.getWritable<ESM4::GameSetting>().insertStatic(temporary);
        EXPECT_EQ(MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, mNpc.mClass, true, 1).mSkills[0], 30);
    }

    TEST_F(OblivionActorStatsTest, playerCharacterBaseCalculatesPreparedCustomClassWithoutPublishingRecords)
    {
        autoNpc();
        const auto expected = MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, mNpc.mClass, true, 1);
        ESM::NPC player{}; player.blank(); player.mId = ESM::RefId::stringRefId("Player");
        player.mRace = mNpc.mRace;
        player.mClass = mNpc.mClass;
        mStore.insertStatic(player);
        ESM::Class custom{}; custom.blank();
        custom.mData.mAttribute = {ESM::Attribute::Personality, ESM::Attribute::Strength};
        custom.mData.mSpecialization = 0;
        const auto& ids = MWWorld::oblivionSkillIds();
        const std::array<unsigned, 7> indices{0, 1, 3, 13, 6, 15, 16};
        for (std::size_t i = 0; i < 5; ++i) custom.mData.mSkills[i][1] = ids[indices[i]];
        custom.mData.mSkills[0][0] = ids[indices[5]];
        custom.mData.mSkills[1][0] = ids[indices[6]];
        {
            auto prepared = mStore.preparePlayerRecord(player, &custom);
            const auto id = prepared.player().mClass;
            EXPECT_EQ(MWWorld::resolveOblivionPlayerCharacterBaseStats(
                mStore, mNpc.mRace, id, true, 1, prepared.customClass()), expected);
            EXPECT_EQ(mStore.find(id), 0);
            EXPECT_EQ(mStore.get<ESM::Class>().getDynamicSize(), 0);
            EXPECT_EQ(mStore.get<ESM::NPC>().find(player.mId)->mClass, player.mClass);
            EXPECT_THROW(MWWorld::resolveOblivionPlayerCharacterBaseStats(
                mStore, mNpc.mRace, id, true, 1), std::invalid_argument);
            EXPECT_THROW(MWWorld::resolveOblivionPlayerCharacterBaseStats(
                mStore, mNpc.mRace, ESM::RefId::generated(123), true, 1, prepared.customClass()), std::invalid_argument);
            auto malformed = *prepared.customClass();
            malformed.mData.mSkills[0][1] = ESM::RefId::stringRefId("invalid-major");
            EXPECT_THROW(MWWorld::resolveOblivionPlayerCharacterBaseStats(
                mStore, mNpc.mRace, id, true, 1, &malformed), std::invalid_argument);
        }
        EXPECT_EQ(mStore.generateId(), ESM::RefId::generated(0));
    }

    TEST_F(OblivionActorStatsTest, playerCharacterBaseAdmitsCustomClassProjectionWithNativeRounding)
    {
        autoNpc();
        const auto expected = MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, mNpc.mClass, true, 1);
        ESM::Class custom{}; custom.blank(); custom.mId = ESM::RefId::stringRefId("custom-player-class");
        custom.mData.mAttribute = {ESM::Attribute::Personality, ESM::Attribute::Strength};
        custom.mData.mSpecialization = 0;
        const auto& ids = MWWorld::oblivionSkillIds();
        const std::array<unsigned, 7> indices{0, 1, 3, 13, 6, 15, 16};
        for (std::size_t i = 0; i < 5; ++i) custom.mData.mSkills[i][1] = ids[indices[i]];
        custom.mData.mSkills[0][0] = ids[indices[5]];
        custom.mData.mSkills[1][0] = ids[indices[6]];
        mStore.getWritable<ESM::Class>().insertStatic(custom);
        EXPECT_EQ(MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, custom.mId, true, 1), expected);
        ESM4::GameSetting temporary{}; temporary.mId = {0x3333, 6};
        temporary.mEditorId = "iClassCharactergenClass"; temporary.mData = std::int32_t(0);
        mStore.getWritable<ESM4::GameSetting>().insertStatic(temporary);
        // A custom string ID is not native FormId0 and never inherits its
        // temporary-class suppression. Spare shared minor slots are ignored.
        custom.mData.mSkills[2][0] = ESM::RefId::stringRefId("not-a-native-skill");
        mStore.getWritable<ESM::Class>().insertStatic(custom);
        EXPECT_EQ(MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, custom.mId, true, 1), expected);
        custom.mData.mAttribute[1] = custom.mData.mAttribute[0];
        mStore.getWritable<ESM::Class>().insertStatic(custom);
        auto native = *mStore.get<ESM4::Class>().find(mNpc.mClass);
        native.mData.mFavoredAttributes = {6, 6};
        mStore.getWritable<ESM4::Class>().insertStatic(native);
        const auto duplicate = MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, custom.mId, true, 1);
        EXPECT_EQ(duplicate, MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, mNpc.mClass, true, 1));
        EXPECT_EQ(duplicate.mAttributes[6], 40); // Add primary once;35+5.5 rounds40.
        EXPECT_EQ(duplicate.mAttributes[0], 36);
        custom.mData.mAttribute[0] = ESM::RefId::stringRefId("invalid-attribute");
        mStore.getWritable<ESM::Class>().insertStatic(custom);
        EXPECT_THROW(MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, custom.mId, true, 1), std::invalid_argument);
        custom.mData.mAttribute[0] = ESM::Attribute::Personality;
        custom.mData.mSkills[0][1] = ESM::RefId::stringRefId("invalid-major");
        mStore.getWritable<ESM::Class>().insertStatic(custom);
        EXPECT_THROW(MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, custom.mId, true, 1), std::invalid_argument);
        custom.mData.mSkills[0][1] = ids[0]; custom.mData.mSpecialization = 3;
        mStore.getWritable<ESM::Class>().insertStatic(custom);
        EXPECT_THROW(MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, custom.mId, true, 1), std::invalid_argument);
    }

    TEST_F(OblivionActorStatsTest, playerCharacterBaseRejectsMissingNativeInputsAndMalformedWinningSettings)
    {
        autoNpc();
        EXPECT_THROW(MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, ESM::RefId{}, mNpc.mClass, true, 1), std::invalid_argument);
        EXPECT_THROW(MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, ESM::RefId{}, true, 1), std::invalid_argument);
        EXPECT_THROW(MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, mNpc.mClass, true, 0), std::invalid_argument);
        ESM4::GameSetting wrong{};
        wrong.mId = {0x3333, 6};
        wrong.mEditorId = "iClassCharactergenClass";
        wrong.mData = 143590.f;
        mStore.getWritable<ESM4::GameSetting>().insertStatic(wrong);
        EXPECT_THROW(MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, mNpc.mClass, true, 1), std::invalid_argument);
        wrong.mData = std::int32_t(143590);
        mStore.getWritable<ESM4::GameSetting>().insertStatic(wrong);
        ASSERT_TRUE(mStore.getWritable<ESM4::Skill>().eraseStatic(ESM::FormKey::content("skills.esm", 0x1000)));
        EXPECT_THROW(MWWorld::resolveOblivionPlayerCharacterBaseStats(
            mStore, mNpc.mRace, mNpc.mClass, true, 1), std::invalid_argument);
    }

    TEST_F(OblivionActorStatsTest, playerCharacterBasePublishesAtomicallyPreservesOtherAuthorityAndRestarts)
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
        values.mPlayerFormValues = {{100, 30, 40, 0}};
        for (auto& value : values.mValues) value.mModifiers = {0, 0, 0};
        values.mValues[0] = {50, {7, 13, -3}};
        values.mValues[8].mModifiers = {2, 3, -5};
        MWMechanics::OblivionCombatService service;
        ESM4::ActorCharacterBaseStats calculated{};
        calculated.mAttributes.fill(60);
        calculated.mSkills.fill(20);
        EXPECT_THROW(service.publishPlayerCharacterBase(player, calculated, {}), std::invalid_argument);
        service.publishPlayerValues(player, values, {});
        service.publishPlayerLife(player, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        service.setPlayerBaseValue(player, 55, 1, {});
        service.publishPlayerCharacterBase(player, calculated, {});
        const auto committed = *service.findActorValues(values.mActor);
        EXPECT_EQ(committed.mPlayerFormValues, values.mPlayerFormValues);
        EXPECT_EQ(committed.mValues[0].mBase, 60);
        EXPECT_EQ(committed.mValues[12].mBase, 20);
        EXPECT_EQ(committed.mValues[55].mBase, 1);
        for (std::size_t i=0; i<72; ++i) EXPECT_EQ(committed.mValues[i].mModifiers, values.mValues[i].mModifiers);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getAttribute(ESM::Attribute::Strength).getBase(), 60);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getHealth().getCurrent(), service.getPlayerValue(8));
        EXPECT_EQ(service.getDeadCount(values.mBase), 0);
        EXPECT_FALSE(service.takeNextDeathEvent());
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = values.mActor;
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        service.capture(saved);
        const auto bytes = saved.serializeBinary();
        auto invalid = ESM4::PlayerDynamicBaseSettings{};
        invalid.mHealthMultiplier = std::numeric_limits<float>::quiet_NaN();
        auto different = calculated;
        different.mAttributes.fill(10);
        EXPECT_THROW(service.publishPlayerCharacterBase(player, different, invalid), std::invalid_argument);
        EXPECT_EQ(*service.findActorValues(values.mActor), committed);
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getAttribute(ESM::Attribute::Strength).getBase(), 60);
        service.capture(saved);
        EXPECT_EQ(saved.serializeBinary(), bytes);
        service.publishPlayerCharacterBase(player, calculated, {});
        EXPECT_EQ(*service.findActorValues(values.mActor), committed);
        MWMechanics::OblivionCombatService restarted;
        restarted.restore(ESM4::RuntimeState::deserializeBinary(bytes));
        EXPECT_EQ(*restarted.findActorValues(values.mActor), committed);
        restarted.publishPlayerValues(player, *restarted.findActorValues(values.mActor), {});
        restarted.publishPlayerCharacterBase(player, different, {});
        EXPECT_EQ(restarted.findActorValues(values.mActor)->mValues[0].mBase, 10);
        EXPECT_EQ(restarted.findActorValues(values.mActor)->mValues[55].mBase, 1);
        EXPECT_EQ(restarted.findActorValues(values.mActor)->mValues[8].mModifiers, values.mValues[8].mModifiers);
        EXPECT_EQ(restarted.getDeadCount(values.mBase), 0);
    }
    TEST_F(OblivionActorStatsTest, passiveRemovalOrderReplaysNativeInsertionBeforeAppliedMagnitudeChanges)
    {
        for (auto [code, id, school, name] : std::array{
            std::tuple{ESM::fourCC("FOAT"), 0x800u, 2u, "Fortify Attribute"},
            std::tuple{ESM::fourCC("FOSP"), 0x801u, 5u, "Fortify Magicka"},
            std::tuple{ESM::fourCC("WKMA"), 0x802u, 2u, "Weakness to Magic"}})
        {
            ESM4::EffectSetting definition{}; definition.mId = {id, 3}; definition.mEffectCode = code;
            definition.mFullName = name; definition.mData = ESM4::EffectSettingData{};
            definition.mData->mSchool = school; definition.preparePassiveValueModifierDefinition();
            mStore.getWritable<ESM4::EffectSetting>().insertStatic(definition,
                ESM::FormKey::content("effects.esm", id));
        }
        const auto a = ESM::FormKey::content("abilities.esp", 0x123);
        const auto b = ESM::FormKey::content("abilities.esp", 0x124);
        std::array abilities{
            ESM4::RuntimePassiveAbility{a, {{8, ESM::fourCC("FOAT"), 0, 10, 10},
                {3, ESM::fourCC("FOAT"), 1, 10, 10}, {5, ESM::fourCC("FOAT"), 5, 100, 100},
                {6, ESM::fourCC("FOSP"), 9, 50, 50}}},
            ESM4::RuntimePassiveAbility{b, {{7, ESM::fourCC("FOAT"), 2, 10, 10},
                {2, ESM::fourCC("WKMA"), 64, -25, 25}, {9, ESM::fourCC("WKMA"), 64, -100, 100}}}};
        const auto before = abilities;
        const auto order = MWWorld::resolveOblivionPlayerPassiveRemovalOrder(mStore, abilities);
        const std::array expected{std::pair{b, 7u}, std::pair{a, 3u}, std::pair{a, 8u}, std::pair{a, 5u},
            std::pair{b, 2u}, std::pair{b, 9u}, std::pair{a, 6u}};
        ASSERT_EQ(order.size(), expected.size());
        for (std::size_t i = 0; i < order.size(); ++i)
        {
            EXPECT_EQ(order[i].mSpell, expected[i].first);
            EXPECT_EQ(order[i].mEffectIndex, expected[i].second);
        }
        EXPECT_EQ(abilities, before);
        // Clamped fractional application must retain its original insertion key.
        abilities[0].mEffects = {{8, ESM::fourCC("FOAT"), 0, -.25f, 25},
            {3, ESM::fourCC("FOAT"), 1, 10, 10}};
        const auto fractional = MWWorld::resolveOblivionPlayerPassiveRemovalOrder(mStore, std::span(abilities).first(1));
        ASSERT_EQ(fractional.size(), 2);
        EXPECT_EQ(fractional[0].mEffectIndex, 8);
        EXPECT_EQ(fractional[1].mEffectIndex, 3);
    }

    TEST_F(OblivionActorStatsTest, passiveRemovalOrderRejectsUnknownAmbiguousOrUnadmittedComparisonInputs)
    {
        const auto spell = ESM::FormKey::content("abilities.esp", 0x123);
        std::array abilities{ESM4::RuntimePassiveAbility{spell, {{0, ESM::fourCC("FOAT"), 5, 10, 10}}}};
        EXPECT_TRUE(MWWorld::resolveOblivionPlayerPassiveRemovalOrder(mStore, {}).empty());
        EXPECT_THROW(MWWorld::resolveOblivionPlayerPassiveRemovalOrder(mStore, abilities), std::invalid_argument);
        ESM4::EffectSetting definition{}; definition.mId = {0x800, 3};
        definition.mEffectCode = ESM::fourCC("FOAT"); definition.mFullName = "Fortify Attribute";
        definition.mData = ESM4::EffectSettingData{}; definition.mData->mSchool = 5;
        definition.preparePassiveValueModifierDefinition();
        const auto key = ESM::FormKey::content("effects.esm", 0x800);
        mStore.getWritable<ESM4::EffectSetting>().insertStatic(definition, key);
        abilities[0].mEffects[0].mInitialMagnitude.reset();
        EXPECT_THROW(MWWorld::resolveOblivionPlayerPassiveRemovalOrder(mStore, abilities), std::invalid_argument);
        abilities[0].mEffects[0].mInitialMagnitude = 10;
        auto duplicate = std::array{abilities[0], abilities[0]};
        EXPECT_THROW(MWWorld::resolveOblivionPlayerPassiveRemovalOrder(mStore, duplicate), std::invalid_argument);
        auto ambiguous = definition; ambiguous.mId = {0x801, 3};
        const auto otherKey = ESM::FormKey::content("effects.esm", 0x801);
        mStore.getWritable<ESM4::EffectSetting>().insertStatic(ambiguous, otherKey);
        EXPECT_THROW(MWWorld::resolveOblivionPlayerPassiveRemovalOrder(mStore, abilities), std::invalid_argument);
        ASSERT_TRUE(mStore.getWritable<ESM4::EffectSetting>().eraseStatic(otherKey));
        auto invalid = definition; invalid.mPassiveValueModifierDefinition.reset();
        mStore.getWritable<ESM4::EffectSetting>().insertStatic(invalid, key);
        EXPECT_THROW(MWWorld::resolveOblivionPlayerPassiveRemovalOrder(mStore, abilities), std::invalid_argument);
        invalid = definition; invalid.mData.reset();
        mStore.getWritable<ESM4::EffectSetting>().insertStatic(invalid, key);
        EXPECT_THROW(MWWorld::resolveOblivionPlayerPassiveRemovalOrder(mStore, abilities), std::invalid_argument);
        invalid = definition; invalid.mFullName = "\xc3\xa9";
        mStore.getWritable<ESM4::EffectSetting>().insertStatic(invalid, key);
        EXPECT_THROW(MWWorld::resolveOblivionPlayerPassiveRemovalOrder(mStore, abilities), std::invalid_argument);
        // A masked quantity is genuinely unqueried, including on legacy data.
        definition.mEffectCode = ESM::fourCC("STMA"); definition.mFullName = "Stunted Magicka";
        definition.mPassiveValueModifierDefinition.reset(); definition.preparePassiveValueModifierDefinition();
        mStore.getWritable<ESM4::EffectSetting>().insertStatic(definition, key);
        abilities[0].mEffects = {{0, ESM::fourCC("STMA"), 57, 1}};
        EXPECT_NO_THROW(MWWorld::resolveOblivionPlayerPassiveRemovalOrder(mStore, abilities));
    }



    TEST_F(OblivionActorStatsTest, meleeAuthorityRestartsPendingAndCommittedAnimationWithoutReplayingContact)
    {
        autoNpc(); sharedStats();
        mNpc.mFormKey = mActorKey;
        mNpc.mBaseConfig.tes4.flags &= ~ESM4::Npc::TES4_PCLevelOffset;
        mNpc.mBaseConfig.tes4.levelOrOffset = 2;
        mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        MWBase::Environment environment; environment.setESMStore(mStore);
        ESM::ReadersCache readers; MWWorld::WorldModel model(mStore, readers); environment.setWorldModel(model);
        MWClass::ESM4Npc::registerSelf();
        ESM4::ActorCharacter reference{};
        reference.mFormKey = ESM::FormKey::content("actors.esm", 0x900);
        reference.mId = {0x900, 3}; reference.mBaseKey = mActorKey;
        mStore.getWritable<ESM4::ActorCharacter>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Npc> live(reference, mStore.search<ESM4::Npc>(mActorKey));
        MWWorld::Ptr ptr(&live);
        MWMechanics::OblivionCombatService service;
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey; values.mBase = mActorKey; values.mValues[8].mBase = 100;

        values.mValues[10].mBase = 60;
        const auto foreign = ESM::FormKey::content("different.esm", 0x900);
        const ESM4::RuntimeMeleeInput input{.25f, true, false, ESM4::MeleeQueuedStrike::Power};
        EXPECT_THROW(service.setMeleeInput(values.mActor, input), std::invalid_argument);
        EXPECT_THROW(service.beginMeleeStrike(values.mActor, ESM4::MeleeStrikeKind::Left, "handtohandattackleft"), std::invalid_argument);
        service.publishNonPlayerValues(ptr, values);
        service.publishNonPlayerLife(ptr, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        const auto anonymous = service.allocateAction();
        service.setMeleeInput(values.mActor, input);
        ASSERT_TRUE(service.findMeleeState(values.mActor));
        EXPECT_EQ(service.findMeleeState(values.mActor)->mInput, input);
        EXPECT_FALSE(service.findMeleeState(values.mActor)->mStrike);
        EXPECT_THROW(service.setMeleeInput(foreign, input), std::invalid_argument);
        const auto id = service.beginMeleeStrike(values.mActor, ESM4::MeleeStrikeKind::RightPower, "handtohandattackrightpower", 1.25f);
        EXPECT_TRUE(service.isActionPending(id, values.mActor));
        const auto strike = *service.findMeleeState(values.mActor)->mStrike;
        EXPECT_EQ(strike.mKind, ESM4::MeleeStrikeKind::RightPower);
        EXPECT_EQ(strike.mAnimationGroup, "handtohandattackrightpower");
        EXPECT_EQ(strike.mPlaybackSpeed, 1.25f);
        EXPECT_EQ(strike.mActionId, id); EXPECT_FALSE(strike.mContactCommitted);
        EXPECT_THROW(service.beginMeleeStrike(values.mActor, ESM4::MeleeStrikeKind::Left, "handtohandattackleft"), std::invalid_argument);
        EXPECT_FALSE(service.updateMeleeAnimation(id, foreign, .5f));
        EXPECT_FALSE(service.updateMeleeAnimation(id+1, values.mActor, .5f));
        EXPECT_TRUE(service.updateMeleeAnimation(id, values.mActor, .25f));
        EXPECT_TRUE(service.updateMeleeAnimation(id, values.mActor, .25f));
        EXPECT_THROW(service.updateMeleeAnimation(id, values.mActor, .125f), std::invalid_argument);
        EXPECT_THROW(service.updateMeleeAnimation(id, values.mActor, std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
        auto invalid = input; invalid.mHeldSeconds = -1;
        EXPECT_THROW(service.setMeleeInput(values.mActor, invalid), std::runtime_error);
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        ESM4::RuntimeReferenceState savedActor;
        savedActor.mKey = values.mActor; savedActor.mBase = values.mBase; savedActor.mCell = saved.mPlayer.mCell;
        saved.mReferences.push_back(savedActor);
        service.capture(saved); const auto before = saved.serializeBinary();
        auto older = saved; older.mVersion = 20;
        EXPECT_THROW(service.capture(older), std::invalid_argument);
        EXPECT_EQ(older.mNativeMeleeStates, saved.mNativeMeleeStates);
        MWMechanics::OblivionCombatService restored, candidate;
        candidate.restore(ESM4::RuntimeState::deserializeBinary(before));
        const std::array residents{ptr};
        restored.installRestoredActorState(std::move(candidate), residents, nullptr);
        ASSERT_TRUE(restored.findMeleeState(values.mActor)->mStrike);
        EXPECT_EQ(restored.findMeleeState(values.mActor)->mStrike->mAnimationTime, .25f);
        EXPECT_EQ(restored.findMeleeState(values.mActor)->mStrike->mAnimationGroup, "handtohandattackrightpower");
        EXPECT_EQ(restored.findMeleeState(values.mActor)->mStrike->mPlaybackSpeed, 1.25f);
        EXPECT_TRUE(restored.isActionPending(id, values.mActor));
        EXPECT_THROW(restored.commitPhysicalContact(id, ptr, {}, {-7, -1, 0}, nullptr, false, {4, .5f}, {}), std::invalid_argument);
        restored.capture(saved); EXPECT_EQ(saved.serializeBinary(), before);
        // A valid resolved miss consumes only its strike, retaining follow-through
        // and the queued power input. This does not execute a geometry query.
        ASSERT_TRUE(restored.commitPhysicalContact(id, ptr, {}, {-7, 0, 0}, nullptr, false, {4, .5f}, {}));
        EXPECT_EQ(ptr.getClass().getCreatureStats(ptr).getFatigue().getCurrent(), 53);
        EXPECT_TRUE(restored.isActionConsumed(id));
        EXPECT_TRUE(restored.isActionPending(anonymous));
        ASSERT_TRUE(restored.findMeleeState(values.mActor)->mStrike);
        EXPECT_TRUE(restored.findMeleeState(values.mActor)->mStrike->mContactCommitted);
        EXPECT_EQ(restored.findMeleeState(values.mActor)->mInput, input);
        EXPECT_TRUE(restored.updateMeleeAnimation(id, values.mActor, .5f));
        restored.capture(saved); const auto committed = saved.serializeBinary();
        MWMechanics::OblivionCombatService after, committedCandidate;
        committedCandidate.restore(ESM4::RuntimeState::deserializeBinary(committed));
        after.installRestoredActorState(std::move(committedCandidate), residents, nullptr);
        EXPECT_TRUE(after.findMeleeState(values.mActor)->mStrike->mContactCommitted);
        EXPECT_EQ(after.findMeleeState(values.mActor)->mStrike->mAnimationTime, .5f);
        EXPECT_FALSE(after.commitPhysicalContact(id, ptr, {}, {-7, 0, 0}, nullptr, false, {4, .5f}, {}));
        after.capture(saved); EXPECT_EQ(saved.serializeBinary(), committed);
        EXPECT_FALSE(after.finishMeleeStrike(id, foreign));
        EXPECT_TRUE(after.finishMeleeStrike(id, values.mActor));
        EXPECT_FALSE(after.finishMeleeStrike(id, values.mActor));
        EXPECT_FALSE(after.findMeleeState(values.mActor)->mStrike);
        EXPECT_THROW(after.beginMeleeStrike(values.mActor, ESM4::MeleeStrikeKind::Left, ""), std::runtime_error);
        EXPECT_THROW(after.beginMeleeStrike(values.mActor, ESM4::MeleeStrikeKind::Left, "handtohandattackleft", 0), std::runtime_error);
        after.capture(saved);
        auto exhausted = saved;
        exhausted.mPhysicalActions.mNext = std::numeric_limits<std::uint64_t>::max();
        MWMechanics::OblivionCombatService allocationFailure;
        allocationFailure.restore(exhausted);
        const auto exhaustedBytes = exhausted.serializeBinary();
        EXPECT_THROW(allocationFailure.beginMeleeStrike(values.mActor, ESM4::MeleeStrikeKind::Left, "handtohandattackleft"), std::overflow_error);
        allocationFailure.capture(exhausted);
        EXPECT_EQ(exhausted.serializeBinary(), exhaustedBytes);
        auto malformed = saved;
        malformed.mNativeMeleeStates.at(values.mActor).mInput.mQueued = static_cast<ESM4::MeleeQueuedStrike>(3);
        const auto stable = saved.serializeBinary();
        EXPECT_THROW(after.restore(malformed), std::runtime_error);
        after.capture(saved); EXPECT_EQ(saved.serializeBinary(), stable);
        const auto next = after.beginMeleeStrike(values.mActor, ESM4::MeleeStrikeKind::Left, "handtohandattackleft");
        EXPECT_EQ(next, id+1);
        EXPECT_TRUE(after.finishMeleeStrike(next, values.mActor));
        EXPECT_TRUE(after.isActionConsumed(next));
        EXPECT_EQ(after.findMeleeState(values.mActor)->mInput.mQueued, ESM4::MeleeQueuedStrike::Power);
        const auto cancelled = after.beginMeleeStrike(values.mActor, ESM4::MeleeStrikeKind::StandingPower, "handtohandattackpower");
        EXPECT_TRUE(after.consumeAction(cancelled, values.mActor));
        EXPECT_FALSE(after.findMeleeState(values.mActor)->mStrike);
        const auto incapacitated = after.beginMeleeStrike(values.mActor, ESM4::MeleeStrikeKind::ForwardPower, "handtohandattackforwardpower");
        after.publishNonPlayerLife(ptr, {values.mActor, values.mBase, ESM4::ActorLifePhase::EssentialUnconscious, 4, {}});
        EXPECT_FALSE(after.findMeleeState(values.mActor));
        EXPECT_TRUE(after.isActionConsumed(incapacitated));
        EXPECT_TRUE(after.isActionPending(anonymous));
        EXPECT_THROW(after.setMeleeInput(values.mActor, input), std::invalid_argument);
        EXPECT_THROW(after.beginMeleeStrike(values.mActor, ESM4::MeleeStrikeKind::Left, "handtohandattackleft"), std::invalid_argument);
        after.clear(); EXPECT_FALSE(after.findMeleeState(values.mActor));
        EXPECT_EQ(after.allocateAction(), 1);
    }

    TEST_F(OblivionActorStatsTest, ownedPhysicalIntentsValidateCompleteOwnerConsumeOnceAndCancelWithLife)
    {
        autoNpc(); sharedStats();
        mNpc.mFormKey = mActorKey;
        mNpc.mBaseConfig.tes4.flags &= ~ESM4::Npc::TES4_PCLevelOffset;
        mNpc.mBaseConfig.tes4.levelOrOffset = 2;
        mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        MWBase::Environment environment; environment.setESMStore(mStore);
        ESM::ReadersCache readers; MWWorld::WorldModel model(mStore, readers); environment.setWorldModel(model);
        MWClass::ESM4Npc::registerSelf();
        ESM4::ActorCharacter reference{};
        reference.mFormKey = ESM::FormKey::content("actors.esm", 0x900);
        reference.mId = {0x900, 3}; reference.mBaseKey = mActorKey;
        mStore.getWritable<ESM4::ActorCharacter>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Npc> live(reference, mStore.search<ESM4::Npc>(mActorKey));
        MWWorld::Ptr ptr(&live);
        MWMechanics::OblivionCombatService service;
        ESM4::RuntimeActorValues values;
        values.mActor = reference.mFormKey; values.mBase = mActorKey; values.mValues[8].mBase = 100;
        const auto foreign = ESM::FormKey::content("different.esm", 0x900);
        EXPECT_THROW(service.allocateAction(values.mActor), std::invalid_argument);
        service.publishNonPlayerValues(ptr, values);
        EXPECT_THROW(service.allocateAction(values.mActor), std::invalid_argument);
        service.publishNonPlayerLife(ptr, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        const auto anonymous = service.allocateAction();
        const auto first = service.allocateAction(values.mActor);
        const auto second = service.allocateAction(values.mActor);
        EXPECT_EQ(first, anonymous + 1); EXPECT_EQ(second, first + 1);
        EXPECT_THROW(service.allocateAction(foreign), std::invalid_argument);
        EXPECT_FALSE(service.isActionPending(first, foreign));
        EXPECT_FALSE(service.consumeAction(first, foreign));
        EXPECT_THROW(service.consumeAction(first), std::invalid_argument);
        EXPECT_FALSE(service.consumeAction(anonymous, values.mActor));
        EXPECT_TRUE(service.isActionPending(first, values.mActor));
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        ESM4::RuntimeReferenceState savedActor;
        savedActor.mKey = values.mActor; savedActor.mBase = values.mBase; savedActor.mCell = saved.mPlayer.mCell;
        saved.mReferences.push_back(savedActor);
        service.capture(saved);
        const auto bytes = saved.serializeBinary();
        MWMechanics::OblivionCombatService restored;
        restored.restore(ESM4::RuntimeState::deserializeBinary(bytes));
        EXPECT_TRUE(restored.isActionPending(first, values.mActor));
        EXPECT_TRUE(restored.consumeAction(second, values.mActor));
        EXPECT_FALSE(restored.consumeAction(second, values.mActor));
        EXPECT_TRUE(restored.isActionConsumed(second));
        auto broken = saved; broken.mPhysicalActionOwners[first] = foreign;
        EXPECT_THROW(restored.restore(broken), std::runtime_error);
        EXPECT_TRUE(restored.isActionPending(first, values.mActor));
        EXPECT_TRUE(restored.isActionConsumed(second));
        auto legacy = saved; legacy.mVersion = 19;
        EXPECT_THROW(service.capture(legacy), std::invalid_argument);
        EXPECT_EQ(legacy.mPhysicalActions, saved.mPhysicalActions);
        EXPECT_EQ(legacy.mPhysicalActionOwners, saved.mPhysicalActionOwners);
        EXPECT_EQ(restored.cancelActorActions(foreign), 0);
        EXPECT_EQ(restored.cancelActorActions(values.mActor), 1);
        EXPECT_EQ(restored.cancelActorActions(values.mActor), 0);
        EXPECT_TRUE(restored.isActionPending(anonymous));
        EXPECT_TRUE(service.changeNonPlayerHealth(ptr, -100, {}, true, {4, .5f}));
        EXPECT_TRUE(service.isActionConsumed(first)); EXPECT_TRUE(service.isActionConsumed(second));
        EXPECT_TRUE(service.isActionPending(anonymous));
        EXPECT_EQ(service.findActorLife(values.mActor)->mPhase, ESM4::ActorLifePhase::EssentialUnconscious);
        EXPECT_THROW(service.allocateAction(values.mActor), std::invalid_argument);
        service.capture(saved); saved.validate();
        service.publishNonPlayerLife(ptr, {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
        const auto deathAction = service.allocateAction(values.mActor);
        service.publishNonPlayerLife(ptr, {values.mActor, values.mBase, ESM4::ActorLifePhase::Dead, 0, {}});
        EXPECT_TRUE(service.isActionConsumed(deathAction));
        EXPECT_THROW(service.allocateAction(values.mActor), std::invalid_argument);
        service.clear();
        EXPECT_FALSE(service.isActionPending(anonymous));
        EXPECT_EQ(service.allocateAction(), 1);
    }

    TEST_F(OblivionActorStatsTest, physicalContactPublishesBothActorsAndConsumptionAtomicallyAcrossRestart)
    {
        autoNpc(); sharedStats(); mNpc.mFormKey = mActorKey;
        mNpc.mBaseConfig.tes4.flags &= ~ESM4::Npc::TES4_PCLevelOffset;
        mNpc.mBaseConfig.tes4.levelOrOffset = 2;
        mStore.getWritable<ESM4::Npc>().insertStatic(mNpc, mActorKey);
        MWBase::Environment environment; environment.setESMStore(mStore);
        ESM::ReadersCache readers; MWWorld::WorldModel model(mStore, readers); environment.setWorldModel(model);
        MWClass::ESM4Npc::registerSelf();
        std::array<ESM4::ActorCharacter, 2> refs{};
        std::array<std::unique_ptr<MWWorld::LiveCellRef<ESM4::Npc>>, 2> lives;
        std::array<MWWorld::Ptr, 2> ptrs;
        MWMechanics::OblivionCombatService service;
        ESM4::RuntimeState saved;
        saved.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        for (std::size_t i = 0; i < refs.size(); ++i)
        {
            refs[i].mId = {static_cast<std::uint32_t>(0x900 + i), 3};
            refs[i].mFormKey = ESM::FormKey::content("actors.esm", 0x900 + i);
            refs[i].mBaseKey = mActorKey;
            mStore.getWritable<ESM4::ActorCharacter>().insertStatic(refs[i], refs[i].mFormKey);
            lives[i] = std::make_unique<MWWorld::LiveCellRef<ESM4::Npc>>(refs[i], mStore.search<ESM4::Npc>(mActorKey));
            ptrs[i] = MWWorld::Ptr(lives[i].get());
            ESM4::RuntimeActorValues values;
            values.mActor = refs[i].mFormKey; values.mBase = mActorKey;
            values.mValues[8] = {100, {20, 5, -5}};
            values.mValues[10] = {80, {7, 3, -4}};
            service.publishNonPlayerValues(ptrs[i], values);
            service.publishNonPlayerLife(ptrs[i], {values.mActor, values.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
            ESM4::RuntimeReferenceState reference;
            reference.mKey = values.mActor; reference.mBase = values.mBase; reference.mCell = saved.mPlayer.mCell;
            saved.mReferences.push_back(reference);
        }
        const ESM4::PlayerDynamicBaseSettings playerBase{2, 1.5f, 5};
        const auto snapshot = [&] { service.capture(saved); return saved.serializeBinary(); };
        const auto id = service.allocateAction(refs[0].mFormKey);
        const auto before = snapshot();
        for (const auto bad : {MWMechanics::OblivionPhysicalContactDeltas{std::numeric_limits<float>::quiet_NaN(), -12, -15},
                 MWMechanics::OblivionPhysicalContactDeltas{-10, std::numeric_limits<float>::infinity(), -15},
                 MWMechanics::OblivionPhysicalContactDeltas{-10, -12, -std::numeric_limits<float>::infinity()}})
        {
            EXPECT_THROW(service.commitPhysicalContact(id, ptrs[0], ptrs[1], bad, nullptr, false, {4, .5f}, playerBase), std::invalid_argument);
            EXPECT_EQ(snapshot(), before);
            EXPECT_EQ(ptrs[0].getClass().getCreatureStats(ptrs[0]).getFatigue().getCurrent(), 86);
            EXPECT_EQ(ptrs[1].getClass().getCreatureStats(ptrs[1]).getHealth().getCurrent(), 120);
        }
        EXPECT_THROW(service.commitPhysicalContact(id, ptrs[0], {}, {-10, -1, 0}, nullptr, false, {4, .5f}, playerBase), std::invalid_argument);
        EXPECT_THROW(service.commitPhysicalContact(id, ptrs[0], ptrs[0], {-10, -1, 0}, nullptr, false, {4, .5f}, playerBase), std::invalid_argument);
        EXPECT_THROW(service.commitPhysicalContact(id, ptrs[0], ptrs[1], {-10, -200, -15}, nullptr, true,
            {4, std::numeric_limits<float>::max()}, playerBase), std::invalid_argument);
        EXPECT_EQ(snapshot(), before);
        EXPECT_FALSE(service.commitPhysicalContact(id, ptrs[1], ptrs[0], {-10, -12, -15}, nullptr, false, {4, .5f}, playerBase));
        EXPECT_EQ(snapshot(), before);
        EXPECT_TRUE(service.commitPhysicalContact(id, ptrs[0], ptrs[1], {-10, -12, -15}, nullptr, false, {4, .5f}, playerBase));
        EXPECT_EQ(service.getNonPlayerValue(ptrs[0], 10), 76);
        EXPECT_EQ(service.getNonPlayerValue(ptrs[0], 8), 120);
        EXPECT_EQ(service.getNonPlayerValue(ptrs[1], 8), 108);
        EXPECT_EQ(service.getNonPlayerValue(ptrs[1], 10), 71);
        EXPECT_EQ(ptrs[0].getClass().getCreatureStats(ptrs[0]).getFatigue().getCurrent(), 76);
        EXPECT_EQ(ptrs[1].getClass().getCreatureStats(ptrs[1]).getHealth().getCurrent(), 108);
        EXPECT_EQ(service.findActorValues(refs[1].mFormKey)->mValues[8].mModifiers[0], 20);
        EXPECT_EQ(service.findActorValues(refs[1].mFormKey)->mValues[8].mModifiers[1], 5);
        EXPECT_TRUE(service.isActionConsumed(id));
        const auto after = snapshot();
        EXPECT_FALSE(service.commitPhysicalContact(id, ptrs[0], ptrs[1], {-10, -12, -15}, nullptr, false, {4, .5f}, playerBase));
        EXPECT_EQ(snapshot(), after);
        MWMechanics::OblivionCombatService candidate, resumed;
        candidate.restore(ESM4::RuntimeState::deserializeBinary(before));
        resumed.installRestoredActorState(std::move(candidate), ptrs, nullptr);
        EXPECT_TRUE(resumed.commitPhysicalContact(id, ptrs[0], ptrs[1], {-10, -12, -15}, nullptr, false, {4, .5f}, playerBase));
        auto continued = saved; resumed.capture(continued);
        EXPECT_EQ(continued.serializeBinary(), after);
        const auto miss = resumed.allocateAction(refs[0].mFormKey);
        EXPECT_TRUE(resumed.commitPhysicalContact(miss, ptrs[0], {}, {-2, 0, 0}, nullptr, false, {4, .5f}, playerBase));
        EXPECT_EQ(resumed.getNonPlayerValue(ptrs[0], 10), 74);
        EXPECT_EQ(resumed.getNonPlayerValue(ptrs[1], 8), 108);
        EXPECT_FALSE(resumed.takeNextDeathEvent());
        const auto cancelled = resumed.allocateAction(refs[0].mFormKey);
        resumed.cancelActorActions(refs[0].mFormKey);
        EXPECT_FALSE(resumed.commitPhysicalContact(cancelled, ptrs[0], ptrs[1], {-10, -12, -15}, nullptr, false, {4, .5f}, playerBase));
    }

    TEST_F(OblivionActorStatsTest, meleePlayerAndZeroFatigueCreatureKeepSeparateContinuationAndCancellation)
    {
        sharedStats();
        ESM::NPC facade{}; facade.blank(); facade.mId = ESM::RefId::stringRefId("Player");
        const auto* playerRecord = mStore.insertStatic(facade);
        ESM4::Creature creature{}; creature.mId = {0x800, 3}; creature.mFormKey = mActorKey;
        creature.mAttackReach = 64; creature.mBaseConfig.tes4.levelOrOffset = 2;
        mStore.getWritable<ESM4::Creature>().insertStatic(creature, mActorKey);
        MWBase::Environment environment; environment.setESMStore(mStore);
        ESM::ReadersCache readers; MWWorld::WorldModel model(mStore, readers); environment.setWorldModel(model);
        MWClass::Npc::registerSelf(); MWClass::ESM4Creature::registerSelf();
        MWWorld::Player player(playerRecord); const auto playerPtr = player.getPlayer();
        ESM::NpcState initial{}; initial.blank(); playerPtr.getClass().readAdditionalState(playerPtr, initial);
        ESM4::ActorCreature reference{}; reference.mId = {0x900, 3};
        reference.mFormKey = ESM::FormKey::content("actors.esm", 0x900); reference.mBaseKey = mActorKey;
        mStore.getWritable<ESM4::ActorCreature>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Creature> live(reference, mStore.search<ESM4::Creature>(mActorKey));
        MWWorld::Ptr creaturePtr(&live);
        const ESM4::PlayerDynamicBaseSettings playerBase{2, 1.5f, 5};
        for (const bool playerAttacks : {false, true})
        for (const bool godMode : {false, true})
        {
            MWMechanics::OblivionCombatService service;
            ESM4::RuntimeActorValues pv; pv.mActor = ESM::FormKey::dynamic("player", 1);
            pv.mBase = ESM::FormKey::dynamic("player-base", 1); pv.mOwner = ESM4::ActorValueOwner::Player;
            pv.mPlayerFormValues = {{0, 0, 0, 0}};
            for (std::size_t i = 0; i < 8; ++i) pv.mValues[i].mBase = 40;
            service.publishPlayerValues(player, pv, playerBase);
            service.publishPlayerLife(player, {pv.mActor, pv.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
            ESM4::RuntimeActorValues cv; cv.mActor = reference.mFormKey; cv.mBase = mActorKey;
            cv.mValues[8].mBase = 100; cv.mValues[10].mBase = 0;
            service.publishNonPlayerValues(creaturePtr, cv);
            service.publishNonPlayerLife(creaturePtr, {cv.mActor, cv.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
            for (const auto& actor : {pv.mActor, cv.mActor})
                service.setMeleeInput(actor, {.125f, false, true, ESM4::MeleeQueuedStrike::Ordinary});
            const auto playerId = service.beginMeleeStrike(pv.mActor, ESM4::MeleeStrikeKind::Left, "handtohandattackleft");
            const auto creatureId = service.beginMeleeStrike(cv.mActor, ESM4::MeleeStrikeKind::Right, "attackright");
            const auto attacker = playerAttacks ? playerPtr : creaturePtr;
            const auto key = playerAttacks ? pv.mActor : cv.mActor;
            const auto other = playerAttacks ? cv.mActor : pv.mActor;
            const auto id = playerAttacks ? playerId : creatureId;
            // The caller supplies zero creature expenditure. This checks saved
            // authority/ownership, not the unresolved controller cost policy.
            ASSERT_TRUE(service.commitPhysicalContact(id, attacker, {}, {playerAttacks ? -7.f : 0.f, 0, 0},
                &player, false, {4, .5f}, playerBase, godMode));
            EXPECT_EQ(playerPtr.getClass().getCreatureStats(playerPtr).getFatigue().getCurrent(),
                playerAttacks && !godMode ? 153 : 160);
            EXPECT_EQ(creaturePtr.getClass().getCreatureStats(creaturePtr).getFatigue().getCurrent(), 0);
            EXPECT_FALSE(creaturePtr.getClass().getCreatureStats(creaturePtr).isFatigueKnockedOut());
            EXPECT_TRUE(service.findMeleeState(key)->mStrike->mContactCommitted);
            EXPECT_FALSE(service.findMeleeState(other)->mStrike->mContactCommitted);
            ESM4::RuntimeState saved;
            saved.mPlayer.mReference = pv.mActor; saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
            saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2); saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
            ESM4::RuntimeReferenceState savedActor; savedActor.mKey = cv.mActor; savedActor.mBase = cv.mBase;
            savedActor.mCell = saved.mPlayer.mCell; saved.mReferences.push_back(savedActor);
            service.capture(saved); const auto bytes = saved.serializeBinary();
            MWMechanics::OblivionCombatService restored, candidate;
            candidate.restore(ESM4::RuntimeState::deserializeBinary(bytes));
            const std::array residents{creaturePtr};
            restored.installRestoredActorState(std::move(candidate), residents, &player);
            EXPECT_FALSE(restored.commitPhysicalContact(id, attacker, {}, {-7, 0, 0}, &player,
                false, {4, .5f}, playerBase, godMode));
            restored.capture(saved); EXPECT_EQ(saved.serializeBinary(), bytes);
            EXPECT_EQ(restored.cancelActorActions(key), 0);
            EXPECT_FALSE(restored.findMeleeState(key));
            ASSERT_TRUE(restored.findMeleeState(other)->mStrike);
            EXPECT_TRUE(restored.isActionPending(restored.findMeleeState(other)->mStrike->mActionId, other));
            EXPECT_EQ(restored.cancelActorActions(other), 1);
            EXPECT_FALSE(restored.findMeleeState(other));
            restored.capture(saved); saved.validate(); EXPECT_TRUE(saved.mNativeMeleeStates.empty());
        }
    }

    TEST_F(OblivionActorStatsTest, physicalPlayerCreatureContactOwnsDeathEssentialGodModeAndEventFailure)
    {
        sharedStats();
        ESM::NPC facade{}; facade.blank(); facade.mId = ESM::RefId::stringRefId("Player");
        const auto* playerRecord = mStore.insertStatic(facade);
        ESM4::Creature creature{}; creature.mId = {0x800, 3}; creature.mFormKey = mActorKey;
        creature.mAttackReach = 64; creature.mBaseConfig.tes4.levelOrOffset = 2;
        mStore.getWritable<ESM4::Creature>().insertStatic(creature, mActorKey);
        MWBase::Environment environment; environment.setESMStore(mStore);
        ESM::ReadersCache readers; MWWorld::WorldModel model(mStore, readers); environment.setWorldModel(model);
        MWClass::Npc::registerSelf(); MWClass::ESM4Creature::registerSelf();
        MWWorld::Player player(playerRecord); const auto playerPtr = player.getPlayer();
        ESM::NpcState initial{}; initial.blank(); playerPtr.getClass().readAdditionalState(playerPtr, initial);
        ESM4::ActorCreature reference{}; reference.mId = {0x900, 3};
        reference.mFormKey = ESM::FormKey::content("actors.esm", 0x900); reference.mBaseKey = mActorKey;
        mStore.getWritable<ESM4::ActorCreature>().insertStatic(reference, reference.mFormKey);
        MWWorld::LiveCellRef<ESM4::Creature> live(reference, mStore.search<ESM4::Creature>(mActorKey));
        MWWorld::Ptr creaturePtr(&live);
        const ESM4::PlayerDynamicBaseSettings playerBase{2, 1.5f, 5};
        for (const bool playerAttacks : {false, true})
        for (const bool essential : {false, true})
        for (const bool godMode : {false, true})
        {
            SCOPED_TRACE(playerAttacks);
            SCOPED_TRACE(essential);
            SCOPED_TRACE(godMode);
            MWMechanics::OblivionCombatService service;
            ESM4::RuntimeActorValues pv; pv.mActor = ESM::FormKey::dynamic("player", 1);
            pv.mBase = ESM::FormKey::dynamic("player-base", 1); pv.mOwner = ESM4::ActorValueOwner::Player;
            pv.mPlayerFormValues = {{0, 0, 0, 0}};
            for (std::size_t i = 0; i < 8; ++i) pv.mValues[i].mBase = 40;
            service.publishPlayerValues(player, pv, playerBase);
            service.publishPlayerLife(player, {pv.mActor, pv.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
            ESM4::RuntimeActorValues cv; cv.mActor = reference.mFormKey; cv.mBase = mActorKey;
            cv.mValues[8].mBase = 100; cv.mValues[10].mBase = 60;
            service.publishNonPlayerValues(creaturePtr, cv);
            service.publishNonPlayerLife(creaturePtr, {cv.mActor, cv.mBase, ESM4::ActorLifePhase::Alive, 0, {}});
            const auto attacker = playerAttacks ? playerPtr : creaturePtr;
            const auto victim = playerAttacks ? creaturePtr : playerPtr;
            const auto attackerKey = playerAttacks ? pv.mActor : cv.mActor;
            const auto victimKey = playerAttacks ? cv.mActor : pv.mActor;
            const auto id = service.allocateAction(attackerKey);
            const auto victimAction = service.allocateAction(victimKey);
            ESM4::RuntimeState saved;
            saved.mPlayer.mReference = pv.mActor; saved.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
            saved.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2); saved.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
            ESM4::RuntimeReferenceState savedActor; savedActor.mKey = cv.mActor; savedActor.mBase = cv.mBase;
            savedActor.mCell = saved.mPlayer.mCell; saved.mReferences.push_back(savedActor);
            service.capture(saved); const auto before = saved.serializeBinary();
            if (playerAttacks)
                EXPECT_FALSE(service.commitPhysicalContact(id, attacker, victim, {-10, -1000, -5}, nullptr,
                    essential, {4, .5f}, playerBase, godMode));
            else
                EXPECT_THROW(service.commitPhysicalContact(id, attacker, victim, {-10, -1000, -5}, nullptr,
                    essential, {4, .5f}, playerBase, godMode), std::invalid_argument);
            service.capture(saved); EXPECT_EQ(saved.serializeBinary(), before);
            ASSERT_TRUE(service.commitPhysicalContact(id, attacker, victim, {-10, -1000, -5}, &player,
                essential, {4, .5f}, playerBase, godMode));
            const bool suppressed = !playerAttacks && godMode;
            const auto* life = service.findActorLife(victimKey);
            EXPECT_EQ(life->mPhase, suppressed ? ESM4::ActorLifePhase::Alive
                : essential ? ESM4::ActorLifePhase::EssentialUnconscious : ESM4::ActorLifePhase::Dead);
            EXPECT_EQ(life->mKiller, suppressed ? ESM::FormKey{} : attackerKey);
            EXPECT_TRUE(service.isActionConsumed(id));
            EXPECT_EQ(service.isActionPending(victimAction, victimKey), suppressed);
            EXPECT_EQ(playerPtr.getClass().getCreatureStats(playerPtr).getFatigue().getCurrent(),
                playerAttacks ? godMode ? 160 : 150 : godMode ? 160 : 155);
            EXPECT_EQ(creaturePtr.getClass().getCreatureStats(creaturePtr).getFatigue().getCurrent(), playerAttacks ? 55 : 50);
            EXPECT_EQ(service.getPlayerValue(8), playerAttacks || godMode ? 80 : essential ? 40 : -920);
            EXPECT_EQ(service.getNonPlayerValue(creaturePtr, 8), !playerAttacks ? 100 : essential ? 50 : -900);
            const auto event = service.takeNextDeathEvent();
            EXPECT_EQ(bool(event), !suppressed && !essential);
            if (event) { EXPECT_EQ(event->mActor, victimKey); EXPECT_EQ(event->mKiller, attackerKey); }
            EXPECT_FALSE(service.takeNextDeathEvent());
            service.capture(saved); saved.validate();
            EXPECT_EQ(saved.mPhysicalActions.mPending.size(), suppressed ? 1 : 0);
            // Event-identity exhaustion rejects a terminal hit before either
            // resource facade or pending action changes; the same action retries.
            MWMechanics::OblivionCombatService failure;
            auto exhausted = ESM4::RuntimeState::deserializeBinary(before);
            exhausted.mNextDeathEvent = std::numeric_limits<std::uint64_t>::max();
            failure.restore(exhausted);
            MWMechanics::OblivionCombatService candidate;
            candidate.restore(exhausted);
            const std::array residents{creaturePtr};
            failure.installRestoredActorState(std::move(candidate), residents, &player);
            const auto beforeFailure = exhausted.serializeBinary();
            EXPECT_THROW(failure.commitPhysicalContact(id, attacker, victim, {-10, -1000, -5}, &player,
                false, {4, .5f}, playerBase), std::overflow_error);
            failure.capture(exhausted);
            EXPECT_EQ(exhausted.serializeBinary(), beforeFailure);
            EXPECT_TRUE(failure.isActionPending(id, attackerKey));
            EXPECT_EQ(playerPtr.getClass().getCreatureStats(playerPtr).getFatigue().getCurrent(), 160);
            EXPECT_EQ(creaturePtr.getClass().getCreatureStats(creaturePtr).getHealth().getCurrent(), 100);
            EXPECT_EQ(playerPtr.getClass().getCreatureStats(playerPtr).getHealth().getCurrent(), 80);
        }
    }

}
