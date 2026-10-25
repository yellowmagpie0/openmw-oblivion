#include "nativesequencedeactivation.hpp"

#include <cmath>
#include <stdexcept>

namespace ESM4
{
    void validateNativeClampSequenceState(const NativeClampSequenceState& state,
        std::optional<float> transitionFrame, std::optional<float> clock)
    {
        const auto finite = [](float value) {
            if (!std::isfinite(value)) throw std::invalid_argument("nonfinite native clamp sequence state");
        };
        const auto& timing = state.mTiming;
        for (float value : {timing.mEaseEnd, timing.mWeightedTime, timing.mOutputTime}) finite(value);
        for (auto value : {timing.mOffset, timing.mEaseStart, timing.mLastInput, transitionFrame, clock})
            if (value) finite(*value);
        const bool pendingHistory = !state.mRetirement
            || (!state.mInactive && !state.mRetirement->mStartClock);
        const bool freshCapture = transitionFrame && timing.mEasing && timing.mOffset
            && !timing.mEaseStart && !timing.mLastInput;
        const bool pendingCache = pendingHistory && (timing.mEasing
            ? !timing.mEaseStart && (!transitionFrame || timing.mOffset) : !timing.mOffset);
        // State1 aliases easeStart to the previous output, which can still be
        // the constructor's sentinel after the first manager update.
        const bool firstOutput = !state.mRetirement && !timing.mEasing && timing.mOffset
            && !timing.mEaseStart && timing.mLastInput;
        const bool unanchoredRetirement = state.mRetirement && !state.mInactive
            && !state.mRetirement->mStartClock && !timing.mEasing && !timing.mEaseStart;
        const bool expiredUnupdated = state.mInactive && state.mRetirement && state.mRetirement->mStartClock
            && timing.mOffset && timing.mEaseStart && !timing.mLastInput;
        const bool cancelledPose = state.mInactive && !state.mRetirement && !timing.mEasing
            && timing.mOffset && !timing.mEaseStart;
        if (!freshCapture && !pendingCache && !firstOutput && !unanchoredRetirement && !expiredUnupdated
            && !cancelledPose && (timing.mOffset.has_value() != timing.mEaseStart.has_value()
                || timing.mOffset.has_value() != timing.mLastInput.has_value()))
            throw std::invalid_argument("partial native clamp sequence timing");
        if (timing.mEasing && timing.mEaseEnd <= timing.mEaseStart.value_or(0.f))
            throw std::invalid_argument("invalid native clamp activation interval");
        if ((transitionFrame && !timing.mEasing && !state.mRetirement)
            || (state.mInactive && (timing.mEasing || transitionFrame)))
            throw std::invalid_argument("invalid native clamp pose or inactive state");
        if (state.mRetirement)
        {
            const auto& span = *state.mRetirement;
            finite(span.mDuration);
            if (span.mDuration <= 0 || timing.mEasing)
                throw std::invalid_argument("invalid native clamp sequence retirement");
            if (span.mStartClock)
            {
                finite(*span.mStartClock);
                const float stop = static_cast<float>(double(*span.mStartClock) + span.mDuration);
                finite(stop);
                if (!timing.mOffset || (state.mInactive && clock && *clock < stop))
                    throw std::invalid_argument("invalid native clamp sequence retirement clock");
            }
            else if (state.mInactive)
                throw std::invalid_argument("unanchored inactive native clamp sequence");
        }
    }

    NativeClampSequenceState retireNativeClampSequence(const NativeClampSequenceState& state,
        float frequency, float duration, std::optional<float> transitionFrame)
    {
        if (!std::isfinite(duration)) throw std::invalid_argument("nonfinite native clamp retirement duration");
        validateNativeClampSequenceState(state, transitionFrame);
        if (!std::isfinite(frequency) || frequency <= 0)
            throw std::invalid_argument("invalid native clamp retirement frequency");
        if (state.mInactive) return state;
        if (state.mRetirement) throw std::invalid_argument("native clamp retirement requires a resident sequence");
        auto next = state;
        next.mTiming.mEasing = false;
        // Positive6C9CCD retains offset. Only immediate6C9CEF and eventual
        // state3 expiry perform the history/frequency correction.
        if (duration > 0)
        {
            next.mRetirement = MeleeSequenceRetirement{duration, {}};
            next.mTiming.mEaseStart.reset();
            next.mTiming.mEaseEnd = duration;
        }
        else
        {
            next.mTiming = deactivateNativeSequenceTiming(next.mTiming, frequency);
            next.mInactive = true;
        }
        return next;
    }

    NativeClampSequenceState advanceNativeClampSequence(const NativeClampSequenceState& state,
        float animationClock, float frequency, float begin, float end,
        const std::array<float, 2>& keyTimes, float retirementDuration,
        bool freezeSlot, std::optional<float> transitionFrame)
    {
        for (float value : {animationClock, frequency, begin, end, retirementDuration})
            if (!std::isfinite(value)) throw std::invalid_argument("nonfinite native clamp sequence input");
        if (frequency <= 0 || end < begin || !std::isfinite(end - begin))
            throw std::invalid_argument("invalid native clamp sequence bounds or frequency");
        (void)advanceReactionSlot(state.mStarted, 0, 0, keyTimes);
        validateNativeClampSequenceState(state, transitionFrame, animationClock);
        auto next = state;
        if (next.mInactive) return next;
        if (!next.mRetirement && !freezeSlot && !state.mTiming.mEasing && state.mTiming.mOffset)
        {
            const auto progress = advanceReactionSlot(state.mStarted, *state.mTiming.mOffset,
                animationClock, keyTimes);
            next.mStarted = progress.mStarted;
            if (progress.mRetire)
                next = retireNativeClampSequence(next, frequency, retirementDuration, transitionFrame);
        }
        if (next.mInactive) return next;
        if (next.mRetirement)
        {
            auto& span = *next.mRetirement;
            // Original6CA950 initializes offset before checking state3 expiry.
            if (!next.mTiming.mOffset) next.mTiming.mOffset = initialMeleeSequenceOffset(animationClock);
            if (!span.mStartClock) span.mStartClock = animationClock;
            const float stop = static_cast<float>(double(*span.mStartClock) + span.mDuration);
            if (!std::isfinite(stop)) throw std::invalid_argument("overflowing native clamp retirement clock");
            next.mTiming.mEaseStart = span.mStartClock;
            next.mTiming.mEaseEnd = stop;
            if (animationClock >= stop)
            {
                next.mTiming = deactivateNativeSequenceTiming(next.mTiming, frequency);
                next.mInactive = true;
                return next;
            }
            auto manager = next.mTiming;
            manager.mEasing = true;
            next.mTiming = updateMeleeSequenceTiming(manager, animationClock, frequency, begin, end, transitionFrame);
            next.mTiming.mEasing = false;
        }
        else
            next.mTiming = updateMeleeSequenceTiming(next.mTiming, animationClock, frequency, begin, end,
                transitionFrame);
        return next;
    }
}
