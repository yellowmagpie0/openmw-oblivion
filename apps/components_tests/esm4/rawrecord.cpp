#include <components/esm4/common.hpp>
#include <components/esm4/loadrawrecord.hpp>
#include <components/esm4/reader.hpp>
#include <components/files/istreamptr.hpp>
#include <components/toutf8/toutf8.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <zlib.h>

namespace
{
    template <class T>
    void append(std::vector<char>& result, const T& value)
    {
        const auto* begin = reinterpret_cast<const char*>(&value);
        result.insert(result.end(), begin, begin + sizeof(value));
    }

    void appendSubRecord(std::vector<char>& result, std::uint32_t type, const std::vector<char>& data)
    {
        append(result, type);
        const auto size = static_cast<std::uint16_t>(data.size());
        append(result, size);
        result.insert(result.end(), data.begin(), data.end());
    }

    void appendRecordHeader(
        std::vector<char>& result, std::uint32_t type, std::uint32_t size, std::uint32_t flags, std::uint32_t id)
    {
        append(result, type);
        append(result, size);
        append(result, flags);
        append(result, id);
        append(result, std::uint32_t{ 0 });
    }

    std::vector<char> makePayload(bool extended)
    {
        std::vector<char> result;
        appendSubRecord(result, ESM::fourCC("EDID"), { 't', 'e', 's', 't', 0 });
        if (extended)
        {
            const std::uint32_t extendedSize = 70000;
            append(result, ESM::fourCC("XXXX"));
            append(result, std::uint16_t{ 4 });
            append(result, extendedSize);
            append(result, ESM::fourCC("DATA"));
            append(result, std::uint16_t{ 0 });
            result.insert(result.end(), extendedSize, char{ 0x5a });
        }
        else
            appendSubRecord(result, ESM::fourCC("DATA"), { 1, 2, 3, 4 });
        return result;
    }

    std::vector<char> makePlugin(bool compressed, bool extended)
    {
        std::vector<char> result;
        std::vector<char> headerData;
        std::vector<char> hedr;
        append(hedr, 1.0f);
        append(hedr, std::int32_t{ 1 });
        append(hedr, std::uint32_t{ 0x800 });
        appendSubRecord(headerData, ESM::fourCC("HEDR"), hedr);
        appendRecordHeader(result, ESM4::REC_TES4, headerData.size(), ESM4::Rec_ESM, 0);
        result.insert(result.end(), headerData.begin(), headerData.end());

        const std::vector<char> payload = makePayload(extended);
        if (!compressed)
        {
            appendRecordHeader(result, ESM4::REC_BSGN, payload.size(), 0, 0x1234);
            result.insert(result.end(), payload.begin(), payload.end());
            return result;
        }

        uLongf compressedSize = compressBound(payload.size());
        std::vector<char> compressedData(compressedSize);
        if (compress2(reinterpret_cast<Bytef*>(compressedData.data()), &compressedSize,
                reinterpret_cast<const Bytef*>(payload.data()), payload.size(), Z_BEST_SPEED)
            != Z_OK)
            throw std::runtime_error("Unable to compress synthetic ESM4 record");
        compressedData.resize(compressedSize);
        appendRecordHeader(
            result, ESM4::REC_BSGN, compressedData.size() + sizeof(std::uint32_t), ESM4::Rec_Compressed, 0x1234);
        append(result, static_cast<std::uint32_t>(payload.size()));
        result.insert(result.end(), compressedData.begin(), compressedData.end());
        return result;
    }

    ESM4::RawRecord readRawRecord(const std::vector<char>& data)
    {
        auto stream = std::make_unique<std::stringstream>(
            std::string(data.begin(), data.end()), std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "memory.esm", nullptr, &encoder, true);
        EXPECT_TRUE(reader.getRecordHeader());
        EXPECT_EQ(reader.hdr().record.typeId, ESM4::REC_BSGN);
        reader.getRecordData();
        ESM4::RawRecord result;
        result.load(reader);
        return result;
    }

    ESM4::RawRecord readRawRecord(bool compressed, bool extended)
    {
        return readRawRecord(makePlugin(compressed, extended));
    }

