#include "inventorystore.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <exception>
#include <iterator>
#include <stdexcept>

#include <components/esm3/inventorystate.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/npcstats.hpp"
#include "../mwmechanics/weapontype.hpp"

#include "class.hpp"
#include "worldimp.hpp"

namespace
{
    std::uint32_t oblivionEquipmentSlots(
        const MWWorld::Ptr& item, int sharedSlot)
    {
        auto* world = static_cast<MWWorld::World*>(
            static_cast<MWBase::World*>(MWBase::Environment::get().getWorld()));
        return world->oblivionEquipmentSlots(item, sharedSlot);
    }
}

void MWWorld::InventoryStore::copySlots(const InventoryStore& store)
{
    for (const MWWorld::ContainerStoreIterator& it : store.mSlots)
    {
        const std::ptrdiff_t distance = store.index(it);
        if (distance == -1)
        {
            mSlots.push_back(end());
            continue;
        }
        ContainerStoreIterator slot = begin();
        std::advance(slot, distance);
        mSlots.push_back(slot);
    }
}

void MWWorld::InventoryStore::initSlots(TSlots& slots)
{
    slots.reserve(Slots);
    for (int i = 0; i < Slots; ++i)
        slots.push_back(end());
}

void MWWorld::InventoryStore::storeEquipmentState(
    const MWWorld::LiveCellRefBase& ref, size_t index, ESM::InventoryState& inventory) const
{
    MWWorld::ContainerStore::storeEquipmentState(ref, index, inventory);

    for (int32_t i = 0; i < MWWorld::InventoryStore::Slots; ++i)
    {
        if (mSlots[i].getType() != -1 && mSlots[i]->getBase() == &ref)
            inventory.mEquipmentSlots[static_cast<uint32_t>(index)] = i;
    }
}

void MWWorld::InventoryStore::readEquipmentState(
    const MWWorld::ContainerStoreIterator& iter, size_t index, const ESM::InventoryState& inventory)
{
    MWWorld::ContainerStore::readEquipmentState(iter, index, inventory);

    auto found = inventory.mEquipmentSlots.find(static_cast<uint32_t>(index));
    if (found != inventory.mEquipmentSlots.end())
    {
        if (found->second < 0 || found->second >= MWWorld::InventoryStore::Slots)
            throw std::runtime_error("Invalid slot index in inventory state");

        // make sure the item can actually be equipped in this slot
        int32_t slot = found->second;
        std::pair<std::vector<int>, bool> allowedSlots = iter->getClass().getEquipmentSlots(*iter);
        if (!allowedSlots.first.size())
            return;
        if (std::find(allowedSlots.first.begin(), allowedSlots.first.end(), slot) == allowedSlots.first.end())
            slot = allowedSlots.first.front();

        // unstack if required
        if (!allowedSlots.second && iter->getCellRef().getCount() > 1)
        {
            int count = iter->getCellRef().getCount(false);
            MWWorld::ContainerStoreIterator newIter = addNewStack(*iter, count > 0 ? 1 : -1);
            if (mDetachedRestoreRead)
            {
                // The split copy is a new instance. It receives a fresh
                // registry identity only when the prepared contents publish.
                newIter->getCellRef().unsetRefNum();
                newIter->getRefData().setLuaScripts(nullptr);
            }
            iter->getCellRef().setCount(subtractItems(count, 1));
            mSlots[slot] = newIter;
        }
        else
            mSlots[slot] = iter;
    }
}

MWWorld::InventoryStore::InventoryStore()
{
    initSlots(mSlots);
}

MWWorld::InventoryStore::InventoryStore(const MWWorld::InventoryStore& store)
    : ContainerStore(store)
    , mInventoryListener(store.mInventoryListener)
    , mUpdatesEnabled(store.mUpdatesEnabled)
    , mFirstAutoEquip(store.mFirstAutoEquip)
{
    copySlots(store);
}

MWWorld::InventoryStore::InventoryStore(MWWorld::InventoryStore&& store)
{
    *this = std::move(store);
}

MWWorld::InventoryStore& MWWorld::InventoryStore::operator=(const InventoryStore& store)
{
    if (this == &store)
        return *this;
    mPreparedAmmunitionIdentity.reset();
    ContainerStore::operator=(store);
    mInventoryListener = store.mInventoryListener;
    mUpdatesEnabled = store.mUpdatesEnabled;
    mFirstAutoEquip = store.mFirstAutoEquip;
    mSlots.clear();
    copySlots(store);
    return *this;
}

MWWorld::InventoryStore& MWWorld::InventoryStore::operator=(InventoryStore&& store)
{
    if (this == &store) return *this;
    mPreparedAmmunitionIdentity.reset();
    store.mPreparedAmmunitionIdentity.reset();
    mInventoryListener = store.mInventoryListener;
    mUpdatesEnabled = store.mUpdatesEnabled;
    mFirstAutoEquip = store.mFirstAutoEquip;
    mSlots.clear();
    std::array<std::ptrdiff_t, Slots> distances;
    for (int i = 0; i < Slots; ++i)
    {
        distances[i] = store.index(store.mSlots[i]);
    }
    ContainerStore::operator=(std::move(store));
    for (int i = 0; i < Slots; ++i)
    {
        if (distances[i] == -1)
            mSlots.push_back(end());
        else
            std::advance(mSlots.emplace_back(begin()), distances[i]);
    }
    return *this;
}

void MWWorld::InventoryStore::swapPreparedContents(InventoryStore& other) noexcept
{
    if (this == &other)
        return;
    mPreparedAmmunitionIdentity.reset();
    other.mPreparedAmmunitionIdentity.reset();
    ContainerStore::swapPreparedContents(other);
    mSlots.swap(other.mSlots);
    for (auto& slot : mSlots)
        rebindPreparedIterator(slot);
    for (auto& slot : other.mSlots)
        other.rebindPreparedIterator(slot);
}

