#ifndef OPENMW_APPS_OPENMW_MWWORLD_PTRREGISTRY_H
#define OPENMW_APPS_OPENMW_MWWORLD_PTRREGISTRY_H

#include "ptr.hpp"

#include "components/esm3/cellref.hpp"

#include <unordered_map>
#include <utility>
#include <map>
#include <stdexcept>
#include <memory>
#include <vector>
#include <limits>
#include <cassert>
#include <algorithm>

namespace MWWorld
{
    class PtrRegistry
    {
        struct RetainedRuntimeData;
        using RefIndex = std::unordered_map<ESM::RefNum, Ptr>;
        struct Reservations
        {
            PtrRegistry* mOwner;
            std::size_t mPending = 0;
        };

    public:
        // Detached index nodes and hash capacity, without assigning any RefNum.
        // Copies reserve independent nodes; cancellation releases only capacity
        // credits. Credits survive clear, which retains the hash buckets.
        class PreparedInsertions
        {
            friend class PtrRegistry;
            std::shared_ptr<Reservations> mReservations;
            std::vector<RefIndex::node_type> mNodes;

            void release() noexcept
            {
                if (mReservations) mReservations->mPending -= mNodes.size();
                mReservations.reset();
                mNodes.clear();
            }

        public:
            PreparedInsertions() = default;
            ~PreparedInsertions() { release(); }
            PreparedInsertions(const PreparedInsertions& other)
            {
                if (!other.mReservations) return;
                if (!other.mReservations->mOwner)
                    throw std::logic_error("Registry insertion reservation outlived its owner");
                *this = other.mReservations->mOwner->prepareInsertions(other.size());
            }
            PreparedInsertions& operator=(const PreparedInsertions& other)
            {
                if (this != &other)
                {
                    PreparedInsertions copy(other);
                    *this = std::move(copy);
                }
                return *this;
            }
            PreparedInsertions(PreparedInsertions&&) noexcept = default;
            PreparedInsertions& operator=(PreparedInsertions&& other) noexcept
            {
                if (this != &other)
                {
                    release();
                    mReservations = std::move(other.mReservations);
                    mNodes = std::move(other.mNodes);
                }
                return *this;
            }
            std::size_t size() const noexcept { return mNodes.size(); }
        };

        PtrRegistry() : mReservations(std::make_shared<Reservations>(Reservations{this})) {}
        PtrRegistry(const PtrRegistry& other)
            : mRevision(other.mRevision), mIndex(other.mIndex), mDynamicNativeIndex(other.mDynamicNativeIndex)
            , mLastGenerated(other.mLastGenerated), mReservations(other.mReservations)
        {
            reserveCapacity(0);
        }
        PtrRegistry(PtrRegistry&& other) noexcept;
        PtrRegistry& operator=(const PtrRegistry&) = delete;
        PtrRegistry& operator=(PtrRegistry&&) = delete;
        ~PtrRegistry();

        // Start a separate reservation domain before preparing an incoming
        // runtime. Detach only after preparation succeeds. Rollback restores
        // the actual outgoing index nodes, serial and revision, not a replay.
        class RetainedRuntime
        {
            friend class PtrRegistry;
            std::unique_ptr<RetainedRuntimeData> mData;
            explicit RetainedRuntime(std::unique_ptr<RetainedRuntimeData> data);

        public:
            ~RetainedRuntime();
            RetainedRuntime(RetainedRuntime&&) noexcept;
            RetainedRuntime& operator=(RetainedRuntime&&) noexcept;
            RetainedRuntime(const RetainedRuntime&) = delete;
            RetainedRuntime& operator=(const RetainedRuntime&) = delete;
            bool detach() noexcept;
            bool restore() noexcept;
            void discard() noexcept;
            bool isDetached() const noexcept;
        };
        RetainedRuntime retainRuntime();

