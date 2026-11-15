#ifndef OPENMW_ESM4_REPAIR_H
#define OPENMW_ESM4_REPAIR_H

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>

namespace ESM4
{
    struct RepairSettings
    {
        float mArmorerBase = 0.f;
        float mArmorerMultiplier = 3.f;
        float mStrengthMultiplier = .1f;
        float mBreakBase = 30.f;
        float mBreakSkillMultiplier = -.2f;
        float mApprenticeBreakMultiplier = .5f;
        float mLuckSkillMultiplier = .4f;
        int mApprenticeMinimum = 25;
        int mMasterMinimum = 100;

        void validate() const
        {
            for (float value : std::array{mArmorerBase, mArmorerMultiplier, mStrengthMultiplier,
                     mBreakBase, mBreakSkillMultiplier, mApprenticeBreakMultiplier, mLuckSkillMultiplier})
                if (!std::isfinite(value))
                    throw std::invalid_argument("non-finite native repair setting");
            if (mApprenticeMinimum < 0 || mMasterMinimum < mApprenticeMinimum)
                throw std::invalid_argument("invalid native repair mastery thresholds");
        }
    };

    inline int repairAmount(int armorer, int strength, const RepairSettings& settings)
    {
        settings.validate();
        const double amount = settings.mArmorerBase + double(armorer) * settings.mArmorerMultiplier
            + double(strength) * settings.mStrengthMultiplier;
        if (!std::isfinite(amount) || amount < 0 || amount > std::numeric_limits<int>::max())
            throw std::invalid_argument("native repair amount outside supported range");
        return static_cast<int>(amount);
    }

    inline float repairHammerBreakChance(int armorer, int luck, const RepairSettings& settings)
    {
        settings.validate();
        if (armorer >= settings.mMasterMinimum) return 0.f;
        const float effective = std::clamp(armorer + settings.mLuckSkillMultiplier * (luck - 50.f), 0.f, 100.f);
        float chance = settings.mBreakBase + effective * settings.mBreakSkillMultiplier;
        if (armorer >= settings.mApprenticeMinimum) chance *= settings.mApprenticeBreakMultiplier;
        return std::clamp(chance, 0.f, 100.f);
    }

    // Ordinary repair through normal maximum condition. Expert over-repair
    // eligibility and presentation remain separate from this bounded path.
    inline std::optional<float> repairedCondition(float current, int maximum, int amount)
    {
        if (!std::isfinite(current) || current < 0 || maximum < 1 || amount < 0)
            throw std::invalid_argument("invalid native repair condition");
        if (current >= maximum || amount == 0) return std::nullopt;
        const float after = static_cast<float>(std::min(double(maximum), std::trunc(double(current)) + amount));
        return after > current ? std::optional<float>{after} : std::nullopt;
    }
}
#endif
