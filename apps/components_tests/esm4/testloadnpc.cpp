#include <components/esm4/loadnpc.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/reader.hpp>
#include <components/toutf8/toutf8.hpp>

#include <gtest/gtest.h>
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
    std::string record(const char* tag, const std::string& data)
    {
        return std::string(tag, 4) + bytes(static_cast<std::uint32_t>(data.size()))
            + bytes(std::uint32_t{}) + bytes(std::uint32_t{0x800}) + bytes(std::uint32_t{}) + data;
    }
    ESM4::Npc load(const std::string& data, ESM4::Npc result = {})
    {
        const auto header = record("TES4", sub("HEDR", bytes(1.f) + bytes(std::uint32_t{1}) + bytes(std::uint32_t{0x900})));
        auto stream = std::make_unique<std::stringstream>(header + record("NPC_", data), std::ios::in | std::ios::binary);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        ESM4::Reader reader(std::move(stream), "actors.esm", nullptr, &encoder, true);
        if (!reader.getRecordHeader())
            throw std::runtime_error("missing fixture record");
        reader.getRecordData();
        result.load(reader);
        return result;
    }
}

TEST(ESM4NpcCombat, NativeHealthPreservesTheFullOriginalWord)
{
    const auto data = std::string(21, char{50}) + bytes(std::uint32_t{100000}) + std::string(8, char{40});
    const auto npc = load(sub("DATA", data));
    // GTest binds arguments by reference; copy the packed disk field first.
    const std::uint32_t health = npc.mData.health;
    EXPECT_EQ(health, 100000);
    EXPECT_EQ(npc.mData.skills.blade, 50);
    EXPECT_EQ(npc.mData.attribs.luck, 40);
    EXPECT_TRUE(npc.mIsTES4);
}

TEST(ESM4NpcCombat, NativeCombatPayloadSizesAndDuplicatesAreChecked)
{
    for (const auto& [tag, size] : {std::pair{"DATA", 33}, {"ACBS", 16}, {"AIDT", 12}})
    {
        for (int bad : {0, size - 1, size + 1, size + 8})
            EXPECT_THROW(load(sub(tag, std::string(bad, '\0'))), std::runtime_error) << tag << ' ' << bad;
        const auto field = sub(tag, std::string(size, '\0'));
        EXPECT_NO_THROW(load(field));
        EXPECT_THROW(load(field + field), std::runtime_error) << tag;
    }
}

TEST(ESM4NpcCombat, ReusedRecordDoesNotRetainPriorActorInputs)
{
    auto old = load(sub("DATA", std::string(33, char{1})) + sub("ZNAM", bytes(std::uint32_t{0x900}))
        + sub("SPLO", bytes(std::uint32_t{0x901})) + sub("CNTO", bytes(std::uint32_t{0x902}) + bytes(std::int32_t{1})));
    const auto empty = load({}, old);
    const std::uint32_t health = empty.mData.health;
    EXPECT_EQ(health, 0);
    EXPECT_TRUE(empty.mCombatStyle.isZeroOrUnset());
    EXPECT_TRUE(empty.mSpell.empty());
    EXPECT_TRUE(empty.mInventory.empty());
}
