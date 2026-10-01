#include "oblivioninteraction.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>

#include <components/debug/debuglog.hpp>
#include <components/esm/formkey.hpp>
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadappa.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadcont.hpp>
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
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwmechanics/creaturestats.hpp"

#include "class.hpp"
#include "datetimemanager.hpp"
#include "inventorystore.hpp"
#include "manualref.hpp"
#include "oblivionprofileservices.hpp"
#include "worldimp.hpp"

namespace MWWorld
{
    namespace
    {
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

    OblivionInteractionAction::OblivionInteractionAction(const Ptr& target, OblivionInteractionKind kind)
        : Action(false, target)
        , mKind(kind)
    {
    }

    void OblivionInteractionAction::executeImp(const Ptr& actor)
    {
        if (actor != MWBase::Environment::get().getWorld()->getPlayerPtr())
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
        return player.getClass().getContainerStore(player).count(sharedId);
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
        if (!mOblivionRuntimeState)
            mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());
        const Ptr player = getPlayerPtr();
        InventoryStore& live = player.getClass().getInventoryStore(player);
        const ESM::RefId sharedId = OblivionProfileServices::sharedItemId(mStore, refId);
        const int requested = delta == std::numeric_limits<int>::min()
            ? std::numeric_limits<int>::max()
            : -delta;
        const int removed = live.remove(sharedId, requested, false, true);
        ESM4::removeInventoryItem(mOblivionRuntimeState->mPlayer.mInventory, key, removed);
        return removed;
    }

    int World::oblivionAddPlayerInventoryItem(ESM4::RuntimeInventoryItem item)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || item.mBase.isNull() || item.mCount <= 0
            || !std::isfinite(item.mCondition) || (item.mCondition < 0 && item.mCondition != -1)
            || item.mCondition > std::numeric_limits<float>::max())
            return 0;
        const std::optional<ESM::FormId> id = ESM::FormKeyResolver(mContentFiles).toFormId(item.mBase);
        if (!id)
            return 0;
        const ESM::RefId nativeId(*id);
        const auto definition = OblivionProfileServices::itemDefinition(mStore, nativeId);
        if (!definition)
            return 0;
        if (!mOblivionRuntimeState)
            mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());

        item.mCondition = static_cast<float>(item.mCondition);
        item.mCount = std::min(item.mCount, std::numeric_limits<int>::max());
        item.mEquippedSlots = 0;
        item.mHotkey = -1;
        ManualRef source(mStore, OblivionProfileServices::sharedItemId(mStore, nativeId), item.mCount);
        if (item.mCondition >= 0)
            source.getPtr().getCellRef().setNativeItemCondition(static_cast<float>(item.mCondition));
        if (item.mCharge >= 0.f)
            source.getPtr().getCellRef().setEnchantmentCharge(item.mCharge);
        if (item.mRemainingUsageTime >= 0.f)
            source.getPtr().getClass().setRemainingUsageTime(source.getPtr(), item.mRemainingUsageTime);
        if (!item.mOwner.isNull())
        {
            const std::optional<ESM::FormId> ownerId = ESM::FormKeyResolver(mContentFiles).toFormId(item.mOwner);
            if (!ownerId)
                return 0;
            source.getPtr().getCellRef().setOwner(ESM::RefId(*ownerId));
        }
        getPlayerPtr().getClass().getInventoryStore(getPlayerPtr()).add(source.getPtr(), item.mCount, false);
        ESM4::addInventoryItem(mOblivionRuntimeState->mPlayer.mInventory, item);
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
    World::oblivionReferenceEquipment(const ESM::FormKey& key) const
    {
        if (!mOblivionRuntimeState || key.isNull())
            return std::nullopt;
        const auto found = std::ranges::find(mOblivionRuntimeState->mReferences, key,
            &ESM4::RuntimeReferenceState::mKey);
        if (found == mOblivionRuntimeState->mReferences.end())
            return std::nullopt;
        const ESM::FormKeyResolver resolver(mContentFiles);
        std::vector<std::pair<ESM::RefId, std::uint32_t>> result;
        for (const ESM4::RuntimeInventoryItem& item : found->mInventory)
            if (item.mCount > 0 && item.mEquippedSlots != 0)
                if (const std::optional<ESM::FormId> id = resolver.toFormId(item.mBase))
                    result.emplace_back(ESM::RefId(*id), item.mEquippedSlots);
        return result;
    }

    void World::interactWithOblivionReference(const Ptr& ptr, OblivionInteractionKind kind, const Ptr& actor)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || ptr.isEmpty())
            return;

        if (dispatchOblivionActivation(ptr, actor))
            return;

        if (kind == OblivionInteractionKind::Door)
        {
            activateOblivionReferenceDefault(ptr, actor);
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
            {
                const int count = std::max(1, ptr.getCellRef().getCount());
                const ESM::FormKey owner = state.mOwner.value_or(ESM::FormKey{});
                oblivionChangePlayerInventory(state.mBase, count, owner);
                const std::string name(ptr.getClass().getName(ptr));
                state.mCustomState["taken"] = true;
                deleteObject(ptr);
                message("Taken: " + name + (count > 1 ? " (" + std::to_string(count) + ")" : ""));
                report("taken");
                break;
            }
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
