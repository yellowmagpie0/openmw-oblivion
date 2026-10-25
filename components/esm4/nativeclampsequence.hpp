#ifndef OPENMW_COMPONENTS_ESM4_NATIVECLAMPSEQUENCE_H
#define OPENMW_COMPONENTS_ESM4_NATIVECLAMPSEQUENCE_H

#include "physicalcombat.hpp"

namespace ESM4
{
    // Unsynchronized clamp-cycle subtype0/1 animation, including GetUp's
    // SpecialIdle in logical slot5. Request ownership and process action are
    // separate: an End key detaches the slot before the sequence is inactive.
    struct NativeClampSequenceState
    {
        bool mStarted = false;
        MeleeSequenceTiming mTiming;
        std::optional<MeleeSequenceRetirement> mRetirement;
        bool mInactive = false;
        friend bool operator==(const NativeClampSequenceState&, const NativeClampSequenceState&) = default;
    };

    // Original477414/4733A0/6CA950. The caller advances the shared animation
    // clock once, resolves activation/pose capture, and supplies Start/End and
    // the signed blend-byte retirement duration. freezeSlot skips key dispatch;
    // it does not stop manager updates or replace the caller's clock policy.
    NativeClampSequenceState advanceNativeClampSequence(const NativeClampSequenceState& state,
        float animationClock, float frequency, float begin, float end,
        const std::array<float, 2>& keyTimes, float retirementDuration,
        bool freezeSlot = false, std::optional<float> transitionFrame = std::nullopt);
}

#endif
