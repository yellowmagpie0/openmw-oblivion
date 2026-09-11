#include <gtest/gtest.h>

#include <array>
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

TEST(ESM4PathgridData, BulkConstructionMatchesIncrementalRegistration)
{
    ESM4::Pathgrid first = makeGrid(1, { point(0), point(10) }, { { 0, 1 } });
    first.mForeign.push_back({ 1, 0, 30, 0, 0 });
    ESM4::Pathgrid::PGRL object;
    object.objectKey = key(200);
    object.linkedNodes = { 1 };
    first.mObjects.push_back(object);
    const ESM4::Pathgrid second = makeGrid(2, { point(0), point(10) }, { { 0, 1 } });
    const ESM4::Pathgrid disconnected = makeGrid(3, { point(1000) });
    const std::array<ESM4::PathgridRegistration, 3> registrations{ {
        { first, key(101), {} },
        { second, key(102), { { 30, 0, 0 }, 1.0f, true } },
        { disconnected, key(103), {} },
    } };
    ESM4::PathgridService bulk(registrations);
    ESM4::PathgridService incremental;
    for (const auto& registration : registrations)
        incremental.registerPathgrid(registration.mDefinition, registration.mCell, registration.mTransform);

    EXPECT_EQ(bulk.graphCount(), incremental.graphCount());
    EXPECT_EQ(bulk.cells(), incremental.cells());
    EXPECT_EQ(bulk.route({ key(1), 1 }, { key(2), 0 }).mFailure,
        ESM4::PathgridRouteFailure::UnresolvedForeignLink);
    EXPECT_TRUE(bulk.resolveForeignLinks(0.01f));
    EXPECT_TRUE(incremental.resolveForeignLinks(0.01f));
    const auto classify = [](const ESM::FormKey&) { return ESM4::PathgridObjectKind::Door; };
    bulk.classifyObjectLinks(classify);
    incremental.classifyObjectLinks(classify);

    for (const auto& registration : registrations)
    {
        const auto& id = registration.mDefinition.mFormKey;
        ASSERT_NE(bulk.graphForCell(registration.mCell), nullptr);
        const auto* graph = bulk.graph(id);
        ASSERT_NE(graph, nullptr);
        EXPECT_EQ(graph->worldPoint(0), incremental.graph(id)->worldPoint(0));
        EXPECT_EQ(graph->localEdges(), incremental.graph(id)->localEdges());
        EXPECT_EQ(bulk.generation(id), incremental.generation(id));
        const auto* navigator = bulk.navigatorPathgrid(id);
        ASSERT_NE(navigator, nullptr);
        EXPECT_EQ(navigator->mPoints.size(), incremental.navigatorPathgrid(id)->mPoints.size());
        EXPECT_EQ(navigator->mEdges.size(), incremental.navigatorPathgrid(id)->mEdges.size());
    }
    EXPECT_EQ(bulk.graph(key(1))->objectLinks().front().mKind, ESM4::PathgridObjectKind::Door);
    const auto route = bulk.route({ key(1), 0 }, { key(2), 1 });
    const auto expected = incremental.route({ key(1), 0 }, { key(2), 1 });
    ASSERT_TRUE(route);
    ASSERT_TRUE(expected);
    EXPECT_EQ(route.mRoute->mNodes, expected.mRoute->mNodes);
    EXPECT_EQ(route.mRoute->mCost, expected.mRoute->mCost);
    EXPECT_EQ(route.mRoute->mGeneration, expected.mRoute->mGeneration);
    EXPECT_EQ(bulk.reachableComponent({ key(1), 0 }), incremental.reachableComponent({ key(1), 0 }));
    EXPECT_EQ(bulk.route({ key(1), 0 }, { key(3), 0 }).mFailure,
        ESM4::PathgridRouteFailure::DifferentComponent);
}

