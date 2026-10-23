#include <components/esm4/friendhit.hpp>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
    const auto first = ESM::FormKey::content("Oblivion.esm", 0x14);
    const auto second = ESM::FormKey::dynamic("friend-hit-test", 1);
}

TEST(ESM4FriendHit, QueryAndRemovalDoNotAllocateAnOwner)
{
    ESM4::FriendHitCounters counters;
    EXPECT_EQ(counters.count(first), 0);
    counters.remove(first);
    counters.advance(1, 10);
    EXPECT_FALSE(counters.present());
    EXPECT_TRUE(counters.entries().empty());
    counters.increment(first);
    EXPECT_TRUE(counters.present());
    EXPECT_EQ(counters.count(first), 1);
    counters.remove(first);
    EXPECT_TRUE(counters.present());
    EXPECT_TRUE(counters.entries().empty());
}

TEST(ESM4FriendHit, EntriesArePrependedAndOwnedByTheReceivingActor)
{
    std::array<ESM4::FriendHitCounters, 2> owners;
    owners[0].increment(first);
    owners[0].advance(2, 10);
    owners[0].increment(second);
    owners[0].increment(first);
    owners[1].increment(second);
    ASSERT_EQ(owners[0].entries().size(), 2);
    EXPECT_EQ(owners[0].entries()[0].mTarget, second);
    EXPECT_EQ(owners[0].entries()[0].mCount, 1);
    EXPECT_EQ(owners[0].entries()[0].mElapsed, 0);
    EXPECT_EQ(owners[0].entries()[1].mTarget, first);
    EXPECT_EQ(owners[0].entries()[1].mCount, 2);
    EXPECT_EQ(owners[0].entries()[1].mElapsed, 2); // Repeated hits do not reset timers.
    EXPECT_EQ(owners[1].count(first), 0);
    EXPECT_EQ(owners[1].count(second), 1);
    owners[0].remove(second);
    EXPECT_EQ(owners[1].count(second), 1);
}

TEST(ESM4FriendHit, AllUnsignedShortCountsWrapWithoutDeletingTheEntry)
{
    // All 65536 starting values independently executed through original
    // 42AA70 + 42AAE0 under PC53/PC64 in native-friend-hit-storage-oracle-01.
    for (unsigned before = 0; before < 65536; ++before)
    {
        ESM4::FriendHitCounters counters(true, {{first, static_cast<std::uint16_t>(before), 1.25f}});
        counters.increment(first);
        ASSERT_EQ(counters.count(first), (before + 1) % 65536);
        ASSERT_EQ(counters.entries().size(), 1);
        ASSERT_EQ(counters.entries()[0].mElapsed, 1.25f);
    }
}

TEST(ESM4FriendHit, SnapshotPreservesZeroCountsTimerBitsAndOrderedDuplicates)
{
    const float timer = std::bit_cast<float>(0x3f800001u);
    ESM4::FriendHitCounters counters(true, {{first, 0, -0.f}, {first, 65535, timer}, {{}, 7, -1}});
    EXPECT_EQ(counters.count(first), 0); // First match wins, including a wrapped zero.
    EXPECT_EQ(counters.count({}), 7);
    counters.increment(first);
    EXPECT_EQ(counters.entries()[0].mCount, 1);
    EXPECT_EQ(std::bit_cast<unsigned>(counters.entries()[0].mElapsed), 0x80000000u);
    EXPECT_EQ(counters.entries()[1].mCount, 65535);
    EXPECT_EQ(std::bit_cast<unsigned>(counters.entries()[1].mElapsed), 0x3f800001u);
    counters.remove(first);
    EXPECT_EQ(counters.count(first), 65535);
    EXPECT_EQ(counters.entries().size(), 2);
}

