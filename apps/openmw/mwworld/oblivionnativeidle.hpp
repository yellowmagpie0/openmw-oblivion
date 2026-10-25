#ifndef OPENMW_MWWORLD_OBLIVIONNATIVEIDLE_H
#define OPENMW_MWWORLD_OBLIVIONNATIVEIDLE_H

#include <components/esm4/nativeidle.hpp>

namespace MWWorld
{
    class ESMStore;

    // Read-only native registration snapshot over actual content order.
    // Records remain owned by the store. This verified profile requires one
    // original definition per IDLE key, no deletion or dynamic replacement,
    // and the component registration domain. Revision history is inspected
    // rather than treating final winning order as native load-event history.
    // Unsupported profiles throw before publishing any actor/animation state.
    ESM4::NativeIdleRegistrationIndex buildOblivionNativeIdleIndex(const ESMStore& store);
}

#endif
