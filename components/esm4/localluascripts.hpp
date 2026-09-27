#ifndef OPENMW_COMPONENTS_ESM4_LOCALLUASCRIPTS_H
#define OPENMW_COMPONENTS_ESM4_LOCALLUASCRIPTS_H

#include <components/esm/formkey.hpp>
#include <components/esm/luascripts.hpp>

namespace ESM4
{
    struct RuntimeState;

    // Optional versioned companion to the global Lua state in LUAM. The Player
    // retains its existing save path; other native references use stable keys.
    using LocalLuaScripts = std::map<ESM::FormKey, ESM::LuaScripts>;

    LocalLuaScripts loadLocalLuaScripts(ESM::ESMReader& reader);
    void saveLocalLuaScripts(ESM::ESMWriter& writer, const LocalLuaScripts& scripts);
    void validateLocalLuaScriptOwners(const LocalLuaScripts& scripts, const RuntimeState& state);
    void validateLocalLuaScriptContent(const LocalLuaScripts& scripts, const ESM::FormKeyIndex& index);
}

#endif
