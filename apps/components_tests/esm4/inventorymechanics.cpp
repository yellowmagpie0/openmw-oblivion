#include <components/esm4/inventorymechanics.hpp>
#include <components/esm4/inventory.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <bit>
#include <limits>

namespace
{

    TEST(ESM4InventoryMechanics, NativeQueryMagnitudePreservesWrappedMinimum)
    {
        // Original script wrapper 4F48F0, frozen native-getitemcount-oracle-02.
        const std::pair<std::int32_t, std::int32_t> rows[]{
            { -2147483648, -2147483648 }, { -999, 999 }, { -3, 3 },
            { 0, 0 }, { 1, 1 }, { 3, 3 }, { 999, 999 }, { 2147483647, 2147483647 },
        };
        for (const auto& [input, expected] : rows)
        {
            SCOPED_TRACE(input);
            EXPECT_EQ(ESM4::nativeInventoryCountMagnitude(input), expected);
        }
    }

    TEST(ESM4InventoryMechanics, NativeBaseChangeQueryMatchesOriginalSignedBoundaries)
    {
        struct Row { std::int32_t base; std::int32_t delta; bool present; std::int32_t expected; };
        // Frozen original GetItemCount 4F48F0/4869C0/469CA0 outputs.
        const Row rows[]{
            { 0, 0, false, 0 },
            { 0, 0, true, 1 },
            { 0, 3, true, 3 },
            { 0, -3, true, 3 },
            { 3, 0, false, 3 },
            { 3, 0, true, 3 },
            { 3, -3, true, 0 },
            { 3, 3, true, 6 },
            { -3, 0, false, 3 },
            { -3, -3, true, 0 },
            { -3, 3, true, 6 },
            { 2147483647, 1, true, -2147483648 },
            { 2147483647, 3, true, 2147483646 },
            { 2147483647, 2147483647, true, 2 },
            { -2147483648, 0, false, -2147483648 },
            { -2147483648, 0, true, -2147483648 },
            { -2147483648, 1, true, 2147483647 },
            { -2147483648, -2147483648, true, 0 },
            { -2147483648, 2147483647, true, 1 },
            { 1, -2147483648, true, 2147483647 },
        };
        for (const auto& row : rows)
        {
            SCOPED_TRACE(row.base);
            SCOPED_TRACE(row.delta);
            SCOPED_TRACE(row.present);
            EXPECT_EQ(ESM4::nativeInventoryCount(row.base, row.delta, row.present), row.expected);
        }
    }

    TEST(ESM4InventoryMechanics, NativeExistingAddDeltaMatchesOriginalResetAndWrapBranches)
    {
        struct Row { std::int32_t base; std::int32_t delta; std::int32_t requested; std::int32_t expected; };
        // Original 48FB31..48FB5A and actual raw base lookup469CA0.
        const Row rows[]{
            { 0, -999, 1, 1 },
            { 0, -999, -1, -1 },
            { 0, -999, 0, 0 },
            { 0, 0, -999, -999 },
            { 0, 1, -999, -998 },
            { 0, 2147483647, 1, -2147483648 },
            { 0, -2147483648, -1, -1 },
            { 1, -999, 1, -998 },
            { 1, -999, -1, -1000 },
            { 1, -999, 0, -999 },
            { -1, -999, 1, 1 },
            { -1, -999, -1, -1 },
            { -2147483648, -2147483648, -2147483648, -2147483648 },
            { 2147483647, 2147483647, 2147483647, -2 },
            { 2147483647, -1, 1, 0 },
            { -2147483648, 1, -2147483648, -2147483647 },
            { 0, 2147483647, 2147483647, -2 },
            { -1, 2147483647, 2147483647, -2 },
            { 1, -2147483648, -2147483648, 0 },
            { 2147483647, -2147483648, -1, 2147483647 },
        };
        for (const auto& row : rows)
        {
            SCOPED_TRACE(row.base);
            SCOPED_TRACE(row.delta);
            SCOPED_TRACE(row.requested);
            EXPECT_EQ(ESM4::nativeAddItemChangeDelta(row.base, row.delta, row.requested), row.expected);
        }
    }

