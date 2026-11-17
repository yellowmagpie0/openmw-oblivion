#include <components/esm4/loadefsh.hpp>
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
    std::vector<std::uint8_t> shader()
    {
        std::vector<std::uint8_t> data(224);
        for (std::size_t offset = 16; offset < data.size(); offset += 4)
            number(data, offset, static_cast<float>(offset) / 8.f);
        data[0] = 0x11;
        data[1] = 0xcd; // Authored padding must be retained.
        put(data, 4, 5); put(data, 8, 1); put(data, 12, 3);
        put(data, 92, 6);
        put(data, 96, 5); put(data, 100, 1); put(data, 104, 4); put(data, 108, 2);
        number(data, 144, -15.f);
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
    ESM4::EffectShader load(std::vector<std::uint8_t> payload, bool compressed = false,
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
        record(data, "EFSH", flags, 0x800, payload);
        auto stream = std::make_unique<std::stringstream>(std::string(data.begin(), data.end()),
            std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "fixture.esm", nullptr, &encoder, true);
        if (!reader.getRecordHeader())
            throw std::runtime_error("test record missing");
        reader.getRecordData();
        ESM4::EffectShader result;
        result.load(reader);
        return result;
    }
}

TEST(ESM4EffectShader, DecodesOriginalLayoutsAndKeepsLegacyParticlesAbsent)
{
    auto data = shader();
    const auto value = ESM4::decodeEffectShaderData(data);
    EXPECT_EQ(value.mFlags, 0x11);
    EXPECT_EQ(value.mSourceBlend, 5);
    EXPECT_EQ(value.mDestinationBlend, 6);
    EXPECT_EQ(value.mFill.mFadeIn, 2.5f);
    EXPECT_EQ(value.mTextureSpeed[1], 6.f);
    EXPECT_EQ(value.mEdge.mPulseFrequency, 10.f);
    ASSERT_TRUE(value.mParticles);
    EXPECT_EQ(value.mParticles->mDestinationBlend, 2);
    EXPECT_EQ(value.mParticles->mBirthRampUp, 14.f);
    EXPECT_EQ(value.mParticles->mNormalAcceleration, -15.f);
    EXPECT_EQ(value.mParticles->mAcceleration[2], 21.f);
    EXPECT_EQ(value.mParticles->mScaleTime[1], 23.f);
    EXPECT_EQ(value.mParticles->mColorTime[2], 27.5f);
    data.resize(96);
    const auto legacy = ESM4::decodeEffectShaderData(data);
    EXPECT_FALSE(legacy.mParticles);
    EXPECT_EQ(legacy.mEdge.mPulseFrequency, value.mEdge.mPulseFrequency);
}

TEST(ESM4EffectShader, RejectsWrongLengthsAndNonfiniteFieldsWithoutClampingSignedMotion)
{
    for (std::size_t size = 0; size <= 225; ++size)
    {
        if (size == 96 || size == 224) continue;
        SCOPED_TRACE(size);
        EXPECT_THROW(ESM4::decodeEffectShaderData(std::vector<std::uint8_t>(size)), std::runtime_error);
    }
    for (const auto [begin, end] : { std::pair{20, 52}, {60, 92}, {112, 188}, {200, 224} })
        for (int offset = begin; offset < end; offset += 4)
        {
            SCOPED_TRACE(offset);
            auto invalid = shader();
            number(invalid, offset, std::numeric_limits<float>::infinity());
            EXPECT_THROW(ESM4::decodeEffectShaderData(invalid), std::runtime_error);
        }
    auto invalid = shader();
    number(invalid, 52, std::numeric_limits<float>::quiet_NaN());
    EXPECT_THROW(ESM4::decodeEffectShaderData(invalid), std::runtime_error);
}

TEST(ESM4EffectShader, LoadsCompressedTexturesAndRetainsUnknownAndExtendedPayloads)
{
    std::vector<std::uint8_t> payload;
    sub(payload, "EDID", {'F','i','r','e',0});
    sub(payload, "ICON", {'f','i','l','l',0});
    sub(payload, "ICO2", {'f','l','a','m','e',0});
    sub(payload, "DATA", shader());
    // XXXX overrides the following short subrecord size.
    std::vector<std::uint8_t> length(4); put(length, 0, 3);
    sub(payload, "XXXX", length);
    sub(payload, "ZZZZ", {1,2,3});
    for (bool compressed : {false, true})
    {
        const auto value = load(payload, compressed);
        EXPECT_EQ(value.mEditorId, "Fire");
        EXPECT_EQ(value.mFillTexture, "fill");
        EXPECT_EQ(value.mParticleTexture, "flame");
        EXPECT_EQ(value.mFormKey.serialize(), "content:fixture.esm:000800");
        ASSERT_TRUE(value.mData);
        ASSERT_EQ(value.mSubRecords.size(), 5);
        EXPECT_EQ(value.mSubRecords[3].mData, shader());
        EXPECT_TRUE(value.mSubRecords[4].mExtended);
        EXPECT_EQ(value.mSubRecords[4].mData, (std::vector<std::uint8_t>{1,2,3}));
    }
}

TEST(ESM4EffectShader, MissingDuplicateMalformedTexturesAndLaterGameLayoutsFail)
{
    std::vector<std::uint8_t> payload;
    EXPECT_THROW(load(payload), std::runtime_error);
    EXPECT_FALSE(load(payload, false, ESM4::Rec_Deleted).mData);
    sub(payload, "DATA", shader());
    EXPECT_THROW(load(payload, false, 0, 1.7f), std::runtime_error);
    auto duplicate = payload; sub(duplicate, "DATA", shader());
    EXPECT_THROW(load(duplicate), std::runtime_error);
    for (const auto* tag : {"ICON", "ICO2"})
    {
        auto bad = payload; sub(bad, tag, {'x'});
        EXPECT_THROW(load(bad), std::runtime_error);
        bad = payload; sub(bad, tag, {'x',0,'y',0});
        EXPECT_THROW(load(bad), std::runtime_error);
        bad = payload; sub(bad, tag, {0}); sub(bad, tag, {0});
        EXPECT_THROW(load(bad), std::runtime_error);
    }
    payload.pop_back();
    EXPECT_THROW(load(payload), std::runtime_error);
}
