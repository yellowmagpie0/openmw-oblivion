#ifndef OPENMW_MWWORLD_PREPAREDINITIALINVENTORY_H
#define OPENMW_MWWORLD_PREPAREDINITIALINVENTORY_H

#include <memory>
#include <map>
#include <vector>

#include <components/misc/rng.hpp>
#include "ptrregistry.hpp"
#include "localscripts.hpp"

namespace ESM
{
    struct InventoryList;
    class RefId;
}

namespace MWWorld
{
    class ContainerStore;
    template <class> class ContainerStoreIteratorBase;
    class ESMStore;

    // Owns initial-item prototypes, selection workspace and detached stack nodes.
    // Selection, registry publication and autoequip remain at their live phase.
    class PreparedInitialInventory
    {
        friend class ContainerStore;
        struct Data;
        std::shared_ptr<const Data> mData;
        std::vector<std::size_t> mCandidates;
        std::vector<std::unique_ptr<ContainerStore>> mStacks;
        std::map<ESM::RefId, std::unique_ptr<ContainerStore>> mSplitStacks;
        std::map<ESM::RefId, std::list<LocalScripts::PreparedRegistration>> mSplitScripts;
        PtrRegistry::PreparedInsertions mRegistrations;
        void addSplitScript(const Ptr& ptr);
        bool mConsumed = false;
        ContainerStoreIteratorBase<Ptr> takeSplitStack(ContainerStore& target, const Ptr& source, int count);

    public:
        PreparedInitialInventory(const ESM::InventoryList& items, const ESM::RefId& owner,
            const ESMStore& content, const ESMStore& incoming, bool equipment = false, const ESM::RefId* race = nullptr);
        ~PreparedInitialInventory();
        PreparedInitialInventory(const PreparedInitialInventory&);
        PreparedInitialInventory& operator=(const PreparedInitialInventory&);
        PreparedInitialInventory(PreparedInitialInventory&&) noexcept;
        PreparedInitialInventory& operator=(PreparedInitialInventory&&) noexcept;
        std::size_t pendingStackCount() const;
        std::size_t pendingSplitCount() const;
        const ESMStore& autoEquipStore() const;
        void fill(ContainerStore& target, const ESM::RefId& owner, Misc::Rng::Generator& prng);
    };
}

#endif