    TEST(ESM4InventoryMechanics, SignedNativeStockCountsHaveFinitePositiveQuantities)
    {
        static_assert(sizeof(ESM4::InventoryItem) == 8);
        ESM4::InventoryItem item{};
        for (const std::int32_t count : { 0, 1, 3, -1, -3, std::numeric_limits<std::int32_t>::max() })
        {
            item.count = count;
            EXPECT_EQ(ESM4::inventoryItemCount(item), count < 0 ? -count : count);
            EXPECT_EQ(item.count, count); // Do not erase authored stock intent.
        }
        item.count = std::bit_cast<std::int32_t>(std::uint32_t(0xffffffff));
        EXPECT_EQ(item.count, -1);
        EXPECT_EQ(ESM4::inventoryItemCount(item), 1);
        item.count = std::bit_cast<std::int32_t>(std::uint32_t(0xfffffffd));
        EXPECT_EQ(ESM4::inventoryItemCount(item), 3);
        item.count = std::numeric_limits<std::int32_t>::min();
        EXPECT_THROW(ESM4::inventoryItemCount(item), std::overflow_error);
    }

    ESM::FormKey key(std::uint32_t id)
    {
        return ESM::FormKey::content("Oblivion.esm", id);
    }

    TEST(ESM4InventoryMechanics, stacksOnlyIdenticalInstancesAndSaturatesCounts)
    {
        std::vector<ESM4::RuntimeInventoryItem> inventory;
        EXPECT_TRUE(ESM4::addInventoryItem(inventory, { key(1), 2 }));
        EXPECT_TRUE(ESM4::addInventoryItem(inventory, { key(1), 3 }));
        EXPECT_TRUE(ESM4::addInventoryItem(inventory, { key(1), 1, 50 }));
        ASSERT_EQ(inventory.size(), 2u);
        EXPECT_EQ(ESM4::inventoryCount(inventory, key(1)), 6);
        EXPECT_TRUE(ESM4::addInventoryItem(
            inventory, { key(1), std::numeric_limits<std::int32_t>::max() }));
        EXPECT_EQ(ESM4::inventoryCount(inventory, key(1)), std::numeric_limits<std::int32_t>::max());
        EXPECT_FALSE(ESM4::addInventoryItem(inventory, { {}, 1 }));
        EXPECT_FALSE(ESM4::addInventoryItem(inventory, { key(2), 0 }));
    }

    TEST(ESM4InventoryMechanics, removalPreservesEquipmentUntilRequired)
    {
        std::vector<ESM4::RuntimeInventoryItem> inventory{
            { key(1), 1, 100, -1.f, ESM4::InventorySlotWeapon }, { key(1), 3 }, { key(1), 2, -1, -1.f, 0, 4 }
        };
        EXPECT_EQ(ESM4::removeInventoryItem(inventory, key(1), 4), 4);
        EXPECT_EQ(ESM4::inventoryCount(inventory, key(1)), 2);
        EXPECT_TRUE(std::any_of(inventory.begin(), inventory.end(),
            [](const auto& item) { return item.mEquippedSlots == ESM4::InventorySlotWeapon; }));
        EXPECT_EQ(ESM4::removeInventoryItem(inventory, key(1), 99), 2);
        EXPECT_TRUE(inventory.empty());
    }

    TEST(ESM4InventoryMechanics, transfersAreBoundedAndPreserveInstanceState)
    {
        const ESM::FormKey owner = key(99);
        std::vector<ESM4::RuntimeInventoryItem> source{ { key(1), 2, 40, 5.f, 0, -1, owner }, { key(2), 7 } };
        std::vector<ESM4::RuntimeInventoryItem> destination{ { key(1), 1, 40, 5.f, 0, -1, owner } };
        EXPECT_EQ(ESM4::transferInventoryItem(source, destination, key(1), 5), 2);
        EXPECT_EQ(ESM4::inventoryCount(source, key(1)), 0);
        EXPECT_EQ(ESM4::inventoryCount(destination, key(1)), 3);
        ASSERT_EQ(destination.size(), 1u);
        EXPECT_EQ(destination[0].mOwner, owner);
        EXPECT_EQ(ESM4::transferInventoryItem(source, destination, key(3), 1), 0);
    }

