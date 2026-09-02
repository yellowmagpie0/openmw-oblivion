/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#include "pathgriddata.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

namespace
{
    float distanceSquared(ESM4::PathgridPoint left, ESM4::PathgridPoint right)
    {
        const float x = left.mX - right.mX;
        const float y = left.mY - right.mY;
        const float z = left.mZ - right.mZ;
        return x * x + y * y + z * z;
    }

    float distance(ESM4::PathgridPoint left, ESM4::PathgridPoint right)
    {
        return std::sqrt(distanceSquared(left, right));
    }

    bool finite(ESM4::PathgridPoint value)
    {
        return std::isfinite(value.mX) && std::isfinite(value.mY) && std::isfinite(value.mZ);
    }

    ESM4::PathgridRouteResult failure(ESM4::PathgridRouteFailure reason, std::string diagnostic)
    {
        return { std::nullopt, reason, std::move(diagnostic) };
    }

    struct QueueEntry
    {
        float priority;
        float cost;
        ESM4::PathgridNodeKey node;

        bool operator>(const QueueEntry& right) const
        {
            if (priority != right.priority)
                return priority > right.priority;
            if (cost != right.cost)
                return cost > right.cost;
            return right.node < node;
        }
    };

    struct PathgridNodeKeyHash
    {
        std::size_t operator()(const ESM4::PathgridNodeKey& value) const noexcept
        {
            std::size_t result = std::hash<ESM::FormKey>{}(value.mPathgrid);
            result ^= std::hash<std::uint32_t>{}(value.mNode) + 0x9e3779b9 + (result << 6) + (result >> 2);
            return result;
        }
    };

    constexpr std::size_t sPathgridRouteExpansionBudget = 8192;

    // Foreign PGRI records contain a world-space destination point.  The
    // naive resolver compared every such point with every node in every
    // graph, which is quadratic for the full Oblivion data set.  A spatial
    // hash keeps the exact distance/tie checks below while reducing the
    // candidate set to nearby nodes.
    struct SpatialBucket
    {
        std::int64_t mX = 0;
        std::int64_t mY = 0;
        std::int64_t mZ = 0;

        friend bool operator==(const SpatialBucket&, const SpatialBucket&) = default;
    };

    struct SpatialBucketHash
    {
        std::size_t operator()(const SpatialBucket& value) const
        {
            std::size_t result = std::hash<std::int64_t>{}(value.mX);
            result = result * 31 + std::hash<std::int64_t>{}(value.mY);
            result = result * 31 + std::hash<std::int64_t>{}(value.mZ);
            return result;
        }
    };

    struct SpatialNode
    {
        const ESM4::PathgridGraph* mGraph = nullptr;
        ESM4::PathgridNodeKey mKey;
        ESM4::PathgridPoint mWorldPoint;
    };

    std::int32_t navigatorCoordinate(float value)
    {
        if (!std::isfinite(value))
            throw std::invalid_argument("TES4 pathgrid compatibility view contains a non-finite point");
        const double rounded = std::round(static_cast<double>(value));
        if (rounded < static_cast<double>(std::numeric_limits<std::int32_t>::min())
            || rounded > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
            throw std::out_of_range("TES4 pathgrid compatibility view point is outside ESM3 integer range");
        return static_cast<std::int32_t>(rounded);
    }

    ESM::Pathgrid makeNavigatorPathgrid(const ESM4::PathgridGraph& graph)
    {
        ESM::Pathgrid result;
        result.mData.mX = 0;
        result.mData.mY = 0;
        result.mData.mGranularity = 0;
        if (graph.encodedPoints().size() > std::numeric_limits<std::uint16_t>::max())
            throw std::out_of_range("TES4 pathgrid compatibility view has too many points");
        result.mData.mPoints = static_cast<std::uint16_t>(graph.encodedPoints().size());
        result.mPoints.reserve(graph.encodedPoints().size());
        for (const ESM4::PathgridPoint& point : graph.encodedPoints())
        {
            result.mPoints.emplace_back(navigatorCoordinate(point.mX * graph.transform().mScale),
                navigatorCoordinate(point.mY * graph.transform().mScale),
                navigatorCoordinate(point.mZ * graph.transform().mScale));
        }
        result.mEdges.reserve(graph.localEdges().size());
        for (const auto [from, to] : graph.localEdges())
            if (graph.isEnabled(from) && graph.isEnabled(to))
                result.mEdges.push_back({ from, to });
        return result;
    }
}

namespace ESM4
{
    PathgridPoint PathgridTransform::toWorld(PathgridPoint value) const
    {
        if (!std::isfinite(mScale) || mScale <= 0.0f)
            throw std::invalid_argument("TES4 pathgrid transform scale must be positive and finite");
        value.mX *= mScale;
        value.mY *= mScale;
        value.mZ *= mScale;
        if (mCoordinatesAreLocal)
        {
            value.mX += mOrigin.mX;
            value.mY += mOrigin.mY;
            value.mZ += mOrigin.mZ;
        }
        return value;
    }

