#include "actorstats.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    void validateNpcDynamicStatsSettings(const NpcDynamicStatsSettings& settings)
    {
        if (!std::isfinite(settings.mAttributeHealthMultiplier) || settings.mAttributeHealthMultiplier < 0
            || settings.mPerLevelHealthMultiplier < 1 || settings.mLowLevelMaximum < 0
            || !std::isfinite(settings.mLowLevelHealthMultiplier) || settings.mLowLevelHealthMultiplier < 0
            || settings.mLowLevelHealthMultiplier > 1 || !std::isfinite(settings.mMagickaMultiplier)
            || settings.mMagickaMultiplier < 0)
            throw std::invalid_argument("unsupported native NPC dynamic-stat settings");
    }

    NpcDynamicStats calculateNpcDynamicStats(
        const NpcDynamicStatsInput& input, const NpcDynamicStatsSettings& settings)
    {
        validateNpcDynamicStatsSettings(settings);
        if (input.mLevel < 1 || input.mSpecialization > 2)
            throw std::invalid_argument("unsupported native NPC level or specialization");
        const auto checkedInteger = [](double value, double maximum) {
            if (!std::isfinite(value) || value < 0 || std::trunc(value) > maximum)
                throw std::invalid_argument("native NPC dynamic-stat arithmetic overflow");
            return static_cast<std::uint32_t>(value);
        };
        const auto attributeHealth = checkedInteger(
            double(input.mStrength + input.mEndurance) * settings.mAttributeHealthMultiplier,
            std::numeric_limits<std::int32_t>::max());
        const std::int64_t perLevel = std::int64_t(settings.mPerLevelHealthMultiplier)
            + input.mFavoredEndurance + (input.mSpecialization == 0 ? 1 : input.mSpecialization == 1 ? -1 : 0);
        const auto gained = input.mLevel - 1;
        const auto subtotal = attributeHealth + perLevel * gained;
        if (perLevel > std::numeric_limits<std::int32_t>::max()
            || subtotal > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("native NPC health integer arithmetic overflow");
        float factor = 1;
        if (gained < settings.mLowLevelMaximum)
        {
            const float ratio = static_cast<float>(double(gained) / settings.mLowLevelMaximum);
            const float scaled = static_cast<float>(double(ratio) * (1.0 - settings.mLowLevelHealthMultiplier));
            factor = static_cast<float>(double(settings.mLowLevelHealthMultiplier) + scaled);
        }
        const auto health = checkedInteger(double(subtotal) * factor, std::numeric_limits<std::int32_t>::max());
        const auto magicka = checkedInteger(double(input.mIntelligence) * settings.mMagickaMultiplier
                + input.mIntelligence, std::numeric_limits<std::uint16_t>::max());
        const auto fatigue = input.mStrength + input.mWillpower + input.mAgility + input.mEndurance;
        return {health, static_cast<std::uint16_t>(magicka), static_cast<std::uint16_t>(fatigue)};
    }

    std::int16_t resolveActorLevel(const ACBS_TES4& base, std::optional<std::uint16_t> playerLevel)
    {
        if (!(base.flags & 0x80))
            return base.levelOrOffset;
        const auto word = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(base.levelOrOffset) + playerLevel.value_or(0));
        const auto level = std::bit_cast<std::int16_t>(word);
        if (base.calcMin > 0 && level < base.calcMin)
            return std::bit_cast<std::int16_t>(base.calcMin);
        if (base.calcMax > 0 && level > base.calcMax)
            return std::bit_cast<std::int16_t>(base.calcMax);
        return std::max<std::int16_t>(level, 1);
    }
}
