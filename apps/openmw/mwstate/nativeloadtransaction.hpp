#ifndef OPENMW_MWSTATE_NATIVELOADTRANSACTION_H
#define OPENMW_MWSTATE_NATIVELOADTRANSACTION_H

#include <memory>

namespace MWWorld { class World; }

namespace MWState
{
    class StateManager;

    // Owns the complete outgoing native game until installation succeeds.
    // The caller must destroy incoming admission plans before rollback.
    class NativeLoadTransaction
    {
        struct Data;
        std::unique_ptr<Data> mData;
    public:
        NativeLoadTransaction(StateManager& state, MWWorld::World& world);
        ~NativeLoadTransaction();
        NativeLoadTransaction(const NativeLoadTransaction&) = delete;
        void startIncomingScripts();
        void rollback();
        void commit();
    };
}

#endif
