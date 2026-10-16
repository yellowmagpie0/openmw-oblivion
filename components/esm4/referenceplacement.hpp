#ifndef OPENMW_COMPONENTS_ESM4_REFERENCEPLACEMENT_H
#define OPENMW_COMPONENTS_ESM4_REFERENCEPLACEMENT_H

#include <components/esm/position.hpp>
#include <components/nif/nativetransform.hpp>

namespace ESM4
{
    // Ordinary nonactor REFR scene-root initialization: original4D7AF0 X/Y/Z
    // axis matrices and successive products, then absolute reference scale.
    // Translation/scale stay separate from rotation. Actor yaw/morphology has
    // its own npcPlacementRoot path. No scene, collision or REFR mutation.
    // Reject nonfinite consumed fields, unknown precision and |angle| >= 2^63.
    std::optional<Nif::NiTransform> referencePlacementRoot(const ESM::Position& position,
        float referenceScale, Nif::NativeTransformPrecision precision = Nif::NativeTransformPrecision::Single) noexcept;
}

#endif
