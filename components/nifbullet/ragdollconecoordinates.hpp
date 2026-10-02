#ifndef OPENMW_COMPONENTS_NIFBULLET_RAGDOLLCONECOORDINATES_HPP
#define OPENMW_COMPONENTS_NIFBULLET_RAGDOLLCONECOORDINATES_HPP

#include "actorragdoll.hpp"

namespace NifBullet
{
    struct RagdollAngularLimit
    {
        osg::Vec3f mAxis;
        float mMin, mMax, mAngle;
    };

    // Evaluate the original ragdoll's separate cone, plane and twist rows.
    // Both frames must already be in world coordinates. This describes the
    // angular rows; it does not integrate motion or solve their impulses.
    std::vector<RagdollAngularLimit> ragdollConeCoordinates(const RagdollConeJoint& joint,
        const RagdollJointFrame& worldA, const RagdollJointFrame& worldB);
}

#endif
