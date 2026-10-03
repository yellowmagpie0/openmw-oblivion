#ifndef OPENMW_MWPHYSICS_OBLIVIONRAGDOLL_HPP
#define OPENMW_MWPHYSICS_OBLIVIONRAGDOLL_HPP

#include <components/esm4/runtimestate.hpp>
#include <components/nifbullet/actorragdollphysics.hpp>

#include <span>
#include <string_view>

namespace MWPhysics
{
    // Save boundary for the physical projection. World lengths are already
    // converted by the physics owner; this adapter does not convert units again.
    ESM4::RuntimeActorRagdoll captureNativeActorRagdoll(const ESM::FormKey& base,
        std::string_view model, const NifBullet::ActorRagdollDefinition& definition,
        std::span<const NifBullet::RagdollBodyState> bodies);
    std::vector<NifBullet::RagdollBodyState> restoreNativeActorRagdoll(
        const ESM4::RuntimeActorRagdoll& snapshot, const ESM::FormKey& base,
        std::string_view model, const NifBullet::ActorRagdollDefinition& definition);
}

#endif
