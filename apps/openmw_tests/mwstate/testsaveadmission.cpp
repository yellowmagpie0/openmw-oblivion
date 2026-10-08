#include <gtest/gtest.h>

#include <osg/Image>
#include <osgDB/Registry>

#include <cstring>
#include <cmath>
#include <array>
#include <set>
#include <sstream>
#include <limits>
#include <type_traits>
#include <tuple>

#include <components/esm3/controlsstate.hpp>
#include <components/esm3/custommarkerstate.hpp>
#include <components/esm3/dialoguestate.hpp>
#include <components/esm3/globalmap.hpp>
#include <components/esm3/globalscript.hpp>
#include <components/esm3/journalentry.hpp>
#include <components/esm3/projectilestate.hpp>
#include <components/esm3/queststate.hpp>
#include <components/esm3/quickkeys.hpp>
#include <components/esm3/stolenitems.hpp>
#include <components/esm3/weatherstate.hpp>
#include <components/misc/rng.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadbsgn.hpp>
#include <components/esm3/loadspel.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadglob.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loadweap.hpp>
#include <components/esm3/loadacti.hpp>
#include <components/esm3/player.hpp>
#include <components/esm3/cellstate.hpp>
#include <components/esm3/containerstate.hpp>
#include <components/esm3/creaturestate.hpp>
#include <components/esm3/creaturelevliststate.hpp>
#include <components/esm3/doorstate.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadappa.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadligh.hpp>
#include <components/esm4/loadkeym.hpp>
#include <components/esm4/loadsgst.hpp>
#include <components/esm4/loadslgm.hpp>
#include <components/esm4/inventorymechanics.hpp>
#include <components/esm4/runtimestate.hpp>

#include "apps/openmw/mwstate/saveadmission.hpp"
#include "apps/openmw/mwclass/weapon.hpp"
#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/inventorystore.hpp"
#include "apps/openmw/mwworld/savedreference.hpp"
#include "apps/openmw/mwworld/timestamp.hpp"
#include "apps/openmw/mwrender/globalmap.hpp"
#include "apps/openmw/mwlua/userdataserializer.hpp"
#include "apps/openmw/mwlua/object.hpp"
#include <components/lua/configuration.hpp>
#include <components/lua/serialization.hpp>

namespace
{
    std::string saveBytes(ESM::GameProfile profile = ESM::GameProfile::Oblivion,
        std::uint32_t version = ESM4::CurrentRuntimeStateVersion,
        int profiles = 1, int nativeRecords = 1, bool trailingProfile = false, bool trailingNative = false,
        const ESM4::RuntimeState* replacement = nullptr)
    {
        ESM::ESMWriter writer;
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        std::stringstream stream;
        writer.save(stream);
        ESM::SavedGame saved{};
        saved.mGameProfile = profile;
        saved.mRuntimeStateVersion = profile == ESM::GameProfile::Oblivion ? version : 0;
        saved.mPlayerName = "Admission Player";
        saved.mPlayerCellName = "Admission Cell";
        saved.mDescription = "Admission fixture";
        saved.mContentFiles = {"headless.esm"};
        for (int i = 0; i < profiles; ++i)
        {
            writer.startRecord(ESM::REC_SAVE);
            saved.save(writer);
            if (trailingProfile)
                writer.writeHNT("EXTR", std::uint32_t{1});
            writer.endRecord(ESM::REC_SAVE);
        }
        ESM4::RuntimeState state;
        state.mVersion = version;
        state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        state.mPlayer.mCell = ESM::FormKey::content("headless.esm", 1);
        if (version >= 3)
        {
            state.mPlayer.mName = saved.mPlayerName;
            state.mPlayer.mRace = ESM::FormKey::content("headless.esm", 2);
            state.mPlayer.mClass = ESM::FormKey::content("headless.esm", 3);
        }
        state.mContent = {{"headless.esm", "sha256:" + std::string(64, 'a')}};
        if (replacement)
            state = *replacement;
        for (int i = 0; i < nativeRecords; ++i)
        {
            writer.startRecord(ESM::REC_T4ST);
            state.save(writer);
            if (trailingNative)
                writer.writeHNT("EXTR", std::uint32_t{1});
            writer.endRecord(ESM::REC_T4ST);
        }
        writer.startRecord(ESM::fourCC("JUNK"));
        writer.writeHNT("TEST", std::uint32_t{42});
        writer.endRecord(ESM::fourCC("JUNK"));
        return stream.str();
    }

    void openBytes(ESM::ESMReader& reader, const std::string& bytes)
    {
        reader.open(std::make_unique<std::stringstream>(bytes), "save-admission-fixture");
    }

    void replaceUint(std::string& bytes, std::size_t offset, std::uint32_t value)
    {
        ASSERT_LE(offset + sizeof(value), bytes.size());
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }

    std::string sharedRecords(const ESM::NPC* player, const ESM::Class* characterClass, const ESM::Global* global,
        bool deleted = false)
    {
        ESM::ESMWriter writer;
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        std::stringstream stream;
        writer.save(stream);
        const auto write = [&](const auto* record) {
            if (!record) return;
            using Record = std::remove_cv_t<std::remove_pointer_t<decltype(record)>>;
            writer.startRecord(Record::sRecordId);
            record->save(writer, deleted);
            writer.endRecord(Record::sRecordId);
        };
        write(player);
        write(characterClass);
        write(global);
        ESM::ESMReader reader;
        openBytes(reader, stream.str());
        return stream.str().substr(reader.getFileOffset());
    }

    ESM4::RuntimeState nativeState()
    {
        ESM::ESMReader reader;
        openBytes(reader, saveBytes());
        while (reader.hasMoreRecs())
        {
            const auto type = reader.getRecName();
            reader.getRecHeader();
            if (type == ESM::REC_T4ST)
            {
                ESM4::RuntimeState state;
                state.load(reader);
                return state;
            }
            reader.skipRecord();
        }
        throw std::logic_error("fixture has no native state");
    }

    std::string worldRecords(const std::function<void(ESM::ESMWriter&)>& write)
    {
        ESM::ESMWriter writer;
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        std::stringstream stream;
        writer.save(stream);
        write(writer);
        ESM::ESMReader reader;
        openBytes(reader, stream.str());
        return stream.str().substr(reader.getFileOffset());
    }
}

TEST(SaveAdmissionTest, SharedPlayerBirthsignResolvesBeforeNativePreparationForEveryVersion)
{
    for (std::uint32_t version = 0; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    for (const bool remapped : {false, true})
    for (int mode = 0; mode != 4; ++mode)
    {
        SCOPED_TRACE(version);
        SCOPED_TRACE(remapped);
        SCOPED_TRACE(mode);
        MWWorld::ESMStore content;
        ESM::BirthSign sign{}; sign.blank();
        sign.mId = remapped ? ESM::RefId(ESM::FormId{0x12, 2}) : ESM::RefId::stringRefId("admission-sign");
        if (mode == 1) content.getWritable<ESM::BirthSign>().insertStatic(sign);
        if (mode == 3) content.getWritable<ESM::BirthSign>().insert(sign); // Outgoing transient definition only.
        const auto records = worldRecords([&](ESM::ESMWriter& writer) {
            ESM::Player player{}; player.mObject.blank();
            player.mObject.mRef.mRefID = ESM::RefId::stringRefId("Player");
            player.mCellId = ESM::RefId(ESM::FormId{1, 0});
            if (mode != 0)
                player.mBirthsign = remapped ? ESM::RefId(ESM::FormId{0x12, 0}) : sign.mId;
            writer.startRecord(ESM::REC_PLAY); player.save(writer); writer.endRecord(ESM::REC_PLAY);
        });
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(ESM::GameProfile::Oblivion,
            version == 0 ? ESM4::CurrentRuntimeStateVersion : version, 1, version == 0 ? 0 : 1) + records);
        const std::map<int, int> mapping{{0, 2}};
        if (remapped) reader.setContentFileMapping(&mapping);
        const auto offset = reader.getFileOffset(); int calls = 0;
        const auto prepare = [&](const auto&, auto) { ++calls; };
        if (mode >= 2)
            EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, &content, prepare), std::runtime_error);
        else
            EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, &content, prepare));
        EXPECT_EQ(calls, int(mode < 2 && version != 0));
        EXPECT_EQ(reader.getFileOffset(), offset);
        EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
        EXPECT_EQ(content.get<ESM::BirthSign>().search(sign.mId) != nullptr, mode == 1 || mode == 3);
    }
}

TEST(SaveAdmissionTest, SharedPlayerAuxiliaryFloatsRejectBeforePreparationForEveryNativeVersion)
{
    // All independently restored float channels, including both halves of
    // recall transforms and the complete werewolf attribute/skill arrays.
    constexpr int channels = 3 + 6 + ESM::Attribute::Length + ESM::Skill::Length;
    const std::array values{std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -123.5f};
    MWWorld::ESMStore content;
    for (std::uint32_t version = 0; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    for (int channel = 0; channel != channels; ++channel)
    for (const float value : values)
    for (const bool prepareState : {false, true})
    {
        SCOPED_TRACE(version);
        SCOPED_TRACE(channel);
        SCOPED_TRACE(value);
        SCOPED_TRACE(prepareState);
        const auto records = worldRecords([&](ESM::ESMWriter& writer) {
            ESM::Player player{}; player.mObject.blank();
            player.mObject.mRef.mRefID = ESM::RefId::stringRefId("Player");
            player.mCellId = ESM::RefId(ESM::FormId{1, 0});
            if (channel < 3)
                player.mLastKnownExteriorPosition[channel] = value;
            else if (channel < 9)
            {
                player.mHasMark = true;
                player.mMarkedCell = player.mCellId;
                if (channel < 6) player.mMarkedPosition.pos[channel - 3] = value;
                else player.mMarkedPosition.rot[channel - 6] = value;
            }
            else if (channel < 9 + ESM::Attribute::Length)
                player.mSaveAttributes[ESM::Attribute::indexToRefId(channel - 9)] = value;
            else
                player.mSaveSkills[ESM::Skill::indexToRefId(channel - 9 - ESM::Attribute::Length)] = value;
            writer.startRecord(ESM::REC_PLAY); player.save(writer); writer.endRecord(ESM::REC_PLAY);
        });
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(ESM::GameProfile::Oblivion,
            version == 0 ? ESM4::CurrentRuntimeStateVersion : version, 1, version == 0 ? 0 : 1) + records);
        const auto offset = reader.getFileOffset();
        int validations = 0, preparations = 0;
        const auto validate = [&](const auto&) { ++validations; };
        const auto prepare = [&](const auto&, auto) { ++preparations; };
        std::function<void(const ESM4::RuntimeState&, std::unique_ptr<MWWorld::ESMStore>)> preparation;
        if (prepareState) preparation = prepare;
        if (std::isfinite(value))
            EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, validate, &content, preparation));
        else
            EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, validate, &content, preparation),
                std::runtime_error);
        EXPECT_EQ(validations, int(std::isfinite(value) && version != 0 && !prepareState));
        EXPECT_EQ(preparations, int(std::isfinite(value) && version != 0 && prepareState));
        EXPECT_EQ(reader.getFileOffset(), offset);
        EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
        EXPECT_EQ(content.get<ESM::NPC>().getDynamicSize(), 0u);
    }
}

TEST(SaveAdmissionTest, SharedPlayerAuxiliaryFloatChecksPreserveMorrowindCompatibility)
{
    const auto records = worldRecords([](ESM::ESMWriter& writer) {
        ESM::Player player{}; player.mObject.blank();
        player.mObject.mRef.mRefID = ESM::RefId::stringRefId("Player");
        player.mCellId = ESM::RefId::stringRefId("Balmora");
        player.mLastKnownExteriorPosition[0] = std::numeric_limits<float>::quiet_NaN();
        player.mHasMark = true;
        player.mMarkedCell = player.mCellId;
        player.mMarkedPosition.rot[2] = std::numeric_limits<float>::infinity();
        player.mSaveSkills[ESM::Skill::Acrobatics] = -std::numeric_limits<float>::infinity();
        writer.startRecord(ESM::REC_PLAY); player.save(writer); writer.endRecord(ESM::REC_PLAY);
    });
    ESM::ESMReader reader;
    openBytes(reader, saveBytes(ESM::GameProfile::Morrowind, 1, 1, 0) + records);
    const auto offset = reader.getFileOffset();
    EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Morrowind, {}));
    EXPECT_EQ(reader.getFileOffset(), offset);
    EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
}

namespace
{
    const auto timestampNpcId = ESM::RefId::stringRefId("timestamp-npc");
    const auto timestampCreatureId = ESM::RefId::stringRefId("timestamp-creature");
    const auto timestampPowerId = ESM::RefId::stringRefId("timestamp-power");

