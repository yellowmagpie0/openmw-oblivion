#include <gtest/gtest.h>

#include <sstream>
#include <tuple>

#include <apps/openmw/mwworld/preparedsaverecords.hpp>
#include <apps/openmw/mwworld/savedreference.hpp>
#include <components/esm3/actoridconverter.hpp>
#include <components/esm3/containerstate.hpp>
#include <components/esm3/creaturelevliststate.hpp>
#include <components/esm3/creaturestate.hpp>
#include <components/esm3/doorstate.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/player.hpp>
#include <components/lua/configuration.hpp>

namespace
{
    template<class State>
    void populate(State& state, std::uint32_t index)
    {
        state.blank();
        state.mRef.mRefID = ESM::RefId::stringRefId("decoded-reference");
        state.mRef.mRefNum = {index, 0};
        state.mRef.mGlobalVariable = std::string(180, 'g');
        state.mAnimationState.mScriptedAnims.resize(1);
        state.mAnimationState.mScriptedAnims[0].mGroup = std::string(180, 'a');
        state.mAnimationState.mScriptedAnims[0].mLoopCount = 0x100000003ull;
    }

    std::unique_ptr<std::stringstream> references()
    {
        auto stream = std::make_unique<std::stringstream>();
        ESM::ESMWriter writer; writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        writer.save(*stream); writer.startRecord(ESM::REC_CSTA);
        ESM::ObjectState object{}; populate(object, 1); object.mHasCustomState = false;
        ESM::DoorState door{}; populate(door, 2); door.mDoorState = 2;
        ESM::CreatureLevListState list{}; populate(list, 3); list.mSpawn = true; list.mSpawnedActor = {20, -1};
        ESM::ContainerState container{}; populate(container, 4);
        ESM::NpcState npc{}; populate(npc, 5);
        ESM::CreatureState creature{}; populate(creature, 6);
        ESM::ObjectState item{}; populate(item, 50); item.mHasCustomState = false;
        container.mInventory.mItems.push_back(item); npc.mInventory.mItems.push_back(item);
        creature.mInventory.mItems.push_back(item);
        npc.mNpcStats.mBounty = 137;
        auto travel = std::make_unique<ESM::AiSequence::AiTravel>(); travel->mData = {12.f, 0.f, 0.f}; travel->mHidden = false; travel->mRepeat = false;
        npc.mCreatureStats.mAiSequence.mPackages.push_back({ESM::AiSequence::Ai_Travel, std::move(travel)});
        const auto write = [&](std::uint32_t type, const ESM::ObjectState& state) {
            writer.writeHNT("OBJE", type); state.save(writer);
        };
        write(ESM::REC_WEAP, object); write(ESM::REC_DOOR, door); write(ESM::REC_LEVC, list);
        write(ESM::REC_CONT, container); write(ESM::REC_NPC_, npc); write(ESM::REC_CREA, creature);
        npc.mHasCustomState = false; npc.mRef.mRefNum = {7, 0}; write(ESM::REC_NPC_, npc);
        writer.endRecord(ESM::REC_CSTA); writer.close();
        return stream;
    }

    void openRecord(ESM::ESMReader& reader, std::unique_ptr<std::stringstream> stream)
    {
        reader.open(std::move(stream), "prepared-records"); reader.getRecName(); reader.getRecHeader();
    }
}

