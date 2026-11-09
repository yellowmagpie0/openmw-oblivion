#ifndef OPENMW_ESM4_INGREDIENTUSE_H
#define OPENMW_ESM4_INGREDIENTUSE_H

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ESM4
{
    // Original 1.2.0416 Wortcraft REFA: native Active Effects magnitude5,
    // total restoration5. Independent Lettuce/Rice and Alchemy5/100 probes.
    // These are ingestion inputs, not the authored EFIT magnitude15.
    inline constexpr float IngredientFatigueMagnitude = 5.f;
    inline constexpr float IngredientFatigueDuration = 1.f;

    struct IngredientRestorationStep
    {
        float mElapsed;
        float mAmount;
        bool mFinished;
    };

    inline IngredientRestorationStep advanceIngredientRestoration(float elapsed, float duration)
    {
        if (!std::isfinite(elapsed) || elapsed < 0 || elapsed >= IngredientFatigueDuration
            || !std::isfinite(duration) || duration < 0)
            throw std::invalid_argument("invalid native ingredient restoration time");
        const float next = std::min(IngredientFatigueDuration, elapsed + duration);
        return {next, (next - elapsed) * IngredientFatigueMagnitude, next >= IngredientFatigueDuration};
    }
}
#endif