MWWorld::ContainerStoreIterator MWWorld::InventoryStore::add(
    const ConstPtr& itemPtr, int count, bool allowAutoEquip, bool resolve)
{
    const MWWorld::ContainerStoreIterator& retVal
        = MWWorld::ContainerStore::add(itemPtr, count, allowAutoEquip, resolve);

    // Auto-equip items if an armor/clothing item is added, but not for the player nor werewolves
    const Ptr& actor = getPtr();
    if (allowAutoEquip && actor != MWMechanics::getPlayer() && actor.getClass().isNpc()
        && !actor.getClass().getNpcStats(actor).isWerewolf())
    {
        auto type = itemPtr.getType();
        if (type == ESM::Armor::sRecordId || type == ESM::Clothing::sRecordId)
            autoEquip();
    }

    if (mListener)
        mListener->itemAdded(*retVal, count);
    if (auto* windows = MWBase::Environment::get().getWindowManagerOrNull())
        windows->inventoryUpdated(actor);

    return retVal;
}

void MWWorld::InventoryStore::equip(int slot, const ContainerStoreIterator& iterator)
{
    if (iterator == end())
        throw std::runtime_error("can't equip end() iterator, use unequip function instead");

    if (slot < 0 || slot >= static_cast<int>(mSlots.size()))
        throw std::runtime_error("slot number out of range");

    if (iterator.getContainerStore() != this)
        throw std::runtime_error("attempt to equip an item that is not in the inventory");

    std::pair<std::vector<int>, bool> slots;

    slots = iterator->getClass().getEquipmentSlots(*iterator);

    if (std::find(slots.first.begin(), slots.first.end(), slot) == slots.first.end())
        throw std::runtime_error("invalid slot");

    // TES4 apparel may cover several biped parts even though the shared item
    // facade exposes one representative TES3 slot. Remove every native slot
    // conflict so robes, armor, rings, weapons, ammunition, and torches obey
    // Oblivion's equipment model when equipped through the shared UI.
    if (MWBase::Environment::get().getWorld()->getGameProfile() == ESM::GameProfile::Oblivion)
    {
        const std::uint32_t nativeSlots = oblivionEquipmentSlots(*iterator, slot);
        if (nativeSlots != 0)
        {
            const bool updatesEnabled = mUpdatesEnabled;
            mUpdatesEnabled = false;
            for (int occupied = 0; occupied < static_cast<int>(mSlots.size()); ++occupied)
            {
                if (mSlots[occupied] == end() || mSlots[occupied] == iterator)
                    continue;
                const std::uint32_t occupiedSlots = oblivionEquipmentSlots(*mSlots[occupied], occupied);
                if ((occupiedSlots & nativeSlots) != 0)
                    unequipSlot(occupied);
            }
            mUpdatesEnabled = updatesEnabled;
        }
    }

    if (mSlots[slot] != end())
        unequipSlot(slot);

    // unstack item pointed to by iterator if required
    if (iterator != end() && !slots.second
        && iterator->getCellRef().getCount() > 1) // if slots.second is true, item can stay stacked when equipped
    {
        unstack(*iterator);
    }

    mSlots[slot] = iterator;

    flagAsModified();

    fireEquipmentChangedEvent();
}

void MWWorld::InventoryStore::unequipAll()
{
    mUpdatesEnabled = false;
    for (int slot = 0; slot < MWWorld::InventoryStore::Slots; ++slot)
        unequipSlot(slot);

    mUpdatesEnabled = true;

    fireEquipmentChangedEvent();
}

MWWorld::ContainerStoreIterator MWWorld::InventoryStore::getSlot(int slot)
{
    return findSlot(slot);
}

MWWorld::ConstContainerStoreIterator MWWorld::InventoryStore::getSlot(int slot) const
{
    return findSlot(slot);
}

MWWorld::ContainerStoreIterator MWWorld::InventoryStore::findSlot(int slot) const
{
    if (slot < 0 || slot >= static_cast<int>(mSlots.size()))
        throw std::runtime_error("slot number out of range");

    if (mSlots[slot] == end())
        return mSlots[slot];

    // NOTE: mSlots[slot]->getRefData().getCount() can be zero if the item is marked
    // for removal by a Lua script, but the removal action is not yet processed.
    // The item will be automatically unequiped in the current frame.

    return mSlots[slot];
}