    TEST(ESM4InventoryMechanics, equipmentSplitsStacksAndResolvesSlotConflicts)
    {
        constexpr std::uint32_t upper = 1u << 2;
        constexpr std::uint32_t lower = 1u << 3;
        std::vector<ESM4::RuntimeInventoryItem> inventory{ { key(1), 2 }, { key(2), 1 }, { key(3), 20 } };
        ESM4::InventoryItemDefinition robe{ key(1), ESM4::InventoryItemType::Clothing, 10, 1.f, -1, -1.f,
            upper | lower };
        ESM4::InventoryItemDefinition cuirass{ key(2), ESM4::InventoryItemType::Armor, 20, 4.f, 100, -1.f, upper };
        ESM4::InventoryItemDefinition arrows{ key(3), ESM4::InventoryItemType::Ammunition, 1, 0.1f, -1, -1.f,
            ESM4::InventorySlotAmmunition };
        ASSERT_TRUE(ESM4::equipInventoryItem(inventory, robe));
        EXPECT_EQ(inventory.size(), 4u);
        ASSERT_TRUE(ESM4::equipInventoryItem(inventory, cuirass));
        EXPECT_FALSE(std::any_of(inventory.begin(), inventory.end(), [&](const auto& item) {
            return item.mBase == key(1) && item.mEquippedSlots != 0;
        }));
        ASSERT_TRUE(ESM4::equipInventoryItem(inventory, arrows));
        const auto arrow = std::find_if(inventory.begin(), inventory.end(),
            [&](const auto& item) { return item.mBase == key(3); });
        ASSERT_NE(arrow, inventory.end());
        EXPECT_EQ(arrow->mCount, 20);
        EXPECT_EQ(arrow->mEquippedSlots, ESM4::InventorySlotAmmunition);
        EXPECT_TRUE(ESM4::unequipInventoryItem(inventory, key(2)));
        EXPECT_FALSE(ESM4::unequipInventoryItem(inventory, key(2)));
    }

    TEST(ESM4InventoryMechanics, ringsAndHotkeysUseOblivionSemantics)
    {
        constexpr std::uint32_t rightRing = 1u << 6;
        constexpr std::uint32_t leftRing = 1u << 7;
        std::vector<ESM4::RuntimeInventoryItem> inventory{ { key(1), 1 }, { key(2), 1 } };
        ESM4::InventoryItemDefinition ring{ key(1), ESM4::InventoryItemType::Clothing, 10, 0.1f, -1, -1.f,
            rightRing | leftRing, true };
        ASSERT_TRUE(ESM4::equipInventoryItem(inventory, ring, leftRing));
        EXPECT_EQ(inventory[0].mEquippedSlots, leftRing);
        EXPECT_TRUE(ESM4::setInventoryHotkey(inventory, key(1), 3));
        EXPECT_TRUE(ESM4::setInventoryHotkey(inventory, key(2), 3));
        EXPECT_EQ(inventory[0].mHotkey, -1);
        EXPECT_EQ(inventory[1].mHotkey, 3);
        EXPECT_FALSE(ESM4::setInventoryHotkey(inventory, key(2), 8));
    }

    TEST(ESM4InventoryMechanics, twoRingsChooseDifferentSlotsAndIdenticalCopiesCanBothEquip)
    {
        constexpr std::uint32_t rightRing = 1u << 6;
        constexpr std::uint32_t leftRing = 1u << 7;
        ESM4::InventoryItemDefinition first{ key(1), ESM4::InventoryItemType::Clothing, 10, 0.1f, -1,
            -1.f, rightRing | leftRing, true };
        ESM4::InventoryItemDefinition second{ key(2), ESM4::InventoryItemType::Clothing, 10, 0.1f, -1,
            -1.f, rightRing | leftRing, true };
        std::vector<ESM4::RuntimeInventoryItem> inventory{ { key(1), 1 }, { key(2), 1 } };
        ASSERT_TRUE(ESM4::equipInventoryItem(inventory, first));
        ASSERT_TRUE(ESM4::equipInventoryItem(inventory, second));
        EXPECT_EQ(inventory[0].mEquippedSlots | inventory[1].mEquippedSlots, rightRing | leftRing);

        std::vector<ESM4::RuntimeInventoryItem> copies{ { key(1), 2 } };
        ASSERT_TRUE(ESM4::equipInventoryItem(copies, first));
        ASSERT_TRUE(ESM4::equipInventoryItem(copies, first));
        ASSERT_EQ(copies.size(), 2u);
        EXPECT_EQ(copies[0].mEquippedSlots | copies[1].mEquippedSlots, rightRing | leftRing);
        EXPECT_EQ(ESM4::inventoryCount(copies, key(1)), 2);
    }

