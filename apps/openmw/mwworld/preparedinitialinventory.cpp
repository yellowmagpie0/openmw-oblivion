#include "preparedinitialinventory.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

#include <components/debug/debuglog.hpp>
#include <components/esm3/loadcont.hpp>
#include <components/esm3/loadlevlist.hpp>
#include <components/esm3/loadrace.hpp>

#include "../mwbase/environment.hpp"
#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/spellutil.hpp"
#include "class.hpp"
#include "containerstore.hpp"
#include "esmstore.hpp"
#include "manualref.hpp"
#include "worldmodel.hpp"

namespace MWWorld
{
    struct PreparedInitialInventory::Data
    {
        struct Node
        {
            bool mExists = false;
            bool mList = false;
            bool mItemList = false;
            bool mEach = false;
            bool mAllLevels = false;
            bool mScripted = false;
            bool mGold = false;
            std::size_t mPool = 0;
            int mChanceNone = 0;
            std::vector<ESM::LevelledListBase::LevelItem> mItems;
            std::unique_ptr<ManualRef> mRef;
            std::string mError;
            std::function<float()> mCharge;
        };
        ESM::InventoryList mItems;
        ESM::RefId mOwner;
        std::map<ESM::RefId, Node> mNodes;
        std::unique_ptr<ManualRef> mGold;
        std::unique_ptr<ESMStore> mEquipmentStore;
    };

