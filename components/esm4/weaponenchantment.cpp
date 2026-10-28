#include "weaponenchantment.hpp"

#include "loadgmst.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace ESM4
{
    namespace
    {
        void validate(const WeaponEnchantmentCostSettings& settings)
        {
            for (float value : {settings.mAreaMultiplier, settings.mDurationMultiplier,
                     settings.mMagnitudeExponent, settings.mTargetMultiplier})
                if (!std::isfinite(value) || value < 0)
                    throw std::invalid_argument("invalid native weapon enchantment cost setting");
        }
    }

    WeaponEnchantmentCostSettings buildWeaponEnchantmentCostSettings(
        std::span<const GameSetting* const> settings)
    {
        WeaponEnchantmentCostSettings result;
        const auto read = [&](const char* name, float fallback) {
            const GameSetting* found = nullptr;
            for (const auto* setting : settings)
            {
                if (!setting)
                    throw std::invalid_argument("null native enchantment setting");
                std::string key = setting->mEditorId;
                for (char& c : key)
                    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
                if (key != name) continue;
                if (found) throw std::invalid_argument("ambiguous native enchantment setting");
                found = setting;
            }
            if (!found) return fallback;
            const auto* value = std::get_if<float>(&found->mData);
            if (!value) throw std::invalid_argument("incorrect native enchantment setting type");
            return *value;
        };
        // Original initializers9EE2FF/9EE32F/9EFDEF/9EE35F. The installed
        // Oblivion.esm overrides only the exponent here, from1.25 to1.28.
        result.mAreaMultiplier = read("fmagicareabasecostmult", result.mAreaMultiplier);
        result.mDurationMultiplier = read("fmagicdurmagbasecostmult", result.mDurationMultiplier);
        result.mMagnitudeExponent = read("fmagiccostscale", result.mMagnitudeExponent);
        result.mTargetMultiplier = read("fmagicrangetargetcostmult", result.mTargetMultiplier);
        validate(result);
        return result;
    }

    float weaponEnchantmentCost(std::optional<std::uint32_t> manualCost,
        std::span<const WeaponEnchantmentCostEffect> effects,
        const WeaponEnchantmentCostSettings& settings)
    {
        if (manualCost)
        {
            if (*manualCost > std::numeric_limits<std::int32_t>::max())
                throw std::invalid_argument("native manual enchantment cost outside supported domain");
            return static_cast<float>(*manualCost);
        }
        validate(settings);
        double total = 0;
        for (const auto& effect : effects)
        {
            if (effect.mNoCost) continue;
            if (!std::isfinite(effect.mBaseCost) || effect.mBaseCost < 0 || effect.mRange > 2)
                throw std::invalid_argument("invalid native enchantment effect cost input");
            const double area = effect.mNoArea || effect.mRange == 0 ? 1
                : std::max(1., double(effect.mArea) * settings.mAreaMultiplier);
            const double duration = effect.mNoDuration ? 1 : std::max(1u, effect.mDuration);
            const double magnitude = effect.mNoMagnitude ? 1
                : std::pow(double(std::max(1u, effect.mMagnitude)), settings.mMagnitudeExponent);
            const double range = effect.mRange == 2 ? settings.mTargetMultiplier : 1;
            const double cost = effect.mBaseCost * double(settings.mDurationMultiplier)
                * duration * magnitude * area * range;
            if (!std::isfinite(cost) || cost > std::numeric_limits<std::int32_t>::max())
                throw std::overflow_error("native enchantment effect cost outside supported domain");
            total += std::max(1., std::floor(cost));
        }
        if (total > std::numeric_limits<std::int32_t>::max())
            throw std::overflow_error("native enchantment cost outside supported domain");
        return static_cast<float>(total);
    }

    WeaponEnchantmentCharge weaponEnchantmentCharge(float current, float maximum, float cost)
    {
        if (!std::isfinite(current) || (current < 0 && current != -1)
            || !std::isfinite(maximum) || maximum < 0 || !std::isfinite(cost) || cost < 0)
            throw std::invalid_argument("invalid native weapon enchantment charge");
        const float charge = current == -1 ? maximum : current;
        if (charge < cost) return {false, std::nullopt};
        return {true, charge - cost};
    }
}
