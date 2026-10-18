#ifndef OPENMW_COMPONENTS_NIFBULLET_NATIVECOMPOUNDCOLLISION_HPP
#define OPENMW_COMPONENTS_NIFBULLET_NATIVECOMPOUNDCOLLISION_HPP

#include <BulletCollision/CollisionShapes/btCollisionShape.h>

namespace NifBullet
{
    // A sphere centered on the rigid body's mass center, contained within one
    // convex part. Zero means no such interior was established. Do not replace
    // a concave compound with its convex hull for collision detection.
    btScalar centeredCompoundSweepRadius(const btCollisionShape& shape);
}

#endif
