#ifndef OPENMW_COMPONENTS_ESM4_BOUNDEQUIPMENT_H
#define OPENMW_COMPONENTS_ESM4_BOUNDEQUIPMENT_H

#include "runtimestate.hpp"

namespace ESM4
{
    // Definitions resolved from the installed NPC/SPEL/MGEF/item graph. Item
    // condition and equipment here apply only to newly materialized instances.
    struct BoundEquipmentGrant
    {
        ESM::FormKey mSpell;
        std::vector<RuntimeInventoryItem> mItems;
    };

    struct BoundEquipmentUpdate
    {
        std::vector<RuntimeInventoryItem> mInventory;
        std::vector<RuntimeBoundAbility> mAbilities;
    };

    // Prepare a replacement without touching live inventory. An empty grant
    // list removes existing abilities; terminal death also removes all grants.
    // Existing instances must match their installed definitions, but their
    // saved wear, charge and equipment state are retained. Missing saved bound
    // instances are an error rather than permission to regenerate equipment.
    BoundEquipmentUpdate prepareBoundEquipmentUpdate(const std::vector<RuntimeInventoryItem>& inventory,
        const std::vector<RuntimeBoundAbility>& abilities, const std::vector<BoundEquipmentGrant>& grants,
        bool dead = false);
}

#endif
