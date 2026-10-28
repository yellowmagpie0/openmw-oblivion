#include "loadspel.hpp"

#include "common.hpp"
#include "reader.hpp"

#include <algorithm>
#include <span>

namespace ESM4
{
    namespace
    {
        std::uint32_t integer(std::span<const std::uint8_t> bytes, std::size_t offset = 0)
        {
            return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
                | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
        }

        std::string text(const std::vector<std::uint8_t>& bytes)
        {
            return {bytes.begin(), std::find(bytes.begin(), bytes.end(), std::uint8_t{0})};
        }
    }

    void decodeMagicItemEffects(Reader& reader, RawRecord& record, std::vector<SpellEffect>& effects)
    {
        // FULL after SCIT is the effect name, not the record's display name.
        record.mFullName.clear();
        std::optional<std::uint32_t> pending;
        for (const auto& sub : record.mSubRecords)
        {
            const auto& bytes = sub.mData;
            if (sub.mType == ESM::fourCC("EFID"))
            {
                if (pending || bytes.size() != 4)
                    reader.fail("Magic item EFID requires four bytes and a paired EFIT");
                pending = integer(bytes);
            }
            else if (sub.mType == ESM::fourCC("EFIT"))
            {
                if (!pending || bytes.size() != 24 || integer(bytes) != *pending)
                    reader.fail("Magic item EFIT requires 24 bytes matching the preceding EFID");
                effects.push_back({*pending, integer(bytes, 4), integer(bytes, 8), integer(bytes, 12),
                    integer(bytes, 16), integer(bytes, 20), {}});
                pending.reset();
            }
            else if (sub.mType == ESM::fourCC("SCIT"))
            {
                if (pending || effects.empty() || effects.back().mScriptEffect
                    || (bytes.size() != 4 && bytes.size() != 12 && bytes.size() != 16))
                    reader.fail("Magic item SCIT requires one 4-, 12- or 16-byte script effect after EFIT");
                SpellScriptEffect script;
                script.mScript = reader.resolveRawFormId(ESM::FormId::fromUint32(integer(bytes)));
                if (bytes.size() >= 12)
                {
                    script.mSchool = integer(bytes, 4);
                    script.mVisualEffect = integer(bytes, 8);
                }
                if (bytes.size() == 16)
                {
                    script.mFlags = bytes[12];
                    script.mPadding = {bytes[13], bytes[14], bytes[15]};
                }
                effects.back().mScriptEffect = std::move(script);
            }
            else if (sub.mType == ESM::fourCC("FULL"))
            {
                if (effects.empty() && !pending)
                    record.mFullName = text(bytes);
                else if (!pending && !effects.empty() && effects.back().mScriptEffect)
                {
                    auto& name = effects.back().mScriptEffect->mName;
                    if (name)
                        reader.fail("Magic item script effect has duplicate FULL");
                    name = text(bytes);
                }
            }
        }
        if (pending)
            reader.fail("Magic item EFID has no EFIT");
    }

    void Spell::load(Reader& reader)
    {
        if (reader.hasFormVersion() || (reader.esmVersionF() != .8f && reader.esmVersionF() != 1.f))
            reader.fail("SPEL semantic decoder supports TES4 only");
        *this = {};
        RawRecord::load(reader);
        for (const auto& sub : mSubRecords)
        {
            if (sub.mType != ESM::fourCC("SPIT"))
                continue;
            const auto& bytes = sub.mData;
            if (mData || bytes.size() != 16)
                reader.fail("SPEL requires one 16-byte SPIT");
            mData = SpellData{integer(bytes), integer(bytes, 4), integer(bytes, 8), bytes[12],
                {bytes[13], bytes[14], bytes[15]}};
        }
        if (!(mFlags & Rec_Deleted) && !mData)
            reader.fail("SPEL is missing required SPIT");
        decodeMagicItemEffects(reader, *this, mEffects);
    }
}
