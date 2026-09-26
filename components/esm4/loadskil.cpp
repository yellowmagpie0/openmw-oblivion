#include "loadskil.hpp"

#include "common.hpp"
#include "reader.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace ESM4
{
    namespace
    {
        std::uint32_t integer(std::span<const std::uint8_t> bytes, std::size_t offset)
        {
            return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
                | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
        }
    }

    SkillData decodeSkillData(std::span<const std::uint8_t> bytes)
    {
        if (bytes.size() != 20)
            throw std::runtime_error("TES4 SKIL DATA requires exactly 20 bytes");
        SkillData result{integer(bytes, 0), integer(bytes, 4), integer(bytes, 8),
            {std::bit_cast<float>(integer(bytes, 12)), std::bit_cast<float>(integer(bytes, 16))}};
        if (result.mActorValue < 12 || result.mActorValue > 32 || result.mGoverningAttribute > 7
            || result.mSpecialization > 2 || !std::isfinite(result.mUseValues[0])
            || !std::isfinite(result.mUseValues[1]))
            throw std::runtime_error("unsupported TES4 SKIL definition");
        return result;
    }

    void Skill::load(Reader& reader)
    {
        if (reader.hasFormVersion() || (reader.esmVersionF() != .8f && reader.esmVersionF() != 1.f))
            reader.fail("SKIL semantic decoder supports TES4 only");
        *this = {};
        RawRecord::load(reader);
        for (const auto& sub : mSubRecords)
        {
            if (sub.mType == ESM::fourCC("INDX"))
            {
                if (mIndex || sub.mData.size() != 4)
                    reader.fail("SKIL requires one four-byte INDX");
                mIndex = integer(sub.mData, 0);
                if (*mIndex < 12 || *mIndex > 32)
                    reader.fail("SKIL INDX is not a native skill actor value");
            }
            else if (sub.mType == ESM::fourCC("DATA"))
            {
                if (mData)
                    reader.fail("SKIL has duplicate DATA");
                mData = decodeSkillData(sub.mData);
            }
        }
        if (!(mFlags & Rec_Deleted) && (!mIndex || !mData))
            reader.fail("SKIL is missing required INDX or DATA");
        if (mIndex && mData && *mIndex != mData->mActorValue)
            reader.fail("SKIL INDX disagrees with DATA actor value");
    }

    std::array<const Skill*, 21> resolveSkillDefinitions(std::span<const Skill* const> skills)
    {
        std::array<const Skill*, 21> result{};
        for (const auto* skill : skills)
        {
            if (!skill || (skill->mFlags & Rec_Deleted) || !skill->mIndex || !skill->mData
                || *skill->mIndex < 12 || *skill->mIndex > 32 || *skill->mIndex != skill->mData->mActorValue
                || skill->mData->mGoverningAttribute > 7 || skill->mData->mSpecialization > 2)
                throw std::invalid_argument("invalid winning native skill definition");
            auto& target = result[*skill->mIndex - 12];
            if (target)
                throw std::invalid_argument("ambiguous winning native skill actor value");
            target = skill;
        }
        if (std::ranges::find(result, nullptr) != result.end())
            throw std::invalid_argument("incomplete winning native skill inventory");
        return result;
    }
}
