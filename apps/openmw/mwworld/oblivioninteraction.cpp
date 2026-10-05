#include "oblivioninteraction.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <exception>
#include <limits>
#include <sstream>
#include <stdexcept>

#include <components/debug/debuglog.hpp>
#include <components/esm/formkey.hpp>
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadappa.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadcont.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadflor.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/inventorymechanics.hpp>
#include <components/esm4/loadkeym.hpp>
#include <components/esm4/loadligh.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadsgst.hpp>
#include <components/esm4/loadslgm.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/runtimestate.hpp>
#include <components/misc/rng.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/oblivioncombat.hpp"

#include "class.hpp"
#include "datetimemanager.hpp"
#include "inventorystore.hpp"
#include "manualref.hpp"
#include "scene.hpp"
#include "oblivionprofileservices.hpp"
#include "worldimp.hpp"

namespace MWWorld
{
    namespace
    {
        bool nativeEquipmentActor(const Ptr& actor)
        {
            if (actor.isEmpty())
                return false;
            if (actor.getType() == ESM::REC_NPC_4)
                return actor.get<ESM4::Npc>()->mBase && actor.get<ESM4::Npc>()->mBase->mIsTES4;
            if (actor.getType() == ESM::REC_CREA4)
                return actor.get<ESM4::Creature>()->mBase && actor.get<ESM4::Creature>()->mBase->mAttackReach.has_value();
            return false;
        }

        template <class T>
        std::string findName(const ESMStore& store, const ESM::RefId& id)
        {
            if (const T* value = store.get<T>().search(id))
                return value->mFullName;
            return {};
        }

        std::string getOblivionItemName(const ESMStore& store, const ESM::RefId& id)
        {
            std::string result;
#define OPENMW_FIND_TES4_NAME(Type) \
    if (result.empty()) \
        result = findName<ESM4::Type>(store, id)
            OPENMW_FIND_TES4_NAME(Ammunition);
            OPENMW_FIND_TES4_NAME(Apparatus);
            OPENMW_FIND_TES4_NAME(Armor);
            OPENMW_FIND_TES4_NAME(Book);
            OPENMW_FIND_TES4_NAME(Clothing);
            OPENMW_FIND_TES4_NAME(Ingredient);
            OPENMW_FIND_TES4_NAME(Key);
            OPENMW_FIND_TES4_NAME(Light);
            OPENMW_FIND_TES4_NAME(MiscItem);
            OPENMW_FIND_TES4_NAME(Potion);
            OPENMW_FIND_TES4_NAME(SigilStone);
            OPENMW_FIND_TES4_NAME(SoulGem);
            OPENMW_FIND_TES4_NAME(Weapon);
#undef OPENMW_FIND_TES4_NAME
            return result.empty() ? id.toDebugString() : result;
        }

        std::string plainBookText(std::string_view input)
        {
            std::string result;
            result.reserve(std::min<std::size_t>(input.size(), 700));
            bool inTag = false;
            for (const char value : input)
            {
                if (value == '<')
                    inTag = true;
                else if (value == '>')
                    inTag = false;
                else if (!inTag && result.size() < 700)
                    result.push_back(value == '\r' ? '\n' : value);
            }
            while (!result.empty() && std::isspace(static_cast<unsigned char>(result.back())))
                result.pop_back();
            return result;
        }

        const char* kindName(OblivionInteractionKind kind)
        {
            switch (kind)
            {
                case OblivionInteractionKind::Actor:
                case OblivionInteractionKind::Activator:
                    return "activate";
                case OblivionInteractionKind::Book:
                    return "read";
                case OblivionInteractionKind::Container:
                    return "loot";
                case OblivionInteractionKind::Door:
                    return "door";
                case OblivionInteractionKind::Flora:
                    return "harvest";
                case OblivionInteractionKind::Take:
                    return "take";
            }
            return "unknown";
        }
    }

    std::int32_t oblivionInventoryItemCount(const ContainerStore& inventory, const ESM::RefId& item)
    {
        std::uint32_t total = 0;
        for (const auto& entry : inventory)
            if (entry.getCellRef().getRefId() == item)
            {
                const auto raw = entry.getCellRef().getCount(false);
                const auto count = static_cast<std::uint32_t>(raw);
                total += raw < 0 ? 0u - count : count;
            }
        // The physical view supplies an already composed wrapped total.
        // Original base/change-entry composition is a separate native rule.
        return ESM4::nativeInventoryCountMagnitude(std::bit_cast<std::int32_t>(total));
    }

    OblivionInteractionAction::OblivionInteractionAction(const Ptr& target, OblivionInteractionKind kind)
        : Action(false, target)
        , mKind(kind)
    {
    }

    void OblivionInteractionAction::executeImp(const Ptr& actor)
    {
        if (mKind != OblivionInteractionKind::Actor
            && actor != MWBase::Environment::get().getWorld()->getPlayerPtr())
            return;

        // World is the sole MWBase::World implementation in the game executable.
        // Keeping the profile API off MWBase::World avoids exposing a temporary
        // M5 interaction vocabulary to unrelated tools and test doubles.
        static_cast<MWWorld::World*>(static_cast<MWBase::World*>(MWBase::Environment::get().getWorld()))
            ->interactWithOblivionReference(getTarget(), mKind, actor);
    }

