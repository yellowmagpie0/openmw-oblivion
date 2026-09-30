#include <components/esm4/loadspel.hpp>
#include <components/esm4/loadrace.hpp>
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
    template <typename T = ESM4::Spell> T load(std::vector<std::uint8_t> payload, bool compressed = false,
        std::uint32_t flags = 0, float version = 1.f, const char* type = "SPEL")
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
        record(data, type, flags, 0x800, payload);
        auto stream = std::make_unique<std::stringstream>(std::string(data.begin(), data.end()),
            std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "fixture.esm", nullptr, &encoder, true);
        if (!reader.getRecordHeader())
            throw std::runtime_error("test record missing");
        reader.getRecordData();
        T result{};
        result.load(reader);
        return result;
    }
}

namespace
{
    std::vector<std::uint8_t> spellInfo()
    {
        std::vector<std::uint8_t> data(16);
        put(data, 0, 4); put(data, 4, 123); put(data, 8, 2);
        data[12] = 1; data[13] = 0xa5; data[14] = 0xff; data[15] = 0x80;
        return data;
    }
    std::vector<std::uint8_t> effectInfo(const char* code = "FOSP")
    {
        std::vector<std::uint8_t> data(24);
        std::copy(code, code + 4, data.begin());
        put(data, 4, 50); put(data, 8, 3); put(data, 12, 7);
        put(data, 16, 0); put(data, 20, 9);
        return data;
    }
    std::vector<std::uint8_t> spellRecord()
    {
        std::vector<std::uint8_t> data;
        sub(data, "EDID", {'a', 'b', 0});
        sub(data, "FULL", {'s', 'p', 'e', 'l', 'l', 0});
        sub(data, "SPIT", spellInfo());
        sub(data, "EFID", {'F', 'O', 'S', 'P'});
        sub(data, "ZZZZ", {1, 2, 3});
        sub(data, "EFIT", effectInfo());
        return data;
    }
}

TEST(ESM4Spell, ReadsOrderedEffectsAndPreservesPaddingUnknownFieldsAndCompression)
{
    for (bool compressed : {false, true})
    {
        auto payload = spellRecord();
        sub(payload, "EFID", {'F', 'O', 'A', 'T'});
        auto second = effectInfo("FOAT"); put(second, 4, 10); put(second, 20, 5);
        sub(payload, "EFIT", second);
        const auto spell = load(payload, compressed);
        ASSERT_TRUE(spell.mData);
        EXPECT_EQ(spell.mEditorId, "ab"); EXPECT_EQ(spell.mFullName, "spell");
        EXPECT_EQ(spell.mData->mType, 4); EXPECT_EQ(spell.mData->mCost, 123);
        EXPECT_EQ(spell.mData->mLevel, 2); EXPECT_EQ(spell.mData->mFlags, 1);
        EXPECT_EQ(spell.mData->mPadding, (std::array<std::uint8_t, 3>{0xa5, 0xff, 0x80}));
        ASSERT_EQ(spell.mEffects.size(), 2);
        const auto& first = spell.mEffects[0];
        EXPECT_EQ(first.mId, ESM::fourCC("FOSP")); EXPECT_EQ(first.mMagnitude, 50);
        EXPECT_EQ(first.mArea, 3); EXPECT_EQ(first.mDuration, 7);
        EXPECT_EQ(first.mRange, 0); EXPECT_EQ(first.mActorValue, 9);
        EXPECT_FALSE(first.mScriptEffect);
        EXPECT_EQ(spell.mEffects[1].mId, ESM::fourCC("FOAT"));
        EXPECT_EQ(spell.mEffects[1].mMagnitude, 10); EXPECT_EQ(spell.mEffects[1].mActorValue, 5);
        ASSERT_EQ(spell.mSubRecords.size(), 8);
        EXPECT_EQ(spell.mSubRecords[4].mData, (std::vector<std::uint8_t>{1,2,3}));
    }
}

