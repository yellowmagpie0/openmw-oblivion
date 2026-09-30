#ifndef OPENMW_ESM4_LOADMGEF_H
#define OPENMW_ESM4_LOADMGEF_H

#include "loadrawrecord.hpp"
#include <components/esm/defs.hpp>
#include <optional>

namespace ESM4
{
    struct EffectSettingData
    {
        std::uint32_t mFlags = 0;
        float mBaseCost = 0;
        std::uint32_t mAssociatedData = 0;
        std::uint32_t mSchool = 0;
        std::int32_t mResistanceActorValue = 0;
        std::uint16_t mCounterCount = 0;
        std::uint16_t mCounterPadding = 0;
        // Presence identifies an authored item-reference field; otherwise the
        // data word remains numeric. Ready-game flags/data are resolved later.
        std::optional<ESM::FormKey> mAssociatedForm;
    };

    // TES4 authored inputs, distinct from the original compiled/loaded rules.
    // Decode the common DATA prefix; preserve all tail fields losslessly.
    struct EffectSetting : RawRecord
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_MGEF4;
        std::optional<std::uint32_t> mEffectCode;
        std::optional<EffectSettingData> mData;
        void load(Reader& reader);
    };
}
#endif
