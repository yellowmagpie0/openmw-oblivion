#include "nativeidlerequest.hpp"

#include <stdexcept>

namespace ESM4
{
    NativeIdleRequest makeNativeIdleRequest(std::uint64_t identity, std::uint64_t loadedResource)
    {
        if (!identity) throw std::invalid_argument("null native idle request identity");
        return {identity, loadedResource, 0, loadedResource ? 1u : 0u, false};
    }

    NativeIdleRequest acceptNativeIdleRequestResource(const NativeIdleRequest& request, std::uint64_t resource)
    {
        if (request.mStatus != 0)
            throw std::invalid_argument("native idle resource callback requires pending request");
        auto next = request;
        next.mResource = resource;
        if (resource) next.mStatus = 1;
        return next;
    }

    namespace
    {
        void releaseRequest(NativeIdleRequestSlotChange& next, std::optional<NativeIdleRequest>& slot,
            std::uint64_t& resident, bool immediate)
        {
            if (!slot) return;
            const auto old = *slot;
            slot.reset();
            if (!immediate)
            {
                if (old.mSequence && old.mSequence == resident)
                {
                    next.mEffects.push_back({NativeIdleRequestEffectKind::RetireSequence, old});
                    resident = 0;
                }
                for (auto& retired : next.mSlots.mRetired)
                    if (!retired) { retired = old; return; }
            }
            next.mEffects.push_back({NativeIdleRequestEffectKind::DestroyRequest, old});
        }
    }

    NativeIdleRequestSlotChange replaceNativeIdleRequest(const NativeIdleRequestSlots& slots,
        std::uint64_t residentSlot0, std::optional<NativeIdleRequest> replacement)
    {
        NativeIdleRequestSlotChange next{slots, {}};
        next.mEffects.reserve(4);
        releaseRequest(next, next.mSlots.mPrimary, residentSlot0, false);
        next.mSlots.mPrimary = replacement;
        return next;
    }

    NativeIdleRequestSlotChange clearNativeIdleRequest(const NativeIdleRequestSlots& slots,
        std::uint64_t residentSlot0, bool clearSecondary, bool immediate)
    {
        NativeIdleRequestSlotChange next{slots, {}};
        next.mEffects.reserve(4);
        releaseRequest(next, next.mSlots.mPrimary, residentSlot0, immediate);
        if (clearSecondary)
            releaseRequest(next, next.mSlots.mSecondary, residentSlot0, immediate);
        else
        {
            next.mSlots.mPrimary = next.mSlots.mSecondary;
            next.mSlots.mSecondary.reset();
        }
        return next;
    }

    NativeIdleRequestStart startNativeIdleRequest(const NativeIdleRequest& request,
        bool registered, std::uint64_t selectedSequence, std::int16_t action, std::uint64_t actionSequence)
    {
        NativeIdleRequestStart next{request, action, actionSequence, false};
        if (request.mStatus != 1) return next;
        next.mRequest.mStatus = 3;
        if (registered && selectedSequence)
        {
            next.mRequest.mStatus = 2;
            next.mRequest.mSequence = selectedSequence;
            next.mRequest.mActorBound = true;
            next.mAction = 11;
            next.mActionSequence = selectedSequence;
            next.mStarted = true;
        }
        return next;
    }

    NativeIdleRequestCompletion completeNativeIdleRequest(const NativeIdleRequest& request,
        std::optional<std::uint32_t> sequenceState, std::int16_t action)
    {
        NativeIdleRequestCompletion next{request, action, false};
        if (request.mStatus != 2) return next;
        if (request.mSequence && !sequenceState)
            throw std::invalid_argument("native idle completion has unresolved sequence");
        if (!request.mSequence || *sequenceState == 0)
        {
            next.mRequest.mStatus = 3;
            next.mCompleted = true;
            if (request.mActorBound && action == 11) next.mAction = -1;
        }
        return next;
    }

    bool nativeIdleRequestReady(const NativeIdleRequestSlots& slots) noexcept
    {
        return slots.mPrimary && slots.mPrimary->mStatus == 1;
    }

    bool nativeIdleRequestDone(const NativeIdleRequestSlots& slots) noexcept
    {
        return !slots.mSecondary && (!slots.mPrimary
            || (slots.mPrimary->mSequence && slots.mPrimary->mStatus == 3));
    }

    NativeIdleRequestDestruction destroyNativeIdleRequest(const NativeIdleRequest& request,
        std::optional<std::uint16_t> resourceGroup, std::optional<std::uint64_t> cachedSequence,
        std::int16_t action, std::uint64_t actionSequence)
    {
        const bool matchingCache = resourceGroup && cachedSequence && *cachedSequence == request.mSequence;
        NativeIdleRequestDestruction next;
        next.mDetachSequence = request.mSequence;
        next.mEraseCacheGroup = matchingCache || !request.mSequence;
        next.mCacheGroup = resourceGroup.value_or(0xff);
        const bool clear = request.mActorBound && action == 11 && actionSequence == request.mSequence;
        next.mAction = clear ? -1 : action;
        next.mActionSequence = clear ? 0 : actionSequence;
        return next;
    }
}
