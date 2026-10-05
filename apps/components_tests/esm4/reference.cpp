#include <components/esm/fourcc.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/readerutils.hpp>
#include <components/files/istreamptr.hpp>
#include <components/toutf8/toutf8.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <limits>
#include <stdexcept>
#include <zlib.h>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    template <class T>
    void append(std::vector<char>& output, const T& value)
    {
        const char* begin = reinterpret_cast<const char*>(&value);
        output.insert(output.end(), begin, begin + sizeof(value));
    }

    void appendSubRecord(std::vector<char>& output, std::uint32_t type, const std::vector<char>& data)
    {
        append(output, type);
        append(output, static_cast<std::uint16_t>(data.size()));
        output.insert(output.end(), data.begin(), data.end());
    }

    void appendRecord(
        std::vector<char>& output, std::uint32_t type, std::uint32_t flags, std::uint32_t id,
        const std::vector<char>& data)
    {
        append(output, type);
        append(output, static_cast<std::uint32_t>(data.size()));
        append(output, flags);
        append(output, id);
        append(output, std::uint32_t{ 0 });
        output.insert(output.end(), data.begin(), data.end());
    }

    std::vector<char> makeMapMarkerPlugin()
    {
        std::vector<char> output;
        std::vector<char> header;
        std::vector<char> hedr;
        append(hedr, 1.f);
        append(hedr, std::int32_t{ 1 });
        append(hedr, std::uint32_t{ 0x800 });
        appendSubRecord(header, ESM::fourCC("HEDR"), hedr);
        appendRecord(output, ESM4::REC_TES4, ESM4::Rec_ESM, 0, header);

        std::vector<char> marker;
        appendSubRecord(marker, ESM::fourCC("EDID"), { 'M', 'a', 'r', 'k', 'e', 'r', 0 });
        appendSubRecord(marker, ESM::fourCC("FULL"), { 'T', 'e', 's', 't', ' ', 'C', 'a', 'v', 'e', 0 });
        appendSubRecord(marker, ESM::fourCC("NAME"), { 0x10, 0, 0, 0 });
        appendSubRecord(marker, ESM::fourCC("XMRK"), {});
        appendSubRecord(marker, ESM::fourCC("FNAM"), { 0x03 });
        appendSubRecord(marker, ESM::fourCC("TNAM"), { ESM4::Map_Cave, 0 });
        appendRecord(output, ESM4::REC_REFR, 0, 0x1234, marker);
        return output;
    }
}

TEST(ESM4Reference, LoadsOblivionMapMarkerVisibilityAndType)
{
    const std::vector<char> data = makeMapMarkerPlugin();
    auto stream = std::make_unique<std::stringstream>(
        std::string(data.begin(), data.end()), std::ios::in | std::ios::binary);
    const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
    ESM4::Reader reader(std::move(stream), "memory.esm", nullptr, &encoder, true);

    ESM4::Reference marker;
    ESM4::ReaderUtils::readAll(reader, [&](ESM4::Reader& value) {
        if (value.hdr().record.typeId != ESM4::REC_REFR)
            return true;
        value.getRecordData();
        marker.load(value);
        return true;
    }, [](ESM4::Reader&) {});

    EXPECT_TRUE(marker.mIsMapMarker);
    EXPECT_EQ(marker.mMapMarkerFlags, 0x03);
    EXPECT_EQ(marker.mMapMarker, ESM4::Map_Cave);
    EXPECT_EQ(marker.mFullName, "Test Cave");
}

namespace
{
    ESM4::Reference loadOwnership(std::vector<char> payload, bool compressed = false,
        float version = 1.f, ESM4::Reference* reused = nullptr)
    {
        std::vector<char> output, header, hedr;
        append(hedr, version);
        append(hedr, std::int32_t{ 1 });
        append(hedr, std::uint32_t{ 0x800 });
        appendSubRecord(header, ESM::fourCC("HEDR"), hedr);
        appendRecord(output, ESM4::REC_TES4, ESM4::Rec_ESM, 0, header);
        std::uint32_t flags = 0;
        if (compressed)
        {
            uLongf size = compressBound(payload.size());
            std::vector<char> packed;
            append(packed, static_cast<std::uint32_t>(payload.size()));
            packed.resize(size + 4);
            if (compress(reinterpret_cast<Bytef*>(packed.data() + 4), &size,
                    reinterpret_cast<const Bytef*>(payload.data()), payload.size()) != Z_OK)
                throw std::runtime_error("test compression failed");
            packed.resize(size + 4);
            payload = std::move(packed);
            flags = ESM4::Rec_Compressed;
        }
        appendRecord(output, ESM4::REC_REFR, flags, 0x1234, payload);
        auto stream = std::make_unique<std::stringstream>(
            std::string(output.begin(), output.end()), std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "ownership.esm", nullptr, &encoder, true);
        ESM4::Reference local{};
        auto& result = reused ? *reused : local;
        ESM4::ReaderUtils::readAll(reader, [&](ESM4::Reader& value) {
            if (value.hdr().record.typeId != ESM4::REC_REFR)
                return true;
            value.getRecordData();
            result.load(value);
            return true;
        }, [](ESM4::Reader&) {});
        return result;
    }

