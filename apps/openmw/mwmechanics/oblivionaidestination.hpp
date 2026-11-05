#ifndef OPENMW_MWMECHANICS_OBLIVIONAIDESTINATION_H
#define OPENMW_MWMECHANICS_OBLIVIONAIDESTINATION_H

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>
#include <set>
#include <span>

#include <osg/Vec3f>

#include <components/esm4/runtimestate.hpp>
#include <components/esm4/pathgriddata.hpp>

namespace MWMechanics
{
    inline bool oblivionDestinationReached(const ESM::FormKey& cell, const osg::Vec3f& position,
        const ESM::FormKey& destinationCell, const osg::Vec3f& destination, float tolerance,
        ESM4::AIPackageType type = ESM4::AIPackageType::Unknown, float travelRadius = 0.f)
    {
        // A positive native Travel radius defines the arrival region; a
        // zero-radius marker specifies a standing place, unlike route nodes.
        // Keep the vertical allowance for authored markers below the settled
        // actor's feet, without allowing an actor to occupy a neighbour's slot.
        if (type == ESM4::AIPackageType::Travel)
        {
            const osg::Vec3f delta = position - destination;
            const float horizontal = travelRadius > 0.f ? travelRadius : std::min(16.f, tolerance);
            return !cell.isNull() && cell == destinationCell && std::isfinite(tolerance) && tolerance >= 0.f
                && std::isfinite(travelRadius) && travelRadius >= 0.f
                && std::abs(delta.z()) <= tolerance
                && delta.x() * delta.x() + delta.y() * delta.y() <= horizontal * horizontal;
        }
        return !cell.isNull() && cell == destinationCell && std::isfinite(tolerance) && tolerance >= 0.f
            && (position - destination).length2() <= tolerance * tolerance;
    }

    inline bool oblivionFollowDestinationReached(const ESM::FormKey& targetCell,
        const osg::Vec3f& targetPosition, const ESM::FormKey& destinationCell,
        const osg::Vec3f& destination, std::int32_t radius)
    {
        return radius >= 0 && oblivionDestinationReached(targetCell, targetPosition,
            destinationCell, destination, 64.f, ESM4::AIPackageType::Travel,
            static_cast<float>(radius));
    }

    // Resident fallback paths must reach the interior of a corner before
    // turning toward its successor. Authored node Z can differ from the
    // collision surface, so keep vertical tolerance separate from XY.
    inline bool oblivionPathgridWaypointReached(const osg::Vec3f& position, const osg::Vec3f& waypoint)
    {
        const auto delta = position - waypoint;
        return delta.x() * delta.x() + delta.y() * delta.y() <= 16.f * 16.f
            && std::abs(delta.z()) <= 64.f;
    }

    inline bool oblivionRouteTailWithinDirectApproach(
        const osg::Vec3f& position, const osg::Vec3f& destination, float maximumDistance)
    {
        return std::isfinite(maximumDistance) && maximumDistance >= 0.f
            && (position - destination).length2() <= maximumDistance * maximumDistance;
    }

    inline bool oblivionRouteAffectedByOverlay(ESM4::PackagePhase phase,
        const ESM::FormKey& currentPathgrid, std::span<const ESM4::PathgridNodeKey> route,
        const std::set<ESM::FormKey>& changedPathgrids)
    {
        if (phase != ESM4::PackagePhase::Path)
            return false;
        if (changedPathgrids.contains(currentPathgrid))
            return true;
        for (const auto& node : route)
            if (changedPathgrids.contains(node.mPathgrid))
                return true;
        return false;
    }

    inline bool preserveOblivionAbstractPosition(bool dirty, bool hasObservedPosition,
        const ESM::FormKey& observedCell, const ESM::FormKey& previousObservedCell,
        const osg::Vec3f& observedPosition, const osg::Vec3f& previousObservedPosition)
    {
        return dirty && (!hasObservedPosition || (observedCell == previousObservedCell
            && (observedPosition - previousObservedPosition).length2() <= 64.f * 64.f));
    }

    // The persisted destination is the last committed route intent, not the
    // last sampled target position. Keep it fixed until cumulative movement
    // warrants a new route, including across process-tier changes and reloads.
    inline bool updateOblivionMovingDestination(
        ESM4::RuntimeActorAiState& state, const ESM::FormKey& cell, const osg::Vec3f& position)
    {
        if (state.mHasDestination && state.mDestinationCell == cell
            && (state.mDestinationPosition.asVec3() - position).length2() <= 96.f * 96.f)
            return false;

        state.mDestinationCell = cell;
        state.mDestinationPosition.pos[0] = position.x();
        state.mDestinationPosition.pos[1] = position.y();
        state.mDestinationPosition.pos[2] = position.z();
        state.mHasDestination = true;
        return true;
    }
    // Recast may route around a short authored drop through a scripted opening.
    // A nearby complete native path can steer around occluding scenery;
    // it only supplies steering points. Collision and actual contact still own
    // movement and hits. Keep the continuous route unless the improvement exceeds
    // two waypoint arrival tolerances, avoiding churn between equivalent paths.
    inline std::optional<std::vector<osg::Vec3f>> shorterOblivionPathgridApproach(
        const ESM4::PathgridService& service, const ESM::FormKey& cell,
        const osg::Vec3f& position, const osg::Vec3f& destination,
        std::span<const osg::Vec3f> continuous)
    {
        if (cell.isNull()) return std::nullopt;
        const auto* graph = service.graphForCell(cell);
        if (!graph) return std::nullopt;
        const auto start = service.nearestEnabledNode(graph->pathgridKey(),
            {position.x(), position.y(), position.z()}, 256.f);
        const auto end = service.nearestEnabledNode(graph->pathgridKey(),
            {destination.x(), destination.y(), destination.z()}, 256.f);
        if (!start || !end) return std::nullopt;
        const auto path = service.route(*start, *end);
        if (!path) return std::nullopt;
        std::vector<osg::Vec3f> points;
        points.reserve(path.mRoute->mNodes.size() + 1);
        for (const auto& node : path.mRoute->mNodes)
        {
            const auto* part = service.graph(node.mPathgrid);
            if (!part || part->cellKey() != cell) return std::nullopt;
            const auto point = part->worldPoint(node.mNode);
            points.emplace_back(point.mX, point.mY, point.mZ);
        }
        points.push_back(destination);
        const auto length = [&](std::span<const osg::Vec3f> route) {
            float result = 0;
            auto previous = position;
            for (const auto& point : route)
            {
                result += (point - previous).length();
                previous = point;
            }
            return result + (destination - previous).length();
        };
        if (!continuous.empty() && length(points) + 32.f >= length(continuous))
            return std::nullopt;
        return points;
    }

}

#endif
