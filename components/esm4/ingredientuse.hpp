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

    struct IngredientFatigueProfile
    {
        float mMagnitude = IngredientFatigueMagnitude;
        float mDuration = IngredientFatigueDuration;
        friend bool operator==(const IngredientFatigueProfile&, const IngredientFatigueProfile&) = default;
    };

    // Original non-food Bonemeal at Alchemy5/Luck50: UI magnitude1,
    // completed total3. Higher skill uses a different, unimplemented profile.
    inline constexpr IngredientFatigueProfile NoviceIngredientFatigueDamage{-1.f, 3.f};

    // Original novice Cairn Bolete: Active Effects magnitude1, completed
    // Health restoration3. Authored REHE15/0 is not eating potency.
    inline constexpr IngredientFatigueProfile NoviceIngredientHealthRestoration{1.f, 3.f};

    struct IngredientRestorationStep
    {
        float mElapsed;
        float mAmount;
        bool mFinished;
    };

    inline IngredientRestorationStep advanceIngredientFatigue(
        float elapsed, float duration, IngredientFatigueProfile profile)
    {
        if ((profile != IngredientFatigueProfile{} && profile != NoviceIngredientFatigueDamage)
            || !std::isfinite(elapsed) || elapsed < 0 || elapsed >= profile.mDuration
            || !std::isfinite(duration) || duration < 0)
            throw std::invalid_argument("invalid native ingredient Fatigue profile/time");
        const float next = std::min(profile.mDuration, elapsed + duration);
        return {next, (next - elapsed) * profile.mMagnitude, next >= profile.mDuration};
    }

    inline IngredientRestorationStep advanceIngredientRestoration(float elapsed, float duration)
    {
        return advanceIngredientFatigue(elapsed, duration, {});
    }

    inline IngredientRestorationStep advanceIngredientHealthRestoration(float elapsed, float duration)
    {
        constexpr auto profile = NoviceIngredientHealthRestoration;
        if (!std::isfinite(elapsed) || elapsed < 0 || elapsed >= profile.mDuration
            || !std::isfinite(duration) || duration < 0)
            throw std::invalid_argument("invalid native ingredient Health restoration time");
        const float next = std::min(profile.mDuration, elapsed + duration);
        return {next, next - elapsed, next >= profile.mDuration};
    }
}
#endif
