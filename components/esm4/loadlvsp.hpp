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
        void load(Reader& reader);
    };

    struct SingleLevelledSpellSelection
    {
        std::optional<ESM::FormKey> mSpell;
        unsigned mDraws = 0;
    };
    // The stock Dog has one direct Disease1 child, count1, flags3. This
    // bounded selector retains original per-school chance draws, the skill
    // threshold and the selection draw even with only one eligible child.
    // Nested/multiple/use-all sources need their own reviewed selection path.
    SingleLevelledSpellSelection selectSingleLevelledSpell(const LevelledSpell& source,
        std::uint32_t childSchool, const std::array<std::int32_t, 6>& magicSkills,
        std::span<const std::uint16_t> randomWords);
}
#endif
