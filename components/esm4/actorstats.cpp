#include "actorstats.hpp"

#include <algorithm>
#include <bit>

namespace ESM4
{
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
