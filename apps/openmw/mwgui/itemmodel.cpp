#include "itemmodel.hpp"
#include "worlditemmodel.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/oblivionprofileservices.hpp"
#include "../mwworld/oblivionscriptmanager.hpp"
#include "../mwworld/worldimp.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"

namespace MWGui
{

    ItemStack::ItemStack(const MWWorld::Ptr& base, ItemModel* creator, size_t count)
        : mType(Type_Normal)
        , mFlags(0)
        , mCreator(creator)
        , mCount(count)
        , mBase(base)
    {
        if (!base.getClass().getEnchantment(base).empty())
            mFlags |= Flag_Enchanted;

        if (MWBase::Environment::get().getMechanicsManager()->isBoundItem(base))
            mFlags |= Flag_Bound;

        auto* world = MWBase::Environment::get().getWorldOrNull();
        if (world && world->getGameProfile() == ESM::GameProfile::Oblivion
            && base.getContainerStore()
            == &world->getPlayerPtr().getClass().getContainerStore(world->getPlayerPtr()))
        {
            const auto& store = world->getStore();
            const auto definition = MWWorld::OblivionProfileServices::itemDefinition(
                store, MWWorld::OblivionProfileServices::nativeItemId(store, base.getCellRef().getRefId()),
                dynamic_cast<const MWWorld::World*>(world));
            if (definition && definition->mQuestItem)
                mFlags |= Flag_Quest;
        }
    }

    ItemStack::ItemStack()
        : mType(Type_Normal)
        , mFlags(0)
        , mCreator(nullptr)
        , mCount(0)
    {
    }

    bool operator==(const ItemStack& left, const ItemStack& right)
    {
        if (left.mType != right.mType)
            return false;

        if (left.mBase == right.mBase)
            return true;

        // If one of the items is in an inventory and currently equipped, we need to check stacking both ways to be sure
        if (left.mBase.getContainerStore() && right.mBase.getContainerStore())
            return left.mBase.getContainerStore()->stacks(left.mBase, right.mBase)
                && right.mBase.getContainerStore()->stacks(left.mBase, right.mBase);

        if (left.mBase.getContainerStore())
            return left.mBase.getContainerStore()->stacks(left.mBase, right.mBase);
        if (right.mBase.getContainerStore())
            return right.mBase.getContainerStore()->stacks(left.mBase, right.mBase);

        MWWorld::ContainerStore store;
        return store.stacks(left.mBase, right.mBase);
    }

    ItemModel::ItemModel() {}

    MWWorld::Ptr ItemModel::moveItem(const ItemStack& item, size_t count, ItemModel* otherModel, bool allowAutoEquip)
    {
        if ((item.mFlags & ItemStack::Flag_Quest) && otherModel != this)
        {
            MWBase::Environment::get().getWindowManager()->messageBox("#{Interface:QuestItemRemovalDenied}");
            return {};
        }
        // Inventory GUI wrappers also enter here directly. Prepare native
        // ownership before any wrapper can debit the selected source stack.
        if (auto* world = dynamic_cast<WorldItemModel*>(otherModel))
            if (auto moved = world->tryMoveNativeInventoryItem(
                    MWBase::Environment::get().getWorld()->getPlayerPtr(), item, count))
                return *moved;
        const auto* sourceStore = item.mBase.getContainerStore();
        const auto source = sourceStore ? sourceStore->getPtr() : MWWorld::Ptr{};
        removeItem(item, count);
        // Preserve the RefNum when the whole stack moves.
        const auto added = item.mBase.getCellRef().getCount() == 0
            ? otherModel->addItem(item, count, allowAutoEquip)
            : otherModel->copyItem(item, count, allowAutoEquip);
        if (!source.isEmpty() && !added.isEmpty() && added.getContainerStore() && added.getContainerStore() != sourceStore)
            if (auto* world = dynamic_cast<MWWorld::World*>(MWBase::Environment::get().getWorldOrNull());
                world && world->getGameProfile() == ESM::GameProfile::Oblivion)
                if (auto* scripts = world->getOblivionScriptManager())
                    return scripts->dispatchLootedItem(source, added);
        return added;
    }

    bool ItemModel::allowedToUseItems() const
    {
        return true;
    }

    bool ItemModel::onDropItem(const MWWorld::Ptr& item, int count)
    {
        return true;
    }

    bool ItemModel::onTakeItem(const MWWorld::Ptr& item, int count)
    {
        return true;
    }

    bool ProxyItemModel::allowedToUseItems() const
    {
        return mSourceModel->allowedToUseItems();
    }

    MWWorld::Ptr ProxyItemModel::copyItem(const ItemStack& item, size_t count, bool allowAutoEquip)
    {
        return mSourceModel->copyItem(item, count, allowAutoEquip);
    }

    void ProxyItemModel::removeItem(const ItemStack& item, size_t count)
    {
        mSourceModel->removeItem(item, count);
    }

    ItemModel::ModelIndex ProxyItemModel::mapToSource(ModelIndex index)
    {
        const ItemStack& itemToSearch = getItem(index);
        for (size_t i = 0; i < mSourceModel->getItemCount(); ++i)
        {
            const ItemStack& item = mSourceModel->getItem(static_cast<ModelIndex>(i));
            if (item.mBase == itemToSearch.mBase)
                return static_cast<ModelIndex>(i);
        }
        return -1;
    }

    ItemModel::ModelIndex ProxyItemModel::mapFromSource(ModelIndex index)
    {
        const ItemStack& itemToSearch = mSourceModel->getItem(index);
        for (size_t i = 0; i < getItemCount(); ++i)
        {
            const ItemStack& item = getItem(static_cast<ModelIndex>(i));
            if (item.mBase == itemToSearch.mBase)
                return static_cast<ModelIndex>(i);
        }
        return -1;
    }

    ItemModel::ModelIndex ProxyItemModel::getIndex(const ItemStack& item)
    {
        return mSourceModel->getIndex(item);
    }

    void ProxyItemModel::setSourceModel(std::unique_ptr<ItemModel> sourceModel)
    {
        mSourceModel = std::move(sourceModel);
    }

    void ProxyItemModel::onClose()
    {
        mSourceModel->onClose();
    }

    bool ProxyItemModel::onDropItem(const MWWorld::Ptr& item, int count)
    {
        return mSourceModel->onDropItem(item, count);
    }

    bool ProxyItemModel::onTakeItem(const MWWorld::Ptr& item, int count)
    {
        return mSourceModel->onTakeItem(item, count);
    }

    MWWorld::Ptr ProxyItemModel::addItem(const ItemStack& item, size_t count, bool allowAutoEquip)
    {
        return mSourceModel->addItem(item, count, allowAutoEquip);
    }

    bool ProxyItemModel::usesContainer(const MWWorld::Ptr& container)
    {
        return mSourceModel->usesContainer(container);
    }
}
