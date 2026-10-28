#include "loadench.hpp"

#include "common.hpp"
#include "reader.hpp"

namespace ESM4
{
    void Enchantment::load(Reader& reader)
    {
        if (reader.hasFormVersion() || (reader.esmVersionF() != .8f && reader.esmVersionF() != 1.f))
            reader.fail("ENCH semantic decoder supports TES4 only");
        *this = {};
        RawRecord::load(reader);
        for (const auto& sub : mSubRecords)
        {
            if (sub.mType != ESM::fourCC("ENIT"))
                continue;
            const auto& bytes = sub.mData;
            if (mData || bytes.size() != 16)
                reader.fail("ENCH requires one 16-byte ENIT");
            const auto integer = [&](std::size_t offset) {
                return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
                    | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
            };
            mData = EnchantmentData{integer(0), integer(4), integer(8), bytes[12],
                {bytes[13], bytes[14], bytes[15]}};
        }
        if (!(mFlags & Rec_Deleted) && !mData)
            reader.fail("ENCH is missing required ENIT");
        decodeMagicItemEffects(reader, *this, mEffects);
    }
}
