#ifndef OPENMW_MWWORLD_OBLIVIONENABLESTATE_H
#define OPENMW_MWWORLD_OBLIVIONENABLESTATE_H

#include <components/esm/formkey.hpp>
#include <optional>
#include <set>
#include <stdexcept>

namespace MWWorld
{
    struct OblivionEnableReference
    {
        bool mEnabled = true;
        ESM::FormKey mParent;
        bool mInverted = false;
    };

    // Original 4DA2D0 applies parentDisabled XOR XESP bit0. Follow parent
    // chains without recursive stack growth. Missing parents retain the last
    // reference's own state; cyclic content is rejected explicitly.
    template <class Lookup>
    std::optional<bool> resolveOblivionEnableState(ESM::FormKey key, Lookup&& lookup)
    {
        std::set<ESM::FormKey> visited;
        std::optional<bool> fallback;
        bool inverted = false;
        while (!key.isNull())
        {
            if (!visited.insert(key).second)
                throw std::runtime_error("Cyclic TES4 enable-parent chain: " + key.serialize());
            const auto node = lookup(key);
            if (!node) return fallback;
            fallback = node->mEnabled != inverted;
            if (node->mParent.isNull()) return fallback;
            inverted = inverted != node->mInverted;
            key = node->mParent;
        }
        return fallback;
    }
}
#endif
