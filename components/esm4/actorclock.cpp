#include "actorclock.hpp"

#include <cmath>
#include <stdexcept>

namespace ESM4
{
    float normalizeActorManagerTime(float time) noexcept
    {
        return !std::isfinite(time) || time > 100000.f ? 0.f : time;
    }

    float advanceActorManagerTime(float time, float elapsed) noexcept
    {
        return normalizeActorManagerTime(static_cast<float>(double(time) + elapsed));
    }

    float actorManagerTimeAfterHour(float time, float timeScale) noexcept
    {
        const float elapsed = static_cast<float>(3600.0 / timeScale);
        return advanceActorManagerTime(time, elapsed);
    }

    float actorUpdateDuration(float time, float previousTime)
    {
        if (!std::isfinite(time) || !std::isfinite(previousTime))
            throw std::invalid_argument("native actor update clock must be finite");
        if (previousTime > time || previousTime < 0.f)
            return time > 0.f && time < .3f ? time : 0.f;
        return static_cast<float>(double(time) - previousTime);
    }
}
