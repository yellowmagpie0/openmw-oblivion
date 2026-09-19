#include "loadcsty.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

#include "common.hpp"
#include "reader.hpp"

namespace ESM4
{
    namespace
    {
        std::uint32_t integer(std::span<const std::uint8_t> bytes, std::size_t offset)
        {
            return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
                | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
        }

        float number(std::span<const std::uint8_t> bytes, std::size_t offset, bool nonnegative = true)
        {
            const float value = std::bit_cast<float>(integer(bytes, offset));
            if (!std::isfinite(value) || (nonnegative && value < 0))
                throw std::runtime_error("CSTY invalid numeric field at byte " + std::to_string(offset));
            return value;
        }

        std::uint8_t percent(std::span<const std::uint8_t> bytes, std::size_t offset)
        {
            if (bytes[offset] > 100)
                throw std::runtime_error("CSTY percentage exceeds 100 at byte " + std::to_string(offset));
            return bytes[offset];
        }

        CombatStyleTimers timers(std::span<const std::uint8_t> bytes, std::size_t offset)
        {
            CombatStyleTimers result{ number(bytes, offset), number(bytes, offset + 4) };
            if (result.mMinimum > result.mMaximum)
                throw std::runtime_error("CSTY minimum timer exceeds maximum");
            return result;
        }
    }

    CombatStyleStandard decodeCombatStyleStandard(std::span<const std::uint8_t> bytes)
    {
        constexpr std::array<std::size_t, 6> sizes{ 84, 92, 104, 112, 120, 124 };
        if (std::find(sizes.begin(), sizes.end(), bytes.size()) == sizes.end())
            throw std::runtime_error("CSTY CSTD has invalid TES4 size " + std::to_string(bytes.size()));
        CombatStyleStandard value;
        value.mDodgeChance = percent(bytes, 0);
        value.mLeftRightChance = percent(bytes, 1);
        value.mDodgeLeftRight = timers(bytes, 4);
        value.mDodgeForward = timers(bytes, 12);
        value.mDodgeBack = timers(bytes, 20);
        value.mIdle = timers(bytes, 28);
        value.mBlockChance = percent(bytes, 36);
        value.mAttackChance = percent(bytes, 37);
        value.mAttackRecoilBonus = number(bytes, 40, false);
        value.mAttackUnconsciousBonus = number(bytes, 44, false);
        value.mAttackUnarmedBonus = number(bytes, 48, false);
        value.mPowerAttackChance = percent(bytes, 52);
        value.mPowerAttackRecoilBonus = number(bytes, 56, false);
        value.mPowerAttackUnconsciousBonus = number(bytes, 60, false);
        for (std::size_t i = 0; i < value.mPowerAttackDirections.size(); ++i)
            value.mPowerAttackDirections[i] = percent(bytes, 64 + i);
        value.mHold = timers(bytes, 72);
        value.mFlags = bytes[80]; // All eight TES4 bits are defined.
        value.mAcrobaticDodgeChance = percent(bytes, 81);
        if (bytes.size() >= 92)
            value.mRangeMultipliers = { number(bytes, 84), number(bytes, 88) };
        if (bytes.size() >= 104)
        {
            value.mSwitchDistances = { number(bytes, 92), number(bytes, 96) };
            value.mBuffStandoff = number(bytes, 100);
        }
        if (bytes.size() >= 112)
            value.mRangedGroupStandoff = { number(bytes, 104), number(bytes, 108) };
        if (bytes.size() >= 120)
        {
            value.mRushChance = percent(bytes, 112);
            value.mRushDistanceMultiplier = number(bytes, 116);
        }
        if (bytes.size() == 124)
        {
            const auto flag = integer(bytes, 120);
            if (flag > 1)
                throw std::runtime_error("CSTY Do Not Acquire has unknown flag bits");
            value.mDoNotAcquire = flag != 0;
        }
        return value;
    }

    CombatStyleAdvanced decodeCombatStyleAdvanced(std::span<const std::uint8_t> bytes)
    {
        if (bytes.size() != 84)
            throw std::runtime_error("CSTY CSAD must contain exactly 21 floats");
        // Named aggregate fields follow the TES4 layout; signed fatigue and
        // speed bases/multipliers are intentional, unlike timers/distances.
        return { number(bytes, 0, false), number(bytes, 4, false), number(bytes, 8, false),
            number(bytes, 12, false), number(bytes, 16, false), number(bytes, 20, false),
            number(bytes, 24, false), number(bytes, 28, false), number(bytes, 32, false),
            number(bytes, 36, false), number(bytes, 40, false), number(bytes, 44, false),
            number(bytes, 48, false), number(bytes, 52, false), number(bytes, 56, false),
            number(bytes, 60, false), number(bytes, 64, false), number(bytes, 68, false),
            number(bytes, 72, false), number(bytes, 76, false), number(bytes, 80, false) };
    }

    void CombatStyle::load(Reader& reader)
    {
        if (reader.hasFormVersion() || (reader.esmVersionF() != 0.8f && reader.esmVersionF() != 1.f))
            reader.fail("CSTY semantic decoder supports TES4 only");
        *this = {};
        RawRecord::load(reader);
        for (const auto& sub : mSubRecords)
        {
            if (sub.mType == ESM::fourCC("CSTD"))
            {
                if (mStandard)
                    reader.fail("CSTY has duplicate CSTD");
                mStandard = decodeCombatStyleStandard(sub.mData);
            }
            else if (sub.mType == ESM::fourCC("CSAD"))
            {
                if (mAdvanced)
                    reader.fail("CSTY has duplicate CSAD");
                mAdvanced = decodeCombatStyleAdvanced(sub.mData);
            }
        }
        if (!mStandard && !(mFlags & Rec_Deleted))
            reader.fail("CSTY is missing required CSTD");
    }
}
