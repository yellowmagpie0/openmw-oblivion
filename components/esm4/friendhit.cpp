#include "friendhit.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ESM4
{
    FriendHitCounters::FriendHitCounters(bool present, std::vector<FriendHitEntry> entries)
        : mPresent(present)
        , mEntries(std::move(entries))
    {
        if (!present && !mEntries.empty())
            throw std::invalid_argument("absent native friend-hit owner has entries");
        for (const auto& entry : mEntries)
            if (!std::isfinite(entry.mElapsed))
                throw std::invalid_argument("nonfinite native friend-hit timer");
    }

    std::uint16_t FriendHitCounters::count(const ESM::FormKey& target) const
    {
        const auto found = std::ranges::find(mEntries, target, &FriendHitEntry::mTarget);
        return found == mEntries.end() ? 0 : found->mCount;
    }

    void FriendHitCounters::increment(const ESM::FormKey& target)
    {
        const auto found = std::ranges::find(mEntries, target, &FriendHitEntry::mTarget);
        if (found != mEntries.end())
            found->mCount = static_cast<std::uint16_t>(found->mCount + 1);
        else
            mEntries.insert(mEntries.begin(), FriendHitEntry{target, 1, 0});
        mPresent = true;
    }

    void FriendHitCounters::remove(const ESM::FormKey& target)
    {
        const auto found = std::ranges::find(mEntries, target, &FriendHitEntry::mTarget);
        if (found != mEntries.end())
            mEntries.erase(found);
    }

    void FriendHitCounters::advance(float duration, float threshold)
    {
        if (!std::isfinite(duration) || duration < 0 || !std::isfinite(threshold))
            throw std::invalid_argument("invalid native friend-hit update");
        auto next = mEntries;
        for (auto& entry : next)
        {
            const double elapsed = static_cast<double>(entry.mElapsed) + duration;
            if (std::abs(elapsed) > std::numeric_limits<float>::max())
                throw std::overflow_error("native friend-hit timer overflow");
            entry.mElapsed = static_cast<float>(elapsed);
        }
        // 42AFF0 resets its iterator to the head after any removal, then
        // follows head.next. In particular it does not recheck a promoted head.
        for (std::size_t i = 0; i < next.size();)
        {
            if (next[i].mElapsed > threshold)
            {
                next.erase(next.begin() + i);
                i = 1;
            }
            else
                ++i;
        }
        mEntries.swap(next);
    }
}
