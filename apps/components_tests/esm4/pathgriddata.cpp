#include <gtest/gtest.h>

#include <stdexcept>

#include <components/esm4/pathgriddata.hpp>

namespace
{
    ESM::FormKey key(std::uint32_t value)
    {
        return ESM::FormKey::content("Oblivion.esm", value);
    }

    ESM4::Pathgrid makeGrid(std::uint32_t id, std::initializer_list<ESM4::Pathgrid::PGRP> nodes,
        std::initializer_list<ESM4::Pathgrid::PGRR> links = {})
    {
        ESM4::Pathgrid result;
        result.mFormKey = key(id);
        result.mNodes = nodes;
        result.mData = static_cast<std::int16_t>(result.mNodes.size());
        result.mLinks = links;
        return result;
    }

    ESM4::Pathgrid::PGRP point(float x, float y = 0.0f, float z = 0.0f)
    {
        return { x, y, z, 0, 0x7f, 0xbeef };
    }
}

TEST(ESM4PathgridData, BuildsDeterministicLocalGraphAndNearestNode)
{
    ESM4::Pathgrid definition = makeGrid(1, { point(0), point(10), point(20) },
        { { 0, 1 }, { 0, 1 }, { 1, 2 } });
    ESM4::PathgridService service;
    service.registerPathgrid(definition, key(100), { { 100, 200, 0 }, 1.0f, true });

    const ESM4::PathgridGraph* graph = service.graph(key(1));
    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->localEdges().size(), 2u);
    EXPECT_EQ(graph->worldPoint(0), (ESM4::PathgridPoint{ 100, 200, 0 }));
    EXPECT_EQ(service.nearestEnabledNode(key(1), { 106, 200, 0 })->mNode, 1u);

    const auto route = service.route({ key(1), 0 }, { key(1), 2 });
    ASSERT_TRUE(route);
    ASSERT_EQ(route.mRoute->mNodes.size(), 3u);
    EXPECT_EQ(route.mRoute->mNodes.front(), (ESM4::PathgridNodeKey{ key(1), 0 }));
    EXPECT_EQ(route.mRoute->mNodes.back(), (ESM4::PathgridNodeKey{ key(1), 2 }));
}

TEST(ESM4PathgridData, KeepsExteriorWorldCoordinatesAndOffsetsInteriorCoordinates)
{
    ESM4::Pathgrid definition = makeGrid(10, { point(29374.5f, 70743.5f, 3609.f) });
    ESM4::PathgridService service;
    service.registerPathgrid(definition, key(110), { {}, 1.0f, false });
    EXPECT_EQ(service.graph(key(10))->worldPoint(0), (ESM4::PathgridPoint{ 29374.5f, 70743.5f, 3609.f }));

    ESM4::Pathgrid interior = makeGrid(11, { point(100.f, -50.f, 20.f) });
    service.registerPathgrid(interior, key(111), { { 4096.f, 8192.f, 0.f }, 1.0f, true });
    EXPECT_EQ(service.graph(key(11))->worldPoint(0), (ESM4::PathgridPoint{ 4196.f, 8142.f, 20.f }));
}

TEST(ESM4PathgridData, OverlayChangesGenerationAndBlocksRoutes)
{
    ESM4::Pathgrid definition = makeGrid(2, { point(0), point(10), point(20) }, { { 0, 1 }, { 1, 2 } });
    ESM4::PathgridService service;
    service.registerPathgrid(definition, key(101));
    const std::uint64_t before = service.generation(key(2));

    EXPECT_TRUE(service.setNodeEnabled({ key(2), 1 }, false));
    EXPECT_GT(service.generation(key(2)), before);
    EXPECT_EQ(service.route({ key(2), 0 }, { key(2), 2 }).mFailure,
        ESM4::PathgridRouteFailure::DifferentComponent);
    EXPECT_FALSE(service.setNodeEnabled({ key(2), 1 }, false));
    EXPECT_TRUE(service.setNodeEnabled({ key(2), 1 }, true));
    EXPECT_TRUE(service.route({ key(2), 0 }, { key(2), 2 }));
}