void MWWorld::InventoryStore::autoEquipWeapon(TSlots& slots)
{
    const Ptr& actor = getPtr();
    if (!actor.getClass().isNpc())
    {
        // In original game creatures do not autoequip weapon, but we need it for weapon sheathing.
        // The only case when the difference is noticable - when this creature sells weapon.
        // So just disable weapon autoequipping for creatures which sells weapon.
        int services = actor.getClass().getServices(actor);
        bool sellsWeapon = services & (ESM::NPC::Weapon | ESM::NPC::MagicItems);
        if (sellsWeapon)
            return;
    }

    static const ESM::RefId weaponSkills[] = {
        ESM::Skill::LongBlade,
        ESM::Skill::Axe,
        ESM::Skill::Spear,
        ESM::Skill::ShortBlade,
        ESM::Skill::Marksman,
        ESM::Skill::BluntWeapon,
    };
    const size_t weaponSkillsLength = sizeof(weaponSkills) / sizeof(weaponSkills[0]);

    bool weaponSkillVisited[weaponSkillsLength] = { false };

    // give arrows/bolt with max damage by default
    int arrowMax = 0;
    int boltMax = 0;
    ContainerStoreIterator arrow(end());
    ContainerStoreIterator bolt(end());

    // rate ammo
    for (ContainerStoreIterator iter(begin(ContainerStore::Type_Weapon)); iter != end(); ++iter)
    {
        const ESM::Weapon* esmWeapon = iter->get<ESM::Weapon>()->mBase;

        if (esmWeapon->mData.mType == ESM::Weapon::Arrow)
        {
            if (esmWeapon->mData.mChop[1] >= arrowMax)
            {
                arrowMax = esmWeapon->mData.mChop[1];
                arrow = iter;
            }
        }
        else if (esmWeapon->mData.mType == ESM::Weapon::Bolt)
        {
            if (esmWeapon->mData.mChop[1] >= boltMax)
            {
                boltMax = esmWeapon->mData.mChop[1];
                bolt = iter;
            }
        }
    }

    // rate weapon
    for (int i = 0; i < static_cast<int>(weaponSkillsLength); ++i)
    {
        float max = 0;
        int maxWeaponSkill = -1;

        for (int j = 0; j < static_cast<int>(weaponSkillsLength); ++j)
        {
            float skillValue = actor.getClass().getSkill(actor, weaponSkills[j]);
            if (skillValue > max && !weaponSkillVisited[j])
            {
                max = skillValue;
                maxWeaponSkill = j;
            }
        }

        if (maxWeaponSkill == -1)
            break;

        max = 0;
        ContainerStoreIterator weapon(end());

        for (ContainerStoreIterator iter(begin(ContainerStore::Type_Weapon)); iter != end(); ++iter)
        {
            const ESM::Weapon* esmWeapon = iter->get<ESM::Weapon>()->mBase;

            if (MWMechanics::getWeaponType(esmWeapon->mData.mType)->mWeaponClass == ESM::WeaponType::Ammo)
                continue;

            if (iter->getClass().getEquipmentSkill(*iter) == weaponSkills[maxWeaponSkill])
            {
                if (esmWeapon->mData.mChop[1] >= max)
                {
                    max = esmWeapon->mData.mChop[1];
                    weapon = iter;
                }

                if (esmWeapon->mData.mSlash[1] >= max)
                {
                    max = esmWeapon->mData.mSlash[1];
                    weapon = iter;
                }

                if (esmWeapon->mData.mThrust[1] >= max)
                {
                    max = esmWeapon->mData.mThrust[1];
                    weapon = iter;
                }
            }
        }

        if (weapon != end() && weapon->getClass().canBeEquipped(*weapon, actor).first)
        {
            // Do not equip ranged weapons, if there is no suitable ammo
            bool hasAmmo = true;
            const MWWorld::LiveCellRef<ESM::Weapon>* ref = weapon->get<ESM::Weapon>();
            int type = ref->mBase->mData.mType;
            int ammotype = MWMechanics::getWeaponType(type)->mAmmoType;
            if (ammotype == ESM::Weapon::Arrow)
            {
                if (arrow == end())
                    hasAmmo = false;
                else
                    slots[Slot_Ammunition] = arrow;
            }
            else if (ammotype == ESM::Weapon::Bolt)
            {
                if (bolt == end())
                    hasAmmo = false;
                else
                    slots[Slot_Ammunition] = bolt;
            }

            if (hasAmmo)
            {
                std::pair<std::vector<int>, bool> itemsSlots = weapon->getClass().getEquipmentSlots(*weapon);

                if (!itemsSlots.first.empty())
                {
                    if (!itemsSlots.second)
                    {
                        if (weapon->getCellRef().getCount() > 1)
                        {
                            unstack(*weapon);
                        }
                    }

                    int slot = itemsSlots.first.front();
                    slots[slot] = weapon;

                    if (ammotype == ESM::Weapon::None)
                        slots[Slot_Ammunition] = end();
                }

                break;
            }
        }

        weaponSkillVisited[maxWeaponSkill] = true;
    }
}

void MWWorld::InventoryStore::autoEquipArmor(TSlots& slots)
{
    const Ptr& actor = getPtr();

    // Creatures only want shields and don't benefit from armor rating or unarmored skill
    const MWWorld::Class& actorCls = actor.getClass();
    const bool actorIsNpc = actorCls.isNpc();

    int equipmentTypes = ContainerStore::Type_Armor;
    float unarmoredRating = 0.f;
    if (actorIsNpc)
    {
        equipmentTypes |= ContainerStore::Type_Clothing;
        const auto& store = MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>();
        const float fUnarmoredBase1 = store.find("fUnarmoredBase1")->mValue.getFloat();
        const float fUnarmoredBase2 = store.find("fUnarmoredBase2")->mValue.getFloat();
        const float unarmoredSkill = actorCls.getSkill(actor, ESM::Skill::Unarmored);
        unarmoredRating = (fUnarmoredBase1 * unarmoredSkill) * (fUnarmoredBase2 * unarmoredSkill);
        unarmoredRating = std::max(unarmoredRating, 0.f);
    }

    for (ContainerStoreIterator iter(begin(equipmentTypes)); iter != end(); ++iter)
    {
        Ptr test = *iter;
        const MWWorld::Class& testCls = test.getClass();
        const bool isArmor = iter.getType() == ContainerStore::Type_Armor;

        // Discard armor that is worse than unarmored for NPCs and non-shields for creatures
        if (isArmor)
        {
            if (actorIsNpc)
            {
                if (testCls.getSkillAdjustedArmorRating(test, actor) <= unarmoredRating)
                    continue;
            }
            else
            {
                if (test.get<ESM::Armor>()->mBase->mData.mType != ESM::Armor::Shield)
                    continue;
            }
        }

        // Don't equip the item if it cannot be equipped
        if (testCls.canBeEquipped(test, actor).first == 0)
            continue;

        const auto [itemSlots, canStack] = testCls.getEquipmentSlots(test);

        // checking if current item pointed by iter can be equipped
        for (const int slot : itemSlots)
        {
            // check if slot may require swapping if current item is more valuable
            if (slots.at(slot) != end())
            {
                Ptr old = *slots.at(slot);
                const MWWorld::Class& oldCls = old.getClass();
                unsigned int oldType = old.getType();

                if (!isArmor)
                {
                    // Armor should replace clothing and weapons, but clothing should only replace clothing
                    if (oldType != ESM::Clothing::sRecordId)
                        continue;

                    // If the left ring slot is filled, don't swap if the right ring is cheaper
                    if (slot == Slot_LeftRing)
                    {
                        if (slots.at(Slot_RightRing) == end())
                            continue;

                        Ptr rightRing = *slots.at(Slot_RightRing);
                        if (rightRing.getClass().getValue(rightRing) <= oldCls.getValue(old))
                            continue;
                    }

                    if (testCls.getValue(test) <= oldCls.getValue(old))
                        continue;
                }
                else if (oldType == ESM::Armor::sRecordId)
                {
                    const int32_t oldArmorType = old.get<ESM::Armor>()->mBase->mData.mType;
                    const int32_t newArmorType = test.get<ESM::Armor>()->mBase->mData.mType;
                    if (oldArmorType == newArmorType)
                    {
                        // For NPCs, compare armor rating; for creatures, compare condition
                        if (actorIsNpc)
                        {
                            const float rating = testCls.getSkillAdjustedArmorRating(test, actor);
                            const float oldRating = oldCls.getSkillAdjustedArmorRating(old, actor);
                            if (rating <= oldRating)
                                continue;
                        }
                        else
                        {
                            if (testCls.getItemHealth(test) <= oldCls.getItemHealth(old))
                                continue;
                        }
                    }
                    else if (oldArmorType < newArmorType)
                        continue;
                }
            }

            // unstack the item if required
            if (!canStack && test.getCellRef().getCount() > 1)
            {
                unstack(test);
            }

            // if we are here it means item can be equipped or swapped
            slots[slot] = iter;
            break;
        }
    }
}

