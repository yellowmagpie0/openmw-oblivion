#include <components/esm4/nativeidlerequest.hpp>

#include <gtest/gtest.h>

#include <stdexcept>

namespace
{
    ESM4::NativeIdleRequest attached(std::uint64_t id, std::uint64_t sequence)
    {
        auto request = ESM4::makeNativeIdleRequest(id, 100);
        request.mStatus = 2;
        request.mSequence = sequence;
        request.mActorBound = true;
        return request;
    }
}

TEST(ESM4NativeIdleRequest, LoadedConstructionAndPendingCallbackDoNotInventPlayback)
{
    const auto pending = ESM4::makeNativeIdleRequest(1, 0);
    EXPECT_EQ(pending.mStatus, 0);
    EXPECT_EQ(ESM4::acceptNativeIdleRequestResource(pending, 0), pending);
    const auto ready = ESM4::acceptNativeIdleRequestResource(pending, 100);
    EXPECT_EQ(ready, ESM4::makeNativeIdleRequest(1, 100));
    EXPECT_EQ(ready.mStatus, 1);
    EXPECT_EQ(ready.mSequence, 0);
    EXPECT_FALSE(ready.mActorBound);
    auto bound = pending;
    bound.mActorBound = true;
    EXPECT_TRUE(ESM4::acceptNativeIdleRequestResource(bound, 100).mActorBound);
    EXPECT_THROW(ESM4::makeNativeIdleRequest(0, 100), std::invalid_argument);
    EXPECT_THROW(ESM4::acceptNativeIdleRequestResource(ready, 100), std::invalid_argument);
}

TEST(ESM4NativeIdleRequest, ReplacementKeepsTwoFixedRetiredPositionsAndDestroysOutgoingThird)
{
    ESM4::NativeIdleRequestSlots slots;
    for (std::uint64_t id = 1; id <= 4; ++id)
    {
        const auto next = ESM4::replaceNativeIdleRequest(slots, 7, attached(id, 7));
        ASSERT_TRUE(next.mSlots.mPrimary);
        EXPECT_EQ(next.mSlots.mPrimary->mIdentity, id);
        if (id > 1)
        {
            ASSERT_FALSE(next.mEffects.empty());
            EXPECT_EQ(next.mEffects.front().mKind, ESM4::NativeIdleRequestEffectKind::RetireSequence);
            EXPECT_EQ(next.mEffects.front().mRequest.mIdentity, id - 1);
        }
        if (id == 4)
        {
            ASSERT_EQ(next.mEffects.size(), 2);
            EXPECT_EQ(next.mEffects.back().mKind, ESM4::NativeIdleRequestEffectKind::DestroyRequest);
            EXPECT_EQ(next.mEffects.back().mRequest.mIdentity, 3);
        }
        slots = next.mSlots;
    }
    ASSERT_TRUE(slots.mRetired[0]);
    ASSERT_TRUE(slots.mRetired[1]);
    EXPECT_EQ(slots.mRetired[0]->mIdentity, 1);
    EXPECT_EQ(slots.mRetired[1]->mIdentity, 2);
    const auto failedAllocation = ESM4::replaceNativeIdleRequest(slots, 0, std::nullopt);
    EXPECT_FALSE(failedAllocation.mSlots.mPrimary);
    EXPECT_EQ(failedAllocation.mSlots.mRetired, slots.mRetired);
    ASSERT_EQ(failedAllocation.mEffects.size(), 1);
    EXPECT_EQ(failedAllocation.mEffects.front().mRequest.mIdentity, 4);
}

