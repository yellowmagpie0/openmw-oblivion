#ifndef OPENMW_MWMECHANICS_OBLIVIONAIDESTINATION_H
#define OPENMW_MWMECHANICS_OBLIVIONAIDESTINATION_H

#include <algorithm>
#include <cmath>
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
}

#endif
