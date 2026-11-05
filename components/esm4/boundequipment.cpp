#include "boundequipment.hpp"

#include "inventorymechanics.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace ESM4
{
    namespace
    {
        bool canonicalContent(const ESM::FormKey& key)
        {
            return key.isContent() && ESM::FormKey::deserialize(key.serialize()) == key;
        }

        bool sameOrdinaryInstance(RuntimeInventoryItem left, RuntimeInventoryItem right)
        {
            left.mCount = right.mCount = 1;
            left.mEquippedSlots = right.mEquippedSlots = 0;
            return !left.mBoundSource && !right.mBoundSource && left == right;
        }
    }

    BoundEquipmentUpdate prepareBoundEquipmentUpdate(const std::vector<RuntimeInventoryItem>& inventory,
        const std::vector<RuntimeBoundAbility>& abilities, const std::vector<BoundEquipmentGrant>& grants, bool dead)
    {
        std::map<ESM::FormKey, const RuntimeBoundAbility*> owners;
        for (const auto& ability : abilities)
        {
            if (!canonicalContent(ability.mSpell) || !owners.emplace(ability.mSpell, &ability).second)
                throw std::invalid_argument("invalid or duplicate bound equipment owner");
            for (const auto& displaced : ability.mDisplacedEquipment)
                if (displaced.mCount != 1 || !displaced.mEquippedSlots || displaced.mBoundSource)
                    throw std::invalid_argument("invalid displaced bound equipment descriptor");
        }
        std::map<BoundItemSource, const RuntimeInventoryItem*> sources;
        for (const auto& item : inventory)
            if (item.mBoundSource)
            {
                if (item.mCount != 1 || item.mHotkey != -1 || !owners.contains(item.mBoundSource->mSpell)
                    || !sources.emplace(*item.mBoundSource, &item).second)
                    throw std::invalid_argument("orphan or duplicate bound equipment instance");
            }
        for (const auto& [spell, owner] : owners)
            if (std::none_of(sources.begin(), sources.end(), [&](const auto& entry) {
                    return entry.first.mSpell == spell;
                }))
                throw std::invalid_argument("bound equipment owner has no materialized instances");

        std::set<ESM::FormKey> wanted;
        std::set<BoundItemSource> expected;
        std::uint32_t grantSlots = 0;
        for (const auto& grant : grants)
        {
            if (!canonicalContent(grant.mSpell) || grant.mItems.empty() || !wanted.insert(grant.mSpell).second)
                throw std::invalid_argument("invalid or duplicate bound equipment grant");
            for (const auto& item : grant.mItems)
            {
                if (!canonicalContent(item.mBase) || item.mCount != 1 || item.mHotkey != -1
                    || !item.mEquippedSlots || (item.mEquippedSlots & ~InventorySlotMask)
                    || (grantSlots & item.mEquippedSlots) || !item.mBoundSource
                    || item.mBoundSource->mSpell != grant.mSpell || !expected.insert(*item.mBoundSource).second)
                    throw std::invalid_argument("invalid or overlapping bound equipment grant item");
                grantSlots |= item.mEquippedSlots;
                if (owners.contains(grant.mSpell))
                {
                    const auto found = sources.find(*item.mBoundSource);
                    if (found == sources.end() || found->second->mBase != item.mBase
                        || (found->second->mEquippedSlots & ~item.mEquippedSlots))
                        throw std::invalid_argument("saved bound equipment differs from installed definition");
                }
            }
        }
        for (const auto& [source, item] : sources)
            if (wanted.contains(source.mSpell) && !expected.contains(source))
                throw std::invalid_argument("saved bound equipment effect is absent from installed ability");

        BoundEquipmentUpdate result{inventory, {}};
        std::vector<RuntimeInventoryItem> restore;
        for (const auto& ability : abilities)
        {
            if (!dead && wanted.contains(ability.mSpell))
                result.mAbilities.push_back(ability);
            else
                restore.insert(restore.end(), ability.mDisplacedEquipment.begin(), ability.mDisplacedEquipment.end());
        }
        std::erase_if(result.mInventory, [&](const auto& item) {
            return item.mBoundSource && (dead || !wanted.contains(item.mBoundSource->mSpell));
        });

        // Restore existing ordinary units only. If another bound effect still
        // occupies part of a multi-slot garment, retain its restoration record
        // with that owner until the remaining effect is removed too.
        for (const auto& displaced : restore)
        {
            const auto blocker = std::find_if(result.mInventory.begin(), result.mInventory.end(), [&](const auto& item) {
                return item.mBoundSource && (item.mEquippedSlots & displaced.mEquippedSlots);
            });
            if (blocker != result.mInventory.end())
            {
                auto owner = std::find_if(result.mAbilities.begin(), result.mAbilities.end(), [&](const auto& ability) {
                    return ability.mSpell == blocker->mBoundSource->mSpell;
                });
                owner->mDisplacedEquipment.push_back(displaced);
                continue;
            }
            const auto existing = std::find_if(result.mInventory.begin(), result.mInventory.end(), [&](const auto& item) {
                return item.mCount > 0 && !item.mEquippedSlots && sameOrdinaryInstance(item, displaced);
            });
            if (existing == result.mInventory.end())
                continue; // The original unit may have been removed; do not recreate it.
            const auto index = static_cast<std::size_t>(existing - result.mInventory.begin());
            for (auto& item : result.mInventory)
                if (item.mEquippedSlots & displaced.mEquippedSlots) item.mEquippedSlots = 0;
            if (result.mInventory[index].mCount == 1)
                result.mInventory[index].mEquippedSlots = displaced.mEquippedSlots;
            else
            {
                auto unit = result.mInventory[index];
                --result.mInventory[index].mCount;
                unit.mCount = 1;
                unit.mEquippedSlots = displaced.mEquippedSlots;
                result.mInventory.push_back(std::move(unit));
            }
        }

        if (!dead)
            for (const auto& grant : grants)
            {
                if (owners.contains(grant.mSpell)) continue;
                RuntimeBoundAbility owner{grant.mSpell, {}};
                std::uint32_t slots = 0;
                for (const auto& item : grant.mItems) slots |= item.mEquippedSlots;
                for (auto& item : result.mInventory)
                    if (item.mEquippedSlots & slots)
                    {
                        auto displaced = item;
                        displaced.mCount = 1;
                        owner.mDisplacedEquipment.push_back(std::move(displaced));
                        item.mEquippedSlots = 0;
                    }
                result.mInventory.insert(result.mInventory.end(), grant.mItems.begin(), grant.mItems.end());
                result.mAbilities.push_back(std::move(owner));
            }
        return result;
    }
}
