#include "oblivionworldfixture.hpp"

#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwworld/player.hpp"
#include "apps/openmw/mwlua/luamanagerimp.hpp"
#include "apps/openmw/mwsound/soundmanagerimp.hpp"

#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/runtimestate.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/files/collections.hpp>
#include <components/loadinglistener/loadinglistener.hpp>

#include <gtest/gtest.h>

#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace TestingOpenMW
{
    NativeWorldFixture::NativeWorldFixture(bool scriptsFirst, bool extraNative, bool activationScript,
        const std::string& scriptSource, const std::filesystem::path& luaLibs,
        const std::string& extraRecords, std::uint32_t extraRecordCount)
    {
        mEnvironment.setResourceSystem(mResources);
        mEnvironment.setWorld(mWorld);
        mEnvironment.setWorldModel(mWorld.getWorldModel());
        mEnvironment.setESMStore(mWorld.getStore());
        mLuaManager = std::make_unique<MWLua::LuaManager>(&mVfs, luaLibs);
        mEnvironment.setLuaManager(*mLuaManager);
        mSoundManager = std::make_unique<MWSound::SoundManager>(&mVfs, false);
        mEnvironment.setSoundManager(*mSoundManager);

        const auto bytes = [](const auto& value) {
            return std::string(reinterpret_cast<const char*>(&value), sizeof(value));
        };
        const auto subrecord = [&](std::uint32_t tag, const std::string& data) {
            return bytes(tag) + bytes(static_cast<std::uint16_t>(data.size())) + data;
        };
        const auto record = [&](std::uint32_t tag, std::uint32_t id, const std::string& data) {
            return bytes(tag) + bytes(static_cast<std::uint32_t>(data.size()))
                + bytes(std::uint32_t{0}) + bytes(id) + bytes(std::uint32_t{0}) + data;
        };
        ESM4::ACBS_TES4 config{};
        config.baseSpell = 20;
        config.fatigue = 40;
        config.levelOrOffset = config.calcMin = config.calcMax = 1;
        std::array<std::uint8_t, 21> skills;
        skills.fill(5);
        std::array<std::uint8_t, 8> attributes;
        attributes.fill(40);
        const auto header = record(ESM4::REC_TES4, 0,
            subrecord(ESM::fourCC("HEDR"), bytes(1.f)
                + bytes(std::uint32_t{(activationScript ? 2u : 1u) + extraRecordCount})
                + bytes(std::uint32_t{extraRecords.empty() ? (activationScript ? 0x881u : 0x801u) : 0x1000u})));
        const auto npc = record(ESM4::REC_NPC_, 0x800,
            subrecord(ESM::fourCC("EDID"), std::string("Player\0", 7))
            + subrecord(ESM::fourCC("ACBS"), bytes(config).substr(0, 16))
            + subrecord(ESM::fourCC("DATA"), bytes(skills) + bytes(std::uint32_t{100}) + bytes(attributes))
            + (activationScript ? subrecord(ESM::fourCC("SCRI"), bytes(std::uint32_t{0x880})) : std::string{}));
        {
            std::ofstream stream(mDirectory / "headless.esm", std::ios::binary);
            stream << header << npc << extraRecords;
            if (activationScript)
            {
                const std::array<std::uint32_t, 6> local{1, 0, 0, 0, 1, 0};
                stream << record(ESM4::REC_SCPT, 0x880,
                    subrecord(ESM::fourCC("EDID"), std::string("ActivationReceiver\0", 19))
                    + subrecord(ESM::fourCC("SLSD"), bytes(local))
                    + subrecord(ESM::fourCC("SCVR"), std::string("calls\0", 6))
                    + subrecord(ESM::fourCC("SCTX"), scriptSource.empty()
                        ? "scn ActivationReceiver\nshort calls\nBegin OnActivate\nset calls to calls + 1\nEnd\n"
                        : scriptSource));
            }
            if (!stream)
                throw std::runtime_error("failed to write native world fixture");
        }
        Files::Collections files(Files::PathContainer{mDirectory});
        Loading::Listener listener;
        if (scriptsFirst)
        {
            std::ofstream scripts(mDirectory / "prefix.omwscripts");
            scripts << "# Empty script configuration shifts the resolved content index.\n";
        }
        std::vector<std::string> content;
        if (scriptsFirst)
            content.push_back("prefix.omwscripts");
        content.push_back("headless.esm");
        if (extraNative)
        {
            std::ofstream second(mDirectory / "second.esm", std::ios::binary);
            second << record(ESM4::REC_TES4, 0, subrecord(ESM::fourCC("HEDR"),
                bytes(1.f) + bytes(std::uint32_t{0}) + bytes(std::uint32_t{0x800})));
            second.close();
            if (!second)
                throw std::runtime_error("failed to write second native content fixture");
            content.push_back("second.esm");
        }
        mWorld.loadData(files, content, {}, nullptr, &listener);
    }

    void readNativeSnapshot(NativeWorldFixture& fixture, const ESM4::RuntimeState& state)
    {
        ESM::ESMWriter writer;
        auto stream = std::make_unique<std::stringstream>();
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        writer.save(*stream);
        writer.startRecord(ESM::REC_T4ST);
        state.save(writer);
        writer.endRecord(ESM::REC_T4ST);
        ESM::ESMReader reader;
        reader.open(std::move(stream), "native-marker-fixture");
        ASSERT_EQ(reader.getRecName(), ESM::REC_T4ST);
        reader.getRecHeader();
        fixture.mWorld.readRecord(reader, ESM::REC_T4ST);
    }

    void prepareNativeSnapshotPlayer(NativeWorldFixture& fixture, const ESM4::RuntimeState& state)
    {
        auto& world = fixture.mWorld;
        MWClass::Npc::registerSelf();
        if (world.getPlayerPtr().isEmpty())
            world.setupPlayer();
        auto& store = world.getStore();
        ESM::Race race{};
        race.blank();
        race.mId = ESM::RefId(ESM::FormId{0x810, 0});
        if (!store.get<ESM::Race>().search(race.mId))
            store.getWritable<ESM::Race>().insertStatic(race);
        if (!world.getPlayerPtr().get<ESM::NPC>()->mBase->mRace.getIf<ESM::FormId>())
        {
            auto player = *world.getPlayerPtr().get<ESM::NPC>()->mBase;
            player.mRace = race.mId;
            auto metadata = store.preparePlayerRecord(player);
            world.getPlayer().set(metadata.commit());
        }
        const ESM::FormKeyResolver resolver({"headless.esm"});
        const auto prepareCell = [&](const ESM::FormKey& key) {
            if (store.get<ESM4::Cell>().search(key))
                return;
            const auto id = resolver.toFormId(key);
            if (!id)
                throw std::runtime_error("snapshot fixture requires a native cell");
            ESM4::Cell cell{};
            cell.mId = ESM::RefId(*id);
            cell.mFormKey = key;
            cell.mCellFlags = ESM4::CELL_Interior;
            cell.mEditorId = "AcceptedSnapshotFixture";
            store.getWritable<ESM4::Cell>().insertStatic(cell, key);
        };
        prepareCell(state.mPlayer.mCell);
        for (const auto& reference : state.mReferences)
            prepareCell(reference.mCell);
    }

    NativeWorldFixture::~NativeWorldFixture() = default;
}
