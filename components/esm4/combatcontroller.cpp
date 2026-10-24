#include "combatcontroller.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace ESM4
{
    namespace
    {
        void validate(const CombatControllerTarget& entry)
        {
            if (entry.mTarget.isNull() || !std::isfinite(entry.mValue0C) || !std::isfinite(entry.mValue10))
                throw std::invalid_argument("invalid native combat-controller target");
        }
    }

    float combatControllerDistance(const std::array<float, 3>& source, const std::array<float, 3>& target)
    {
        std::array<float, 3> delta;
        for (std::size_t i = 0; i < delta.size(); ++i)
            if (!std::isfinite(source[i]) || !std::isfinite(target[i]))
                throw std::invalid_argument("nonfinite native combat-controller position");
        for (std::size_t i = 0; i < delta.size(); ++i)
        {
            const double difference = double(target[i]) - source[i];
            if (std::abs(difference) > std::numeric_limits<float>::max())
                return std::numeric_limits<float>::infinity();
            delta[i] = static_cast<float>(difference);
        }
        const double squared = double(delta[0]) * delta[0] + double(delta[1]) * delta[1]
            + double(delta[2]) * delta[2];
        // Native FSTP rounds values just above FLT_MAX back to FLT_MAX until
        // the midpoint to 2^128. Comparing only against FLT_MAX loses that case.
        if (squared >= 0x1.ffffffp127)
            return std::numeric_limits<float>::infinity();
        const float stored = squared > std::numeric_limits<float>::max()
            ? std::numeric_limits<float>::max() : static_cast<float>(squared);
        return static_cast<float>(std::sqrt(double(stored)));
    }

    bool admitsCombatControllerTarget(const ESM::FormKey& owner, const ESM::FormKey& target,
        const ESM::FormKey& targetCommandSource, std::int32_t speed, float naturalReach, float distance)
    {
        if (owner.isNull())
            throw std::invalid_argument("null native combat-controller owner");
        if (target.isNull() || target == owner || targetCommandSource == owner)
            return false;
        if (speed != 0)
            return true;
        if (!std::isfinite(naturalReach) || !std::isfinite(distance) || distance < 0)
            throw std::invalid_argument("invalid native combat-controller geometry");
        // 6161F4 stores the quarter-reach as binary32. The subsequent sum
        // is compared without a final binary32 store, including at equality.
        const float extension = static_cast<float>(static_cast<double>(naturalReach) * .25);
        return static_cast<double>(distance) <= static_cast<double>(naturalReach) + extension;
    }

    CombatControllerTargets::CombatControllerTargets(std::vector<CombatControllerTarget> entries)
        : mEntries(std::move(entries))
    {
        std::set<ESM::FormKey> seen;
        for (std::size_t i = 0; i < mEntries.size(); ++i)
        {
            validate(mEntries[i]);
            if (!seen.insert(mEntries[i].mTarget).second
                || (i != 0 && mEntries[i - 1].mPriority < mEntries[i].mPriority))
                throw std::invalid_argument("invalid native combat-controller target order");
        }
    }

    bool CombatControllerTargets::contains(const ESM::FormKey& target) const
    {
        return std::ranges::find(mEntries, target, &CombatControllerTarget::mTarget) != mEntries.end();
    }

    ESM::FormKey CombatControllerTargets::first() const
    {
        return mEntries.empty() ? ESM::FormKey{} : mEntries.front().mTarget;
    }

    bool CombatControllerTargets::add(const CombatControllerTarget& entry)
    {
        if (entry.mTarget.isNull() || contains(entry.mTarget))
            return false;
        validate(entry);
        const auto position = std::ranges::find_if(mEntries,
            [&](const CombatControllerTarget& other) { return other.mPriority <= entry.mPriority; });
        mEntries.insert(position, entry);
        return true;
    }

    bool CombatControllerTargets::remove(const ESM::FormKey& target)
    {
        const auto found = std::ranges::find(mEntries, target, &CombatControllerTarget::mTarget);
        if (found == mEntries.end())
            return false;
        mEntries.erase(found);
        return true;
    }

    std::optional<std::int32_t> CombatControllerTargets::nextScriptPriority() const
    {
        if (mEntries.empty())
            return std::nullopt;
        // 5147BE adds 20 with native int32 wrapping, including at INT_MAX.
        return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(mEntries.front().mPriority) + 20u);
    }

    bool CombatControllerTargets::applyScriptPriority(const ESM::FormKey& target, std::int32_t priority)
    {
        const auto found = std::ranges::find(mEntries, target, &CombatControllerTarget::mTarget);
        if (found == mEntries.end())
            return false;
        if (mEntries.size() == 1)
        {
            // 51484E..51486F: any nonzero marker skips the single-target
            // rewrite. Zero becomes 1 even if the priority itself is unchanged.
            if (found->mMarker != 0)
                return false;
            found->mPriority = priority;
            found->mMarker = 1;
            return true;
        }
        if (found->mPriority == priority)
            return false;
        auto next = mEntries;
        next[static_cast<std::size_t>(found - mEntries.begin())].mPriority = priority;
        // 5B27A0/506F00 sorts a pointer array using gaps 1,4,13,... and a
        // strictly negative signed comparison. Larger gaps can change equal
        // priorities' relative order; stable_sort would produce different state.
        std::size_t gap = 1;
        while (gap <= (next.size() - 1) / 9)
            gap = gap * 3 + 1;
        for (; gap != 0; gap /= 3)
            for (std::size_t i = gap; i < next.size(); ++i)
            {
                auto entry = std::move(next[i]);
                std::size_t position = i;
                while (position >= gap && entry.mPriority > next[position - gap].mPriority)
                {
                    next[position] = std::move(next[position - gap]);
                    position -= gap;
                }
                next[position] = std::move(entry);
            }
        mEntries.swap(next);
        return true;
    }
}
