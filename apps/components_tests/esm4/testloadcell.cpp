#include <components/esm4/loadcell.hpp>
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
    ESM4::Cell load(std::vector<std::uint8_t> payload, bool compressed = false,
        std::uint32_t flags = 0, float version = 1.f, ESM4::Cell* reused = nullptr)
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
        std::vector<std::uint8_t> cell;
        record(cell, "CELL", flags, 0x800, payload);
        // CELL loading consults its real containing group even for an
        // interior/standalone parser fixture.
        append(data, ESM4::REC_GRUP);
        append(data, static_cast<std::uint32_t>(20 + cell.size()));
        append(data, ESM4::REC_CELL);
        append(data, std::int32_t{0});
        append(data, std::uint32_t{0});
        data.insert(data.end(), cell.begin(), cell.end());
        auto stream = std::make_unique<std::stringstream>(std::string(data.begin(), data.end()),
            std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "fixture.esm", nullptr, &encoder, true);
        if (!reader.getRecordHeader() || reader.hdr().record.typeId != ESM4::REC_GRUP)
            throw std::runtime_error("test CELL group missing");
        reader.enterGroup();
        if (!reader.getRecordHeader() || reader.hdr().record.typeId != ESM4::REC_CELL)
            throw std::runtime_error("test CELL record missing");
        reader.getRecordData();
        ESM4::Cell local{};
        ESM4::Cell& result = reused ? *reused : local;
        result.load(reader);
        return result;
    }
}

TEST(ESM4CellOwnership, LoadsOptionalSignedRankWithoutInventingZero)
{
    EXPECT_FALSE(load({}).mOwnershipRank);
    for (const auto rank : {std::numeric_limits<std::int32_t>::min(), -2, -1, 0, 1, 9,
                            std::numeric_limits<std::int32_t>::max()})
        for (bool compressed : {false, true})
            for (float version : {.8f, 1.f})
            {
                SCOPED_TRACE(rank);
                SCOPED_TRACE(compressed);
                SCOPED_TRACE(version);
                std::vector<std::uint8_t> payload, data;
                append(data, rank);
                sub(payload, "XRNK", data);
                const auto cell = load(payload, compressed, 0, version);
                ASSERT_TRUE(cell.mOwnershipRank);
                EXPECT_EQ(*cell.mOwnershipRank, rank);
                EXPECT_EQ(cell.mFormKey.serialize(), "content:fixture.esm:000800");
            }
}

TEST(ESM4CellOwnership, RejectsMalformedRankLengths)
{
    for (std::size_t size : {0u, 1u, 3u, 5u, 8u})
    {
        SCOPED_TRACE(size);
        std::vector<std::uint8_t> payload;
        sub(payload, "XRNK", std::vector<std::uint8_t>(size, 0));
        try
        {
            load(payload);
            ADD_FAILURE() << "malformed CELL XRNK accepted";
        }
        catch (const std::runtime_error& error)
        {
            EXPECT_NE(std::string(error.what()).find("CELL XRNK"), std::string::npos);
        }
    }
}

TEST(ESM4CellOwnership, RejectsDuplicateRankIncludingEqualValues)
{
    for (std::int32_t initial : {0, 1})
      for (std::int32_t second : {0, 1, 9})
    {
        std::vector<std::uint8_t> payload, first, last;
        append(first, initial); append(last, second);
        sub(payload, "XRNK", first); sub(payload, "XRNK", last);
        try
        {
            load(payload);
            ADD_FAILURE() << "malformed CELL XRNK accepted";
        }
        catch (const std::runtime_error& error)
        {
            EXPECT_NE(std::string(error.what()).find("CELL XRNK"), std::string::npos);
        }
    }
}

TEST(ESM4CellOwnership, ReusedRecordDropsRankAbsentFromReplacement)
{
    ESM4::Cell cell{};
    std::vector<std::uint8_t> payload, data;
    append(data, std::int32_t{8}); sub(payload, "XRNK", data);
    load(payload, false, 0, 1.f, &cell);
    ASSERT_EQ(cell.mOwnershipRank, 8);
    load({}, false, 0, 1.f, &cell);
    EXPECT_FALSE(cell.mOwnershipRank);
}

TEST(ESM4CellOwnership, RankConsumptionPreservesFollowingSubrecord)
{
    std::vector<std::uint8_t> payload, data;
    append(data, std::int32_t{5}); sub(payload, "XRNK", data);
    sub(payload, "EDID", {'R', 'a', 'n', 'k', 'e', 'd', 0});
    const auto cell = load(payload);
    ASSERT_EQ(cell.mOwnershipRank, 5);
    EXPECT_EQ(cell.mEditorId, "Ranked");
}