    // owner: Player, shared NPC, shared creature, cell. channel: restock,
    // death, used power, active effect, queued effect (cell has only respawn).
    std::string timestampRecords(int owner, int channel, float hour, bool customState = true)
    {
        return worldRecords([&](ESM::ESMWriter& writer) {
            const ESM::TimeStamp timestamp{hour, -17}; // Preserve signed legacy days.
            ESM::Player player{}; player.mObject.blank();
            ESM::CreatureState creature{}; creature.blank();
            ESM::NpcState npc{}; npc.blank();
            ESM::ObjectState* object = owner == 0 ? &player.mObject : owner == 1
                ? static_cast<ESM::ObjectState*>(&npc) : &creature;
            ESM::CreatureStats* stats = owner == 2 ? &creature.mCreatureStats : owner == 0
                ? &player.mObject.mCreatureStats : &npc.mCreatureStats;
            object->mRef.mRefID = owner == 0 ? ESM::RefId::stringRefId("Player")
                : owner == 1 ? timestampNpcId : timestampCreatureId;
            object->mRef.mRefNum = {1, -1};
            object->mHasCustomState = customState;
            if (channel == 0) stats->mTradeTime = timestamp;
            else if (channel == 1) stats->mTimeOfDeath = timestamp;
            else if (channel == 2) stats->mSpells.mUsedPowers[timestampPowerId] = timestamp;
            else
            {
                ESM::ActiveSpells::ActiveSpellParams spell{};
                spell.mSourceSpellId = timestampPowerId;
                spell.mActiveSpellId = ESM::RefId::generated(23);
                spell.mWorsenings = 0;
                spell.mNextWorsening = timestamp;
                (channel == 3 ? stats->mActiveSpells.mSpells : stats->mActiveSpells.mQueue).push_back(spell);
            }
            if (owner == 0)
            {
                player.mCellId = ESM::RefId(ESM::FormId{1, 0});
                writer.startRecord(ESM::REC_PLAY); player.save(writer); writer.endRecord(ESM::REC_PLAY);
            }
            else
            {
                writer.startRecord(ESM::REC_CSTA);
                writer.writeCellId(ESM::RefId(ESM::FormId{1, 0}));
                ESM::CellState cell{}; cell.mIsInterior = true;
                if (owner == 3) cell.mLastRespawn = timestamp;
                cell.save(writer);
                if (owner != 3)
                {
                    writer.writeHNT("OBJE", std::uint32_t{0});
                    object->save(writer);
                }
                writer.endRecord(ESM::REC_CSTA);
            }
        });
    }

    void installTimestampContent(MWWorld::ESMStore& content)
    {
        ESM::NPC npc{}; npc.blank(); npc.mId = timestampNpcId;
        ESM::Creature creature{}; creature.blank(); creature.mId = timestampCreatureId;
        ESM::Spell spell{}; spell.blank(); spell.mId = timestampPowerId;
        content.getWritable<ESM::NPC>().insertStatic(npc);
        content.getWritable<ESM::Creature>().insertStatic(creature);
        content.getWritable<ESM::Spell>().insertStatic(spell);
        content.setUp();
    }
}

TEST(SaveAdmissionTest, SharedTimestampDomainsRejectBeforePreparationForEveryNativeVersion)
{
    MWWorld::ESMStore content;
    installTimestampContent(content);
    const std::array hours{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), -0.01f, 24.f, -0.f, 0.f, std::nextafter(24.f, 0.f)};
    for (std::uint32_t version = 0; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    for (int owner = 0; owner != 4; ++owner)
    for (int channel = 0; channel != (owner == 3 ? 1 : 5); ++channel)
    for (const float hour : hours)
    {
        SCOPED_TRACE(version);
        SCOPED_TRACE(owner);
        SCOPED_TRACE(channel);
        SCOPED_TRACE(hour);
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(ESM::GameProfile::Oblivion,
            version == 0 ? ESM4::CurrentRuntimeStateVersion : version, 1, version == 0 ? 0 : 1)
            + timestampRecords(owner, channel, hour));
        const auto offset = reader.getFileOffset();
        int calls = 0;
        const auto prepare = [&](const auto&, auto) { ++calls; };
        const bool valid = std::isfinite(hour) && hour >= 0 && hour < 24;
        if (valid)
            EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, &content, prepare));
        else
            EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, &content, prepare),
                std::runtime_error);
        EXPECT_EQ(calls, int(valid && version != 0));
        EXPECT_EQ(reader.getFileOffset(), offset);
        EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
        EXPECT_EQ(content.get<ESM::NPC>().getDynamicSize(), 0u);
        EXPECT_EQ(content.get<ESM::Creature>().getDynamicSize(), 0u);
        EXPECT_EQ(content.get<ESM::Spell>().getDynamicSize(), 0u);
    }
}

TEST(SaveAdmissionTest, SharedUsedPowerTimestampUsesIncomingDefinitionsInEitherRecordOrder)
{
    for (int owner = 0; owner != 3; ++owner)
    for (int definition = 0; definition != 4; ++definition)
    for (const bool definitionFirst : {false, true})
    {
        SCOPED_TRACE(owner);
        SCOPED_TRACE(definition);
        SCOPED_TRACE(definitionFirst);
        MWWorld::ESMStore content;
        ESM::NPC npc{}; npc.blank(); npc.mId = timestampNpcId;
        ESM::Creature creature{}; creature.blank(); creature.mId = timestampCreatureId;
        ESM::Spell spell{}; spell.blank(); spell.mId = timestampPowerId;
        content.getWritable<ESM::NPC>().insertStatic(npc);
        content.getWritable<ESM::Creature>().insertStatic(creature);
        if (definition == 1) content.getWritable<ESM::Spell>().insertStatic(spell);
        if (definition == 2) content.getWritable<ESM::Spell>().insert(spell); // Outgoing only.
        content.setUp();
        const auto savedDefinition = definition == 3 ? worldRecords([&](ESM::ESMWriter& writer) {
            writer.startRecord(ESM::REC_SPEL); spell.save(writer); writer.endRecord(ESM::REC_SPEL);
        }) : std::string{};
        const auto actor = timestampRecords(owner, 2, 24.f);
        ESM::ESMReader reader;
        openBytes(reader, saveBytes() + (definitionFirst ? savedDefinition + actor : actor + savedDefinition));
        const auto offset = reader.getFileOffset(); int calls = 0;
        const auto prepare = [&](const auto&, auto) { ++calls; };
        if (definition == 1 || definition == 3)
            EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, &content, prepare), std::runtime_error);
        else
            EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, &content, prepare));
        EXPECT_EQ(calls, int(definition == 0 || definition == 2));
        EXPECT_EQ(reader.getFileOffset(), offset);
        EXPECT_EQ(content.get<ESM::Spell>().getDynamicSize(), std::size_t(definition == 2));
    }
}

TEST(SaveAdmissionTest, SharedTimestampChecksPreserveOmittedActorStateAndMorrowindAdmission)
{
    MWWorld::ESMStore content;
    installTimestampContent(content);
    for (int owner = 0; owner != 4; ++owner)
    for (int channel = 0; channel != (owner == 3 ? 1 : 5); ++channel)
    {
        SCOPED_TRACE(owner);
        SCOPED_TRACE(channel);
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(ESM::GameProfile::Morrowind, 1, 1, 0) + timestampRecords(owner, channel, 24.f));
        const auto offset = reader.getFileOffset();
        EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Morrowind, {}));
        EXPECT_EQ(reader.getFileOffset(), offset);
        if (owner == 3) continue;
        openBytes(reader, saveBytes() + timestampRecords(owner, channel, 24.f, false));
        int calls = 0;
        EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, &content,
            [&](const auto&, auto) { ++calls; }));
        EXPECT_EQ(calls, 1);
    }
}

TEST(SaveAdmissionTest, ActiveEffectWithoutWorseningDecodesDeterministicTimestampForLiveRestore)
{
    ESM::ActiveSpells state{};
    ESM::ActiveSpells::ActiveSpellParams spell{};
    spell.mSourceSpellId = timestampPowerId;
    spell.mActiveSpellId = ESM::RefId::generated(23);
    spell.mWorsenings = -1;
    spell.mNextWorsening = {std::numeric_limits<float>::quiet_NaN(), 17}; // Not serialized.
    state.mSpells.push_back(spell);
    state.mQueue.push_back(spell);
    const auto records = worldRecords([&](ESM::ESMWriter& writer) {
        writer.startRecord(ESM::fourCC("TEST")); state.save(writer); writer.endRecord(ESM::fourCC("TEST"));
    });
    ESM::ESMReader reader;
    openBytes(reader, saveBytes(ESM::GameProfile::Morrowind, 1, 0, 0) + records);
    ASSERT_EQ(reader.getRecName(), ESM::fourCC("JUNK")); reader.getRecHeader(); reader.skipRecord();
    ASSERT_EQ(reader.getRecName(), ESM::fourCC("TEST")); reader.getRecHeader();
    ESM::ActiveSpells decoded{}; decoded.load(reader);
    ASSERT_EQ(decoded.mSpells.size(), 1u);
    ASSERT_EQ(decoded.mQueue.size(), 1u);
    for (const auto* effects : {&decoded.mSpells, &decoded.mQueue})
    {
        EXPECT_EQ(effects->front().mWorsenings, -1);
        EXPECT_EQ(effects->front().mNextWorsening.mHour, 0.f);
        EXPECT_EQ(effects->front().mNextWorsening.mDay, 0);
        EXPECT_NO_THROW(MWWorld::TimeStamp{effects->front().mNextWorsening});
    }
}

namespace
{
    std::vector<char> mapPng(int width, int height)
    {
        osg::ref_ptr<osg::Image> image = new osg::Image;
        image->allocateImage(width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE);
        std::memset(image->data(), 127, image->getTotalSizeInBytes());
        auto* codec = osgDB::Registry::instance()->getReaderWriterForExtension("png");
        if (!codec) throw std::runtime_error("Test requires the actual PNG codec");
        std::ostringstream stream;
        if (!codec->writeImage(*image, stream).success()) throw std::runtime_error("Test PNG write failed");
        const auto bytes = stream.str();
        return {bytes.begin(), bytes.end()};
    }
}

TEST(SaveAdmissionTest, GlobalMapResourcePreparesBeforeNativeStateForEveryAcceptedVersion)
{
    const auto square = mapPng(2, 2);
    const auto rectangle = mapPng(4, 2);
    const auto remainder = mapPng(4, 5);
    for (std::uint32_t version = 0; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    for (int mode = 0; mode != 7; ++mode)
    for (const bool retain : {false, true})
    {
        SCOPED_TRACE(version);
        SCOPED_TRACE(mode);
        SCOPED_TRACE(retain);
        ESM::GlobalMap map{};
        map.mBounds = {0, 0, 0, 0};
        map.mImageData = mode == 1 || mode == 2 ? rectangle : mode == 3 ? remainder : square;
        if (mode == 2) map.mBounds = {0, 1, 0, 0};
        if (mode == 3) map.mBounds = {0, 1, 0, 1}; // Preserve legacy integer cell-size division.
        if (mode == 4) map.mImageData.clear();
        if (mode == 5) { map.mBounds = {1, 0, 0, 0}; map.mImageData = {'b', 'a', 'd'}; }
        if (mode == 6) map.mImageData = {'b', 'a', 'd'}; // Deliberate unreadable-image skip.
        map.mMarkers.emplace(3, 7);
        const auto records = worldRecords([&](ESM::ESMWriter& writer) {
            writer.startRecord(ESM::REC_GMAP); map.save(writer); writer.endRecord(ESM::REC_GMAP);
        });
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(ESM::GameProfile::Oblivion,
            version ? version : ESM4::CurrentRuntimeStateVersion, 1, version ? 1 : 0) + records);
        const auto offset = reader.getFileOffset();
        int nativeCalls = 0, resourceCalls = 0;
        osg::ref_ptr<osg::Image> retained;
        std::function<void(const ESM::GlobalMap&)> prepare;
        if (retain) prepare = [&](const ESM::GlobalMap& saved) {
            ++resourceCalls;
            EXPECT_EQ(saved.mMarkers, map.mMarkers);
            retained = MWRender::GlobalMap::prepareRead(saved);
        };
        const auto native = [&](const auto&, auto) {
            ++nativeCalls;
            if (retain)
            {
                EXPECT_EQ(resourceCalls, 1);
            }
        };
        if (mode == 1)
            EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, nullptr, native, prepare),
                std::runtime_error);
        else
            EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, nullptr, native, prepare));
        EXPECT_EQ(nativeCalls, int(mode != 1 && version != 0));
        EXPECT_EQ(resourceCalls, int(retain));
        EXPECT_EQ(reader.getFileOffset(), offset);
        EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
        EXPECT_EQ(retained.valid(), retain && mode <= 3 && mode != 1);
        if (retained)
        {
            // A retained decoder result survives destruction/replacement of
            // the source PNG; restore does not decode mutable bytes again.
            map.mImageData.clear();
            EXPECT_EQ(retained->s(), mode == 0 ? 2 : 4);
            EXPECT_EQ(retained->t(), mode == 3 ? 5 : 2);
            EXPECT_EQ(retained->data()[0], 127);
        }
    }
}