    PathgridGraph::PathgridGraph(const Pathgrid& definition, const ESM::FormKey& cell, PathgridTransform transform)
        : mPathgridKey(definition.mFormKey)
        , mCellKey(cell)
        , mTransform(transform)
    {
        if (mPathgridKey.isNull())
            throw std::invalid_argument("TES4 pathgrid graph requires a stable PGRD key");
        if (mCellKey.isNull())
            throw std::invalid_argument("TES4 pathgrid graph requires a stable owning cell key");
        if (!std::isfinite(mTransform.mScale) || mTransform.mScale <= 0.0f || !finite(mTransform.mOrigin))
            throw std::invalid_argument("TES4 pathgrid graph has an invalid coordinate transform");

        mPoints.reserve(definition.mNodes.size());
        for (const Pathgrid::PGRP& point : definition.mNodes)
        {
            const PathgridPoint value{ point.x, point.y, point.z };
            if (!finite(value))
                throw std::invalid_argument("TES4 pathgrid contains a non-finite point");
            mPoints.push_back(value);
        }

        mLocalEdges.reserve(definition.mLinks.size());
        for (const Pathgrid::PGRR& edge : definition.mLinks)
        {
            if (edge.startNode == -1 || edge.endNode == -1)
                continue; // -1 is the native no-link sentinel.
            if (edge.startNode < 0 || edge.endNode < 0)
                throw std::invalid_argument("TES4 pathgrid local edge has an invalid negative endpoint");
            if (!contains(static_cast<std::uint32_t>(edge.startNode))
                || !contains(static_cast<std::uint32_t>(edge.endNode)))
                throw std::invalid_argument("TES4 pathgrid local edge is outside the node array");
            mLocalEdges.emplace_back(static_cast<std::uint32_t>(edge.startNode),
                static_cast<std::uint32_t>(edge.endNode));
        }
        std::sort(mLocalEdges.begin(), mLocalEdges.end());
        mLocalEdges.erase(std::unique(mLocalEdges.begin(), mLocalEdges.end()), mLocalEdges.end());

        // Keep the canonical edge list for serialization/debugging, but use a
        // compact CSR adjacency index for routing.  Scanning every edge for
        // every Dijkstra vertex is quadratic on the released city graphs and
        // makes valid exterior pathgrids unusable at startup.
        mOutgoingOffsets.assign(mPoints.size() + 1, 0);
        for (const auto [from, to] : mLocalEdges)
            ++mOutgoingOffsets[from + 1];
        for (std::size_t index = 1; index < mOutgoingOffsets.size(); ++index)
            mOutgoingOffsets[index] += mOutgoingOffsets[index - 1];
        mOutgoingNodes.resize(mLocalEdges.size());
        std::vector<std::uint32_t> outgoingWriteOffsets = mOutgoingOffsets;
        for (const auto [from, to] : mLocalEdges)
            mOutgoingNodes[outgoingWriteOffsets[from]++] = to;

        mForeignLinks.reserve(definition.mForeign.size());
        for (const Pathgrid::PGRI& foreign : definition.mForeign)
        {
            if (!contains(foreign.localNode))
                throw std::invalid_argument("TES4 pathgrid foreign source is outside the node array");
            const PathgridPoint encoded{ foreign.x, foreign.y, foreign.z };
            if (!finite(encoded))
                throw std::invalid_argument("TES4 pathgrid contains a non-finite foreign point");
            mForeignLinks.push_back({ foreign.localNode, encoded, mTransform.toWorld(encoded), std::nullopt, false });
        }
        mForeignLinkOffsets.assign(mPoints.size() + 1, 0);
        for (const PathgridForeignLink& foreign : mForeignLinks)
            ++mForeignLinkOffsets[foreign.mSourceNode + 1];
        for (std::size_t index = 1; index < mForeignLinkOffsets.size(); ++index)
            mForeignLinkOffsets[index] += mForeignLinkOffsets[index - 1];
        mForeignLinkIndices.resize(mForeignLinks.size());
        std::vector<std::uint32_t> foreignWriteOffsets = mForeignLinkOffsets;
        for (std::uint32_t index = 0; index < mForeignLinks.size(); ++index)
            mForeignLinkIndices[foreignWriteOffsets[mForeignLinks[index].mSourceNode]++] = index;

        mObjectLinks.reserve(definition.mObjects.size());
        for (const Pathgrid::PGRL& object : definition.mObjects)
        {
            PathgridObjectLink link;
            link.mObject = object.objectKey;
            link.mNodes.reserve(object.linkedNodes.size());
            for (const std::int32_t node : object.linkedNodes)
            {
                if (node < 0)
                    continue; // PGRL uses -1 for an unused link slot.
                if (!contains(static_cast<std::uint32_t>(node)))
                    throw std::invalid_argument("TES4 pathgrid object link is outside the node array");
                link.mNodes.push_back(static_cast<std::uint32_t>(node));
            }
            mObjectLinks.push_back(std::move(link));
        }
    }

