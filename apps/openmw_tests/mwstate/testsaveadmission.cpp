#include <gtest/gtest.h>

#include <cstring>
#include <set>
#include <sstream>

#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm4/runtimestate.hpp>

#include "apps/openmw/mwstate/saveadmission.hpp"

namespace
{
    std::string saveBytes(ESM::GameProfile profile = ESM::GameProfile::Oblivion,
        std::uint32_t version = ESM4::CurrentRuntimeStateVersion,
        int profiles = 1, int nativeRecords = 1, bool trailingProfile = false, bool trailingNative = false)
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
