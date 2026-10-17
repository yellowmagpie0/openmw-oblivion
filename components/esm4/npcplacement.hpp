#ifndef OPENMW_COMPONENTS_ESM4_NPCPLACEMENT_H
#define OPENMW_COMPONENTS_ESM4_NPCPLACEMENT_H

#include <components/esm/position.hpp>
#include <components/nif/nativetransform.hpp>
#include <optional>

namespace ESM4
{
    struct NpcPlacementMorphology
    {
        float mWeight;
        float mHeight;
    };

    // Native NPC scene-root placement: yaw and race morphology belong to the
    // rotationScale matrix; absolute reference scale stays separate. Pitch/roll do not
    // participate in the original actor root matrix. Missing race is distinct
    // from an authored zero/negative morphology. Nonfinite consumed fields,
    // results and the original x87 trig range boundary |yaw| >= 2^63 reject.
    // No scene or actor mutation.
    std::optional<Nif::NiTransform> npcPlacementRoot(const ESM::Position& position,
        float referenceScale, std::optional<NpcPlacementMorphology> morphology) noexcept;

    // Creature4D7260 multiplies reference scale by TESCreature BNAM, stores
    // binary32, then GenerateNiNode takes its absolute value. Keep scale out
    // of the actor's yaw-only rotation matrix; no NPC morphology applies.
    std::optional<float> creaturePlacementScale(float referenceScale, float baseScale,
        Nif::NativeTransformPrecision precision = Nif::NativeTransformPrecision::Single) noexcept;
    std::optional<Nif::NiTransform> creaturePlacementRoot(const ESM::Position& position,
        float referenceScale, float baseScale,
        Nif::NativeTransformPrecision precision = Nif::NativeTransformPrecision::Single) noexcept;
}

#endif
