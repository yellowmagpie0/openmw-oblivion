/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#ifndef ESM4_PATHGRIDDATA_H
#define ESM4_PATHGRIDDATA_H

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#include <components/esm/formkey.hpp>
#include <components/esm3/loadpgrd.hpp>

#include "loadpgrd.hpp"

namespace ESM4
{
    struct PathgridPoint
    {
        float mX = 0.0f;
        float mY = 0.0f;
        float mZ = 0.0f;

        friend bool operator==(const PathgridPoint&, const PathgridPoint&) = default;
    };

    struct PathgridTransform
    {
        // TES4 PGRP coordinates are retained as encoded.  The adapter is
        // given the cell-space origin by the world layer; this keeps interior
        // and exterior coordinate policy out of the binary record parser.
        PathgridPoint mOrigin;
        float mScale = 1.0f;
        bool mCoordinatesAreLocal = true;

        PathgridPoint toWorld(PathgridPoint value) const;
    };

    struct PathgridNodeKey
    {
        ESM::FormKey mPathgrid;
        std::uint32_t mNode = 0;

        friend bool operator==(const PathgridNodeKey&, const PathgridNodeKey&) = default;
        friend bool operator<(const PathgridNodeKey& left, const PathgridNodeKey& right)
        {
            return std::tie(left.mPathgrid, left.mNode) < std::tie(right.mPathgrid, right.mNode);
        }
    };

    enum class PathgridRouteFailure : std::uint8_t
    {
        None,
        NoGrid,
        NoNearPoint,
        DifferentComponent,
        Disabled,
        UnresolvedForeignLink,
        InvalidGraph,
        SearchLimit,
    };

    struct PathgridRoute
    {
        std::vector<PathgridNodeKey> mNodes;
        float mCost = 0.0f;
        std::uint64_t mGeneration = 0;
    };

    struct PathgridRouteResult
    {
        std::optional<PathgridRoute> mRoute;
        PathgridRouteFailure mFailure = PathgridRouteFailure::None;
        std::string mDiagnostic;

        explicit operator bool() const { return mRoute.has_value(); }
    };

    struct PathgridForeignLink
    {
        std::uint32_t mSourceNode = 0;
        PathgridPoint mEncodedPoint;
        PathgridPoint mWorldPoint;
        std::optional<PathgridNodeKey> mDestination;
        bool mAmbiguous = false;
    };

    enum class PathgridObjectKind : std::uint8_t
    {
        Unknown,
        Door,
        Furniture,
        Other,
        Missing,
        Deleted,
    };

    struct PathgridObjectLink
    {
        ESM::FormKey mObject;
        std::vector<std::uint32_t> mNodes;
        PathgridObjectKind mKind = PathgridObjectKind::Unknown;
    };

    class PathgridGraph
    {
    public:
        PathgridGraph() = default;
        PathgridGraph(const Pathgrid& definition, const ESM::FormKey& cell, PathgridTransform transform);

        const ESM::FormKey& pathgridKey() const { return mPathgridKey; }
        const ESM::FormKey& cellKey() const { return mCellKey; }
        const PathgridTransform& transform() const { return mTransform; }
        std::uint64_t generation() const { return mGeneration; }
        std::size_t nodeCount() const { return mPoints.size(); }
        const std::vector<PathgridPoint>& encodedPoints() const { return mPoints; }
        PathgridPoint worldPoint(std::uint32_t node) const;
        bool contains(std::uint32_t node) const { return node < mPoints.size(); }
        bool isEnabled(std::uint32_t node) const;
        bool setNodeEnabled(std::uint32_t node, bool enabled);
        const std::set<std::uint32_t>& disabledNodes() const { return mDisabledNodes; }
        const std::vector<std::pair<std::uint32_t, std::uint32_t>>& localEdges() const { return mLocalEdges; }
        std::span<const std::uint32_t> outgoingNodes(std::uint32_t node) const;
        const std::vector<PathgridForeignLink>& foreignLinks() const { return mForeignLinks; }
        std::span<const std::uint32_t> foreignLinkIndices(std::uint32_t node) const;
        std::vector<PathgridObjectLink>& objectLinks() { return mObjectLinks; }
        const std::vector<PathgridObjectLink>& objectLinks() const { return mObjectLinks; }

        std::optional<std::uint32_t> nearestEnabledNode(PathgridPoint world, float maxDistance = -1.0f) const;
        PathgridRouteResult route(std::uint32_t start, std::uint32_t destination) const;

        // Called by PathgridService after all winning graphs have been
        // registered.  Resolution is deterministic and does not mutate the
        // native Pathgrid definition.
        bool resolveForeignLinks(const std::map<ESM::FormKey, const PathgridGraph*>& candidates, float tolerance);