    PreparedInitialInventory::PreparedInitialInventory(const ESM::InventoryList& items, const ESM::RefId& owner,
        const ESMStore& content, const ESMStore& incoming, bool equipment, const ESM::RefId* race)
    {
        auto data = std::make_shared<Data>();
        data->mItems = items;
        data->mOwner = owner;
        if (equipment)
        {
            data->mEquipmentStore = std::make_unique<ESMStore>();
            // Own only restore-visible definitions. Missing or mistyped values
            // remain deferred to the same consumed branch as ordinary autoequip.
            for (const char* name : {"fUnarmoredBase1", "fUnarmoredBase2", "iHelmWeight", "iCuirassWeight",
                     "iPauldronWeight", "iGreavesWeight", "iBootsWeight", "iGauntletWeight", "iShieldWeight",
                     "fLightMaxMod", "fMedMaxMod", "iBaseArmorSkill"})
                if (const auto* value = content.searchForRestore<ESM::GameSetting>(ESM::RefId::stringRefId(name), incoming))
                    data->mEquipmentStore->getWritable<ESM::GameSetting>().insertStatic(*value);
            if (!race)
                if (const auto* npc = content.searchForRestore<ESM::NPC>(owner, incoming)) race = &npc->mRace;
            if (race)
                if (const auto* value = content.searchForRestore<ESM::Race>(*race, incoming))
                    data->mEquipmentStore->getWritable<ESM::Race>().insertStatic(*value);
        }
        std::size_t capacity = 0;
        bool gold = false;
        const std::function<void(const ESM::RefId&)> visit = [&](const ESM::RefId& id) {
            auto [entry, inserted] = data->mNodes.try_emplace(id);
            if (!inserted) return; // Shared sublists and cycles have one snapshot.
            auto& node = entry->second;
            node.mPool = data->mNodes.size() - 1;
            const int type = content.findForRestore(id, incoming);
            node.mExists = type != 0;
            const ESM::LevelledListBase* list = nullptr;
            if (type == ESM::REC_LEVI)
            {
                list = content.searchForRestore<ESM::ItemLevList>(id, incoming);
                node.mItemList = true;
            }
            else if (type == ESM::REC_LEVC)
                list = content.searchForRestore<ESM::CreatureLevList>(id, incoming);
            if (list)
            {
                node.mList = true;
                node.mEach = node.mItemList && (list->mFlags & ESM::ItemLevList::Each);
                node.mAllLevels = list->mFlags
                    & (node.mItemList ? int(ESM::ItemLevList::AllLevels) : int(ESM::CreatureLevList::AllLevels));
                node.mChanceNone = list->mChanceNone;
                node.mItems = list->mList;
                capacity = std::max(capacity, node.mItems.size());
                for (const auto& item : node.mItems) visit(item.mId);
                return;
            }
            try
            {
                node.mRef = std::make_unique<ManualRef>(content, id, 1, &incoming);
                const auto& ptr = node.mRef->getPtr();
                node.mScripted = !ptr.getClass().getScript(ptr).empty();
                node.mGold = ptr.getClass().isGold(ptr);
                gold = gold || node.mGold;
                ptr.getCellRef().setOwner(owner);
                const auto& enchantment = ptr.getClass().getEnchantment(ptr);
                if (!enchantment.empty())
                    node.mCharge = MWMechanics::prepareEnchantmentCharge(enchantment, content, incoming);
            }
            catch (const std::logic_error& error)
            {
                node.mRef.reset();
                node.mError = error.what();
            }
        };
        for (const auto& item : items.mList)
        {
            if (!item.mCount) continue;
            visit(item.mItem);
            const auto& node = data->mNodes.at(item.mItem);
            if (item.mCount == std::numeric_limits<int>::min()
                && (node.mScripted || node.mItemList
                    || (node.mRef && ContainerStore::isStorableType(node.mRef->getPtr().getType()))))
                throw std::runtime_error("Prepared initial inventory count has no representable magnitude");
        }
        if (gold && content.findForRestore(ContainerStore::sGoldId, incoming))
        {
            try
            {
                data->mGold = std::make_unique<ManualRef>(content, ContainerStore::sGoldId, 1, &incoming);
            }
            catch (const std::logic_error&)
            {
                // Ordinary initial fill warns and skips an unusable gold
                // normalization target only if that branch is consumed.
            }
        }
        // Reserve stack nodes for every selectable leaf without drawing RNG or
        // querying the Player. Pristine unscripted items share a single stack;
        // scripted items need a node per possible unit. A mismatched gold
        // normalization type cannot be found by the ordinary gold stacking scan.
        std::vector<std::size_t> counts(data->mNodes.size());
        for (const auto& item : items.mList)
        {
            if (!item.mCount) continue;
            const auto& root = data->mNodes.at(item.mItem);
            std::set<ESM::RefId> visited;
            const std::function<void(const ESM::RefId&, bool)> reserve = [&](const ESM::RefId& id, bool select) {
                if (!visited.insert(id).second) return;
                const auto& node = data->mNodes.at(id);
                if (node.mList)
                {
                    if (select && node.mChanceNone < 100)
                        for (const auto& entry : node.mItems) reserve(entry.mId, true);
                    return;
                }
                const auto* prototype = node.mGold ? data->mGold.get() : node.mRef.get();
                if (!prototype || !ContainerStore::isStorableType(prototype->getPtr().getType())) return;
                const bool goldStacks = node.mGold && node.mRef
                    && prototype->getPtr().getType() == node.mRef->getPtr().getType();
                auto& count = counts[node.mPool];
                if (goldStacks || (!node.mGold && !node.mScripted))
                    count = 1;
                else
                {
                    const auto extra = (root.mEach || node.mScripted) ? std::size_t(std::abs(item.mCount)) : 1;
                    if (extra > std::numeric_limits<std::size_t>::max() - count)
                        throw std::length_error("Prepared initial inventory stack capacity overflow");
                    count += extra;
                }
            };
            reserve(item.mItem, root.mItemList);
        }
        mStacks.resize(counts.size());
        for (const auto& [id, node] : data->mNodes)
        {
            if (!counts[node.mPool]) continue;
            const auto& prototype = node.mGold ? data->mGold->getPtr() : node.mRef->getPtr();
            auto pool = std::make_unique<ContainerStore>();
            for (std::size_t i = 0; i < counts[node.mPool]; ++i) pool->addNewStack(prototype, 1);
            mStacks[node.mPool] = std::move(pool);
            // Initial autoequip can split a plain item once per alternative
            // slot (twice for rings). It may later replace that candidate, so
            // reserve for all reachable leaves, not only eventual winners.
            const auto type = prototype.getType();
            if (equipment && (type == ESM::REC_WEAP || type == ESM::REC_ARMO || type == ESM::REC_CLOT))
            {
                const auto [slots, stackable] = prototype.getClass().getEquipmentSlots(prototype);
                if (!stackable && !slots.empty() && !mSplitStacks.contains(prototype.getCellRef().getRefId()))
                {
                    auto splits = std::make_unique<ContainerStore>();
                    for (std::size_t i = 0; i < slots.size(); ++i) splits->addNewStack(prototype, 1);
                    mSplitStacks.emplace(prototype.getCellRef().getRefId(), std::move(splits));
                }
            }
        }
        mRegistrations = MWBase::Environment::get().getWorldModel()->preparePtrInsertions(
            pendingStackCount() + pendingSplitCount());
        mCandidates.resize(capacity); // Copies retain full workspace, even before first selection.
        mData = std::move(data);
    }

    PreparedInitialInventory::~PreparedInitialInventory() = default;
    PreparedInitialInventory::PreparedInitialInventory(PreparedInitialInventory&&) noexcept = default;
    PreparedInitialInventory& PreparedInitialInventory::operator=(PreparedInitialInventory&& other) noexcept
    {
        if (this != &other)
        {
            mData = std::move(other.mData);
            mCandidates = std::move(other.mCandidates);
            mStacks = std::move(other.mStacks);
            mSplitStacks = std::move(other.mSplitStacks);
            mRegistrations = std::move(other.mRegistrations);
            mConsumed = std::exchange(other.mConsumed, true);
        }
        return *this;
    }