TEST(ESM4NativeIdleRequest, SecondarySharingPrimarySequenceIsNotRetiredTwiceAfterSlotDetachment)
{
    ESM4::NativeIdleRequestSlots slots;
    slots.mPrimary = attached(1, 7);
    slots.mSecondary = attached(2, 7);
    const auto next = ESM4::clearNativeIdleRequest(slots, 7, true, false);
    EXPECT_FALSE(next.mSlots.mPrimary);
    EXPECT_FALSE(next.mSlots.mSecondary);
    ASSERT_TRUE(next.mSlots.mRetired[0]);
    ASSERT_TRUE(next.mSlots.mRetired[1]);
    EXPECT_EQ(next.mSlots.mRetired[0]->mIdentity, 1);
    EXPECT_EQ(next.mSlots.mRetired[1]->mIdentity, 2);
    ASSERT_EQ(next.mEffects.size(), 1);
    EXPECT_EQ(next.mEffects.front().mRequest.mIdentity, 1);
    EXPECT_EQ(slots.mPrimary->mIdentity, 1);
}

TEST(ESM4NativeIdleRequest, ClearPromotesSecondaryAndImmediateClearBypassesRetiredSlots)
{
    ESM4::NativeIdleRequestSlots slots;
    slots.mPrimary = attached(1, 7);
    slots.mSecondary = ESM4::makeNativeIdleRequest(2, 100);
    const auto promoted = ESM4::clearNativeIdleRequest(slots, 7, false, false);
    EXPECT_EQ(promoted.mSlots.mPrimary, slots.mSecondary);
    EXPECT_FALSE(promoted.mSlots.mSecondary);
    EXPECT_EQ(promoted.mSlots.mRetired[0], slots.mPrimary);
    const auto destroyed = ESM4::clearNativeIdleRequest(slots, 7, true, true);
    EXPECT_FALSE(destroyed.mSlots.mPrimary);
    EXPECT_FALSE(destroyed.mSlots.mSecondary);
    EXPECT_FALSE(destroyed.mSlots.mRetired[0]);
    EXPECT_FALSE(destroyed.mSlots.mRetired[1]);
    ASSERT_EQ(destroyed.mEffects.size(), 2);
    for (const auto& effect : destroyed.mEffects)
        EXPECT_EQ(effect.mKind, ESM4::NativeIdleRequestEffectKind::DestroyRequest);
}

TEST(ESM4NativeIdleRequest, ActionElevenRequiresActualRegisteredSequenceAttachment)
{
    const auto ready = ESM4::makeNativeIdleRequest(1, 100);
    for (bool registered : {false, true})
        for (std::uint64_t sequence : {0u, 7u})
        {
            const auto result = ESM4::startNativeIdleRequest(ready, registered, sequence, 16, 9);
            EXPECT_EQ(result.mStarted, registered && sequence);
            EXPECT_EQ(result.mRequest.mStatus, result.mStarted ? 2 : 3);
            EXPECT_EQ(result.mAction, result.mStarted ? 11 : 16);
            EXPECT_EQ(result.mActionSequence, result.mStarted ? 7 : 9);
            EXPECT_EQ(result.mRequest.mActorBound, result.mStarted);
        }
    const auto pending = ESM4::makeNativeIdleRequest(2, 0);
    EXPECT_EQ(ESM4::startNativeIdleRequest(pending, true, 7, 16, 9).mRequest, pending);
    EXPECT_EQ(ready.mStatus, 1);
}

TEST(ESM4NativeIdleRequest, CompletionRetainsRequestAndClearsOnlyBoundActionEleven)
{
    const auto request = attached(1, 7);
    for (std::uint32_t state : {1u, 2u, 3u, 4u, 5u, 6u})
    {
        const auto result = ESM4::completeNativeIdleRequest(request, state, 11);
        EXPECT_FALSE(result.mCompleted);
        EXPECT_EQ(result.mRequest, request);
        EXPECT_EQ(result.mAction, 11);
    }
    for (std::int16_t action : {-1, 11, 16})
    {
        const auto result = ESM4::completeNativeIdleRequest(request, 0, action);
        EXPECT_TRUE(result.mCompleted);
        EXPECT_EQ(result.mRequest.mStatus, 3);
        EXPECT_EQ(result.mRequest.mSequence, 7);
        EXPECT_EQ(result.mRequest.mIdentity, 1);
        EXPECT_EQ(result.mAction, action == 11 ? -1 : action);
    }
    EXPECT_THROW(ESM4::completeNativeIdleRequest(request, std::nullopt, 11), std::invalid_argument);
    auto unbound = request;
    unbound.mActorBound = false;
    EXPECT_EQ(ESM4::completeNativeIdleRequest(unbound, 0, 11).mAction, 11);
}

