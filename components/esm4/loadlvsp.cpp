#include "loadlvsp.hpp"

#include "common.hpp"
#include "reader.hpp"

#include <stdexcept>

namespace ESM4
{
    SingleLevelledSpellSelection selectSingleLevelledSpell(const LevelledSpell& source,
        std::uint32_t childSchool, const std::array<std::int32_t, 6>& magicSkills,
        std::span<const std::uint16_t> randomWords)
    {
        if (!source.mChanceNone || *source.mChanceNone > 100 || !source.mListFlags
            || (*source.mListFlags & ~3u) || source.mEntries.size() != 1 || childSchool >= 6
            || source.mEntries[0].mCount != 1 || source.mEntries[0].mSpellKey.isNull())
            throw std::invalid_argument("unsupported single-child native leveled spell source");
        SingleLevelledSpellSelection result;
        const auto draw = [&]() {
            if (result.mDraws >= randomWords.size() || randomWords[result.mDraws] > 32767)
                throw std::invalid_argument("missing or invalid native leveled spell random word");
            return randomWords[result.mDraws++];
        };
        for (unsigned school = 0; school < 6; ++school)
        {
            if (*source.mChanceNone && draw() % 100 < *source.mChanceNone)
                continue;
            if (school != childSchool
                || source.mEntries[0].mLevel > static_cast<std::uint16_t>(magicSkills[school]))
                continue;
            draw(); // The native index draw is retained for an eligible singleton.
            result.mSpell = source.mEntries[0].mSpellKey;
        }
        return result;
    }

    void LevelledSpell::load(Reader& reader)
    {
        if (reader.hasFormVersion() || (reader.esmVersionF() != .8f && reader.esmVersionF() != 1.f))
            reader.fail("LVSP semantic decoder supports TES4 only");
        *this = {};
        RawRecord::load(reader);
        for (const auto& sub : mSubRecords)
        {
            const auto& bytes = sub.mData;
            if (sub.mType == ESM::fourCC("LVLD"))
            {
                if (mChanceNone || bytes.size() != 1 || bytes[0] > 100)
                    reader.fail("LVSP requires one percentage byte LVLD");
                mChanceNone = bytes[0];
            }
            else if (sub.mType == ESM::fourCC("LVLF"))
            {
                if (mListFlags || bytes.size() != 1 || (bytes[0] & ~7u))
                    reader.fail("LVSP requires one known flags byte LVLF");
                mListFlags = bytes[0];
            }
            else if (sub.mType == ESM::fourCC("LVLO"))
            {
                if (bytes.size() != 12)
                    reader.fail("TES4 LVSP entry requires twelve bytes");
                const auto word = [&](std::size_t offset) {
                    return static_cast<std::uint16_t>(bytes[offset] | (std::uint16_t(bytes[offset + 1]) << 8));
                };
                const auto raw = std::uint32_t(bytes[4]) | (std::uint32_t(bytes[5]) << 8)
                    | (std::uint32_t(bytes[6]) << 16) | (std::uint32_t(bytes[7]) << 24);
                auto id = ESM::FormId::fromUint32(raw);
                const auto key = reader.resolveRawFormId(id);
                reader.adjustFormId(id);
                mEntries.push_back({word(0), id, key, word(8)});
            }
        }
        if (!(mFlags & Rec_Deleted) && (!mChanceNone || !mListFlags))
            reader.fail("LVSP is missing LVLD or LVLF");
    }
}
