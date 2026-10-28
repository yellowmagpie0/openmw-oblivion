#ifndef OPENMW_ESM4_LOADENCH_H
#define OPENMW_ESM4_LOADENCH_H

#include "loadspel.hpp"

namespace ESM4
{
    struct EnchantmentData
    {
        std::uint32_t mType = 2; // Scroll, Staff, Weapon, Apparel.
        std::uint32_t mCharge = 0;
        std::uint32_t mCost = 0;
        std::uint8_t mFlags = 0; // No-autocalc byte; preserve unknown values.
        std::array<std::uint8_t, 3> mPadding{};
    };

    // TES4 input data, not TES3 enchantment execution or a charge formula.
    // Original subrecords, unknown domains and padding remain available.
    struct Enchantment : RawRecord
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_ENCH4;
        std::optional<EnchantmentData> mData;
        std::vector<SpellEffect> mEffects;
        void load(Reader& reader);
    };
}

#endif
