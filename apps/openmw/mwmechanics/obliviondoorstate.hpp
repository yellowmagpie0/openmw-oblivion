#ifndef OPENMW_MWMECHANICS_OBLIVIONDOORSTATE_H
#define OPENMW_MWMECHANICS_OBLIVIONDOORSTATE_H

#include <optional>
#include <stdexcept>

#include <components/esm/refid.hpp>
#include <components/esm4/runtimestate.hpp>

namespace MWMechanics
{
    inline bool isOblivionDoorInterruption(const ESM4::RuntimeActorAiState& state)
    {
        return state.mPhase == ESM4::PackagePhase::Interrupted && !state.mDoor.isNull()
            && (state.mInterruptionReason == "door-unavailable" || state.mInterruptionReason == "door-locked");
    }

    struct OblivionDoorState
    {
        bool mAvailable = true;
        bool mLocked = false;
        ESM::RefId mKey;
        ESM::FormKey mOwner;
    };

    // Resident state is authoritative, then a complete saved reference
    // snapshot, then authored data. In particular, false is a value, not an
    // instruction to fall back to a plugin's enabled/locked/owned state.
    inline OblivionDoorState resolveOblivionDoorState(OblivionDoorState authored,
        const ESM4::RuntimeReferenceState* saved, const std::optional<OblivionDoorState>& resident)
    {
        if (resident)
            return *resident;
        if (saved != nullptr)
        {
            authored.mAvailable = saved->mEnabled && !saved->mDeleted;
            authored.mLocked = saved->mLockLevel > 0;
            if (const auto locked = saved->mCustomState.find("locked"); locked != saved->mCustomState.end())
            {
                const auto* value = std::get_if<bool>(&locked->second);
                if (value == nullptr)
                    throw std::runtime_error("TES4 saved door locked state is not boolean");
                authored.mLocked = *value;
            }
            authored.mOwner = saved->mOwner.value_or(ESM::FormKey{});
        }
        return authored;
    }
}

#endif
