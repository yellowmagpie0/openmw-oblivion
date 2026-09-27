#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/grouptype.hpp>
#include <components/esm4/reader.hpp>

#include "apps/openmw/mwworld/esmstore.hpp"

namespace
{
    template <class T>
    std::string bytes(const T& value)
    {
        return {reinterpret_cast<const char*>(&value), sizeof(value)};
    }

    std::string sub(std::uint32_t tag, const std::string& data)
    {
        return bytes(tag) + bytes(static_cast<std::uint16_t>(data.size())) + data;
    }

    std::string record(std::uint32_t tag, std::uint32_t id, const std::string& data, std::uint32_t flags = 0)
    {
        return bytes(tag) + bytes(static_cast<std::uint32_t>(data.size())) + bytes(flags)
            + bytes(id) + bytes(std::uint32_t{}) + data;
    }

    std::string group(std::uint32_t label, ESM4::GroupType type, const std::string& data)
    {
        return bytes(std::uint32_t{ESM4::REC_GRUP}) + bytes(static_cast<std::uint32_t>(data.size() + 20))
            + bytes(label) + bytes(static_cast<std::uint32_t>(type)) + bytes(std::uint32_t{}) + data;
    }

    void load(MWWorld::ESMStore& store, const std::string& name, const std::vector<std::string>& masters,
        const std::string& records)
    {
        auto header = sub(ESM::fourCC("HEDR"), bytes(1.f) + bytes(std::uint32_t{}) + bytes(std::uint32_t{0x900}));
        for (const auto& master : masters)
            header += sub(ESM::fourCC("MAST"), master + '\0') + sub(ESM::fourCC("DATA"), std::string(8, '\0'));
        auto stream = std::make_unique<std::stringstream>(record(ESM4::REC_TES4, 0, header, 1) + records,
            std::ios::in | std::ios::binary);
        ESM4::Reader reader(std::move(stream), name, nullptr, nullptr, true);
        const std::map<std::string, int> indices{{"base.esm", 0}, {"other.esm", 1}, {"patch.esp", 2}};
        reader.setModIndex(indices.at(name));
        reader.updateModIndices(indices);
        store.loadESM4(reader, nullptr);
    }
}

TEST(MWWorldPathgridStoreTest, classifiesWinningNativeObjectLinksAfterLoadOrderResolution)
{
    using Kind = ESM4::PathgridObjectKind;
    // Direct bases, placed objects, absent bases, deleted bases/references and
    // actor references all travel through the real binary reader and store.
    const std::map<std::uint32_t, Kind> expected{
        {0x801, Kind::Door}, {0x802, Kind::Furniture}, {0x803, Kind::Other},
        {0x804, Kind::Deleted}, {0x810, Kind::Door}, {0x811, Kind::Furniture},
        {0x812, Kind::Other}, {0x813, Kind::Missing}, {0x814, Kind::Deleted},
        {0x815, Kind::Deleted}, {0x816, Kind::Missing}, {0x817, Kind::Missing},
        {0x818, Kind::Missing}, {0x819, Kind::Furniture}, {0x899, Kind::Missing}};
    auto bases = record(ESM4::REC_DOOR, 0x801, {}) + record(ESM4::REC_FURN, 0x802, {})
        + record(ESM4::REC_STAT, 0x803, {}) + record(ESM4::REC_DOOR, 0x804, {});
    const auto placed = [](std::uint32_t tag, std::uint32_t id, std::uint32_t base) {
        return record(tag, id, sub(ESM::fourCC("NAME"), bytes(base)));
    };
    auto children = placed(ESM4::REC_REFR, 0x810, 0x801) + placed(ESM4::REC_REFR, 0x811, 0x802)
        + placed(ESM4::REC_REFR, 0x812, 0x803) + placed(ESM4::REC_REFR, 0x813, 0x899)
        + placed(ESM4::REC_REFR, 0x814, 0x804) + placed(ESM4::REC_REFR, 0x815, 0x801)
        + placed(ESM4::REC_REFR, 0x816, 0) + placed(ESM4::REC_ACHR, 0x817, 0x899)
        + placed(ESM4::REC_ACRE, 0x818, 0x899) + placed(ESM4::REC_REFR, 0x819, 0x801);
    auto pathgrid = sub(ESM::fourCC("DATA"), bytes(std::int16_t{1}))
        + sub(ESM::fourCC("PGRP"), std::string(16, '\0'));
    for (const auto& [id, kind] : expected)
        pathgrid += sub(ESM::fourCC("PGRL"), bytes(id) + bytes(std::int32_t{}));
    children += record(ESM4::REC_PGRD, 0x820, pathgrid);
    MWWorld::ESMStore store;
    load(store, "base.esm", {}, bases + group(ESM4::REC_CELL, ESM4::Grp_RecordType,
        record(ESM4::REC_CELL, 0x800, sub(ESM::fourCC("DATA"), bytes(std::uint8_t{1})))
            + group(0x800, ESM4::Grp_CellTemporaryChild, children)));
    // Identical local IDs in independent masters must not alias. The patch's
    // master order deliberately differs from the global load order.
    load(store, "other.esm", {}, record(ESM4::REC_FURN, 0x801, {}));
    load(store, "patch.esp", {"other.esm", "base.esm"},
        record(ESM4::REC_DOOR, 0x01000804, {}, ESM4::Rec_Deleted)
        + group(ESM4::REC_CELL, ESM4::Grp_RecordType,
            record(ESM4::REC_CELL, 0x01000800, sub(ESM::fourCC("DATA"), bytes(std::uint8_t{1})))
                + group(0x01000800, ESM4::Grp_CellTemporaryChild,
                    record(ESM4::REC_REFR, 0x01000815, {}, ESM4::Rec_Deleted)
                        + placed(ESM4::REC_REFR, 0x01000819, 0x801))));
    store.setUp();
    const auto* graph = store.getOblivionPathgridService().graph(ESM::FormKey::content("base.esm", 0x820));
    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->cellKey(), ESM::FormKey::content("base.esm", 0x800));
    ASSERT_EQ(graph->objectLinks().size(), expected.size());
    for (const auto& link : graph->objectLinks())
    {
        SCOPED_TRACE(link.mObject.serialize());
        EXPECT_EQ(link.mKind, expected.at(link.mObject.localId()));
        EXPECT_EQ(link.mNodes, std::vector<std::uint32_t>{0});
    }
}
