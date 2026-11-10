#include <components/esm4/common.hpp>
#include <components/esm4/loadfurn.hpp>
#include <components/esm4/reader.hpp>
#include <components/toutf8/toutf8.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <vector>

namespace
{
    template <typename T>
    void append(std::vector<std::uint8_t>& data, T value)
    {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        data.insert(data.end(), bytes, bytes + sizeof(T));
    }

    void subrecord(std::vector<std::uint8_t>& data, const char* type, const std::vector<std::uint8_t>& payload)
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

    ESM4::Furniture load(const std::vector<std::uint8_t>& payload, float version = 1.f)
    {
        std::vector<std::uint8_t> header, hedr, data;
        append(hedr, version);
        append(hedr, std::uint32_t{1});
        append(hedr, std::uint32_t{0x800});
        subrecord(header, "HEDR", hedr);
        record(data, "TES4", ESM4::Rec_ESM, 0, header);
        record(data, "FURN", 0, 0x800, payload);
        auto stream = std::make_unique<std::stringstream>(
            std::string(data.begin(), data.end()), std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "furniture.esm", nullptr, &encoder, true);
        if (!reader.getRecordHeader())
            throw std::runtime_error("missing furniture record");
        reader.getRecordData();
        ESM4::Furniture result{};
        result.mActiveMarkerFlags = 0x80000001u;
        result.load(reader);
        return result;
    }
}

TEST(ESM4Furniture, MissingMarkersDoNotRetainSleepBit)
{
    EXPECT_EQ(load({}).mActiveMarkerFlags, 0u);
}

TEST(ESM4Furniture, PreservesOriginalBedrollAndBenchMarkers)
{
    // Original BedrollSideEntry, BedrollCrawlEntry, and ARBench01 MNAM.
    for (const auto flags : {0x80000001u, 0x80000002u, 0x400000e0u})
    {
        std::vector<std::uint8_t> payload, markers;
        append(markers, flags);
        subrecord(payload, "MNAM", markers);
        for (const float version : {0.8f, 1.f})
            EXPECT_EQ(load(payload, version).mActiveMarkerFlags, flags);
    }
}

TEST(ESM4Furniture, RejectsMalformedNativeMarkerSizes)
{
    for (const auto size : {0, 1, 2, 3, 5, 8})
    {
        std::vector<std::uint8_t> payload;
        subrecord(payload, "MNAM", std::vector<std::uint8_t>(size));
        EXPECT_THROW(load(payload), std::runtime_error);
    }
}