TEST(SaveAdmissionTest, GlobalMapPreparationIsDiscardableAndMorrowindDoesNotUseAdmissionCallback)
{
    ESM::GlobalMap map{};
    map.mBounds = {0, 0, 0, 0}; map.mImageData = mapPng(4, 2);
    const auto records = worldRecords([&](ESM::ESMWriter& writer) {
        writer.startRecord(ESM::REC_GMAP); map.save(writer); writer.endRecord(ESM::REC_GMAP);
    });
    ESM::ESMReader reader;
    openBytes(reader, saveBytes(ESM::GameProfile::Morrowind, 1, 1, 0) + records);
    int calls = 0;
    EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Morrowind, {}, nullptr, {},
        [&](const auto&) { ++calls; }));
    EXPECT_EQ(calls, 0);
    map.mImageData = mapPng(2, 2);
    auto first = MWRender::GlobalMap::prepareRead(map);
    auto second = MWRender::GlobalMap::prepareRead(map);
    ASSERT_TRUE(first); ASSERT_TRUE(second);
    EXPECT_NE(first.get(), second.get()); // No global image cache or publication.
    first = nullptr;
    EXPECT_EQ(second->data()[0], 127);
    map.mBounds = {std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 0, 0};
    EXPECT_THROW(MWRender::GlobalMap::prepareRead(map), std::runtime_error);
}

TEST(SaveAdmissionTest, QuickkeySpellDependenciesUseIncomingDefinitionsBeforePreparation)
{
    for (std::uint32_t version = 0; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    for (const bool remapped : {false, true})
    for (const bool definitionFirst : {false, true})
    for (int mode = 0; mode != 8; ++mode)
    {
        SCOPED_TRACE(version);
        SCOPED_TRACE(remapped);
        SCOPED_TRACE(definitionFirst);
        SCOPED_TRACE(mode);
        const auto savedId = remapped ? ESM::RefId(ESM::FormId{0x901, 0})
            : ESM::RefId::stringRefId("quickkey-spell");
        const auto incomingId = remapped ? ESM::RefId(ESM::FormId{0x901, 2}) : savedId;
        const auto effectId = ESM::MagicEffect::FortifyHealth;
        MWWorld::ESMStore content;
        ESM::Spell spell{}; spell.blank(); spell.mId = incomingId;
        ESM::IndexedENAMstruct effect{}; effect.mData.mEffectID = effectId;
        if (mode == 2 || mode == 3 || mode == 4 || mode == 7) spell.mEffects.mList.push_back(effect);
        // 0 removed spell; 1 empty static spell; 2 valid static; 3 missing
        // effect; 4 outgoing-only effect; 5 outgoing-only empty spell;
        // 6 empty saved override of a valid static spell; 7 valid saved spell.
        if (mode >= 1 && mode <= 4) content.getWritable<ESM::Spell>().insertStatic(spell);
        if (mode == 5) content.getWritable<ESM::Spell>().insert(spell);
        if (mode == 6)
        {
            auto valid = spell; valid.mEffects.mList.push_back(effect);
            content.getWritable<ESM::Spell>().insertStatic(valid);
        }
        ESM::MagicEffect definition{}; definition.blank(); definition.mId = effectId;
        if (mode == 2 || mode == 6 || mode == 7) content.getWritable<ESM::MagicEffect>().insertStatic(definition);
        if (mode == 4) content.getWritable<ESM::MagicEffect>().insert(definition);
        const auto keys = worldRecords([&](ESM::ESMWriter& writer) {
            ESM::QuickKeys state{}; state.mKeys.push_back({ESM::QuickKeys::Type::Magic, savedId});
            writer.startRecord(ESM::REC_KEYS); state.save(writer); writer.endRecord(ESM::REC_KEYS);
        });
        const auto records = (mode == 6 || mode == 7) ? worldRecords([&](ESM::ESMWriter& writer) {
            spell.mId = savedId;
            writer.startRecord(ESM::REC_SPEL); spell.save(writer); writer.endRecord(ESM::REC_SPEL);
        }) : std::string{};
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(ESM::GameProfile::Oblivion,
            version ? version : ESM4::CurrentRuntimeStateVersion, 1, version ? 1 : 0)
            + (definitionFirst ? records + keys : keys + records));
        const std::map<int, int> mapping{{0, 2}};
        if (remapped) reader.setContentFileMapping(&mapping);
        const auto offset = reader.getFileOffset(); int calls = 0;
        const auto prepare = [&](const auto&, auto) { ++calls; };
        const bool valid = mode == 0 || mode == 2 || mode == 5 || mode == 7;
        if (valid)
            EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, &content, prepare));
        else
            EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, &content, prepare), std::runtime_error);
        EXPECT_EQ(calls, int(valid && version != 0));
        EXPECT_EQ(reader.getFileOffset(), offset);
        EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
        EXPECT_EQ(content.get<ESM::Spell>().getDynamicSize(), std::size_t(mode == 5));
        EXPECT_EQ(content.get<ESM::MagicEffect>().getDynamicSize(), std::size_t(mode == 4));
    }
}

TEST(SaveAdmissionTest, QuickkeySpellDependencyChecksRespectIgnoredSlotAndMorrowindAdmission)
{
    MWWorld::ESMStore content;
    ESM::Spell spell{}; spell.blank(); spell.mId = ESM::RefId::stringRefId("empty-quickkey-spell");
    content.getWritable<ESM::Spell>().insertStatic(spell);
    for (const auto type : {ESM::QuickKeys::Type::Item, ESM::QuickKeys::Type::Magic,
             ESM::QuickKeys::Type::MagicItem, ESM::QuickKeys::Type::Unassigned, ESM::QuickKeys::Type::HandToHand})
    for (const int slot : {0, 8, 9})
    for (const auto profile : {ESM::GameProfile::Oblivion, ESM::GameProfile::Morrowind})
    {
        SCOPED_TRACE(slot);
        SCOPED_TRACE(int(type));
        SCOPED_TRACE(int(profile));
        const auto records = worldRecords([&](ESM::ESMWriter& writer) {
            ESM::QuickKeys state{};
            state.mKeys.resize(slot + 1, {ESM::QuickKeys::Type::Unassigned, {}});
            state.mKeys[slot] = {type, spell.mId};
            writer.startRecord(ESM::REC_KEYS); state.save(writer); writer.endRecord(ESM::REC_KEYS);
        });
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(profile, ESM4::CurrentRuntimeStateVersion, 1,
            profile == ESM::GameProfile::Oblivion ? 1 : 0) + records);
        const auto offset = reader.getFileOffset(); int calls = 0;
        const auto prepare = [&](const auto&, auto) { ++calls; };
        const bool rejected = profile == ESM::GameProfile::Oblivion && type == ESM::QuickKeys::Type::Magic && slot < 9;
        if (rejected)
            EXPECT_THROW(MWState::admitSave(reader, profile, {}, &content, prepare), std::runtime_error);
        else
            EXPECT_NO_THROW(MWState::admitSave(reader, profile, {}, &content, prepare));
        EXPECT_EQ(calls, int(!rejected && profile == ESM::GameProfile::Oblivion));
        EXPECT_EQ(reader.getFileOffset(), offset);
    }
}

TEST(SaveAdmissionTest, QuickkeyItemRolesUseIncomingDefinitionsBeforePreparation)
{
    for (std::uint32_t version = 0; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    for (const bool remapped : {false, true})
    for (const bool definitionFirst : {false, true})
    for (const auto type : {ESM::QuickKeys::Type::Item, ESM::QuickKeys::Type::MagicItem})
    for (int mode = 0; mode != 7; ++mode)
    {
        SCOPED_TRACE(::testing::PrintToString(std::make_tuple(version, remapped, definitionFirst, int(type), mode)));
        const auto savedId = remapped ? ESM::RefId(ESM::FormId{0x901, 0})
            : ESM::RefId::stringRefId("quickkey-item");
        const auto incomingId = remapped ? ESM::RefId(ESM::FormId{0x901, 2}) : savedId;
        MWWorld::ESMStore content;
        ESM::Weapon weapon{}; weapon.blank(); weapon.mId = incomingId;
        ESM::Activator activator{}; activator.blank(); activator.mId = incomingId;
        // Removed, immutable weapon, immutable activator, outgoing-only
        // activator, saved weapon, saved activator overriding an authored one,
        // and an unknown ordinary activator discarded during installation.
        if (mode == 1) content.getWritable<ESM::Weapon>().insertStatic(weapon);
        if (mode == 2 || mode == 5) content.getWritable<ESM::Activator>().insertStatic(activator);
        content.setUp();
        if (mode == 3) { content.getWritable<ESM::Activator>().insert(activator); content.rebuildIdsIndex(); }
        const auto keys = worldRecords([&](ESM::ESMWriter& writer) {
            ESM::QuickKeys state{}; state.mKeys.push_back({type, savedId});
            writer.startRecord(ESM::REC_KEYS); state.save(writer); writer.endRecord(ESM::REC_KEYS);
        });
        const auto records = worldRecords([&](ESM::ESMWriter& writer) {
            if (mode == 4) { weapon.mId = savedId;
                writer.startRecord(ESM::REC_WEAP); weapon.save(writer); writer.endRecord(ESM::REC_WEAP); }
            if (mode == 5 || mode == 6) { activator.mId = savedId;
                writer.startRecord(ESM::REC_ACTI); activator.save(writer); writer.endRecord(ESM::REC_ACTI); }
        });
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(ESM::GameProfile::Oblivion,
            version ? version : ESM4::CurrentRuntimeStateVersion, 1, version ? 1 : 0)
            + (definitionFirst ? records + keys : keys + records));
        const std::map<int, int> mapping{{0, 2}};
        if (remapped) reader.setContentFileMapping(&mapping);
        const auto offset = reader.getFileOffset(); int calls = 0;
        const auto prepare = [&](const auto&, auto) { ++calls; };
        const bool valid = mode != 2 && mode != 5;
        if (valid)
            EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, &content, prepare));
        else
            EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, &content, prepare), std::runtime_error);
        EXPECT_EQ(calls, int(valid && version != 0));
        EXPECT_EQ(reader.getFileOffset(), offset);
        EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
        EXPECT_EQ(content.get<ESM::Activator>().getDynamicSize(), std::size_t(mode == 3));
    }
}

TEST(SaveAdmissionTest, QuickkeyItemChecksRespectAllProjectedFamiliesAndCompatibilitySkips)
{
    const auto check = [&]<class Shared, class Native>() {
        for (const bool native : {false, true})
        for (const bool projection : {false, true})
        for (const int slot : {0, 8, 9})
        for (const auto profile : {ESM::GameProfile::Oblivion, ESM::GameProfile::Morrowind})
        {
            SCOPED_TRACE(::testing::PrintToString(std::make_tuple(Shared::sRecordId, Native::sRecordId, native, projection, slot, int(profile))));
            MWWorld::ESMStore content;
            const auto id = ESM::RefId(ESM::FormId{0x901, 0});
            Shared shared{}; shared.blank(); shared.mId = id;
            Native record{}; record.mId = {0x901, 0};
            if (native) content.getWritable<Native>().insertStatic(record);
            if (projection) content.getWritable<Shared>().insertStatic(shared);
            content.setUp();
            const auto records = worldRecords([&](ESM::ESMWriter& writer) {
                ESM::QuickKeys keys{}; keys.mKeys.resize(slot + 1, {ESM::QuickKeys::Type::Unassigned, {}});
                keys.mKeys[slot] = {ESM::QuickKeys::Type::Item, id};
                writer.startRecord(ESM::REC_KEYS); keys.save(writer); writer.endRecord(ESM::REC_KEYS);
            });
            ESM::ESMReader reader;
            openBytes(reader, saveBytes(profile, ESM4::CurrentRuntimeStateVersion, 1,
                profile == ESM::GameProfile::Oblivion ? 1 : 0) + records);
            const auto offset = reader.getFileOffset(); int calls = 0;
            const auto prepare = [&](const auto&, auto) { ++calls; };
            const bool rejected = native && !projection && content.findStatic(id) != 0
                && slot < 9 && profile == ESM::GameProfile::Oblivion;
            if (rejected)
                EXPECT_THROW(MWState::admitSave(reader, profile, {}, &content, prepare), std::runtime_error);
            else
                EXPECT_NO_THROW(MWState::admitSave(reader, profile, {}, &content, prepare));
            EXPECT_EQ(calls, int(!rejected && profile == ESM::GameProfile::Oblivion));
            EXPECT_EQ(reader.getFileOffset(), offset);
        }
    };
    check.template operator()<ESM::Potion, ESM4::Potion>();
    check.template operator()<ESM::Apparatus, ESM4::Apparatus>();
    check.template operator()<ESM::Armor, ESM4::Armor>();
    check.template operator()<ESM::Book, ESM4::Book>();
    check.template operator()<ESM::Clothing, ESM4::Clothing>();
    check.template operator()<ESM::Ingredient, ESM4::Ingredient>();
    check.template operator()<ESM::Light, ESM4::Light>();
    check.template operator()<ESM::Miscellaneous, ESM4::MiscItem>();
    check.template operator()<ESM::Miscellaneous, ESM4::Key>();
    check.template operator()<ESM::Miscellaneous, ESM4::SigilStone>();
    check.template operator()<ESM::Miscellaneous, ESM4::SoulGem>();
    check.template operator()<ESM::Weapon, ESM4::Weapon>();
    check.template operator()<ESM::Weapon, ESM4::Ammunition>();
    check.template operator()<ESM::Lockpick, ESM4::MiscItem>();
    check.template operator()<ESM::Repair, ESM4::MiscItem>();
}