    TEST(ESM4InventoryMechanics, transfersConserveCountsAndKeepDistinctItemMetadata)
    {
        const ESM::FormKey owner = key(99);
        for (std::int32_t count = 1; count <= 32; ++count)
        {
            std::vector<ESM4::RuntimeInventoryItem> source{
                { key(1), count, 80, 20.f, 0, -1, owner, 125.f },
                { key(1), count + 1, 40, 5.f, 0, -1, {}, 75.f },
            };
            std::vector<ESM4::RuntimeInventoryItem> destination{
                { key(1), 2, 80, 20.f, 0, -1, owner, 125.f },
            };
            const std::int32_t before
                = ESM4::inventoryCount(source, key(1)) + ESM4::inventoryCount(destination, key(1));
            const std::int32_t moved = ESM4::transferInventoryItem(source, destination, key(1), count + 3);
            EXPECT_EQ(moved, std::min(count + 3, count * 2 + 1));
            EXPECT_EQ(ESM4::inventoryCount(source, key(1)) + ESM4::inventoryCount(destination, key(1)), before);
            EXPECT_TRUE(std::any_of(destination.begin(), destination.end(), [&](const auto& item) {
                return item.mOwner == owner && item.mCondition == 80 && item.mRemainingUsageTime == 125.f;
            }));
            EXPECT_TRUE(std::any_of(destination.begin(), destination.end(), [](const auto& item) {
                return item.mOwner.isNull() && item.mCondition == 40 && item.mRemainingUsageTime == 75.f;
            }));
        }
    }

    TEST(ESM4InventoryMechanics, conditionChargeConsumablesAndCostsTransitionExplicitly)
    {
        ESM4::RuntimeInventoryItem weapon{ key(1), 1, 25, 12.f };
        ESM4::InventoryItemDefinition definition{
            key(1), ESM4::InventoryItemType::Weapon, 100, 8.f, 100, 50.f, ESM4::InventorySlotWeapon };
        EXPECT_EQ(ESM4::repairCost(100, 25, 100), 68);
        EXPECT_EQ(ESM4::rechargeCost(12.f, 50.f), 38);
        EXPECT_TRUE(ESM4::repairInventoryItem(weapon, definition));
        EXPECT_TRUE(ESM4::rechargeInventoryItem(weapon, definition));
        EXPECT_EQ(weapon.mCondition, 100);
        EXPECT_EQ(weapon.mCharge, 50.f);
        EXPECT_FALSE(ESM4::repairInventoryItem(weapon, definition));
        std::vector<ESM4::RuntimeInventoryItem> potions{ { key(2), 2 } };
        EXPECT_TRUE(ESM4::consumeInventoryItem(potions, key(2)));
        EXPECT_EQ(ESM4::inventoryCount(potions, key(2)), 1);
    }