TEST(ESM4NativeIdleRequest, ReadyAndDoneAreDistinctAndFailedNullSequenceIsNotDone)
{
    ESM4::NativeIdleRequestSlots slots;
    EXPECT_FALSE(ESM4::nativeIdleRequestReady(slots));
    EXPECT_TRUE(ESM4::nativeIdleRequestDone(slots));
    slots.mPrimary = ESM4::makeNativeIdleRequest(1, 100);
    EXPECT_TRUE(ESM4::nativeIdleRequestReady(slots));
    EXPECT_FALSE(ESM4::nativeIdleRequestDone(slots));
    slots.mPrimary = ESM4::startNativeIdleRequest(*slots.mPrimary, false, 0, -1, 0).mRequest;
    EXPECT_FALSE(ESM4::nativeIdleRequestDone(slots));
    const auto cleared = ESM4::clearNativeIdleRequest(slots, 0, true, false);
    EXPECT_TRUE(ESM4::nativeIdleRequestDone(cleared.mSlots));
    ASSERT_TRUE(cleared.mSlots.mRetired[0]);
    EXPECT_EQ(cleared.mSlots.mRetired[0]->mStatus, 3);
    slots.mPrimary = ESM4::completeNativeIdleRequest(attached(2, 7), 0, 11).mRequest;
    EXPECT_TRUE(ESM4::nativeIdleRequestDone(slots));
    slots.mSecondary = ESM4::makeNativeIdleRequest(3, 0);
    EXPECT_FALSE(ESM4::nativeIdleRequestDone(slots));
}

TEST(ESM4NativeIdleRequest, DestructionDetachesSharedCacheAndUsesProcessSequenceForActionOwnership)
{
    const auto request = attached(1, 7);
    const auto matching = ESM4::destroyNativeIdleRequest(request, 2, 7, 11, 7);
    EXPECT_EQ(matching.mDetachSequence, 7);
    EXPECT_FALSE(matching.mCancelPendingLoad);
    EXPECT_TRUE(matching.mEraseCacheGroup);
    EXPECT_EQ(matching.mCacheGroup, 2);
    EXPECT_EQ(matching.mAction, -1);
    EXPECT_EQ(matching.mActionSequence, 0);
    const auto foreign = ESM4::destroyNativeIdleRequest(request, 2, 8, 11, 8);
    EXPECT_EQ(foreign.mDetachSequence, 7);
    EXPECT_FALSE(foreign.mEraseCacheGroup);
    EXPECT_EQ(foreign.mAction, 11);
    EXPECT_EQ(foreign.mActionSequence, 8);
    const auto pending = ESM4::makeNativeIdleRequest(2, 0);
    const auto empty = ESM4::destroyNativeIdleRequest(pending, std::nullopt, std::nullopt, 11, 0);
    EXPECT_TRUE(empty.mCancelPendingLoad);
    EXPECT_TRUE(empty.mEraseCacheGroup);
    EXPECT_EQ(empty.mCacheGroup, 0xff);
    EXPECT_EQ(empty.mAction, 11);
}

