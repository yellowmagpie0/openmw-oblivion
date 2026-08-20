#include <gtest/gtest.h>

#include <components/esm/records.hpp>

#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/oblivionprofileservices.hpp"

namespace
{
    TEST(OblivionProfileServicesTest, adaptsNativeBootRecordsWithoutACatchAll)
    {
        MWWorld::ESMStore store;

        ESM4::GameSetting gameSetting{};
        gameSetting.mId = ESM::FormId{ 0x10, 0 };
        gameSetting.mEditorId = "fFatigueBase";
        gameSetting.mData = 42.f;
        store.getWritable<ESM4::GameSetting>().insertStatic(gameSetting);

        ESM4::GlobalVariable year{};
        year.mId = ESM::FormId{ 0x20, 0 };
        year.mEditorId = "GameYear";
        year.mType = 'l';
        year.mValue = 431.f;
        store.getWritable<ESM4::GlobalVariable>().insertStatic(year);

        ESM4::GlobalVariable daysPassed{};
        daysPassed.mId = ESM::FormId{ 0x21, 0 };
        daysPassed.mEditorId = "GameDaysPassed";
        daysPassed.mType = 'f';
        daysPassed.mValue = 12.5f;
        store.getWritable<ESM4::GlobalVariable>().insertStatic(daysPassed);

        ESM4::Race race{};
        race.mId = ESM::FormId{ 0x907, 0 };
        race.mEditorId = "Imperial";
        race.mFullName = "Native Imperial";
        race.mAttribMale = { 45, 40, 35, 30, 25, 20, 15, 10 };
        race.mAttribFemale = { 40, 45, 35, 30, 25, 20, 15, 10 };
        store.getWritable<ESM4::Race>().insertStatic(race);

        ESM4::Class characterClass{};
        characterClass.mId = ESM::FormId{ 0x30e6, 0 };
        characterClass.mEditorId = "CharactergenClass";
        characterClass.mFullName = "Native Adventurer";
        store.getWritable<ESM4::Class>().insertStatic(characterClass);

        ESM4::Npc player{};
        player.mId = ESM::FormId{ 7, 0 };
        player.mEditorId = "Player";
        player.mFullName = "Native Prisoner";
        player.mModel = "characters/_male/skeleton.nif";
        player.mRace = race.mId;
        player.mClass = characterClass.mId;
        player.mIsTES4 = true;
        player.mBaseConfig.tes4.levelOrOffset = 3;
        player.mBaseConfig.tes4.fatigue = 77;
        player.mData.health = 88;
        player.mData.attribs = { 41, 42, 43, 44, 45, 46, 47, 48 };
        player.mData.skills.armorer = 31;
        player.mData.skills.speechcraft = 52;
        store.getWritable<ESM4::Npc>().insertStatic(player);

        const MWWorld::OblivionProfileInstallReport report = MWWorld::OblivionProfileServices::install(store);

        EXPECT_EQ(report.mNativeGameSettings, 1);
        EXPECT_EQ(report.mNativeGlobals, 2);
        EXPECT_EQ(report.mPlayerSource, "Player@0x7");
        EXPECT_EQ(report.mRaceSource, "Imperial@0x907");
        EXPECT_EQ(report.mClassSource, "CharactergenClass@0x30e6");

        const auto& settings = store.get<ESM::GameSetting>();
        ASSERT_NE(settings.search("fFatigueBase"), nullptr);
        EXPECT_FLOAT_EQ(settings.find("fFatigueBase")->mValue.getFloat(), 42.f);
        EXPECT_FLOAT_EQ(settings.find("fSwimHeightScale")->mValue.getFloat(), 0.75f);
        EXPECT_FLOAT_EQ(settings.find("fFatigueSneakBase")->mValue.getFloat(), 1.5f);
        EXPECT_FLOAT_EQ(settings.find("fFatigueSneakMult")->mValue.getFloat(), 1.5f);
        EXPECT_FLOAT_EQ(settings.find("fFatigueSwimRunBase")->mValue.getFloat(), 7.f);
        EXPECT_FLOAT_EQ(settings.find("fFatigueSwimRunMult")->mValue.getFloat(), 0.f);
        EXPECT_FLOAT_EQ(settings.find("fFatigueSwimWalkBase")->mValue.getFloat(), 2.5f);
        EXPECT_FLOAT_EQ(settings.find("fFatigueSwimWalkMult")->mValue.getFloat(), 0.f);
        EXPECT_FLOAT_EQ(settings.find("fAudioDefaultMinDistance")->mValue.getFloat(), 5.f);
        EXPECT_FLOAT_EQ(settings.find("fAudioDefaultMaxDistance")->mValue.getFloat(), 40.f);
        EXPECT_FLOAT_EQ(settings.find("fAudioVoiceDefaultMinDistance")->mValue.getFloat(), 10.f);
        EXPECT_FLOAT_EQ(settings.find("fAudioVoiceDefaultMaxDistance")->mValue.getFloat(), 60.f);
        EXPECT_FLOAT_EQ(settings.find("fAudioMinDistanceMult")->mValue.getFloat(), 20.f);
        EXPECT_FLOAT_EQ(settings.find("fAudioMaxDistanceMult")->mValue.getFloat(), 50.f);
        EXPECT_GT(settings.find("fMajorSkillBonus")->mValue.getFloat(), 0.f);
        EXPECT_GT(settings.find("fMinorSkillBonus")->mValue.getFloat(), 0.f);
        EXPECT_GT(settings.find("fMiscSkillBonus")->mValue.getFloat(), 0.f);
        EXPECT_GT(settings.find("fSpecialSkillBonus")->mValue.getFloat(), 0.f);
        EXPECT_EQ(settings.search("fUnreviewedOblivionFallback"), nullptr);

        const auto& globals = store.get<ESM::Global>();
        EXPECT_EQ(globals.find(ESM::RefId::stringRefId("GameYear"))->mValue.getInteger(), 431);
        EXPECT_EQ(globals.find(ESM::RefId::stringRefId("year"))->mValue.getInteger(), 431);
        EXPECT_FLOAT_EQ(globals.find(ESM::RefId::stringRefId("dayspassed"))->mValue.getFloat(), 12.5f);

        const ESM::NPC* adaptedPlayer = store.get<ESM::NPC>().search(ESM::RefId::stringRefId("Player"));
        ASSERT_NE(adaptedPlayer, nullptr);
        EXPECT_EQ(adaptedPlayer->mName, "Native Prisoner");
        EXPECT_EQ(adaptedPlayer->mModel.getNormalized().value(), "meshes/characters/_male/skeleton.nif");
        EXPECT_EQ(adaptedPlayer->mNpdt.mLevel, 3);
        EXPECT_EQ(adaptedPlayer->mNpdt.mHealth, 88);
        EXPECT_EQ(adaptedPlayer->mNpdt.mFatigue, 77);
        EXPECT_EQ(adaptedPlayer->mNpdt.getAttribute(ESM::Attribute::Strength), 41);
        EXPECT_EQ(adaptedPlayer->mNpdt.getSkill(ESM::Skill::Armorer), 31);
        EXPECT_EQ(adaptedPlayer->mNpdt.getSkill(ESM::Skill::Speechcraft), 52);

        EXPECT_EQ(store.get<ESM::Race>().find(adaptedPlayer->mRace)->mName, "Native Imperial");
        EXPECT_EQ(store.get<ESM::Class>().find(adaptedPlayer->mClass)->mName, "Native Adventurer");
    }
}