void MWWorld::InventoryStore::autoEquip()
{
    TSlots slots;
    initSlots(slots);

    // Disable model update during auto-equip
    mUpdatesEnabled = false;

    // Autoequip clothing, armor and weapons.
    // Equipping lights is handled in Actors::updateEquippedLight based on environment light.
    autoEquipWeapon(slots);
    autoEquipArmor(slots);

    bool changed = false;

    for (std::size_t i = 0; i < slots.size(); ++i)
    {
        if (slots[i] != mSlots[i])
        {
            changed = true;
            break;
        }
    }
    mUpdatesEnabled = true;

    if (changed)
    {
        mSlots.swap(slots);
        fireEquipmentChangedEvent();
        flagAsModified();
    }
}

MWWorld::ContainerStoreIterator MWWorld::InventoryStore::getPreferredShield()
{
    TSlots slots;
    initSlots(slots);
    autoEquipArmor(slots);
    return slots[Slot_CarriedLeft];
}

bool MWWorld::InventoryStore::stacks(const ConstPtr& ptr1, const ConstPtr& ptr2) const
{
    bool canStack = MWWorld::ContainerStore::stacks(ptr1, ptr2);
    if (!canStack)
        return false;

    // don't stack if either item is currently equipped
    for (TSlots::const_iterator iter(mSlots.begin()); iter != mSlots.end(); ++iter)
    {
        if (*iter != end() && (ptr1 == **iter || ptr2 == **iter))
        {
            bool stackWhenEquipped = (*iter)->getClass().getEquipmentSlots(**iter).second;
            if (!stackWhenEquipped)
                return false;
        }
    }

    return true;
}

int MWWorld::InventoryStore::remove(const Ptr& item, int count, bool equipReplacement, bool resolve)
{
    int retCount = ContainerStore::remove(item, count, equipReplacement, resolve);

    bool wasEquipped = false;
    if (!item.getCellRef().getCount())
    {
        for (int slot = 0; slot < MWWorld::InventoryStore::Slots; ++slot)
        {
            if (mSlots[slot] == end())
                continue;

            if (*mSlots[slot] == item)
            {
                unequipSlot(slot);
                wasEquipped = true;
                break;
            }
        }
    }

    // If an armor/clothing item is removed, try to find a replacement,
    // but not for the player nor werewolves, and not if the RemoveItem script command
    // was used (equipReplacement is false)
    const Ptr& actor = getPtr();
    if (equipReplacement && wasEquipped && (actor != MWMechanics::getPlayer()) && actor.getClass().isNpc()
        && !actor.getClass().getNpcStats(actor).isWerewolf())
    {
        auto type = item.getType();
        if (type == ESM::Armor::sRecordId || type == ESM::Clothing::sRecordId)
            autoEquip();
    }

    if (mListener)
        mListener->itemRemoved(item, retCount);
    MWBase::Environment::get().getWindowManager()->inventoryUpdated(actor);

    return retCount;
}

MWWorld::ContainerStoreIterator MWWorld::InventoryStore::unequipSlot(int slot, bool applyUpdates)
{
    if (slot < 0 || slot >= static_cast<int>(mSlots.size()))
        throw std::runtime_error("slot number out of range");

    ContainerStoreIterator it = mSlots[slot];

    if (it != end())
    {
        ContainerStoreIterator retval = it;

        // empty this slot
        mSlots[slot] = end();

        if (it->getCellRef().getCount())
        {
            retval = restack(*it);

            if (getPtr() == MWMechanics::getPlayer())
            {
                // Unset OnPCEquip Variable on item's script, if it has a script with that variable declared
                const ESM::RefId& script = it->getClass().getScript(*it);
                if (!script.empty())
                    (*it).getRefData().getLocals().setVarByInt(script, "onpcequip", 0);
            }

            if ((mSelectedEnchantItem != end()) && (mSelectedEnchantItem == it))
            {
                mSelectedEnchantItem = end();
            }
        }

        if (applyUpdates)
        {
            fireEquipmentChangedEvent();
        }

        return retval;
    }

    return it;
}

