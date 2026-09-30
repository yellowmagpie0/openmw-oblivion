#include "loadmgef.hpp"

#include "common.hpp"
#include "reader.hpp"

#include <algorithm>
#include <bit>
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

        std::uint16_t word(std::span<const std::uint8_t> bytes, std::size_t offset)
        {
            return std::uint16_t(bytes[offset]) | (std::uint16_t(bytes[offset + 1]) << 8);
        }
    }

    void EffectSetting::load(Reader& reader)
    {
        if (reader.hasFormVersion() || (reader.esmVersionF() != .8f && reader.esmVersionF() != 1.f))
            reader.fail("MGEF semantic decoder supports TES4 only");
        *this = {};
        RawRecord::load(reader);
        for (const auto& sub : mSubRecords)
        {
            const auto& bytes = sub.mData;
            if (sub.mType == ESM::fourCC("EDID"))
            {
                if (mEffectCode || (bytes.size() != 4 && bytes.size() != 5)
                    || (bytes.size() == 5 && bytes[4] != 0)
                    || std::find(bytes.begin(), bytes.begin() + 4, std::uint8_t{0}) != bytes.begin() + 4)
                    reader.fail("MGEF requires one four-character EDID");
                mEffectCode = integer(bytes);
            }
            else if (sub.mType == ESM::fourCC("DATA"))
            {
                if (mData || bytes.size() < 24 || bytes.size() > 68 || bytes.size() % 4 != 0)
                    reader.fail("MGEF requires one 24-68 byte DATA in four-byte increments");
                EffectSettingData data{integer(bytes), std::bit_cast<float>(integer(bytes, 4)), integer(bytes, 8),
                    integer(bytes, 12), std::bit_cast<std::int32_t>(integer(bytes, 16)), word(bytes, 20), word(bytes, 22), {}};
                if (data.mFlags & ((1u << 16) | (1u << 17) | (1u << 18)))
                    data.mAssociatedForm = reader.resolveRawFormId(ESM::FormId::fromUint32(data.mAssociatedData));
                mData = std::move(data);
            }
        }
        if (!(mFlags & Rec_Deleted) && (!mEffectCode || !mData))
            reader.fail("MGEF is missing required EDID or DATA");
    }
}