    PathgridPoint PathgridGraph::worldPoint(std::uint32_t node) const
    {
        if (!contains(node))
            throw std::out_of_range("TES4 pathgrid node index is outside the graph");
        return mTransform.toWorld(mPoints[node]);
    }

    bool PathgridGraph::isEnabled(std::uint32_t node) const
    {
        if (!contains(node))
            throw std::out_of_range("TES4 pathgrid node index is outside the graph");
        return !mDisabledNodes.contains(node);
    }

    std::span<const std::uint32_t> PathgridGraph::outgoingNodes(std::uint32_t node) const
    {
        if (!contains(node))
            throw std::out_of_range("TES4 pathgrid node index is outside the graph");
        const std::uint32_t begin = mOutgoingOffsets[node];
        const std::uint32_t end = mOutgoingOffsets[node + 1];
        return std::span<const std::uint32_t>(mOutgoingNodes).subspan(begin, end - begin);
    }

    std::span<const std::uint32_t> PathgridGraph::foreignLinkIndices(std::uint32_t node) const
    {
        if (!contains(node))
            throw std::out_of_range("TES4 pathgrid node index is outside the graph");
        const std::uint32_t begin = mForeignLinkOffsets[node];
        const std::uint32_t end = mForeignLinkOffsets[node + 1];
        return std::span<const std::uint32_t>(mForeignLinkIndices).subspan(begin, end - begin);
    }

    bool PathgridGraph::setNodeEnabled(std::uint32_t node, bool enabled)
    {
        if (!contains(node))
            throw std::out_of_range("TES4 pathgrid node index is outside the graph");
        const bool wasEnabled = isEnabled(node);
        if (wasEnabled == enabled)
            return false;
        if (enabled)
            mDisabledNodes.erase(node);
        else
            mDisabledNodes.insert(node);
        ++mGeneration;
        return true;
    }

    std::optional<std::uint32_t> PathgridGraph::nearestEnabledNode(PathgridPoint world, float maxDistance) const
    {
        if (!finite(world) || (maxDistance < 0.0f ? false : !std::isfinite(maxDistance)))
            return std::nullopt;
        const float maxDistanceSquared = maxDistance < 0.0f ? std::numeric_limits<float>::infinity()
                                                            : maxDistance * maxDistance;
        std::optional<std::uint32_t> result;
        float bestDistance = maxDistanceSquared;
        for (std::uint32_t node = 0; node < mPoints.size(); ++node)
        {
            if (!isEnabled(node))
                continue;
            const float currentDistance = distanceSquared(worldPoint(node), world);
            if (currentDistance <= bestDistance
                && (!result.has_value() || currentDistance < bestDistance || node < *result))
            {
                result = node;
                bestDistance = currentDistance;
            }
        }
        return result;
    }

    PathgridRouteResult PathgridGraph::route(std::uint32_t start, std::uint32_t destination) const
    {
        if (!contains(start) || !contains(destination))
            return failure(PathgridRouteFailure::NoNearPoint, "pathgrid route endpoint is outside the graph");
        if (!isEnabled(start) || !isEnabled(destination))
            return failure(PathgridRouteFailure::Disabled, "pathgrid route endpoint is disabled");

        std::map<std::uint32_t, float> distances;
        std::map<std::uint32_t, std::uint32_t> previous;
        std::priority_queue<std::pair<float, std::uint32_t>, std::vector<std::pair<float, std::uint32_t>>,
            std::greater<>> queue;
        distances[start] = 0.0f;
        queue.emplace(0.0f, start);
        std::size_t expandedNodes = 0;
        while (!queue.empty())
        {
            const auto [currentDistance, current] = queue.top();
            queue.pop();
            if (currentDistance != distances[current])
                continue;
            if (++expandedNodes > sPathgridRouteExpansionBudget)
                return failure(PathgridRouteFailure::SearchLimit,
                    "pathgrid route search budget exceeded");
            if (current == destination)
                break;
            for (const std::uint32_t to : outgoingNodes(current))
            {
                if (!isEnabled(to))
                    continue;
                const float candidate = currentDistance + distance(worldPoint(current), worldPoint(to));
                const auto found = distances.find(to);
                if (found == distances.end() || candidate < found->second)
                {
                    distances[to] = candidate;
                    previous[to] = current;
                    queue.emplace(candidate, to);
                }
            }
        }
        if (!distances.contains(destination))
            return failure(PathgridRouteFailure::DifferentComponent, "pathgrid endpoints are disconnected");

        std::vector<PathgridNodeKey> nodes;
        for (std::uint32_t current = destination;; current = previous.at(current))
        {
            nodes.push_back({ mPathgridKey, current });
            if (current == start)
                break;
        }
        std::reverse(nodes.begin(), nodes.end());
        return { PathgridRoute{ std::move(nodes), distances[destination], mGeneration },
            PathgridRouteFailure::None, {} };
    }

