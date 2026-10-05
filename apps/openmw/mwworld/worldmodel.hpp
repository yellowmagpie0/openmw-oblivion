#ifndef GAME_MWWORLD_WORLDMODEL_H
#define GAME_MWWORLD_WORLDMODEL_H

#include <list>
#include <memory>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <components/esm/exteriorcelllocation.hpp>
#include <components/misc/algorithm.hpp>

#include "cellstore.hpp"
#include "ptr.hpp"
#include "ptrregistry.hpp"

namespace ESM
{
    class ESMReader;
    class ESMWriter;
    class ReadersCache;
    struct Cell;
}

namespace ESM4
{
    struct Cell;
}

namespace Loading
{
    class Listener;
}

namespace osg { class Quat; }
namespace MWRender { class Objects; }
namespace MWPhysics { class PhysicsSystem; }
namespace NifBullet { struct ActorRagdollDefinition; }
class btTransform;

namespace MWWorld
{
    class ESMStore;

    /// \brief Cell container
    class WorldModel
    {
    public:
        explicit WorldModel(ESMStore& store, ESM::ReadersCache& reader);

        // Batch publication for detached prepared references. Preparation may
        // assign their RefNums, but leaves the live registry and serial alone.
        // The references must outlive the preparation and its commit.
        class PreparedPtrReplacement
        {
            friend class WorldModel;
            WorldModel* mWorld;
            std::weak_ptr<const char> mIdentity;
            std::size_t mRevision;
            ESM::RefNum mLastGenerated;
            PtrRegistry mRegistry;
            std::vector<Ptr> mInserted;

            PreparedPtrReplacement(WorldModel& world, std::span<const Ptr> removed,
                std::span<const Ptr> inserted);

        public:
            PreparedPtrReplacement(const PreparedPtrReplacement&) = delete;
            PreparedPtrReplacement& operator=(const PreparedPtrReplacement&) = delete;
            PreparedPtrReplacement(PreparedPtrReplacement&& other) noexcept;
            PreparedPtrReplacement& operator=(PreparedPtrReplacement&&) = delete;

            // Reject a stale or already committed preparation before mutation.
            // Successful publication performs no allocations or callbacks.
            bool isValid() const noexcept;
            void commit();
        };

        PreparedPtrReplacement preparePtrReplacement(std::span<const Ptr> removed,
            std::span<const Ptr> inserted);

        struct LooseWeaponPublicationHooks
        {
            void* mContext = nullptr;
            bool (*mValidate)(void*) noexcept = nullptr;
            // Called once after the last fallible scene/body work and final
            // validation, immediately before guaranteed cell/registry splice.
            // Must perform only already-validated noexcept resource/serial
            // writes: no allocation, callbacks or registry/cell mutation.
            void (*mPublish)(void*) noexcept = nullptr;
        };

        class PreparedLooseWeaponAdmission
        {
            struct Data;
            std::unique_ptr<Data> mData;
            explicit PreparedLooseWeaponAdmission(std::unique_ptr<Data> data);
            friend class WorldModel;
        public:
            ~PreparedLooseWeaponAdmission();
            PreparedLooseWeaponAdmission(const PreparedLooseWeaponAdmission&) = delete;
            PreparedLooseWeaponAdmission& operator=(const PreparedLooseWeaponAdmission&) = delete;
            bool isValid() const;
            // Scene/physical admission may throw. Failed publication rolls back
            // both before any cell or registry change. No game observers.
            Ptr commit();
            Ptr commit(const LooseWeaponPublicationHooks& hooks);
        };

        // Caller reserves the stable dynamic identity and supplies resolved
        // placement, ownership, extras and authored model/body metadata.
        // Does not debit source inventory or advance a World dynamic serial.
        std::unique_ptr<PreparedLooseWeaponAdmission> prepareLooseWeaponAdmission(
            CellStore& cell, const LiveCellRef<ESM4::Weapon>& reference,
            MWRender::Objects& objects, MWPhysics::PhysicsSystem& physics,
            const std::string& model, const osg::Quat& rotation, unsigned nodeMask,
            const NifBullet::ActorRagdollDefinition& definition, float lengthScale,
            std::span<const btTransform> poses, int collisionGroup, int collisionMask);