TEST(SaveAdmissionTest, QuickkeyNonItemsRespectGeneratedIdentityIgnoredSlotAndProfiles)
{
    for (const bool generated : {false, true})
    for (const auto type : {ESM::QuickKeys::Type::Item, ESM::QuickKeys::Type::MagicItem,
             ESM::QuickKeys::Type::Magic, ESM::QuickKeys::Type::Unassigned, ESM::QuickKeys::Type::HandToHand})
    for (const int slot : {0, 8, 9})
    for (const auto profile : {ESM::GameProfile::Oblivion, ESM::GameProfile::Morrowind})
    {
        SCOPED_TRACE(::testing::PrintToString(std::make_tuple(generated, int(type), slot, int(profile))));
        MWWorld::ESMStore content;
        ESM::Activator activator{}; activator.blank();
        activator.mId = generated ? ESM::RefId::generated(17) : ESM::RefId::stringRefId("authored-activator");
        if (!generated) content.getWritable<ESM::Activator>().insertStatic(activator);
        content.setUp();
        const auto records = worldRecords([&](ESM::ESMWriter& writer) {
            ESM::QuickKeys keys{}; keys.mKeys.resize(slot + 1, {ESM::QuickKeys::Type::Unassigned, {}});
            keys.mKeys[slot] = {type, activator.mId};
            writer.startRecord(ESM::REC_KEYS); keys.save(writer); writer.endRecord(ESM::REC_KEYS);
            if (generated)
            {
                writer.startRecord(ESM::REC_DYNA); writer.writeHNT("COUN", std::uint64_t{18}); writer.endRecord(ESM::REC_DYNA);
                writer.startRecord(ESM::REC_ACTI); activator.save(writer); writer.endRecord(ESM::REC_ACTI);
            }
        });
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(profile, ESM4::CurrentRuntimeStateVersion, 1,
            profile == ESM::GameProfile::Oblivion ? 1 : 0) + records);
        const auto offset = reader.getFileOffset(); int calls = 0;
        const auto prepare = [&](const auto&, auto) { ++calls; };
        const bool rejected = slot < 9 && profile == ESM::GameProfile::Oblivion
            && (type == ESM::QuickKeys::Type::Item || type == ESM::QuickKeys::Type::MagicItem);
        if (rejected)
            EXPECT_THROW(MWState::admitSave(reader, profile, {}, &content, prepare), std::runtime_error);
        else
            EXPECT_NO_THROW(MWState::admitSave(reader, profile, {}, &content, prepare));
        EXPECT_EQ(calls, int(!rejected && profile == ESM::GameProfile::Oblivion));
        EXPECT_EQ(reader.getFileOffset(), offset);
    }
}

TEST(SaveAdmissionTest, EverySupportedNativeVersionIsValidatedAndReaderIsRewound)
{
    for (std::uint32_t version = 1; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    {
        SCOPED_TRACE(version);
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(ESM::GameProfile::Oblivion, version));
        const auto start = reader.getFileOffset();
        int calls = 0;
        const auto profile = MWState::admitSave(reader, ESM::GameProfile::Oblivion, [&](const auto& state) {
            ++calls;
            EXPECT_EQ(state.mVersion, version);
            state.validateContent({{"headless.esm", "sha256:" + std::string(64, 'a')}});
        });
        EXPECT_EQ(calls, 1);
        EXPECT_EQ(profile.mPlayerName, "Admission Player");
        EXPECT_EQ(reader.getFileOffset(), start);
        EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
    }
}

TEST(SaveAdmissionTest, LegacyNativeAndMorrowindWithoutT4stRemainDeliberatelyReadable)
{
    for (const auto profile : {ESM::GameProfile::Oblivion, ESM::GameProfile::Morrowind})
    {
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(profile, 3, 1, 0));
        const auto admitted = MWState::admitSave(reader, profile,
            [](const auto&) { ADD_FAILURE() << "Legacy save invented native state"; });
        EXPECT_EQ(admitted.mGameProfile, profile);
        EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
    }
}

TEST(SaveAdmissionTest, MissingDuplicateForeignAndTrailingMetadataRejectBeforeNativeValidation)
{
    const auto version = ESM4::CurrentRuntimeStateVersion;
    const std::vector<std::string> rejected{
        saveBytes(ESM::GameProfile::Oblivion, version, 0),
        saveBytes(ESM::GameProfile::Oblivion, version, 2),
        saveBytes(ESM::GameProfile::Oblivion, version, 1, 2),
        saveBytes(ESM::GameProfile::Morrowind),
        saveBytes(ESM::GameProfile::Oblivion, version, 1, 1, true),
        saveBytes(ESM::GameProfile::Oblivion, version, 1, 1, false, true),
        saveBytes(ESM::GameProfile::Oblivion, version + 1, 1, 0)};
    for (std::size_t i = 0; i < rejected.size(); ++i)
    {
        SCOPED_TRACE(i);
        ESM::ESMReader reader;
        openBytes(reader, rejected[i]);
        const auto start = reader.getFileOffset();
        int calls = 0;
        EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion,
            [&](const auto&) { ++calls; }), std::runtime_error);
        EXPECT_EQ(calls, 0);
        EXPECT_EQ(reader.getFileOffset(), start);
    }
    ESM::ESMReader reader;
    openBytes(reader, saveBytes(ESM::GameProfile::Morrowind));
    EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Morrowind, [](const auto&) {}), std::runtime_error);
}

TEST(SaveAdmissionTest, EveryMidRecordTruncationRejectsWhileCompleteLegacyPrefixesRemainReadable)
{
    const auto bytes = saveBytes();
    ESM::ESMReader original;
    openBytes(original, bytes);
    const auto start = original.getFileOffset();
    std::set<std::size_t> boundaries;
    while (original.hasMoreRecs())
    {
        original.getRecName();
        original.getRecHeader();
        original.skipRecord();
        boundaries.insert(original.getFileOffset());
    }
    for (std::size_t length = start; length < bytes.size(); ++length)
    {
        SCOPED_TRACE(length);
        ESM::ESMReader reader;
        openBytes(reader, bytes.substr(0, length));
        int calls = 0;
        if (boundaries.contains(length))
        {
            // Admission deliberately allows legacy saves with no T4ST and
            // does not assert that later manager-specific records are present.
            EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion,
                [&](const auto&) { ++calls; }));
        }
        else
        {
            EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion,
                [&](const auto&) { ++calls; }), std::runtime_error);
            EXPECT_EQ(calls, 0);
        }
        EXPECT_EQ(reader.getFileOffset(), start);
    }
}

TEST(SaveAdmissionTest, MalformedIgnoredRecordsAndNativeVersionMismatchReject)
{
    const auto bytes = saveBytes();
    std::vector<std::string> rejected(6, bytes);
    const auto unknown = bytes.rfind("JUNK");
    ASSERT_NE(unknown, std::string::npos);
    replaceUint(rejected[0], unknown + 4, 7); // Incomplete subrecord header.
    replaceUint(rejected[1], unknown + 20, 5); // Payload overruns outer record.
    rejected[2] += 'x'; // Incomplete record header after otherwise valid data.
    replaceUint(rejected[3], bytes.find("VERS") + 8, ESM4::CurrentRuntimeStateVersion - 1);
    replaceUint(rejected[4], bytes.find("T4VR") + 8, ESM4::CurrentRuntimeStateVersion - 1);
    rejected[5][bytes.find("DATA") + 8] = '!'; // Invalid native payload magic.
    for (std::size_t i = 0; i < rejected.size(); ++i)
    {
        SCOPED_TRACE(i);
        ESM::ESMReader reader;
        openBytes(reader, rejected[i]);
        const auto start = reader.getFileOffset();
        int calls = 0;
        EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion,
            [&](const auto&) { ++calls; }), std::runtime_error);
        EXPECT_EQ(calls, 0);
        EXPECT_EQ(reader.getFileOffset(), start);
    }
}

TEST(SaveAdmissionTest, RejectedContentIdentityRewindsAndCorrectedRetryDoesNotRestoreRecords)
{
    for (const bool missing : {false, true})
    {
        ESM::ESMReader reader;
        openBytes(reader, saveBytes());
        const auto start = reader.getFileOffset();
        std::vector<ESM4::RuntimeContentIdentity> content;
        if (!missing)
            content.push_back({"headless.esm", "sha256:" + std::string(64, 'b')});
        EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion,
            [&](const auto& state) { state.validateContent(content); }), std::runtime_error);
        EXPECT_EQ(reader.getFileOffset(), start);
        content = {{"HEADLESS.ESM", "sha256:" + std::string(64, 'a')}};
        EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion,
            [&](const auto& state) { state.validateContent(content); }));
        EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
    }
}

TEST(SaveAdmissionTest, MalformedProfileAtEofDoesNotPoisonReaderContextRecovery)
{
    auto bytes = saveBytes(ESM::GameProfile::Oblivion, ESM4::CurrentRuntimeStateVersion, 0, 0);
    const auto record = bytes.rfind("JUNK");
    ASSERT_NE(record, std::string::npos);
    bytes.replace(record, 4, "SAVE");
    replaceUint(bytes, record + 4, 0);
    bytes.resize(record + 16);
    ESM::ESMReader reader;
    openBytes(reader, bytes);
    const auto start = reader.getFileOffset();
    EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, [](const auto&) {}), std::runtime_error);
    EXPECT_EQ(reader.getFileOffset(), start);
    EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
    EXPECT_NO_THROW(reader.getRecHeader());
}

TEST(SaveAdmissionTest, ProfileNameUsesFirstNulTerminationLikeTheSharedReader)
{
    auto bytes = saveBytes();
    const auto profile = bytes.find("SAVE");
    const auto tag = bytes.find("GPRO");
    ASSERT_NE(profile, std::string::npos);
    ASSERT_NE(tag, std::string::npos);
    std::uint32_t profileSize = 0, tagSize = 0;
    std::memcpy(&profileSize, bytes.data() + profile + 4, sizeof(profileSize));
    std::memcpy(&tagSize, bytes.data() + tag + 4, sizeof(tagSize));
    const std::string padded("oblivion\0padding", 16);
    ASSERT_LE(tagSize, padded.size());
    bytes.replace(tag + 8, tagSize, padded);
    replaceUint(bytes, tag + 4, padded.size());
    replaceUint(bytes, profile + 4, profileSize + padded.size() - tagSize);
    ESM::ESMReader reader;
    openBytes(reader, bytes);
    const auto admitted = MWState::admitSave(reader, ESM::GameProfile::Oblivion, [](const auto&) {});
    EXPECT_EQ(admitted.mGameProfile, ESM::GameProfile::Oblivion);
    EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
}

TEST(SaveAdmissionTest, MalformedSharedDynamicRecordsRejectBeforeNativeValidation)
{
    for (const auto type : {ESM::REC_ALCH, ESM::REC_ARMO, ESM::REC_BOOK, ESM::REC_CLAS,
             ESM::REC_CLOT, ESM::REC_ENCH, ESM::REC_SPEL, ESM::REC_WEAP, ESM::REC_NPC_,
             ESM::REC_CREA, ESM::REC_CONT, ESM::REC_MISC, ESM::REC_ACTI, ESM::REC_LEVI,
             ESM::REC_LEVC, ESM::REC_LIGH, ESM::REC_STAT, ESM::REC_DOOR, ESM::REC_PROB,
             ESM::REC_INGR, ESM::REC_DYNA, ESM::REC_GLOB})
    {
        SCOPED_TRACE(type);
        auto bytes = saveBytes();
        const auto record = bytes.rfind("JUNK");
        ASSERT_NE(record, std::string::npos);
        replaceUint(bytes, record, type);
        ESM::ESMReader reader;
        openBytes(reader, bytes);
        const auto start = reader.getFileOffset();
        int nativeCalls = 0;
        EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion,
            [&](const auto&) { ++nativeCalls; }), std::exception);
        EXPECT_EQ(nativeCalls, 0);
        EXPECT_EQ(reader.getFileOffset(), start);
        EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
    }
}

TEST(SaveAdmissionTest, ValidSharedClassGlobalAndDynamicCounterRemainReadable)
{
    ESM::ESMWriter writer;
    writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
    std::stringstream stream;
    writer.save(stream);
    ESM::Class custom{};
    custom.blank();
    custom.mId = ESM::RefId::stringRefId("custom-class");
    writer.startRecord(ESM::REC_CLAS);
    custom.save(writer);
    writer.endRecord(ESM::REC_CLAS);
    ESM::Global global{};
    global.mId = ESM::RefId::stringRefId("gamehour");
    global.mValue.setType(ESM::VT_Short);
    global.mValue.setInteger(5);
    writer.startRecord(ESM::REC_GLOB);
    global.save(writer);
    writer.endRecord(ESM::REC_GLOB);
    writer.startRecord(ESM::REC_DYNA);
    writer.writeHNT("COUN", std::uint64_t{17});
    writer.endRecord(ESM::REC_DYNA);
    ESM::ESMReader shared;
    openBytes(shared, stream.str());
    const auto records = stream.str().substr(shared.getFileOffset());
    ESM::ESMReader reader;
    openBytes(reader, saveBytes() + records);
    const auto start = reader.getFileOffset();
    int calls = 0;
    EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion,
        [&](const auto&) { ++calls; }));
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(reader.getFileOffset(), start);
    EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
}