TEST(ESM4Spell, ReadsShortAndFullScriptEffectsWithoutConfusingSpellNameOrMagicCodeWithFormId)
{
    for (unsigned size : {4, 12, 16})
    {
        auto payload = spellRecord();
        sub(payload, "EFID", {'S', 'E', 'F', 'F'});
        sub(payload, "EFIT", effectInfo("SEFF"));
        std::vector<std::uint8_t> script(size);
        put(script, 0, 0x812);
        if (size >= 12) { put(script, 4, 3); put(script, 8, ESM::fourCC("FIDG")); }
        if (size == 16) {script[12] = 1; script[13] = 0xa5; script[14] = 0xff; script[15] = 0x80;}
        sub(payload, "SCIT", script); sub(payload, "FULL", {'e','f','f','e','c','t',0});
        const auto spell = load(payload, true);
        EXPECT_EQ(spell.mFullName, "spell");
        ASSERT_EQ(spell.mEffects.size(), 2);
        ASSERT_TRUE(spell.mEffects[1].mScriptEffect);
        const auto& effect = *spell.mEffects[1].mScriptEffect;
        EXPECT_EQ(effect.mScript, ESM::FormKey::content("fixture.esm", 0x812));
        EXPECT_EQ(effect.mName, "effect");
        EXPECT_EQ(effect.mSchool.has_value(), size >= 12);
        EXPECT_EQ(effect.mVisualEffect.has_value(), size >= 12);
        EXPECT_EQ(effect.mFlags.has_value(), size == 16);
        if (size >= 12) { EXPECT_EQ(effect.mSchool, 3); EXPECT_EQ(effect.mVisualEffect, ESM::fourCC("FIDG")); }
        if (size == 16) { EXPECT_EQ(effect.mFlags, 1); EXPECT_EQ(effect.mPadding, (std::array<std::uint8_t,3>{0xa5,0xff,0x80})); }
    }
}

TEST(ESM4Spell, RejectsTruncatedDuplicateUnpairedAndMismatchedFields)
{
    EXPECT_THROW(load({}), std::runtime_error);
    EXPECT_FALSE(load({}, false, ESM4::Rec_Deleted).mData);
    for (const auto& [tag, expected] : std::array<std::pair<const char*,unsigned>,3>{{{"SPIT",16},{"EFID",4},{"EFIT",24}}})
    {
        for (unsigned size=0; size<=26; ++size)
        {
            if (size == expected) continue;
            std::vector<std::uint8_t> payload;
            if (std::string(tag) != "SPIT") sub(payload, "SPIT", spellInfo());
            if (std::string(tag) == "EFIT") sub(payload, "EFID", {'F','O','S','P'});
            sub(payload, tag, std::vector<std::uint8_t>(size));
            EXPECT_THROW(load(payload), std::runtime_error) << tag << ':' << size;
        }
    }
    auto payload = spellRecord(); sub(payload, "SPIT", spellInfo()); EXPECT_THROW(load(payload), std::runtime_error);
    payload = spellRecord(); sub(payload, "EFIT", effectInfo()); EXPECT_THROW(load(payload), std::runtime_error);
    payload = spellRecord(); sub(payload, "EFID", {'F','O','S','P'}); EXPECT_THROW(load(payload), std::runtime_error);
    payload.clear(); sub(payload, "SPIT", spellInfo()); sub(payload, "EFID", {'F','O','A','T'});
    sub(payload, "EFIT", effectInfo()); EXPECT_THROW(load(payload), std::runtime_error);
    payload.clear(); sub(payload, "EFID", {'F','O','S','P'}); sub(payload, "EFID", {'F','O','S','P'});
    EXPECT_THROW(load(payload), std::runtime_error);
    for (unsigned size=0; size<=18; ++size)
    {
        if (size==4 || size==12 || size==16) continue;
        payload=spellRecord(); sub(payload, "SCIT", std::vector<std::uint8_t>(size));
        EXPECT_THROW(load(payload), std::runtime_error) << size;
    }
    payload.clear(); sub(payload, "SPIT", spellInfo()); sub(payload, "SCIT", std::vector<std::uint8_t>(4));
    EXPECT_THROW(load(payload), std::runtime_error);
    payload=spellRecord(); sub(payload, "SCIT", std::vector<std::uint8_t>(4)); sub(payload, "SCIT", std::vector<std::uint8_t>(4));
    EXPECT_THROW(load(payload), std::runtime_error);
    EXPECT_THROW(load(spellRecord(), false, 0, 1.7f), std::runtime_error);
}

TEST(ESM4Spell, PreservesUnknownNumericDomainsForExplicitRuntimePolicy)
{
    auto payload = spellRecord();
    auto info=spellInfo(); put(info, 0, 0xffffffff); put(info, 8, 0xdeadbeef);
    payload.clear(); sub(payload, "SPIT", info);
    sub(payload, "EFID", {'Z','Z','Z','Z'});
    auto effect=effectInfo("ZZZZ"); put(effect,16,0xffffffff); put(effect,20,0xffffffff);
    sub(payload,"EFIT",effect);
    const auto spell=load(payload);
    EXPECT_EQ(spell.mData->mType, 0xffffffff); EXPECT_EQ(spell.mData->mLevel, 0xdeadbeef);
    ASSERT_EQ(spell.mEffects.size(),1);
    EXPECT_EQ(spell.mEffects[0].mRange, 0xffffffff); EXPECT_EQ(spell.mEffects[0].mActorValue, 0xffffffff);
}
