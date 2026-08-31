#ifndef OPENMW_MWWORLD_OBLIVIONPROFILESERVICES_H
#define OPENMW_MWWORLD_OBLIVIONPROFILESERVICES_H

#include <cstddef>
#include <optional>
#include <string>

#include <components/esm/refid.hpp>
#include <components/esm4/inventorymechanics.hpp>

namespace MWWorld
{
    class ESMStore;

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
    };
}

#endif
