#ifndef OPENMW_MWWORLD_OBLIVIONPROFILESERVICES_H
#define OPENMW_MWWORLD_OBLIVIONPROFILESERVICES_H

#include <cstddef>
#include <string>

namespace MWWorld
{
    class ESMStore;

    struct OblivionProfileInstallReport
    {
        std::size_t mNativeGameSettings = 0;
        std::size_t mRuntimeContractSettings = 0;
        std::size_t mNativeGlobals = 0;
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
    };
}

#endif