        PreparedInsertions prepareInsertions(std::size_t count)
        {
            if (mReservations->mOwner != this)
                throw std::logic_error("Registry snapshots cannot own insertion reservations");
            reserveCapacity(count);
            PreparedInsertions result;
            result.mNodes.reserve(count);
            RefIndex temporary;
            for (std::size_t i = 0; i < count; ++i)
            {
                temporary.emplace(ESM::RefNum{}, Ptr{});
                result.mNodes.push_back(temporary.extract(temporary.begin()));
            }
            mReservations->mPending += count;
            result.mReservations = mReservations;
            return result;
        }

        std::size_t pendingInsertions() const noexcept { return mReservations->mPending; }
        std::size_t bucketCount() const noexcept { return mIndex.bucket_count(); }
        std::size_t preparedBucketCount() const noexcept { return mSpareIndex.bucket_count(); }
        bool canSwap(const PtrRegistry& other) const noexcept;

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

        void setLastGenerated(ESM::RefNum v) { requireMutableRuntime(); mLastGenerated = v; }

        void swap(PtrRegistry& other) noexcept
        {
            assert(canSwap(other));
            mIndex.swap(other.mIndex);
            mSpareIndex.swap(other.mSpareIndex);
            mDynamicNativeIndex.swap(other.mDynamicNativeIndex);
            std::swap(mRevision, other.mRevision);
            std::swap(mLastGenerated, other.mLastGenerated);
        }

        void clear()
        {
            requireMutableRuntime();
            mIndex.clear();
            mDynamicNativeIndex.clear();
            mLastGenerated = ESM::RefNum{};
            ++mRevision;
        }

        void insert(const Ptr& ptr)
        {
            requireMutableRuntime();
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
            // New identities must leave capacity for every outstanding plan.
            // Re-registering a moved reference only replaces its Ptr: reserving
            // another slot here can allocate at the reservation boundary.
            if (mReservations->mPending && !mIndex.contains(ptr.getCellRef().getRefNum()))
                reserveCapacity(1);
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

        void insertPrepared(const Ptr& ptr, PreparedInsertions& prepared)
        {
            requireMutableRuntime();
            if (!prepared.mReservations || prepared.mReservations != mReservations
                || mReservations->mOwner != this || !hasReservedCapacity())
                throw std::logic_error("Registry insertion reservation is unavailable");
            if (!ptr.getCellRef().getFormKey().isNull())
                throw std::invalid_argument("Prepared legacy registration requires a shared reference");
            auto id = ptr.getCellRef().getRefNum();
            if (!id.isSet())
            {
                id = mLastGenerated;
                if (id.mContentFile >= 0)
                    throw std::invalid_argument("Registry requires a generated reference namespace");
                if (id.mIndex == std::numeric_limits<std::uint32_t>::max()
                    && id.mContentFile == std::numeric_limits<std::int32_t>::min())
                    throw std::overflow_error("Registry reference namespace exhausted");
                if (++id.mIndex == 0) --id.mContentFile;
            }
            const bool present = mIndex.find(id) != mIndex.end();
            if (!present && prepared.mNodes.empty())
                throw std::logic_error("Prepared registry insertion nodes exhausted");
            // No allocation from here: key hashing and Ptr assignment are
            // nonthrowing, and the pending credit already owns bucket capacity.
            installReservedCapacity();
            ptr.getCellRef().getOrAssignRefNum(mLastGenerated);
            if (id.mContentFile < 0 && (id.mContentFile < mLastGenerated.mContentFile
                || (id.mContentFile == mLastGenerated.mContentFile && id.mIndex > mLastGenerated.mIndex)))
                mLastGenerated = id;
            if (present)
                mIndex.find(id)->second = ptr;
            else
            {
                auto node = std::move(prepared.mNodes.back());
                prepared.mNodes.pop_back();
                --mReservations->mPending;
                node.key() = id;
                node.mapped() = ptr;
                mIndex.insert(std::move(node));
            }
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
                requireMutableRuntime();
                CellRef temp(ref);
                temp.getOrAssignRefNum(mLastGenerated);
                ref.mRefNum = temp.getRefNum();
            }
        }

    private:
        std::size_t mRevision = 0;
        RefIndex mIndex;
        RefIndex mSpareIndex; // Empty detached bucket table; preparation never rehashes mIndex.
        std::map<ESM::FormKey, Ptr> mDynamicNativeIndex;
        ESM::RefNum mLastGenerated;
        std::shared_ptr<Reservations> mReservations;
        RetainedRuntimeData* mRetainedRuntime = nullptr;

