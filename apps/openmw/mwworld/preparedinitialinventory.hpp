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

    // Owns initial-item prototypes, selection workspace and detached stack nodes.
    // Selection, registry publication and autoequip remain at their live phase.
    class PreparedInitialInventory
    {
        struct Data;
        std::shared_ptr<const Data> mData;
        std::vector<std::size_t> mCandidates;
        std::vector<std::unique_ptr<ContainerStore>> mStacks;
        bool mConsumed = false;

    public:
        PreparedInitialInventory(const ESM::InventoryList& items, const ESM::RefId& owner,
            const ESMStore& content, const ESMStore& incoming);
        ~PreparedInitialInventory();
        PreparedInitialInventory(const PreparedInitialInventory&);
        PreparedInitialInventory& operator=(const PreparedInitialInventory&);
        PreparedInitialInventory(PreparedInitialInventory&&) noexcept;
        PreparedInitialInventory& operator=(PreparedInitialInventory&&) noexcept;
        std::size_t pendingStackCount() const;
        void fill(ContainerStore& target, const ESM::RefId& owner, Misc::Rng::Generator& prng);
    };
}

#endif