    bool PathgridGraph::resolveForeignLinks(const std::map<ESM::FormKey, const PathgridGraph*>& candidates,
        float tolerance)
    {
        if (!std::isfinite(tolerance) || tolerance < 0.0f)
            throw std::invalid_argument("TES4 pathgrid foreign-link tolerance must be finite and non-negative");
        const float toleranceSquared = tolerance * tolerance;
        bool changed = false;
        for (PathgridForeignLink& link : mForeignLinks)
        {
            const auto oldDestination = link.mDestination;
            const bool oldAmbiguous = link.mAmbiguous;
            link.mDestination.reset();
            link.mAmbiguous = false;

            float bestDistance = toleranceSquared;
            std::optional<PathgridNodeKey> best;
            for (const auto& [key, candidate] : candidates)
            {
                if (candidate == nullptr || key == mPathgridKey)
                    continue;
                for (std::uint32_t node = 0; node < candidate->nodeCount(); ++node)
                {
                    if (!candidate->isEnabled(node))
                        continue;
                    const float currentDistance = distanceSquared(link.mWorldPoint, candidate->worldPoint(node));
                    const PathgridNodeKey candidateKey{ key, node };
                    if (currentDistance > bestDistance)
                        continue;
                    if (!best.has_value() || currentDistance < bestDistance - 0.0001f)
                    {
                        bestDistance = currentDistance;
                        best = candidateKey;
                        link.mAmbiguous = false;
                    }
                    else if (std::abs(currentDistance - bestDistance) <= 0.0001f && candidateKey != *best)
                    {
                        link.mAmbiguous = true;
                        if (candidateKey < *best)
                            best = candidateKey;
                    }
                }
            }
            if (best.has_value() && !link.mAmbiguous)
                link.mDestination = best;
            changed = changed || oldDestination != link.mDestination || oldAmbiguous != link.mAmbiguous;
        }
        if (changed)
            ++mGeneration;
        return changed;
    }

    void PathgridService::registerPathgrid(const Pathgrid& definition, const ESM::FormKey& cell,
        PathgridTransform transform)
    {
        if (definition.mFormKey.isNull() || cell.isNull())
            throw std::invalid_argument("TES4 pathgrid registration requires stable identities");
        const auto cellIt = mCells.find(cell);
        if (cellIt != mCells.end() && cellIt->second != definition.mFormKey)
            throw std::logic_error("More than one winning TES4 pathgrid is registered for a cell");

        // A winning PGRD can be replaced while stores are rebuilt.  Remove
        // the old reverse mapping first so graphForCell never returns a graph
        // that is no longer present in the graph map.
        const auto graphIt = mGraphs.find(definition.mFormKey);
        if (graphIt != mGraphs.end() && graphIt->second.cellKey() != cell)
        {
            const auto oldCellIt = mCells.find(graphIt->second.cellKey());
            if (oldCellIt != mCells.end() && oldCellIt->second == definition.mFormKey)
                mCells.erase(oldCellIt);
        }
        PathgridGraph graph(definition, cell, transform);
        ESM::Pathgrid navigatorPathgrid = makeNavigatorPathgrid(graph);
        mGraphs.insert_or_assign(definition.mFormKey, std::move(graph));
        mNavigatorPathgrids.insert_or_assign(definition.mFormKey, std::move(navigatorPathgrid));
        mCells[cell] = definition.mFormKey;
        if (mForeignLinkTolerance)
            resolveForeignLinks(*mForeignLinkTolerance);
        else
            rebuildCoarseGraphIndex();
    }

