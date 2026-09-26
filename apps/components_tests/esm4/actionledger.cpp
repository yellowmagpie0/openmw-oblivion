#include <components/esm4/actionledger.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>

TEST(ESM4ActionLedger, OlderProjectileCanCompleteAfterNewerMeleeAndRestore)
{
    ESM4::ActionLedger ledger;
    const auto arrow = ledger.allocate();
    const auto melee = ledger.allocate();
    EXPECT_EQ(arrow, 1);
    EXPECT_EQ(melee, 2);
    EXPECT_TRUE(ledger.consume(melee));
    EXPECT_FALSE(ledger.consume(melee));
    ESM4::ActionLedger restored;
    restored.restore(ledger.capture());
    EXPECT_TRUE(restored.isPending(arrow));
    EXPECT_FALSE(restored.isConsumed(arrow));
    EXPECT_TRUE(restored.isConsumed(melee));
    EXPECT_FALSE(restored.consume(melee));
    EXPECT_TRUE(restored.consume(arrow));
    EXPECT_TRUE(restored.isConsumed(arrow));
    EXPECT_FALSE(restored.consume(arrow));
    EXPECT_EQ(restored.allocate(), 3);
}

TEST(ESM4ActionLedger, RejectsUnissuedAndMalformedIdentitiesWithoutChangingState)
{
    ESM4::ActionLedger ledger;
    EXPECT_FALSE(ledger.isConsumed(0));
    EXPECT_FALSE(ledger.consume(0));
    EXPECT_FALSE(ledger.consume(1));
    const auto id = ledger.allocate();
    const auto before = ledger.capture();
    EXPECT_FALSE(ledger.isConsumed(id + 1));
    EXPECT_FALSE(ledger.consume(id + 1));
    EXPECT_FALSE(ledger.consume(std::numeric_limits<std::uint64_t>::max()));
    for (const auto& invalid : std::vector<ESM4::ActionLedgerState>{
        {0, {}}, {2, {0}}, {2, {2}}, {2, {1, 1}}, {2, {3}}})
    {
        EXPECT_THROW(ledger.restore(invalid), std::invalid_argument);
        EXPECT_EQ(ledger.capture(), before);
    }
}

TEST(ESM4ActionLedger, ExhaustionNeverWrapsOrReusesAnIdentity)
{
    ESM4::ActionLedger ledger;
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    ledger.restore({maximum - 1, {1}});
    EXPECT_EQ(ledger.allocate(), maximum - 1);
    const auto before = ledger.capture();
    EXPECT_THROW(ledger.allocate(), std::overflow_error);
    EXPECT_EQ(ledger.capture(), before);
    EXPECT_TRUE(ledger.consume(1));
    EXPECT_TRUE(ledger.consume(maximum - 1));
    EXPECT_FALSE(ledger.isConsumed(maximum));
    EXPECT_THROW(ledger.allocate(), std::overflow_error);
}

TEST(ESM4ActionLedger, RetentionDependsOnPendingActionsNotCompletedHistory)
{
    ESM4::ActionLedger ledger;
    const auto oldArrow = ledger.allocate();
    for (unsigned i = 0; i < 100'000; ++i)
        ASSERT_TRUE(ledger.consume(ledger.allocate()));
    const auto snapshot = ledger.capture();
    EXPECT_EQ(snapshot.mNext, 100'002);
    EXPECT_EQ(snapshot.mPending, std::vector<std::uint64_t>{oldArrow});
    ESM4::ActionLedger restored;
    restored.restore(snapshot);
    EXPECT_TRUE(restored.consume(oldArrow));
    EXPECT_TRUE(restored.capture().mPending.empty());
    EXPECT_TRUE(restored.isConsumed(50'000));
}

TEST(ESM4ActionLedger, RandomCompletionAndRepeatedRestoreMatchFullHistoryModel)
{
    std::mt19937 random(0x15);
    ESM4::ActionLedger ledger;
    std::set<std::uint64_t> issued;
    std::set<std::uint64_t> consumed;
    for (unsigned step = 0; step < 1000; ++step)
    {
        if (random() % 3 == 0 || issued.empty())
        {
            const auto id = ledger.allocate();
            ASSERT_TRUE(issued.insert(id).second);
        }
        else
        {
            const auto id = random() % (issued.size() + 2);
            const bool expected = issued.contains(id) && !consumed.contains(id);
            EXPECT_EQ(ledger.consume(id), expected);
            if (expected)
                consumed.insert(id);
        }
        if (step % 37 == 0)
        {
            auto snapshot = ledger.capture();
            std::reverse(snapshot.mPending.begin(), snapshot.mPending.end());
            ESM4::ActionLedger restored;
            restored.restore(snapshot);
            ledger = std::move(restored);
        }
        for (const auto id : issued)
        {
            ASSERT_EQ(ledger.isConsumed(id), consumed.contains(id));
            ASSERT_EQ(ledger.isPending(id), !consumed.contains(id));
        }
    }
    ledger.restore({});
    EXPECT_EQ(ledger.allocate(), 1);
}
