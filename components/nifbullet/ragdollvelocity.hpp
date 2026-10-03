#ifndef OPENMW_COMPONENTS_NIFBULLET_RAGDOLLVELOCITY_HPP
#define OPENMW_COMPONENTS_NIFBULLET_RAGDOLLVELOCITY_HPP

#include <osg/Vec3f>
#include <optional>
#include <array>

namespace NifBullet
{
    inline constexpr float RagdollNativeDefaultGravityZ = -73.57500457763672f;

    // Original89DA50/89DAC0 dynamic mass setter, including zero's inverse0.
    // This is the scalar store, not a body/inertia factory or mass projection.
    float ragdollNativeInverseMass(float mass);

    // Original dynamic motion virtual+6C8EAC80: three separate SSE stores
    // (frame*force, inverseMass*product, current+delta). Native units; no
    // damping/cap, activation, body publication or keyframed dispatch.
    std::array<float, 4> ragdollNativeLinearVelocityAfterForce(const std::array<float, 4>& current,
        float inverseMass, float frameSeconds, const std::array<float, 4>& force);

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

    struct RagdollNativeTargetPose
    {
        osg::Vec3f mPosition;
        // Native quaternion layout X,Y,Z,W; world/native length units.
        std::array<float, 4> mRotation;
    };

    // Native88F5A4 velocity-drive target, not the flag-dependent scene pose.
    // Gain is finite and unclamped. Both input rotations must be near-unit.
    // Unrepresentable results and spherical FSIN arguments outside +/-2^63
    // reject. Portable quaternion libm/reciprocal-square-root arithmetic.
    RagdollNativeTargetPose ragdollNativeBlendTargetPose(const RagdollNativeTargetPose& physical,
        const RagdollNativeTargetPose& animated, float hierarchyGain);

    // Prepare native target velocities from desired body origin/rotation,
    // body-local COM and current world COM/rotation. Caller resolves the
    // effective motion limits (including alternate keyframed motion).
    // Quaternion normalization uses portable reciprocal square root rather
    // than a CPU-specific approximate RSQRT instruction.
    RagdollNativeVelocities ragdollNativeTargetVelocities(const osg::Vec3f& localCenterOfMass,
        const osg::Vec3f& currentCenterOfMass, const std::array<float, 4>& currentRotation,
        const RagdollNativeTargetPose& target, float inverseFrameSeconds,
        float maximumLinearVelocity, float maximumAngularVelocity);

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
