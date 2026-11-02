#include <components/esm4/loadlvsp.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/reader.hpp>
#include <components/toutf8/toutf8.hpp>

#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <zlib.h>

namespace
{
    void put(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i)
            data.at(offset + i) = static_cast<std::uint8_t>(value >> (i * 8));
    }
    void number(std::vector<std::uint8_t>& data, std::size_t offset, float value)
    {
        put(data, offset, std::bit_cast<std::uint32_t>(value));
    }
    template <typename T> void append(std::vector<std::uint8_t>& data, T value)
    {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        data.insert(data.end(), bytes, bytes + sizeof(T));
    }
    void sub(std::vector<std::uint8_t>& data, const char* type, const std::vector<std::uint8_t>& payload)
    {
        data.insert(data.end(), type, type + 4);
        append(data, static_cast<std::uint16_t>(payload.size()));
        data.insert(data.end(), payload.begin(), payload.end());
    }
    void record(std::vector<std::uint8_t>& data, const char* type, std::uint32_t flags,
        std::uint32_t id, const std::vector<std::uint8_t>& payload)
    {
        data.insert(data.end(), type, type + 4);
        append(data, static_cast<std::uint32_t>(payload.size()));
        append(data, flags);
        append(data, id);
        append(data, std::uint32_t{});
        data.insert(data.end(), payload.begin(), payload.end());
    }
    ESM4::LevelledSpell load(std::vector<std::uint8_t> payload, bool compressed = false,
        std::uint32_t flags = 0, float version = 1.f)
    {
        std::vector<std::uint8_t> header, hedr, data;
        append(hedr, version);
        append(hedr, std::uint32_t{ 1 });
        append(hedr, std::uint32_t{ 0x800 });
        sub(header, "HEDR", hedr);
        record(data, "TES4", ESM4::Rec_ESM, 0, header);
        if (compressed)
        {
            uLongf length = compressBound(payload.size());
            std::vector<std::uint8_t> packed(length + 4);
            put(packed, 0, payload.size());
            if (compress(packed.data() + 4, &length, payload.data(), payload.size()) != Z_OK)
                throw std::runtime_error("test compression failed");
            packed.resize(length + 4);
            payload = std::move(packed);
            flags |= ESM4::Rec_Compressed;
        }
        record(data, "LVSP", flags, 0x800, payload);
        auto stream = std::make_unique<std::stringstream>(std::string(data.begin(), data.end()),
            std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "fixture.esm", nullptr, &encoder, true);
        if (!reader.getRecordHeader())
            throw std::runtime_error("test record missing");
        reader.getRecordData();
        ESM4::LevelledSpell result;
        result.load(reader);
        return result;
    }
}

namespace
{
    std::vector<std::uint8_t> entry(std::uint32_t id = 0x98416)
    {
        std::vector<std::uint8_t> result(12);
        result[0] = 1; result[2] = 0xcd; result[3] = 0xff;
        put(result, 4, id); result[8] = 1; result[10] = 0xaa; result[11] = 0x55;
        return result;
    }
    std::vector<std::uint8_t> spellList()
    {
        std::vector<std::uint8_t> result;
        sub(result, "LVLD", {90}); sub(result, "LVLF", {3}); sub(result, "LVLO", entry());
        sub(result, "XTRA", {1, 2, 3});
        return result;
    }
}

TEST(ESM4LevelledSpell, ReadsStockDogLayoutAndPreservesPaddingAndUnknownFields)
{
    for (const bool compressed : {false, true})
    {
        const auto source = spellList();
        const auto record = load(source, compressed);
        EXPECT_EQ(record.mChanceNone, 90); EXPECT_EQ(record.mListFlags, 3);
        ASSERT_EQ(record.mEntries.size(), 1);
        const auto& selected = record.mEntries.front();
        EXPECT_EQ(selected.mLevel, 1); EXPECT_EQ(selected.mCount, 1);
        EXPECT_EQ(selected.mSpell, (ESM::FormId{0x98416, 0}));
        EXPECT_EQ(selected.mSpellKey, ESM::FormKey::content("fixture.esm", 0x98416));
        ASSERT_EQ(record.mSubRecords.size(), 4);
        EXPECT_EQ(record.mSubRecords[2].mData, entry());
        EXPECT_EQ(record.mSubRecords[3].mData, (std::vector<std::uint8_t>{1, 2, 3}));
    }
}

