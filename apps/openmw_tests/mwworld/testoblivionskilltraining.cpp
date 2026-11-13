#include "oblivionworldfixture.hpp"

#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwclass/esm4interactive.hpp"
#include "apps/openmw/mwworld/livecellref.hpp"
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadrefr.hpp>
#include "apps/openmw/mwmechanics/npcstats.hpp"
#include "apps/openmw/mwmechanics/oblivioncombat.hpp"
#include "apps/openmw/mwworld/class.hpp"
#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/oblivionactorstats.hpp"
#include "apps/openmw/mwworld/oblivionskilltraining.hpp"
#include "apps/openmw/mwworld/oblivionscriptmanager.hpp"
#include "apps/openmw/mwworld/player.hpp"
#include "apps/openmw/mwworld/worldmodel.hpp"

#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/esm4/loadclas.hpp>
#include <components/esm4/loadgmst.hpp>
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

TEST(OblivionSkillTraining, OrdinaryUseRetainsFractionalExperienceAndAwardsOnlyEarnedLevels)
{
    TrainingFixture fixture(true);
    auto& world = fixture.mWorld;
    auto& combat = *world.getOblivionCombatService();
    const auto player = world.getPlayerPtr();
    auto& stats = player.getClass().getNpcStats(player);
    auto& skill = stats.getSkill(ESM::RefId(ESM::Skill::Mercantile));
    const float required = MWWorld::oblivionPlayerSkillProgressRequirement(world.getStore(), player, 29);
    skill.setProgress(required - .5f);
    stats.setLevelProgress(4);
    world.useOblivionPlayerSkill(29, 0);
    EXPECT_EQ(combat.getPlayerBaseValue(29), 99);
    EXPECT_EQ(combat.getPlayerValue(29), 103);
    EXPECT_FLOAT_EQ(skill.getProgress(), .5f);
    EXPECT_EQ(stats.getLevelProgress(), 5);
    EXPECT_EQ(stats.getSkillIncreasesForAttribute(ESM::Attribute::Personality), 1);
    const auto saved = world.captureOblivionRuntimeState();
    EXPECT_EQ(saved.mPlayer.mActorValues.at("skill_advances.29"), 1);
    ESM::NpcStats shared{}; shared.blank(); stats.writeState(shared);
    auto stream = std::make_unique<std::stringstream>();
    ESM::ESMWriter writer; writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion); writer.save(*stream);
    writer.startRecord(ESM::REC_PLAY); shared.save(writer); writer.endRecord(ESM::REC_PLAY); writer.close();
    ESM::ESMReader reader;
    reader.open(std::move(stream), "ordinary-skill-shared-stats"); reader.getRecName(); reader.getRecHeader();
    ESM::NpcStats restored{}; restored.blank(); restored.load(reader);
    EXPECT_FLOAT_EQ(restored.mSkills.at(ESM::RefId(ESM::Skill::Mercantile)).mProgress, .5f);
    EXPECT_EQ(restored.mLevelProgress, 5);
    world.useOblivionPlayerSkill(29, 0, 0);
    EXPECT_FLOAT_EQ(skill.getProgress(), .5f);
    EXPECT_EQ(world.captureOblivionRuntimeState().mPlayer.mActorValues.at("skill_advances.29"), 1);
    EXPECT_THROW(world.useOblivionPlayerSkill(29, 2), std::invalid_argument);
    EXPECT_THROW(world.useOblivionPlayerSkill(29, 0, -1), std::invalid_argument);
    EXPECT_FLOAT_EQ(skill.getProgress(), .5f);
    world.useOblivionPlayerSkill(29, 0, 1000);
    EXPECT_EQ(combat.getPlayerBaseValue(29), 100);
    EXPECT_EQ(combat.getPlayerValue(29), 104);
    EXPECT_FLOAT_EQ(skill.getProgress(), 0);
    EXPECT_EQ(stats.getLevelProgress(), 6);
    world.useOblivionPlayerSkill(29, 0, 1000);
    EXPECT_EQ(stats.getLevelProgress(), 6);
    EXPECT_EQ(world.captureOblivionRuntimeState().mPlayer.mActorValues.at("skill_advances.29"), 2);
}