        void requireMutableRuntime() const;

        static std::size_t capacity(const RefIndex& index) noexcept
        {
            // The default one-bucket sentinel is not an allocated table. The
            // maximum load factor stays one throughout both indexes' lifetime.
            return index.bucket_count() == 1 ? 0 : index.bucket_count();
        }
        bool hasReservedCapacity() const noexcept
        {
            const auto available = std::max(capacity(mIndex), capacity(mSpareIndex));
            return mIndex.size() <= available && mReservations->mPending <= available - mIndex.size();
        }
        void reserveCapacity(std::size_t extra)
        {
            const auto limit = mIndex.max_size();
            if (mReservations->mPending > limit - mIndex.size()
                || extra > limit - mIndex.size() - mReservations->mPending)
                throw std::length_error("Registry reservation capacity overflow");
            const auto required = mIndex.size() + mReservations->mPending + extra;
            if (required > std::max(capacity(mIndex), capacity(mSpareIndex))) mSpareIndex.reserve(required);
        }
        void installReservedCapacity() noexcept
        {
            if (capacity(mIndex) >= mIndex.size() + mReservations->mPending) return;
            assert(hasReservedCapacity() && mSpareIndex.empty());
            while (!mIndex.empty()) mSpareIndex.insert(mIndex.extract(mIndex.begin()));
            mIndex.swap(mSpareIndex);
        }

    };

    struct PtrRegistry::RetainedRuntimeData
    {
        PtrRegistry* mOwner;
        std::unique_ptr<PtrRegistry> mOutgoing;
        std::shared_ptr<Reservations> mIncoming;
        std::size_t mRevision;
        ESM::RefNum mLastGenerated;
        bool mDetached = false;

        explicit RetainedRuntimeData(PtrRegistry& owner)
            : mOwner(&owner), mOutgoing(std::make_unique<PtrRegistry>(owner))
            , mIncoming(std::make_shared<Reservations>(Reservations{&owner}))
            , mRevision(owner.mRevision), mLastGenerated(owner.mLastGenerated)
        {
        }
    };

    inline void PtrRegistry::requireMutableRuntime() const
    {
        if (mRetainedRuntime && !mRetainedRuntime->mDetached)
            throw std::logic_error("Outgoing registry is frozen during runtime preparation");
    }

    inline bool PtrRegistry::canSwap(const PtrRegistry& other) const noexcept
    {
        return (!mRetainedRuntime || mRetainedRuntime->mDetached)
            && (!other.mRetainedRuntime || other.mRetainedRuntime->mDetached)
            && mReservations == other.mReservations && other.hasReservedCapacity();
    }

    inline PtrRegistry::PtrRegistry(PtrRegistry&& other) noexcept
        : mRevision(other.mRevision), mIndex(std::move(other.mIndex))
        , mSpareIndex(std::move(other.mSpareIndex))
        , mDynamicNativeIndex(std::move(other.mDynamicNativeIndex)), mLastGenerated(other.mLastGenerated)
        , mReservations(std::move(other.mReservations))
        , mRetainedRuntime(std::exchange(other.mRetainedRuntime, nullptr))
    {
        if (mReservations->mOwner == &other) mReservations->mOwner = this;
        if (mRetainedRuntime) mRetainedRuntime->mOwner = this;
    }

    inline PtrRegistry::~PtrRegistry()
    {
        if (mRetainedRuntime)
        {
            mRetainedRuntime->mOwner = nullptr;
            mRetainedRuntime->mOutgoing->mReservations->mOwner = nullptr;
            mRetainedRuntime->mOutgoing->clear();
        }
        if (mReservations && mReservations->mOwner == this) mReservations->mOwner = nullptr;
    }

