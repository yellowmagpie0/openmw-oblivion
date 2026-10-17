#include "fatigueexpenditure.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ESM4
{
    float fatigueExpenditureDebit(float currentFatigue, float requestedDelta, bool eligible)
    {
        if (!std::isfinite(currentFatigue) || !std::isfinite(requestedDelta))
            throw std::invalid_argument("native Fatigue expenditure requires finite inputs");
        if (!eligible || currentFatigue <= 0 || requestedDelta >= 0)
            return 0.f;
        return std::min(currentFatigue, -requestedDelta);
    }
}
