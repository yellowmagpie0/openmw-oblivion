#include "trapdamage.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    float trapImpactDamage(const TrapImpactSettings& settings, std::uint32_t actorLevel,
        float bodySpeed, float resistance)
    {
        if (!std::isfinite(settings.mDamage) || !std::isfinite(settings.mLevelledDamage)
            || !std::isfinite(settings.mMinimumSpeed) || !std::isfinite(bodySpeed)
            || bodySpeed < 0 || !std::isfinite(resistance))
            throw std::invalid_argument("invalid scripted trap impact inputs");
        if (bodySpeed < std::max(0.f, settings.mMinimumSpeed))
            return 0;
        const double damage = (double(settings.mDamage) + double(settings.mLevelledDamage) * actorLevel)
            * (1.0 - std::min(double(resistance) / 100.0, 1.0));
        if (damage > std::numeric_limits<float>::max())
            throw std::invalid_argument("scripted trap impact damage overflow");
        return static_cast<float>(std::max(0.0, damage));
    }
}
