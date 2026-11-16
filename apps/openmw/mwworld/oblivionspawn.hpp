#ifndef OPENMW_MWWORLD_OBLIVIONSPAWN_H
#define OPENMW_MWWORLD_OBLIVIONSPAWN_H

#include <components/esm4/runtimestate.hpp>
#include <algorithm>
#include <optional>
#include <set>
#include <stdexcept>

namespace MWWorld
{
    // A leveled actor template supplies script/factions to this reference, not
    // to the selected shared NPC/CREA base. Keep the ordered snapshot in save
    // authority so later base edits cannot change already-created encounters.
    inline std::optional<ESM::FormKey> oblivionSpawnScript(const ESM4::RuntimeReferenceState& reference)
    {
        const auto found = reference.mCustomState.find("native.spawn.script");
        if (found == reference.mCustomState.end()) return {};
        const auto* text = std::get_if<std::string>(&found->second);
        if (!text) throw std::invalid_argument("Invalid spawned actor script snapshot");
        const auto key = ESM::FormKey::deserialize(*text);
        if (key.serialize() != *text || (!key.isNull() && !key.isContent()))
            throw std::invalid_argument("Invalid spawned actor script identity");
        return key;
    }

    inline std::optional<std::vector<ESM4::RuntimeActorFaction>> oblivionSpawnFactions(
        const ESM4::RuntimeReferenceState& reference)
    {
        const auto found = reference.mCustomState.find("native.spawn.factions");
        if (found == reference.mCustomState.end()) return {};
        const auto* count = std::get_if<std::int64_t>(&found->second);
        if (!count || *count < 0 || *count > 4096)
            throw std::invalid_argument("Invalid spawned actor faction count");
        std::vector<ESM4::RuntimeActorFaction> result;
        std::set<ESM::FormKey> seen;
        for (std::int64_t i = 0; i < *count; ++i)
        {
            const auto prefix = "native.spawn.faction." + std::to_string(i);
            const auto keyIt = reference.mCustomState.find(prefix + ".key");
            const auto rankIt = reference.mCustomState.find(prefix + ".rank");
            const auto* text = keyIt == reference.mCustomState.end() ? nullptr
                : std::get_if<std::string>(&keyIt->second);
            const auto* rank = rankIt == reference.mCustomState.end() ? nullptr
                : std::get_if<std::int64_t>(&rankIt->second);
            if (!text || !rank || *rank < -128 || *rank > 127)
                throw std::invalid_argument("Invalid spawned actor faction snapshot");
            const auto key = ESM::FormKey::deserialize(*text);
            if (!key.isContent() || key.serialize() != *text || !seen.insert(key).second)
                throw std::invalid_argument("Invalid spawned actor faction identity");
            result.push_back({key, static_cast<std::int8_t>(*rank)});
        }
        return result;
    }

    inline void setOblivionSpawnFactions(ESM4::RuntimeReferenceState& reference,
        const std::vector<ESM4::RuntimeActorFaction>& factions)
    {
        auto replacement = reference.mCustomState;
        std::erase_if(replacement, [](const auto& entry) { return entry.first.starts_with("native.spawn.faction."); });
        replacement["native.spawn.factions"] = std::int64_t(factions.size());
        for (std::size_t i = 0; i < factions.size(); ++i)
        {
            const auto prefix = "native.spawn.faction." + std::to_string(i);
            replacement[prefix + ".key"] = factions[i].mFaction.serialize();
            replacement[prefix + ".rank"] = std::int64_t(factions[i].mRank);
        }
        reference.mCustomState.swap(replacement);
    }
}
#endif
