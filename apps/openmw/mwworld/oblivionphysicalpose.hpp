#ifndef OPENMW_MWWORLD_OBLIVIONPHYSICALPOSE_HPP
#define OPENMW_MWWORLD_OBLIVIONPHYSICALPOSE_HPP

#include <components/esm4/physicalblendsettings.hpp>
#include <span>

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
    // uniform placement, resolved packed body filters/gain table and collision policy; this does not select reactions,
    // infer NPC morphology, or install generated controllers.
    void beginNativeActorPhysicalPose(MWPhysics::PhysicsSystem& physics, MWRender::Animation& animation,
        const Ptr& actor, const NifBullet::ActorRagdollDefinition& authored,
        const Nif::NiTransform& placement, std::span<const std::uint32_t> resolvedPackedFilters,
        const ESM4::PhysicalBlendGainTable& resolvedGains, int collisionGroup, int collisionMask,
        const NifBullet::RagdollInternalCollisionFilter* internalFilter);
    void endNativeActorPhysicalPose(MWPhysics::PhysicsSystem& physics, MWRender::Animation& animation,
        const Ptr& actor);
}

#endif
