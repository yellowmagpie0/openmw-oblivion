#ifndef OPENMW_ESM4_LOADSPEL_H
#define OPENMW_ESM4_LOADSPEL_H

#include "loadrawrecord.hpp"
#include <components/esm/defs.hpp>
#include <array>
#include <optional>

namespace ESM4
{
    struct SpellData
    {
        std::uint32_t mType = 0;
        std::uint32_t mCost = 0;
        std::uint32_t mLevel = 0;
        std::uint8_t mFlags = 0;
        std::array<std::uint8_t, 3> mPadding{};
    };

    struct SpellScriptEffect
    {
        ESM::FormKey mScript;
        std::optional<std::uint32_t> mSchool;
        std::optional<std::uint32_t> mVisualEffect;
        std::optional<std::uint8_t> mFlags;
        std::array<std::uint8_t, 3> mPadding{};
        std::optional<std::string> mName;
    };

    struct SpellEffect
    {
        std::uint32_t mId = 0; // FourCC, not a FormId.
        std::uint32_t mMagnitude = 0;
        std::uint32_t mArea = 0;
        std::uint32_t mDuration = 0;
        std::uint32_t mRange = 0;
        std::uint32_t mActorValue = 0;
        std::optional<SpellScriptEffect> mScriptEffect;
    };

    // Typed TES4 inputs only. No spell execution or active-effect semantics.
    // All original subrecords, including unknown fields and padding, survive.
    struct Spell : RawRecord
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_SPEL4;
        std::optional<SpellData> mData;
        std::vector<SpellEffect> mEffects;
        void load(Reader& reader);
    };
}
#endif
