#ifndef OPENMW_MWPHYSICS_ACTORPLACEMENT_H
#define OPENMW_MWPHYSICS_ACTORPLACEMENT_H

#include <osg/Vec3f>

class btCollisionObject;

namespace MWPhysics
{
    class PhysicsTaskScheduler;

    // Upright movement capsule. Test destination occupancy against
    // actual bodies and the intervening path against static scene geometry.
    // Queries never register a body or change an actor's position.
    bool isActorPlacementClear(PhysicsTaskScheduler& scheduler, const btCollisionObject* ignoredActor,
        const osg::Vec3f& fromFeet, const osg::Vec3f& toFeet, const osg::Vec3f& halfExtents,
        bool actorsOnly = false);
}

#endif
