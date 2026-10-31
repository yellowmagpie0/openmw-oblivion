#ifndef OPENMW_MWMECHANICS_OBLIVIONDOORSTATE_H
#define OPENMW_MWMECHANICS_OBLIVIONDOORSTATE_H

#include <algorithm>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include <components/esm/refid.hpp>
#include <components/esm4/runtimestate.hpp>

#include "../mwworld/doorstate.hpp"

namespace MWMechanics
{
    // A read-only view for one low-process pass, before package callbacks run.
    // Index identities, not values: in-place state edits remain authoritative.
    // Insertion, removal, reordering or key edits require a new view.
    class OblivionSavedReferenceIndex
    {
        std::span<const ESM4::RuntimeReferenceState> mSource;
        std::vector<const ESM4::RuntimeReferenceState*> mIndex;

    public:
        explicit OblivionSavedReferenceIndex(std::span<const ESM4::RuntimeReferenceState> references)
            : mSource(references)
        {
            mIndex.reserve(references.size());
            for (const auto& reference : references)
                mIndex.push_back(&reference);
            const auto less = [](const auto* left, const auto* right) { return left->mKey < right->mKey; };
            if (!std::is_sorted(mIndex.begin(), mIndex.end(), less))
                std::stable_sort(mIndex.begin(), mIndex.end(), less);
        }

        bool matches(std::span<const ESM4::RuntimeReferenceState> references) const
        {
            return references.data() == mSource.data() && references.size() == mSource.size();
        }

        const ESM4::RuntimeReferenceState* find(const ESM::FormKey& key) const
        {
            const auto found = std::lower_bound(mIndex.begin(), mIndex.end(), key,
                [](const auto* reference, const ESM::FormKey& wanted) { return reference->mKey < wanted; });
            return found != mIndex.end() && (*found)->mKey == key ? *found : nullptr;
        }
    };

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

    inline int oblivionDoorOpenState(MWWorld::DoorState state, float currentYaw, float closedYaw)
    {
        switch (state)
        {
            case MWWorld::DoorState::Opening:
                return 2;
            case MWWorld::DoorState::Closing:
                return 4;
            case MWWorld::DoorState::Idle:
                return currentYaw == closedYaw ? 3 : 1;
        }
        return 0;
    }

    inline std::optional<MWWorld::DoorState> oblivionDoorTransition(int state)
    {
        if (state == 0)
            return MWWorld::DoorState::Closing;
        if (state == 1)
            return MWWorld::DoorState::Opening;
        return std::nullopt;
    }

    inline bool observesOblivionPhysicalDoorTransition(const ESM4::RuntimeActorAiState& state,
        const ESM::FormKey& initiator, const ESM::FormKey& cell)
    {
        return state.mActor != initiator && state.mTier == ESM4::ProcessTier::High
            && !state.mPackage.isNull() && !cell.isNull() && state.mCell == cell;
    }
}

#endif
