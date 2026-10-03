#include "physicalblendsettings.hpp"

#include <cmath>
#include <stdexcept>

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

    float physicalBlendDurationForFilter(
        const PhysicalBlendDurationTables& tables, std::uint32_t filter, bool getUp)
    {
        const auto index = (filter >> 8) & 31u;
        const float duration = getUp ? tables.mGetUp[index] : tables.mKnockdown[index];
        validate(duration);
        return duration;
    }
}
