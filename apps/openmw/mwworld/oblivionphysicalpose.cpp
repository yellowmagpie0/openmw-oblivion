#include "oblivionphysicalpose.hpp"

#include "../mwphysics/physicssystem.hpp"
#include "../mwrender/animation.hpp"

#include <components/nifbullet/actorragdollphysics.hpp>

#include <stdexcept>

namespace MWWorld
{
    void beginNativeActorPhysicalPose(MWPhysics::PhysicsSystem& physics, MWRender::Animation& animation,
        const Ptr& actor, const NifBullet::ActorRagdollDefinition& authored,
        const Nif::NiTransform& placement, std::span<const std::uint32_t> resolvedPackedFilters,
        const ESM4::PhysicalBlendGainTable& resolvedGains, int collisionGroup, int collisionMask,
        const NifBullet::RagdollInternalCollisionFilter* internalFilter)
    {
        if (actor.isEmpty() || !physics.getActor(actor)
            || animation.hasPhysicalPose() || physics.hasActorRagdoll(actor))
            throw std::invalid_argument("invalid or already bound native physical actor");
        // Scale an owned definition before detaching renderer callbacks. The
        // caller resolves uniform placement/morphology; authored data stays
        // immutable and world/native lengths are converted only once.
        const auto linked = NifBullet::ragdollDefinitionWithNativeLinkedBlendState(
            authored, resolvedPackedFilters, resolvedGains);
        const auto scaled = NifBullet::ragdollDefinitionWithNativeScaledProperties(linked, placement.mScale);
        const auto bones = animation.beginPhysicalPose(
            scaled, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        try
        {
            const auto poses = NifBullet::ragdollBodyWorldPoses(scaled, bones);
            physics.addActorRagdoll(actor, scaled, NifBullet::RagdollNativeLengthScale,
                poses, collisionGroup, collisionMask, internalFilter);
        }
        catch (...)
        {
            // Physics admission publishes capsule suspension only on success.
            // Restore animation ownership if conversion/body admission rejects.
            animation.endPhysicalPose();
            throw;
        }
    }

    void endNativeActorPhysicalPose(MWPhysics::PhysicsSystem& physics, MWRender::Animation& animation,
        const Ptr& actor)
    {
        try
        {
            animation.endPhysicalPose();
        }
        catch (...)
        {
            // Release bodies and restore capsule collision even if external
            // renderer controllers fail to rebuild during teardown. Report
            // the renderer failure; this is cleanup, not successful recovery.
            physics.removeActorRagdoll(actor);
            throw;
        }
        physics.removeActorRagdoll(actor);
    }
}
