#ifndef OPENMW_MWSTATE_SAVEADMISSION_H
#define OPENMW_MWSTATE_SAVEADMISSION_H

#include <functional>

#include <components/esm/gameprofile.hpp>
#include <components/esm3/savedgame.hpp>

namespace ESM { class ESMReader; }
namespace ESM4 { struct RuntimeState; }

namespace MWState
{
    // Read-only admission before tearing down the running game. Checks outer
    // framing and native/profile metadata, then restores the same open reader
    // to its starting position. Record-specific restoration still follows.
    ESM::SavedGame admitSave(ESM::ESMReader& reader, ESM::GameProfile activeProfile,
        const std::function<void(const ESM4::RuntimeState&)>& validateNative);
}

#endif
