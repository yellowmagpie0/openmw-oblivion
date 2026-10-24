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

TEST(ESM4CombatController, ScriptPriorityCapturesThePriorFirstTargetWithInt32Wrapping)
{
    EXPECT_FALSE(ESM4::CombatControllerTargets{}.nextScriptPriority());
    for (const auto& [prior, expected] : std::array<std::pair<std::int32_t, std::int32_t>, 5>{
             {{200, 220}, {-20, 0}, {0, 20},
                 {std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::min() + 19},
                 {std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::min() + 20}}})
    {
        const ESM4::CombatControllerTargets targets({{player, prior, 0}});
        EXPECT_EQ(targets.nextScriptPriority(), expected);
    }
}

TEST(ESM4CombatController, ScriptPriorityHonorsSingleTargetMarkerAndPreservesPayloadBits)
{
    for (const std::uint8_t marker : {0, 1, 255})
    {
        ESM4::CombatControllerTargets targets({{player, 200, marker, -0.f, -2.5f}});
        EXPECT_FALSE(targets.applyScriptPriority(other, 220));
        EXPECT_EQ(targets.applyScriptPriority(player, 220), marker == 0);
        EXPECT_EQ(targets.entries()[0].mPriority, marker == 0 ? 220 : 200);
        EXPECT_EQ(targets.entries()[0].mMarker, marker == 0 ? 1 : marker);
        EXPECT_EQ(std::bit_cast<unsigned>(targets.entries()[0].mValue0C), 0x80000000u);
        EXPECT_EQ(std::bit_cast<unsigned>(targets.entries()[0].mValue10), 0xc0200000u);
        EXPECT_FALSE(targets.applyScriptPriority(player, 220));
    }
    ESM4::CombatControllerTargets markerOnly({{player, 200, 0}});
    EXPECT_TRUE(markerOnly.applyScriptPriority(player, 200));
    EXPECT_EQ(markerOnly.entries()[0].mMarker, 1);
}

TEST(ESM4CombatController, ScriptPriorityUsesNativeShellSortForEqualPriorityTargets)
{
    std::vector<ESM4::CombatControllerTarget> entries;
    for (unsigned i = 0; i < 10; ++i)
        entries.push_back({ESM::FormKey::dynamic("script-sort", i + 1), 200, static_cast<std::uint8_t>(i), -0.f, 1.25f});
    ESM4::CombatControllerTargets targets(entries);
    EXPECT_TRUE(targets.applyScriptPriority(entries[4].mTarget, *targets.nextScriptPriority()));
    // Original506F00 gap4 moves target4 past0 without moving targets1..3.
    constexpr std::array<unsigned, 10> order{4, 1, 2, 3, 0, 5, 6, 7, 8, 9};
    for (unsigned i = 0; i < order.size(); ++i)
    {
        const auto& actual = targets.entries()[i];
        EXPECT_EQ(actual.mTarget, entries[order[i]].mTarget);
        EXPECT_EQ(actual.mPriority, i == 0 ? 220 : 200);
        EXPECT_EQ(actual.mMarker, entries[order[i]].mMarker);
        EXPECT_EQ(std::bit_cast<unsigned>(actual.mValue0C), 0x80000000u);
        EXPECT_EQ(std::bit_cast<unsigned>(actual.mValue10), 0x3fa00000u);
    }
    EXPECT_FALSE(targets.applyScriptPriority(entries[4].mTarget, 220));
    ESM4::CombatControllerTargets wrapped({{player, std::numeric_limits<std::int32_t>::max(), 255},
        {other, std::numeric_limits<std::int32_t>::min() + 19, 0}});
    EXPECT_FALSE(wrapped.applyScriptPriority(other, *wrapped.nextScriptPriority()));
    EXPECT_EQ(wrapped.first(), player);
}

TEST(ESM4CombatController, CoordinateDistanceRetainsNativeFloatStores)
{
    const std::array<float, 3> origin{};
    EXPECT_EQ(ESM4::combatControllerDistance(origin, {3, 4, 12}), 13.f);
    EXPECT_EQ(std::bit_cast<unsigned>(ESM4::combatControllerDistance(origin, origin)), 0u);
    // Original 4D7E30 rounds the squared sum to binary32 before sqrt.
    // This sum exceeds FLT_MAX but rounds back to FLT_MAX, below overflow.
    const auto nearLimit = ESM4::combatControllerDistance(origin,
        {std::bit_cast<float>(0x5f7fffffu), 0x1p52f, 0});
    EXPECT_EQ(std::bit_cast<unsigned>(nearLimit), 0x5f7fffffu);
    EXPECT_EQ(ESM4::combatControllerDistance(origin, {0x1p64f, 0, 0}),
        std::numeric_limits<float>::infinity());
    EXPECT_EQ(ESM4::combatControllerDistance(origin, {0x1p-100f, 0x1p-100f, 0x1p-100f}), 0.f);
    const float max = std::numeric_limits<float>::max();
    EXPECT_EQ(ESM4::combatControllerDistance({max, 0, 0}, {-max, 0, 0}),
        std::numeric_limits<float>::infinity());
}

TEST(ESM4CombatController, CoordinateDistanceValidatesAllCoordinatesBeforeOverflow)
{
    const float max = std::numeric_limits<float>::max();
    for (const float invalid : {std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        EXPECT_THROW(ESM4::combatControllerDistance({max, 0, 0}, {-max, 0, invalid}), std::invalid_argument);
        EXPECT_THROW(ESM4::combatControllerDistance({max, invalid, 0}, {-max, 0, 0}), std::invalid_argument);
    }
}