    PreparedInitialInventory::PreparedInitialInventory(const PreparedInitialInventory& other)
        : mData(other.mData), mCandidates(other.mCandidates), mRegistrations(other.mRegistrations)
        , mConsumed(other.mConsumed)
    {
        mStacks.reserve(other.mStacks.size());
        for (const auto& pool : other.mStacks)
            mStacks.push_back(pool ? std::make_unique<ContainerStore>(*pool) : nullptr);
        for (const auto& [id, pool] : other.mSplitStacks)
            mSplitStacks.emplace(id, std::make_unique<ContainerStore>(*pool));
    }

    PreparedInitialInventory& PreparedInitialInventory::operator=(const PreparedInitialInventory& other)
    {
        if (this != &other)
        {
            PreparedInitialInventory copy(other);
            *this = std::move(copy);
        }
        return *this;
    }

    const ESMStore& PreparedInitialInventory::autoEquipStore() const
    {
        if (!mData || !mData->mEquipmentStore)
            throw std::logic_error("Prepared initial autoequip dependencies are unavailable");
        return *mData->mEquipmentStore;
    }

    std::size_t PreparedInitialInventory::pendingStackCount() const
    {
        std::size_t result = 0;
        for (const auto& pool : mStacks)
            if (pool) result += std::distance(pool->begin(), pool->end());
        return result;
    }

    std::size_t PreparedInitialInventory::pendingSplitCount() const
    {
        std::size_t result = 0;
        for (const auto& [id, pool] : mSplitStacks) result += std::distance(pool->begin(), pool->end());
        return result;
    }

    ContainerStoreIterator PreparedInitialInventory::takeSplitStack(
        ContainerStore& target, const Ptr& source, int count)
    {
        const auto found = mSplitStacks.find(source.getCellRef().getRefId());
        if (!mConsumed || source.getContainerStore() != &target || found == mSplitStacks.end())
            throw std::logic_error("Prepared initial autoequip split is unavailable");
        return target.movePreparedStack(source, count, *found->second);
    }

    void PreparedInitialInventory::fill(ContainerStore& target, const ESM::RefId& owner, Misc::Rng::Generator& prng)
    {
        if (!mData || mConsumed || owner != mData->mOwner)
            throw std::logic_error("Prepared initial inventory consumed or bound to a different owner");
        mConsumed = true;
        const auto select = [&](ESM::RefId id) -> const Data::Node* {
            const auto& player = MWMechanics::getPlayer();
            const int level = player.getClass().getCreatureStats(player).getLevel();
            for (;;)
            {
                const auto& node = mData->mNodes.at(id);
                if (Misc::Rng::roll0to99(prng) < node.mChanceNone) return nullptr;
                int highest = 0;
                for (const auto& item : node.mItems)
                    if (item.mLevel > highest && item.mLevel <= level) highest = item.mLevel;
                std::size_t count = 0;
                for (std::size_t i = 0; i < node.mItems.size(); ++i)
                    if (level >= node.mItems[i].mLevel && (node.mAllLevels || node.mItems[i].mLevel == highest))
                        mCandidates[count++] = i;
                if (!count) return nullptr;
                const auto& item = node.mItems[mCandidates[Misc::Rng::rollDice(count, prng)]];
                const auto& next = mData->mNodes.at(item.mId);
                if (!next.mExists)
                {
                    Log(Debug::Warning) << "Warning: ignoring nonexistent item " << item.mId << " in levelled list " << id;
                    return nullptr;
                }
                if (!next.mList) return &next;
                id = item.mId;
            }
        };
        const Ptr gold = mData->mGold ? mData->mGold->getPtr() : Ptr{};
        const auto insert = [&](const Data::Node& node, int count) {
            if (!node.mRef) throw std::logic_error(node.mError.empty() ? "Initial item is not storable" : node.mError);
            const auto add = [&](int quantity) {
                auto item = target.addImp(node.mRef->getPtr(), quantity, false, &gold, &node.mCharge,
                    mStacks[node.mPool].get());
                MWBase::Environment::get().getWorldModel()->registerPreparedPtr(*item, mRegistrations);
            };
            if (node.mScripted)
                for (int i = 0; i < std::abs(count); ++i) add(count < 0 ? -1 : 1);
            else add(count);
        };
        for (const auto& item : mData->mItems.mList)
        {
            if (!item.mCount) continue;
            const auto& root = mData->mNodes.at(item.mItem);
            const int repetitions = root.mEach ? std::abs(item.mCount) : 1;
            for (int i = 0; i < repetitions; ++i)
            {
                try
                {
                    const auto* node = root.mItemList ? select(item.mItem) : &root;
                    if (node) insert(*node, root.mEach ? (item.mCount < 0 ? -1 : 1) : item.mCount);
                }
                catch (const std::exception& error)
                {
                    Log(Debug::Warning) << "Warning: MWWorld::ContainerStore::addInitialItem: " << error.what();
                }
            }
        }
    }
}
