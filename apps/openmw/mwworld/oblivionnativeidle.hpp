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

    // Prepared over current actor/content inputs. No animation or live random
    // state is published here; the physical owner must validate and consume it.
    struct OblivionNativeIdleSelection
    {
        ESM::FormKey mActor;
        std::string mActorModel;
        ESM::FormKey mIdle;
        std::string mModel;
        std::uint32_t mExpectedRandomState = 0;
        std::uint32_t mNextRandomState = 0;
        std::uint32_t mRandomDraws = 0;
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
