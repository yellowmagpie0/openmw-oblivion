#ifndef OPENMW_COMPONENTS_ESM4_PLAYERMISCSTATS_H
#define OPENMW_COMPONENTS_ESM4_PLAYERMISCSTATS_H

#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ESM4
{
    inline constexpr std::string_view PlayerMiscStatPrefix = "pc_misc_stat.";
    inline constexpr std::int32_t PlayerMiscStatCount = 34;

    inline std::string playerMiscStatKey(std::int64_t index)
    {
        if (index < 0 || index >= PlayerMiscStatCount)
            throw std::invalid_argument("Native Player miscellaneous-stat index is outside 0..33");
        return std::string(PlayerMiscStatPrefix) + std::to_string(index);
    }

    inline std::uint32_t playerMiscStatValue(double value)
    {
        if (!std::isfinite(value) || value < 0 || value > std::numeric_limits<std::uint32_t>::max()
            || std::trunc(value) != value)
            throw std::invalid_argument("Native Player miscellaneous stat requires a uint32 value");
        return static_cast<std::uint32_t>(value);
    }

    inline std::uint32_t addPlayerMiscStat(std::uint32_t value, std::int32_t delta) noexcept
    {
        // Original50D795 writes the low 32 bits; Original4F8590 reads unsigned.
        return value + static_cast<std::uint32_t>(delta);
    }

    inline void validatePlayerMiscStats(const std::map<std::string, double, std::less<>>& values)
    {
        for (const auto& [name, value] : values)
        {
            if (!name.starts_with(PlayerMiscStatPrefix)) continue;
            const std::string_view suffix(name.data() + PlayerMiscStatPrefix.size(),
                name.size() - PlayerMiscStatPrefix.size());
            std::int32_t index = -1;
            const auto parsed = std::from_chars(suffix.data(), suffix.data() + suffix.size(), index);
            if (parsed.ec != std::errc{} || parsed.ptr != suffix.data() + suffix.size()
                || playerMiscStatKey(index) != name)
                throw std::invalid_argument("Noncanonical native Player miscellaneous-stat key");
            playerMiscStatValue(value);
        }
    }
}

#endif
