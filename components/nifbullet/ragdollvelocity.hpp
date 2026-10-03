#ifndef OPENMW_COMPONENTS_NIFBULLET_RAGDOLLVELOCITY_HPP
#define OPENMW_COMPONENTS_NIFBULLET_RAGDOLLVELOCITY_HPP

#include <osg/Vec3f>
#include <optional>

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

    // Native8A37E8 mixes current velocities with already prepared/capped
    // pose-target velocities. A present world additionally compensates Z
    // gravity by gain/inverseFrameSeconds before the ordinary world step.
    // This does not prepare pose targets, select motion modes, wake bodies or
    // publish velocities. The gain is finite but deliberately not clamped.
    RagdollNativeVelocities ragdollNativeBlendVelocities(const RagdollNativeVelocities& current,
        const RagdollNativeVelocities& target, float velocityGain, float inverseFrameSeconds,
        std::optional<float> worldGravityZ);

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