    bool PathgridService::unregisterPathgrid(const ESM::FormKey& pathgrid)
    {
        const auto graphIt = mGraphs.find(pathgrid);
        if (graphIt == mGraphs.end())
            return false;
        const ESM::FormKey cell = graphIt->second.cellKey();
        mGraphs.erase(graphIt);
        mNavigatorPathgrids.erase(pathgrid);
        const auto cellIt = mCells.find(cell);
        if (cellIt != mCells.end() && cellIt->second == pathgrid)
            mCells.erase(cellIt);
        mLoadedCells.erase(cell);
        if (mForeignLinkTolerance)
            resolveForeignLinks(*mForeignLinkTolerance);
        else
            rebuildCoarseGraphIndex();
        return true;
    }

    void PathgridService::clear()
    {
        mGraphs.clear();
        mCells.clear();
        mNavigatorPathgrids.clear();
        mForeignLinkTolerance.reset();
        mLoadedCells.clear();
        mCoarseGraphAdjacency.clear();
        mGraphComponents.clear();
    }

    bool PathgridService::cellLoaded(const ESM::FormKey& cell)
    {
        if (cell.isNull())
            throw std::invalid_argument("TES4 pathgrid lifecycle requires a stable cell key");
        return mLoadedCells.insert(cell).second;
    }

    bool PathgridService::cellUnloaded(const ESM::FormKey& cell)
    {
        if (cell.isNull())
            return false;
        return mLoadedCells.erase(cell) != 0;
    }

    bool PathgridService::isCellLoaded(const ESM::FormKey& cell) const
    {
        return !cell.isNull() && mLoadedCells.contains(cell);
    }

    const PathgridGraph* PathgridService::graph(const ESM::FormKey& pathgrid) const
    {
        const auto found = mGraphs.find(pathgrid);
        return found == mGraphs.end() ? nullptr : &found->second;
    }

    PathgridGraph* PathgridService::graph(const ESM::FormKey& pathgrid)
    {
        const auto found = mGraphs.find(pathgrid);
        return found == mGraphs.end() ? nullptr : &found->second;
    }

    const PathgridGraph* PathgridService::graphForCell(const ESM::FormKey& cell) const
    {
        const auto found = mCells.find(cell);
        return found == mCells.end() ? nullptr : graph(found->second);
    }

    const ESM::Pathgrid* PathgridService::navigatorPathgrid(const ESM::FormKey& pathgrid) const
    {
        const auto found = mNavigatorPathgrids.find(pathgrid);
        return found == mNavigatorPathgrids.end() ? nullptr : &found->second;
    }

    void PathgridService::rebuildNavigatorPathgrid(const ESM::FormKey& pathgrid)
    {
        const PathgridGraph* graphValue = graph(pathgrid);
        if (graphValue == nullptr)
            throw std::out_of_range("TES4 pathgrid compatibility view addresses a missing graph");
        mNavigatorPathgrids.insert_or_assign(pathgrid, makeNavigatorPathgrid(*graphValue));
    }

