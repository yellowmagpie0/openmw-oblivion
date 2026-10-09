#ifndef OPENMW_MWSTATE_SAVEADMISSION_H
#define OPENMW_MWSTATE_SAVEADMISSION_H

#include <functional>
#include <memory>
#include <vector>

#include <components/esm/gameprofile.hpp>
#include <components/esm3/savedgame.hpp>

namespace ESM { class ESMReader; class RefId; struct ObjectState; struct QuickKeys; struct FogState; struct GlobalMap; struct WeatherState; struct ProjectileState; struct MagicBoltState; }
namespace ESM4 { struct RuntimeState; }
namespace MWWorld { class ESMStore; }

namespace MWState
{
    // Read-only admission before tearing down the running game. Checks outer
    // framing and native/profile metadata, then restores the same open reader
    // to its starting position. Record-specific restoration still follows.
    // When supplied, prepareNative replaces validateNative and owns the decoded
    // incoming definition store only after every other admission layer passes.
    // Quickkey resource preparation sees detached incoming definitions and the
    // migrated native snapshot (if present), before ownership moves to native
    // preparation. It must not mutate live bindings or inventory.
    ESM::SavedGame admitSave(ESM::ESMReader& reader, ESM::GameProfile activeProfile,
        const std::function<void(const ESM4::RuntimeState&)>& validateNative,
        const MWWorld::ESMStore* content = nullptr,
        const std::function<void(const ESM4::RuntimeState&, std::unique_ptr<MWWorld::ESMStore>)>& prepareNative = {},
        const std::function<void(const ESM::GlobalMap&)>& prepareGlobalMap = {},
        const std::function<void(const ESM::WeatherState&)>& prepareWeather = {},
        const std::function<void(const std::vector<ESM::ProjectileState>&,
            const std::vector<ESM::MagicBoltState>&, const MWWorld::ESMStore&)>& prepareProjectiles = {},
        const std::function<void(const ESM::RefId&, ESM::FogState)>& prepareFog = {},
        const std::function<void(const ESM::QuickKeys&, const MWWorld::ESMStore&,
            const ESM4::RuntimeState*)>& prepareQuickKeys = {},
        const std::function<void(ESM::ObjectState&, const MWWorld::ESMStore&,
            const ESM4::RuntimeState*)>& prepareInventory = {},
        const std::function<void(std::unique_ptr<MWWorld::ESMStore>)>& prepareSharedDefinitions = {},
        const std::function<bool(const ESM::RefId&)>& restoreCell = {});
}

#endif
