#ifndef OPENMW_COMPONENTS_OBSCRIPT_BUILTINREFERENCES_H
#define OPENMW_COMPONENTS_OBSCRIPT_BUILTINREFERENCES_H

#include <components/esm/formkey.hpp>
#include <components/misc/strings/algorithm.hpp>

#include <optional>
#include <string_view>

namespace ObScript
{
    // Expressions and event filters must agree on native runtime identities.
    // In particular, the player instance is not the authored Player NPC base.
    inline std::optional<ESM::FormKey> builtinReferenceKey(
        std::string_view name, const ESM::FormKey& self, const ESM::FormKey& actionReference)
    {
        if (Misc::StringUtils::ciEqual(name, "player"))
            return ESM::FormKey::dynamic("player", 1);
        if (Misc::StringUtils::ciEqual(name, "self"))
            return self;
        if (Misc::StringUtils::ciEqual(name, "actionref") || Misc::StringUtils::ciEqual(name, "actionreference"))
            return actionReference;
        return std::nullopt;
    }
}

#endif
