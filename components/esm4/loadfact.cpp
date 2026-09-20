#include "loadfact.hpp"

#include <bit>
#include <cmath>
#include <stdexcept>

#include "common.hpp"
#include "reader.hpp"

namespace ESM4
{
    namespace
    {
        std::uint32_t integer(const std::vector<std::uint8_t>& bytes, std::size_t offset = 0)
        {
            return std::uint32_t(bytes.at(offset)) | (std::uint32_t(bytes.at(offset + 1)) << 8)
                | (std::uint32_t(bytes.at(offset + 2)) << 16) | (std::uint32_t(bytes.at(offset + 3)) << 24);
        }
    }

    void Faction::load(Reader& reader)
    {
        if (reader.hasFormVersion() || (reader.esmVersionF() != 0.8f && reader.esmVersionF() != 1.f))
            reader.fail("FACT semantic decoder supports TES4 only");
        *this = {};
        RawRecord::load(reader);
        for (const auto& sub : mSubRecords)
        {
            switch (sub.mType)
            {
                case ESM::fourCC("DATA"):
                    if (mFactionFlags || sub.mData.size() != 1 || (sub.mData[0] & ~7u))
                        reader.fail("FACT invalid or duplicate DATA flags");
                    mFactionFlags = sub.mData[0];
                    break;
                case ESM::fourCC("CNAM"):
                {
                    if (mCrimeMultiplier || sub.mData.size() != 4)
                        reader.fail("FACT invalid or duplicate CNAM");
                    const float value = std::bit_cast<float>(integer(sub.mData));
                    if (!std::isfinite(value) || value < 0)
                        reader.fail("FACT invalid crime multiplier");
                    mCrimeMultiplier = value;
                    break;
                }
                case ESM::fourCC("XNAM"):
                    if (sub.mData.size() != 8)
                        reader.fail("FACT XNAM must contain a faction and signed reaction");
                    mRelationships.push_back({reader.resolveRawFormId(ESM::FormId::fromUint32(integer(sub.mData))),
                        std::bit_cast<std::int32_t>(integer(sub.mData, 4))});
                    break;
            }
        }
        if (!mFactionFlags && !(mFlags & Rec_Deleted))
            reader.fail("FACT is missing required DATA");
    }
}