TEST(PreparedSaveRecordsTest, TypedOwnershipCursorAndConvertersSurvivePreparation)
{
    ESM::ESMReader reader; openRecord(reader, references());
    std::map<int, int> mapping{{0, 3}}; reader.setContentFileMapping(&mapping);
    const auto start = reader.getContext();
    MWWorld::PreparedSaveRecords records(reader);
    std::vector<ESM::ObjectState*> addresses;
    while (reader.isNextSub("OBJE"))
    {
        std::uint32_t type; reader.getHT(type);
        ESM::CellRef reference; reference.loadId(reader, true);
        const auto begin = reader.getFileOffset();
        auto state = MWWorld::readSavedReferenceState(reader, reference, type);
        addresses.push_back(state.get());
        records.addReference(reader, begin, type, std::move(state));
        // Reproduce admission's in-place owner mapping; restore must recover
        // raw saved identity without remapping inventory item identities twice.
        addresses.back()->mRef.mRefNum = {999, 8};
    }
    ASSERT_EQ(records.size(), 7u);
    reader.restoreContext(start);
    ESM::ActorIdConverter converter; reader.mActorIdConverter = &converter;
    std::size_t index = 0;
    while (reader.isNextSub("OBJE"))
    {
        std::uint32_t type; reader.getHT(type);
        ESM::CellRef reference; reference.loadId(reader, true);
        const auto begin = reader.getContext();
        EXPECT_THROW(records.takeReference(reader, reference, 0), std::logic_error);
        EXPECT_EQ(reader.getFileOffset(), begin.filePos);
        auto state = records.takeReference(reader, reference, type); ASSERT_TRUE(state);
        EXPECT_EQ(state.get(), addresses.at(index++)); EXPECT_EQ(state->mRef.mRefNum, reference.mRefNum);
        EXPECT_EQ(state->mActorIdConverter, &converter);
        EXPECT_EQ(state->mRef.mGlobalVariable, std::string(180, 'g'));
        ASSERT_EQ(state->mAnimationState.mScriptedAnims.size(), 1u);
        EXPECT_EQ(state->mAnimationState.mScriptedAnims[0].mLoopCount, 0x100000003ull);
        ESM::InventoryState* inventory = nullptr; ESM::CreatureStats* stats = nullptr;
        if (auto* npc = dynamic_cast<ESM::NpcState*>(state.get()))
        {
            if (state->mHasCustomState)
            {
                EXPECT_EQ(npc->mNpcStats.mBounty, 137);
                ASSERT_EQ(npc->mCreatureStats.mAiSequence.mPackages.size(), 1u);
                EXPECT_EQ(static_cast<ESM::AiSequence::AiTravel&>(*npc->mCreatureStats.mAiSequence.mPackages[0].mPackage).mData.mX, 12.f);
                inventory = &npc->mInventory; stats = &npc->mCreatureStats;
            }
            else
            {
                EXPECT_EQ(npc->mCreatureStats.mAiSequence.mActorIdConverter, nullptr);
                EXPECT_EQ(npc->mCreatureStats.mActiveSpells.mActorIdConverter, nullptr);
            }
        }
        else if (auto* creature = dynamic_cast<ESM::CreatureState*>(state.get()))
        { inventory = &creature->mInventory; stats = &creature->mCreatureStats; }
        else if (auto* container = dynamic_cast<ESM::ContainerState*>(state.get())) inventory = &container->mInventory;
        else if (auto* door = dynamic_cast<ESM::DoorState*>(state.get())) { EXPECT_EQ(door->mDoorState, 2); }
        else if (auto* list = dynamic_cast<ESM::CreatureLevListState*>(state.get())) { EXPECT_TRUE(list->mSpawn); }
        if (inventory)
        {
            ASSERT_EQ(inventory->mItems.size(), 1u);
            EXPECT_EQ(inventory->mItems[0].mActorIdConverter, &converter);
            EXPECT_EQ(inventory->mItems[0].mRef.mRefNum, (ESM::RefNum{50, 3}));
        }
        if (stats)
        {
            EXPECT_EQ(stats->mAiSequence.mActorIdConverter, &converter);
            EXPECT_EQ(stats->mActiveSpells.mActorIdConverter, &converter);
        }
        const auto end = reader.getContext(); reader.restoreContext(begin);
        EXPECT_THROW(records.takeReference(reader, reference, type), std::logic_error);
        reader.restoreContext(end);
    }
    EXPECT_EQ(index, 7u); EXPECT_EQ(records.size(), 0u); EXPECT_FALSE(reader.hasMoreSubs());
}