TEST(OblivionSkillTraining, SkillBooksPreserveExperienceModifiersAndBaseHistoryAcrossCopiesAndCap)
{
    for (const bool major : {false, true})
    {
        SCOPED_TRACE(major);
        TrainingFixture fixture(major);
        MWClass::ESM4Book::registerSelf();
        auto& world = fixture.mWorld;
        auto& store = world.getStore();
        auto& combat = *world.getOblivionCombatService();
        const auto player = world.getPlayerPtr();
        auto& stats = player.getClass().getNpcStats(player);
        auto& skill = stats.getSkill(ESM::RefId(ESM::Skill::Mercantile));
        skill.setProgress(25.f);
        const auto originalModifiers = combat.findActorValues(ESM::FormKey::dynamic("player", 1))->mValues[29].mModifiers;
        const auto readBook = [&](unsigned id, unsigned reference) {
            const auto key = ESM::FormKey::content("headless.esm", id);
            if (!store.get<ESM4::Book>().search(key))
            {
                ESM4::Book book{}; book.mId = {id, 0}; book.mData.bookSkill = 17;
                store.getWritable<ESM4::Book>().insertStatic(book, key);
            }
            ESM4::Reference placed{}; placed.mId = {reference, 0}; placed.mBaseObj = {id, 0};
            placed.mFormKey = ESM::FormKey::content("headless.esm", reference);
            placed.mBaseKey = key;
            MWWorld::LiveCellRef<ESM4::Book> live(placed, store.get<ESM4::Book>().search(key));
            EXPECT_TRUE(world.readOblivionSkillBook(MWWorld::Ptr(&live, nullptr)));
        };
        readBook(0x840, 0x850);
        EXPECT_EQ(combat.getPlayerBaseValue(29), 99);
        EXPECT_EQ(skill.getProgress(), 25.f);
        EXPECT_EQ(stats.getLevelProgress(), major ? 1 : 0);
        EXPECT_EQ(stats.getSkillIncreasesForAttribute(ESM::Attribute::Personality), 1);
        EXPECT_EQ(combat.findActorValues(ESM::FormKey::dynamic("player", 1))->mValues[29].mModifiers, originalModifiers);
        readBook(0x840, 0x851); // A second reference to the same base cannot award again.
        EXPECT_EQ(combat.getPlayerBaseValue(29), 99);
        readBook(0x841, 0x852);
        EXPECT_EQ(combat.getPlayerBaseValue(29), 100);
        readBook(0x842, 0x853); // Reading at the cap does not exhaust the book.
        auto saved = world.captureOblivionRuntimeState();
        EXPECT_EQ(saved.mPlayer.mAwardedSkillBooks.size(), 2u);
        EXPECT_FALSE(saved.mPlayer.mAwardedSkillBooks.contains(ESM::FormKey::content("headless.esm", 0x842)));
        EXPECT_EQ(saved.mPlayer.mActorValues.at("pc_misc_stat.17"), 4);
        EXPECT_EQ(saved.mPlayer.mActorValues.at("pc_misc_stat.18"), 2);
        EXPECT_EQ(saved.mPlayer.mActorValues.at("pc_misc_stat.2"), 2);
        EXPECT_EQ(saved.mPlayer.mActorValues.at("skill_advances.29"), 2);
        EXPECT_NO_THROW(MWWorld::validateOblivionAwardedSkillBooks(store, saved));
        EXPECT_EQ(ESM4::RuntimeState::deserializeBinary(saved.serializeBinary()).mPlayer.mAwardedSkillBooks,
            saved.mPlayer.mAwardedSkillBooks);
        ASSERT_TRUE(world.executeOblivionActorValueCommand(player, 29, ESM4::ActorValueCommand::Set,
            ESM4::ActorValueCommandSource::Script, 99));
        readBook(0x842, 0x854);
        EXPECT_EQ(combat.getPlayerBaseValue(29), 100);
        EXPECT_EQ(skill.getProgress(), 25.f);
        EXPECT_EQ(stats.getLevelProgress(), major ? 3 : 0);
        EXPECT_EQ(world.captureOblivionRuntimeState().mPlayer.mAwardedSkillBooks.size(), 3u);
    }
}

TEST(OblivionSkillTraining, OriginalDestructionUsesCrossOneMinorLevelAndPersistItsStatistics)
{
    TrainingFixture fixture(false);
    auto& world = fixture.mWorld;
    auto& store = world.getStore();
    ESM4::Skill definition{};
    definition.mId = {0x840, 0}; definition.mIndex = 22;
    definition.mData = ESM4::SkillData{22, 2, 1, {0, 1.2f}};
    store.getWritable<ESM4::Skill>().insertStatic(definition, ESM::FormKey::content("headless.esm", 0x840));
    std::uint32_t id = 0x841;
    for (const auto& [name, value] : std::array{
             std::pair{"fSkillUseFactor", .35f}, std::pair{"fSkillUseExp", 1.5f}})
    {
        ESM4::GameSetting setting{};
        setting.mId = {id, 0}; setting.mEditorId = name; setting.mData = value;
        store.getWritable<ESM4::GameSetting>().insertStatic(setting, ESM::FormKey::content("headless.esm", id++));
    }
    auto& combat = *world.getOblivionCombatService();
    auto values = *combat.findActorValues(ESM::FormKey::dynamic("player", 1));
    values.mValues[22].mBase = 5;
    values.mValues[22].mModifiers = {2.f, 3.f, -1.f};
    combat.publishPlayerValues(world.getPlayer(), values, MWWorld::resolveOblivionPlayerDynamicBaseSettings(store));
    const auto player = world.getPlayerPtr();
    auto& stats = player.getClass().getNpcStats(player);
    auto& skill = stats.getSkill(ESM::RefId(ESM::Skill::Destruction));
    skill.setProgress(0);
    for (int i = 0; i < 4; ++i) world.useOblivionPlayerSkill(22, 1);
    EXPECT_EQ(combat.getPlayerBaseValue(22), 6);
    EXPECT_EQ(combat.getPlayerValue(22), 10);
    EXPECT_NEAR(skill.getProgress(), 1.9062095f, .001f);
    EXPECT_EQ(stats.getLevelProgress(), 0);
    EXPECT_EQ(stats.getSkillIncreasesForAttribute(ESM::Attribute::Willpower), 1);
    EXPECT_EQ(stats.getSkillIncreasesForSpecialization(ESM::Class::Magic), 1);
    const auto saved = ESM4::RuntimeState::deserializeBinary(world.captureOblivionRuntimeState().serializeBinary());
    EXPECT_EQ(saved.mPlayer.mActorValues.at("skill_advances.22"), 1);
    EXPECT_EQ(saved.mPlayer.mActorValues.at("pc_misc_stat.2"), 1);
    world.useOblivionPlayerSkill(22, 1, 0);
    EXPECT_NEAR(skill.getProgress(), 1.9062095f, .001f);
    EXPECT_EQ(world.captureOblivionRuntimeState().mPlayer.mActorValues.at("pc_misc_stat.2"), 1);
}
