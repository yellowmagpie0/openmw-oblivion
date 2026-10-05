#ifndef GAME_MWWORLD_INVENTORYSTORE_H
#define GAME_MWWORLD_INVENTORYSTORE_H

#include "containerstore.hpp"

namespace ESM
{
    struct MagicEffect;
}

namespace MWMechanics
{
    class NpcStats;
}

namespace MWWorld
{
    class InventoryStoreListener
    {
    public:
        /**
         * Fired when items are equipped or unequipped
         */
        virtual void equipmentChanged() {}

        virtual ~InventoryStoreListener() = default;
    };

    ///< \brief Variant of the ContainerStore for NPCs
    class InventoryStore : public ContainerStore
    {
    public:
        static constexpr int Slot_Helmet = 0;
        static constexpr int Slot_Cuirass = 1;
        static constexpr int Slot_Greaves = 2;
        static constexpr int Slot_LeftPauldron = 3;
        static constexpr int Slot_RightPauldron = 4;
        static constexpr int Slot_LeftGauntlet = 5;
        static constexpr int Slot_RightGauntlet = 6;
        static constexpr int Slot_Boots = 7;
        static constexpr int Slot_Shirt = 8;
        static constexpr int Slot_Pants = 9;
        static constexpr int Slot_Skirt = 10;
        static constexpr int Slot_Robe = 11;
        static constexpr int Slot_LeftRing = 12;
        static constexpr int Slot_RightRing = 13;
        static constexpr int Slot_Amulet = 14;
        static constexpr int Slot_Belt = 15;
        static constexpr int Slot_CarriedRight = 16;
        static constexpr int Slot_CarriedLeft = 17;
        static constexpr int Slot_Ammunition = 18;

        static constexpr int Slots = 19;

        static constexpr int Slot_NoSlot = -1;

    private:
        InventoryStoreListener* mInventoryListener = nullptr;

        // Enables updates of magic effects and actor model whenever items are equipped or unequipped.
        // This is disabled during autoequip to avoid excessive updates
        bool mUpdatesEnabled = true;

        bool mFirstAutoEquip = true;

        typedef std::vector<ContainerStoreIterator> TSlots;

        TSlots mSlots;
        std::shared_ptr<const char> mPreparedAmmunitionIdentity;

        void autoEquipWeapon(TSlots& slots);
        void autoEquipArmor(TSlots& slots);

        void copySlots(const InventoryStore& store);

        void initSlots(TSlots& slots);

        void fireEquipmentChangedEvent();

        void storeEquipmentState(
            const MWWorld::LiveCellRefBase& ref, size_t index, ESM::InventoryState& inventory) const override;

    protected:
        void readEquipmentState(
            const MWWorld::ContainerStoreIterator& iter, size_t index, const ESM::InventoryState& inventory) override;

    private:
        ContainerStoreIterator findSlot(int slot) const;

    public:
        // Native RemoveItem: publish the entire bounded request before any
        // equipment/item observer. The standalone token survives owner removal.
        class PreparedItemRemoval
        {
            struct Impl;
            std::unique_ptr<Impl> mImpl;
            explicit PreparedItemRemoval(std::unique_ptr<Impl> impl);
            friend class InventoryStore;
        public:
            ~PreparedItemRemoval();
            PreparedItemRemoval(const PreparedItemRemoval&) = delete;
            PreparedItemRemoval& operator=(const PreparedItemRemoval&) = delete;
            int getCount() const noexcept;
            bool depletesEquipment() const noexcept;
            bool ownerIsCurrent() const noexcept;
            bool isValid() const;
            bool commit();
            bool notify();
        };
        std::unique_ptr<PreparedItemRemoval> prepareItemRemoval(const ESM::RefId& item, int count);

        class PreparedAmmunitionDebit
        {
            struct Impl;
            std::unique_ptr<Impl> mImpl;
            explicit PreparedAmmunitionDebit(std::unique_ptr<Impl> impl);
            friend class InventoryStore;
        public:
            ~PreparedAmmunitionDebit();
            PreparedAmmunitionDebit(PreparedAmmunitionDebit&&);
            PreparedAmmunitionDebit& operator=(PreparedAmmunitionDebit&&);
            PreparedAmmunitionDebit(const PreparedAmmunitionDebit&) = delete;
            PreparedAmmunitionDebit& operator=(const PreparedAmmunitionDebit&) = delete;
        };
        // One actual equipped ammunition instance. Preparation/cancellation
        // do not debit. Publication is allocation/callback-free and once-only;
        // owner replacement, slot replacement or changed count reject it.
        class PreparedEquippedWeaponRemoval
        {
            struct Impl;
            std::unique_ptr<Impl> mImpl;
            explicit PreparedEquippedWeaponRemoval(std::unique_ptr<Impl> impl);
            friend class InventoryStore;
        public:
            ~PreparedEquippedWeaponRemoval();
            PreparedEquippedWeaponRemoval(PreparedEquippedWeaponRemoval&&) noexcept;
            PreparedEquippedWeaponRemoval& operator=(PreparedEquippedWeaponRemoval&&) noexcept;
            PreparedEquippedWeaponRemoval(const PreparedEquippedWeaponRemoval&) = delete;
            PreparedEquippedWeaponRemoval& operator=(const PreparedEquippedWeaponRemoval&) = delete;
        };

