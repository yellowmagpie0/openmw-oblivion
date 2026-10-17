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
        auto renderer = animation.prepareNativePhysicalPose(
            scaled, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        const auto poses = NifBullet::ragdollBodyWorldPoses(scaled, renderer->initialBones());
        auto bodies = physics.prepareActorRagdoll(actor, scaled, NifBullet::RagdollNativeLengthScale,
            poses, collisionGroup, collisionMask, internalFilter);
        // Staging may allocate or reject geometry. Neither renderer callbacks
        // nor capsule ownership has changed at that point. Physics publication
        // invokes no renderer callbacks; the validated renderer commit below
        // only detaches the captured chains and moves already-owned storage.
        if (!renderer->isValid() || !physics.validatePreparedActorRagdoll(*bodies)
            || !physics.commitActorRagdoll(*bodies))
            throw std::invalid_argument("native physical actor changed before admission");
        if (!animation.commitPhysicalPose(*renderer))
        {
            physics.removeActorRagdoll(actor);
            throw std::logic_error("renderer changed during native physics publication");
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
