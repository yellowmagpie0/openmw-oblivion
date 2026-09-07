#ifndef OPENMW_COMPONENTS_ESM4_IDLETREE_H
#define OPENMW_COMPONENTS_ESM4_IDLETREE_H

#include "loadidle.hpp"

#include <map>
#include <set>
#include <span>
#include <stdexcept>

namespace ESM4
{
    // Non-owning index over the final, override-resolved store. Record order
    // breaks ties between independent chains; DATA, not FormID, orders siblings.
    class IdleTree
    {
    public:
        explicit IdleTree(std::span<const IdleAnimation* const> records)
        {
            for (const auto* record : records)
            {
                if (record == nullptr || record->mId.isZeroOrUnset()
                    || !mRecords.emplace(record->mId, record).second)
                    throw std::runtime_error("IDLE hierarchy has a null or duplicate record");
                mChildren[record->mParent.isZeroOrUnset() ? ESM::FormId{} : record->mParent].push_back(record);
            }
            for (const auto* record : records)
            {
                if (!record->mParent.isZeroOrUnset() && !mRecords.contains(record->mParent))
                    throw std::runtime_error("IDLE hierarchy has a missing parent");
                if (!record->mPrevious.isZeroOrUnset())
                {
                    const auto previous = mRecords.find(record->mPrevious);
                    if (previous == mRecords.end()
                        || (previous->second->mParent != record->mParent
                            && !(previous->second->mParent.isZeroOrUnset() && record->mParent.isZeroOrUnset())))
                        throw std::runtime_error("IDLE hierarchy has a missing or foreign predecessor");
                }
                checkChain(record, true);
                checkChain(record, false);
            }
            for (auto& [parent, children] : mChildren)
            {
                const auto unordered = children;
                children.clear();
                std::vector<const IdleAnimation*> pending;
                const auto append = [&](ESM::FormId previous) {
                    for (auto it = unordered.rbegin(); it != unordered.rend(); ++it)
                        if ((*it)->mPrevious == previous
                            || (previous.isZeroOrUnset() && (*it)->mPrevious.isZeroOrUnset()))
                            pending.push_back(*it);
                };
                append({});
                while (!pending.empty())
                {
                    const auto* record = pending.back();
                    pending.pop_back();
                    children.push_back(record);
                    append(record->mId);
                }
                if (children.size() != unordered.size())
                    throw std::runtime_error("IDLE hierarchy contains an unreachable sibling");
            }
        }

        // A matching directory is a branch, not a playable animation. A branch
        // which must return a file falls through when none of its children match.
        // An optional-file match stops selection even when it has no KF.
        template <class Matches, class HasFile>
        const IdleAnimation* select(Matches&& matches, HasFile&& hasFile) const
        {
            struct Candidate
            {
                const IdleAnimation* mRecord;
                bool mChildrenVisited;
            };
            std::vector<Candidate> pending;
            const auto append = [&](ESM::FormId parent) {
                const auto children = mChildren.find(parent);
                if (children != mChildren.end())
                    for (auto it = children->second.rbegin(); it != children->second.rend(); ++it)
                        pending.push_back({ *it, false });
            };
            append({});
            while (!pending.empty())
            {
                const auto [record, childrenVisited] = pending.back();
                pending.pop_back();
                if (childrenVisited)
                {
                    if (hasFile(*record) || (record->mAnimationGroup & 0x80) != 0)
                        return record;
                }
                else if (matches(*record))
                {
                    pending.push_back({ record, true });
                    append(record->mId);
                }
            }
            return nullptr;
        }

    private:
        void checkChain(const IdleAnimation* record, bool parents) const
        {
            std::set<ESM::FormId> seen;
            while (record != nullptr)
            {
                if (!seen.insert(record->mId).second)
                    throw std::runtime_error("IDLE hierarchy contains a cycle");
                const auto next = parents ? record->mParent : record->mPrevious;
                const auto found = mRecords.find(next);
                record = found == mRecords.end() ? nullptr : found->second;
            }
        }

        std::map<ESM::FormId, const IdleAnimation*> mRecords;
        std::map<ESM::FormId, std::vector<const IdleAnimation*>> mChildren;
    };
}

#endif
