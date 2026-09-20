#include <components/esm4/loadfact.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/reader.hpp>
#include <components/toutf8/toutf8.hpp>

#include <gtest/gtest.h>
#include <bit>
#include <limits>
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
    ESM4::Faction load(std::vector<std::uint8_t> payload, bool compressed = false,
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
        record(data, "FACT", flags, 0x800, payload);
        auto stream = std::make_unique<std::stringstream>(std::string(data.begin(), data.end()),
            std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "fixture.esm", nullptr, &encoder, true);
        if (!reader.getRecordHeader())
            throw std::runtime_error("test record missing");
        reader.getRecordData();
        ESM4::Faction result;
        result.load(reader);
        return result;
    }
}

TEST(ESM4Faction, LoadsFlagsSignedRelationshipsAndOptionalFineWithoutLosingPayload)
{
    std::vector<std::uint8_t> payload;
    sub(payload, "EDID", {'M', '1', '5', 0});
    sub(payload, "DATA", {7});
    std::vector<std::uint8_t> relation;
    append(relation, std::uint32_t{0x900});
    append(relation, std::int32_t{-100});
    sub(payload, "XNAM", relation);
    sub(payload, "RNAM", {0,0,0,0});
    sub(payload, "MNAM", {'R',0});
    sub(payload, "ZZZZ", {1,2,3});
    auto absent = load(payload);
    EXPECT_FALSE(absent.mCrimeMultiplier);
    std::vector<std::uint8_t> fine;
    append(fine, 1.5f);
    sub(payload, "CNAM", fine);
    for (bool compressed : {false, true})
    {
        const auto value = load(payload, compressed);
        EXPECT_EQ(value.mEditorId, "M15");
        EXPECT_EQ(value.mFormKey.serialize(), "content:fixture.esm:000800");
        EXPECT_TRUE(value.has(ESM4::FactionFlag::Hidden));
        EXPECT_TRUE(value.has(ESM4::FactionFlag::Evil));
        EXPECT_TRUE(value.has(ESM4::FactionFlag::SpecialCombat));
        ASSERT_TRUE(value.mCrimeMultiplier);
        EXPECT_EQ(*value.mCrimeMultiplier, 1.5f);
        ASSERT_EQ(value.mRelationships.size(), 1);
        EXPECT_EQ(value.mRelationships[0].mFaction.serialize(), "content:fixture.esm:000900");
        EXPECT_EQ(value.mRelationships[0].mModifier, -100);
        ASSERT_EQ(value.mSubRecords.size(), 7);
        EXPECT_EQ(value.mSubRecords[5].mData, (std::vector<std::uint8_t>{1,2,3}));
    }
}

TEST(ESM4Faction, RejectsInvalidLengthsFlagsMultipliersAndDuplicates)
{
    for (const char* tag : {"DATA", "CNAM", "XNAM"})
        for (std::size_t size = 0; size < 12; ++size)
        {
            if (size == (std::string(tag) == "DATA" ? 1u : std::string(tag) == "CNAM" ? 4u : 8u))
                continue;
            std::vector<std::uint8_t> payload;
            if (std::string(tag) != "DATA") sub(payload, "DATA", {0});
            sub(payload, tag, std::vector<std::uint8_t>(size));
            EXPECT_THROW(load(payload), std::runtime_error);
        }
    for (int flags = 8; flags <= 255; ++flags)
    {
        std::vector<std::uint8_t> payload;
        sub(payload, "DATA", {static_cast<std::uint8_t>(flags)});
        EXPECT_THROW(load(payload), std::runtime_error);
    }
    for (float invalid : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        std::vector<std::uint8_t> payload, fine;
        sub(payload, "DATA", {0});
        append(fine, invalid);
        sub(payload, "CNAM", fine);
        EXPECT_THROW(load(payload), std::runtime_error);
    }
    for (const char* tag : {"DATA", "CNAM"})
    {
        std::vector<std::uint8_t> payload;
        sub(payload, "DATA", {0});
        sub(payload, "CNAM", {0,0,0,0});
        sub(payload, tag, std::vector<std::uint8_t>(std::string(tag) == "DATA" ? 1 : 4));
        EXPECT_THROW(load(payload), std::runtime_error);
    }
}

TEST(ESM4Faction, RequiresDataExceptDeletedAndRejectsTruncationAndLaterGames)
{
    EXPECT_THROW(load({}), std::runtime_error);
    EXPECT_FALSE(load({}, false, ESM4::Rec_Deleted).mFactionFlags);
    std::vector<std::uint8_t> payload;
    sub(payload, "DATA", {0});
    EXPECT_NO_THROW(load(payload, false, 0, 0.8f));
    EXPECT_THROW(load(payload, false, 0, 1.7f), std::runtime_error);
    payload.pop_back();
    EXPECT_THROW(load(payload), std::runtime_error);
}