    TEST(ESM4InventoryMechanics, marketWorkflowCoversEveryOfficialCategoryAndPersistsEveryOperation)
    {
        ESM4::RuntimeState state;
        state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        state.mPlayer.mCell = key(1000);
        state.mPlayer.mRace = key(1001);
        state.mPlayer.mClass = key(1002);
        ESM4::RuntimeReferenceState merchant;
        merchant.mKey = key(2000);
        merchant.mBase = key(2001);
        merchant.mCell = state.mPlayer.mCell;
        state.mReferences.push_back(std::move(merchant));

        const std::vector<ESM4::InventoryItemDefinition> definitions{
            { key(1), ESM4::InventoryItemType::Ammunition, 1, 0.1f, -1, -1.f,
                ESM4::InventorySlotAmmunition },
            { key(2), ESM4::InventoryItemType::Apparatus, 40, 3.f },
            { key(3), ESM4::InventoryItemType::Armor, 50, 12.f, 100, -1.f, 1u << 2 },
            { key(4), ESM4::InventoryItemType::Book, 25, 1.f },
            { key(5), ESM4::InventoryItemType::Clothing, 10, 1.f, -1, -1.f, 1u << 3 },
            { key(6), ESM4::InventoryItemType::Ingredient, 3, 0.2f, -1, -1.f, 0, false, true },
            { key(7), ESM4::InventoryItemType::Key, 0, 0.f },
            { key(8), ESM4::InventoryItemType::Light, 1, 0.f, -1, -1.f, ESM4::InventorySlotLight,
                false, false, 1000.f },
            { key(9), ESM4::InventoryItemType::Miscellaneous, 5, 0.5f },
            { key(10), ESM4::InventoryItemType::Potion, 30, 0.5f, -1, -1.f, 0, false, true },
            { key(11), ESM4::InventoryItemType::Scroll, 20, 0.2f, -1, -1.f, 0, false, true },
            { key(12), ESM4::InventoryItemType::SigilStone, 500, 1.f, -1, -1.f, 0, false, true },
            { key(13), ESM4::InventoryItemType::SoulGem, 150, 0.4f, -1, -1.f, 0, false, true },
            { key(14), ESM4::InventoryItemType::Weapon, 100, 8.f, 100, 50.f,
                ESM4::InventorySlotWeapon },
        };
        for (const ESM4::InventoryItemDefinition& definition : definitions)
        {
            ESM4::RuntimeInventoryItem item;
            item.mBase = definition.mBase;
            item.mCount = definition.mType == ESM4::InventoryItemType::Ammunition ? 20 : 1;
            item.mCondition = definition.mMaxCondition < 0 ? -1 : 25;
            item.mCharge = definition.mMaxCharge < 0.f ? -1.f : 10.f;
            item.mRemainingUsageTime = definition.mMaxUsageTime;
            ASSERT_TRUE(ESM4::addInventoryItem(state.mPlayer.mInventory, std::move(item)));
        }

        const auto persist = [&] {
            state = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
            state.validate();
        };
        persist();
        const ESM4::BarterActor player{ 55.f, 60.f };
        const ESM4::BarterActor seller{ 40.f, 50.f };
        std::int32_t proceeds = 0;
        for (const ESM4::InventoryItemDefinition& definition : definitions)
        {
            const std::int32_t before = ESM4::inventoryCount(state.mPlayer.mInventory, definition.mBase)
                + ESM4::inventoryCount(state.mReferences[0].mInventory, definition.mBase);
            ASSERT_EQ(ESM4::transferInventoryItem(state.mPlayer.mInventory,
                          state.mReferences[0].mInventory, definition.mBase, 1),
                1);
            proceeds += ESM4::barterOffer(definition.mValue, false, player, seller, 65.f);
            persist();
            EXPECT_EQ(ESM4::inventoryCount(state.mPlayer.mInventory, definition.mBase)
                    + ESM4::inventoryCount(state.mReferences[0].mInventory, definition.mBase),
                before);
            ASSERT_EQ(ESM4::transferInventoryItem(state.mReferences[0].mInventory,
                          state.mPlayer.mInventory, definition.mBase, 1),
                1);
            persist();
            EXPECT_EQ(ESM4::inventoryCount(state.mPlayer.mInventory, definition.mBase)
                    + ESM4::inventoryCount(state.mReferences[0].mInventory, definition.mBase),
                before);
        }
        EXPECT_GT(proceeds, 0);
        EXPECT_TRUE(state.mReferences[0].mInventory.empty());

        auto weapon = std::ranges::find(state.mPlayer.mInventory, key(14), &ESM4::RuntimeInventoryItem::mBase);
        ASSERT_NE(weapon, state.mPlayer.mInventory.end());
        EXPECT_TRUE(ESM4::repairInventoryItem(*weapon, definitions[13]));
        persist();
        weapon = std::ranges::find(state.mPlayer.mInventory, key(14), &ESM4::RuntimeInventoryItem::mBase);
        ASSERT_NE(weapon, state.mPlayer.mInventory.end());
        EXPECT_EQ(weapon->mCondition, 100);
        EXPECT_TRUE(ESM4::rechargeInventoryItem(*weapon, definitions[13]));
        persist();
        weapon = std::ranges::find(state.mPlayer.mInventory, key(14), &ESM4::RuntimeInventoryItem::mBase);
        ASSERT_NE(weapon, state.mPlayer.mInventory.end());
        EXPECT_EQ(weapon->mCharge, 50.f);

        EXPECT_TRUE(ESM4::consumeInventoryItem(state.mPlayer.mInventory, key(10)));
        persist();
        EXPECT_EQ(ESM4::inventoryCount(state.mPlayer.mInventory, key(10)), 0);
        EXPECT_TRUE(ESM4::setInventoryHotkey(state.mPlayer.mInventory, key(11), 7));
        persist();
        EXPECT_TRUE(ESM4::equipInventoryItem(state.mPlayer.mInventory, definitions[0]));
        persist();
        EXPECT_TRUE(ESM4::equipInventoryItem(state.mPlayer.mInventory, definitions[2]));
        persist();
        EXPECT_TRUE(ESM4::equipInventoryItem(state.mPlayer.mInventory, definitions[7]));
        persist();
        EXPECT_TRUE(ESM4::equipInventoryItem(state.mPlayer.mInventory, definitions[13]));
        persist();
        EXPECT_EQ(state.mPlayer.mInventory.size(), definitions.size() - 1);
    }

