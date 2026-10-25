#ifndef OPENMW_COMPONENTS_ESM4_NATIVEIDLEREQUEST_H
#define OPENMW_COMPONENTS_ESM4_NATIVEIDLEREQUEST_H

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace ESM4
{
    // Policy1/logical-slot5 request protocol. Identities are caller-owned
    // references to requests/resources/sequences, not combat contact IDs.
    // Zero is a null native pointer; distinct requests may share a sequence.
    struct NativeIdleRequest
    {
        std::uint64_t mIdentity = 0;
        std::uint64_t mResource = 0;
        std::uint64_t mSequence = 0;
        std::uint32_t mStatus = 0;
        bool mActorBound = false;
        friend bool operator==(const NativeIdleRequest&, const NativeIdleRequest&) = default;
    };

    struct NativeIdleRequestSlots
    {
        std::optional<NativeIdleRequest> mPrimary;
        std::optional<NativeIdleRequest> mSecondary;
        std::array<std::optional<NativeIdleRequest>, 2> mRetired;
        friend bool operator==(const NativeIdleRequestSlots&, const NativeIdleRequestSlots&) = default;
    };

    // The caller owns resource acquisition, reference counts and NI playback.
    // These operations publish no live objects or actor state.
    NativeIdleRequest makeNativeIdleRequest(std::uint64_t identity, std::uint64_t loadedResource);
    NativeIdleRequest acceptNativeIdleRequestResource(const NativeIdleRequest& request, std::uint64_t resource);

    enum class NativeIdleRequestEffectKind { RetireSequence, DestroyRequest };
    struct NativeIdleRequestEffect
    {
        NativeIdleRequestEffectKind mKind;
        NativeIdleRequest mRequest;
        friend bool operator==(const NativeIdleRequestEffect&, const NativeIdleRequestEffect&) = default;
    };
    struct NativeIdleRequestSlotChange
    {
        NativeIdleRequestSlots mSlots;
        // Apply in order to a staged ownership graph before publishing it.
        std::vector<NativeIdleRequestEffect> mEffects;
    };
    // Original475300: retire a matching resident primary; keep the first two
    // old requests in fixed D4/D8 positions. A full pair destroys the outgoing
    // primary, without shifting or destroying the earlier retained requests.
    // Null replacement represents failed request allocation, not pending IO.
    NativeIdleRequestSlotChange replaceNativeIdleRequest(const NativeIdleRequestSlots& slots,
        std::uint64_t residentSlot0, std::optional<NativeIdleRequest> replacement);
    // Original475440. First boolean clears secondary; second forces immediate
    // destruction. Otherwise primary is retired before secondary is promoted.
    NativeIdleRequestSlotChange clearNativeIdleRequest(const NativeIdleRequestSlots& slots,
        std::uint64_t residentSlot0, bool clearSecondary, bool immediate);

    struct NativeIdleRequestStart
    {
        NativeIdleRequest mRequest;
        std::int16_t mAction;
        std::uint64_t mActionSequence;
        bool mStarted;
    };
    // Original476380/477E50 after secondary promotion and actual registration/
    // selection. A failed start changes status1 to3; it does not fabricate a
    // sequence or action11. Caller must execute the native failure cleanup.
    NativeIdleRequestStart startNativeIdleRequest(const NativeIdleRequest& request,
        bool registered, std::uint64_t selectedSequence, std::int16_t action, std::uint64_t actionSequence);

    struct NativeIdleRequestCompletion
    {
        NativeIdleRequest mRequest;
        std::int16_t mAction;
        bool mCompleted;
    };
    // Original477AD0: no sequence or actual NI state0 completes status2.
    // A nonnull sequence requires its resolved state; missing context throws.
    // Policy1 retains the completed request and its sequence reference.
    NativeIdleRequestCompletion completeNativeIdleRequest(const NativeIdleRequest& request,
        std::optional<std::uint32_t> sequenceState, std::int16_t action);
    bool nativeIdleRequestReady(const NativeIdleRequestSlots& slots) noexcept;
    bool nativeIdleRequestDone(const NativeIdleRequestSlots& slots) noexcept;

    struct NativeIdleRequestDestruction
    {
        std::uint64_t mDetachSequence = 0;
        // Resulting cache mutation. Native can repeat a group erasure;
        // the second erase has no entry left to remove.
        bool mEraseCacheGroup = false;
        std::uint16_t mCacheGroup = 0xff;
        std::int16_t mAction = -1;
        std::uint64_t mActionSequence = 0;
    };
    // Original472ED0/4729F0, supplied resolved resource group/cache and process
    // action reference. This differs from completion: destructor action clear
    // additionally requires the process sequence to match this request.
    // Manager detachment precedes request destruction; cache erase follows it.
    NativeIdleRequestDestruction destroyNativeIdleRequest(const NativeIdleRequest& request,
        std::optional<std::uint16_t> resourceGroup, std::optional<std::uint64_t> cachedSequence,
        std::int16_t action, std::uint64_t actionSequence);
}

#endif
