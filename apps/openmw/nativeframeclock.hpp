#ifndef OPENMW_NATIVEFRAMECLOCK_HPP
#define OPENMW_NATIVEFRAMECLOCK_HPP

#include <components/esm4/physicalframe.hpp>
#include <components/nif/nativetransformmath.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace OMW
{
    // One process clock, independent of saves, actors and renderer frame IDs.
    // Host ticks are unsigned milliseconds, with the original wrapping rules.
    class NativeFrameClock
    {
        ESM4::PhysicalTimerState mTimer;
        ESM4::PhysicalTimerCounters mCounters;
        ESM4::PhysicalFrameClock mFrame;
        ESM4::PhysicalFrameSettings mSettings;
        std::uint32_t mUpdateType;
        bool mSuspended = false;

    public:
        NativeFrameClock(std::uint32_t tick, float fixedSubstep, float smoothing, std::uint32_t updateType)
            : mTimer(tick), mUpdateType(updateType)
        {
            if (!std::isfinite(fixedSubstep) || fixedSubstep <= 0.f || !std::isfinite(smoothing))
                throw std::invalid_argument("invalid native process clock settings");
            // The original initialized fixed-step setter caps fMaxTime at1.
            mSettings.mFixedSubstep = std::min(fixedSubstep, 1.f);
            mSettings.mSmoothing = smoothing;
            auto frame = mFrame;
            ESM4::preparePhysicalFrame(frame, mSettings, 0.f, mUpdateType, false,
                Nif::NativeTransformPrecision::Single);
        }

        ESM4::PhysicalFrameResult advance(std::uint32_t tick, float scale, bool suspended,
            bool limitSubsteps = false)
        {
            // Stage the complete producer: a rejected frame must not consume
            // host elapsed time, a pause transition or the signed carry.
            auto timer = mTimer;
            auto counters = mCounters;
            auto frame = mFrame;
            if (suspended != mSuspended)
            {
                if (suspended) ESM4::pausePhysicalTimer(timer, tick);
                else ESM4::resumePhysicalTimer(timer, tick);
            }
            const float delta = ESM4::advancePhysicalTimer(timer, counters, tick, scale,
                Nif::NativeTransformPrecision::Single);
            // A hidden window does not run a physical frame. Keep any carry
            // until an actual frame is admitted again.
            const auto result = suspended ? ESM4::PhysicalFrameResult{0.f, 0.f, 0.f, 0}
                : ESM4::preparePhysicalFrame(frame, mSettings, delta, mUpdateType,
                    limitSubsteps, Nif::NativeTransformPrecision::Single);
            mTimer = timer;
            mCounters = counters;
            mFrame = frame;
            mSuspended = suspended;
            return result;
        }

        float remaining() const { return mFrame.mRemaining; }
    };
}

#endif
