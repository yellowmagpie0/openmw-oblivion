#ifndef OPENMW_ESM4_RUNTIME_REFERENCES_H
#define OPENMW_ESM4_RUNTIME_REFERENCES_H

#include <components/esm/formkey.hpp>

namespace ESM4
{
    // Native references use 000014 for the player, even though no placed
    // record exists for it. Keep record keys lossless; canonicalize only when
    // resolving/comparing runtime reference identity. NPC base 000007 is not
    // an alias, nor is a different plugin's own local form 000014.
    inline ESM::FormKey runtimeReferenceKey(const ESM::FormKey& key)
    {
        if (key == ESM::FormKey::content("Oblivion.esm", 0x14))
            return ESM::FormKey::dynamic("player", 1);
        return key;
    }
}

#endif
