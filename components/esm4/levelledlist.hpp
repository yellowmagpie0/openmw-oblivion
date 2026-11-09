#ifndef OPENMW_ESM4_LEVELLEDLIST_H
#define OPENMW_ESM4_LEVELLEDLIST_H

#include "inventory.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace ESM4
{
    // TES4 0046CC70 selects the nearest eligible tier unless flag1 opens a
    // level window. A zero maximum difference opens all lower levels. If the
    // window contains no tier, retain the nearest lower tier instead.
    // 0046CDE0 first clamps an all-level container list to its highest tier;
    // nested lists receive the original requested level, not that local clamp.
    inline std::vector<std::size_t> levelledEntryPool(std::span<const LVLO> entries,
        std::uint16_t requestedLevel, std::uint8_t flags, std::uint16_t maximumDifference,
        bool containerCalculation)
    {
        std::uint16_t highest = 0;
        for (const auto& entry : entries)
        {
            if (entry.level <= 0 || entry.count <= 0)
                throw std::invalid_argument("native leveled entry requires positive level and count");
            highest = std::max(highest, static_cast<std::uint16_t>(entry.level));
        }
        const bool all = (flags & 1) != 0;
        const auto level = all && containerCalculation ? std::min(requestedLevel, highest) : requestedLevel;
        std::uint16_t nearest = 0;
        for (const auto& entry : entries)
            if (entry.level <= level)
                nearest = std::max(nearest, static_cast<std::uint16_t>(entry.level));
        std::vector<std::size_t> result;
        if (nearest == 0) return result;
        const int minimum = !all ? nearest
            : maximumDifference == 0 ? 0 : std::max(0, int(level) - int(maximumDifference));
        const bool windowHasTier = std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
            return entry.level >= minimum && entry.level <= level;
        });
        for (std::size_t i = 0; i < entries.size(); ++i)
            if (entries[i].level <= level
                && entries[i].level >= (windowHasTier ? minimum : int(nearest)))
                result.push_back(i);
        return result;
    }
}

#endif
