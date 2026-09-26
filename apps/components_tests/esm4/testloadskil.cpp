#include <components/esm4/loadskil.hpp>
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
    void number(std::vector<std::uint8_t>& data, std::size_t offset, float value)
    {
        put(data, offset, std::bit_cast<std::uint32_t>(value));
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
    template <typename T = ESM4::Skill> T load(std::vector<std::uint8_t> payload, bool compressed = false,
        std::uint32_t flags = 0, float version = 1.f, const char* type = "SKIL")
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
    std::vector<std::uint8_t> skillData()
    {
        std::vector<std::uint8_t> result(20);
        put(result, 0, 19);
        put(result, 4, 1);
        put(result, 8, 1);
        number(result, 12, 5.f);
        number(result, 16, .5f);
        return result;
    }
    std::vector<std::uint8_t> skillRecord()
    {
        std::vector<std::uint8_t> result, index(4);
        put(index, 0, 19);
        sub(result, "INDX", index);
        sub(result, "DATA", skillData());
        sub(result, "ANAM", {'a', 'p', 'p', 0});
        sub(result, "ZZZZ", {1, 2, 3});
        return result;
    }
}

TEST(ESM4Skill, ReadsNativeDefinitionsAndPreservesOtherFieldsWithCompression)
{
    for (bool compressed : {false, true})
    {
        const auto skill = load(skillRecord(), compressed);
        ASSERT_TRUE(skill.mIndex);
        ASSERT_TRUE(skill.mData);
        EXPECT_EQ(*skill.mIndex, 19);
        EXPECT_EQ(skill.mData->mActorValue, 19);
        EXPECT_EQ(skill.mData->mGoverningAttribute, 1);
        EXPECT_EQ(skill.mData->mSpecialization, 1);
        EXPECT_EQ(skill.mData->mUseValues[0], 5);
        EXPECT_EQ(skill.mData->mUseValues[1], .5f);
        ASSERT_EQ(skill.mSubRecords.size(), 4);
        EXPECT_EQ(skill.mSubRecords[2].mData, (std::vector<std::uint8_t>{'a', 'p', 'p', 0}));
        EXPECT_EQ(skill.mSubRecords[3].mData, (std::vector<std::uint8_t>{1, 2, 3}));
    }
}

TEST(ESM4Skill, RejectsMalformedLengthsDomainsAndNonfiniteUseValues)
{
    for (unsigned length = 0; length <= 24; ++length)
    {
        if (length == 20)
            continue;
        auto data = skillData();
        data.resize(length);
        EXPECT_THROW(ESM4::decodeSkillData(data), std::runtime_error) << length;
    }
    for (auto [offset, value] : std::array<std::pair<unsigned, unsigned>, 4>{{{0, 11}, {0, 33}, {4, 8}, {8, 3}}})
    {
        auto data = skillData();
        put(data, offset, value);
        EXPECT_THROW(ESM4::decodeSkillData(data), std::runtime_error);
    }
    for (unsigned offset : {12, 16})
    {
        auto data = skillData();
        number(data, offset, std::numeric_limits<float>::quiet_NaN());
        EXPECT_THROW(ESM4::decodeSkillData(data), std::runtime_error);
        number(data, offset, std::numeric_limits<float>::infinity());
        EXPECT_THROW(ESM4::decodeSkillData(data), std::runtime_error);
    }
}

TEST(ESM4Skill, RequiresConsistentUniqueIndexAndDataExceptEmptyDeletion)
{
    EXPECT_THROW(load({}), std::runtime_error);
    auto skill = load({}, false, ESM4::Rec_Deleted);
    EXPECT_FALSE(skill.mData);
    EXPECT_FALSE(skill.mIndex);
    std::vector<std::uint8_t> index(4), payload;
    put(index, 0, 20);
    sub(payload, "INDX", index);
    EXPECT_THROW(load(payload), std::runtime_error);
    sub(payload, "DATA", skillData());
    EXPECT_THROW(load(payload), std::runtime_error); // DATA declares 19.
    payload = skillRecord();
    sub(payload, "INDX", index);
    EXPECT_THROW(load(payload), std::runtime_error);
    payload = skillRecord();
    sub(payload, "DATA", skillData());
    EXPECT_THROW(load(payload), std::runtime_error);
    payload.clear();
    sub(payload, "INDX", {19, 0, 0});
    sub(payload, "DATA", skillData());
    EXPECT_THROW(load(payload), std::runtime_error);
    EXPECT_THROW(load(skillRecord(), false, 0, 1.7f), std::runtime_error);
}

