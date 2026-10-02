#ifndef OPENMW_COMPONENTS_NIFBULLET_RAGDOLLBONEPOSE_HPP
#define OPENMW_COMPONENTS_NIFBULLET_RAGDOLLBONEPOSE_HPP

#include <components/nif/niftypes.hpp>

#include <cstdint>

namespace NifBullet
{
    struct RagdollBonePoseWriteback
    {
        Nif::NiTransform mLocal;
        Nif::NiTransform mWorld;
        // Original publication bits: position=1, rotation=2.
        std::uint8_t mWorldChangeMask;
    };

    // Original collision-object node writeback. Desired pose uses NetImmerse
    // world units, not Havok lengths. Flag 0x8 requests local writeback;
    // scale slots retain their previous values. The parent inverse follows
    // NiTransform's transpose/reciprocal-scale convention.
    // This does not select actor modes or dispatch renderer/script callbacks.
    RagdollBonePoseWriteback ragdollBonePoseWriteback(const Nif::NiTransform& previousLocal,
        const Nif::NiTransform& previousWorld, const Nif::NiTransform& desiredWorld,
        const Nif::NiTransform* parentWorld, std::uint16_t collisionFlags, bool forceWorldUpdate);
}

#endif
