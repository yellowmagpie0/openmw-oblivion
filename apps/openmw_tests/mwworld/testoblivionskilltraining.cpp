#include "oblivionworldfixture.hpp"

#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwmechanics/npcstats.hpp"
#include "apps/openmw/mwmechanics/oblivioncombat.hpp"
#include "apps/openmw/mwworld/class.hpp"
#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/oblivionactorstats.hpp"
#include "apps/openmw/mwworld/oblivionscriptmanager.hpp"
#include "apps/openmw/mwworld/player.hpp"
#include "apps/openmw/mwworld/worldmodel.hpp"

#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/esm4/loadclas.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadskil.hpp>
#include <components/esm4/runtimestate.hpp>
#include <components/obscript/vm.hpp>

#include <gtest/gtest.h>
#include <limits>
#include <sstream>

namespace
{
    struct TrainingFixture : TestingOpenMW::NativeWorldFixture
    {
        explicit TrainingFixture(bool major)
        {
            MWClass::Npc::registerSelf();
            mWorld.setupPlayer();
            auto& store = mWorld.getStore();
            ESM::Race race{}; race.blank(); race.mId = ESM::RefId(ESM::FormId{0x810, 0});
            store.getWritable<ESM::Race>().insertStatic(race);
            ESM4::Class characterClass{};
            characterClass.mId = {0x820, 0}; characterClass.mData.mSpecialization = 0;
            characterClass.mData.mMajorSkills.fill(12);
            if (major) characterClass.mData.mMajorSkills[0] = 29;
            store.getWritable<ESM4::Class>().insertStatic(characterClass, ESM::FormKey::content("headless.esm", 0x820));
            auto player = *mWorld.getPlayerPtr().get<ESM::NPC>()->mBase;
            player.mRace = race.mId; player.mClass = ESM::RefId(characterClass.mId);
            auto metadata = store.preparePlayerRecord(player); mWorld.getPlayer().set(metadata.commit());
            ESM4::Cell cell{}; cell.mId = ESM::RefId(ESM::FormId{1, 0});
            cell.mFormKey = ESM::FormKey::content("headless.esm", 1);
            cell.mCellFlags = ESM4::CELL_Interior;
            store.getWritable<ESM4::Cell>().insertStatic(cell, cell.mFormKey);
            mWorld.getPlayer().setCell(&mWorld.getWorldModel().getCell(cell.mId));
            ESM4::Npc native{}; native.mId = {7, 1}; native.mIsTES4 = true;
            native.mFormKey = ESM::FormKey::content("Oblivion.esm", 7);
            native.mData.attribs = {50, 50, 50, 50, 50, 50, 50, 50};
            store.getWritable<ESM4::Npc>().insertStatic(native, native.mFormKey);
            if (!mWorld.initializeOblivionPlayerActor()) throw std::runtime_error("training Player initialization failed");
            ESM4::Skill skill{}; skill.mId = {0x830, 0}; skill.mIndex = 29;
            skill.mData = ESM4::SkillData{29, 6, 2, {1, 1}};
            store.getWritable<ESM4::Skill>().insertStatic(skill, ESM::FormKey::content("headless.esm", 0x830));
            auto& combat = *mWorld.getOblivionCombatService();
            auto values = *combat.findActorValues(ESM::FormKey::dynamic("player", 1));
            values.mValues[29].mBase = 98;
            values.mValues[29].mModifiers = {2.f, 3.f, -1.f};
            combat.publishPlayerValues(mWorld.getPlayer(), values,
                MWWorld::resolveOblivionPlayerDynamicBaseSettings(store));
        }
    };
}