TEST(SaveAdmissionTest, IncomingPlayerClassCannotBorrowAnOutgoingDynamicDefinition)
{
    MWWorld::ESMStore content;
    ESM::Class fallback{};
    fallback.blank();
    fallback.mId = ESM::RefId::stringRefId("fallback-class");
    content.getWritable<ESM::Class>().insertStatic(fallback);
    ESM::NPC player{};
    player.blank();
    player.mId = ESM::RefId::stringRefId("Player");
    player.mClass = fallback.mId;
    content.getWritable<ESM::NPC>().insertStatic(player);
    ESM::Class custom = fallback;
    custom.mId = ESM::RefId::generated(17);
    content.getWritable<ESM::Class>().insert(custom);
    auto outgoing = player;
    outgoing.mClass = custom.mId;
    content.getWritable<ESM::NPC>().insert(outgoing);
    auto state = nativeState();
    state.mPlayer.mClass = ESM::FormKey::dynamic("player-class", 1);
    const auto bytes = saveBytes(ESM::GameProfile::Oblivion, state.mVersion, 1, 1, false, false, &state);
    for (int attempt = 0; attempt != 4; ++attempt)
    {
        SCOPED_TRACE(attempt);
        ESM::ESMReader reader;
        openBytes(reader, bytes + sharedRecords(attempt == 0 ? nullptr : &outgoing,
            attempt == 2 ? &custom : nullptr, nullptr));
        const auto start = reader.getFileOffset();
        int calls = 0;
        const auto validate = [&](const auto&) { ++calls; };
        if (attempt == 1 || attempt == 3)
        {
            EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, validate, &content), std::runtime_error);
            EXPECT_EQ(calls, 0);
        }
        else
        {
            EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, validate, &content));
            EXPECT_EQ(calls, 1);
        }
        EXPECT_EQ(reader.getFileOffset(), start);
        EXPECT_EQ(reader.getRecName(), ESM::REC_SAVE);
        EXPECT_EQ(content.get<ESM::NPC>().search(player.mId)->mClass, custom.mId);
        EXPECT_EQ(content.get<ESM::NPC>().searchStatic(player.mId)->mClass, fallback.mId);
    }
}

TEST(SaveAdmissionTest, NativeGlobalConversionsUseIncomingTypesAndImmutableFallbacks)
{
    MWWorld::ESMStore content;
    const auto key = ESM::FormKey::content("headless.esm", 4);
    ESM4::GlobalVariable definition{};
    definition.mId = {4, 0};
    definition.mEditorId = "GameDaysPassed";
    content.getWritable<ESM4::GlobalVariable>().insertStatic(definition, key);
    ESM::Global global{};
    global.mId = ESM::RefId::stringRefId("dayspassed");
    global.mValue.setType(ESM::VT_Long);
    global.mValue.setInteger(5);
    content.getWritable<ESM::Global>().insertStatic(global);
    auto outgoing = global;
    outgoing.mValue.setType(ESM::VT_Float);
    content.getWritable<ESM::Global>().insert(outgoing);
    for (const auto type : {ESM::VT_Short, ESM::VT_Long, ESM::VT_Float})
        for (const double value : {1e30, 1e300, -0x1p63, 0x1p63, 3.75})
            for (const bool savedType : {false, true})
            {
                SCOPED_TRACE(type);
                SCOPED_TRACE(value);
                SCOPED_TRACE(savedType);
                auto incoming = global;
                incoming.mValue.setType(type);
                auto state = nativeState();
                state.mGlobals[key] = value;
                ESM::ESMReader reader;
                openBytes(reader, saveBytes(ESM::GameProfile::Oblivion, state.mVersion, 1, 1, false, false, &state)
                    + sharedRecords(nullptr, nullptr, savedType ? &incoming : nullptr));
                const auto start = reader.getFileOffset();
                int calls = 0;
                const auto validate = [&](const auto&) { ++calls; };
                const bool floating = savedType && type == ESM::VT_Float;
                const bool valid = floating ? value >= -std::numeric_limits<float>::max()
                        && value <= std::numeric_limits<float>::max()
                    : value >= -0x1p63 && value < 0x1p63;
                if (valid)
                {
                    EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, validate, &content));
                    EXPECT_EQ(calls, 1);
                }
                else
                {
                    EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, validate, &content), std::runtime_error);
                    EXPECT_EQ(calls, 0);
                }
                EXPECT_EQ(reader.getFileOffset(), start);
                EXPECT_EQ(content.get<ESM::Global>().search(global.mId)->mValue.getType(), ESM::VT_Float);
                EXPECT_EQ(content.get<ESM::Global>().searchStatic(global.mId)->mValue.getInteger(), 5);
            }
}

TEST(SaveAdmissionTest, DeletedSharedAuthorityRecordsRejectBeforeNativeValidation)
{
    ESM::NPC player{};
    player.blank();
    player.mId = ESM::RefId::stringRefId("Player");
    ESM::Class characterClass{};
    characterClass.blank();
    characterClass.mId = ESM::RefId::generated(17);
    ESM::Global global{};
    global.mId = ESM::RefId::stringRefId("gamehour");
    for (int record = 0; record != 3; ++record)
    {
        ESM::ESMReader reader;
        openBytes(reader, saveBytes() + sharedRecords(record == 0 ? &player : nullptr,
            record == 1 ? &characterClass : nullptr, record == 2 ? &global : nullptr, true));
        const auto start = reader.getFileOffset();
        int calls = 0;
        EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion,
            [&](const auto&) { ++calls; }), std::runtime_error);
        EXPECT_EQ(calls, 0);
        EXPECT_EQ(reader.getFileOffset(), start);
    }
}

TEST(SaveAdmissionTest, SharedCellDecodingRejectsBeforeNativeValidationAndPreservesContent)
{
    MWWorld::ESMStore content;
    ESM4::Weapon weapon{};
    weapon.mId = {0x940, 1};
    content.getWritable<ESM4::Weapon>().insertStatic(weapon);
    content.setUp();
    ASSERT_EQ(content.findStatic(ESM::RefId(weapon.mId)), ESM::REC_WEAP4);
    const std::map<int, int> mapping{{0, 1}};
    for (int fault = 0; fault != 9; ++fault)
    {
        SCOPED_TRACE(fault);
        const auto records = worldRecords([&](ESM::ESMWriter& writer) {
            for (int copy = 0; copy != (fault == 6 ? 2 : 1); ++copy)
            {
                writer.startRecord(ESM::REC_CSTA);
                writer.writeCellId(ESM::RefId(ESM::FormId{1, 0}));
                ESM::CellState cell{};
                cell.mIsInterior = true;
                cell.mWaterLevel = fault == 1 ? std::numeric_limits<float>::quiet_NaN() : 2.f;
                cell.mHasFogOfWar = fault == 8;
                cell.save(writer);
                if (fault == 8)
                    writer.writeHNT("FTEX", std::uint32_t{1});
                if (fault == 3)
                    writer.writeHNT("OBJE", std::uint16_t{0});
                else
                    writer.writeHNT("OBJE", std::uint32_t(ESM::REC_NPC_)); // unused, winning base type wins
                ESM::ObjectState object;
                object.blank();
                object.mRef.mRefID = ESM::RefId(ESM::FormId{0x940, 0});
                object.mRef.mRefNum = {0x942, 0};
                object.mPosition.pos[0] = fault == 2 ? std::numeric_limits<float>::infinity() : 7.f;
                object.mHasCustomState = false;
                object.save(writer);
                if (fault == 4)
                    writer.writeHNT("LUAS", std::uint8_t{1});
                if (fault == 5)
                    writer.writeFormId(ESM::FormId{0x942, 0}, true, "MVRF");
                if (fault == 7)
                    writer.writeHNT("EXTR", std::uint32_t{1});
                writer.endRecord(ESM::REC_CSTA);
            }
        });
        ESM::ESMReader reader;
        openBytes(reader, saveBytes() + records);
        reader.setContentFileMapping(&mapping);
        const auto start = reader.getFileOffset();
        int calls = 0;
        const auto validate = [&](const auto&) { ++calls; };
        if (!fault)
        {
            EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, validate, &content));
            EXPECT_EQ(calls, 1);
        }
        else
        {
            EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, validate, &content), std::exception);
            EXPECT_EQ(calls, 0);
        }
        EXPECT_EQ(reader.getFileOffset(), start);
        EXPECT_EQ(content.get<ESM4::Weapon>().searchStatic(ESM::RefId(weapon.mId)),
            content.get<ESM4::Weapon>().search(ESM::RefId(weapon.mId)));
        EXPECT_EQ(content.get<ESM4::Weapon>().getDynamicSize(), 0u);
    }
}

TEST(SaveAdmissionTest, SharedPlayerDecodingRejectsMalformedAndDuplicateRecords)
{
    ESM::Player player{};
    player.mObject.blank();
    player.mObject.mRef.mRefID = ESM::RefId::stringRefId("Player");
    player.mCellId = ESM::RefId(ESM::FormId{1, 0});
    for (int fault = 0; fault != 4; ++fault)
    {
        auto records = worldRecords([&](ESM::ESMWriter& writer) {
            writer.startRecord(ESM::REC_PLAY);
            player.save(writer);
            if (fault == 3) writer.writeHNT("EXTR", std::uint32_t{1});
            writer.endRecord(ESM::REC_PLAY);
        });
        if (fault == 1)
        {
            const auto sign = records.find("SIGN");
            ASSERT_NE(sign, std::string::npos);
            records.replace(sign, 4, "TEST");
        }
        if (fault == 2) records += records;
        ESM::ESMReader reader;
        openBytes(reader, saveBytes() + records);
        const auto start = reader.getFileOffset();
        int calls = 0;
        const auto validate = [&](const auto&) { ++calls; };
        if (!fault)
        {
            EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, validate));
            EXPECT_EQ(calls, 1);
        }
        else
        {
            EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, validate), std::exception);
            EXPECT_EQ(calls, 0);
        }
        EXPECT_EQ(reader.getFileOffset(), start);
    }
}

TEST(SaveAdmissionTest, DetachedReferenceDecoderUsesAllCellStoreStateSpecializations)
{
    std::vector<std::pair<std::uint32_t, std::unique_ptr<ESM::ObjectState>>> states;
    states.emplace_back(ESM::REC_NPC_, std::make_unique<ESM::NpcState>());
    states.emplace_back(ESM::REC_CREA, std::make_unique<ESM::CreatureState>());
    states.emplace_back(ESM::REC_CONT, std::make_unique<ESM::ContainerState>());
    states.emplace_back(ESM::REC_DOOR, std::make_unique<ESM::DoorState>());
    states.emplace_back(ESM::REC_LEVC, std::make_unique<ESM::CreatureLevListState>());
    states.emplace_back(ESM::REC_NPC_4, std::make_unique<ESM::ObjectState>());
    states.emplace_back(ESM::REC_WEAP4, std::make_unique<ESM::ObjectState>());
    for (auto& [type, state] : states)
    {
        SCOPED_TRACE(type);
        state->blank();
        state->mRef.mRefID = ESM::RefId::stringRefId("saved-reference");
        state->mRef.mRefNum = {17, -1};
        state->mPosition.pos[0] = 7.5;
        const auto records = worldRecords([&](ESM::ESMWriter& writer) {
            writer.startRecord(ESM::REC_CSTA);
            state->save(writer);
            writer.endRecord(ESM::REC_CSTA);
        });
        ESM::ESMReader reader;
        openBytes(reader, saveBytes() + records);
        while (reader.hasMoreRecs())
        {
            const auto record = reader.getRecName();
            reader.getRecHeader();
            if (record != ESM::REC_CSTA) { reader.skipRecord(); continue; }
            ESM::CellRef reference;
            reference.loadId(reader, true);
            const auto decoded = MWWorld::readSavedReferenceState(reader, reference, type);
            EXPECT_EQ(typeid(*decoded), typeid(*state));
            EXPECT_EQ(decoded->mPosition, state->mPosition);
            EXPECT_EQ(decoded->mRef.mRefID, state->mRef.mRefID);
            EXPECT_FALSE(reader.hasMoreSubs());
        }
    }
}

