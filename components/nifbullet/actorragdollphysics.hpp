#ifndef OPENMW_COMPONENTS_NIFBULLET_ACTORRAGDOLLPHYSICS_HPP
#define OPENMW_COMPONENTS_NIFBULLET_ACTORRAGDOLLPHYSICS_HPP

#include "actorragdoll.hpp"

#include <memory>
#include <span>

#include <LinearMath/btTransform.h>

class btDynamicsWorld;

namespace NifBullet
{
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
            float lengthScale, std::span<const btTransform> bodyPoses, int collisionGroup, int collisionMask);
        ~ActorRagdollPhysics();
        ActorRagdollPhysics(const ActorRagdollPhysics&) = delete;
        ActorRagdollPhysics& operator=(const ActorRagdollPhysics&) = delete;

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
