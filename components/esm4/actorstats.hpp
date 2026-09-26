#ifndef OPENMW_ESM4_ACTORSTATS_H
#define OPENMW_ESM4_ACTORSTATS_H

#include "actor.hpp"

#include <cstdint>
#include <optional>

namespace ESM4
{
    // Pure native ACBS lookup. The optional player level represents its resolved
    // base-record field. Fixed levels bypass offset clamps. The returned signed
    // word preserves native wrapping and bounds order, including malformed raw
    // records; callers must diagnose unsupported effective actor configurations.
    std::int16_t resolveActorLevel(const ACBS_TES4& base, std::optional<std::uint16_t> playerLevel);
}

#endif
