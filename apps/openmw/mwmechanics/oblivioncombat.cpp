#include "oblivioncombat.hpp"

#include <components/esm4/runtimestate.hpp>

#include <stdexcept>

namespace MWMechanics
{
    void OblivionCombatService::clear()
    {
        mActions = {};
    }

    std::uint64_t OblivionCombatService::allocateAction()
    {
        return mActions.allocate();
    }

    bool OblivionCombatService::isActionPending(std::uint64_t id) const
    {
        return mActions.isPending(id);
    }

    bool OblivionCombatService::isActionConsumed(std::uint64_t id) const
    {
        return mActions.isConsumed(id);
    }

    bool OblivionCombatService::consumeAction(std::uint64_t id)
    {
        return mActions.consume(id);
    }

    void OblivionCombatService::capture(ESM4::RuntimeState& state) const
    {
        if (state.mProfile != ESM::GameProfile::Oblivion || state.mVersion < 8
            || state.mVersion > ESM4::CurrentRuntimeStateVersion)
            throw std::invalid_argument("native physical actions require an Oblivion v8+ save");
        state.mPhysicalActions = mActions.capture();
    }

    void OblivionCombatService::restore(const ESM4::RuntimeState& state)
    {
        state.validate();
        mActions.restore(state.mPhysicalActions);
    }
}