MWWorld::ContainerStoreIterator MWWorld::InventoryStore::unequipItem(const MWWorld::Ptr& item)
{
    for (int slot = 0; slot < MWWorld::InventoryStore::Slots; ++slot)
    {
        MWWorld::ContainerStoreIterator equipped = getSlot(slot);
        if (equipped != end() && *equipped == item)
            return unequipSlot(slot);
    }

    throw std::runtime_error("attempt to unequip an item that is not currently equipped");
}

MWWorld::ContainerStoreIterator MWWorld::InventoryStore::unequipItemQuantity(const Ptr& item, int count)
{
    if (!isEquipped(item))
        throw std::runtime_error("attempt to unequip an item that is not currently equipped");
    if (count <= 0)
        throw std::runtime_error("attempt to unequip nothing (count <= 0)");
    if (count > item.getCellRef().getCount())
        throw std::runtime_error("attempt to unequip more items than equipped");

    if (count == item.getCellRef().getCount())
        return unequipItem(item);

    // Move items to an existing stack if possible, otherwise split count items out into a new stack.
    // Moving counts manually here, since ContainerStore's restack can't target unequipped stacks.
    for (MWWorld::ContainerStoreIterator iter(begin()); iter != end(); ++iter)
    {
        if (stacks(*iter, item) && !isEquipped(*iter))
        {
            iter->getCellRef().setCount(addItems(iter->getCellRef().getCount(false), count));
            item.getCellRef().setCount(subtractItems(item.getCellRef().getCount(false), count));
            return iter;
        }
    }

    return unstack(item, item.getCellRef().getCount() - count);
}

MWWorld::InventoryStoreListener* MWWorld::InventoryStore::getInvListener() const
{
    return mInventoryListener;
}

void MWWorld::InventoryStore::setInvListener(InventoryStoreListener* listener)
{
    mInventoryListener = listener;
}

void MWWorld::InventoryStore::fireEquipmentChangedEvent()
{
    if (!mUpdatesEnabled)
        return;
    if (mInventoryListener)
        mInventoryListener->equipmentChanged();

    // if player, update inventory window
    /*
    if (mActor == MWMechanics::getPlayer())
    {
        MWBase::Environment::get().getWindowManager()->getInventoryWindow()->updateItemView();
    }
    */
}

void MWWorld::InventoryStore::clear()
{
    mPreparedAmmunitionIdentity.reset();
    mSlots.clear();
    initSlots(mSlots);
    ContainerStore::clear();
}

bool MWWorld::InventoryStore::isEquipped(const MWWorld::ConstPtr& item)
{
    for (int i = 0; i < MWWorld::InventoryStore::Slots; ++i)
    {
        if (getSlot(i) != end() && *getSlot(i) == item)
            return true;
    }
    return false;
}

bool MWWorld::InventoryStore::isEquipped(const ESM::RefId& id)
{
    for (int i = 0; i < MWWorld::InventoryStore::Slots; ++i)
    {
        if (getSlot(i) != end() && getSlot(i)->getCellRef().getRefId() == id)
            return true;
    }
    return false;
}

bool MWWorld::InventoryStore::isFirstEquip()
{
    bool first = mFirstAutoEquip;
    mFirstAutoEquip = false;
    return first;
}

struct MWWorld::InventoryStore::PreparedItemRemoval::Impl
{
    struct Stack { Ptr mItem; std::int32_t mBefore; std::int32_t mAfter; int mRemoved = 0; };
    InventoryStore* mOwner = nullptr;
    std::weak_ptr<const char> mIdentity;
    ESM::RefId mBase;
    std::vector<Stack> mStacks;
    std::array<Ptr, Slots> mSlots;
    int mCount = 0;
    bool mEquipmentChanged = false;
    bool mCommitted = false;
    bool mNotified = false;
};

MWWorld::InventoryStore::PreparedItemRemoval::PreparedItemRemoval(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl)) {}
MWWorld::InventoryStore::PreparedItemRemoval::~PreparedItemRemoval() = default;
int MWWorld::InventoryStore::PreparedItemRemoval::getCount() const noexcept { return mImpl->mCount; }
bool MWWorld::InventoryStore::PreparedItemRemoval::depletesEquipment() const noexcept
{ return mImpl->mEquipmentChanged; }
bool MWWorld::InventoryStore::PreparedItemRemoval::ownerIsCurrent() const noexcept
{ return !mImpl->mIdentity.expired(); }

std::unique_ptr<MWWorld::InventoryStore::PreparedItemRemoval>
MWWorld::InventoryStore::prepareItemRemoval(const ESM::RefId& base, int count)
{
    if (base.empty() || count <= 0)
        throw std::invalid_argument("prepared native removal requires a base and positive count");
    auto value = std::make_unique<PreparedItemRemoval::Impl>();
    value->mOwner = this;
    value->mBase = base;
    value->mIdentity = prepareStorageIdentity();
    std::uint32_t available = 0;
    for (auto it = begin(); it != end(); ++it)
    {
        if (it->getCellRef().getRefId() != base) continue;
        const auto* ref = std::get_if<ESM::CellRef>(&it->getCellRef().mCellRef.mVariant);
        if (!ref || it->getContainerStore() != this)
            throw std::invalid_argument("native removal requires projected inventory instances");
        const auto raw = static_cast<std::uint32_t>(ref->mCount);
        available += ref->mCount < 0 ? 0u - raw : raw;
        value->mStacks.push_back({*it, ref->mCount, ref->mCount});
    }
    // Preserve the audited native signed-int32 count composition. INT_MIN's
    // magnitude remains negative, so RemoveItem must do nothing.
    constexpr auto sign = std::uint32_t(1) << 31;
    const auto magnitude = available >= sign ? 0u - available : available;
    int remaining = available == sign ? 0 : std::min(count, static_cast<int>(magnitude));
    value->mCount = remaining;
    for (auto& stack : value->mStacks)
    {
        const auto raw = static_cast<std::uint32_t>(stack.mBefore);
        const auto size = stack.mBefore < 0 ? 0u - raw : raw;
        stack.mRemoved = static_cast<int>(std::min(size, static_cast<std::uint32_t>(remaining)));
        const auto after = size - static_cast<std::uint32_t>(stack.mRemoved);
        stack.mAfter = std::bit_cast<std::int32_t>(stack.mBefore < 0 ? 0u - after : after);
        remaining -= stack.mRemoved;
    }
    for (int slot = 0; slot < Slots; ++slot)
    {
        if (mSlots[slot] == end()) continue;
        value->mSlots[slot] = *mSlots[slot];
        for (const auto& stack : value->mStacks)
            if (stack.mItem == value->mSlots[slot] && stack.mRemoved && stack.mAfter == 0)
                value->mEquipmentChanged = true;
    }
    return std::unique_ptr<PreparedItemRemoval>(new PreparedItemRemoval(std::move(value)));
}

