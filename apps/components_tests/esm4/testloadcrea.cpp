#include <components/esm4/loadcrea.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/reader.hpp>
#include <components/toutf8/toutf8.hpp>

#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <vector>

namespace
{
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
    ESM4::Creature load(const std::vector<std::uint8_t>& payload)
    {
        std::vector<std::uint8_t> header, hedr, data;
        append(hedr, 1.f);
        append(hedr, std::uint32_t{1});
        append(hedr, std::uint32_t{0x800});
        sub(header, "HEDR", hedr);
        record(data, "TES4", ESM4::Rec_ESM, 0, header);
        record(data, "CREA", 0, 0x800, payload);
        auto stream = std::make_unique<std::stringstream>(std::string(data.begin(), data.end()),
            std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "fixture.esm", nullptr, &encoder, true);
        if (!reader.getRecordHeader())
            throw std::runtime_error("missing fixture record");
        reader.getRecordData();
        ESM4::Creature result{};
        result.load(reader);
        return result;
    }
}

TEST(ESM4CreatureCombat, RejectsMalformedReachAndOrphanSoundChance)
{
    for (auto size : {0, 2, 4})
    {
        std::vector<std::uint8_t> payload;
        sub(payload, "RNAM", std::vector<std::uint8_t>(size));
        EXPECT_THROW(load(payload), std::runtime_error);
    }
    std::vector<std::uint8_t> payload;
    sub(payload, "CSDC", {100});
    EXPECT_THROW(load(payload), std::runtime_error);
}

TEST(ESM4CreatureCombat, PreservesReachPaddingAndAllSoundSlots)
{
    std::vector<std::uint8_t> payload;
    std::vector<std::uint8_t> stats(20, 0);
    stats[0] = 2;
    stats[4] = 3;
    stats[5] = 0xcd; // Padding must not become the soul value.
    stats[10] = 15;
    sub(payload, "DATA", stats);
    sub(payload, "RNAM", {48});
    sub(payload, "CSCR", {0x20, 0x08, 0, 0});
    for (const auto type : {6, 8})
    {
        sub(payload, "CSDT", {static_cast<std::uint8_t>(type), 0, 0, 0});
        for (const auto id : {0x801, 0x802})
        {
            std::vector<std::uint8_t> sound;
            append(sound, static_cast<std::uint32_t>(id));
            sub(payload, "CSDI", sound);
            sub(payload, "CSDC", {50});
        }
    }
    const auto value = load(payload);
    EXPECT_EQ(value.mAttackReach, 48);
    EXPECT_EQ(value.mData.soul, 3);
    EXPECT_EQ(value.mData.soulPadding, 0xcd);
    EXPECT_EQ(value.mData.damage, 15);
    EXPECT_EQ(value.mSoundBaseKey.serialize(), "content:fixture.esm:000820");
    ASSERT_EQ(value.mSounds.size(), 4);
    EXPECT_EQ(value.mSounds[0].mType, ESM4::Creature::SoundType::Attack);
    EXPECT_EQ(value.mSounds[2].mType, ESM4::Creature::SoundType::Death);
    EXPECT_EQ(value.mSounds[0].mKey.serialize(), "content:fixture.esm:000801");
    EXPECT_EQ(value.mSounds[1].mKey.serialize(), "content:fixture.esm:000802");
    EXPECT_EQ(value.mSounds[3].mChance, 50);
    EXPECT_FALSE(load({}).mAttackReach);
    EXPECT_TRUE(load({}).mSounds.empty());
}

TEST(ESM4CreatureCombat, RejectsInvalidNativeStatsAndSoundSequences)
{
    for (unsigned size = 0; size < 30; ++size)
    {
        if (size == 20)
            continue;
        std::vector<std::uint8_t> payload;
        sub(payload, "DATA", std::vector<std::uint8_t>(size));
        EXPECT_THROW(load(payload), std::runtime_error) << size;
    }
    for (const auto offset : {0, 4})
    {
        std::vector<std::uint8_t> payload, stats(20, 0);
        stats[offset] = 6;
        sub(payload, "DATA", stats);
        EXPECT_THROW(load(payload), std::runtime_error);
    }
    for (const auto chance : {0, 100, 101, 255})
    {
        std::vector<std::uint8_t> payload;
        sub(payload, "CSDT", {6, 0, 0, 0});
        sub(payload, "CSDI", {1, 8, 0, 0});
        EXPECT_THROW(load(payload), std::runtime_error); // Missing final chance.
        sub(payload, "CSDC", {static_cast<std::uint8_t>(chance)});
        if (chance <= 100)
            EXPECT_NO_THROW(load(payload));
        else
            EXPECT_THROW(load(payload), std::runtime_error);
    }
    for (const auto type : {10, 255})
    {
        std::vector<std::uint8_t> payload;
        sub(payload, "CSDT", {static_cast<std::uint8_t>(type), 0, 0, 0});
        EXPECT_THROW(load(payload), std::runtime_error);
    }
    std::vector<std::uint8_t> duplicate;
    sub(duplicate, "RNAM", {0});
    sub(duplicate, "RNAM", {0});
    EXPECT_THROW(load(duplicate), std::runtime_error);
    std::vector<std::uint8_t> unordered;
    sub(unordered, "CSDI", {1, 8, 0, 0});
    EXPECT_THROW(load(unordered), std::runtime_error);
    sub(unordered, "CSDT", {6, 0, 0, 0});
    EXPECT_THROW(load(unordered), std::runtime_error);
}