    bool World::oblivionPlayerHasItem(const ESM::RefId& id)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion)
            return false;
        const ESM::FormId* formId = id.getIf<ESM::FormId>();
        if (formId == nullptr)
            return false;
        const ESM::FormKey key = ESM::FormKeyResolver(mContentFiles).toFormKey(*formId);
        return oblivionPlayerItemCount(key) > 0;
    }

    int World::oblivionPlayerItemCount(const ESM::FormKey& key)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || key.isNull())
            return 0;
        const std::optional<ESM::FormId> id = ESM::FormKeyResolver(mContentFiles).toFormId(key);
        if (!id)
            return 0;
        const Ptr player = getPlayerPtr();
        const ESM::RefId sharedId = OblivionProfileServices::sharedItemId(mStore, ESM::RefId(*id));
        return oblivionInventoryItemCount(player.getClass().getContainerStore(player), sharedId);
    }

    int World::oblivionChangePlayerInventory(
        const ESM::FormKey& key, int delta, const ESM::FormKey& owner)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || key.isNull() || delta == 0)
            return 0;
        const std::optional<ESM::FormId> id = ESM::FormKeyResolver(mContentFiles).toFormId(key);
        if (!id)
            return 0;
        const ESM::RefId refId(*id);
        std::optional<ESM4::InventoryItemDefinition> definition
            = OblivionProfileServices::itemDefinition(mStore, refId);
        if (!definition)
            return 0;
        if (delta > 0)
        {
            ESM4::RuntimeInventoryItem item;
            item.mBase = key;
            item.mCount = delta;
            item.mCondition = definition->mMaxCondition;
            item.mCharge = definition->mMaxCharge;
            item.mRemainingUsageTime = definition->mMaxUsageTime;
            item.mOwner = owner;
            return oblivionAddPlayerInventoryItem(std::move(item));
        }
        const Ptr player = getPlayerPtr();
        const int requested = delta == std::numeric_limits<int>::min()
            ? std::numeric_limits<int>::max()
            : -delta;
        return oblivionRemoveActorItem(player, key, requested);
    }

    int World::oblivionAddPlayerInventoryItem(ESM4::RuntimeInventoryItem item)
    {
        const auto canonical = [](const ESM::FormKey& key) {
            return ESM::FormKey::deserialize(key.serialize()) == key;
        };
        if (mGameProfile != ESM::GameProfile::Oblivion || item.mBase.isNull() || item.mCount <= 0
            || !std::isfinite(item.mCondition) || (item.mCondition < 0 && item.mCondition != -1)
            || item.mCondition > std::numeric_limits<float>::max()
            || !std::isfinite(item.mCharge) || item.mCharge < -1.f
            || !std::isfinite(item.mRemainingUsageTime) || item.mRemainingUsageTime < -1.f
            || !canonical(item.mBase) || !canonical(item.mOwner) || !canonical(item.mOwnershipGlobal))
            return 0;
        const ESM::FormKeyResolver resolver(mContentFiles);
        const auto id = resolver.toFormId(item.mBase);
        if (!id || !OblivionProfileServices::itemDefinition(mStore, ESM::RefId(*id)))
            return 0;
        if (!item.mOwner.isNull() && !resolver.toFormId(item.mOwner))
            return 0;
        item.mCondition = static_cast<float>(item.mCondition);
        item.mEquippedSlots = 0;
        item.mHotkey = -1;
        // Resolve every field and allocate the projected item before publishing
        // either inventory counts or registry identity.
        auto sources = OblivionProfileServices::prepareActorInventory(mStore, resolver, {item});
        const Ptr player = getPlayerPtr();
        auto& inventory = player.getClass().getInventoryStore(player);
        auto addition = inventory.prepareItemAddition(sources.front().mReference.getPtr(), item.mCount);
        std::optional<WorldModel::PreparedPtrReplacement> registration;
        if (addition->needsRegistration())
        {
            const std::array inserted{addition->getItem()};
            registration.emplace(mWorldModel.preparePtrReplacement({}, inserted));
        }
        if (!addition->isValid() || (registration && !registration->isValid()))
            return 0;
        if (!mOblivionDynamicReferenceIdentity)
            mOblivionDynamicReferenceIdentity = std::make_shared<const char>(0);
        const std::weak_ptr<const char> worldIdentity = mOblivionDynamicReferenceIdentity;
        const Ptr published = addition->commit();
        if (published.isEmpty()) return 0;
        if (registration) registration->commit();
        std::exception_ptr observerFailure;
        try { addition->notify(); }
        catch (...) { observerFailure = std::current_exception(); }
        // Observers may clear the World or replace its inventory/cache. The
        // committed transfer stays committed even if notification throws.
        // Reacquire the current Player's store while its World epoch survives.
        if (worldIdentity.expired() || getPlayerPtr() != player)
        {
            if (observerFailure) std::rethrow_exception(observerFailure);
            return item.mCount;
        }
        try
        {
            if (auto* windows = MWBase::Environment::get().getWindowManagerOrNull())
                windows->inventoryUpdated(getPlayerPtr());
        }
        catch (...)
        {
            if (!observerFailure) observerFailure = std::current_exception();
        }
        if (worldIdentity.expired() || getPlayerPtr() != player)
        {
            if (observerFailure) std::rethrow_exception(observerFailure);
            return item.mCount;
        }
        if (mOblivionRuntimeState)
            mOblivionRuntimeState->mPlayer.mInventory = captureOblivionActorInventory(getPlayerPtr());
        if (observerFailure) std::rethrow_exception(observerFailure);
        return item.mCount;
    }

    bool World::oblivionEquipPlayerItem(const ESM::FormKey& key, bool equip)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || key.isNull())
            return false;
        const std::optional<ESM::FormId> id = ESM::FormKeyResolver(mContentFiles).toFormId(key);
        if (!id)
            return false;
        std::optional<ESM4::InventoryItemDefinition> definition
            = OblivionProfileServices::itemDefinition(mStore, ESM::RefId(*id));
        if (!definition)
            return false;
        definition->mBase = key;
        if (!mOblivionRuntimeState)
            mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());
        InventoryStore& inventory = getPlayerPtr().getClass().getInventoryStore(getPlayerPtr());
        const ESM::RefId sharedId = OblivionProfileServices::sharedItemId(mStore, ESM::RefId(*id));
        ContainerStoreIterator found = inventory.end();
        for (auto iterator = inventory.begin(); iterator != inventory.end(); ++iterator)
            if ((*iterator).getCellRef().getRefId() == sharedId)
            {
                found = iterator;
                break;
            }
        if (found == inventory.end())
            return false;
        if (!equip)
        {
            if (!inventory.isEquipped(*found))
                return ESM4::unequipInventoryItem(mOblivionRuntimeState->mPlayer.mInventory, key);
            inventory.unequipItem(*found);
            oblivionPlayerEquipmentChanged();
            return true;
        }
        const std::vector<int> slots = (*found).getClass().getEquipmentSlots(*found).first;
        if (slots.empty())
            return false;
        int selectedSlot = slots.front();
        if (definition->mChooseOneSlot && slots.size() > 1)
            selectedSlot = inventory.getSlot(slots.front()) == inventory.end() ? slots.front() : slots.back();
        inventory.equip(selectedSlot, found);
        oblivionPlayerEquipmentChanged();
        const std::uint32_t preferred = definition->mChooseOneSlot
            ? (selectedSlot == InventoryStore::Slot_LeftRing
                    ? static_cast<std::uint32_t>(ESM4::Armor::TES4_LeftRing)
                    : static_cast<std::uint32_t>(ESM4::Armor::TES4_RightRing))
            : 0;
        ESM4::equipInventoryItem(mOblivionRuntimeState->mPlayer.mInventory, *definition, preferred);
        return true;
    }

    bool World::oblivionActorItemEquipped(const Ptr& actor, const ESM::FormKey& key) const
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !nativeEquipmentActor(actor) || key.isNull())
            return false;
        const auto id = ESM::FormKeyResolver(mContentFiles).toFormId(key);
        if (!id)
            return false;
        const auto shared = OblivionProfileServices::sharedItemId(mStore, ESM::RefId(*id));
        auto& inventory = actor.getClass().getInventoryStore(actor);
        for (auto it = inventory.begin(); it != inventory.end(); ++it)
            if (it->getCellRef().getCount() > 0 && it->getCellRef().getRefId() == shared
                && inventory.isEquipped(*it))
                return true;
        return false;
    }

    int World::oblivionAddActorItem(const Ptr& actor, const ESM::FormKey& key, int count)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !nativeEquipmentActor(actor)
            || key.isNull() || count <= 0)
            return 0;
        const auto id = ESM::FormKeyResolver(mContentFiles).toFormId(key);
        if (!id)
            return 0;
        const auto definition = OblivionProfileServices::itemDefinition(mStore, ESM::RefId(*id));
        if (!definition)
            return 0;
        const auto shared = OblivionProfileServices::sharedItemId(mStore, ESM::RefId(*id));
        auto& inventory = actor.getClass().getInventoryStore(actor);
        std::uint32_t existing = 0;
        const auto capacity = static_cast<std::uint32_t>(std::numeric_limits<int>::max() - count);
        for (auto it = inventory.begin(); it != inventory.end(); ++it)
            if (it->getCellRef().getRefId() == shared)
            {
                const std::int32_t raw = it->getCellRef().getCount(false);
                const std::uint32_t quantity = raw < 0 ? 0u - static_cast<std::uint32_t>(raw)
                                                     : static_cast<std::uint32_t>(raw);
                if (quantity > capacity - existing)
                    throw std::invalid_argument("Native AddItem exceeds the supported physical int32 quantity");
                existing += quantity;
            }
        ManualRef source(mStore, shared, count);
        if (definition->mMaxCondition >= 0)
            source.getPtr().getCellRef().setNativeItemCondition(static_cast<float>(definition->mMaxCondition));
        if (definition->mMaxCharge >= 0.f)
            source.getPtr().getCellRef().setEnchantmentCharge(definition->mMaxCharge);
        if (definition->mMaxUsageTime >= 0.f)
            source.getPtr().getClass().setRemainingUsageTime(source.getPtr(), definition->mMaxUsageTime);
        inventory.add(source.getPtr(), count, false);
        return count;
    }

    int World::oblivionRemoveActorItem(const Ptr& actor, const ESM::FormKey& key, int count)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || actor.isEmpty() || key.isNull() || count <= 0
            || ESM::FormKey::deserialize(key.serialize()) != key)
            return 0;
        const bool player = actor == getPlayerPtr();
        if (!player && !nativeEquipmentActor(actor)) return 0;
        const auto id = ESM::FormKeyResolver(mContentFiles).toFormId(key);
        if (!id) return 0;
        const auto definition = OblivionProfileServices::itemDefinition(mStore, ESM::RefId(*id));
        if (!definition) return 0;
        auto& inventory = actor.getClass().getInventoryStore(actor);
        const auto shared = OblivionProfileServices::sharedItemId(mStore, ESM::RefId(*id));
        auto removal = inventory.prepareItemRemoval(shared, count);
        const int removed = removal->getCount();
        if (!removed) return 0;
        if (!mOblivionDynamicReferenceIdentity)
            mOblivionDynamicReferenceIdentity = std::make_shared<const char>(0);
        const std::weak_ptr<const char> worldIdentity = mOblivionDynamicReferenceIdentity;
        const auto actorRefNum = actor.getCellRef().getRefNum();
        const auto actorKey = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
        const auto current = [&] {
            return !worldIdentity.expired()
                && (player ? getPlayerPtr() == actor : mWorldModel.getPtr(actorRefNum) == actor);
        };
        if (removal->depletesEquipment() && mOblivionCombat
            && (definition->mSlots & (ESM4::InventorySlotWeapon | ESM4::Armor::TES4_Shield
                | ESM4::InventorySlotLight)) != 0)
        {
            auto* mechanics = MWBase::Environment::get().getMechanicsManagerOrNull();
            if (!mechanics || !mechanics->cancelOblivionCombatInput(actor))
            {
                if (!current() || !removal->ownerIsCurrent()) return 0;
                if (const auto* state = mOblivionCombat->findMeleeState(actorKey); state && state->mStrike)
                    mOblivionCombat->cancelMeleeStrike(state->mStrike->mActionId, actorKey);
                mOblivionCombat->endBlocking(actorKey);
                mOblivionCombat->clearMeleeInput(actorKey);
            }
        }
        if (!current() || !removal->commit()) return 0;
        std::exception_ptr observerFailure;
        try { removal->notify(); }
        catch (...) { observerFailure = std::current_exception(); }
        if (current())
        {
            try
            {
                if (auto* windows = MWBase::Environment::get().getWindowManagerOrNull())
                    windows->inventoryUpdated(actor);
            }
            catch (...) { if (!observerFailure) observerFailure = std::current_exception(); }
        }
        if (current() && mOblivionRuntimeState)
        {
            auto live = captureOblivionActorInventory(actor);
            if (player) mOblivionRuntimeState->mPlayer.mInventory = std::move(live);
            else
                for (auto& reference : mOblivionRuntimeState->mReferences)
                    if (reference.mKey == actorKey)
                    {
                        reference.mInventory = std::move(live);
                        break;
                    }
        }
        if (observerFailure) std::rethrow_exception(observerFailure);
        return removed;
    }

    bool World::oblivionEquipActorItem(const Ptr& actor, const ESM::FormKey& key, bool equip)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !nativeEquipmentActor(actor) || key.isNull())
            return false;
        const auto id = ESM::FormKeyResolver(mContentFiles).toFormId(key);
        if (!id)
            return false;
        const auto definition = OblivionProfileServices::itemDefinition(mStore, ESM::RefId(*id));
        if (!definition || definition->mSlots == 0)
            return false;
        const auto shared = OblivionProfileServices::sharedItemId(mStore, ESM::RefId(*id));
        auto& inventory = actor.getClass().getInventoryStore(actor);
        const auto matches = [&](const Ptr& item) {
            return item.getCellRef().getCount() > 0 && item.getCellRef().getRefId() == shared;
        };
        const auto cancel = [&] {
            if ((definition->mSlots & (ESM4::InventorySlotWeapon | ESM4::Armor::TES4_Shield
                    | ESM4::InventorySlotLight)) == 0 || !mOblivionCombat)
                return;
            if (auto* mechanics = MWBase::Environment::get().getMechanicsManagerOrNull();
                mechanics && mechanics->cancelOblivionCombatInput(actor))
                return;
            // A nonresident/headless view has no controller/animation observer.
            // Cancel the same authority fields without constructing a controller.
            const auto owner = actor.getCellRef().getFormKey();
            if (const auto* state = mOblivionCombat->findMeleeState(owner); state && state->mStrike)
                mOblivionCombat->cancelMeleeStrike(state->mStrike->mActionId, owner);
            mOblivionCombat->endBlocking(owner);
            mOblivionCombat->clearMeleeInput(owner);
        };
        if (!equip)
        {
            bool changed = false;
            // Re-find after restacking/cancellation callbacks. Visit each slot
            // once: an equipment observer can issue a new equip request, and
            // this outer request must not repeatedly undo it or loop forever.
            for (int slot = 0; slot < InventoryStore::Slots; ++slot)
            {
                auto found = inventory.getSlot(slot);
                if (found == inventory.end() || !matches(*found))
                    continue;
                if (!changed)
                {
                    cancel(); // Before equipment/animation-end observers.
                    changed = true;
                    found = inventory.getSlot(slot);
                    if (found == inventory.end() || !matches(*found))
                        continue; // A cancellation observer completed this slot.
                }
                inventory.unequipSlot(slot);
            }
            return changed;
        }
        auto found = inventory.end();
        auto alreadyEquipped = inventory.end();
        for (auto it = inventory.begin(); it != inventory.end(); ++it)
        {
            if (!matches(*it))
                continue;
            if (!inventory.isEquipped(*it))
            {
                found = it;
                break;
            }
            if (alreadyEquipped == inventory.end())
                alreadyEquipped = it;
        }
        if (found == inventory.end())
            found = alreadyEquipped;
        if (found == inventory.end())
            return false;
        const auto slots = found->getClass().getEquipmentSlots(*found).first;
        if (slots.empty())
            return false;
        int selected = slots.front();
        if (definition->mChooseOneSlot)
        {
            // Follow the typed inventory's native RightRing/LeftRing order,
            // rather than the shared facade's opposite slot enumeration.
            if (std::ranges::find(slots, InventoryStore::Slot_RightRing) == slots.end()
                || std::ranges::find(slots, InventoryStore::Slot_LeftRing) == slots.end())
                return false;
            selected = InventoryStore::Slot_RightRing;
            if (inventory.getSlot(selected) != inventory.end()
                && inventory.getSlot(InventoryStore::Slot_LeftRing) == inventory.end())
                selected = InventoryStore::Slot_LeftRing;
            if (inventory.isEquipped(*found) && inventory.getSlot(selected) != found)
                return false; // One physical ring cannot occupy both slots.
        }
        if (inventory.getSlot(selected) == found)
            return true; // Idempotent: retain playback and the physical instance.
        const Ptr requested = *found;
        cancel();
        // Animation-end observers can change inventory. Re-find the physical
        // instance rather than equipping a removed/restacked iterator.
        found = std::find(inventory.begin(), inventory.end(), requested);
        if (found == inventory.end() || !matches(*found))
            return false;
        if (inventory.getSlot(selected) == found)
            return true;
        inventory.equip(selected, found);
        return true;
    }

    std::uint32_t World::oblivionEquipmentSlots(const Ptr& item, int sharedSlot) const
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || item.isEmpty())
            return 0;
        const ESM::RefId nativeId
            = OblivionProfileServices::nativeItemId(mStore, item.getCellRef().getRefId());
        const auto definition = OblivionProfileServices::itemDefinition(mStore, nativeId);
        if (!definition)
            return 0;
        if (!definition->mChooseOneSlot)
            return definition->mSlots;
        return sharedSlot == InventoryStore::Slot_LeftRing
            ? static_cast<std::uint32_t>(ESM4::Armor::TES4_LeftRing)
            : static_cast<std::uint32_t>(ESM4::Armor::TES4_RightRing);
    }

    void World::oblivionPlayerEquipmentChanged()
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionRuntimeState)
            return;
        for (ESM4::RuntimeInventoryItem& item : mOblivionRuntimeState->mPlayer.mInventory)
            item.mEquippedSlots = 0;
        ESM4::normalizeInventory(mOblivionRuntimeState->mPlayer.mInventory);

        const ESM::FormKeyResolver resolver(mContentFiles);
        InventoryStore& inventory = getPlayerPtr().getClass().getInventoryStore(getPlayerPtr());
        for (auto iterator = inventory.begin(); iterator != inventory.end(); ++iterator)
        {
            const Ptr item = *iterator;
            if (!inventory.isEquipped(item))
                continue;
            const ESM::RefId nativeId = OblivionProfileServices::nativeItemId(mStore, item.getCellRef().getRefId());
            const ESM::FormId* formId = nativeId.getIf<ESM::FormId>();
            if (formId == nullptr)
                continue;
            auto definition = OblivionProfileServices::itemDefinition(mStore, nativeId);
            if (!definition)
                continue;
            definition->mBase = resolver.toFormKey(*formId);
            std::uint32_t preferredSlot = 0;
            if (definition->mChooseOneSlot)
            {
                const ContainerStoreIterator left = inventory.getSlot(InventoryStore::Slot_LeftRing);
                preferredSlot = left != inventory.end() && *left == item
                    ? static_cast<std::uint32_t>(ESM4::Armor::TES4_LeftRing)
                    : static_cast<std::uint32_t>(ESM4::Armor::TES4_RightRing);
            }
            ESM4::equipInventoryItem(mOblivionRuntimeState->mPlayer.mInventory, *definition, preferredSlot);
        }
    }

    bool World::oblivionSetPlayerHotkey(const ESM::RefId& sharedId, int hotkey)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || hotkey < -1 || hotkey > 7)
            return false;
        const ESM::RefId nativeId = OblivionProfileServices::nativeItemId(mStore, sharedId);
        const ESM::FormId* formId = nativeId.getIf<ESM::FormId>();
        if (formId == nullptr)
            return false;
        if (!mOblivionRuntimeState)
            mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());
        return ESM4::setInventoryHotkey(mOblivionRuntimeState->mPlayer.mInventory,
            ESM::FormKeyResolver(mContentFiles).toFormKey(*formId), hotkey);
    }

    std::optional<std::vector<std::pair<ESM::RefId, std::uint32_t>>>
    World::oblivionReferenceEquipment(const Ptr& actor) const
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !nativeEquipmentActor(actor))
            return std::nullopt;
        auto& inventory = actor.getClass().getInventoryStore(actor);
        std::vector<std::pair<ESM::RefId, std::uint32_t>> result;
        for (auto it = inventory.begin(); it != inventory.end(); ++it)
        {
            const Ptr item = *it;
            if (item.getCellRef().getCount() <= 0 || !inventory.isEquipped(item))
                continue;
            std::uint32_t masks = 0;
            for (int slot = 0; slot < InventoryStore::Slots; ++slot)
                if (inventory.getSlot(slot) == it)
                    masks |= oblivionEquipmentSlots(item, slot);
            if (masks != 0)
                result.emplace_back(OblivionProfileServices::nativeItemId(mStore, item.getCellRef().getRefId()), masks);
        }
        return result;
    }

    void World::takeOblivionReference(const Ptr& ptr)
    {
        if (ptr.isEmpty() || !ptr.isInCell() || ptr.getContainerStore() || ptr.getClass().isActor()
            || !ptr.getClass().isItem(ptr) || ptr.getClass().useAnim() || ptr.getCellRef().getCount() <= 0)
            return;
        const auto canonical = [](const ESM::FormKey& value) {
            return ESM::FormKey::deserialize(value.serialize()) == value;
        };
        const ESM::FormKeyResolver resolver(mContentFiles);
        const ESM::FormKey key = ptr.getCellRef().getFormKey();
        const ESM::RefId sourceId = ptr.getCellRef().getRefId();
        const auto* id = sourceId.getIf<ESM::FormId>();
        if (!id || key.isNull() || !canonical(key)
            || mWorldModel.getPtr(ptr.getCellRef().getRefNum()) != ptr)
            throw std::invalid_argument("pickup source has no current native reference identity");
        const auto definition = OblivionProfileServices::itemDefinition(mStore, ESM::RefId(*id));
        if (!definition)
            throw std::invalid_argument("pickup source has no winning native item definition");
        ESM4::RuntimeInventoryItem item;
        item.mBase = resolver.toFormKey(*id);
        item.mCount = ptr.getCellRef().getCount();
        const auto condition = ptr.getCellRef().getNativeItemCondition();
        const float charge = ptr.getCellRef().getEnchantmentCharge();
        if ((condition && definition->mMaxCondition < 0) || (charge >= 0 && definition->mMaxCharge < 0))
            throw std::invalid_argument("pickup extras are unsupported by the native item definition");
        item.mCondition = condition.value_or(-1.f);
        item.mCharge = charge;
        item.mRemainingUsageTime = definition->mMaxUsageTime < 0 ? -1
            : ptr.getClass().getRemainingUsageTime(ptr);
        const ESM::RefId owner = ptr.getCellRef().getOwner();
        if (!owner.empty())
        {
            const auto* ownerId = owner.getIf<ESM::FormId>();
            if (!ownerId)
                throw std::invalid_argument("pickup source ownership is not a native identity");
            item.mOwner = resolver.toFormKey(*ownerId);
            if (item.mOwner.isNull() || !resolver.toFormId(item.mOwner))
                throw std::invalid_argument("pickup owner cannot be resolved to native content");
        }
        item.mOwnershipRank = ptr.getCellRef().getNativeOwnershipRank();
        item.mOwnershipGlobal = OblivionProfileServices::captureOwnershipGlobal(mStore, resolver, ptr.getCellRef());
        if (item.mBase.isNull() || !canonical(item.mBase) || !canonical(item.mOwner)
            || !std::isfinite(item.mCondition) || (item.mCondition < 0 && item.mCondition != -1)
            || item.mCondition > std::numeric_limits<float>::max()
            || !std::isfinite(item.mCharge) || item.mCharge < -1
            || !std::isfinite(item.mRemainingUsageTime) || item.mRemainingUsageTime < -1)
            throw std::invalid_argument("invalid native pickup metadata");
        const std::string name(ptr.getClass().getName(ptr));
        if (!mOblivionRuntimeState)
            mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());
        const auto findSource = [&]() -> ESM4::RuntimeReferenceState* {
            if (!mOblivionRuntimeState) return nullptr;
            const auto found = std::find_if(mOblivionRuntimeState->mReferences.begin(), mOblivionRuntimeState->mReferences.end(),
                [&](const auto& reference) { return reference.mKey == key; });
            return found == mOblivionRuntimeState->mReferences.end() ? nullptr : &*found;
        };
        auto* cached = findSource();
        if (!cached || cached->mBase != item.mBase)
            throw std::invalid_argument("pickup source is absent from the current native state");
        auto custom = cached->mCustomState;
        custom["taken"] = true;
        if (!owner.empty()) custom["ownership_checked"] = true;
        auto sources = OblivionProfileServices::prepareActorInventory(mStore, resolver, {item});
        const Ptr player = getPlayerPtr();
        auto& inventory = player.getClass().getInventoryStore(player);
        auto addition = inventory.prepareItemAddition(sources.front().mReference.getPtr(), item.mCount);
        std::optional<WorldModel::PreparedPtrReplacement> registration;
        if (addition->needsRegistration())
        {
            const std::array inserted{addition->getItem()};
            registration.emplace(mWorldModel.preparePtrReplacement({}, inserted));
        }
        if (!mOblivionDynamicReferenceIdentity)
            mOblivionDynamicReferenceIdentity = std::make_shared<const char>(0);
        const std::weak_ptr<const char> worldIdentity = mOblivionDynamicReferenceIdentity;
        std::unique_ptr<Scene::PreparedItemRemoval> scene;
        if (mWorldScene)
            scene = mWorldScene->prepareItemRemoval(ptr);
        else if (ptr.getRefData().getBaseNode())
            throw std::invalid_argument("pickup source model has no owning scene");
        if (worldIdentity.expired() || getPlayerPtr() != player || findSource() != cached
            || ptr.getCellRef().getCount() != item.mCount
            || mWorldModel.getPtr(ptr.getCellRef().getRefNum()) != ptr
            || !addition->isValid() || (registration && !registration->isValid())
            || (scene && !scene->isValid()))
            throw std::invalid_argument("native pickup preparation became stale");
        // No allocation or external observer in this publication interval.
        if (scene && !scene->commitResources())
            throw std::logic_error("validated pickup scene removal was rejected");
        ptr.getCellRef().setCount(0);
        const Ptr published = addition->commit();
        if (published.isEmpty())
            throw std::logic_error("validated pickup inventory addition was rejected");
        if (registration) registration->commit();
        cached->mCustomState.swap(custom);
        if (scene)
        {
            if (!scene->commitInactiveEvent())
                throw std::logic_error("validated pickup inactive event was rejected");
        }
        // Release navigation/collision locks before any inventory/UI observer.
        scene.reset();
        std::exception_ptr failure;
        try { addition->notify(); } catch (...) { failure = std::current_exception(); }
        if (!worldIdentity.expired() && getPlayerPtr() == player)
        {
            try
            {
                if (auto* windows = MWBase::Environment::get().getWindowManagerOrNull())
                    windows->inventoryUpdated(getPlayerPtr());
            }
            catch (...) { if (!failure) failure = std::current_exception(); }
        }
        if (!worldIdentity.expired() && getPlayerPtr() == player)
        {
            try
            {
                if (mOblivionRuntimeState)
                    mOblivionRuntimeState->mPlayer.mInventory = captureOblivionActorInventory(getPlayerPtr());
            }
            catch (...) { if (!failure) failure = std::current_exception(); }
        }
        if (failure) std::rethrow_exception(failure);
        if (worldIdentity.expired() || getPlayerPtr() != player) return;
        if (auto* windows = MWBase::Environment::get().getWindowManagerOrNull())
            windows->messageBox("Taken: " + name + (item.mCount > 1 ? " (" + std::to_string(item.mCount) + ")" : ""));
        Log(Debug::Info) << "M5 interaction: kind=take result=taken ref=" << key.serialize()
                         << " base=" << item.mBase.serialize();
    }

    void World::interactWithOblivionReference(const Ptr& ptr, OblivionInteractionKind kind, const Ptr& actor)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || ptr.isEmpty())
            return;

        if (kind == OblivionInteractionKind::Take && ptr.getCellRef().getCount() <= 0)
            return;

        if (dispatchOblivionActivation(ptr, actor))
            return;

        if (kind == OblivionInteractionKind::Door || kind == OblivionInteractionKind::Actor)
        {
            activateOblivionReferenceDefault(ptr, actor);
            return;
        }

        if (kind == OblivionInteractionKind::Take)
        {
            takeOblivionReference(ptr);
            return;
        }

        if (!mOblivionRuntimeState)
            mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());

        const ESM::FormKey key = ptr.getCellRef().getFormKey();
        const auto found = std::find_if(mOblivionRuntimeState->mReferences.begin(),
            mOblivionRuntimeState->mReferences.end(),
            [&](const ESM4::RuntimeReferenceState& state) { return state.mKey == key; });
        if (found == mOblivionRuntimeState->mReferences.end())
            throw std::runtime_error("M5 interaction reference is absent from native TES4 state: " + key.serialize());

        ESM4::RuntimeReferenceState& state = *found;
        auto report = [&](std::string_view result) {
            const Ptr player = getPlayerPtr();
            const ESM::Position& playerPosition = player.getRefData().getPosition();
            Log(Debug::Info) << "M5 interaction: kind=" << kindName(kind) << " result=" << result
                             << " ref=" << key.serialize() << " base=" << state.mBase.serialize()
                             << " grounded=" << (isOnGround(player) ? "true" : "false")
                             << " player_z=" << playerPosition.pos[2];
        };
        auto message = [&](std::string value) {
            MWBase::Environment::get().getWindowManager()->messageBox(value);
        };

        const bool isPlaceholderCreature = ptr.getClass().getType() == ESM::REC_CREA4;
        const bool isOwned = !isPlaceholderCreature && !ptr.getCellRef().getOwner().empty();
        if (isOwned)
            state.mCustomState["ownership_checked"] = true;

        if (kind == OblivionInteractionKind::Container && !isPlaceholderCreature && ptr.getCellRef().isLocked())
        {
            state.mCustomState["lock_checked"] = true;
            const ESM::RefId lockKey = ptr.getCellRef().getKey();
            if (!lockKey.empty() && oblivionPlayerHasItem(lockKey))
            {
                ptr.getCellRef().setLocked(false);
                ptr.getCellRef().setLockLevel(0);
                state.mLockLevel = 0;
                state.mCustomState["locked"] = false;
                state.mCustomState["unlocked_with_key"] = true;
                message("Unlocked with key");
            }
            else
            {
                const std::optional<ESM::FormKey> lockpick = mStore.findEsm4FormKey("Lockpick");
                if (!lockpick || oblivionPlayerItemCount(*lockpick) <= 0)
                {
                    message("Locked (level " + std::to_string(std::max(0, ptr.getCellRef().getLockLevel())) + ")");
                    report("locked");
                    return;
                }
                const Ptr player = getPlayerPtr();
                const MWMechanics::CreatureStats& stats = player.getClass().getCreatureStats(player);
                const float chance = ESM4::lockpickAutoChance(
                    static_cast<int>(player.getClass().getSkill(player, ESM::Skill::Security)),
                    stats.getAttribute(ESM::Attribute::Agility).getModified(),
                    stats.getAttribute(ESM::Attribute::Luck).getModified(), ptr.getCellRef().getLockLevel());
                state.mCustomState["lockpick_auto_chance"] = static_cast<double>(chance);
                std::int64_t attempts = 0;
                if (const auto previous = state.mCustomState.find("lockpick_auto_attempts");
                    previous != state.mCustomState.end())
                    if (const auto* value = std::get_if<std::int64_t>(&previous->second))
                        attempts = *value;
                state.mCustomState["lockpick_auto_attempts"] = attempts + 1;
                if (Misc::Rng::rollProbability(mPrng) * 100.f >= chance)
                {
                    oblivionChangePlayerInventory(*lockpick, -1);
                    state.mCustomState["lockpick_broken"] = true;
                    message("Lockpick broke");
                    report("lockpick_failed");
                    return;
                }
                ptr.getCellRef().setLocked(false);
                ptr.getCellRef().setLockLevel(0);
                state.mLockLevel = 0;
                state.mCustomState["locked"] = false;
                state.mCustomState["unlocked_with_lockpick"] = true;
                message("Lock opened");
            }
        }

        const ESM::FormKeyResolver resolver(mContentFiles);
        switch (kind)
        {
            case OblivionInteractionKind::Take:
                break; // Handled before any borrowed cached reference state.
            case OblivionInteractionKind::Container:
            {
                if (state.mInventory.empty())
                {
                    message("Empty");
                    report("empty");
                    break;
                }
                std::ostringstream summary;
                summary << "Looted:";
                int lines = 0;
                for (const ESM4::RuntimeInventoryItem& item : state.mInventory)
                {
                    ESM4::RuntimeInventoryItem transferred = item;
                    transferred.mOwner = !item.mOwner.isNull() ? item.mOwner
                        : state.mOwner.value_or(ESM::FormKey{});
                    transferred.mEquippedSlots = 0;
                    transferred.mHotkey = -1;
                    oblivionAddPlayerInventoryItem(std::move(transferred));
                    if (lines++ < 8)
                    {
                        const std::optional<ESM::FormId> itemId = resolver.toFormId(item.mBase);
                        summary << "\n" << item.mCount << " x "
                                << (itemId ? getOblivionItemName(mStore, ESM::RefId(*itemId))
                                           : item.mBase.serialize());
                    }
                }
                state.mInventory.clear();
                state.mCustomState["opened"] = true;
                message(summary.str());
                report("looted");
                break;
            }
            case OblivionInteractionKind::Actor:
            case OblivionInteractionKind::Door:
                break;
            case OblivionInteractionKind::Book:
            {
                const ESM4::Book* book = ptr.get<ESM4::Book>()->mBase;
                state.mCustomState["read"] = true;
                std::string text = plainBookText(book->mText);
                message(book->mFullName + (text.empty() ? std::string{} : "\n\n" + text));
                report("read");
                break;
            }
            case OblivionInteractionKind::Flora:
            {
                if (const auto harvested = state.mCustomState.find("harvested");
                    harvested != state.mCustomState.end() && std::get_if<bool>(&harvested->second)
                    && *std::get_if<bool>(&harvested->second))
                {
                    message("Nothing to harvest");
                    report("empty");
                    break;
                }
                const ESM4::Flora* flora = ptr.get<ESM4::Flora>()->mBase;
                const ESM::FormKey ingredient = resolver.toFormKey(flora->mIngredient);
                if (ingredient.isNull())
                {
                    message("Nothing to harvest");
                    report("empty");
                    break;
                }
                state.mCustomState["harvested"] = true;
                const int month = std::clamp(mTimeManager->getEpochTimeStamp().mMonth, 0, 11);
                const std::uint8_t chance = month >= 2 && month <= 4 ? flora->mPercentHarvest.spring
                    : month >= 5 && month <= 7 ? flora->mPercentHarvest.summer
                    : month >= 8 && month <= 10 ? flora->mPercentHarvest.autumn
                                                : flora->mPercentHarvest.winter;
                state.mCustomState["harvest_chance"] = static_cast<std::int64_t>(chance);
                if (Misc::Rng::rollDice(100, mPrng) < chance)
                {
                    oblivionChangePlayerInventory(ingredient, 1);
                    message("Harvested: " + getOblivionItemName(mStore, ESM::RefId(flora->mIngredient)));
                    report("harvested");
                }
                else
                {
                    message("Nothing to harvest");
                    report("harvest_failed");
                }
                break;
            }
            case OblivionInteractionKind::Activator:
            {
                std::int64_t count = 0;
                if (const auto existing = state.mCustomState.find("activation_count");
                    existing != state.mCustomState.end())
                    if (const auto* value = std::get_if<std::int64_t>(&existing->second))
                        count = *value;
                state.mCustomState["activation_count"] = count + 1;
                message("Activated: " + std::string(ptr.getClass().getName(ptr)));
                report("activated");
                break;
            }
        }
    }
}