        WorldModel(const WorldModel&) = delete;
        WorldModel& operator=(const WorldModel&) = delete;

        void clear();

        CellStore& getExterior(ESM::ExteriorCellLocation location, bool forceLoad = true) const;

        CellStore* findCell(ESM::RefId id, bool forceLoad = true) const;

        CellStore& getCell(ESM::RefId id, bool forceLoad = true) const;

        // Returns a special cell that is never active. Can be used for creating objects
        // without adding them to the scene.
        CellStore& getDraftCell() const;

        CellStore* findInterior(std::string_view name, bool forceLoad = true) const;

        CellStore& getInterior(std::string_view name, bool forceLoad = true) const;

        CellStore* findCell(std::string_view name, bool forceLoad = true) const;

        CellStore& getCell(std::string_view name, bool forceLoad = true) const;

        Ptr getPtrByRefId(const ESM::RefId& name);

        Ptr getPtr(ESM::RefNum refNum) const { return mPtrRegistry.getOrEmpty(refNum); }

        // Also finds inactive/disabled references in already loaded cells.
        // Does not load cells or register references. Deleted references are
        // returned so callers can reject them explicitly rather than fall
        // back to stale authored state. Not a per-actor scheduler lookup.
        Ptr getResidentPtr(ESM::RefNum refNum);

        // Union of the public registry and references in already loaded cells,
        // including disabled/deleted references. Each live reference appears
        // once. Does not load cells or publish registry entries.
        std::vector<Ptr> getResidentPtrs();

        // Membership only: does not create, load or register a cell.
        bool ownsCell(const CellStore& cell) const noexcept
        {
            const auto found = mCells.find(cell.getCell()->getId());
            return found != mCells.end() && &found->second == &cell;
        }


        PtrRegistryView getPtrRegistryView() const { return PtrRegistryView(mPtrRegistry); }

        ESM::RefNum getLastGeneratedRefNum() const { return mPtrRegistry.getLastGenerated(); }

        void setLastGeneratedRefNum(ESM::RefNum v) { mPtrRegistry.setLastGenerated(v); }

        std::size_t getPtrRegistryRevision() const { return mPtrRegistry.getRevision(); }

        void registerPtr(const Ptr& ptr);

        void deregisterLiveCellRef(LiveCellRefBase& ref) noexcept;

        void assignSaveFileRefNum(ESM::CellRef& ref) { mPtrRegistry.assign(ref); }

        template <typename Fn>
        void forEachLoadedCellStore(Fn&& fn)
        {
            for (auto& [_, store] : mCells)
                fn(store);
        }

        /// Get all Ptrs referencing \a name in exterior cells
        /// @note Due to the current implementation of getPtr this only supports one Ptr per cell.
        /// @note name must be lower case
        void getExteriorPtrs(const ESM::RefId& name, std::vector<MWWorld::Ptr>& out);

        std::vector<MWWorld::Ptr> getAll(const ESM::RefId& id);

        size_t countSavedGameRecords() const;

        void write(ESM::ESMWriter& writer, Loading::Listener& progress) const;

        bool readRecord(ESM::ESMReader& reader, uint32_t type);

    private:
        struct GetCellStoreCallback;

        std::shared_ptr<const char> mPreparationIdentity;
        PtrRegistry mPtrRegistry; // defined before mCells because during destruction it should be the last

        MWWorld::ESMStore& mStore;
        ESM::ReadersCache& mReaders;
        mutable std::unordered_map<ESM::RefId, CellStore> mCells;
        mutable std::map<std::string, CellStore*, Misc::StringUtils::CiComp> mInteriors;
        mutable std::map<ESM::ExteriorCellLocation, CellStore*> mExteriors;
        ESM::Cell mDraftCell;
        std::vector<std::pair<ESM::RefId, CellStore*>> mIdCache;
        std::size_t mIdCacheIndex = 0;

        CellStore& getOrInsertCellStore(const ESM::Cell& cell);

        CellStore& insertCellStore(const ESM::Cell& cell);

        Ptr getPtrAndCache(const ESM::RefId& name, CellStore& cellStore);

        void writeCell(ESM::ESMWriter& writer, CellStore& cell) const;
    };
}

#endif