TEST(SaveAdmissionTest, LuaFailuresAreRejectedBeforeNativeValidationAndRewind)
{
    for (int failure = 0; failure != 6; ++failure)
    {
        SCOPED_TRACE(failure);
        const auto records = worldRecords([&](ESM::ESMWriter& writer) {
            for (int i = 0; i != (failure == 3 ? 2 : 1); ++i)
            {
                writer.startRecord(ESM::REC_LUAM);
                writer.writeHNT("LUAW", failure == 0 ? std::numeric_limits<double>::infinity() : 1.25);
                writer.writeFormId(ESM::RefNum{1, failure == 1 ? 0 : -1}, true);
                if (failure == 2)
                {
                    writer.writeHNString("LUAE", "bad-event");
                    writer.writeFormId(ESM::RefNum{}, true);
                    ESM::saveLuaBinaryData(writer, "invalid serialized payload");
                }
                ESM4::LocalLuaScripts scripts;
                if (failure == 4 || failure == 5)
                    scripts.emplace(ESM::FormKey::dynamic("native-reference", 19), ESM::LuaScripts{});
                ESM4::saveLocalLuaScripts(writer, scripts);
                writer.endRecord(ESM::REC_LUAM);
            }
        });
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(ESM::GameProfile::Oblivion, ESM4::CurrentRuntimeStateVersion,
            1, failure == 5 ? 0 : 1) + records);
        LuaUtil::ScriptsConfiguration previous;
        reader.mScriptsConfiguration = &previous;
        const auto start = reader.getFileOffset();
        bool validated = false;
        EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion,
            [&](const auto&) { validated = true; }), std::exception);
        EXPECT_FALSE(validated);
        EXPECT_EQ(reader.getFileOffset(), start);
        EXPECT_EQ(reader.mScriptsConfiguration, &previous);
    }
}

TEST(SaveAdmissionTest, IsolatedLuaDecodeChecksPayloadsTimersAndIdsWithoutCallbacks)
{
    sol::state lua;
    const auto valid = LuaUtil::serialize(sol::make_object(lua, 7.25));
    ESM::LuaScriptsCfg configuration;
    ESM::LuaScriptCfg script{};
    script.mScriptPath = VFS::Path::Normalized("test.lua");
    configuration.mScripts.push_back(script);
    for (int failure = -1; failure != 5; ++failure)
    {
        SCOPED_TRACE(failure);
        ESM::LuaScripts globals;
        globals.mScripts.push_back({failure == 3 ? 1 : 0, failure == 0 ? "bad" : valid,
            {{ESM::LuaTimer::Type::SIMULATION_TIME,
                failure == 2 ? std::numeric_limits<double>::quiet_NaN() : -2,
                "callback-must-not-run", failure == 1 ? "bad" : valid}}});
        if (failure == 4)
            globals.mScripts.push_back(globals.mScripts.front());
        ESM::ESMWriter writer;
        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
        std::stringstream stream;
        writer.save(stream);
        writer.startRecord(ESM::REC_LUAM);
        writer.writeHNT("LUAW", 1.25);
        writer.writeFormId(ESM::RefNum{1, -1}, true);
        writer.writeHNString("LUAP", "test.lua");
        globals.save(writer);
        ESM4::saveLocalLuaScripts(writer, {});
        writer.endRecord(ESM::REC_LUAM);
        ESM::ESMReader reader;
        openBytes(reader, stream.str());
        reader.getRecName();
        reader.getRecHeader();
        LuaUtil::ScriptsConfiguration previous;
        reader.mScriptsConfiguration = &previous;
        if (failure == -1)
            EXPECT_TRUE(MWLua::validateSavedLuaRecord(reader, configuration).empty());
        else
            EXPECT_THROW(MWLua::validateSavedLuaRecord(reader, configuration), std::exception);
        EXPECT_EQ(reader.mScriptsConfiguration, &previous);
    }
}

TEST(SaveAdmissionTest, IsolatedLuaDecodeAcceptsObjectListsAndDeliberatelyRemovedScripts)
{
    sol::state lua;
    auto serializer = MWLua::createUserdataSerializer(false);
    auto ids = std::make_shared<std::vector<ESM::RefNum>>();
    ids->push_back({12, 0});
    ids->push_back({15, -1});
    auto table = lua.create_table();
    table["object"] = MWLua::GObject(ESM::RefNum{12, 0});
    table["objects"] = MWLua::GObjectList{ids};
    ESM::LuaScripts data;
    data.mScripts.push_back({0, LuaUtil::serialize(table, serializer.get()), {}});
    data.mScripts.push_back({1, "malformed data of a removed script", {}});
    ESM::LuaScriptsCfg configuration;
    ESM::LuaScriptCfg script{};
    script.mScriptPath = VFS::Path::Normalized("kept.lua");
    configuration.mScripts.push_back(script);
    ESM::ESMWriter writer;
    writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);
    std::stringstream stream;
    writer.save(stream);
    writer.startRecord(ESM::REC_LUAM);
    writer.writeHNT("LUAW", 0.0);
    writer.writeFormId(ESM::RefNum{}, true);
    writer.writeHNString("LUAP", "kept.lua");
    writer.writeHNString("LUAP", "removed.lua");
    data.save(writer);
    writer.writeHNString("LUAE", "saved-event");
    writer.writeFormId(ESM::RefNum{15, -1}, true);
    ESM::saveLuaBinaryData(writer, data.mScripts.front().mData);
    const auto key = ESM::FormKey::dynamic("native-reference", 19);
    ESM4::saveLocalLuaScripts(writer, {{key, data}});
    writer.endRecord(ESM::REC_LUAM);
    ESM::ESMReader reader;
    openBytes(reader, stream.str());
    reader.getRecName();
    reader.getRecHeader();
    const auto decoded = MWLua::validateSavedLuaRecord(reader, configuration);
    ASSERT_EQ(decoded.size(), 1);
    EXPECT_EQ(decoded.at(key).mScripts.front().mData, data.mScripts.front().mData);
    EXPECT_FALSE(reader.hasMoreSubs());
    EXPECT_EQ(reader.mScriptsConfiguration, nullptr);
}

namespace
{
    void installAdmissionLuaConfiguration(MWWorld::ESMStore& content)
    {
        ESM::LuaScriptsCfg configuration;
        ESM::LuaScriptCfg script{};
        script.mScriptPath = VFS::Path::Normalized("kept.lua");
        script.mFlags = ESM::LuaScriptCfg::sCustom;
        configuration.mScripts.push_back(script);
        ESM::ESMWriter writer;
        std::stringstream stream;
        writer.save(stream);
        writer.startRecord(ESM::REC_LUAL);
        configuration.save(writer);
        writer.endRecord(ESM::REC_LUAL);
        ESM::ESMReader reader;
        openBytes(reader, stream.str());
        ESM::Dialogue* dialogue = nullptr;
        content.load(reader, nullptr, dialogue);
    }

    void writeAdmissionLuaMapping(ESM::ESMWriter& writer)
    {
        writer.startRecord(ESM::REC_LUAM);
        writer.writeHNT("LUAW", 0.0);
        writer.writeFormId(ESM::RefNum{}, true);
        writer.writeHNString("LUAP", "removed.lua");
        writer.writeHNString("LUAP", "kept.lua");
        ESM4::saveLocalLuaScripts(writer, {});
        writer.endRecord(ESM::REC_LUAM);
    }
}

TEST(SaveAdmissionTest, SharedPlayerAndInventoryLuaUseIncomingMappingInEitherRecordOrder)
{
    MWWorld::ESMStore content;
    installAdmissionLuaConfiguration(content);
    content.setUp();
    sol::state lua;
    const auto valid = LuaUtil::serialize(sol::make_object(lua, 42));
    for (bool luaFirst : {false, true})
    for (bool inInventory : {false, true})
    for (int fault = 0; fault != 6; ++fault)
    {
        SCOPED_TRACE(luaFirst);
        SCOPED_TRACE(inInventory);
        SCOPED_TRACE(fault);
        ESM::Player player{};
        player.mObject.blank();
        player.mObject.mRef.mRefID = ESM::RefId::stringRefId("Player");
        player.mCellId = ESM::RefId(ESM::FormId{1, 0});
        ESM::ObjectState* owner = &player.mObject;
        if (inInventory)
        {
            auto& item = player.mObject.mInventory.mItems.emplace_back();
            item.blank();
            item.mRef.mRefID = ESM::RefId::stringRefId("item");
            owner = &item;
        }
        owner->mLuaScripts.mScripts.push_back({fault == 3 ? 2 : (fault == 5 ? 0 : 1),
            fault == 1 || fault == 5 ? "bad payload" : valid,
            {{ESM::LuaTimer::Type::GAME_TIME, fault == 2 ? std::numeric_limits<double>::infinity() : -1,
                "must-not-run", valid}}});
        if (fault == 4)
            owner->mLuaScripts.mScripts.push_back(owner->mLuaScripts.mScripts.front());
        const auto records = worldRecords([&](ESM::ESMWriter& writer) {
            if (luaFirst) writeAdmissionLuaMapping(writer);
            writer.startRecord(ESM::REC_PLAY);
            player.save(writer);
            writer.endRecord(ESM::REC_PLAY);
            if (!luaFirst) writeAdmissionLuaMapping(writer);
        });
        ESM::ESMReader reader;
        openBytes(reader, saveBytes() + records);
        LuaUtil::ScriptsConfiguration outgoing;
        reader.mScriptsConfiguration = &outgoing;
        const auto offset = reader.getFileOffset();
        bool validated = false;
        const auto admit = [&] { MWState::admitSave(reader, ESM::GameProfile::Oblivion,
            [&](const auto&) { validated = true; }, &content); };
        if (fault == 0 || fault == 5)
        {
            EXPECT_NO_THROW(admit());
            EXPECT_TRUE(validated);
        }
        else
        {
            EXPECT_THROW(admit(), std::exception);
            EXPECT_FALSE(validated);
        }
        EXPECT_EQ(reader.getFileOffset(), offset);
        EXPECT_EQ(reader.mScriptsConfiguration, &outgoing);
    }
}

TEST(SaveAdmissionTest, SharedCellLuaChecksObjectsAndAllInventoryStateOwners)
{
    MWWorld::ESMStore content;
    installAdmissionLuaConfiguration(content);
    const auto id = ESM::RefId::stringRefId("lua-owner");
    ESM::NPC npc{}; npc.mId = id;
    ESM::Creature creature{}; creature.mId = ESM::RefId::stringRefId("lua-creature");
    ESM::Container container{}; container.mId = ESM::RefId::stringRefId("lua-container");
    ESM4::Weapon weapon{}; weapon.mId = {0x940, 0};
    content.getWritable<ESM::NPC>().insertStatic(npc);
    content.getWritable<ESM::Creature>().insertStatic(creature);
    content.getWritable<ESM::Container>().insertStatic(container);
    content.getWritable<ESM4::Weapon>().insertStatic(weapon);
    content.setUp();
    for (int type = 0; type != 4; ++type)
    for (bool inventory : {false, true})
    {
        if (type == 3 && inventory) continue;
        SCOPED_TRACE(type);
        SCOPED_TRACE(inventory);
        std::unique_ptr<ESM::ObjectState> object;
        ESM::InventoryState* items = nullptr;
        ESM::RefId base;
        if (type == 0) { auto p = std::make_unique<ESM::NpcState>(); items = &p->mInventory; object = std::move(p); base = id; }
        else if (type == 1) { auto p = std::make_unique<ESM::CreatureState>(); items = &p->mInventory; object = std::move(p); base = creature.mId; }
        else if (type == 2) { auto p = std::make_unique<ESM::ContainerState>(); items = &p->mInventory; object = std::move(p); base = container.mId; }
        else { object = std::make_unique<ESM::ObjectState>(); base = ESM::RefId(weapon.mId); }
        object->blank();
        object->mRef.mRefID = base;
        object->mRef.mRefNum = {1, -1};
        ESM::ObjectState* owner = object.get();
        if (inventory)
        {
            owner = &items->mItems.emplace_back();
            owner->blank();
            owner->mRef.mRefID = ESM::RefId(weapon.mId);
        }
        owner->mLuaScripts.mScripts.push_back({1, "malformed local state", {}});
        const auto records = worldRecords([&](ESM::ESMWriter& writer) {
            writer.startRecord(ESM::REC_CSTA);
            writer.writeCellId(ESM::RefId(ESM::FormId{1, 0}));
            ESM::CellState cell{}; cell.mIsInterior = true; cell.save(writer);
            writer.writeHNT("OBJE", std::uint32_t{0});
            object->save(writer);
            writer.endRecord(ESM::REC_CSTA);
            writeAdmissionLuaMapping(writer);
        });
        ESM::ESMReader reader;
        openBytes(reader, saveBytes() + records);
        bool validated = false;
        try
        {
            MWState::admitSave(reader, ESM::GameProfile::Oblivion,
                [&](const auto&) { validated = true; }, &content);
            FAIL() << "malformed local data admitted";
        }
        catch (const std::exception& error)
        {
            EXPECT_NE(std::string(error.what()).find("Lua serialization format"), std::string::npos);
        }
        EXPECT_FALSE(validated);
        EXPECT_EQ(reader.mScriptsConfiguration, nullptr);
    }
}

