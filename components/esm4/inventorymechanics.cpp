#include "inventorymechanics.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace ESM4
{
    namespace
    {
        bool sameStack(const RuntimeInventoryItem& left, const RuntimeInventoryItem& right)
        {
            return left.mBase == right.mBase && left.mCondition == right.mCondition
                && left.mCharge == right.mCharge && left.mRemainingUsageTime == right.mRemainingUsageTime
                && left.mEquippedSlots == right.mEquippedSlots
                && left.mHotkey == right.mHotkey && left.mOwner == right.mOwner;
        }

        std::int32_t saturatingAdd(std::int32_t left, std::int32_t right)
        {
            const std::int64_t result = static_cast<std::int64_t>(left) + right;
            return static_cast<std::int32_t>(std::clamp<std::int64_t>(
                result, 0, std::numeric_limits<std::int32_t>::max()));
        }

        std::uint32_t firstBit(std::uint32_t value)
        {
            return value & (~value + 1u);
        }
    }

    void normalizeInventory(std::vector<RuntimeInventoryItem>& inventory)
    {
        std::erase_if(inventory, [](const RuntimeInventoryItem& item) { return item.mCount <= 0; });
        for (RuntimeInventoryItem& item : inventory)
        {
            item.mEquippedSlots &= InventorySlotMask;
            if (item.mHotkey < -1 || item.mHotkey > 7)
                item.mHotkey = -1;
        }
        for (std::size_t i = 0; i < inventory.size(); ++i)
        {
            for (std::size_t j = i + 1; j < inventory.size();)
            {
                if (!sameStack(inventory[i], inventory[j]))
                {
                    ++j;
                    continue;
                }
                inventory[i].mCount = saturatingAdd(inventory[i].mCount, inventory[j].mCount);
                inventory.erase(inventory.begin() + static_cast<std::ptrdiff_t>(j));
            }
        }
    }

    std::int32_t inventoryCount(const std::vector<RuntimeInventoryItem>& inventory, const ESM::FormKey& base)
    {
        std::int32_t result = 0;
        for (const RuntimeInventoryItem& item : inventory)
            if (item.mBase == base && item.mCount > 0)
                result = saturatingAdd(result, item.mCount);
        return result;
    }

    bool addInventoryItem(std::vector<RuntimeInventoryItem>& inventory, RuntimeInventoryItem item)
    {
        if (item.mBase.isNull() || item.mCount <= 0 || !std::isfinite(item.mCharge)
            || !std::isfinite(item.mRemainingUsageTime))
            return false;
        item.mEquippedSlots &= InventorySlotMask;
        if (item.mHotkey < -1 || item.mHotkey > 7)
            item.mHotkey = -1;
        const auto found = std::find_if(inventory.begin(), inventory.end(),
            [&](const RuntimeInventoryItem& existing) { return sameStack(existing, item); });
        if (found == inventory.end())
            inventory.push_back(std::move(item));
        else
            found->mCount = saturatingAdd(found->mCount, item.mCount);
        return true;
    }

    std::int32_t removeInventoryItem(
        std::vector<RuntimeInventoryItem>& inventory, const ESM::FormKey& base, std::int32_t count)
    {
        if (base.isNull() || count <= 0)
            return 0;
        std::int32_t removed = 0;
        // Prefer unbound, unequipped stacks so scripted removal does not
        // silently disturb the player's chosen equipment.
        std::stable_sort(inventory.begin(), inventory.end(), [&](const auto& left, const auto& right) {
            const auto priority = [&](const RuntimeInventoryItem& item) {
                return std::tuple(item.mBase != base, item.mEquippedSlots != 0, item.mHotkey >= 0);
            };
            return priority(left) < priority(right);
        });
        for (RuntimeInventoryItem& item : inventory)
        {
            if (item.mBase != base || removed == count)
                continue;
            const std::int32_t take = std::min(item.mCount, count - removed);
            item.mCount -= take;
            removed += take;
        }
        normalizeInventory(inventory);
        return removed;
    }

    std::int32_t transferInventoryItem(std::vector<RuntimeInventoryItem>& source,
        std::vector<RuntimeInventoryItem>& destination, const ESM::FormKey& base, std::int32_t count)
    {
        if (base.isNull() || count <= 0)
            return 0;
        std::vector<RuntimeInventoryItem> moved;
        std::int32_t remaining = count;
        for (RuntimeInventoryItem& item : source)
        {
            if (item.mBase != base || remaining == 0)
                continue;
            const std::int32_t take = std::min(item.mCount, remaining);
            RuntimeInventoryItem copy = item;
            copy.mCount = take;
            // Equipment and hotkeys describe the source actor, not the item.
            copy.mEquippedSlots = 0;
            copy.mHotkey = -1;
            moved.push_back(std::move(copy));
            item.mCount -= take;
            remaining -= take;
        }
        normalizeInventory(source);
        for (RuntimeInventoryItem& item : moved)
            addInventoryItem(destination, std::move(item));
        return count - remaining;
    }

    bool equipInventoryItem(std::vector<RuntimeInventoryItem>& inventory,
        const InventoryItemDefinition& definition, std::uint32_t preferredSlot)
    {
        std::uint32_t slots = definition.mSlots & InventorySlotMask;
        if (definition.mBase.isNull() || slots == 0)
            return false;
        if (definition.mChooseOneSlot)
        {
            const std::uint32_t preferred = preferredSlot & slots;
            std::uint32_t occupied = 0;
            for (const RuntimeInventoryItem& item : inventory)
                occupied |= item.mEquippedSlots;
            const std::uint32_t available = slots & ~occupied;
            slots = preferred != 0 ? firstBit(preferred)
                                   : (available != 0 ? firstBit(available) : firstBit(slots));
        }

        auto found = std::find_if(inventory.begin(), inventory.end(), [&](const RuntimeInventoryItem& item) {
            return item.mBase == definition.mBase && item.mCount > 0 && item.mEquippedSlots == 0;
        });
        if (found == inventory.end())
            found = std::find_if(inventory.begin(), inventory.end(), [&](const RuntimeInventoryItem& item) {
                return item.mBase == definition.mBase && item.mCount > 0 && (item.mEquippedSlots & slots) != 0;
            });
        if (found == inventory.end())
            return false;

        for (RuntimeInventoryItem& item : inventory)
            if ((item.mEquippedSlots & slots) != 0)
                item.mEquippedSlots = 0;

        found = std::find_if(inventory.begin(), inventory.end(), [&](const RuntimeInventoryItem& item) {
            return item.mBase == definition.mBase && item.mCount > 0 && item.mEquippedSlots == 0;
        });
        if (found == inventory.end())
            found = std::find_if(inventory.begin(), inventory.end(), [&](const RuntimeInventoryItem& item) {
                return item.mBase == definition.mBase && item.mCount > 0 && (item.mEquippedSlots & slots) != 0;
            });
        if (found == inventory.end())
            return false;
        if (found->mCount > 1 && slots != InventorySlotAmmunition)
        {
            RuntimeInventoryItem equipped = *found;
            equipped.mCount = 1;
            equipped.mEquippedSlots = slots;
            equipped.mHotkey = -1;
            --found->mCount;
            inventory.push_back(std::move(equipped));
        }
        else
            found->mEquippedSlots = slots;
        normalizeInventory(inventory);
        return true;
    }

    bool unequipInventoryItem(std::vector<RuntimeInventoryItem>& inventory, const ESM::FormKey& base)
    {
        bool changed = false;
        for (RuntimeInventoryItem& item : inventory)
            if (item.mBase == base && item.mEquippedSlots != 0)
            {
                item.mEquippedSlots = 0;
                changed = true;
            }
        normalizeInventory(inventory);
        return changed;
    }

    bool setInventoryHotkey(
        std::vector<RuntimeInventoryItem>& inventory, const ESM::FormKey& base, std::int32_t hotkey)
    {
        if (hotkey < -1 || hotkey > 7)
            return false;
        auto found = std::find_if(inventory.begin(), inventory.end(),
            [&](const RuntimeInventoryItem& item) { return item.mBase == base && item.mCount > 0; });
        if (found == inventory.end())
            return false;
        if (hotkey >= 0)
            for (RuntimeInventoryItem& item : inventory)
                if (item.mHotkey == hotkey)
                    item.mHotkey = -1;
        for (RuntimeInventoryItem& item : inventory)
            if (item.mBase == base)
                item.mHotkey = -1;
        found = std::find_if(inventory.begin(), inventory.end(),
            [&](const RuntimeInventoryItem& item) { return item.mBase == base && item.mCount > 0; });
        found->mHotkey = static_cast<std::int8_t>(hotkey);
        normalizeInventory(inventory);
        return true;
    }

    bool consumeInventoryItem(std::vector<RuntimeInventoryItem>& inventory, const ESM::FormKey& base)
    {
        return removeInventoryItem(inventory, base, 1) == 1;
    }

    bool repairInventoryItem(RuntimeInventoryItem& item, const InventoryItemDefinition& definition)
    {
        if (item.mBase != definition.mBase || definition.mMaxCondition < 0 || item.mCondition < 0
            || item.mCondition >= definition.mMaxCondition)
            return false;
        item.mCondition = definition.mMaxCondition;
        return true;
    }

    bool rechargeInventoryItem(RuntimeInventoryItem& item, const InventoryItemDefinition& definition)
    {
        if (item.mBase != definition.mBase || definition.mMaxCharge < 0.f || item.mCharge < 0.f
            || item.mCharge >= definition.mMaxCharge)
            return false;
        item.mCharge = definition.mMaxCharge;
        return true;
    }

    float effectiveBarterSkill(const BarterActor& actor, const BarterParameters& parameters)
    {
        return std::clamp(actor.mMercantile + parameters.mActorLuckSkillMult * (actor.mLuck - 50.f), 0.f, 100.f);
    }

    std::int32_t barterOffer(std::int32_t baseValue, bool playerBuys, const BarterActor& player,
        const BarterActor& merchant, float disposition, const BarterParameters& parameters)
    {
        if (baseValue <= 0)
            return 0;
        const float playerSkill = effectiveBarterSkill(player, parameters);
        const float merchantSkill = effectiveBarterSkill(merchant, parameters);
        const float dispositionFactor = (std::clamp(disposition, 0.f, 100.f) - 50.f) / 100.f;
        float multiplier = playerBuys ? parameters.mBuyBase + parameters.mBuyMult * (playerSkill - merchantSkill)
                                      : parameters.mSellBase + parameters.mSellMult * (playerSkill - merchantSkill);
        multiplier += playerBuys ? -dispositionFactor * 0.2f : dispositionFactor * 0.2f;
        const float raw = baseValue * std::max(playerBuys ? 1.f : 0.f, multiplier);
        return std::max(1, static_cast<std::int32_t>(std::lround(raw)));
    }

    float barterAcceptance(const BarterActor& player, const BarterActor& merchant, float disposition,
        float sliderPosition, const BarterParameters& parameters)
    {
        const float dispositionTerm = parameters.mHaggleDispositionMult
            * std::floor(parameters.mActorLuckSkillMult * (std::clamp(disposition, 0.f, 100.f) - 10.f) / 4.f);
        const float skillTerm = 100.f + effectiveBarterSkill(player, parameters)
            - effectiveBarterSkill(merchant, parameters);
        return dispositionTerm + skillTerm / 10.f
            - std::clamp(sliderPosition, 0.f, 40.f) * parameters.mHaggleBase;
    }

    std::int32_t repairCost(
        std::int32_t baseValue, std::int32_t condition, std::int32_t maxCondition, float costMultiplier)
    {
        if (baseValue <= 0 || maxCondition <= 0 || condition >= maxCondition || costMultiplier <= 0.f)
            return 0;
        const float missing = static_cast<float>(maxCondition - std::max(0, condition)) / maxCondition;
        return std::max(1, static_cast<std::int32_t>(std::ceil(baseValue * missing * costMultiplier)));
    }

    std::int32_t rechargeCost(float charge, float maxCharge, float costMultiplier)
    {
        if (!std::isfinite(charge) || !std::isfinite(maxCharge) || maxCharge <= 0.f || charge >= maxCharge
            || costMultiplier <= 0.f)
            return 0;
        return std::max(1,
            static_cast<std::int32_t>(std::ceil((maxCharge - std::max(0.f, charge)) * costMultiplier)));
    }

    std::int32_t trainingCost(std::int32_t currentSkill, float costMultiplier)
    {
        if (currentSkill < 0 || currentSkill >= 100 || costMultiplier <= 0.f)
            return 0;
        return std::max(0, static_cast<std::int32_t>(std::lround(currentSkill * costMultiplier)));
    }

    float lockpickAutoChance(std::int32_t security, float agility, float luck, std::int32_t lockLevel,
        float autoBase, float difficultyMultiplier, float autoOffset)
    {
        const float actorSkill = std::clamp(static_cast<float>(security), 0.f, 100.f)
            + 0.2f * std::clamp(agility, 0.f, 100.f) + 0.1f * std::clamp(luck, 0.f, 100.f);
        const float difficulty = std::max(0, lockLevel) * (1.f + difficultyMultiplier);
        return std::clamp(autoBase + actorSkill + autoOffset - difficulty, 0.f, 100.f);
    }
}
