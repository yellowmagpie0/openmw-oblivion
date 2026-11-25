#ifndef OPENMW_ESM4_LOADLVSP_H
#define OPENMW_ESM4_LOADLVSP_H

#include "loadrawrecord.hpp"

#include <components/esm/defs.hpp>
#include <array>
#include <optional>
#include <span>

namespace ESM4
{
    struct LevelledSpellEntry
    {
        std::uint16_t mLevel = 0;
        ESM::FormId mSpell;
        ESM::FormKey mSpellKey;
        std::uint16_t mCount = 0;
    };

    // TES4 input data, including lossless padding and unknown fields. Selection
    // uses the actor's magic skills and the spell's school, not actor level.
    struct LevelledSpell : RawRecord
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_LVSP4;
        std::optional<std::uint8_t> mChanceNone;
        std::optional<std::uint8_t> mListFlags;
        std::vector<LevelledSpellEntry> mEntries;
        bool containsSpell(const ESM::FormKey& key) const;
        void load(Reader& reader);
    };

    struct SingleLevelledSpellSelection
    {
        std::optional<ESM::FormKey> mSpell;
        unsigned mDraws = 0;
    };
    // Select one child from a list with a common level and school. Preserve
    // per-school chance draws and the index draw even for a singleton.
    SingleLevelledSpellSelection selectSingleLevelledSpell(const LevelledSpell& source,
        std::uint32_t childSchool, const std::array<std::int32_t, 6>& magicSkills,
        std::span<const std::uint16_t> randomWords);
}
#endif
