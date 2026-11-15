#ifndef OPENMW_MWWORLD_OBLIVIONMISCCONSUMABLES_H
#define OPENMW_MWWORLD_OBLIVIONMISCCONSUMABLES_H

#include <memory>

namespace ESM4 { struct MiscItem; }
namespace MWBase { class World; }

namespace MWWorld
{
    class Action;
    class ConstPtr;
    class ESMStore;
    class Ptr;

    // Original built-in MISC role, independent of display/editor names.
    bool isOblivionWelkyndStone(const ESMStore& store, const ESM4::MiscItem& item);
    bool isOblivionWelkyndItem(MWBase::World& world, const ConstPtr& item);
    std::unique_ptr<Action> createOblivionMiscConsumableUse(MWBase::World& world, const Ptr& item);
}

#endif