TEST(ESM4PathgridData, BulkConstructionPreservesLiveUpdatesAndUnloadedRoutes)
{
    ESM4::Pathgrid first = makeGrid(1, { point(0), point(10) }, { { 0, 1 } });
    first.mForeign.push_back({ 1, 0, 30, 0, 0 });
    const ESM4::Pathgrid second = makeGrid(2, { point(30) });
    const std::array<ESM4::PathgridRegistration, 2> registrations{ {
        { first, key(101), {} }, { second, key(102), {} },
    } };
    ESM4::PathgridService service(registrations);
    service.resolveForeignLinks(0.01f);
    const auto* navigator = service.navigatorPathgrid(key(1));
    ASSERT_NE(navigator, nullptr);
    EXPECT_TRUE(service.cellLoaded(key(101)));
    EXPECT_TRUE(service.cellUnloaded(key(101)));
    EXPECT_TRUE(service.route({ key(1), 0 }, { key(2), 0 }));
    EXPECT_EQ(service.graphCount(), 2u);

    const auto beforeOverlay = service.generation(key(1));
    EXPECT_TRUE(service.setNodeEnabled({ key(1), 1 }, false));
    EXPECT_GT(service.generation(key(1)), beforeOverlay);
    EXPECT_FALSE(service.route({ key(1), 0 }, { key(2), 0 }));
    EXPECT_EQ(service.navigatorPathgrid(key(1)), navigator);
    EXPECT_TRUE(service.navigatorPathgrid(key(1))->mEdges.empty());
    const auto disabled = service.disabledNodes();
    ASSERT_EQ(disabled.size(), 1u);
    ESM4::PathgridService restored(registrations);
    restored.resolveForeignLinks(0.01f);
    restored.applyOverlay(disabled.front(), false);
    EXPECT_FALSE(restored.route({ key(1), 0 }, { key(2), 0 }));
    EXPECT_EQ(restored.disabledNodes(), disabled);
    EXPECT_TRUE(service.setNodeEnabled({ key(1), 1 }, true));
    EXPECT_TRUE(service.route({ key(1), 0 }, { key(2), 0 }));

    const auto beforeRemoval = service.generation(key(1));
    EXPECT_TRUE(service.cellLoaded(key(102)));
    EXPECT_TRUE(service.unregisterPathgrid(key(2)));
    EXPECT_FALSE(service.isCellLoaded(key(102)));
    EXPECT_GT(service.generation(key(1)), beforeRemoval);
    EXPECT_FALSE(service.graph(key(1))->foreignLinks().front().mDestination);
    service.registerPathgrid(second, key(102));
    EXPECT_TRUE(service.route({ key(1), 0 }, { key(2), 0 }));
    EXPECT_EQ(service.navigatorPathgrid(key(1)), navigator);

    service.registerPathgrid(second, key(104), { { 100, 0, 0 }, 1.0f, true });
    EXPECT_EQ(service.graphForCell(key(102)), nullptr);
    ASSERT_NE(service.graphForCell(key(104)), nullptr);
    EXPECT_EQ(service.route({ key(1), 1 }, { key(2), 0 }).mFailure,
        ESM4::PathgridRouteFailure::UnresolvedForeignLink);
    service.registerPathgrid(second, key(104));
    EXPECT_TRUE(service.route({ key(1), 0 }, { key(2), 0 }));
}

TEST(ESM4PathgridData, FailedBulkConstructionPreservesExistingService)
{
    const ESM4::Pathgrid first = makeGrid(1, { point(0) });
    const ESM4::Pathgrid second = makeGrid(2, { point(10) });
    const ESM4::Pathgrid invalid = makeGrid(3, { point(0) }, { { 0, 2 } });
    ESM4::PathgridService service;
    service.registerPathgrid(first, key(101));
    service.cellLoaded(key(101));
    service.setNodeEnabled({ key(1), 0 }, false);
    const auto* navigator = service.navigatorPathgrid(key(1));
    const auto generation = service.generation(key(1));
    const std::array<ESM4::PathgridRegistration, 2> ambiguous{ {
        { first, key(101), {} }, { second, key(101), {} },
    } };
    EXPECT_THROW(service = ESM4::PathgridService(ambiguous), std::logic_error);
    const std::array<ESM4::PathgridRegistration, 2> malformed{ {
        { first, key(101), {} }, { invalid, key(103), {} },
    } };
    EXPECT_THROW(service = ESM4::PathgridService(malformed), std::invalid_argument);
    const std::array<ESM4::PathgridRegistration, 1> missingCell{ { { first, {}, {} } } };
    EXPECT_THROW(service = ESM4::PathgridService(missingCell), std::invalid_argument);
    EXPECT_EQ(service.graphCount(), 1u);
    EXPECT_TRUE(service.isCellLoaded(key(101)));
    EXPECT_FALSE(service.isNodeEnabled({ key(1), 0 }));
    EXPECT_EQ(service.generation(key(1)), generation);
    EXPECT_EQ(service.navigatorPathgrid(key(1)), navigator);
}

TEST(ESM4PathgridData, BulkConstructionSupportsEmptyAndReplacedGraphs)
{
    ESM4::PathgridService empty(std::span<const ESM4::PathgridRegistration>{});
    EXPECT_EQ(empty.graphCount(), 0u);
    EXPECT_FALSE(empty.resolveForeignLinks(0.01f));

    const ESM4::Pathgrid first = makeGrid(1, { point(0) });
    const ESM4::Pathgrid replacement = makeGrid(1, { point(20) });
    const std::array<ESM4::PathgridRegistration, 2> registrations{ {
        { first, key(101), {} }, { replacement, key(102), {} },
    } };
    ESM4::PathgridService service(registrations);
    EXPECT_EQ(service.graphCount(), 1u);
    EXPECT_EQ(service.graphForCell(key(101)), nullptr);
    ASSERT_NE(service.graphForCell(key(102)), nullptr);
    EXPECT_EQ(service.graphForCell(key(102))->worldPoint(0), (ESM4::PathgridPoint{ 20, 0, 0 }));
    service.clear();
    EXPECT_EQ(service.graphCount(), 0u);
    EXPECT_TRUE(service.cells().empty());
    EXPECT_EQ(service.navigatorPathgrid(key(1)), nullptr);
    service.registerPathgrid(first, key(101));
    EXPECT_TRUE(service.route({ key(1), 0 }, { key(1), 0 }));
}
