#include "preparedinitialinventory.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>

#include <components/debug/debuglog.hpp>
#include <components/esm3/loadcont.hpp>
#include <components/esm3/loadlevlist.hpp>

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
    };

    PreparedInitialInventory::PreparedInitialInventory(const ESM::InventoryList& items, const ESM::RefId& owner,
        const ESMStore& content, const ESMStore& incoming)
    {
        auto data = std::make_shared<Data>();
        data->mItems = items;
        data->mOwner = owner;
        std::size_t capacity = 0;
        bool gold = false;
        const std::function<void(const ESM::RefId&)> visit = [&](const ESM::RefId& id) {
            auto [entry, inserted] = data->mNodes.try_emplace(id);
            if (!inserted) return; // Shared sublists and cycles have one snapshot.
            auto& node = entry->second;
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
                gold = gold || ptr.getClass().isGold(ptr);
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
        mCandidates.resize(capacity); // Copies retain full workspace, even before first selection.
        mData = std::move(data);
    }

    void PreparedInitialInventory::fill(ContainerStore& target, const ESM::RefId& owner, Misc::Rng::Generator& prng)
    {
        if (mConsumed || owner != mData->mOwner)
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
                auto item = target.addImp(node.mRef->getPtr(), quantity, false, &gold, &node.mCharge);
                MWBase::Environment::get().getWorldModel()->registerPtr(*item);
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
