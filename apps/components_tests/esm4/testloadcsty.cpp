#include <components/esm4/loadcsty.hpp>
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
    void number(std::vector<std::uint8_t>& data, std::size_t offset, float value)
    {
        put(data, offset, std::bit_cast<std::uint32_t>(value));
    }
    std::vector<std::uint8_t> standard()
    {
        std::vector<std::uint8_t> data(124, 0);
        data[0] = 75;
        data[1] = 50;
        for (const auto offset : { 4, 12, 20, 28, 72 })
        {
            number(data, offset, 0.5f);
            number(data, offset + 4, 1.5f);
        }
        data[36] = 30;
        data[37] = 40;
        data[52] = 25;
        data[64] = 100; // Directional weights need not sum to 100.
        data[65] = 100;
        data[80] = 0xff;
        number(data, 84, 1.f);
        number(data, 88, 2.f);
        number(data, 92, 250.f);
        number(data, 96, 1000.f);
        number(data, 100, 325.f);
        number(data, 104, 500.f);
        number(data, 108, 325.f);
        data[112] = 25;
        number(data, 116, 1.f);
        put(data, 120, 1);
        // Padding in shipped records often contains editor memory, not zero.
        data[2] = 0xcd;
        data[53] = 0xff;
        return data;
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
    ESM4::CombatStyle load(std::vector<std::uint8_t> payload, bool compressed = false,
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
        record(data, "CSTY", flags, 0x800, payload);
        auto stream = std::make_unique<std::stringstream>(std::string(data.begin(), data.end()),
            std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "fixture.esm", nullptr, &encoder, true);
        if (!reader.getRecordHeader())
            throw std::runtime_error("test record missing");
        reader.getRecordData();
        ESM4::CombatStyle result;
        result.load(reader);
        return result;
    }
}

TEST(ESM4CombatStyle, DecodesAllHistoricalSizesWithoutInventingTailDefaults)
{
    for (const std::size_t size : { 84, 92, 104, 112, 120, 124 })
    {
        SCOPED_TRACE(size);
        auto data = standard();
        data.resize(size);
        const auto value = ESM4::decodeCombatStyleStandard(data);
        EXPECT_EQ(value.mDodgeChance, 75);
        EXPECT_EQ(value.mBlockChance, 30);
        EXPECT_EQ(value.mAttackChance, 40);
        EXPECT_EQ(value.mDodgeLeftRight.mMaximum, 1.5f);
        EXPECT_EQ(value.mPowerAttackDirections[1], 100);
        EXPECT_TRUE(value.has(ESM4::CombatStyleFlag::PreferRanged));
        EXPECT_EQ(value.mRangeMultipliers.has_value(), size >= 92);
        EXPECT_EQ(value.mSwitchDistances.has_value(), size >= 104);
        EXPECT_EQ(value.mBuffStandoff.has_value(), size >= 104);
        EXPECT_EQ(value.mRangedGroupStandoff.has_value(), size >= 112);
        EXPECT_EQ(value.mRushChance.has_value(), size >= 120);
        EXPECT_EQ(value.mRushDistanceMultiplier.has_value(), size >= 120);
        EXPECT_EQ(value.mDoNotAcquire.has_value(), size == 124);
        if (value.mSwitchDistances)
        {
            EXPECT_EQ((*value.mSwitchDistances)[1], 1000);
        }
        if (value.mDoNotAcquire)
        {
            EXPECT_TRUE(*value.mDoNotAcquire);
        }
    }
}

