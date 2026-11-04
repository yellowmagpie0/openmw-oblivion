#include <components/esm4/boundequipment.hpp>
#include <components/esm4/inventorymechanics.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>

namespace
{
    ESM::FormKey key(std::uint32_t id) { return ESM::FormKey::content("Oblivion.esm", id); }

    ESM4::RuntimeInventoryItem ordinary(std::uint32_t id, std::uint32_t slots, std::int32_t count = 1)
    {
        ESM4::RuntimeInventoryItem item;
        item.mBase = key(id);
        item.mCount = count;
        item.mEquippedSlots = slots;
        item.mCondition = 4190.5;
        item.mCharge = 17.5f;
        item.mOwner = key(0x900);
        item.mOwnershipRank = -2;
        item.mOwnershipGlobal = key(0x901);
        item.mExtraScale = 0.75f;
        return item;
    }

    ESM4::BoundEquipmentGrant grant(std::uint32_t spell = 0x100, std::uint32_t slots = 0x3c)
    {
        auto item = ordinary(0x200 + spell, slots);
        item.mBoundSource = ESM4::BoundItemSource{key(spell), 0};
        return {key(spell), {item}};
    }

    TEST(ESM4BoundEquipment, GrantsDisplaceOrdinaryEquipmentAndReloadPreservesWornInstances)
    {
        const auto robe = ordinary(0x10, 0x3c, 3);
        const auto hood = ordinary(0x11, 3);
        const auto weapon = ordinary(0x12, ESM4::InventorySlotWeapon);
        auto ability = grant();
        auto helmet = ordinary(0x301, 3);
        helmet.mBoundSource = ESM4::BoundItemSource{ability.mSpell, 1};
        auto mace = ordinary(0x302, ESM4::InventorySlotWeapon);
        mace.mBoundSource = ESM4::BoundItemSource{ability.mSpell, 2};
        ability.mItems.insert(ability.mItems.end(), {helmet, mace});
        const std::vector<ESM4::RuntimeInventoryItem> original{robe, hood, weapon};
        auto applied = ESM4::prepareBoundEquipmentUpdate(original, {}, {ability});
        ASSERT_EQ(applied.mInventory.size(), 6u);
        ASSERT_EQ(applied.mAbilities.size(), 1u);
        auto singleRobe = robe;
        singleRobe.mCount = 1;
        EXPECT_EQ(applied.mAbilities[0].mDisplacedEquipment,
            (std::vector<ESM4::RuntimeInventoryItem>{singleRobe, hood, weapon}));
        for (std::size_t i = 0; i < original.size(); ++i)
        {
            auto expected = original[i];
            expected.mEquippedSlots = 0;
            EXPECT_EQ(applied.mInventory[i], expected);
            EXPECT_EQ(original[i].mEquippedSlots, applied.mAbilities[0].mDisplacedEquipment[i].mEquippedSlots);
        }
        // Real wear/charge and a saved unequip are instance state, not defaults
        // to repair or override on a fresh process's actor initialization.
        applied.mInventory[3].mCondition = 101.25;
        applied.mInventory[4].mCharge = 0;
        applied.mInventory[5].mEquippedSlots = 0;
        auto reloaded = ESM4::prepareBoundEquipmentUpdate(applied.mInventory, applied.mAbilities, {ability});
        EXPECT_EQ(reloaded.mInventory, applied.mInventory);
        EXPECT_EQ(reloaded.mAbilities, applied.mAbilities);
        EXPECT_EQ(ESM4::inventoryCount(reloaded.mInventory, robe.mBase), 3);
    }

    TEST(ESM4BoundEquipment, DeathAndSourceRemovalRestoreOnlyExistingOrdinaryUnits)
    {
        const auto robe = ordinary(0x10, 0x3c, 3);
        auto applied = ESM4::prepareBoundEquipmentUpdate({robe}, {}, {grant()});
        for (const bool dead : {false, true})
        {
            SCOPED_TRACE(dead);
            const auto removed = ESM4::prepareBoundEquipmentUpdate(
                applied.mInventory, applied.mAbilities, dead ? std::vector{grant()} : std::vector<ESM4::BoundEquipmentGrant>{}, dead);
            EXPECT_TRUE(removed.mAbilities.empty());
            ASSERT_EQ(removed.mInventory.size(), 2u);
            auto stack = robe;
            stack.mCount = 2;
            stack.mEquippedSlots = 0;
            auto equipped = robe;
            equipped.mCount = 1;
            EXPECT_EQ(removed.mInventory, (std::vector{stack, equipped}));
            EXPECT_EQ(ESM4::inventoryCount(removed.mInventory, robe.mBase), 3);
            const auto again = ESM4::prepareBoundEquipmentUpdate(removed.mInventory, removed.mAbilities, {}, dead);
            EXPECT_EQ(again.mInventory, removed.mInventory);
        }
        // Selling/removing ordinary equipment while the ability is active
        // must never cause the restoration record to generate another unit.
        applied.mInventory.erase(applied.mInventory.begin());
        const auto removed = ESM4::prepareBoundEquipmentUpdate(applied.mInventory, applied.mAbilities, {}, true);
        EXPECT_TRUE(removed.mInventory.empty());
        EXPECT_TRUE(removed.mAbilities.empty());
    }

