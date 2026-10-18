#include "oblivionphysicalpose.hpp"

#include "../mwphysics/physicssystem.hpp"
#include "../mwphysics/oblivionragdoll.hpp"
#include "../mwrender/animation.hpp"

#include <components/nifbullet/actorragdollphysics.hpp>

#include <stdexcept>

namespace MWWorld
{
    struct PreparedNativeActorPhysicalPose::Data
    {
        MWPhysics::PhysicsSystem* mPhysics;
        MWRender::Animation* mAnimation;
        Ptr mActor;
        std::weak_ptr<const char> mActorIdentity;
        ESM::Position mPosition;
        NifBullet::ActorRagdollDefinition mDefinition;
        std::unique_ptr<MWRender::Animation::PreparedPhysicalPose> mRenderer;
        std::unique_ptr<MWPhysics::PreparedActorRagdoll> mBodies;
    };

    PreparedNativeActorPhysicalPose::PreparedNativeActorPhysicalPose(std::unique_ptr<Data> data)
        : mData(std::move(data)) {}
    PreparedNativeActorPhysicalPose::~PreparedNativeActorPhysicalPose() = default;

    std::unique_ptr<PreparedNativeActorPhysicalPose> PreparedNativeActorPhysicalPose::prepare(
        MWPhysics::PhysicsSystem& physics, MWRender::Animation& animation,
        const Ptr& actor, const NifBullet::ActorRagdollDefinition& authored,
        const Nif::NiTransform& placement, std::span<const std::uint32_t> resolvedPackedFilters,
        const ESM4::PhysicalBlendGainTable& resolvedGains, int collisionGroup, int collisionMask,
        const NifBullet::RagdollInternalCollisionFilter* internalFilter)
    {
        if (actor.isEmpty() || !physics.getActor(actor)
            || animation.hasPhysicalPose() || physics.hasActorRagdoll(actor))
            throw std::invalid_argument("invalid or already bound native physical actor");
        const auto linked = NifBullet::ragdollDefinitionWithNativeLinkedBlendState(
            authored, resolvedPackedFilters, resolvedGains);
        auto scaled = NifBullet::ragdollDefinitionWithNativeScaledProperties(linked, placement.mScale);
        auto renderer = animation.prepareNativePhysicalPose(
            scaled, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        const auto poses = NifBullet::ragdollBodyWorldPoses(scaled, renderer->initialBones());
        auto bodies = physics.prepareActorRagdoll(actor, scaled, NifBullet::RagdollNativeLengthScale,
            poses, collisionGroup, collisionMask, internalFilter);
        auto data = std::make_unique<Data>(Data{&physics, &animation, actor,
            actor.getRefData().prepareRuntimeIdentity(), actor.getRefData().getPosition(), std::move(scaled), std::move(renderer), std::move(bodies)});
        return std::unique_ptr<PreparedNativeActorPhysicalPose>(new PreparedNativeActorPhysicalPose(std::move(data)));
    }

    bool PreparedNativeActorPhysicalPose::isValid() const
    {
        // Each subordinate handle checks its own weak owner identity before
        // dereferencing a borrowed owner, including after owner destruction.
        if (!mData || mData->mActorIdentity.expired()
            || !mData->mRenderer->isValid() || !mData->mBodies->isValid()) return false;
        const auto& position = mData->mActor.getRefData().getPosition();
        for (unsigned i = 0; i < 3; ++i)
            if (position.pos[i] != mData->mPosition.pos[i] || position.rot[i] != mData->mPosition.rot[i]) return false;
        return true;
    }

    NifBullet::ActorRagdollPhysics& PreparedNativeActorPhysicalPose::physics()
    {
        if (!isValid()) throw std::invalid_argument("consumed, stale or expired native physical preparation");
        return mData->mBodies->physics();
    }

    void PreparedNativeActorPhysicalPose::restoreSnapshot(const ESM4::RuntimeActorRagdoll& snapshot,
        const ESM::FormKey& base, std::string_view model)
    {
        MWPhysics::restoreNativeActorRagdollSnapshot(physics(), snapshot, base, model, mData->mDefinition);
    }

    ESM4::RuntimeActorRagdoll PreparedNativeActorPhysicalPose::captureSnapshot(
        const ESM::FormKey& base, std::string_view model)
    {
        return MWPhysics::captureNativeActorRagdollSnapshot(physics(), base, model, mData->mDefinition);
    }

    bool PreparedNativeActorPhysicalPose::commit()
    {
        if (!isValid() || !mData->mBodies->commit()) return false;
        // Physics commit invokes no renderer callback. Both guards were checked
        // before publication; renderer commit only moves prepared storage.
        if (!mData->mAnimation->commitPhysicalPose(*mData->mRenderer))
        {
            mData->mPhysics->removeActorRagdoll(mData->mActor);
            throw std::logic_error("renderer changed during native physics publication");
        }
        return true;
    }

    void beginNativeActorPhysicalPose(MWPhysics::PhysicsSystem& physics, MWRender::Animation& animation,
        const Ptr& actor, const NifBullet::ActorRagdollDefinition& authored,
        const Nif::NiTransform& placement, std::span<const std::uint32_t> resolvedPackedFilters,
        const ESM4::PhysicalBlendGainTable& resolvedGains, int collisionGroup, int collisionMask,
        const NifBullet::RagdollInternalCollisionFilter* internalFilter)
    {
        auto prepared = PreparedNativeActorPhysicalPose::prepare(physics, animation, actor, authored,
            placement, resolvedPackedFilters, resolvedGains, collisionGroup, collisionMask, internalFilter);
        if (!prepared->commit())
            throw std::invalid_argument("native physical actor changed before admission");
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
