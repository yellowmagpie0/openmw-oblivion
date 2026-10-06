#include <gtest/gtest.h>

#include <cstring>
#include <set>
#include <sstream>
#include <limits>
#include <type_traits>

#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadglob.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/esm4/runtimestate.hpp>

#include "apps/openmw/mwstate/saveadmission.hpp"
#include "apps/openmw/mwworld/esmstore.hpp"

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
