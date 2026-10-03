#ifndef OPENMW_COMPONENTS_NIFBULLET_RAGDOLLVELOCITY_HPP
#define OPENMW_COMPONENTS_NIFBULLET_RAGDOLLVELOCITY_HPP

#include <osg/Vec3f>

namespace NifBullet
{
    inline constexpr float RagdollNativeDefaultGravityZ = -73.57500457763672f;

    // Original world+190 gravity delta, before the motion damping/cap stores.
    osg::Vec3f ragdollNativeGravityDelta(const osg::Vec3f& gravity, float frameSeconds);

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
        // Raw original motion field +B8; the loaded-body factory copies it unchanged.
        float mAngularLimit;
    };

    // Original loaded bhk body limit preparation. Load caps linear speed at
    // 250; construction raises it to at least 250. Angular speed is copied
    // unchanged into motion+B8. This is not the general motion factory policy.
    RagdollMotionLimits ragdollLoadedMotionLimits(float linearDamping, float angularDamping,
        float maxLinearVelocity, float maxAngularVelocity);

    // Original sphere/box-motion velocity stores, in native units: the supplied
    // velocity delta precedes damping, linear speed and angular rotation caps.
    // This does not integrate a transform or prepare a world gravity delta.
    RagdollNativeVelocities ragdollNativeVelocityStep(const RagdollNativeVelocities& input,
        const RagdollMotionLimits& limits, float frameSeconds, const osg::Vec3f& linearDelta);
}

#endif
