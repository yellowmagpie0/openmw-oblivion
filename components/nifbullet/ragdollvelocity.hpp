#ifndef OPENMW_COMPONENTS_NIFBULLET_RAGDOLLVELOCITY_HPP
#define OPENMW_COMPONENTS_NIFBULLET_RAGDOLLVELOCITY_HPP

#include <osg/Vec3f>

namespace NifBullet
{
    struct RagdollNativeVelocities
    {
        osg::Vec3f mLinear;
        osg::Vec3f mAngular;
    };

    struct RagdollMotionLimits
    {
        float mLinearDamping;
        float mAngularDamping;
        float mMaxLinearVelocity;
        // Raw original motion field +B8, not serialized NIF maxAngularVelocity.
        float mAngularLimit;
    };

    // Original sphere-motion velocity stores, in native units: the supplied
    // velocity delta precedes damping, linear speed and angular rotation caps.
    // This does not integrate a transform or prepare a world gravity delta.
    RagdollNativeVelocities ragdollNativeVelocityStep(const RagdollNativeVelocities& input,
        const RagdollMotionLimits& limits, float frameSeconds, const osg::Vec3f& linearDelta);
}

#endif