    private:
        ESM::FormKey mPathgridKey;
        ESM::FormKey mCellKey;
        PathgridTransform mTransform;
        std::vector<PathgridPoint> mPoints;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> mLocalEdges;
        std::vector<std::uint32_t> mOutgoingOffsets;
        std::vector<std::uint32_t> mOutgoingNodes;
        std::vector<PathgridForeignLink> mForeignLinks;
        std::vector<std::uint32_t> mForeignLinkOffsets;
        std::vector<std::uint32_t> mForeignLinkIndices;
        std::vector<PathgridObjectLink> mObjectLinks;
        std::set<std::uint32_t> mDisabledNodes;
        std::uint64_t mGeneration = 1;

        friend class PathgridService;
    };

    class PathgridService
    {
    public:
        using ObjectResolver = std::function<PathgridObjectKind(const ESM::FormKey&)>;

        void registerPathgrid(const Pathgrid& definition, const ESM::FormKey& cell,
            PathgridTransform transform = {});
        bool unregisterPathgrid(const ESM::FormKey& pathgrid);
        void clear();

        // Scene owns the loaded-cell lifetime, while the service owns the
        // immutable graph definitions used by both high- and low-process AI.
        // Keeping this marker separate means unloading a cell cannot destroy
        // a graph needed to advance an unloaded actor, and repeated scene
        // notifications remain idempotent.
        bool cellLoaded(const ESM::FormKey& cell);
        bool cellUnloaded(const ESM::FormKey& cell);
        bool isCellLoaded(const ESM::FormKey& cell) const;
        std::size_t loadedCellCount() const { return mLoadedCells.size(); }

        const PathgridGraph* graph(const ESM::FormKey& pathgrid) const;
        PathgridGraph* graph(const ESM::FormKey& pathgrid);
        const PathgridGraph* graphForCell(const ESM::FormKey& cell) const;
        std::size_t graphCount() const { return mGraphs.size(); }

        bool resolveForeignLinks(float tolerance);
        void classifyObjectLinks(const ObjectResolver& resolver);

        bool setNodeEnabled(const PathgridNodeKey& node, bool enabled);
        bool isNodeEnabled(const PathgridNodeKey& node) const;
        std::uint64_t generation(const ESM::FormKey& pathgrid) const;
        std::optional<PathgridNodeKey> nearestEnabledNode(
            const ESM::FormKey& pathgrid, PathgridPoint world, float maxDistance = -1.0f) const;
        PathgridRouteResult route(const PathgridNodeKey& start, const PathgridNodeKey& destination) const;
        std::set<PathgridNodeKey> reachableComponent(const PathgridNodeKey& start) const;

        const std::map<ESM::FormKey, PathgridGraph>& graphs() const { return mGraphs; }
        const std::map<ESM::FormKey, ESM::FormKey>& cells() const { return mCells; }

        // Detour's public pathgrid API is currently expressed in the ESM3
        // record type.  These owned views contain the winning native TES4
        // graph points/edges and are never taken from the binary store.  The
        // FormKey-keyed map keeps their addresses stable for ObjectId-based
        // off-mesh connections until the graph is explicitly unregistered.
        const ESM::Pathgrid* navigatorPathgrid(const ESM::FormKey& pathgrid) const;

        // Runtime overlays are deliberately exposed as a value view.  The
        // native graph remains the source of truth; callers persist only the
        // changed node states and reapply them after the winning graphs have
        // been rebuilt.
        std::vector<PathgridNodeKey> disabledNodes() const;
        void applyOverlay(const PathgridNodeKey& node, bool enabled);

    private:
        std::map<ESM::FormKey, PathgridGraph> mGraphs;
        std::map<ESM::FormKey, ESM::FormKey> mCells;
        std::map<ESM::FormKey, ESM::Pathgrid> mNavigatorPathgrids;
        // A resolved foreign-link set is part of the live graph lifecycle.
        // Remember the policy so adding/removing a loaded cell cannot leave
        // stale cross-cell edges behind.
        std::optional<float> mForeignLinkTolerance;
        std::set<ESM::FormKey> mLoadedCells;
        // A weak component index is a cheap coarse graph for cross-cell
        // reachability. It rejects obviously disconnected world routes before
        // the exact node-level search has to inspect thousands of graphs.
        std::map<ESM::FormKey, std::vector<ESM::FormKey>> mCoarseGraphAdjacency;
        std::map<ESM::FormKey, std::size_t> mGraphComponents;

        void rebuildNavigatorPathgrid(const ESM::FormKey& pathgrid);
        void rebuildCoarseGraphIndex();
    };
}

#endif // ESM4_PATHGRIDDATA_H