TEST(SaveAdmissionTest, InvalidSharedTimerWireValuesRejectBeforeNativePublication)
{
    for (bool playerContext : {false, true})
    for (int fault = 0; fault != 7; ++fault)
    {
        SCOPED_TRACE(playerContext);
        SCOPED_TRACE(fault);
        ESM::LuaScripts scripts;
        scripts.mScripts.push_back({0, {}, {{ESM::LuaTimer::Type::SIMULATION_TIME, 1.25, "never-run", {}}}});
        auto records = worldRecords([&](ESM::ESMWriter& writer) {
            if (playerContext)
            {
                ESM::Player player{};
                player.mObject.blank();
                player.mObject.mRef.mRefID = ESM::RefId::stringRefId("Player");
                player.mObject.mLuaScripts = scripts;
                player.mCellId = ESM::RefId(ESM::FormId{1, 0});
                writer.startRecord(ESM::REC_PLAY);
                player.save(writer);
                writer.endRecord(ESM::REC_PLAY);
            }
            else
            {
                writer.startRecord(ESM::REC_LUAM);
                writer.writeHNT("LUAW", 0.0);
                writer.writeFormId(ESM::RefNum{}, true);
                writer.writeHNString("LUAP", "removed.lua");
                scripts.save(writer);
                ESM4::saveLocalLuaScripts(writer, {});
                writer.endRecord(ESM::REC_LUAM);
            }
        });
        const auto timer = records.find("LUAT");
        ASSERT_NE(timer, std::string::npos);
        if (fault < 2)
            records[timer + 8] = fault == 0 ? 2 : static_cast<char>(255);
        else if (fault < 4)
        {
            std::uint32_t recordSize;
            std::memcpy(&recordSize, records.data() + 4, sizeof(recordSize));
            if (fault == 2)
            {
                records.erase(timer + 16, 1);
                replaceUint(records, timer + 4, 8);
                replaceUint(records, 4, recordSize - 1);
            }
            else
            {
                records.insert(timer + 17, 1, '\0');
                replaceUint(records, timer + 4, 10);
                replaceUint(records, 4, recordSize + 1);
            }
        }
        else
        {
            const double deadline = fault == 4 ? std::numeric_limits<double>::quiet_NaN()
                : (fault == 5 ? std::numeric_limits<double>::infinity() : -std::numeric_limits<double>::infinity());
            std::memcpy(records.data() + timer + 9, &deadline, sizeof(deadline));
        }
        if (playerContext)
            records += worldRecords(writeAdmissionLuaMapping);
        ESM::ESMReader reader;
        openBytes(reader, saveBytes() + records);
        const auto offset = reader.getFileOffset();
        bool validated = false;
        try
        {
            MWState::admitSave(reader, ESM::GameProfile::Oblivion,
                [&](const auto&) { validated = true; });
            FAIL() << "invalid timer admitted";
        }
        catch (const std::exception& error)
        {
            EXPECT_NE(std::string(error.what()).find("Invalid Lua timer"), std::string::npos);
        }
        EXPECT_FALSE(validated);
        EXPECT_EQ(reader.getFileOffset(), offset);
        EXPECT_EQ(reader.mScriptsConfiguration, nullptr);
    }
}

namespace
{
    std::string legacyGeneratedRecords(std::uint32_t version, int fault = -1, bool creature = false)
    {
        return worldRecords([&](ESM::ESMWriter& writer) {
            ESM::Weapon weapon{}; weapon.blank(); weapon.mId = ESM::RefId::generated(0);
            weapon.mData.mType = ESM::Weapon::LongBladeOneHand; weapon.mData.mHealth = 100;
            if (fault != 0)
            {
                writer.startRecord(ESM::REC_WEAP); weapon.save(writer); writer.endRecord(ESM::REC_WEAP);
            }
            writer.startRecord(ESM::REC_DYNA); writer.writeHNT("COUN", std::uint64_t{1}); writer.endRecord(ESM::REC_DYNA);
            ESM::InventoryState inventory;
            auto& item = inventory.mItems.emplace_back(); item.blank(); item.mRef.mRefID = weapon.mId;
            item.mRef.mCount = 1;
            item.mRef.mNativeItemCondition = version < 24 ? 37.f : 37.125f;
            item.mRef.mEnchantmentCharge = 9.25f;
            item.mRef.mOwner = ESM::RefId(ESM::FormId{5, 0});
            if (version >= 41)
            {
                item.mRef.mNativeOwnershipRank = -2;
                item.mRef.mNativeOwnershipGlobal = ESM::RefId(ESM::FormId{6, 0});
            }
            inventory.mEquipmentSlots[0] = fault == 1 ? 99 : MWWorld::InventoryStore::Slot_CarriedRight;
            ESM::Player player{}; player.mObject.blank(); player.mObject.mRef.mRefID = ESM::RefId::stringRefId("Player");
            player.mCellId = ESM::RefId(ESM::FormId{1, 0}); player.mObject.mInventory = inventory;
            writer.startRecord(ESM::REC_PLAY); player.save(writer); writer.endRecord(ESM::REC_PLAY);
            const auto base = ESM::RefId(ESM::FormId{4, 0});
            if (creature)
            {
                ESM::Creature definition{}; definition.blank(); definition.mId = base;
                writer.startRecord(ESM::REC_CREA); definition.save(writer); writer.endRecord(ESM::REC_CREA);
            }
            else
            {
                ESM::NPC definition{}; definition.blank(); definition.mId = base;
                writer.startRecord(ESM::REC_NPC_); definition.save(writer); writer.endRecord(ESM::REC_NPC_);
            }
            std::unique_ptr<ESM::ObjectState> actor = creature
                ? std::unique_ptr<ESM::ObjectState>(std::make_unique<ESM::CreatureState>())
                : std::unique_ptr<ESM::ObjectState>(std::make_unique<ESM::NpcState>());
            actor->blank(); actor->mRef.mRefID = base; actor->mRef.mRefNum = {0x900, 0};
            if (creature) actor->asCreatureState().mInventory = inventory;
            else actor->asNpcState().mInventory = inventory;
            writer.startRecord(ESM::REC_CSTA); writer.writeCellId(ESM::RefId(ESM::FormId{1, 0}));
            ESM::CellState cell{}; cell.mIsInterior = true; cell.save(writer);
            writer.writeHNT("OBJE", std::uint32_t{0}); actor->save(writer); writer.endRecord(ESM::REC_CSTA);
        });
    }

    ESM4::RuntimeState legacyGeneratedNative(std::uint32_t version)
    {
        auto state = nativeState(); state.mVersion = version;
        if (version < 3)
        {
            state.mPlayer.mName.clear(); state.mPlayer.mRace = {}; state.mPlayer.mClass = {};
        }
        ESM4::RuntimeReferenceState actor;
        actor.mKey = ESM::FormKey::content("headless.esm", 0x900);
        actor.mBase = ESM::FormKey::content("headless.esm", 4);
        actor.mCell = state.mPlayer.mCell;
        state.mReferences.push_back(actor);
        return state;
    }
}

TEST(SaveAdmissionTest, LegacyGeneratedPlayerNpcAndCreatureGearMigratesEverySupportedVersion)
{
    MWClass::Weapon::registerSelf();
    for (std::uint32_t version = 1; version <= ESM4::CurrentRuntimeStateVersion; ++version)
    for (bool creature : {false, true})
    {
        SCOPED_TRACE(version);
        SCOPED_TRACE(creature);
        const auto state = legacyGeneratedNative(version);
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(ESM::GameProfile::Oblivion, version, 1, 1, false, false, &state)
            + legacyGeneratedRecords(version, -1, creature));
        const std::map<int, int> mapping{{0, 2}};
        reader.setContentFileMapping(&mapping);
        const auto start = reader.getContext(); bool called = false;
        MWState::admitSave(reader, ESM::GameProfile::Oblivion, [&](const auto& restored) {
            called = true;
            ASSERT_EQ(restored.mPlayer.mInventory.size(), 1);
            ASSERT_EQ(restored.mReferences.front().mInventory.size(), 1);
            const auto& item = restored.mPlayer.mInventory.front();
            EXPECT_EQ(item.mBase, ESM::FormKey::dynamic("shared-item", 1)); EXPECT_EQ(item.mCount, 1);
            EXPECT_EQ(item.mCondition, version < 4 ? -1 : version < 24 ? 37 : 37.125);
            EXPECT_EQ(item.mCharge, version < 4 ? -1 : 9.25);
            EXPECT_EQ(item.mEquippedSlots, version < 4 ? 0 : ESM4::InventorySlotWeapon);
            // The shared inventory writer deliberately omits ANAM ownership.
            EXPECT_EQ(item.mOwner, ESM::FormKey{});
            EXPECT_EQ(item.mOwnershipRank, version < 41 ? std::nullopt : std::optional<std::int32_t>(-2));
            EXPECT_EQ(item.mOwnershipGlobal,
                version < 41 ? ESM::FormKey{} : ESM::FormKey::content("headless.esm", 6));
            EXPECT_EQ(restored.mReferences.front().mInventory, restored.mPlayer.mInventory);
            EXPECT_EQ(restored.mVersion, version); EXPECT_NO_THROW(restored.validate());
        });
        EXPECT_TRUE(called); EXPECT_EQ(reader.getContext().filePos, start.filePos);
    }
}

TEST(SaveAdmissionTest, ExplicitNativeGeneratedInventoryWinsWithoutSharedDuplication)
{
    MWClass::Weapon::registerSelf();
    auto state = legacyGeneratedNative(ESM4::CurrentRuntimeStateVersion);
    ESM4::RuntimeInventoryItem explicitItem;
    explicitItem.mBase = ESM::FormKey::dynamic("shared-item", 1); explicitItem.mCount = 2;
    explicitItem.mCondition = 11.25; explicitItem.mCharge = 3.5f;
    state.mPlayer.mInventory = {explicitItem}; state.mReferences.front().mInventory = {explicitItem};
    ESM::ESMReader reader;
    openBytes(reader, saveBytes(ESM::GameProfile::Oblivion, state.mVersion, 1, 1, false, false, &state)
        + legacyGeneratedRecords(state.mVersion));
    MWState::admitSave(reader, ESM::GameProfile::Oblivion, [&](const auto& restored) {
        EXPECT_EQ(restored.serializeBinary(), state.serializeBinary());
    });
}

TEST(SaveAdmissionTest, LegacyGeneratedRecoveryRejectsMissingDefinitionsBadSlotsAndActorBaseConflicts)
{
    MWClass::Weapon::registerSelf();
    for (int fault = 0; fault != 3; ++fault)
    {
        auto state = legacyGeneratedNative(ESM4::CurrentRuntimeStateVersion);
        if (fault == 2) state.mReferences.front().mBase = ESM::FormKey::content("headless.esm", 6);
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(ESM::GameProfile::Oblivion, state.mVersion, 1, 1, false, false, &state)
            + legacyGeneratedRecords(state.mVersion, fault));
        const auto start = reader.getContext(); bool called = false;
        EXPECT_ANY_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, [&](const auto&) { called = true; }));
        EXPECT_FALSE(called); EXPECT_EQ(reader.getContext().filePos, start.filePos);
    }
}

namespace
{
    constexpr std::array auxiliaryTypes{ESM::REC_INPU, ESM::REC_CAM_, ESM::REC_ENAB, ESM::REC_RAND,
        ESM::REC_DIAS, ESM::REC_JOUR, ESM::REC_QUES, ESM::REC_GSCR, ESM::REC_KEYS, ESM::REC_ASPL,
        ESM::REC_MARK, ESM::REC_GMAP, ESM::REC_STLN, ESM::REC_DCOU, ESM::REC_WTHR, ESM::REC_PROJ, ESM::REC_MPRJ};

