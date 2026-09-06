#ifndef OPENMW_MWMECHANICS_OBLIVIONAIDESTINATION_H
#define OPENMW_MWMECHANICS_OBLIVIONAIDESTINATION_H

#include <osg/Vec3f>

#include <components/esm4/runtimestate.hpp>

namespace MWMechanics
{
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