TEST(PreparedSaveRecordsTest, ReaderIdentityAndReferenceMismatchRejectBeforeConsumption)
{
    ESM::ESMReader reader, foreign; openRecord(reader, references()); openRecord(foreign, references());
    MWWorld::PreparedSaveRecords records(reader);
    ASSERT_TRUE(reader.isNextSub("OBJE")); std::uint32_t type; reader.getHT(type);
    ESM::CellRef reference; reference.loadId(reader, true); const auto start = reader.getContext();
    auto state = MWWorld::readSavedReferenceState(reader, reference, type);
    records.addReference(reader, start.filePos, type, std::move(state)); reader.restoreContext(start);
    const auto foreignStart = foreign.getFileOffset();
    EXPECT_THROW(records.takeReference(foreign, reference, type), std::logic_error);
    EXPECT_EQ(foreign.getFileOffset(), foreignStart);
    auto wrong = reference; ++wrong.mRefNum.mIndex;
    EXPECT_THROW(records.takeReference(reader, wrong, type), std::logic_error);
    EXPECT_EQ(reader.getFileOffset(), start.filePos); EXPECT_EQ(records.size(), 1u);
    EXPECT_TRUE(records.takeReference(reader, reference, type)); EXPECT_EQ(records.size(), 0u);
}

TEST(PreparedSaveRecordsTest, PlayerOwnsCompleteRecordAndRebindsNestedState)
{
    auto stream = std::make_unique<std::stringstream>();
    ESM::ESMWriter writer; writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion); writer.save(*stream);
    ESM::Player source{}; populate(source.mObject, 10); source.mObject.mRef.mRefID = ESM::RefId::stringRefId("Player");
    source.mCellId = ESM::RefId::stringRefId("saved-cell"); source.mSaveSkills[ESM::Skill::LongBlade] = 42.f;
    source.mPreviousItems[ESM::RefId::stringRefId("bound")] = ESM::RefId::stringRefId("previous");
    ESM::ObjectState item{}; populate(item, 11); item.mHasCustomState = false;
    source.mObject.mInventory.mItems.push_back(item);
    writer.startRecord(ESM::REC_PLAY); source.save(writer); writer.endRecord(ESM::REC_PLAY); writer.close();
    ESM::ESMReader reader; openRecord(reader, std::move(stream)); const auto begin = reader.getContext();
    auto player = std::make_unique<ESM::Player>(); player->load(reader); auto* address = player.get();
    MWWorld::PreparedSaveRecords records(reader); records.addPlayer(reader, begin.filePos, std::move(player));
    reader.restoreContext(begin); ESM::ActorIdConverter converter; reader.mActorIdConverter = &converter;
    auto restored = records.takePlayer(reader); ASSERT_TRUE(restored); EXPECT_EQ(restored.get(), address);
    EXPECT_EQ(restored->mSaveSkills.at(ESM::Skill::LongBlade), 42.f);
    EXPECT_EQ(restored->mPreviousItems.at(ESM::RefId::stringRefId("bound")), ESM::RefId::stringRefId("previous"));
    EXPECT_EQ(restored->mObject.mActorIdConverter, &converter);
    EXPECT_EQ(restored->mObject.mInventory.mItems[0].mActorIdConverter, &converter);
    EXPECT_EQ(restored->mObject.mCreatureStats.mActiveSpells.mActorIdConverter, &converter);
    EXPECT_EQ(restored->mObject.mCreatureStats.mAiSequence.mActorIdConverter, &converter);
    EXPECT_FALSE(reader.hasMoreSubs()); EXPECT_EQ(records.size(), 0u);
    reader.restoreContext(begin); EXPECT_THROW(records.takePlayer(reader), std::logic_error);
}

