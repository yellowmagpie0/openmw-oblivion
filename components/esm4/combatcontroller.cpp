#include "combatcontroller.hpp"

#include <algorithm>
#include <cmath>
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
}
