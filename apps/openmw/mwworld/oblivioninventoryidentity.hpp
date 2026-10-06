#ifndef OPENMW_MWWORLD_OBLIVIONINVENTORYIDENTITY_H
#define OPENMW_MWWORLD_OBLIVIONINVENTORYIDENTITY_H

#include <limits>
#include <optional>
#include <stdexcept>

#include <components/esm/formkey.hpp>
#include <components/esm/refid.hpp>
#include <components/esm4/inventorymechanics.hpp>
#include <components/esm4/loadarmo.hpp>

#include "inventorystore.hpp"

namespace MWWorld::OblivionInventory
{
    // GeneratedRefId includes zero; FormKey dynamic serials start at one.
    // This is a shared record identity, never a native reference allocation.
    inline std::optional<ESM::FormKey> sharedKey(const ESM::RefId& id)
    {
        const auto* generated = id.getIf<ESM::GeneratedRefId>();
        if (!generated)
            return std::nullopt;
        if (generated->getValue() == std::numeric_limits<std::uint64_t>::max())
            throw std::invalid_argument("Generated inventory identity cannot be represented");
        return ESM::FormKey::dynamic("shared-item", generated->getValue() + 1);
    }

    inline std::optional<ESM::RefId> sharedId(const ESM::FormKey& key)
    {
        if (!key.isDynamic() || key.mNamespace != "shared-item")
            return std::nullopt;
        if (key.mValue == 0)
            throw std::invalid_argument("Invalid generated inventory identity");
        return ESM::RefId::generated(key.mValue - 1);
    }

    // Preserve the shared equipment selection in the existing TES4 save-slot
    // domain. This describes persistence only, not native item rule derivation.
    inline std::uint32_t slotMask(int slot, bool light)
    {
        switch (slot)
        {
            case InventoryStore::Slot_Helmet: return ESM4::Armor::TES4_Head;
            case InventoryStore::Slot_Cuirass:
            case InventoryStore::Slot_Shirt: return ESM4::Armor::TES4_UpperBody;
            case InventoryStore::Slot_Greaves:
            case InventoryStore::Slot_Pants: return ESM4::Armor::TES4_LowerBody;
            case InventoryStore::Slot_LeftGauntlet:
            case InventoryStore::Slot_RightGauntlet: return ESM4::Armor::TES4_Hands;
            case InventoryStore::Slot_Boots: return ESM4::Armor::TES4_Feet;
            case InventoryStore::Slot_Robe: return ESM4::Armor::TES4_UpperBody | ESM4::Armor::TES4_LowerBody;
            case InventoryStore::Slot_LeftRing: return ESM4::Armor::TES4_LeftRing;
            case InventoryStore::Slot_RightRing: return ESM4::Armor::TES4_RightRing;
            case InventoryStore::Slot_Amulet: return ESM4::Armor::TES4_Amulet;
            case InventoryStore::Slot_CarriedRight: return ESM4::InventorySlotWeapon;
            case InventoryStore::Slot_CarriedLeft: return light ? ESM4::InventorySlotLight : ESM4::Armor::TES4_Shield;
            case InventoryStore::Slot_Ammunition: return ESM4::InventorySlotAmmunition;
            default: throw std::invalid_argument("Shared inventory equipment slot has no TES4 save representation");
        }
    }
}

#endif