TEST(PreparedSaveRecordsTest, LuaIdentityMatchesOrdinaryDecodeAcrossConfigurationsAndInventory)
{
    for (auto version : {ESM::MaxLuaScriptPathFormatVersion, ESM::CurrentSaveGameFormatVersion})
    for (int mode : {0, 1, 2})
    {
        SCOPED_TRACE(version);
        SCOPED_TRACE(mode);
        const auto configuration = [](std::initializer_list<const char*> paths) {
            ESM::LuaScriptsCfg cfg;
            for (const auto* path : paths)
            {
                ESM::LuaScriptCfg script{};
                script.mScriptPath = VFS::Path::Normalized(path);
                script.mFlags = ESM::LuaScriptCfg::sCustom;
                cfg.mScripts.push_back(std::move(script));
            }
            LuaUtil::ScriptsConfiguration result; result.init(std::move(cfg), false); return result;
        };
        auto admittedCfg = configuration({"probe.lua"});
        auto changedCfg = configuration({"other.lua", "probe.lua", "added.lua"});
        auto stream = std::make_unique<std::stringstream>();
        ESM::ESMWriter writer; writer.setFormatVersion(version); writer.save(*stream);
        writer.startRecord(ESM::REC_CSTA);
        ESM::CellRef reference; reference.blank();
        reference.mRefID = ESM::RefId::stringRefId("lua-container"); reference.save(writer, true);
        const auto script = [&](const char* path, int id, const char* data) {
            if (version <= ESM::MaxLuaScriptPathFormatVersion) writer.writeHNString("LUAS", path);
            else writer.writeHNT("LUAS", id);
            ESM::saveLuaBinaryData(writer, data);
        };
        script("probe.lua", 0, "root"); script("added.lua", 99, "previously-unknown");
        writer.writeHNT("ICNT", std::uint32_t{1});
        ESM::CellRef item; item.blank(); item.mRefID = ESM::RefId::stringRefId("lua-item"); item.save(writer, true);
        script("probe.lua", 0, "inventory"); writer.writeHNT("HCUS", false);
        writer.endRecord(ESM::REC_CSTA); writer.close();
        ESM::ESMReader reader; openRecord(reader, std::move(stream)); reference.loadId(reader, true);
        const auto begin = reader.getContext(); reader.mScriptsConfiguration = &admittedCfg;
        auto admitted = std::make_unique<ESM::ContainerState>(); admitted->mRef = reference; admitted->load(reader);
        MWWorld::PreparedSaveRecords records(reader);
        records.addReference(reader, begin.filePos, ESM::REC_CONT, std::move(admitted));
        reader.restoreContext(begin);
        reader.mScriptsConfiguration = mode == 0 ? nullptr : mode == 1 ? &admittedCfg : &changedCfg;
        auto decoded = records.takeReference(reader, reference, ESM::REC_CONT); ASSERT_TRUE(decoded);
        auto& cached = decoded->asContainerState();
        reader.restoreContext(begin);
        ESM::ContainerState ordinary; ordinary.mRef = reference; ordinary.load(reader);
        ASSERT_EQ(cached.mLuaScripts.mScripts.size(), 2u);
        EXPECT_EQ(cached.mLuaScripts.mScripts[1].mLegacyPath.has_value(), version <= ESM::MaxLuaScriptPathFormatVersion);
        ASSERT_EQ(ordinary.mLuaScripts.mScripts.size(), 2u);
        for (std::size_t i = 0; i < 2; ++i)
        {
            EXPECT_EQ(cached.mLuaScripts.mScripts[i].mScriptId, ordinary.mLuaScripts.mScripts[i].mScriptId);
            EXPECT_EQ(cached.mLuaScripts.mScripts[i].mData, ordinary.mLuaScripts.mScripts[i].mData);
        }
        ASSERT_EQ(cached.mInventory.mItems.size(), 1u);
        ASSERT_EQ(ordinary.mInventory.mItems.size(), 1u);
        EXPECT_EQ(cached.mInventory.mItems[0].mLuaScripts.mScripts[0].mScriptId,
            ordinary.mInventory.mItems[0].mLuaScripts.mScripts[0].mScriptId);
    }
}