        // Remove one exact carried-right weapon instance after its replacement
        // world reference, rendering and physics have been admitted. The caller
        // validates item extras and commits wear before this callback-free debit.
        // Removal preserves the item extras and all unrelated inventory pointers.
        std::unique_ptr<PreparedEquippedWeaponRemoval> prepareEquippedWeaponRemoval();
        bool validatePreparedEquippedWeaponRemoval(const PreparedEquippedWeaponRemoval& removal) const noexcept;
        bool commitPreparedEquippedWeaponRemoval(PreparedEquippedWeaponRemoval& removal) noexcept;
        bool notifyPreparedEquippedWeaponRemoval(PreparedEquippedWeaponRemoval& removal);

        class PreparedUnequip
        {
            struct Impl;
            std::unique_ptr<Impl> mImpl;
            explicit PreparedUnequip(std::unique_ptr<Impl> impl);
            friend class InventoryStore;
        public:
            ~PreparedUnequip();
            PreparedUnequip(PreparedUnequip&&) noexcept;
            PreparedUnequip& operator=(PreparedUnequip&&) noexcept;
            PreparedUnequip(const PreparedUnequip&) = delete;
            PreparedUnequip& operator=(const PreparedUnequip&) = delete;
        };

        // Stage exact-instance slot removal for compound resource changes.
        // Commit leaves item quantity unchanged and invokes no observers.
        // The caller prepares condition/resource changes before committing.
        std::unique_ptr<PreparedUnequip> prepareUnequip(int slot);
        bool validatePreparedUnequip(const PreparedUnequip& change) const noexcept;
        bool commitPreparedUnequip(PreparedUnequip& change) noexcept;
        // One-shot deferred restacking/script/equipment notification.
        bool notifyPreparedUnequip(PreparedUnequip& change);

        std::unique_ptr<PreparedAmmunitionDebit> prepareAmmunitionDebit();
        bool validatePreparedAmmunitionDebit(const PreparedAmmunitionDebit& debit) const noexcept;
        bool commitPreparedAmmunitionDebit(PreparedAmmunitionDebit& debit) noexcept;
        // After the complete resource/projectile release: script, equipment
        // and item observers run once. Caller owns the inventory UI refresh.
        // Observer exceptions leave publication committed and cannot replay.
        bool notifyPreparedAmmunitionDebit(PreparedAmmunitionDebit& debit);

        InventoryStore();
        InventoryStore(const InventoryStore& store);
        InventoryStore(InventoryStore&& store);

        InventoryStore& operator=(const InventoryStore& store);
        InventoryStore& operator=(InventoryStore&& store);

        // Publish prebuilt contents without allocation or callbacks, retaining
        // each store's owner/listeners. Caller must prepare pointer registration
        // for the new containers first. External iterators must be reacquired.
        void swapPreparedContents(InventoryStore& other) noexcept;

        std::unique_ptr<ContainerStore> clone() override
        {
            auto res = std::make_unique<InventoryStore>(*this);
            res->updateRefNums();
            return res;
        }

        ContainerStoreIterator add(
            const ConstPtr& itemPtr, int count, bool allowAutoEquip = true, bool resolve = true) override;
        ///< Add the item pointed to by \a ptr to this container. (Stacks automatically if needed)
        /// Auto-equip items if specific conditions are fulfilled and allowAutoEquip is true (see the implementation).
        ///
        /// \note The item pointed to is not required to exist beyond this function call.
        ///
        /// \attention Do not add items to an existing stack by increasing the count instead of
        /// calling this function!
        ///
        /// @return if stacking happened, return iterator to the item that was stacked against, otherwise iterator to
        /// the newly inserted item.

        void equip(int slot, const ContainerStoreIterator& iterator);
        ///< \warning \a iterator can not be an end()-iterator, use unequip function instead

        bool isEquipped(const MWWorld::ConstPtr& item);
        bool isEquipped(const ESM::RefId& id);
        ///< Utility function, returns true if the given item is equipped in any slot

        ContainerStoreIterator getSlot(int slot);
        ConstContainerStoreIterator getSlot(int slot) const;

        ContainerStoreIterator getPreferredShield();

        void unequipAll();
        ///< Unequip all currently equipped items.

        void autoEquip();
        ///< Auto equip items according to stats and item value.

        bool stacks(const ConstPtr& ptr1, const ConstPtr& ptr2) const override;
        ///< @return true if the two specified objects can stack with each other

        using ContainerStore::remove;
        int remove(const Ptr& item, int count, bool equipReplacement = 0, bool resolve = true) override;
        ///< Remove \a count item(s) designated by \a item from this inventory.
        ///
        /// @return the number of items actually removed

        ContainerStoreIterator unequipSlot(int slot, bool applyUpdates = true);
        ///< Unequip \a slot.
        ///
        /// @return an iterator to the item that was previously in the slot

        ContainerStoreIterator unequipItem(const Ptr& item);
        ///< Unequip an item identified by its Ptr. An exception is thrown
        /// if the item is not currently equipped.
        ///
        /// @return an iterator to the item that was previously in the slot
        /// (it can be re-stacked so its count may be different than when it
        /// was equipped).

        ContainerStoreIterator unequipItemQuantity(const Ptr& item, int count);
        ///< Unequip a specific quantity of an item identified by its Ptr.
        /// An exception is thrown if the item is not currently equipped,
        /// if count <= 0, or if count > the item stack size.
        ///
        /// @return an iterator to the unequipped items that were previously
        /// in the slot (they can be re-stacked so its count may be different
        /// than the requested count).

        void setInvListener(InventoryStoreListener* listener);
        ///< Set a listener for various events, see \a InventoryStoreListener

        InventoryStoreListener* getInvListener() const;

        void clear() override;
        ///< Empty container.

        bool isFirstEquip();
    };
}

#endif
