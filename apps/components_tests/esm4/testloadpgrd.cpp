#include <components/esm/fourcc.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadpgrd.hpp>
#include <components/esm4/reader.hpp>
#include <components/toutf8/toutf8.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <memory>
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

    void appendRecord(std::vector<char>& output, std::uint32_t type, std::uint32_t flags,
        std::uint32_t id, const std::vector<char>& payload)
    {
        append(output, type);
        append(output, static_cast<std::uint32_t>(payload.size()));
        append(output, flags);
        append(output, id);
        append(output, std::uint32_t{ 0 });
        output.insert(output.end(), payload.begin(), payload.end());
    }

    template <class T>
    std::vector<char> bytes(const T& value)
    {
        const char* begin = reinterpret_cast<const char*>(&value);
        return { begin, begin + sizeof(value) };
    }

    std::vector<char> makeHeader()
    {
        std::vector<char> payload;
        std::vector<char> hedr;
        append(hedr, 1.0f);
        append(hedr, std::int32_t{ 1 });
        append(hedr, std::uint32_t{ 0x800 });
        appendSubRecord(payload, ESM::fourCC("HEDR"), hedr);
        std::vector<char> result;
        appendRecord(result, ESM4::REC_TES4, ESM4::Rec_ESM, 0, payload);
        return result;
    }

    std::vector<char> makePathgridPayload()
    {
        std::vector<char> payload;
        appendSubRecord(payload, ESM::fourCC("DATA"), bytes(std::int16_t{ 3 }));
        const std::vector<ESM4::Pathgrid::PGRP> points = {
            { 0.f, 1.f, 2.f, 1, 4, 0x1111 },
            { 10.f, 11.f, 12.f, 1, 8, 0x2222 },
            { 20.f, 21.f, 22.f, 0, 16, 0x3333 },
        };
        std::vector<char> pointData;
        for (const auto& point : points)
        {
            const auto value = bytes(point);
            pointData.insert(pointData.end(), value.begin(), value.end());
        }
        appendSubRecord(payload, ESM::fourCC("PGRP"), pointData);
        std::vector<char> links;
        append(links, std::int16_t{ 1 });
        append(links, std::int16_t{ 2 });
        appendSubRecord(payload, ESM::fourCC("PGRR"), links);

        ESM4::Pathgrid::PGRI foreign{ 1, 0xabcd, 30.f, 31.f, 32.f };
        appendSubRecord(payload, ESM::fourCC("PGRI"), bytes(foreign));

        std::vector<char> object;
        append(object, ESM::FormId32{ 0x1234 });
        append(object, std::int32_t{ 0 });
        append(object, std::int32_t{ -1 });
        appendSubRecord(payload, ESM::fourCC("PGRL"), object);
        appendSubRecord(payload, ESM::fourCC("PGAG"), { 0x01, 0x02, 0x03 });
        return payload;
    }

    ESM4::Pathgrid loadPathgrid(const std::vector<char>& plugin)
    {
        auto stream = std::make_unique<std::stringstream>(
            std::string(plugin.begin(), plugin.end()), std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "memory.esm", nullptr, &encoder, true);
        EXPECT_TRUE(reader.getRecordHeader());
        EXPECT_EQ(reader.hdr().record.typeId, ESM4::REC_PGRD);
        reader.getRecordData();
        ESM4::Pathgrid result;
        result.load(reader);
        return result;
    }

    std::vector<char> makePathgridPlugin(const std::vector<char>& payload = makePathgridPayload())
    {
        std::vector<char> result = makeHeader();
        appendRecord(result, ESM4::REC_PGRD, 0, 0x2345, payload);
        return result;
    }
}

TEST(ESM4LoadPathgrid, PreservesPointsLinksForeignObjectsAndGraphAttributes)
{
    const ESM4::Pathgrid pathgrid = loadPathgrid(makePathgridPlugin());
    EXPECT_EQ(pathgrid.mFormKey, ESM::FormKey::content("memory.esm", 0x2345));
    ASSERT_EQ(pathgrid.mNodes.size(), 3u);
    EXPECT_EQ(pathgrid.mNodes[0].priority, 4);
    EXPECT_EQ(pathgrid.mNodes[0].unknown, 0x1111);
    ASSERT_EQ(pathgrid.mLinks.size(), 2u);
    EXPECT_EQ(pathgrid.mLinks[0].startNode, 0);
    EXPECT_EQ(pathgrid.mLinks[0].endNode, 1);
    ASSERT_EQ(pathgrid.mForeign.size(), 1u);
    EXPECT_EQ(pathgrid.mForeign[0].localNode, 1);
    EXPECT_FLOAT_EQ(pathgrid.mForeign[0].z, 32.f);
    ASSERT_EQ(pathgrid.mObjects.size(), 1u);
    EXPECT_EQ(pathgrid.mObjects[0].objectKey, ESM::FormKey::content("memory.esm", 0x1234));
    EXPECT_EQ(pathgrid.mObjects[0].linkedNodes, (std::vector<std::int32_t>{ 0, -1 }));
    EXPECT_EQ(pathgrid.mGraphAttributes, (std::vector<std::uint8_t>{ 1, 2, 3 }));
}

