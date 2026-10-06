#ifndef OPENMW_MWSTATE_SAVEADMISSION_H
#define OPENMW_MWSTATE_SAVEADMISSION_H

#include <functional>
#include <memory>

#include <components/esm/gameprofile.hpp>
#include <components/esm3/savedgame.hpp>

namespace ESM { class ESMReader; }
namespace ESM4 { struct RuntimeState; }
namespace MWWorld { class ESMStore; }

namespace MWState
{
    // Read-only admission before tearing down the running game. Checks outer
    // framing and native/profile metadata, then restores the same open reader
    // to its starting position. Record-specific restoration still follows.
    // When supplied, prepareNative replaces validateNative and owns the decoded
    // incoming definition store only after every other admission layer passes.
    ESM::SavedGame admitSave(ESM::ESMReader& reader, ESM::GameProfile activeProfile,
        const std::function<void(const ESM4::RuntimeState&)>& validateNative,
        const MWWorld::ESMStore* content = nullptr,
        const std::function<void(const ESM4::RuntimeState&, std::unique_ptr<MWWorld::ESMStore>)>& prepareNative = {});
}

#endif
