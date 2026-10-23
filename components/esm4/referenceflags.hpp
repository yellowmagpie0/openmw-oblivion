#ifndef OPENMW_COMPONENTS_ESM4_REFERENCEFLAGS_H
#define OPENMW_COMPONENTS_ESM4_REFERENCEFLAGS_H

#include <cstdint>
#include <optional>

namespace ESM4
{
    inline constexpr std::uint32_t IgnoreFriendlyHitsMask = 0x00100000;

    constexpr bool ignoresFriendlyHits(std::uint32_t flags)
    {
        return (flags & IgnoreFriendlyHitsMask) != 0;
    }

    // Original506750/46AB20: -1 is an accepted no-op; every other int32
    // requests a reference flag write, including a numerically equal value.
    constexpr std::optional<std::uint32_t> ignoreFriendlyHitsFlags(std::uint32_t flags, std::int32_t value)
    {
        if (value == -1)
            return std::nullopt;
        return value ? flags | IgnoreFriendlyHitsMask : flags & ~IgnoreFriendlyHitsMask;
    }
}

#endif