    TEST(ESM4RawRecord, preservesOrdinarySubRecords)
    {
        const ESM4::RawRecord value = readRawRecord(false, false);
        EXPECT_EQ(value.mId, ESM::FormId::fromUint32(0x1234));
        EXPECT_EQ(value.mFormKey, ESM::FormKey::content("memory.esm", 0x1234));
        EXPECT_EQ(value.mEditorId, "test");
        ASSERT_EQ(value.mSubRecords.size(), 2);
        EXPECT_FALSE(value.mSubRecords[1].mExtended);
        EXPECT_EQ(value.mSubRecords[1].mData, (std::vector<std::uint8_t>{ 1, 2, 3, 4 }));
    }

    TEST(ESM4RawRecord, preservesExtendedSubRecords)
    {
        const ESM4::RawRecord value = readRawRecord(false, true);
        ASSERT_EQ(value.mSubRecords.size(), 2);
        EXPECT_TRUE(value.mSubRecords[1].mExtended);
        ASSERT_EQ(value.mSubRecords[1].mData.size(), 70000);
        EXPECT_EQ(value.mSubRecords[1].mData.front(), 0x5a);
        EXPECT_EQ(value.mSubRecords[1].mData.back(), 0x5a);
    }

    TEST(ESM4RawRecord, preservesEntireCompressedPayload)
    {
        const ESM4::RawRecord value = readRawRecord(true, false);
        ASSERT_EQ(value.mSubRecords.size(), 2);
        EXPECT_EQ(value.mSubRecords[1].mData, (std::vector<std::uint8_t>{ 1, 2, 3, 4 }));
    }

    TEST(ESM4RawRecord, rejectsPayloadThatExtendsPastRecord)
    {
        std::vector<char> data = makePlugin(false, false);
        constexpr std::size_t secondSubRecordSizeOffset = 73;
        const std::uint16_t invalidSize = 5;
        std::memcpy(data.data() + secondSubRecordSizeOffset, &invalidSize, sizeof(invalidSize));
        EXPECT_THROW(readRawRecord(data), std::runtime_error);
    }

    TEST(ESM4RawRecord, rejectsInvalidExtendedHeader)
    {
        std::vector<char> data = makePlugin(false, true);
        constexpr std::size_t extendedHeaderSizeOffset = 73;
        const std::uint16_t invalidSize = 3;
        std::memcpy(data.data() + extendedHeaderSizeOffset, &invalidSize, sizeof(invalidSize));
        EXPECT_THROW(readRawRecord(data), std::runtime_error);
    }

    TEST(ESM4RawRecord, rejectsTruncatedCompressedPayload)
    {
        std::vector<char> data = makePlugin(true, false);
        data.pop_back();
        EXPECT_THROW(readRawRecord(data), std::runtime_error);
    }

    TEST(ESM4RawRecord, rejectsTrailingBytesShorterThanAHeader)
    {
        std::vector<char> data = makePlugin(false, false);
        data.push_back(0x1);
        constexpr std::size_t recordSizeOffset = 42;
        std::uint32_t recordSize = 0;
        std::memcpy(&recordSize, data.data() + recordSizeOffset, sizeof(recordSize));
        ++recordSize;
        std::memcpy(data.data() + recordSizeOffset, &recordSize, sizeof(recordSize));
        EXPECT_THROW(readRawRecord(data), std::runtime_error);
    }

    TEST(ESM4RawRecord, rejectsTrailingCompressedBytesAfterStreamEnd)
    {
        std::vector<char> data = makePlugin(true, false);
        data.push_back(0x1);
        constexpr std::size_t recordSizeOffset = 42;
        std::uint32_t recordSize = 0;
        std::memcpy(&recordSize, data.data() + recordSizeOffset, sizeof(recordSize));
        ++recordSize;
        std::memcpy(data.data() + recordSizeOffset, &recordSize, sizeof(recordSize));
        EXPECT_THROW(readRawRecord(data), std::runtime_error);
    }

    TEST(ESM4RawRecord, boundedMutationCorpusDoesNotEscapeReaderChecks)
    {
        const std::vector<char> original = makePlugin(false, false);
        constexpr std::size_t payloadOffset = 58;
        for (std::size_t i = payloadOffset; i < original.size(); ++i)
        {
            std::vector<char> mutated = original;
            mutated[i] ^= static_cast<char>(0x5a);
            try
            {
                static_cast<void>(readRawRecord(mutated));
            }
            catch (const std::exception&)
            {
            }
        }
    }
}