TEST(ESM4LoadPathgrid, AcceptsSentinelsAndRejectsEveryInvalidEndpointClass)
{
    std::vector<char> noPgrr;
    appendSubRecord(noPgrr, ESM::fourCC("DATA"), bytes(std::int16_t{ 1 }));
    ESM4::Pathgrid::PGRP point{ 0.f, 0.f, 0.f, 0, 0, 0 };
    appendSubRecord(noPgrr, ESM::fourCC("PGRP"), bytes(point));
    EXPECT_NO_THROW(loadPathgrid(makePathgridPlugin(noPgrr)));

    std::vector<char> invalid = makePathgridPayload();
    // PGRL is the final subrecord in the fixture; replace its first node.
    const std::size_t pgrlNodeOffset = invalid.size() - 3 - 6 - 8;
    std::int32_t badNode = 99;
    std::memcpy(invalid.data() + pgrlNodeOffset, &badNode, sizeof(badNode));
    EXPECT_THROW(loadPathgrid(makePathgridPlugin(invalid)), std::runtime_error);

    std::vector<char> missingPgrr;
    appendSubRecord(missingPgrr, ESM::fourCC("DATA"), bytes(std::int16_t{ 1 }));
    point.numLinks = 1;
    appendSubRecord(missingPgrr, ESM::fourCC("PGRP"), bytes(point));
    EXPECT_THROW(loadPathgrid(makePathgridPlugin(missingPgrr)), std::runtime_error);
}

TEST(ESM4LoadPathgrid, RejectsCountAlignmentAndUnknownSubrecords)
{
    std::vector<char> malformed = makePathgridPayload();
    // DATA is the first subrecord. Make it disagree with PGRP.
    malformed[6] = 2;
    EXPECT_THROW(loadPathgrid(makePathgridPlugin(malformed)), std::runtime_error);

    std::vector<char> unknown;
    appendSubRecord(unknown, ESM::fourCC("DATA"), bytes(std::int16_t{ 0 }));
    appendSubRecord(unknown, ESM::fourCC("PGRP"), {});
    appendSubRecord(unknown, ESM::fourCC("ZZZZ"), { 0 });
    EXPECT_THROW(loadPathgrid(makePathgridPlugin(unknown)), std::runtime_error);
}

TEST(ESM4LoadPathgrid, AppendsRepeatedArraySubrecordsWithoutDependingOnTheirOrder)
{
    std::vector<char> payload;
    appendSubRecord(payload, ESM::fourCC("PGRI"), bytes(ESM4::Pathgrid::PGRI{ 0, 0, 1.f, 2.f, 3.f }));
    appendSubRecord(payload, ESM::fourCC("DATA"), bytes(std::int16_t{ 3 }));

    const ESM4::Pathgrid::PGRP first{ 0.f, 1.f, 2.f, 1, 4, 0x1111 };
    const std::vector<ESM4::Pathgrid::PGRP> rest = {
        { 10.f, 11.f, 12.f, 1, 8, 0x2222 },
        { 20.f, 21.f, 22.f, 0, 16, 0x3333 },
    };
    appendSubRecord(payload, ESM::fourCC("PGRP"), bytes(first));
    std::vector<char> restData;
    for (const auto& point : rest)
    {
        const auto value = bytes(point);
        restData.insert(restData.end(), value.begin(), value.end());
    }
    appendSubRecord(payload, ESM::fourCC("PGRP"), restData);
    appendSubRecord(payload, ESM::fourCC("PGRR"), bytes(std::int16_t{ 1 }));
    appendSubRecord(payload, ESM::fourCC("PGRR"), bytes(std::int16_t{ 2 }));
    appendSubRecord(payload, ESM::fourCC("PGRI"), bytes(ESM4::Pathgrid::PGRI{ 2, 0, 4.f, 5.f, 6.f }));

    const ESM4::Pathgrid pathgrid = loadPathgrid(makePathgridPlugin(payload));
    ASSERT_EQ(pathgrid.mNodes.size(), 3u);
    ASSERT_EQ(pathgrid.mLinks.size(), 2u);
    EXPECT_EQ(pathgrid.mLinks[0].endNode, 1);
    EXPECT_EQ(pathgrid.mLinks[1].endNode, 2);
    ASSERT_EQ(pathgrid.mForeign.size(), 2u);
    EXPECT_EQ(pathgrid.mForeign[0].localNode, 0);
    EXPECT_EQ(pathgrid.mForeign[1].localNode, 2);
}
