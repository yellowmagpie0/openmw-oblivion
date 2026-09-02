#include <components/esm/fourcc.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadpack.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/readerutils.hpp>
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

    std::vector<char> makePackagePayload()
    {
        std::vector<char> payload;
        appendSubRecord(payload, ESM::fourCC("EDID"), { 'M', '1', '4', 0 });

        ESM4::AIPackage::PKDT data{};
        data.flags = static_cast<std::uint32_t>(ESM4::PackageFlag::AlwaysRun) | 0x80000000u;
        data.type = static_cast<std::uint8_t>(ESM4::AIPackageType::Travel);
        appendSubRecord(payload, ESM::fourCC("PKDT"), bytes(data));

        ESM4::AIPackage::PSDT schedule{ -1, 7, 0, 22, 4 };
        appendSubRecord(payload, ESM::fourCC("PSDT"), bytes(schedule));

        ESM4::AIPackage::PLDT location{ 5, 12, 128 };
        appendSubRecord(payload, ESM::fourCC("PLDT"), bytes(location));

        // This deliberately differs from PLDT.location. It is the regression
        // that catches reading PTDT through the location discriminant.
        ESM4::AIPackage::PTDT target{ 0, 0x00001234, 96 };
        appendSubRecord(payload, ESM::fourCC("PTDT"), bytes(target));

        ESM4::AIPackage::CTDA condition{};
        condition.condition = 0x67; // OR + run-on-target + global + >=.
        condition.compValue = 3.5f;
        condition.fnIndex = 18; // GetCurrentTime
        condition.param1 = 0x00005678;
        condition.param2 = 0x00009abc;
        condition.unknown4 = 2; // explicit reference run-on
        appendSubRecord(payload, ESM::fourCC("CTDA"), bytes(condition));

        const std::vector<char> conditionBytes = bytes(condition);
        std::vector<char> shortCondition(conditionBytes.begin(), conditionBytes.end() - 4);
        appendSubRecord(payload, ESM::fourCC("CTDT"), shortCondition);
        appendSubRecord(payload, ESM::fourCC("TNAM"), { 1, 2, 3 });
        return payload;
    }

    ESM4::AIPackage loadPackage(const std::vector<char>& plugin)
    {
        auto stream = std::make_unique<std::stringstream>(
            std::string(plugin.begin(), plugin.end()), std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "memory.esm", nullptr, &encoder, true);
        EXPECT_TRUE(reader.getRecordHeader());
        EXPECT_EQ(reader.hdr().record.typeId, ESM4::REC_PACK);
        reader.getRecordData();
        ESM4::AIPackage result;
        result.load(reader);
        return result;
    }

    std::vector<char> makePackagePlugin()
    {
        std::vector<char> result = makeHeader();
        appendRecord(result, ESM4::REC_PACK, 0, 0x1234, makePackagePayload());
        return result;
    }
}

TEST(ESM4LoadPackage, DecodesAllNativeFieldsAndBothConditionLayouts)
{
    const ESM4::AIPackage package = loadPackage(makePackagePlugin());
    EXPECT_EQ(package.mFormKey, ESM::FormKey::content("memory.esm", 0x1234));
    EXPECT_EQ(package.mEditorId, "M14");
    EXPECT_EQ(package.mPackageType, ESM4::AIPackageType::Travel);
    EXPECT_TRUE(package.mPackageFlags.has(ESM4::PackageFlag::AlwaysRun));
    EXPECT_EQ(package.mPackageFlags.mReserved, 0x80000000u);
    EXPECT_EQ(package.mScheduleData.mStartHour, 22);
    EXPECT_EQ(package.mScheduleData.mDuration, 4);
    EXPECT_EQ(package.mLocationData.mKind, ESM4::PackageLocationKind::ObjectType);
    EXPECT_EQ(package.mLocationData.mObjectType, 12u);
    EXPECT_EQ(package.mTargetData.mKind, ESM4::PackageTargetKind::SpecificReference);
    EXPECT_EQ(package.mTargetData.mReferenceKey, ESM::FormKey::content("memory.esm", 0x1234));
    ASSERT_EQ(package.mCanonicalConditions.size(), 2u);
    EXPECT_EQ(package.mCanonicalConditions[0].mEncodedSize, 24u);
    EXPECT_EQ(package.mCanonicalConditions[0].mRunOn, ESM4::ConditionRunOn::Reference);
    EXPECT_EQ(package.mCanonicalConditions[1].mEncodedSize, 20u);
    EXPECT_EQ(package.mCanonicalConditions[1].mRunOn, ESM4::ConditionRunOn::Subject);
    ASSERT_EQ(package.mSkippedSubrecords.size(), 1u);
    EXPECT_EQ(package.mSkippedSubrecords.front(), ESM::fourCC("TNAM"));
}

TEST(ESM4LoadPackage, DecodesLegacyShortPackageData)
{
    std::vector<char> payload;
    ESM4::AIPackage::PKDTShort data{ 0x0040, static_cast<std::uint8_t>(ESM4::AIPackageType::Find), 0 };
    appendSubRecord(payload, ESM::fourCC("PKDT"), bytes(data));
    ESM4::AIPackage::PSDT schedule{ -1, -1, 0, -1, 0 };
    appendSubRecord(payload, ESM::fourCC("PSDT"), bytes(schedule));

    std::vector<char> plugin = makeHeader();
    appendRecord(plugin, ESM4::REC_PACK, 0, 0x1235, payload);
    const ESM4::AIPackage package = loadPackage(plugin);
    EXPECT_TRUE(package.mUsedShortPKDT);
    EXPECT_EQ(package.mPackageType, ESM4::AIPackageType::Find);
    EXPECT_TRUE(package.mPackageFlags.has(ESM4::PackageFlag::UnlockDoorsAtStart));
}

TEST(ESM4LoadPackage, RejectsMalformedLayoutsAndDiscriminants)
{
    std::vector<char> plugin = makePackagePlugin();
    // PKDT's subrecord header starts after the 20-byte TES4 record and EDID.
    const std::size_t pkdtSize = 20 + 20 + 6 + 5 + 6;
    plugin[pkdtSize + 4] = 7;
    EXPECT_THROW(loadPackage(plugin), std::runtime_error);

    std::vector<char> invalid = makeHeader();
    std::vector<char> payload;
    ESM4::AIPackage::PKDT data{};
    data.type = static_cast<std::uint8_t>(ESM4::AIPackageType::Find);
    appendSubRecord(payload, ESM::fourCC("PKDT"), bytes(data));
    appendSubRecord(payload, ESM::fourCC("PSDT"), { 0, 0, 0, 0, 0, 0, 0 });
    appendRecord(invalid, ESM4::REC_PACK, 0, 0x1236, payload);
    EXPECT_THROW(loadPackage(invalid), std::runtime_error);
}