bool MWWorld::InventoryStore::PreparedItemRemoval::isValid() const
{
    if (!ownerIsCurrent() || mImpl->mCommitted) return false;
    auto& owner = *mImpl->mOwner;
    std::size_t matched = 0;
    for (auto it = owner.begin(); it != owner.end(); ++it)
    {
        if (it->getCellRef().getRefId() != mImpl->mBase) continue;
        if (matched == mImpl->mStacks.size()) return false;
        const auto& stack = mImpl->mStacks[matched++];
        if (*it != stack.mItem || it->getCellRef().getCount(false) != stack.mBefore) return false;
    }
    if (matched != mImpl->mStacks.size()) return false;
    for (int slot = 0; slot < Slots; ++slot)
    {
        const Ptr current = owner.mSlots[slot] == owner.end() ? Ptr{} : *owner.mSlots[slot];
        if (current != mImpl->mSlots[slot]) return false;
    }
    return true;
}

bool MWWorld::InventoryStore::PreparedItemRemoval::commit()
{
    if (!isValid()) return false;
    auto& owner = *mImpl->mOwner;
    for (const auto& stack : mImpl->mStacks)
    {
        if (!stack.mRemoved) continue;
        auto& cell = stack.mItem.getCellRef();
        // Do not invoke setCount(0)'s script cleanup until the entire batch and
        // equipment changes are visible. These projected fields allocate nothing.
        std::get<ESM::CellRef>(cell.mCellRef.mVariant).mCount = stack.mAfter;
        cell.mChanged = true;
        if (stack.mAfter != 0) continue;
        if (owner.mSelectedEnchantItem != owner.end() && *owner.mSelectedEnchantItem == stack.mItem)
            owner.mSelectedEnchantItem = owner.end();
        for (auto& slot : owner.mSlots)
            if (slot != owner.end() && *slot == stack.mItem) slot = owner.end();
    }
    if (mImpl->mCount) owner.ContainerStore::flagAsModified();
    mImpl->mCommitted = true;
    return true;
}

bool MWWorld::InventoryStore::PreparedItemRemoval::notify()
{
    if (!ownerIsCurrent() || !mImpl->mCommitted || mImpl->mNotified) return false;
    mImpl->mNotified = true; // A callback cannot replay this batch.
    std::exception_ptr failure;
    const auto observe = [&](auto&& callback) {
        try { callback(); }
        catch (...) { if (!failure) failure = std::current_exception(); }
    };
    for (const auto& stack : mImpl->mStacks)
    {
        if (!ownerIsCurrent()) break;
        if (stack.mRemoved && !stack.mAfter)
            observe([&] { MWBase::Environment::get().getWorld()->removeRefScript(&stack.mItem.getCellRef()); });
    }
    if (ownerIsCurrent() && mImpl->mEquipmentChanged)
        observe([&] { mImpl->mOwner->fireEquipmentChangedEvent(); });
    for (const auto& stack : mImpl->mStacks)
    {
        if (!ownerIsCurrent()) break;
        if (stack.mRemoved)
            if (auto* listener = mImpl->mOwner->mListener)
                observe([&] { listener->itemRemoved(stack.mItem, stack.mRemoved); });
    }
    // No retained item or owner is dereferenced after its lifetime guard expires.
    if (failure) std::rethrow_exception(failure);
    return true;
}

struct MWWorld::InventoryStore::PreparedAmmunitionDebit::Impl
{
    InventoryStore* mOwner;
    std::shared_ptr<const char> mIdentity;
    Ptr mItem;
    int mCount;
    bool mCommitted = false;
    bool mNotified = false;
};

MWWorld::InventoryStore::PreparedAmmunitionDebit::PreparedAmmunitionDebit(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl))
{
}
MWWorld::InventoryStore::PreparedAmmunitionDebit::~PreparedAmmunitionDebit() = default;
MWWorld::InventoryStore::PreparedAmmunitionDebit::PreparedAmmunitionDebit(PreparedAmmunitionDebit&&) = default;
MWWorld::InventoryStore::PreparedAmmunitionDebit&
MWWorld::InventoryStore::PreparedAmmunitionDebit::operator=(PreparedAmmunitionDebit&&) = default;

std::unique_ptr<MWWorld::InventoryStore::PreparedAmmunitionDebit>
MWWorld::InventoryStore::prepareAmmunitionDebit()
{
    const auto slot = getSlot(Slot_Ammunition);
    if (slot == end() || slot.getType() != ContainerStore::Type_Weapon)
        throw std::invalid_argument("prepared ammunition debit requires an equipped ammunition instance");
    const auto item = *slot;
    const auto* ref = std::get_if<ESM::CellRef>(&item.getCellRef().mCellRef.mVariant);
    if (item.getContainerStore() != this || !ref || ref->mCount <= 0)
        throw std::invalid_argument("prepared ammunition debit requires a positive projected inventory count");
    auto impl = std::make_unique<PreparedAmmunitionDebit::Impl>();
    if (!mPreparedAmmunitionIdentity) mPreparedAmmunitionIdentity = std::make_shared<const char>();
    impl->mOwner = this; impl->mIdentity = mPreparedAmmunitionIdentity;
    impl->mItem = item; impl->mCount = ref->mCount;
    return std::unique_ptr<PreparedAmmunitionDebit>(new PreparedAmmunitionDebit(std::move(impl)));
}