TEST(ESM4NativeIdleRequest, ColdReplacementCancelsOutgoingThirdWithoutCancellingRetainedRequests)
{
    // Original pending-request03: four 475300/474C50 cold requests, then
    // 472ED0/4729F0 cancellation and 4352B0/477C40 delivery. No NI is created
    // by resource readiness, including delivery to the D4/D8 retired requests.
    ESM4::NativeIdleRequestSlots slots;
    for (std::uint64_t id = 1; id <= 4; ++id)
    {
        auto next = ESM4::replaceNativeIdleRequest(slots, 0, ESM4::makeNativeIdleRequest(id, 0));
        if (id < 4)
            EXPECT_TRUE(next.mEffects.empty());
        else
        {
            ASSERT_EQ(next.mEffects.size(), 1);
            EXPECT_EQ(next.mEffects[0].mKind, ESM4::NativeIdleRequestEffectKind::DestroyRequest);
            EXPECT_EQ(next.mEffects[0].mRequest.mIdentity, 3);
            const auto destruction = ESM4::destroyNativeIdleRequest(
                next.mEffects[0].mRequest, std::nullopt, std::nullopt, 16, 0);
            EXPECT_TRUE(destruction.mCancelPendingLoad);
            EXPECT_EQ(destruction.mDetachSequence, 0);
            EXPECT_EQ(destruction.mAction, 16);
        }
        slots = std::move(next.mSlots);
    }
    ASSERT_TRUE(slots.mPrimary);
    ASSERT_TRUE(slots.mRetired[0]);
    ASSERT_TRUE(slots.mRetired[1]);
    EXPECT_EQ(slots.mPrimary->mIdentity, 4);
    EXPECT_EQ(slots.mRetired[0]->mIdentity, 1);
    EXPECT_EQ(slots.mRetired[1]->mIdentity, 2);
    EXPECT_FALSE(ESM4::nativeIdleRequestReady(slots));
    for (auto* slot : {&slots.mPrimary, &slots.mRetired[0], &slots.mRetired[1]})
    {
        *slot = ESM4::acceptNativeIdleRequestResource(**slot, 100);
        EXPECT_EQ((*slot)->mResource, 100);
        EXPECT_EQ((*slot)->mStatus, 1);
        EXPECT_EQ((*slot)->mSequence, 0);
        EXPECT_FALSE((*slot)->mActorBound);
        EXPECT_FALSE(ESM4::destroyNativeIdleRequest(**slot, std::nullopt, std::nullopt, 16, 0)
                         .mCancelPendingLoad);
    }
    EXPECT_TRUE(ESM4::nativeIdleRequestReady(slots));
    EXPECT_FALSE(ESM4::nativeIdleRequestDone(slots));
}

TEST(ESM4NativeIdleRequest, LoaderCancellationRequiresPendingStatusAndAbsentInfo)
{
    // Native policy1 has a nonzero request flag: present info takes the release
    // branch before the status0 cancellation branch at 472B47.
    for (std::uint32_t status : {0u, 1u, 2u, 3u})
        for (std::uint64_t resource : {0u, 100u})
        {
            auto request = ESM4::makeNativeIdleRequest(1, resource);
            request.mStatus = status;
            EXPECT_EQ(ESM4::destroyNativeIdleRequest(request, std::nullopt, std::nullopt, 16, 0)
                          .mCancelPendingLoad,
                status == 0 && resource == 0);
        }
}

TEST(ESM4NativeIdleRequest, FullRetiredPairCanEvictSequenceSharedByEarlierRequests)
{
    // Original full create/register/start course, same loaded KF four times.
    ESM4::NativeIdleRequestSlots slots;
    slots.mPrimary = attached(3, 7);
    slots.mRetired[0] = attached(1, 7);
    slots.mRetired[1] = attached(2, 7);
    const auto next = ESM4::replaceNativeIdleRequest(slots, 7, ESM4::makeNativeIdleRequest(4, 100));
    ASSERT_EQ(next.mEffects.size(), 2);
    EXPECT_EQ(next.mEffects[0].mKind, ESM4::NativeIdleRequestEffectKind::RetireSequence);
    EXPECT_EQ(next.mEffects[1].mKind, ESM4::NativeIdleRequestEffectKind::DestroyRequest);
    const auto destruction = ESM4::destroyNativeIdleRequest(next.mEffects[1].mRequest, 2, 7, 11, 7);
    EXPECT_TRUE(destruction.mEraseCacheGroup);
    EXPECT_EQ(destruction.mDetachSequence, 7);
    EXPECT_EQ(destruction.mAction, -1);
    EXPECT_EQ(next.mSlots.mRetired, slots.mRetired);
    ASSERT_TRUE(next.mSlots.mPrimary);
    EXPECT_EQ(next.mSlots.mPrimary->mStatus, 1);
    EXPECT_EQ(next.mSlots.mPrimary->mSequence, 0);
}