TEST(ESM4PathgridData, BoundsLargeRouteSearch)
{
    constexpr std::uint32_t nodeCount = 8194;
    ESM4::Pathgrid definition;
    definition.mFormKey = key(12);
    definition.mData = static_cast<std::int16_t>(nodeCount);
    definition.mNodes.reserve(nodeCount);
    definition.mLinks.reserve(nodeCount - 1);
    for (std::uint32_t node = 0; node < nodeCount; ++node)
    {
        definition.mNodes.push_back(point(static_cast<float>(node)));
        if (node != 0)
            definition.mLinks.push_back({ static_cast<std::int16_t>(node - 1), static_cast<std::int16_t>(node) });
    }

    ESM4::PathgridService service;
    service.registerPathgrid(definition, key(112));
    const auto route = service.route({ key(12), 0 }, { key(12), nodeCount - 1 });
    EXPECT_FALSE(route);
    EXPECT_EQ(route.mFailure, ESM4::PathgridRouteFailure::SearchLimit);
}

TEST(ESM4PathgridData, ResolvesForeignCoordinatesAndReportsMissingEdges)
{
    ESM4::Pathgrid first = makeGrid(3, { point(0), point(10) });
    first.mForeign.push_back({ 0, 0, 30, 0, 0 });
    ESM4::Pathgrid second = makeGrid(4, { point(0) });
    second.mForeign.push_back({ 0, 0, -20, 0, 0 });

    ESM4::PathgridService service;
    service.registerPathgrid(first, key(102));
    service.registerPathgrid(second, key(103), { { 30, 0, 0 }, 1.0f, true });
    EXPECT_TRUE(service.resolveForeignLinks(0.01f));
    const auto route = service.route({ key(3), 0 }, { key(4), 0 });
    ASSERT_TRUE(route);
    ASSERT_EQ(route.mRoute->mNodes.size(), 2u);
    EXPECT_TRUE(service.route({ key(3), 0 }, { key(3), 1 }));

    ESM4::Pathgrid unresolved = makeGrid(5, { point(0) });
    unresolved.mForeign.push_back({ 0, 0, 100, 100, 100 });
    service.registerPathgrid(unresolved, key(104));
    EXPECT_FALSE(service.resolveForeignLinks(0.01f));
    EXPECT_EQ(service.route({ key(5), 0 }, { key(4), 0 }).mFailure,
        ESM4::PathgridRouteFailure::UnresolvedForeignLink);

    const std::uint64_t beforeUnload = service.generation(key(3));
    EXPECT_TRUE(service.unregisterPathgrid(key(4)));
    EXPECT_GT(service.generation(key(3)), beforeUnload);
    EXPECT_EQ(service.route({ key(3), 0 }, { key(3), 1 }).mFailure,
        ESM4::PathgridRouteFailure::UnresolvedForeignLink);
}

TEST(ESM4PathgridData, PreservesObjectLinksAndRejectsAmbiguousCells)
{
    ESM4::Pathgrid definition = makeGrid(6, { point(0) });
    ESM4::Pathgrid::PGRL object;
    object.objectKey = key(200);
    object.linkedNodes = { 0, -1 };
    definition.mObjects.push_back(object);

    ESM4::PathgridService service;
    service.registerPathgrid(definition, key(105));
    ASSERT_EQ(service.graph(key(6))->objectLinks().size(), 1u);
    EXPECT_EQ(service.graph(key(6))->objectLinks().front().mObject, key(200));
    EXPECT_THROW(service.registerPathgrid(makeGrid(7, { point(0) }), key(105)), std::logic_error);
}

TEST(ESM4PathgridData, RejectsInvalidDefinition)
{
    ESM4::Pathgrid definition = makeGrid(8, { point(0) }, { { 0, 2 } });
    EXPECT_THROW(ESM4::PathgridGraph(definition, key(106), {}), std::invalid_argument);

    definition = makeGrid(9, { point(0), point(1) }, { { -2, 1 } });
    EXPECT_THROW(ESM4::PathgridGraph(definition, key(107), {}), std::invalid_argument);
}

TEST(ESM4PathgridData, CellLifecycleIsStableAndIdempotent)
{
    ESM4::PathgridService service;
    const ESM::FormKey cellKey = key(300);
    EXPECT_TRUE(service.cellLoaded(cellKey));
    EXPECT_FALSE(service.cellLoaded(cellKey));
    EXPECT_TRUE(service.isCellLoaded(cellKey));
    EXPECT_EQ(service.loadedCellCount(), 1u);
    EXPECT_TRUE(service.cellUnloaded(cellKey));
    EXPECT_FALSE(service.cellUnloaded(cellKey));
    EXPECT_FALSE(service.isCellLoaded(cellKey));
    EXPECT_EQ(service.loadedCellCount(), 0u);
    EXPECT_THROW(service.cellLoaded({}), std::invalid_argument);
}