    bool PathgridService::resolveForeignLinks(float tolerance)
    {
        if (!std::isfinite(tolerance) || tolerance < 0.0f)
            throw std::invalid_argument("TES4 pathgrid foreign-link tolerance must be finite and non-negative");
        mForeignLinkTolerance = tolerance;

        // The tolerance is also the largest distance a matching node can be
        // from a foreign point in any one axis.  Keeping the bucket at least
        // 256 units wide means each lookup needs at most the surrounding
        // 3x3x3 buckets for the normal TES4 tolerance, while the exact
        // squared-distance check remains authoritative.
        const double bucketSize = std::max(256.0, static_cast<double>(tolerance));
        const auto bucketFor = [bucketSize](PathgridPoint point) {
            return SpatialBucket{ static_cast<std::int64_t>(std::floor(point.mX / bucketSize)),
                static_cast<std::int64_t>(std::floor(point.mY / bucketSize)),
                static_cast<std::int64_t>(std::floor(point.mZ / bucketSize)) };
        };

        std::size_t nodeCount = 0;
        for (const auto& [key, graphValue] : mGraphs)
            nodeCount += graphValue.nodeCount();
        std::unordered_map<SpatialBucket, std::vector<SpatialNode>, SpatialBucketHash> spatialIndex;
        spatialIndex.reserve(nodeCount);
        for (const auto& [key, graphValue] : mGraphs)
        {
            for (std::uint32_t node = 0; node < graphValue.nodeCount(); ++node)
            {
                if (!graphValue.isEnabled(node))
                    continue;
                const PathgridPoint point = graphValue.worldPoint(node);
                spatialIndex[bucketFor(point)].push_back(
                    { &graphValue, { key, node }, point });
            }
        }

        const float toleranceSquared = tolerance * tolerance;
        bool changed = false;
        for (auto& [key, graphValue] : mGraphs)
        {
            bool graphChanged = false;
            for (PathgridForeignLink& link : graphValue.mForeignLinks)
            {
                const auto oldDestination = link.mDestination;
                const bool oldAmbiguous = link.mAmbiguous;
                link.mDestination.reset();
                link.mAmbiguous = false;

                const SpatialBucket center = bucketFor(link.mWorldPoint);
                float bestDistance = toleranceSquared;
                std::optional<PathgridNodeKey> best;
                for (std::int64_t dx = -1; dx <= 1; ++dx)
                {
                    for (std::int64_t dy = -1; dy <= 1; ++dy)
                    {
                        for (std::int64_t dz = -1; dz <= 1; ++dz)
                        {
                            const auto found = spatialIndex.find(
                                { center.mX + dx, center.mY + dy, center.mZ + dz });
                            if (found == spatialIndex.end())
                                continue;
                            for (const SpatialNode& candidate : found->second)
                            {
                                if (candidate.mGraph == nullptr || candidate.mKey.mPathgrid == key)
                                    continue;
                                const float currentDistance = distanceSquared(link.mWorldPoint, candidate.mWorldPoint);
                                if (currentDistance > toleranceSquared)
                                    continue;

                                if (!best.has_value() || currentDistance < bestDistance - 0.0001f)
                                {
                                    bestDistance = currentDistance;
                                    best = candidate.mKey;
                                    link.mAmbiguous = false;
                                }
                                else if (std::abs(currentDistance - bestDistance) <= 0.0001f)
                                {
                                    if (candidate.mKey != *best)
                                    {
                                        link.mAmbiguous = true;
                                        if (candidate.mKey < *best)
                                            best = candidate.mKey;
                                    }
                                }
                            }
                        }
                    }
                }
                if (best.has_value() && !link.mAmbiguous)
                    link.mDestination = best;
                graphChanged = graphChanged || oldDestination != link.mDestination
                    || oldAmbiguous != link.mAmbiguous;
            }
            if (graphChanged)
                ++graphValue.mGeneration;
            changed = graphChanged || changed;
        }
        rebuildCoarseGraphIndex();
        return changed;
    }

    void PathgridService::rebuildCoarseGraphIndex()
    {
        mCoarseGraphAdjacency.clear();
        mGraphComponents.clear();
        for (const auto& [key, _] : mGraphs)
            mCoarseGraphAdjacency.emplace(key, std::vector<ESM::FormKey>{});

        for (const auto& [key, graphValue] : mGraphs)
        {
            auto sourceAdjacency = mCoarseGraphAdjacency.find(key);
            if (sourceAdjacency == mCoarseGraphAdjacency.end())
                continue;
            for (const PathgridForeignLink& link : graphValue.mForeignLinks)
            {
                if (link.mAmbiguous || !link.mDestination || !graphValue.isEnabled(link.mSourceNode))
                    continue;
                const auto destinationGraph = mGraphs.find(link.mDestination->mPathgrid);
                if (destinationGraph == mGraphs.end()
                    || !destinationGraph->second.isEnabled(link.mDestination->mNode))
                    continue;
                sourceAdjacency->second.push_back(link.mDestination->mPathgrid);
                mCoarseGraphAdjacency[link.mDestination->mPathgrid].push_back(key);
            }
        }

        for (auto& [_, neighbours] : mCoarseGraphAdjacency)
        {
            std::sort(neighbours.begin(), neighbours.end());
            neighbours.erase(std::unique(neighbours.begin(), neighbours.end()), neighbours.end());
        }

        std::set<ESM::FormKey> visited;
        std::size_t component = 0;
        for (const auto& [key, _] : mCoarseGraphAdjacency)
        {
            if (!visited.insert(key).second)
                continue;
            std::queue<ESM::FormKey> queue;
            queue.push(key);
            mGraphComponents[key] = component;
            while (!queue.empty())
            {
                const ESM::FormKey current = queue.front();
                queue.pop();
                const auto neighbours = mCoarseGraphAdjacency.find(current);
                if (neighbours == mCoarseGraphAdjacency.end())
                    continue;
                for (const ESM::FormKey& neighbour : neighbours->second)
                {
                    if (visited.insert(neighbour).second)
                    {
                        mGraphComponents[neighbour] = component;
                        queue.push(neighbour);
                    }
                }
            }
            ++component;
        }
    }

    void PathgridService::classifyObjectLinks(const ObjectResolver& resolver)
    {
        for (auto& [key, graphValue] : mGraphs)
        {
            for (PathgridObjectLink& link : graphValue.mObjectLinks)
                link.mKind = resolver ? resolver(link.mObject) : PathgridObjectKind::Unknown;
        }
    }

