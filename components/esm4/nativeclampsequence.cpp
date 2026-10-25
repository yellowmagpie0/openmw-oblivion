#include "nativeclampsequence.hpp"

#include <cmath>
#include <stdexcept>

namespace ESM4
{
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
        // Validate stored values without advancing time: an inactive sequence
        // or an immediate End can return before any manager arithmetic.
        for (float value : {state.mTiming.mEaseEnd, state.mTiming.mWeightedTime, state.mTiming.mOutputTime})
            if (!std::isfinite(value)) throw std::invalid_argument("nonfinite native clamp sequence timing");
        for (const auto value : {state.mTiming.mOffset, state.mTiming.mEaseStart, state.mTiming.mLastInput})
            if (value && !std::isfinite(*value))
                throw std::invalid_argument("nonfinite native clamp sequence timing");
        if (transitionFrame && (!std::isfinite(*transitionFrame) || !state.mTiming.mEasing))
            throw std::invalid_argument("invalid native clamp transition frame");
        const bool capture = transitionFrame && state.mTiming.mEasing && state.mTiming.mOffset
            && !state.mTiming.mEaseStart && !state.mTiming.mLastInput;
        if (!capture && (state.mTiming.mOffset.has_value() != state.mTiming.mEaseStart.has_value()
                || state.mTiming.mOffset.has_value() != state.mTiming.mLastInput.has_value()))
            throw std::invalid_argument("partial native clamp sequence timing");
        if (state.mRetirement)
        {
            const auto& span = *state.mRetirement;
            if (!std::isfinite(span.mDuration) || span.mDuration <= 0 || !state.mTiming.mOffset
                || state.mTiming.mEasing || transitionFrame)
                throw std::invalid_argument("invalid native clamp sequence retirement");
            if (span.mStartClock)
            {
                const float stop = static_cast<float>(double(*span.mStartClock) + span.mDuration);
                if (!std::isfinite(*span.mStartClock) || !std::isfinite(stop)
                    || (state.mInactive && animationClock < stop))
                    throw std::invalid_argument("invalid native clamp sequence retirement clock");
            }
            else if (state.mInactive)
                throw std::invalid_argument("unanchored inactive native clamp sequence");
        }
        if (state.mInactive && (!state.mTiming.mOffset || state.mTiming.mEasing || transitionFrame))
            throw std::invalid_argument("invalid inactive native clamp sequence");

        auto next = state;
        if (next.mInactive) return next;
        if (!next.mRetirement && !freezeSlot && !state.mTiming.mEasing && state.mTiming.mOffset)
        {
            const auto progress = advanceReactionSlot(state.mStarted, *state.mTiming.mOffset,
                animationClock, keyTimes);
            next.mStarted = progress.mStarted;
            if (progress.mRetire)
            {
                if (retirementDuration > 0)
                    next.mRetirement = MeleeSequenceRetirement{retirementDuration, {}};
                else next.mInactive = true;
            }
        }
        if (next.mInactive) return next;
        if (next.mRetirement)
        {
            auto& span = *next.mRetirement;
            if (!span.mStartClock) span.mStartClock = animationClock;
            const float stop = static_cast<float>(double(*span.mStartClock) + span.mDuration);
            if (!std::isfinite(stop)) throw std::invalid_argument("overflowing native clamp retirement clock");
            if (animationClock >= stop) { next.mInactive = true; return next; }
            auto manager = next.mTiming;
            manager.mEasing = true;
            manager.mEaseStart = span.mStartClock;
            manager.mEaseEnd = stop;
            next.mTiming = updateMeleeSequenceTiming(manager, animationClock, frequency, begin, end);
            next.mTiming.mEasing = false;
        }
        else
            next.mTiming = updateMeleeSequenceTiming(next.mTiming, animationClock, frequency, begin, end,
                transitionFrame);
        return next;
    }
}
