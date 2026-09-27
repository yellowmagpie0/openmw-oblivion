#ifndef OPENMW_MWWORLD_OBLIVIONPROFILESERVICES_H
#define OPENMW_MWWORLD_OBLIVIONPROFILESERVICES_H

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include <components/esm/refid.hpp>
#include <components/esm4/inventorymechanics.hpp>

#include "manualref.hpp"

namespace MWWorld
{
    class ESMStore;
    class InventoryStore;

    struct PreparedOblivionInventoryItem
    {
        ManualRef mReference;
        std::optional<int> mEquipmentSlot;
    };

    struct OblivionProfileInstallReport
    {
        std::size_t mNativeGameSettings = 0;
        std::size_t mRuntimeContractSettings = 0;
        std::size_t mNativeGlobals = 0;
        std::size_t mProjectedItems = 0;
        std::string mPlayerSource;
        std::string mRaceSource;
        std::string mClassSource;
    };

    /// Installs the narrow adapters required by shared runtime systems from
    /// native Oblivion records. This service is selected only for TES4 data.
    class OblivionProfileServices
    {
    public:
        static OblivionProfileInstallReport install(ESMStore& store);
        static std::optional<ESM4::InventoryItemDefinition> itemDefinition(
            const ESMStore& store, const ESM::RefId& id);
        static ESM::RefId sharedItemId(const ESMStore& store, const ESM::RefId& nativeId);
        static ESM::RefId nativeItemId(const ESMStore& store, const ESM::RefId& sharedId);
        static int sharedItemType(const ESMStore& store, const ESM::RefId& sharedId);

        // Resolve and construct the entire replacement before a caller clears
        // a live inventory. These references are detached and unregistered.
        // Publication/registration and equipment callbacks remain caller-owned.
        static std::vector<PreparedOblivionInventoryItem> prepareActorInventory(const ESMStore& store,
            const ESM::FormKeyResolver& resolver, const std::vector<ESM4::RuntimeInventoryItem>& items);
        static std::unique_ptr<InventoryStore> stageActorInventory(
            const std::vector<PreparedOblivionInventoryItem>& items);

        // Populate the shared equipment slots from a projected native actor
        // inventory.  This is deliberately separate from InventoryStore's
        // TES3 auto-equip heuristic: TES4 slot conflicts and ring choice are
        // resolved from the native item definition.
        static void equipNativeApparel(InventoryStore& inventory, const ESMStore& store);
    };
}

#endif