    bool PathgridService::setNodeEnabled(const PathgridNodeKey& node, bool enabled)
    {
        PathgridGraph* value = graph(node.mPathgrid);
        if (value == nullptr)
            throw std::out_of_range("TES4 pathgrid overlay addresses a missing graph");
        const bool changed = value->setNodeEnabled(node.mNode, enabled);
        if (changed)
        {
            rebuildNavigatorPathgrid(node.mPathgrid);
            rebuildCoarseGraphIndex();
        }
        return changed;
    }

    bool PathgridService::isNodeEnabled(const PathgridNodeKey& node) const
    {
        const PathgridGraph* value = graph(node.mPathgrid);
        return value != nullptr && value->isEnabled(node.mNode);
    }

    std::uint64_t PathgridService::generation(const ESM::FormKey& pathgrid) const
    {
        const PathgridGraph* value = graph(pathgrid);
        return value == nullptr ? 0 : value->generation();
    }

    std::optional<PathgridNodeKey> PathgridService::nearestEnabledNode(
        const ESM::FormKey& pathgrid, PathgridPoint world, float maxDistance) const
    {
        const PathgridGraph* value = graph(pathgrid);
        if (value == nullptr)
            return std::nullopt;
        const auto node = value->nearestEnabledNode(world, maxDistance);
        if (!node)
            return std::nullopt;
        return PathgridNodeKey{ pathgrid, *node };
    }

    PathgridRouteResult PathgridService::route(
        const PathgridNodeKey& start, const PathgridNodeKey& destination) const
    {
        const PathgridGraph* startGraph = graph(start.mPathgrid);
        const PathgridGraph* destinationGraph = graph(destination.mPathgrid);
        if (startGraph == nullptr || destinationGraph == nullptr)
            return failure(PathgridRouteFailure::NoGrid, "pathgrid route references a missing graph");
        if (!startGraph->contains(start.mNode) || !destinationGraph->contains(destination.mNode))
            return failure(PathgridRouteFailure::NoNearPoint, "pathgrid route references a missing node");
        if (!startGraph->isEnabled(start.mNode) || !destinationGraph->isEnabled(destination.mNode))
            return failure(PathgridRouteFailure::Disabled, "pathgrid route endpoint is disabled");

        if (start.mPathgrid != destination.mPathgrid)
        {
            const bool sourceHasUnresolvedForeign = std::any_of(startGraph->foreignLinks().begin(),
                startGraph->foreignLinks().end(), [&](const PathgridForeignLink& link) {
                    return link.mSourceNode == start.mNode && (link.mAmbiguous || !link.mDestination);
                });
            const auto startComponent = mGraphComponents.find(start.mPathgrid);
            const auto destinationComponent = mGraphComponents.find(destination.mPathgrid);
            if (startComponent == mGraphComponents.end() || destinationComponent == mGraphComponents.end()
                || startComponent->second != destinationComponent->second)
            {
                if (sourceHasUnresolvedForeign)
                    return failure(PathgridRouteFailure::UnresolvedForeignLink,
                        "pathgrid route encountered an unresolved foreign link");
                return failure(PathgridRouteFailure::DifferentComponent,
                    "pathgrid route endpoints are in different coarse components");
            }
        }

        std::unordered_map<PathgridNodeKey, float, PathgridNodeKeyHash> distances;
        std::unordered_map<PathgridNodeKey, PathgridNodeKey, PathgridNodeKeyHash> previous;
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<>> queue;
        const PathgridPoint destinationPoint = destinationGraph->worldPoint(destination.mNode);
        distances[start] = 0.0f;
        queue.push({ distance(startGraph->worldPoint(start.mNode), destinationPoint), 0.0f, start });
        bool sawUnresolvedForeign = false;
        std::size_t expandedNodes = 0;
        while (!queue.empty())
        {
            const QueueEntry current = queue.top();
            queue.pop();
            const auto distanceIt = distances.find(current.node);
            if (distanceIt == distances.end() || current.cost != distanceIt->second)
                continue;
            if (++expandedNodes > sPathgridRouteExpansionBudget)
                return failure(PathgridRouteFailure::SearchLimit,
                    "pathgrid route search budget exceeded");
            if (current.node == destination)
                break;
            const PathgridGraph* graphValue = graph(current.node.mPathgrid);
            if (graphValue == nullptr)
                continue;

            auto relax = [&](const PathgridNodeKey& target, float cost) {
                const PathgridGraph* targetGraph = graph(target.mPathgrid);
                if (targetGraph == nullptr || !targetGraph->contains(target.mNode)
                    || !targetGraph->isEnabled(target.mNode))
                    return;
                const float candidate = current.cost + cost;
                const auto found = distances.find(target);
                const auto previousIt = previous.find(target);
                if (found == distances.end() || candidate < found->second
                    || (candidate == found->second
                        && (previousIt == previous.end() || current.node < previousIt->second)))
                {
                    distances[target] = candidate;
                    previous[target] = current.node;
                    const PathgridPoint targetPoint = targetGraph->worldPoint(target.mNode);
                    queue.push({ candidate + distance(targetPoint, destinationPoint), candidate, target });
                }
            };

            for (const std::uint32_t to : graphValue->outgoingNodes(current.node.mNode))
            {
                if (graphValue->isEnabled(to))
                    relax({ current.node.mPathgrid, to },
                        distance(graphValue->worldPoint(current.node.mNode), graphValue->worldPoint(to)));
            }
            for (const std::uint32_t foreignIndex : graphValue->foreignLinkIndices(current.node.mNode))
            {
                const PathgridForeignLink& foreign = graphValue->foreignLinks()[foreignIndex];
                if (!foreign.mDestination || foreign.mAmbiguous)
                {
                    sawUnresolvedForeign = true;
                    continue;
                }
                const PathgridNodeKey target = *foreign.mDestination;
                const PathgridGraph* targetGraph = graph(target.mPathgrid);
                if (targetGraph == nullptr)
                {
                    sawUnresolvedForeign = true;
                    continue;
                }
                relax(target, distance(graphValue->worldPoint(current.node.mNode), targetGraph->worldPoint(target.mNode)));
            }
        }

        if (!distances.contains(destination))
        {
            if (sawUnresolvedForeign)
                return failure(PathgridRouteFailure::UnresolvedForeignLink,
                    "pathgrid route encountered an unresolved foreign link");
            return failure(PathgridRouteFailure::DifferentComponent, "pathgrid route endpoints are disconnected");
        }

        std::vector<PathgridNodeKey> nodes;
        for (PathgridNodeKey current = destination;; current = previous.at(current))
        {
            nodes.push_back(current);
            if (current == start)
                break;
        }
        std::reverse(nodes.begin(), nodes.end());
        std::uint64_t generationValue = 0;
        for (const PathgridNodeKey& node : nodes)
            generationValue = std::max(generationValue, generation(node.mPathgrid));
        return { PathgridRoute{ std::move(nodes), distances[destination], generationValue },
            PathgridRouteFailure::None, {} };
    }

