#ifndef OPENMW_MWMECHANICS_OBLIVIONCOMBAT_H
#define OPENMW_MWMECHANICS_OBLIVIONCOMBAT_H

#include <components/esm4/actionledger.hpp>

namespace ESM4
{
    struct RuntimeState;
}

namespace MWMechanics
{
    // Profile-owned native physical action authority. Actor state and contact
    // transitions are integrated separately; issuing an ID is not a hit.
    class OblivionCombatService
    {
        ESM4::ActionLedger mActions;

    public:
        void clear();
        std::uint64_t allocateAction();
        bool isActionPending(std::uint64_t id) const;
        bool isActionConsumed(std::uint64_t id) const;
        // Internal completion/cancellation boundary. Call only with the
        // corresponding gameplay transition committed, before event callbacks.
        bool consumeAction(std::uint64_t id);
        void capture(ESM4::RuntimeState& state) const;
        void restore(const ESM4::RuntimeState& state);
    };
}

#endif