TEST(ESM4Skill, ResolvesByNativeActorValueAndRejectsIncompleteOrAmbiguousInventory)
{
    std::array<ESM4::Skill, 21> records{};
    std::array<const ESM4::Skill*, 21> winning{};
    for (unsigned i = 0; i < records.size(); ++i)
    {
        records[i].mIndex = i + 12;
        records[i].mData = ESM4::SkillData{i + 12, i % 8, i % 3, {0, 0}};
        winning[20 - i] = &records[i];
    }
    const auto resolved = ESM4::resolveSkillDefinitions(winning);
    for (unsigned i = 0; i < records.size(); ++i)
        EXPECT_EQ(resolved[i], &records[i]);
    EXPECT_THROW(ESM4::resolveSkillDefinitions({}), std::invalid_argument);
    winning[0] = nullptr;
    EXPECT_THROW(ESM4::resolveSkillDefinitions(winning), std::invalid_argument);
    winning[0] = winning[1];
    EXPECT_THROW(ESM4::resolveSkillDefinitions(winning), std::invalid_argument);
    winning[0] = &records[20];
    records[20].mFlags = ESM4::Rec_Deleted;
    EXPECT_THROW(ESM4::resolveSkillDefinitions(winning), std::invalid_argument);
    records[20].mFlags = 0;
    records[20].mData.reset();
    EXPECT_THROW(ESM4::resolveSkillDefinitions(winning), std::invalid_argument);
}

TEST(ESM4Race, PreservesSevenOrderedSignedBonusesAndSeparatesPadding)
{
    std::vector<std::uint8_t> data(36), payload;
    const std::array<std::int8_t, 14> pairs{12, -5, 12, 10, 32, 127, -1, 0, 19, -128, 20, 3, 21, 4};
    for (unsigned i = 0; i < pairs.size(); ++i)
        data[i] = static_cast<std::uint8_t>(pairs[i]);
    data[14] = 30; // Poisoned padding must not become an eighth skill bonus.
    data[15] = 99;
    number(data, 16, 1.1f);
    number(data, 20, 1.2f);
    number(data, 24, .9f);
    number(data, 28, .8f);
    put(data, 32, 1);
    sub(payload, "DATA", data);
    const auto race = load<ESM4::Race>(payload, true, 0, 1.f, "RACE");
    ASSERT_TRUE(race.mTES4SkillBonuses);
    for (unsigned i = 0; i < 7; ++i)
    {
        EXPECT_EQ((*race.mTES4SkillBonuses)[i].mSkill, pairs[i * 2]);
        EXPECT_EQ((*race.mTES4SkillBonuses)[i].mBonus, pairs[i * 2 + 1]);
    }
    EXPECT_EQ(race.mTES4SkillBonusPadding, 0x631e);
    EXPECT_EQ(race.mSkillBonus.count(ESM4::Race::Skill_Security), 0);
    EXPECT_EQ(race.mHeightMale, 1.1f);
    EXPECT_EQ(race.mHeightFemale, 1.2f);
    EXPECT_EQ(race.mWeightMale, .9f);
    EXPECT_EQ(race.mWeightFemale, .8f);
    EXPECT_EQ(race.mRaceFlags, 1);
    sub(payload, "DATA", data);
    EXPECT_THROW(load<ESM4::Race>(payload, false, 0, 1.f, "RACE"), std::runtime_error);
}

TEST(ESM4Race, RejectsMalformedNativeDataWithoutAffectingLaterGameLayouts)
{
    for (unsigned length : {0, 14, 16, 35, 37, 128, 164})
    {
        std::vector<std::uint8_t> payload;
        sub(payload, "DATA", std::vector<std::uint8_t>(length));
        EXPECT_THROW(load<ESM4::Race>(payload, false, 0, 1.f, "RACE"), std::runtime_error) << length;
    }
    std::vector<std::uint8_t> payload;
    sub(payload, "DATA", std::vector<std::uint8_t>(128));
    const auto later = load<ESM4::Race>(payload, false, 0, 1.7f, "RACE");
    EXPECT_TRUE(later.mIsTES5);
    EXPECT_FALSE(later.mTES4SkillBonuses);
}
