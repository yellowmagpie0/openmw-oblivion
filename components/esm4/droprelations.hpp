#ifndef OPENMW_COMPONENTS_ESM4_DROPRELATIONS_H
#define OPENMW_COMPONENTS_ESM4_DROPRELATIONS_H

#include <components/esm/formkey.hpp>

#include <algorithm>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

namespace ESM4
{
    // Owned runtime ExtraItemDropper / ExtraDroppedItemList identities. List
    // order is native insertion order. Absence differs from an empty extra.
    // These are instance extras, never authored record fields or inventory slots.
    struct DropRelations
    {
        std::optional<ESM::FormKey> mDropper;
        std::optional<std::vector<ESM::FormKey>> mItems;

        bool empty() const noexcept { return !mDropper && !mItems; }
        // Original424C00/65C620 removes the first match and detaches an
        // existing empty list, even when the target was absent. The item's
        // separate dropper extra survives reference finalization.
        void removeItem(const ESM::FormKey& item)
        {
            if (item.isNull() || !mItems) return;
            const auto found = std::find(mItems->begin(), mItems->end(), item);
            if (found != mItems->end()) mItems->erase(found);
            if (mItems->empty()) mItems.reset();
        }
        void validate() const
        {
            const auto key = [](const ESM::FormKey& value) {
                if (value.isNull() || ESM::FormKey::deserialize(value.serialize()) != value)
                    throw std::runtime_error("Invalid native drop relation identity");
            };
            if (mDropper) key(*mDropper);
            if (mItems)
            {
                if (mItems->size() > 1000000)
                    throw std::runtime_error("Native dropped item list exceeds size limit");
                std::set<ESM::FormKey> seen;
                for (const auto& item : *mItems)
                {
                    key(item);
                    if (!seen.insert(item).second)
                        throw std::runtime_error("Duplicate native dropped item identity");
                }
            }
        }
        friend bool operator==(const DropRelations&, const DropRelations&) = default;
    };
}

#endif