    inline PtrRegistry::RetainedRuntime::RetainedRuntime(std::unique_ptr<RetainedRuntimeData> data)
        : mData(std::move(data))
    {
    }
    inline PtrRegistry::RetainedRuntime::~RetainedRuntime() { restore(); }
    inline PtrRegistry::RetainedRuntime::RetainedRuntime(RetainedRuntime&&) noexcept = default;
    inline PtrRegistry::RetainedRuntime& PtrRegistry::RetainedRuntime::operator=(RetainedRuntime&& other) noexcept
    {
        if (this != &other) { restore(); mData = std::move(other.mData); }
        return *this;
    }

    inline PtrRegistry::RetainedRuntime PtrRegistry::retainRuntime()
    {
        if (mRetainedRuntime || mReservations->mOwner != this)
            throw std::logic_error("Registry runtime retention requires an exclusive live owner");
        auto data = std::make_unique<RetainedRuntimeData>(*this);
        // All allocation precedes the domain switch. Old reservation copies
        // continue to reserve against the retained registry, not the incoming one.
        data->mOutgoing->mReservations->mOwner = data->mOutgoing.get();
        mReservations = data->mIncoming;
        mRetainedRuntime = data.get();
        return RetainedRuntime(std::move(data));
    }

    inline bool PtrRegistry::RetainedRuntime::isDetached() const noexcept
    {
        return mData && mData->mOwner && mData->mDetached;
    }

    inline bool PtrRegistry::RetainedRuntime::detach() noexcept
    {
        if (!mData || !mData->mOwner || mData->mDetached) return false;
        auto& owner = *mData->mOwner;
        if (owner.mRetainedRuntime != mData.get() || owner.mRevision != mData->mRevision
            || owner.mLastGenerated != mData->mLastGenerated || owner.mReservations != mData->mIncoming)
            return false;
        auto& outgoing = *mData->mOutgoing;
        outgoing.mIndex.clear(); // Drop preparation copies; retain original nodes below.
        outgoing.mDynamicNativeIndex.clear();
        outgoing.mIndex.swap(owner.mIndex);
        outgoing.mDynamicNativeIndex.swap(owner.mDynamicNativeIndex);
        owner.mLastGenerated = {};
        ++owner.mRevision;
        mData->mDetached = true;
        return true;
    }

    inline bool PtrRegistry::RetainedRuntime::restore() noexcept
    {
        if (!mData || !mData->mOwner) return false;
        auto& owner = *mData->mOwner;
        if (owner.mRetainedRuntime != mData.get()) return false;
        auto& outgoing = *mData->mOutgoing;
        if (mData->mDetached)
        {
            owner.mIndex.swap(outgoing.mIndex);
            owner.mDynamicNativeIndex.swap(outgoing.mDynamicNativeIndex);
            owner.mSpareIndex.swap(outgoing.mSpareIndex);
            std::swap(owner.mRevision, outgoing.mRevision);
            std::swap(owner.mLastGenerated, outgoing.mLastGenerated);
        }
        else
        {
            // A copied hash table may have a different bucket count. Transfer
            // its largest empty table without replacing outgoing index nodes.
            outgoing.mIndex.clear();
            if (capacity(outgoing.mIndex) > capacity(outgoing.mSpareIndex))
                outgoing.mIndex.swap(outgoing.mSpareIndex);
            if (capacity(outgoing.mSpareIndex) > capacity(owner.mSpareIndex))
                owner.mSpareIndex.swap(outgoing.mSpareIndex);
        }
        // Retire every incoming reservation, including externally held plans.
        // Their cancellation remains safe, but they cannot publish into the old game.
        owner.mReservations->mOwner = nullptr;
        owner.mReservations = outgoing.mReservations;
        owner.mReservations->mOwner = &owner;
        owner.mRetainedRuntime = nullptr;
        mData->mOwner = nullptr;
        mData->mOutgoing.reset();
        return true;
    }

    inline void PtrRegistry::RetainedRuntime::discard() noexcept
    {
        if (!mData || !mData->mOwner) return;
        if (!mData->mDetached) { restore(); return; }
        auto& owner = *mData->mOwner;
        owner.mRetainedRuntime = nullptr;
        mData->mOutgoing->mReservations->mOwner = nullptr;
        mData->mOwner = nullptr;
        mData->mOutgoing.reset();
    }

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
