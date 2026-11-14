#include "worlditemmodel.hpp"
#include "../mwworld/worldimp.hpp"

namespace MWGui
{
    std::optional<MWWorld::Ptr> WorldItemModel::tryMoveNativeScriptedItem(
        const MWWorld::Ptr& actor, const ItemStack& item, size_t count)
    {
        auto* world = dynamic_cast<MWWorld::World*>(&*MWBase::Environment::get().getWorld());
        return world ? world->dropOblivionInventoryIngredient(actor, item.mBase, count, mCursorX, mCursorY)
                     : std::nullopt;
    }
}
