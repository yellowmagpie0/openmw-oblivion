#include "physicalblendsettings.hpp"

#include <cmath>
#include <stdexcept>
#include <limits>

namespace ESM4
{
    namespace
    {
        void validate(float value)
        {
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native physical blend duration");
        }
    }

    PhysicalBlendDurationTables resolvePhysicalBlendDurationTables(
        const PhysicalBlendDurationTables& previous, const PhysicalBlendDurationSettings& settings)
    {
        validate(settings.mGetUpTime);
        validate(settings.mKnockdownTime);
        auto result = previous;
        for (std::size_t i = 0; i < result.mGetUp.size(); ++i)
        {
            validate(previous.mGetUp[i]);
            validate(previous.mKnockdown[i]);
            if (previous.mGetUp[i] >= 0.f)
                result.mGetUp[i] = settings.mGetUpTime;
            if (previous.mKnockdown[i] >= 0.f)
                result.mKnockdown[i] = settings.mKnockdownTime;
        }
        return result;
    }

    PhysicalKnockdownBlend preparePhysicalKnockdownBlend(PhysicalBlendGains current,
        float duration, float startKey, std::uint16_t controllerFlags, PhysicalBlendClock previousClock)
    {
        validate(current.mHierarchy);
        validate(current.mVelocity);
        validate(duration);
        validate(startKey);
        validate(previousClock.mElapsed);
        if (duration < 0.f)
            throw std::invalid_argument("disabled native knockdown blend duration");
        constexpr float sentinel = -std::numeric_limits<float>::max();
        previousClock.mStartTime = sentinel;
        previousClock.mPreviousTime = sentinel;
        // Setup ORs0xc5; NiTimeController::Start adds active bit0x8.
        return {{{{0.f, current}, {duration, {0.f, 0.f}}}}, startKey, duration,
            static_cast<std::uint16_t>((controllerFlags & 0xfef5u) | 0xcdu), previousClock};
    }

    float physicalBlendDurationForFilter(
        const PhysicalBlendDurationTables& tables, std::uint32_t filter, bool getUp)
    {
        const auto index = (filter >> 8) & 31u;
        const float duration = getUp ? tables.mGetUp[index] : tables.mKnockdown[index];
        validate(duration);
        return duration;
    }
}