    std::string auxiliaryRecord(std::uint32_t type, int fault = 0)
    {
        return worldRecords([&](ESM::ESMWriter& writer) {
            writer.startRecord(type);
            if (fault == 1)
                writer.writeHNT("BAD_", std::uint8_t{1});
            else
                switch (type)
                {
                    case ESM::REC_INPU:
                        ESM::ControlsState{}.save(writer);
                        break;
                    case ESM::REC_CAM_:
                        writer.writeHNT("FIRS", true);
                        break;
                    case ESM::REC_ENAB:
                        writer.writeHNT("TELE", true);
                        writer.writeHNT("LEVT", false);
                        break;
                    case ESM::REC_RAND:
                        writer.writeHNString("RAND", fault == 3 ? "bad-random" : Misc::Rng::serialize(Misc::Rng::Generator{}));
                        break;
                    case ESM::REC_DIAS:
                        ESM::DialogueState{}.save(writer);
                        break;
                    case ESM::REC_JOUR:
                    {
                        ESM::JournalEntry entry{};
                        entry.mType = fault == 3 ? 99 : ESM::JournalEntry::Type_Quest;
                        entry.mTopic = ESM::RefId::stringRefId("quest");
                        entry.mInfo = ESM::RefId::stringRefId("info");
                        entry.save(writer);
                        break;
                    }
                    case ESM::REC_QUES:
                    {
                        ESM::QuestState quest{};
                        quest.mTopic = ESM::RefId::stringRefId("quest");
                        quest.save(writer);
                        break;
                    }
                    case ESM::REC_GSCR:
                    {
                        ESM::GlobalScript script{};
                        script.mId = ESM::RefId::stringRefId("script");
                        script.save(writer);
                        break;
                    }
                    case ESM::REC_KEYS:
                    {
                        ESM::QuickKeys keys;
                        keys.mKeys.push_back({fault == 3 ? static_cast<ESM::QuickKeys::Type>(99)
                            : ESM::QuickKeys::Type::Unassigned, {}});
                        keys.save(writer);
                        break;
                    }
                    case ESM::REC_ASPL:
                        writer.writeHNRefId("ID__", ESM::RefId::stringRefId("spell"));
                        break;
                    case ESM::REC_MARK:
                    {
                        ESM::CustomMarker marker{};
                        marker.mCell = ESM::RefId(ESM::FormId{1, 0});
                        marker.mWorldX = fault == 3 ? std::numeric_limits<float>::infinity() : 3.f;
                        marker.save(writer);
                        break;
                    }
                    case ESM::REC_GMAP:
                    {
                        ESM::GlobalMap map{};
                        if (fault == 3)
                            map.mBounds = {std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 0, 0};
                        map.save(writer);
                        break;
                    }
                    case ESM::REC_STLN:
                    {
                        ESM::StolenItems stolen;
                        stolen.mStolenItems[ESM::RefId::stringRefId("item")][{ESM::RefId::stringRefId("owner"), false}]
                            = fault == 3 ? -1 : 2;
                        stolen.write(writer);
                        break;
                    }
                    case ESM::REC_DCOU:
                        writer.writeHNRefId("ID__", ESM::RefId::stringRefId("actor"));
                        writer.writeHNT("COUN", fault == 3 ? -1 : 2);
                        break;
                    case ESM::REC_WTHR:
                    {
                        ESM::WeatherState weather{};
                        weather.mNextWeather = weather.mQueuedWeather = -1;
                        weather.mTimePassed = fault == 3 ? std::numeric_limits<float>::quiet_NaN() : 0.f;
                        weather.save(writer);
                        break;
                    }
                    case ESM::REC_PROJ:
                    {
                        ESM::ProjectileState projectile{};
                        projectile.mId = ESM::RefId::stringRefId("arrow");
                        projectile.mBowId = ESM::RefId::stringRefId("bow");
                        projectile.mOrientation = osg::Quat{};
                        projectile.mAttackStrength = fault == 3 ? std::numeric_limits<float>::infinity() : 1.f;
                        projectile.mAttackWindUp = -1.f;
                        projectile.save(writer);
                        break;
                    }
                    case ESM::REC_MPRJ:
                    {
                        ESM::MagicBoltState bolt{};
                        bolt.mId = ESM::RefId::stringRefId("bolt");
                        bolt.mSpellId = ESM::RefId::stringRefId("spell");
                        bolt.mOrientation = osg::Quat{};
                        bolt.mSpeed = fault == 3 ? std::numeric_limits<float>::quiet_NaN() : 1.f;
                        bolt.save(writer);
                        break;
                    }
                }
            if (fault == 2)
                writer.writeHNT("EXTR", std::uint8_t{1});
            writer.endRecord(type);
        });
    }
}

TEST(SaveAdmissionTest, AllAuxiliaryRecordFamiliesDecodeBeforeNativePreparationAndRewind)
{
    std::string records;
    for (const auto type : auxiliaryTypes) records += auxiliaryRecord(type);
    ESM::ESMReader reader;
    openBytes(reader, saveBytes() + records);
    const auto start = reader.getContext();
    int calls = 0;
    EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, [&](const auto&) { ++calls; }));
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(reader.getContext().filePos, start.filePos);
}

TEST(SaveAdmissionTest, AuxiliaryMalformedPayloadsAndTrailingFieldsRejectBeforeNativePreparation)
{
    for (const auto type : auxiliaryTypes)
        for (const int fault : {1, 2})
        {
            SCOPED_TRACE(type);
            SCOPED_TRACE(fault);
            ESM::ESMReader reader;
            openBytes(reader, saveBytes() + auxiliaryRecord(type, fault));
            const auto start = reader.getContext();
            bool called = false;
            EXPECT_ANY_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, [&](const auto&) { called = true; }));
            EXPECT_FALSE(called);
            EXPECT_EQ(reader.getContext().filePos, start.filePos);
        }
}

TEST(SaveAdmissionTest, AuxiliarySingletonsRejectDuplicatesButRepeatableRecordsRemainAccepted)
{
    for (const auto type : auxiliaryTypes)
    {
        SCOPED_TRACE(type);
        const bool repeatable = type == ESM::REC_JOUR || type == ESM::REC_QUES || type == ESM::REC_GSCR
            || type == ESM::REC_MARK || type == ESM::REC_PROJ || type == ESM::REC_MPRJ;
        ESM::ESMReader reader;
        openBytes(reader, saveBytes() + auxiliaryRecord(type) + auxiliaryRecord(type));
        bool called = false;
        const auto admit = [&] {
            MWState::admitSave(reader, ESM::GameProfile::Oblivion, [&](const auto&) { called = true; });
        };
        if (repeatable) EXPECT_NO_THROW(admit());
        else EXPECT_ANY_THROW(admit());
        EXPECT_EQ(called, repeatable);
    }
}

TEST(SaveAdmissionTest, AuxiliaryInvalidDomainsRejectBeforeNativePreparation)
{
    for (const auto type : {ESM::REC_RAND, ESM::REC_JOUR, ESM::REC_KEYS, ESM::REC_MARK, ESM::REC_GMAP,
             ESM::REC_STLN, ESM::REC_DCOU, ESM::REC_WTHR, ESM::REC_PROJ, ESM::REC_MPRJ})
    {
        SCOPED_TRACE(type);
        ESM::ESMReader reader;
        openBytes(reader, saveBytes() + auxiliaryRecord(type, 3));
        bool called = false;
        EXPECT_ANY_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, [&](const auto&) { called = true; }));
        EXPECT_FALSE(called);
    }
}

TEST(SaveAdmissionTest, AuxiliaryAdmissionDoesNotChangeMorrowindOrUnknownRecordCompatibility)
{
    for (const auto profile : {ESM::GameProfile::Morrowind, ESM::GameProfile::Oblivion})
    {
        ESM::ESMReader reader;
        const auto records = profile == ESM::GameProfile::Morrowind ? auxiliaryRecord(ESM::REC_INPU, 1)
            : worldRecords([](auto& writer) {
                writer.startRecord("ZZZZ"); writer.writeHNT("BAD_", std::uint8_t{1}); writer.endRecord("ZZZZ");
            });
        openBytes(reader, saveBytes(profile, ESM4::CurrentRuntimeStateVersion, 1,
            profile == ESM::GameProfile::Oblivion ? 1 : 0) + records);
        EXPECT_NO_THROW(MWState::admitSave(reader, profile, [](const auto&) {}));
    }
}

TEST(SaveAdmissionTest, OwnedPlayerFameRejectsConflictingSharedViewBeforePreparation)
{
    for (const int fame : {0, -3, 16777217, std::numeric_limits<int>::max()})
    for (const bool owned : {false, true})
    for (const bool conflict : {false, true})
    {
        SCOPED_TRACE(fame);
        SCOPED_TRACE(owned);
        SCOPED_TRACE(conflict);
        auto state = nativeState();
        ESM4::RuntimeActorValues actor;
        actor.mActor = state.mPlayer.mReference;
        actor.mBase = ESM::FormKey::dynamic("player-base", 1);
        actor.mOwner = ESM4::ActorValueOwner::Player;
        actor.mProcess = ESM4::ActorValueProcess::Active;
        actor.mPlayerFormValues = {{0, 0, 0, 0}};
        if (owned) actor.mReputation = ESM4::PlayerReputationState{fame, -9, std::nullopt};
        state.mNativeActorValues.push_back(actor);
        const auto records = worldRecords([&](ESM::ESMWriter& writer) {
            ESM::Player player{}; player.mObject.blank();
            player.mObject.mRef.mRefID = ESM::RefId::stringRefId("Player");
            player.mCellId = ESM::RefId(ESM::FormId{1, 0});
            player.mObject.mNpcStats.mReputation = conflict ? (fame == 0 ? 1 : 0) : fame;
            writer.startRecord(ESM::REC_PLAY); player.save(writer); writer.endRecord(ESM::REC_PLAY);
        });
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(ESM::GameProfile::Oblivion, state.mVersion, 1, 1, false, false, &state) + records);
        const auto offset = reader.getFileOffset(); int calls = 0;
        const auto prepare = [&](const auto&, auto) { ++calls; };
        if (owned && conflict)
            EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, nullptr, prepare), std::runtime_error);
        else
            EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, nullptr, prepare));
        EXPECT_EQ(calls, int(!(owned && conflict)));
        EXPECT_EQ(reader.getFileOffset(), offset);
    }
}

TEST(SaveAdmissionTest, OwnedPlayerBountyRejectsConflictingSharedViewBeforePreparation)
{
    for (const auto [normal, alternate, realm, expected] : {
             std::tuple{.25f, -3.5f, false, 1}, std::tuple{.25f, -3.5f, true, -3},
             std::tuple{10.75f, .5f, false, 10}, std::tuple{10.75f, .5f, true, 1},
             std::tuple{0x1p31f, 0.f, false, std::numeric_limits<int>::min()}})
    for (std::uint32_t version : {44u, 45u})
    for (bool owned : {false, true})
    for (bool conflict : {false, true})
    {
        SCOPED_TRACE(normal);
        SCOPED_TRACE(realm);
        SCOPED_TRACE(owned);
        SCOPED_TRACE(conflict);
        SCOPED_TRACE(version);
        auto state = nativeState();
        state.mVersion = version;
        ESM4::RuntimeActorValues actor;
        actor.mActor = state.mPlayer.mReference;
        actor.mBase = ESM::FormKey::dynamic("player-base", 1);
        actor.mOwner = ESM4::ActorValueOwner::Player;
        actor.mProcess = ESM4::ActorValueProcess::Active;
        actor.mPlayerFormValues = {{0, 0, 0, 0}};
        actor.mValues[37].mModifiers = {10.f, 20.f, -2.f}; // Independent of legal bounty.
        if (owned)
        {
            actor.mBounty = ESM4::CrimeBountyState{normal, alternate};
            actor.mPlayerInShiveringIsles = realm;
        }
        state.mNativeActorValues.push_back(actor);
        auto records = worldRecords([&](ESM::ESMWriter& writer) {
            ESM::Player player{}; player.mObject.blank();
            player.mObject.mRef.mRefID = ESM::RefId::stringRefId("Player");
            player.mCellId = ESM::RefId(ESM::FormId{1, 0});
            player.mObject.mNpcStats.mBounty = expected + int(conflict);
            writer.startRecord(ESM::REC_PLAY); player.save(writer); writer.endRecord(ESM::REC_PLAY);
        });
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(ESM::GameProfile::Oblivion, state.mVersion, 1, 1, false, false, &state) + records);
        const auto offset = reader.getFileOffset(); int calls = 0;
        const auto prepare = [&](const auto&, auto) { ++calls; };
        if (version >= 45 && owned && conflict)
            EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, nullptr, prepare), std::runtime_error);
        else
            EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, nullptr, prepare));
        EXPECT_EQ(calls, int(!(version >= 45 && owned && conflict)));
        EXPECT_EQ(reader.getFileOffset(), offset);
    }
}

TEST(SaveAdmissionTest, OwnedNpcBountyChecksStableReferenceBaseAndDuplicateSharedViews)
{
    for (int fault = 0; fault != 4; ++fault)
    {
        SCOPED_TRACE(fault);
        auto state = legacyGeneratedNative(ESM4::CurrentRuntimeStateVersion);
        ESM4::RuntimeActorValues actor;
        actor.mActor = state.mReferences.front().mKey;
        actor.mBase = state.mReferences.front().mBase;
        actor.mBounty = ESM4::CrimeBountyState{10.75f, 0.f};
        state.mNativeActorValues.push_back(actor);
        auto records = worldRecords([&](ESM::ESMWriter& writer) {
            ESM::NPC definition{}; definition.blank();
            definition.mId = ESM::RefId(ESM::FormId{fault == 2 ? 5u : 4u, 0});
            writer.startRecord(ESM::REC_NPC_); definition.save(writer); writer.endRecord(ESM::REC_NPC_);
            ESM::NpcState npc{}; npc.blank(); npc.mRef.mRefID = definition.mId;
            npc.mRef.mRefNum = {0x900, 0}; npc.mNpcStats.mBounty = fault == 1 ? 11 : 10;
            writer.startRecord(ESM::REC_CSTA); writer.writeCellId(ESM::RefId(ESM::FormId{1, 0}));
            ESM::CellState cell{}; cell.mIsInterior = true; cell.save(writer);
            for (int i = 0; i != (fault == 3 ? 2 : 1); ++i)
            {
                writer.writeHNT("OBJE", std::uint32_t{0}); npc.save(writer);
            }
            writer.endRecord(ESM::REC_CSTA);
        });
        ESM::ESMReader reader;
        openBytes(reader, saveBytes(ESM::GameProfile::Oblivion, state.mVersion, 1, 1, false, false, &state) + records);
        const std::map<int, int> mapping{{0, 2}};
        reader.setContentFileMapping(&mapping); // Base ID is remapped; reference number retains saved index.
        const auto offset = reader.getFileOffset(); int calls = 0;
        const auto prepare = [&](const auto&, auto) { ++calls; };
        if (fault)
            EXPECT_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, nullptr, prepare), std::runtime_error);
        else
            EXPECT_NO_THROW(MWState::admitSave(reader, ESM::GameProfile::Oblivion, {}, nullptr, prepare));
        EXPECT_EQ(calls, int(fault == 0));
        EXPECT_EQ(reader.getFileOffset(), offset);
    }
}
