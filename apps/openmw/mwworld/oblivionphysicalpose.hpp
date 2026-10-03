#ifndef OPENMW_MWWORLD_OBLIVIONPHYSICALPOSE_HPP
#define OPENMW_MWWORLD_OBLIVIONPHYSICALPOSE_HPP

namespace MWPhysics { class PhysicsSystem; }
namespace MWRender { class Animation; }
namespace Nif { struct NiTransform; }
namespace NifBullet
{
    struct ActorRagdollDefinition;
    struct RagdollInternalCollisionFilter;
}

namespace MWWorld
{
    class Ptr;
    // Borrowed renderer/physics lifetimes. The caller supplies resolved native
    // uniform placement and collision policy; this does not select reactions,
    // infer NPC morphology, or install generated controllers.
    void beginNativeActorPhysicalPose(MWPhysics::PhysicsSystem& physics, MWRender::Animation& animation,
        const Ptr& actor, const NifBullet::ActorRagdollDefinition& authored,
        const Nif::NiTransform& placement, int collisionGroup, int collisionMask,
        const NifBullet::RagdollInternalCollisionFilter* internalFilter);
    void endNativeActorPhysicalPose(MWPhysics::PhysicsSystem& physics, MWRender::Animation& animation,
        const Ptr& actor);
}

#endif
