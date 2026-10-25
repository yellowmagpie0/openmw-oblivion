#ifndef OPENMW_COMPONENTS_ESM4_NATIVESEQUENCEDEACTIVATION_H
#define OPENMW_COMPONENTS_ESM4_NATIVESEQUENCEDEACTIVATION_H

#include "nativeclampsequence.hpp"

namespace ESM4
{
    // Original6C9CEF offset correction in the observed PC24 frame context.
    // The caller owns the NI state guard. Other timing fields are retained.
    MeleeSequenceTiming deactivateNativeSequenceTiming(const MeleeSequenceTiming& state, float frequency);

    // Static validation, including cached activation and cold retirement masks.
    // Missing clock context skips only the inactive retirement deadline check.
    void validateNativeClampSequenceState(const NativeClampSequenceState& state,
        std::optional<float> transitionFrame = std::nullopt, std::optional<float> clock = std::nullopt);

    // Retire an active/easing resident; the caller applies logical-slot reset,
    // retains a captured pose during positive retirement, and clears it on NI0.
    // This operation anchors no clock and publishes no live sequence.
    NativeClampSequenceState retireNativeClampSequence(const NativeClampSequenceState& state,
        float frequency, float duration, std::optional<float> transitionFrame = std::nullopt);
}

#endif
