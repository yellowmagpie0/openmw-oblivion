#include "actionledger.hpp"

#include <limits>
#include <stdexcept>

namespace ESM4
{
    std::uint64_t ActionLedger::allocate()
    {
        if (mNext == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("native action identity exhausted");
        const auto id = mNext;
        mPending.insert(id);
        ++mNext;
        return id;
    }

    bool ActionLedger::isPending(std::uint64_t id) const
    {
        return mPending.contains(id);
    }

    bool ActionLedger::isConsumed(std::uint64_t id) const
    {
        return id != 0 && id < mNext && !isPending(id);
    }

    bool ActionLedger::consume(std::uint64_t id)
    {
        return mPending.erase(id) != 0;
    }

    ActionLedgerState ActionLedger::capture() const
    {
        return {mNext, {mPending.begin(), mPending.end()}};
    }

    void ActionLedger::restore(const ActionLedgerState& state)
    {
        if (state.mNext == 0)
            throw std::invalid_argument("zero native next action identity");
        std::set<std::uint64_t> pending;
        for (const auto id : state.mPending)
            if (id == 0 || id >= state.mNext || !pending.insert(id).second)
                throw std::invalid_argument("invalid or duplicate native pending action identity");
        mPending.swap(pending);
        mNext = state.mNext;
    }
}
