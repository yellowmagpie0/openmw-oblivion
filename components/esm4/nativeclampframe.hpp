#ifndef OPENMW_COMPONENTS_ESM4_NATIVECLAMPFRAME_H
#define OPENMW_COMPONENTS_ESM4_NATIVECLAMPFRAME_H

#include "nativeclampsequence.hpp"

namespace ESM4
{
    // Resident portion of original476D10:49F4A0 queries timing at the old
    // actor clock before key dispatch and the manager update at the new clock.
    // NI5 skips that query. Detached retirement tails use the manager/key
    // kernel directly. The caller advances the shared actor clock once and
    // owns request cleanup, slot publication, events and scene transforms.
    NativeClampSequenceState advanceNativeResidentClampSequence(const NativeClampSequenceState& state,
        float previousAnimationClock, float animationClock, float frequency, float begin, float end,
        const std::array<float, 2>& keyTimes, float retirementDuration,
        bool freezeSlot = false, std::optional<float> transitionFrame = std::nullopt);
}

#endif