    TEST(ESM4InventoryMechanics, economyAndLockFormulaBoundariesAreDeterministic)
    {
        const ESM4::BarterActor novice{ 10.f, 40.f };
        const ESM4::BarterActor expert{ 90.f, 60.f };
        EXPECT_LT(ESM4::barterOffer(100, true, expert, novice, 80.f),
            ESM4::barterOffer(100, true, novice, expert, 20.f));
        EXPECT_GT(ESM4::barterOffer(100, false, expert, novice, 80.f),
            ESM4::barterOffer(100, false, novice, expert, 20.f));
        EXPECT_NEAR(ESM4::barterAcceptance(expert, novice, 80.f, 40.f), 0.3f, 0.0001f);
        EXPECT_NEAR(ESM4::barterAcceptance(novice, expert, 20.f, 40.f), -20.3f, 0.0001f);
        EXPECT_NEAR(ESM4::barterAcceptance(expert, novice, 80.f, 100.f), 0.3f, 0.0001f);
        EXPECT_EQ(ESM4::trainingCost(0), 0);
        EXPECT_EQ(ESM4::trainingCost(99), 990);
        EXPECT_EQ(ESM4::trainingCost(100), 0);
        EXPECT_EQ(ESM4::lockpickAutoChance(0, 0.f, 0.f, 100), 0.f);
        EXPECT_EQ(ESM4::lockpickAutoChance(100, 100.f, 100.f, 0), 100.f);
        EXPECT_GT(ESM4::lockpickAutoChance(75, 60.f, 55.f, 25),
            ESM4::lockpickAutoChance(25, 60.f, 55.f, 75));
    }
}

TEST(ESM4InventoryMechanics, OwnershipRankPresenceAndGlobalKeepDifferentInstancesApart)
{
    ESM4::RuntimeInventoryItem item;
    item.mBase = ESM::FormKey::content("items.esm", 1);
    item.mCount = 1;
    item.mOwner = ESM::FormKey::content("items.esm", 2);
    std::vector<ESM4::RuntimeInventoryItem> inventory;
    ASSERT_TRUE(ESM4::addInventoryItem(inventory, item));
    item.mOwnershipRank = -1;
    ASSERT_TRUE(ESM4::addInventoryItem(inventory, item));
    item.mOwnershipRank = -2;
    ASSERT_TRUE(ESM4::addInventoryItem(inventory, item));
    item.mOwnershipRank = 0;
    ASSERT_TRUE(ESM4::addInventoryItem(inventory, item));
    item.mOwnershipGlobal = ESM::FormKey::content("items.esm", 3);
    ASSERT_TRUE(ESM4::addInventoryItem(inventory, item));
    item.mOwnershipGlobal = ESM::FormKey::content("items.esm", 4);
    ASSERT_TRUE(ESM4::addInventoryItem(inventory, item));
    ASSERT_EQ(inventory.size(), 6u);
    for (const auto& instance : inventory) EXPECT_EQ(instance.mCount, 1);
    ASSERT_TRUE(ESM4::addInventoryItem(inventory, item));
    EXPECT_EQ(inventory.size(), 6u);
    EXPECT_EQ(inventory.back().mCount, 2);
}