    std::set<PathgridNodeKey> PathgridService::reachableComponent(const PathgridNodeKey& start) const
    {
        std::set<PathgridNodeKey> result;
        const PathgridGraph* graphValue = graph(start.mPathgrid);
        if (graphValue == nullptr || !graphValue->contains(start.mNode) || !graphValue->isEnabled(start.mNode))
            return result;
        std::queue<PathgridNodeKey> queue;
        queue.push(start);
        result.insert(start);
        while (!queue.empty())
        {
            const PathgridNodeKey current = queue.front();
            queue.pop();
            const PathgridGraph* currentGraph = graph(current.mPathgrid);
            if (currentGraph == nullptr)
                continue;
            auto visit = [&](const PathgridNodeKey& node) {
                const PathgridGraph* targetGraph = graph(node.mPathgrid);
                if (targetGraph != nullptr && targetGraph->contains(node.mNode) && targetGraph->isEnabled(node.mNode)
                    && result.insert(node).second)
                    queue.push(node);
            };
            for (const std::uint32_t to : currentGraph->outgoingNodes(current.mNode))
                visit({ current.mPathgrid, to });
            for (const std::uint32_t foreignIndex : currentGraph->foreignLinkIndices(current.mNode))
            {
                const PathgridForeignLink& foreign = currentGraph->foreignLinks()[foreignIndex];
                if (foreign.mSourceNode == current.mNode && foreign.mDestination && !foreign.mAmbiguous)
                    visit(*foreign.mDestination);
            }
        }
        return result;
    }

    std::vector<PathgridNodeKey> PathgridService::disabledNodes() const
    {
        std::vector<PathgridNodeKey> result;
        for (const auto& [key, graphValue] : mGraphs)
            for (const std::uint32_t node : graphValue.disabledNodes())
                result.push_back({ key, node });
        return result;
    }

    void PathgridService::applyOverlay(const PathgridNodeKey& node, bool enabled)
    {
        // A saved overlay may outlive a removed DLC record.  It is safer to
        // report that as a recoverable load diagnostic than to retarget the
        // node to another graph with the same numeric index.
        if (graph(node.mPathgrid) == nullptr)
            throw std::out_of_range("TES4 pathgrid overlay addresses a missing graph");
        static_cast<void>(setNodeEnabled(node, enabled));
    }
}
