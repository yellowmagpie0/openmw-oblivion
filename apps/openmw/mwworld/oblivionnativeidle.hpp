#ifndef OPENMW_MWWORLD_OBLIVIONNATIVEIDLE_H
#define OPENMW_MWWORLD_OBLIVIONNATIVEIDLE_H

#include <components/esm4/nativeidle.hpp>
#include <components/esm/formkey.hpp>

namespace MWWorld
{
    class ESMStore;

    struct OblivionNativeIdleLoadEvent
    {
        ESM::FormRecordMetadata mMetadata;
        ESM4::NativeIdleRegistrationEvent mRegistration;
        bool mIsTES4 = false;
    };

    // Read-only native registration snapshot over actual content order.
    // Records remain owned by the store. Actual successful load events retain
    // ordinary overrides, including same-plugin duplicates. Deleted/special
    // flags, dynamic replacements and the component's unsupported inputs are
    // rejected. Full revision history must agree with the retained events.
    // Unsupported profiles throw before publishing any actor/animation state.
    ESM4::NativeIdleRegistrationIndex buildOblivionNativeIdleIndex(const ESMStore& store);
}

#endif
