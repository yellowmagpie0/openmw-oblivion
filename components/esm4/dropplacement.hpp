#ifndef OPENMW_COMPONENTS_ESM4_DROPPLACEMENT_H
#define OPENMW_COMPONENTS_ESM4_DROPPLACEMENT_H

#include <components/esm/position.hpp>
#include <array>
#include <optional>

namespace Nif
{
    struct NiTransform;
}

namespace ESM4
{
    // Fresh dropped-reference scale from the source item's ExtraScale. Absence
    // selects the constructor's 1, independently of the actor/attachment scale.
    // Original SetScale formats two decimal places, parses binary32, then
    // clamps to [0.01, 10]. Nonfinite extras reject. No caller state changes.
    float droppedReferenceScale(std::optional<float> sourceExtraScale);

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

    // Live physics scene -> stored reference pose (4E2160 / 711300).
    // This uses a different Euler order from fresh carried-node drop arguments.
    // Inputs are the nine original NiMatrix3 elements and world translation;
    // separate reference/model scale is excluded. A pole still supplies valid
    // angles even when mUniqueRotation is false. Nonfinite inputs reject.
    DroppedItemPlacement physicalDroppedItemPlacement(
        const std::array<float, 9>& rotation, const std::array<float, 3>& translation);
}

#endif
