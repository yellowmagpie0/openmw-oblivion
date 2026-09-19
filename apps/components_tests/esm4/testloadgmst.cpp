#include <components/esm4/loadgmst.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/reader.hpp>
#include <components/toutf8/toutf8.hpp>

#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <sstream>

namespace
{
    template <typename T> std::string bytes(const T& value)
    {
        return { reinterpret_cast<const char*>(&value), sizeof(value) };
    }
    std::string sub(const char* tag, const std::string& data)
    {
        return std::string(tag, 4) + bytes(static_cast<std::uint16_t>(data.size())) + data;
    }
    std::string record(const char* tag, std::uint32_t flags, const std::string& data)
    {
        return std::string(tag, 4) + bytes(static_cast<std::uint32_t>(data.size()))
            + bytes(flags) + bytes(std::uint32_t{0x800}) + bytes(std::uint32_t{}) + data;
    }
    ESM4::GameSetting load(const std::string& data, std::uint32_t flags = 0,
        ESM4::GameSetting result = {})
    {
        const auto header = record("TES4", ESM4::Rec_ESM,
            sub("HEDR", bytes(1.f) + bytes(std::uint32_t{1}) + bytes(std::uint32_t{0x900})));
        auto stream = std::make_unique<std::stringstream>(header + record("GMST", flags, data),
            std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "settings.esm", nullptr, &encoder, true);
        if (!reader.getRecordHeader())
            throw std::runtime_error("missing fixture record");
        reader.getRecordData();
        result.load(reader);
        return result;
    }
    std::string name(const char* value) { return sub("EDID", std::string(value) + '\0'); }
}

TEST(ESM4GameSetting, RejectsMalformedNumericPayloadsAndNonfiniteValues)
{
    for (const auto* id : {"iAIDefaultAttackChance", "fDamageSkillMult"})
        for (const auto size : {0, 1, 3, 5, 8})
            EXPECT_THROW(load(name(id) + sub("DATA", std::string(size, '\0'))), std::runtime_error)
                << id << " size " << size;
    for (float value : {std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        EXPECT_THROW(load(name("fDamageSkillMult") + sub("DATA", bytes(value))), std::runtime_error);
}

TEST(ESM4GameSetting, RejectsMissingDuplicateOrUnorderedRequiredFields)
{
    const auto id = name("iAIDefaultAttackChance");
    const auto data = sub("DATA", bytes(std::int32_t{40}));
    for (const auto& payload : {std::string{}, id, data, data + id, name("") + data,
             id + id + data, id + data + data})
        EXPECT_THROW(load(payload), std::runtime_error);
}

TEST(ESM4GameSetting, PreservesTypedValuesAndClearsReusedDeletedRecords)
{
    EXPECT_EQ(std::get<std::int32_t>(load(name("iActorLuckSkillBase")
        + sub("DATA", bytes(std::int32_t{-20}))).mData), -20);
    EXPECT_FLOAT_EQ(std::get<float>(load(name("fDamageSkillMult") + sub("DATA", bytes(1.5f))).mData), 1.5f);
    EXPECT_EQ(std::get<std::string>(load(name("sTest") + sub("DATA", std::string("Native\0", 7))).mData), "Native");
    auto old = load(name("fDamageSkillMult") + sub("DATA", bytes(1.5f)));
    const auto deleted = load({}, ESM4::Rec_Deleted, old);
    EXPECT_TRUE(deleted.mEditorId.empty());
    EXPECT_TRUE(std::holds_alternative<std::monostate>(deleted.mData));
}
