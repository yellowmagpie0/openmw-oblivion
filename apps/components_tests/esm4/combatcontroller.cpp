#include <components/esm4/combatcontroller.hpp>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <limits>
#include <stdexcept>

namespace
{
    const auto owner = ESM::FormKey::dynamic("controller-test", 1);
    const auto player = ESM::FormKey::content("Oblivion.esm", 0x14);
    const auto other = ESM::FormKey::dynamic("controller-test", 2);
    const auto third = ESM::FormKey::dynamic("controller-test", 3);
}

TEST(ESM4CombatController, AdmissionUsesDirectedActorAndCommandCasterIdentity)
{
    EXPECT_FALSE(ESM4::admitsCombatControllerTarget(owner, {}, {}, 40, 0, 0));
    EXPECT_FALSE(ESM4::admitsCombatControllerTarget(owner, owner, {}, 40, 0, 0));
    EXPECT_FALSE(ESM4::admitsCombatControllerTarget(owner, player, owner, 40, 0, 0));
    EXPECT_TRUE(ESM4::admitsCombatControllerTarget(owner, player, other, 40, 0, 0));
    EXPECT_TRUE(ESM4::admitsCombatControllerTarget(owner, other, player, 40, 0, 0));
    EXPECT_THROW(ESM4::admitsCombatControllerTarget({}, player, {}, 40, 0, 0), std::invalid_argument);
}

TEST(ESM4CombatController, OnlyZeroIntegerSpeedReadsRootedReachAndDistance)
{
    // Original 616190: reach + binary32(reach*.25), equality included.
    constexpr float reach = 76.8f;
    const float above = std::bit_cast<float>(0x42c00001u);
    EXPECT_TRUE(ESM4::admitsCombatControllerTarget(owner, player, {}, 0, reach, 96));
    EXPECT_FALSE(ESM4::admitsCombatControllerTarget(owner, player, {}, 0, reach, above));
    EXPECT_TRUE(ESM4::admitsCombatControllerTarget(owner, player, {}, -1, reach, above));
    EXPECT_TRUE(ESM4::admitsCombatControllerTarget(owner, player, {}, 40, reach, above));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_TRUE(ESM4::admitsCombatControllerTarget(owner, player, {}, -1, nan, nan));
    EXPECT_THROW(ESM4::admitsCombatControllerTarget(owner, player, {}, 0, nan, 0), std::invalid_argument);
    EXPECT_THROW(ESM4::admitsCombatControllerTarget(owner, player, {}, 0, reach, nan), std::invalid_argument);
    EXPECT_THROW(ESM4::admitsCombatControllerTarget(owner, player, {}, 0, reach, -1), std::invalid_argument);
    // Native rejection precedes geometry and never reads an invalid target's range.
    EXPECT_FALSE(ESM4::admitsCombatControllerTarget(owner, owner, {}, 0, nan, nan));
}

TEST(ESM4CombatController, SignedPriorityIsDescendingWithNewEqualEntriesFirst)
{
    ESM4::CombatControllerTargets targets;
    targets.add({player, std::numeric_limits<std::int32_t>::min()});
    targets.add({other, std::numeric_limits<std::int32_t>::max()});
    targets.add({third, 200});
    targets.add({owner, 200});
    ASSERT_EQ(targets.entries().size(), 4);
    EXPECT_EQ(targets.entries()[0].mTarget, other);
    EXPECT_EQ(targets.entries()[1].mTarget, owner);
    EXPECT_EQ(targets.entries()[2].mTarget, third);
    EXPECT_EQ(targets.entries()[3].mTarget, player);
    EXPECT_EQ(targets.first(), other);
}

TEST(ESM4CombatController, RepeatedTargetPreservesPriorityMarkerAndFloatBits)
{
    ESM4::CombatControllerTargets targets;
    ASSERT_TRUE(targets.add({player, 200, 255, -0.f, 1.25f}));
    EXPECT_FALSE(targets.add({player, std::numeric_limits<std::int32_t>::max(), 0, 1, -1}));
    // Native duplicate lookup returns before loading the new payload's floats.
    EXPECT_FALSE(targets.add({player, 0, 0, std::numeric_limits<float>::quiet_NaN(), 0}));
    ASSERT_EQ(targets.entries().size(), 1);
    const auto& entry = targets.entries()[0];
    EXPECT_EQ(entry.mPriority, 200);
    EXPECT_EQ(entry.mMarker, 255);
    EXPECT_EQ(std::bit_cast<unsigned>(entry.mValue0C), 0x80000000u);
    EXPECT_EQ(entry.mValue10, 1.25f);
}

TEST(ESM4CombatController, QueriesAndRemovalKeepSeparateDirectedListsIndependent)
{
    std::array<ESM4::CombatControllerTargets, 2> targets;
    EXPECT_TRUE(targets[0].first().isNull());
    EXPECT_FALSE(targets[0].contains(player));
    EXPECT_FALSE(targets[0].remove(player));
    targets[0].add({player, 200});
    targets[0].add({other, 200});
    EXPECT_FALSE(targets[1].contains(owner));
    targets[1].add({player, 1});
    EXPECT_TRUE(targets[0].remove(other));
    EXPECT_EQ(targets[0].first(), player);
    EXPECT_TRUE(targets[1].contains(player));
    EXPECT_TRUE(targets[0].remove(player));
    EXPECT_TRUE(targets[0].first().isNull());
    EXPECT_TRUE(targets[1].contains(player));
}

TEST(ESM4CombatController, OrderedSnapshotValidationRejectsUnsupportedState)
{
    const std::vector<ESM4::CombatControllerTarget> entries{{player, 200, 1, -0.f, 1.25f}, {other, -1}};
    ESM4::CombatControllerTargets targets(entries);
    ASSERT_EQ(targets.entries().size(), entries.size());
    EXPECT_EQ(targets.entries()[0], entries[0]);
    EXPECT_EQ(std::bit_cast<unsigned>(targets.entries()[0].mValue0C), 0x80000000u);
    EXPECT_THROW((ESM4::CombatControllerTargets({{player, 0}, {other, 1}})), std::invalid_argument);
    EXPECT_THROW((ESM4::CombatControllerTargets({{player, 1}, {player, 0}})), std::invalid_argument);
    EXPECT_THROW((ESM4::CombatControllerTargets({{{}, 0}})), std::invalid_argument);
    EXPECT_THROW(targets.add({third, 0, 0, std::numeric_limits<float>::infinity(), 0}), std::invalid_argument);
    EXPECT_EQ(targets.entries().size(), entries.size());
    EXPECT_FALSE(targets.add({}));
}
