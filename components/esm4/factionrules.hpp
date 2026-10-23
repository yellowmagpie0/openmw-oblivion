#ifndef OPENMW_ESM4_FACTIONRULES_H
#define OPENMW_ESM4_FACTIONRULES_H

#include <algorithm>
#include <span>

#include "actor.hpp"

namespace ESM4
{
    // Original5E02E0/467510/4F70C0: a nonempty changed list replaces the
    // base list. The first match owns the signed rank; only -1 is nonmember.
    // Faction flag mask0x8 suppresses the Player query only. Identities and
    // mutable list ownership remain with the caller.
    inline bool factionMembership(std::span<const ActorFaction> base,
        std::span<const ActorFaction> changed, ESM::FormId32 faction, bool player, std::uint8_t flags)
    {
        if (player && (flags & 0x8))
            return false;
        const auto entries = changed.empty() ? base : changed;
        const auto found = std::find_if(entries.begin(), entries.end(),
            [&](const ActorFaction& entry) { return entry.faction == faction; });
        return found != entries.end() && found->rank != -1;
    }
}

#endif
