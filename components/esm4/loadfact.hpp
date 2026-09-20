#ifndef OPENMW_COMPONENTS_ESM4_LOADFACT_H
#define OPENMW_COMPONENTS_ESM4_LOADFACT_H

#include <optional>
#include <components/esm/defs.hpp>
#include "loadrawrecord.hpp"

namespace ESM4
{
    enum class FactionFlag : std::uint8_t { Hidden = 1, Evil = 2, SpecialCombat = 4 };
    struct FactionRelationship
    {
        ESM::FormKey mFaction;
        std::int32_t mModifier;
    };
    // Keep ranks and unknown fields losslessly; absence of CNAM is explicit.
    struct Faction : RawRecord
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_FACT4;
        std::optional<std::uint8_t> mFactionFlags;
        std::optional<float> mCrimeMultiplier;
        std::vector<FactionRelationship> mRelationships;
        bool has(FactionFlag flag) const
        { return mFactionFlags && (*mFactionFlags & static_cast<std::uint8_t>(flag)) != 0; }
        void load(Reader& reader);
    };
}
#endif
