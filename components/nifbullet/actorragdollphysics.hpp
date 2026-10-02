#ifndef OPENMW_COMPONENTS_NIFBULLET_ACTORRAGDOLLPHYSICS_HPP
#define OPENMW_COMPONENTS_NIFBULLET_ACTORRAGDOLLPHYSICS_HPP

#include "actorragdoll.hpp"

#include <memory>
#include <span>

#include <LinearMath/btTransform.h>

class btDynamicsWorld;
class btCollisionObject;

namespace NifBullet
{
    // Original bhk position adapters store binary32 after multiplying by
    // these separately stored constants. The reverse is not computed as 1/k.
    inline constexpr float RagdollNativeLengthScale = 6.999040126800537f;
    osg::Vec3f ragdollWorldToNativePosition(const osg::Vec3f& position);
    osg::Vec3f ragdollNativeToWorldPosition(const osg::Vec3f& position);

    struct RagdollBoneWorldPose
    {
        std::uint32_t mNodeRecord;
        osg::Matrixf mPose;
    };

    // Current bone world pose at the ordinary bhkRigidBody sync boundary.
    // Input already includes actor/world placement and must be rigid.
    // The single pose uses native lengths; the graph adapter returns world
    // lengths for ActorRagdollPhysics with RagdollNativeLengthScale.
    btTransform ragdollNativePoseFromBoneWorld(const osg::Matrixf& worldPose);
    std::vector<btTransform> ragdollBodyWorldPoses(const ActorRagdollDefinition& definition,
        std::span<const RagdollBoneWorldPose> bones);

    // Original torque*dt binary32 store, then torque-unit conversion to the
    // caller's world units (mass is unchanged, lengths use lengthScale).
    btScalar ragdollFrictionImpulse(float torque, float frameSeconds, float lengthScale);

    struct RagdollBodyState
    {
        std::uint32_t mRecord;
        btTransform mPose;
        btVector3 mLinearVelocity;
        btVector3 mAngularVelocity;
    };

    // Owns collision shapes, rigid bodies and constraints. The borrowed world
    // must outlive the instance; destruction removes every registered object.
    // Poses and velocities use the caller's world units. Native shape/inertia
    // lengths are converted once with the supplied positive length scale.
    class ActorRagdollPhysics
    {
    public:
        ActorRagdollPhysics(const ActorRagdollDefinition& definition, btDynamicsWorld& world,
            float lengthScale, std::span<const btTransform> bodyPoses, int collisionGroup, int collisionMask,
            void* userPointer = nullptr);
        ~ActorRagdollPhysics();
        ActorRagdollPhysics(const ActorRagdollPhysics&) = delete;
        ActorRagdollPhysics& operator=(const ActorRagdollPhysics&) = delete;

        // Borrowed identities for engine collision routing; ownership stays here.
        std::span<btCollisionObject* const> collisionObjects() const;
        std::vector<RagdollBodyState> capture() const;
        void restore(std::span<const RagdollBodyState> states);
        void applyImpulse(std::size_t body, const btVector3& impulse, const btVector3& worldPoint);
        // Original sphere-motion velocity damping; caller owns gravity/force
        // composition and the serial step boundary. Bullet damping stays zero.
        void applyNativeDamping(float frameSeconds);

    private:
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}

#endif
