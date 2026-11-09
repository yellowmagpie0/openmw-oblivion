#ifndef OPENMW_MWWORLD_OBLIVIONLEVELLED_H
#define OPENMW_MWWORLD_OBLIVIONLEVELLED_H

#include <optional>
#include <vector>
#include <components/esm/formkey.hpp>
#include <components/esm4/inventory.hpp>
#include <components/misc/rng.hpp>

namespace ESM4 { struct RuntimeReferenceState; }
namespace MWWorld
{
    class ESMStore;
    std::vector<ESM4::InventoryItem> expandOblivionLevelledItems(const ESMStore& store,
        const std::vector<ESM4::InventoryItem>& items, int level, Misc::Rng::Generator& random);
    bool supportsOblivionLevelledActor(const ESMStore& store, const ESM::FormKey& list, int level);
    std::optional<ESM::FormId> selectOblivionLevelledActor(const ESMStore& store,
        const ESM::FormKey& list, int level, Misc::Rng::Generator& random);
    bool oblivionLevelledActorContains(const ESMStore& store, const ESM::FormKey& reference,
        const ESM::FormKey& base, std::optional<int> level = std::nullopt);
    // Returns true only for a validated saved selection, including a saved empty draw.
    bool validateOblivionLevelledActor(const ESMStore& store, const ESM4::RuntimeReferenceState& reference);
}
#endif