TEST(ESM4LevelledSpell, RejectsDuplicateMissingMalformedFieldsAndLaterLayouts)
{
    for (const auto tag : {"LVLD", "LVLF"})
    {
        auto data = spellList(); sub(data, tag, {0});
        EXPECT_THROW(load(data), std::runtime_error);
        for (const auto length : {0, 2, 4})
        {
            std::vector<std::uint8_t> fields;
            sub(fields, "LVLD", tag == std::string_view("LVLD") ? std::vector<std::uint8_t>(length) : std::vector<std::uint8_t>{90});
            sub(fields, "LVLF", tag == std::string_view("LVLF") ? std::vector<std::uint8_t>(length) : std::vector<std::uint8_t>{3});
            EXPECT_THROW(load(fields), std::runtime_error);
        }
    }
    for (const auto length : {0, 8, 11, 13})
    {
        auto data = spellList(); sub(data, "LVLO", std::vector<std::uint8_t>(length));
        EXPECT_THROW(load(data), std::runtime_error);
    }
    for (auto chance : {101, 255})
    {
        std::vector<std::uint8_t> data;
        sub(data, "LVLD", {static_cast<std::uint8_t>(chance)}); sub(data, "LVLF", {3});
        EXPECT_THROW(load(data), std::runtime_error);
    }
    std::vector<std::uint8_t> unknown;
    sub(unknown, "LVLD", {0}); sub(unknown, "LVLF", {8});
    EXPECT_THROW(load(unknown), std::runtime_error);
    EXPECT_THROW(load({}), std::runtime_error);
    EXPECT_THROW(load(spellList(), false, 0, .94f), std::runtime_error);
    EXPECT_NO_THROW(load(spellList(), false, 0, .8f));
    EXPECT_NO_THROW(load({}, false, ESM4::Rec_Deleted));
}

TEST(ESM4LevelledSpell, RetainsUnsignedWireWordsAndExplicitEmptyList)
{
    std::vector<std::uint8_t> data;
    sub(data, "LVLD", {0}); sub(data, "LVLF", {0});
    const auto empty = load(data);
    EXPECT_TRUE(empty.mEntries.empty());
    auto bytes = entry(); bytes[0] = bytes[1] = bytes[8] = bytes[9] = 0xff;
    sub(data, "LVLO", bytes);
    const auto value = load(data);
    ASSERT_EQ(value.mEntries.size(), 1);
    EXPECT_EQ(value.mEntries[0].mLevel, 65535); EXPECT_EQ(value.mEntries[0].mCount, 65535);
}

TEST(ESM4LevelledSpell, StockDogSelectionMatchesOriginalPerSchoolSkillAndChanceBranches)
{
    const auto source = load(spellList());
    for (const auto chance : {0, 90, 100})
    for (const auto skill : {0, 1, 40})
    for (const auto word : {89, 90})
    {
        auto inputs = source; inputs.mChanceNone = chance;
        std::array<std::int32_t, 6> skills; skills.fill(skill);
        std::array<std::uint16_t, 20> words; words.fill(word);
        const auto selected = ESM4::selectSingleLevelledSpell(inputs, 2, skills, words);
        const bool expected = skill >= 1 && (chance == 0 || word >= chance);
        EXPECT_EQ(selected.mSpell.has_value(), expected);
        if (expected) EXPECT_EQ(*selected.mSpell, source.mEntries[0].mSpellKey);
        EXPECT_EQ(selected.mDraws, (chance ? 6 : 0) + unsigned(expected));
    }
}

TEST(ESM4LevelledSpell, SingleChildSelectionRejectsUnreviewedShapesAndIncompleteRandomInputs)
{
    const auto source = load(spellList());
    std::array<std::int32_t, 6> skills; skills.fill(40);
    std::array<std::uint16_t, 7> words; words.fill(99);
    EXPECT_THROW(ESM4::selectSingleLevelledSpell(source, 6, skills, words), std::invalid_argument);
    EXPECT_THROW(ESM4::selectSingleLevelledSpell(source, 2, skills, std::span(words).first(6)), std::invalid_argument);
    words[0] = 32768;
    EXPECT_THROW(ESM4::selectSingleLevelledSpell(source, 2, skills, words), std::invalid_argument);
    words.fill(99);
    for (const auto flags : {4, 8, 255})
    {
        auto unsupported = source; unsupported.mListFlags = flags;
        EXPECT_THROW(ESM4::selectSingleLevelledSpell(unsupported, 2, skills, words), std::invalid_argument);
    }
    auto unsupported = source; unsupported.mEntries[0].mCount = 2;
    EXPECT_THROW(ESM4::selectSingleLevelledSpell(unsupported, 2, skills, words), std::invalid_argument);
    unsupported = source; unsupported.mEntries.push_back(unsupported.mEntries[0]);
    EXPECT_THROW(ESM4::selectSingleLevelledSpell(unsupported, 2, skills, words), std::invalid_argument);
    unsupported = source; unsupported.mEntries[0].mSpellKey = {};
    EXPECT_THROW(ESM4::selectSingleLevelledSpell(unsupported, 2, skills, words), std::invalid_argument);
}
