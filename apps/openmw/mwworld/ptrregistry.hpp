#ifndef OPENMW_APPS_OPENMW_MWWORLD_PTRREGISTRY_H
#define OPENMW_APPS_OPENMW_MWWORLD_PTRREGISTRY_H

#include "ptr.hpp"

#include "components/esm3/cellref.hpp"

#include <unordered_map>
#include <utility>
#include <map>
#include <stdexcept>

namespace MWWorld
{
    class PtrRegistry
    {
    public:
        std::size_t getRevision() const { return mRevision; }

        ESM::RefNum getLastGenerated() const { return mLastGenerated; }

        auto begin() const { return mIndex.cbegin(); }

        auto end() const { return mIndex.cend(); }

        Ptr getOrEmpty(ESM::RefNum refNum) const
        {
            const auto it = mIndex.find(refNum);
            if (it != mIndex.end())
                return it->second;
            return Ptr();
        }

        Ptr getDynamicNativePtr(const ESM::FormKey& key) const
        {
            const auto found = mDynamicNativeIndex.find(key);
            return found == mDynamicNativeIndex.end() ? Ptr() : found->second;
        }

        void setLastGenerated(ESM::RefNum v) { mLastGenerated = v; }

        void swap(PtrRegistry& other) noexcept
        {
            mIndex.swap(other.mIndex);
            mDynamicNativeIndex.swap(other.mDynamicNativeIndex);
            std::swap(mRevision, other.mRevision);
            std::swap(mLastGenerated, other.mLastGenerated);
        }

        void clear()
        {
            mIndex.clear();
            mDynamicNativeIndex.clear();
            mLastGenerated = ESM::RefNum{};
            ++mRevision;
        }

        void insert(const Ptr& ptr)
        {
            const auto key = ptr.getCellRef().getFormKey();
            auto native = mDynamicNativeIndex.end();
            decltype(mDynamicNativeIndex)::node_type preparedNative;
            if (key.isDynamic())
            {
                native = mDynamicNativeIndex.find(key);
                if (native != mDynamicNativeIndex.end() && native->second.mRef != ptr.mRef)
                    throw std::invalid_argument("duplicate dynamic native reference identity");
                if (native == mDynamicNativeIndex.end())
                {
                    decltype(mDynamicNativeIndex) prepared;
                    prepared.emplace(key, ptr);
                    preparedNative = prepared.extract(prepared.begin());
                }
            }
            const auto id = ptr.getCellRef().getOrAssignRefNum(mLastGenerated);
            // Restored references can already have a generated identity. Keep
            // subsequent allocation beyond it, including a retained Player
            // reference registered after a World clear.
            if (id.mContentFile < 0 && (id.mContentFile < mLastGenerated.mContentFile
                || (id.mContentFile == mLastGenerated.mContentFile && id.mIndex > mLastGenerated.mIndex)))
                mLastGenerated = id;
            mIndex[id] = ptr;
            // The native node was allocated before the RefNum index changed.
            // Publication below only transfers a node or assigns a Ptr.
            if (!preparedNative.empty())
                mDynamicNativeIndex.insert(std::move(preparedNative));
            else if (native != mDynamicNativeIndex.end())
                native->second = ptr;
            ++mRevision;
        }

        void remove(const LiveCellRefBase& ref) noexcept
        {
            // Removal is noexcept. Avoid copying the FormKey's string here;
            // the small dynamic index can be searched by borrowed identity.
            for (auto native = mDynamicNativeIndex.begin(); native != mDynamicNativeIndex.end(); ++native)
                if (native->second.mRef == &ref)
                {
                    mDynamicNativeIndex.erase(native);
                    break;
                }
            ESM::RefNum refNum = ref.mRef.getRefNum();
            if (!refNum.isSet())
                return;
            auto it = mIndex.find(refNum);
            if (it != mIndex.end() && it->second.mRef == &ref)
            {
                mIndex.erase(it);
                ++mRevision;
            }
        }

        // For fixing old saves
        void assign(ESM::CellRef& ref)
        {
            if (!ref.mRefNum.isSet())
            {
                CellRef temp(ref);
                temp.getOrAssignRefNum(mLastGenerated);
                ref.mRefNum = temp.getRefNum();
            }
        }

    private:
        std::size_t mRevision = 0;
        std::unordered_map<ESM::RefNum, Ptr> mIndex;
        std::map<ESM::FormKey, Ptr> mDynamicNativeIndex;
        ESM::RefNum mLastGenerated;
    };

    class PtrRegistryView
    {
    public:
        explicit PtrRegistryView(const PtrRegistry& ref)
            : mPtr(&ref)
        {
        }

        auto begin() const { return mPtr->begin(); }

        auto end() const { return mPtr->end(); }

    private:
        const PtrRegistry* mPtr;
    };
}

#endif