bool MWWorld::InventoryStore::validatePreparedAmmunitionDebit(const PreparedAmmunitionDebit& debit) const noexcept
{
    const auto* value = debit.mImpl.get();
    // Check lifetime identity before dereferencing a retained item pointer.
    if (!value || value->mOwner != this || value->mIdentity != mPreparedAmmunitionIdentity || value->mCommitted)
        return false;
    const auto& slot = mSlots[Slot_Ammunition];
    if (slot == end() || slot.getType() != ContainerStore::Type_Weapon || *slot != value->mItem)
        return false;
    const auto* ref = std::get_if<ESM::CellRef>(&value->mItem.getCellRef().mCellRef.mVariant);
    return ref && ref->mCount == value->mCount && value->mCount > 0;
}

bool MWWorld::InventoryStore::commitPreparedAmmunitionDebit(PreparedAmmunitionDebit& debit) noexcept
{
    if (!validatePreparedAmmunitionDebit(debit)) return false;
    auto& value = *debit.mImpl;
    auto& cell = value.mItem.getCellRef();
    // Validation proves the variant and exact instance. No script/observer
    // can run between the count/slot/cache writes and the caller's resources.
    std::get<ESM::CellRef>(cell.mCellRef.mVariant).mCount = value.mCount - 1;
    cell.mChanged = true;
    if (value.mCount == 1)
    {
        if (mSelectedEnchantItem == mSlots[Slot_Ammunition]) mSelectedEnchantItem = end();
        mSlots[Slot_Ammunition] = end();
    }
    ContainerStore::flagAsModified();
    value.mCommitted = true;
    return true;
}

bool MWWorld::InventoryStore::notifyPreparedAmmunitionDebit(PreparedAmmunitionDebit& debit)
{
    auto* value = debit.mImpl.get();
    if (!value || value->mOwner != this || value->mIdentity != mPreparedAmmunitionIdentity
        || !value->mCommitted || value->mNotified)
        return false;
    value->mNotified = true;
    if (value->mCount == 1)
    {
        MWBase::Environment::get().getWorld()->removeRefScript(&value->mItem.getCellRef());
        if (value->mIdentity != mPreparedAmmunitionIdentity) return true;
        fireEquipmentChangedEvent();
    }
    // An observer can replace or clear the inventory. Do not touch its former
    // item pointer after that replacement.
    if (value->mIdentity != mPreparedAmmunitionIdentity) return true;
    if (mListener) mListener->itemRemoved(value->mItem, 1);
    return true;
}

struct MWWorld::InventoryStore::PreparedEquippedWeaponRemoval::Impl
{
    InventoryStore* mOwner = nullptr;
    std::shared_ptr<const char> mIdentity;
    Ptr mItem;
    bool mCommitted = false;
    bool mNotified = false;
};

MWWorld::InventoryStore::PreparedEquippedWeaponRemoval::PreparedEquippedWeaponRemoval(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl)) {}
MWWorld::InventoryStore::PreparedEquippedWeaponRemoval::~PreparedEquippedWeaponRemoval() = default;
MWWorld::InventoryStore::PreparedEquippedWeaponRemoval::PreparedEquippedWeaponRemoval(
    PreparedEquippedWeaponRemoval&&) noexcept = default;
MWWorld::InventoryStore::PreparedEquippedWeaponRemoval&
MWWorld::InventoryStore::PreparedEquippedWeaponRemoval::operator=(PreparedEquippedWeaponRemoval&&) noexcept = default;

std::unique_ptr<MWWorld::InventoryStore::PreparedEquippedWeaponRemoval>
MWWorld::InventoryStore::prepareEquippedWeaponRemoval()
{
    const auto slot = getSlot(Slot_CarriedRight);
    if (slot == end() || slot.getType() != ContainerStore::Type_Weapon)
        throw std::invalid_argument("prepared weapon removal requires an equipped weapon");
    const auto item = *slot;
    const auto* ref = std::get_if<ESM::CellRef>(&item.getCellRef().mCellRef.mVariant);
    if (item.getContainerStore() != this || !ref || ref->mCount != 1)
        throw std::invalid_argument("prepared weapon removal requires one projected instance");
    for (int i = 0; i < Slots; ++i)
        if (i != Slot_CarriedRight && mSlots[i] != end() && *mSlots[i] == item)
            throw std::invalid_argument("prepared weapon removal cannot clear another slot");
    auto impl = std::make_unique<PreparedEquippedWeaponRemoval::Impl>();
    if (!mPreparedAmmunitionIdentity) mPreparedAmmunitionIdentity = std::make_shared<const char>();
    impl->mOwner = this;
    impl->mIdentity = mPreparedAmmunitionIdentity;
    impl->mItem = item;
    return std::unique_ptr<PreparedEquippedWeaponRemoval>(new PreparedEquippedWeaponRemoval(std::move(impl)));
}

bool MWWorld::InventoryStore::validatePreparedEquippedWeaponRemoval(
    const PreparedEquippedWeaponRemoval& removal) const noexcept
{
    const auto* value = removal.mImpl.get();
    if (!value || value->mOwner != this || value->mIdentity != mPreparedAmmunitionIdentity || value->mCommitted)
        return false;
    const auto& slot = mSlots[Slot_CarriedRight];
    if (slot == end() || slot.getType() != ContainerStore::Type_Weapon || *slot != value->mItem)
        return false;
    const auto* ref = std::get_if<ESM::CellRef>(&value->mItem.getCellRef().mCellRef.mVariant);
    if (!ref || ref->mCount != 1) return false;
    for (int i = 0; i < Slots; ++i)
        if (i != Slot_CarriedRight && mSlots[i] != end() && *mSlots[i] == value->mItem)
            return false;
    return true;
}