TEST(ESM4FriendHit, ExpiryMatchesOriginalOrderedRemovalCases)
{
    // Condensed outputs of full original 42AFF0/65C620, 13 cases each at
    // PC53/PC64. Every timer advances first; removal restarts at head.next.
    struct Case
    {
        std::vector<float> times;
        float delta;
        float threshold;
        std::vector<unsigned> remaining;
    };
    const float above = std::bit_cast<float>(0x3f800001u);
    const float below = std::bit_cast<float>(0x3f7fffffu);
    const std::array<Case, 13> cases{{
        {{0}, 0, 1, {0}}, {{1}, 0, 1, {0}}, {{above}, 0, 1, {}}, {{below}, 0, 1, {0}},
        {{0}, 1, 1, {0}}, {{0}, above, 1, {}}, {{-1}, 0, 0, {0}},
        {{0, 0}, 2, 1, {1}}, {{0, 0, 0}, 2, 1, {1}}, {{0, 0, 0, 0}, 2, 1, {1}},
        {{0, 2, 2}, 0, 1, {0}}, {{2, 0, 2}, 0, 1, {1}}, {{2, 2, 0}, 0, 1, {1, 2}}
    }};
    for (std::size_t index = 0; index < cases.size(); ++index)
    {
        SCOPED_TRACE(index);
        const auto& test = cases[index];
        std::vector<ESM4::FriendHitEntry> entries;
        for (std::size_t i = 0; i < test.times.size(); ++i)
            entries.push_back({ESM::FormKey::dynamic("expiry", i + 1), 5, test.times[i]});
        ESM4::FriendHitCounters counters(true, std::move(entries));
        counters.advance(test.delta, test.threshold);
        ASSERT_EQ(counters.entries().size(), test.remaining.size());
        for (std::size_t i = 0; i < test.remaining.size(); ++i)
        {
            const auto original = test.remaining[i];
            EXPECT_EQ(counters.entries()[i].mTarget.mValue, original + 1);
            EXPECT_EQ(counters.entries()[i].mElapsed, test.times[original] + test.delta);
        }
        EXPECT_TRUE(counters.present());
    }
}

TEST(ESM4FriendHit, PromotedExpiredHeadIsRemovedOnTheNextAdmittedUpdate)
{
    ESM4::FriendHitCounters counters(true, {{first, 3, 0}, {second, 5, 0}});
    counters.advance(2, 1);
    ASSERT_EQ(counters.entries().size(), 1);
    EXPECT_EQ(counters.entries()[0].mTarget, second);
    EXPECT_EQ(counters.entries()[0].mElapsed, 2);
    counters.advance(0, 1);
    EXPECT_TRUE(counters.entries().empty());
    EXPECT_TRUE(counters.present());
}

TEST(ESM4FriendHit, InvalidRestoresAndUpdatesAreRejectedWithoutPartialMutation)
{
    EXPECT_THROW((ESM4::FriendHitCounters(false, {{first, 1, 0}})), std::invalid_argument);
    for (float invalid : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        EXPECT_THROW((ESM4::FriendHitCounters(true, {{first, 1, invalid}})), std::invalid_argument);
        ESM4::FriendHitCounters counters(true, {{first, 1, 2}});
        EXPECT_THROW(counters.advance(invalid, 10), std::invalid_argument);
        EXPECT_THROW(counters.advance(1, invalid), std::invalid_argument);
        EXPECT_EQ(counters.entries()[0].mElapsed, 2);
    }
    ESM4::FriendHitCounters counters(true, {{first, 1, 2}, {second, 1, std::numeric_limits<float>::max()}});
    EXPECT_THROW(counters.advance(-1, 10), std::invalid_argument);
    EXPECT_THROW(counters.advance(std::numeric_limits<float>::max(), 10), std::overflow_error);
    ASSERT_EQ(counters.entries().size(), 2);
    EXPECT_EQ(counters.entries()[0].mElapsed, 2);
    EXPECT_EQ(counters.entries()[1].mElapsed, std::numeric_limits<float>::max());
}