TEST(OblivionSkillTraining, QuestRewardPublishesNativeSkillAndSavedAdvancementForMajorAndMinor)
{
    for (const bool major : {false, true})
    {
        SCOPED_TRACE(major);
        TrainingFixture fixture(major);
        auto& world = fixture.mWorld;
        auto& combat = *world.getOblivionCombatService();
        const auto player = world.getPlayerPtr();
        auto& stats = player.getClass().getNpcStats(player);
        const auto personality = ESM::Attribute::Personality;
        auto& skill = stats.getSkill(ESM::RefId(ESM::Skill::Mercantile));
        skill.setProgress(25);
        stats.setLevelProgress(7);
        stats.setSkillIncreasesForAttribute(personality, 3);
        stats.setSkillIncreasesForSpecialization(ESM::Class::Stealth, 254);
        MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
        EXPECT_EQ(ObScript::asInteger(host.call("ModPCSkill", {}, {std::string{"Mercantile"}, std::int64_t{5}}, {}, {})), 0);
        EXPECT_EQ(combat.getPlayerBaseValue(29), 103);
        EXPECT_EQ(combat.getPlayerValue(29), 107.f);
        EXPECT_EQ(skill.getBase(), 103);
        EXPECT_EQ(skill.getModified(), 107);
        EXPECT_EQ(skill.getProgress(), 0);
        EXPECT_EQ(stats.getLevelProgress(), major ? 12 : 7);
        EXPECT_EQ(stats.getSkillIncreasesForAttribute(personality), 8);
        EXPECT_EQ(stats.getSkillIncreasesForSpecialization(ESM::Class::Stealth), 3);
        const auto saved = ESM4::RuntimeState::deserializeBinary(world.captureOblivionRuntimeState().serializeBinary());
        EXPECT_EQ(saved.mPlayer.mActorValues.at("skill_advances.29"), 5);
        ESM::NpcStats shared{}; shared.blank(); stats.writeState(shared);
        auto stream = std::make_unique<std::stringstream>();
        ESM::ESMWriter writer; writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion); writer.save(*stream);
        writer.startRecord(ESM::REC_PLAY); shared.save(writer); writer.endRecord(ESM::REC_PLAY); writer.close();
        ESM::ESMReader reader;
        reader.open(std::move(stream), "training-shared-stats"); reader.getRecName(); reader.getRecHeader();
        ESM::NpcStats restored{}; restored.blank(); restored.load(reader);
        EXPECT_EQ(restored.mLevelProgress, major ? 12 : 7);
        EXPECT_EQ(restored.mSkillIncrease.at(personality), 8);
        EXPECT_EQ(restored.mSpecIncreases[2], 3);
        EXPECT_EQ(restored.mSkills.at(ESM::RefId(ESM::Skill::Mercantile)).mProgress, 0);
    }
}

TEST(OblivionSkillTraining, InvalidRewardInputsLeaveNativeValuesAndBookkeepingUntouched)
{
    TrainingFixture fixture(false);
    auto& world = fixture.mWorld;
    MWWorld::OblivionScriptManager host(world, world.getStore(), {"headless.esm"});
    const auto before = world.captureOblivionRuntimeState();
    const auto player = world.getPlayerPtr();
    const auto& stats = player.getClass().getNpcStats(player);
    for (const auto& skill : {"Strength", "Mercantile"})
        EXPECT_THROW(host.call("ModPCSkill", {}, {std::string{skill}, std::int64_t{-1}}, {}, {}), ObScript::RuntimeError);
    EXPECT_THROW(host.call("ModPCSkill", {}, {std::string{"Mercantile"}, std::numeric_limits<std::int64_t>::max()}, {}, {}),
        ObScript::RuntimeError);
    EXPECT_EQ(ObScript::asInteger(host.call("ModPCSkill", {}, {std::string{"Mercantile"}, std::int64_t{0}}, {}, {})), 0);
    const auto after = world.captureOblivionRuntimeState();
    EXPECT_EQ(after.mNativeActorValues, before.mNativeActorValues);
    EXPECT_EQ(after.mPlayer.mActorValues, before.mPlayer.mActorValues);
    EXPECT_EQ(stats.getLevelProgress(), 0);
    EXPECT_EQ(stats.getSkillIncreasesForAttribute(ESM::Attribute::Personality), 0);
}