    TEST(ESM4BoundEquipment, IdenticalOrdinaryBaseDoesNotBecomeOwnedOrDisappear)
    {
        auto ability = grant();
        auto ordinaryCopy = ability.mItems[0];
        ordinaryCopy.mBoundSource.reset();
        ordinaryCopy.mEquippedSlots = 0;
        ordinaryCopy.mCount = 7;
        auto applied = ESM4::prepareBoundEquipmentUpdate({ordinaryCopy}, {}, {ability});
        ESM4::normalizeInventory(applied.mInventory);
        ASSERT_EQ(applied.mInventory.size(), 2u);
        EXPECT_EQ(applied.mInventory[0], ordinaryCopy);
        auto removed = ESM4::prepareBoundEquipmentUpdate(applied.mInventory, applied.mAbilities, {}, true);
        EXPECT_EQ(removed.mInventory, std::vector{ordinaryCopy});
    }

    TEST(ESM4BoundEquipment, MultiSlotDisplacementWaitsForTheRemainingBoundAbility)
    {
        const auto robe = ordinary(0x10, 0xc);
        auto first = grant(0x100, 0x4);
        auto second = grant(0x101, 0x8);
        const auto both = ESM4::prepareBoundEquipmentUpdate({robe}, {}, {first, second});
        ASSERT_EQ(both.mAbilities[0].mDisplacedEquipment.size(), 1u);
        ASSERT_TRUE(both.mAbilities[1].mDisplacedEquipment.empty());
        const auto remaining = ESM4::prepareBoundEquipmentUpdate(both.mInventory, both.mAbilities, {second});
        ASSERT_EQ(remaining.mAbilities.size(), 1u);
        EXPECT_EQ(remaining.mAbilities[0].mDisplacedEquipment, std::vector{robe});
        EXPECT_EQ(remaining.mInventory[0].mEquippedSlots, 0u);
        const auto removed = ESM4::prepareBoundEquipmentUpdate(remaining.mInventory, remaining.mAbilities, {});
        EXPECT_EQ(removed.mInventory, std::vector{robe});
    }

    TEST(ESM4BoundEquipment, RemovalThenReplacementDoesNotDuplicateDisplacedEquipment)
    {
        const auto robe = ordinary(0x10, 0x3c);
        auto first = grant();
        auto second = grant(0x101);
        auto applied = ESM4::prepareBoundEquipmentUpdate({robe}, {}, {first});
        const auto replaced = ESM4::prepareBoundEquipmentUpdate(applied.mInventory, applied.mAbilities, {second});
        ASSERT_EQ(replaced.mAbilities.size(), 1u);
        EXPECT_EQ(replaced.mAbilities[0].mSpell, second.mSpell);
        EXPECT_EQ(replaced.mAbilities[0].mDisplacedEquipment, std::vector{robe});
        EXPECT_EQ(ESM4::inventoryCount(replaced.mInventory, robe.mBase), 1);
        EXPECT_EQ(ESM4::inventoryCount(replaced.mInventory, first.mItems[0].mBase), 0);
        const auto removed = ESM4::prepareBoundEquipmentUpdate(replaced.mInventory, replaced.mAbilities, {});
        EXPECT_EQ(removed.mInventory, std::vector{robe});
    }

    TEST(ESM4BoundEquipment, RejectsCorruptOwnershipAndInstalledDefinitionChangesWithoutMutatingInputs)
    {
        const auto definition = grant();
        const auto valid = ESM4::prepareBoundEquipmentUpdate({}, {}, {definition});
        for (int variant = 0; variant < 10; ++variant)
        {
            SCOPED_TRACE(variant);
            auto inventory = valid.mInventory;
            auto owners = valid.mAbilities;
            auto definitions = std::vector{definition};
            switch (variant)
            {
                case 0: owners.clear(); break;
                case 1: inventory.clear(); break;
                case 2: inventory.push_back(inventory[0]); break;
                case 3: owners.push_back(owners[0]); break;
                case 4: inventory[0].mCount = 2; break;
                case 5: inventory[0].mHotkey = 0; break;
                case 6: inventory[0].mBase = key(0x999); break;
                case 7: inventory[0].mBoundSource->mEffectIndex = 5; break;
                case 8: definitions.push_back(definitions[0]); break;
                case 9: owners[0].mDisplacedEquipment.push_back(inventory[0]); break;
            }
            const auto unchanged = inventory;
            const auto unchangedOwners = owners;
            EXPECT_THROW(ESM4::prepareBoundEquipmentUpdate(inventory, owners, definitions), std::invalid_argument);
            EXPECT_EQ(inventory, unchanged);
            EXPECT_EQ(owners, unchangedOwners);
        }
    }
}
