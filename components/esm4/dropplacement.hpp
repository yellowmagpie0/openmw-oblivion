#ifndef OPENMW_COMPONENTS_ESM4_DROPPLACEMENT_H
#define OPENMW_COMPONENTS_ESM4_DROPPLACEMENT_H

#include <components/esm/position.hpp>
#include <array>

namespace Nif
{
    struct NiTransform;
}

namespace ESM4
{
    struct DroppedItemPlacement
    {
        ESM::Position mPosition;
        // False at a pole: the selected angles are still valid drop arguments.
        bool mUniqueRotation = false;
    };

    // The nine consecutive original NiMatrix3 elements, with separate world
    // translation. Preserve embedded rotationScale; exclude only the separate
    // NiTransform scale. The native equipped-node caller does not forward its
    // separate scale to the drop. Nonfinite matrix/position inputs reject.
    // This computes arguments only; it does not select created-reference scale.
    DroppedItemPlacement droppedItemPlacement(
        const std::array<float, 9>& rotation, const std::array<float, 3>& translation);
    DroppedItemPlacement droppedItemPlacement(const Nif::NiTransform& worldTransform);
}

#endif