bool MWWorld::InventoryStore::commitPreparedEquippedWeaponRemoval(PreparedEquippedWeaponRemoval& removal) noexcept
{
    if (!validatePreparedEquippedWeaponRemoval(removal)) return false;
    auto& value = *removal.mImpl;
    auto& cell = value.mItem.getCellRef();
    // Caller has already committed final wear. Change only count/slot/caches;
    // swapping an earlier CellRef snapshot here would resurrect or undo wear.
    std::get<ESM::CellRef>(cell.mCellRef.mVariant).mCount = 0;
    cell.mChanged = true;
    if (mSelectedEnchantItem == mSlots[Slot_CarriedRight]) mSelectedEnchantItem = end();
    mSlots[Slot_CarriedRight] = end();
    ContainerStore::flagAsModified();
    value.mCommitted = true;
    return true;
}

bool MWWorld::InventoryStore::notifyPreparedEquippedWeaponRemoval(PreparedEquippedWeaponRemoval& removal)
{
    auto* value = removal.mImpl.get();
    if (!value || value->mOwner != this || value->mIdentity != mPreparedAmmunitionIdentity
        || !value->mCommitted || value->mNotified)
        return false;
    value->mNotified = true;
    MWBase::Environment::get().getWorld()->removeRefScript(&value->mItem.getCellRef());
    if (value->mIdentity != mPreparedAmmunitionIdentity) return true;
    fireEquipmentChangedEvent();
    if (value->mIdentity != mPreparedAmmunitionIdentity) return true;
    if (mListener) mListener->itemRemoved(value->mItem, 1);
    return true;
}

struct MWWorld::InventoryStore::PreparedUnequip::Impl
{
    InventoryStore* mOwner;
    std::shared_ptr<const char> mIdentity;
    Ptr mItem;
    int mSlot;
    int mCount;
    bool mCommitted = false;
    bool mNotified = false;
};

MWWorld::InventoryStore::PreparedUnequip::PreparedUnequip(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl))
{
}
MWWorld::InventoryStore::PreparedUnequip::~PreparedUnequip() = default;
MWWorld::InventoryStore::PreparedUnequip::PreparedUnequip(PreparedUnequip&&) noexcept = default;
MWWorld::InventoryStore::PreparedUnequip&
MWWorld::InventoryStore::PreparedUnequip::operator=(PreparedUnequip&&) noexcept = default;

std::unique_ptr<MWWorld::InventoryStore::PreparedUnequip>
MWWorld::InventoryStore::prepareUnequip(int slot)
{
    if (slot < 0 || slot >= static_cast<int>(mSlots.size()) || mSlots[slot] == end())
        throw std::invalid_argument("prepared unequip requires an occupied valid slot");
    const auto item = *mSlots[slot];
    const auto* reference = std::get_if<ESM::CellRef>(&item.getCellRef().mCellRef.mVariant);
    if (item.getContainerStore() != this || !reference || reference->mCount <= 0)
        throw std::invalid_argument("prepared unequip requires a positive projected instance");
    auto impl = std::make_unique<PreparedUnequip::Impl>();
    if (!mPreparedAmmunitionIdentity) mPreparedAmmunitionIdentity = std::make_shared<const char>();
    impl->mOwner = this;
    impl->mIdentity = mPreparedAmmunitionIdentity;
    impl->mItem = item;
    impl->mSlot = slot;
    impl->mCount = reference->mCount;
    return std::unique_ptr<PreparedUnequip>(new PreparedUnequip(std::move(impl)));
}

bool MWWorld::InventoryStore::validatePreparedUnequip(const PreparedUnequip& change) const noexcept
{
    const auto* value = change.mImpl.get();
    if (!value || value->mOwner != this || value->mIdentity != mPreparedAmmunitionIdentity || value->mCommitted)
        return false;
    const auto& slot = mSlots[value->mSlot];
    if (slot == end() || *slot != value->mItem)
        return false;
    const auto* reference = std::get_if<ESM::CellRef>(&value->mItem.getCellRef().mCellRef.mVariant);
    return reference && reference->mCount == value->mCount && value->mCount > 0;
}

bool MWWorld::InventoryStore::commitPreparedUnequip(PreparedUnequip& change) noexcept
{
    if (!validatePreparedUnequip(change)) return false;
    auto& value = *change.mImpl;
    if (mSelectedEnchantItem == mSlots[value.mSlot]) mSelectedEnchantItem = end();
    mSlots[value.mSlot] = end();
    ContainerStore::flagAsModified();
    value.mCommitted = true;
    return true;
}

bool MWWorld::InventoryStore::notifyPreparedUnequip(PreparedUnequip& change)
{
    auto* value = change.mImpl.get();
    if (!value || value->mOwner != this || value->mIdentity != mPreparedAmmunitionIdentity
        || !value->mCommitted || value->mNotified)
        return false;
    // Mark before any operation that can throw or invoke an observer.
    value->mNotified = true;
    if (value->mItem.getCellRef().getCount())
    {
        restack(value->mItem);
        if (value->mIdentity != mPreparedAmmunitionIdentity) return true;
        if (getPtr() == MWMechanics::getPlayer())
        {
            const ESM::RefId& script = value->mItem.getClass().getScript(value->mItem);
            if (!script.empty())
                value->mItem.getRefData().getLocals().setVarByInt(script, "onpcequip", 0);
        }
    }
    if (value->mIdentity != mPreparedAmmunitionIdentity) return true;
    fireEquipmentChangedEvent();
    return true;
}
