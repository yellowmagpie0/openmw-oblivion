#ifndef OPENMW_COMPONENTS_NIFBULLET_RAGDOLLBONEPOSE_HPP
#define OPENMW_COMPONENTS_NIFBULLET_RAGDOLLBONEPOSE_HPP

#include <components/nif/nativetransform.hpp>

#include <cstdint>

namespace NifBullet
{
    // Original NiTransform parent/local composition with separate scale
    // and binary32 stores at the native matrix/vector/position boundaries.
    Nif::NiTransform composeRagdollBonePose(const Nif::NiTransform& parent, const Nif::NiTransform& local,
        Nif::NativeTransformPrecision precision = Nif::NativeTransformPrecision::Double);

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
        const Nif::NiTransform* parentWorld, std::uint16_t collisionFlags, bool forceWorldUpdate,
        Nif::NativeTransformPrecision precision = Nif::NativeTransformPrecision::Double);

    // One settled node traversal: compose the current local with its parent's
    // cached world before testing collision bit0. Inactive nodes do not read
    // desiredWorld. Unlike registered roots, unselected children keep their
    // settled flags (including bit8) without enabling reference publication.
    // Callers dispatch controllers first and own any reference callbacks.
    // Default arithmetic matches observed live007F single precision; the older
    // composition/writeback APIs retain their explicit double-precision default.
    RagdollBonePoseWriteback ragdollBoneFrame(const Nif::NiTransform& previousLocal,
        const Nif::NiTransform& desiredWorld, const Nif::NiTransform* parentWorld,
        std::uint16_t settledCollisionFlags,
        Nif::NativeTransformPrecision precision = Nif::NativeTransformPrecision::Single);

    // One settled registered-root traversal. Registration enables local and
    // reference publication, but never enables an authored inactive bit0.
    // Inactive roots compose their existing local without reading desiredWorld.
    RagdollBonePoseWriteback ragdollReferenceRootFrame(const Nif::NiTransform& previousLocal,
        const Nif::NiTransform& desiredWorld, std::uint16_t authoredCollisionFlags);
}

#endif
