#ifndef OPENMW_ESM4_ACTIONLEDGER_H
#define OPENMW_ESM4_ACTIONLEDGER_H

#include <cstdint>
#include <set>
#include <vector>

namespace ESM4
{
    struct ActionLedgerState
    {
        std::uint64_t mNext = 1;
        std::vector<std::uint64_t> mPending;

        friend bool operator==(const ActionLedgerState&, const ActionLedgerState&) = default;
    };

    // Every issued action is either pending or consumed. Keeping only pending
    // IDs permits out-of-order completion without retaining completed history.
    // The service must commit gameplay changes and consumption together before
    // callbacks or saves; this class itself does not perform those transitions.
    class ActionLedger
    {
        std::uint64_t mNext = 1;
        std::set<std::uint64_t> mPending;

    public:
        std::uint64_t allocate();
        bool isPending(std::uint64_t id) const;
        bool isConsumed(std::uint64_t id) const;
        // Cancellation also consumes an action. False means unissued or replay.
        bool consume(std::uint64_t id);
        ActionLedgerState capture() const;
        // Validates a temporary replacement before changing live state.
        void restore(const ActionLedgerState& state);
    };
}

#endif