TEST(ESM4CombatStyle, RejectsEveryOtherLengthAndNonfiniteOrOutOfDomainFields)
{
    for (std::size_t size = 0; size < 140; ++size)
    {
        if (size == 84 || size == 92 || size == 104 || size == 112 || size == 120 || size == 124)
            continue;
        EXPECT_THROW(ESM4::decodeCombatStyleStandard(std::vector<std::uint8_t>(size)), std::runtime_error);
    }
    for (const auto offset : { 0, 1, 36, 37, 52, 64, 65, 66, 67, 68, 81, 112 })
    {
        auto data = standard();
        data[offset] = 101;
        EXPECT_THROW(ESM4::decodeCombatStyleStandard(data), std::runtime_error);
    }
    for (const float invalid : { -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
    {
        auto data = standard();
        number(data, 84, invalid);
        EXPECT_THROW(ESM4::decodeCombatStyleStandard(data), std::runtime_error);
    }
    auto data = standard();
    number(data, 4, 2.f);
    EXPECT_THROW(ESM4::decodeCombatStyleStandard(data), std::runtime_error);
    data = standard();
    put(data, 120, 2);
    EXPECT_THROW(ESM4::decodeCombatStyleStandard(data), std::runtime_error);
}

TEST(ESM4CombatStyle, AttackBonusesRemainSignedRatherThanInventingClamps)
{
    auto data = standard();
    number(data, 40, -5.f);
    number(data, 60, -10.f);
    const auto value = ESM4::decodeCombatStyleStandard(data);
    EXPECT_EQ(value.mAttackRecoilBonus, -5.f);
    EXPECT_EQ(value.mPowerAttackUnconsciousBonus, -10.f);
}

TEST(ESM4CombatStyle, AdvancedSignedValuesAreFiniteAndKeepTheirFieldOrder)
{
    std::vector<std::uint8_t> data(84);
    for (int i = 0; i < 21; ++i)
        number(data, i * 4, static_cast<float>(i - 10));
    const auto value = ESM4::decodeCombatStyleAdvanced(data);
    EXPECT_EQ(value.mDodgeFatigueMultiplier, -10.f);
    EXPECT_EQ(value.mBlockSkillMultiplier, 0.f);
    EXPECT_EQ(value.mPowerAttackFatigueMultiplier, 10.f);
    for (int i = 0; i < 21; ++i)
    {
        auto invalid = data;
        number(invalid, i * 4, std::numeric_limits<float>::quiet_NaN());
        EXPECT_THROW(ESM4::decodeCombatStyleAdvanced(invalid), std::runtime_error);
    }
    data.push_back(0);
    EXPECT_THROW(ESM4::decodeCombatStyleAdvanced(data), std::runtime_error);
    data.resize(80);
    EXPECT_THROW(ESM4::decodeCombatStyleAdvanced(data), std::runtime_error);
}

TEST(ESM4CombatStyle, LoadsCompressedRecordsAndRetainsPaddingAndUnknownPayloads)
{
    std::vector<std::uint8_t> payload;
    sub(payload, "EDID", { 'M', '1', '5', 0 });
    sub(payload, "CSTD", standard());
    sub(payload, "CSAD", std::vector<std::uint8_t>(84));
    sub(payload, "ZZZZ", { 1, 2, 3 });
    for (bool compressed : { false, true })
    {
        const auto value = load(payload, compressed);
        ASSERT_TRUE(value.mStandard);
        ASSERT_TRUE(value.mAdvanced);
        EXPECT_EQ(value.mEditorId, "M15");
        EXPECT_EQ(value.mFormKey.serialize(), "content:fixture.esm:000800");
        ASSERT_EQ(value.mSubRecords.size(), 4);
        EXPECT_EQ(value.mSubRecords[1].mData, standard());
        EXPECT_EQ(value.mSubRecords[3].mData, (std::vector<std::uint8_t>{ 1, 2, 3 }));
    }
}

TEST(ESM4CombatStyle, MissingDuplicateTruncatedAndLaterGameLayoutsFail)
{
    std::vector<std::uint8_t> payload;
    sub(payload, "EDID", { 'M', '1', '5', 0 });
    EXPECT_THROW(load(payload), std::runtime_error);
    EXPECT_FALSE(load(payload, false, ESM4::Rec_Deleted).mStandard);
    sub(payload, "CSTD", standard());
    EXPECT_THROW(load(payload, false, 0, 1.7f), std::runtime_error);
    auto truncated = payload;
    truncated.pop_back();
    EXPECT_THROW(load(truncated), std::runtime_error);
    auto duplicate = payload;
    sub(duplicate, "CSTD", standard());
    EXPECT_THROW(load(duplicate), std::runtime_error);
    sub(payload, "CSAD", std::vector<std::uint8_t>(84));
    sub(payload, "CSAD", std::vector<std::uint8_t>(84));
    EXPECT_THROW(load(payload), std::runtime_error);
}
