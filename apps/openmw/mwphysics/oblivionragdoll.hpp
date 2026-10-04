#ifndef OPENMW_MWPHYSICS_OBLIVIONRAGDOLL_HPP
#define OPENMW_MWPHYSICS_OBLIVIONRAGDOLL_HPP

#include <components/esm4/runtimestate.hpp>
#include <components/nifbullet/actorragdollphysics.hpp>

#include <optional>
#include <span>
#include <string_view>

namespace MWPhysics
{
    // Save boundary for the physical projection. World lengths are already
    // converted by the physics owner; this adapter does not convert units again.
    ESM4::RuntimeActorRagdoll captureNativeActorRagdoll(const ESM::FormKey& base,
        std::string_view model, const NifBullet::ActorRagdollDefinition& definition,
        std::span<const NifBullet::RagdollBodyState> bodies,
        std::span<const NifBullet::RagdollNativePackedVelocityState> packedVelocities = {},
        std::span<const NifBullet::RagdollNativeMotionRequest> motions = {},
        std::optional<std::span<const NifBullet::RagdollNativeBlendState>> blends = std::nullopt);
    std::vector<NifBullet::RagdollBodyState> restoreNativeActorRagdoll(
        const ESM4::RuntimeActorRagdoll& snapshot, const ESM::FormKey& base,
        std::string_view model, const NifBullet::ActorRagdollDefinition& definition);
    // Absent for legacy snapshots; complete in current asset body order when
    // present. Resolve both projections before calling the combined owner restore.
    std::optional<std::vector<NifBullet::RagdollNativePackedVelocityState>> restoreNativeActorRagdollPackedVelocities(
        const ESM4::RuntimeActorRagdoll& snapshot, const ESM::FormKey& base,
        std::string_view model, const NifBullet::ActorRagdollDefinition& definition);
    std::optional<std::vector<NifBullet::RagdollNativeMotionRequest>> restoreNativeActorRagdollMotionModes(
        const ESM4::RuntimeActorRagdoll& snapshot, const ESM::FormKey& base,
        std::string_view model, const NifBullet::ActorRagdollDefinition& definition);
    // Present entries must match every blend-bearing body in the winning asset.
    std::optional<std::vector<NifBullet::RagdollNativeBlendState>> restoreNativeActorRagdollBlendStates(
        const ESM4::RuntimeActorRagdoll& snapshot, const ESM::FormKey& base,
        std::string_view model, const NifBullet::ActorRagdollDefinition& definition);
}

#endif
