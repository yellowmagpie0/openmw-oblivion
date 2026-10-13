#ifndef OPENMW_MWWORLD_PREPAREDINITIALINVENTORY_H
#define OPENMW_MWWORLD_PREPAREDINITIALINVENTORY_H

#include <memory>
#include <vector>

#include <components/misc/rng.hpp>

namespace ESM
{
    struct InventoryList;
    class RefId;
}

namespace MWWorld
{
    class ContainerStore;
    class ESMStore;

    // Owns initial-item definitions and leveled-list selection storage. Item
    // insertion, registry publication and autoequip remain at their live phase.
    class PreparedInitialInventory
    {
        struct Data;
        std::shared_ptr<const Data> mData;
        std::vector<std::size_t> mCandidates;
        bool mConsumed = false;

    public:
        PreparedInitialInventory(const ESM::InventoryList& items, const ESM::RefId& owner,
            const ESMStore& content, const ESMStore& incoming);
        PreparedInitialInventory(const PreparedInitialInventory&) = default;
        void fill(ContainerStore& target, const ESM::RefId& owner, Misc::Rng::Generator& prng);
    };
}

#endif