    std::vector<char> ownershipPayload(std::int32_t rank)
    {
        std::vector<char> payload, data;
        append(data, rank);
        appendSubRecord(payload, ESM::fourCC("XRNK"), data);
        appendSubRecord(payload, ESM::fourCC("XOWN"), { 0x42, 0, 0, 0 });
        appendSubRecord(payload, ESM::fourCC("XGLB"), { 0x43, 0, 0, 0 });
        return payload;
    }
}

TEST(ESM4ReferenceOwnership, PreservesAbsentAndEverySignedRankBoundary)
{
    EXPECT_FALSE(loadOwnership({}).mFactionRank);
    for (const auto rank : {std::numeric_limits<std::int32_t>::min(), -2, -1, 0, 1, 9,
                            std::numeric_limits<std::int32_t>::max()})
        for (bool compressed : {false, true})
            for (float version : {.8f, 1.f})
            {
                SCOPED_TRACE(rank);
                SCOPED_TRACE(compressed);
                SCOPED_TRACE(version);
                const auto reference = loadOwnership(ownershipPayload(rank), compressed, version);
                ASSERT_TRUE(reference.mFactionRank);
                EXPECT_EQ(*reference.mFactionRank, rank);
                EXPECT_EQ(reference.mOwner, ESM::FormId::fromUint32(0x42));
                EXPECT_EQ(reference.mGlobal, ESM::FormId::fromUint32(0x43));
                EXPECT_EQ(reference.mFormKey.serialize(), "content:ownership.esm:001234");
            }
}

TEST(ESM4ReferenceOwnership, RejectsMalformedOwnershipFieldLengths)
{
    for (const auto tag : {ESM::fourCC("XOWN"), ESM::fourCC("XGLB"), ESM::fourCC("XRNK")})
        for (std::size_t size : {0u, 1u, 3u, 5u, 8u, 12u})
            for (bool compressed : {false, true})
            {
                SCOPED_TRACE(tag);
                SCOPED_TRACE(size);
                SCOPED_TRACE(compressed);
                std::vector<char> payload;
                appendSubRecord(payload, tag, std::vector<char>(size, 0));
                EXPECT_THROW(loadOwnership(payload, compressed), std::runtime_error);
            }
}

TEST(ESM4ReferenceOwnership, RejectsDuplicateFieldsIncludingZeroAndMinusOne)
{
    for (const auto tag : {ESM::fourCC("XOWN"), ESM::fourCC("XGLB"), ESM::fourCC("XRNK")})
        for (std::uint32_t first : {0u, 0xffffffffu, 9u})
            for (std::uint32_t last : {0u, 0xffffffffu, 9u})
                for (bool compressed : {false, true})
                {
                    std::vector<char> payload, data;
                    append(data, first);
                    appendSubRecord(payload, tag, data);
                    data.clear(); append(data, last);
                    appendSubRecord(payload, tag, data);
                    EXPECT_THROW(loadOwnership(payload, compressed), std::runtime_error);
                }
}

TEST(ESM4ReferenceOwnership, ClearsOwnershipExtrasWhenLoadingAnOverrideIntoReusedRecord)
{
    ESM4::Reference reference{};
    loadOwnership(ownershipPayload(-1), false, 1.f, &reference);
    ASSERT_TRUE(reference.mFactionRank);
    ASSERT_FALSE(reference.mOwner.isZeroOrUnset());
    ASSERT_FALSE(reference.mGlobal.isZeroOrUnset());
    loadOwnership({}, false, 1.f, &reference);
    EXPECT_FALSE(reference.mFactionRank);
    EXPECT_TRUE(reference.mOwner.isZeroOrUnset());
    EXPECT_TRUE(reference.mGlobal.isZeroOrUnset());
    loadOwnership(ownershipPayload(-2), true, .8f, &reference);
    EXPECT_EQ(reference.mFactionRank, -2);
}
