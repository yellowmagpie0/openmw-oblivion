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
