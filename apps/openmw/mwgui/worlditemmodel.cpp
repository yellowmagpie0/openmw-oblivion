#include "worlditemmodel.hpp"
#include "../mwworld/worldimp.hpp"

namespace MWGui
{
    std::optional<MWWorld::Ptr> WorldItemModel::tryMoveNativeInventoryItem(
        const MWWorld::Ptr& actor, const ItemStack& item, size_t count)
    {
        auto* world = dynamic_cast<MWWorld::World*>(&*MWBase::Environment::get().getWorld());
        if (!world) return std::nullopt;
        if (auto dropped = world->dropOblivionInventoryIngredient(actor, item.mBase, count, mCursorX, mCursorY))
            return dropped;
        return world->dropOblivionInventoryLooseItem(actor, item.mBase, count, mCursorX, mCursorY);
    }
}
