#include "apps/openmw/mwclass/npc.hpp"
#include "apps/openmw/mwclass/weapon.hpp"
#include "apps/openmw/mwworld/manualref.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <components/esm3/objectstate.hpp>
#include <limits>
#include "apps/openmw/mwworld/esmstore.hpp"
#include "apps/openmw/mwworld/cellref.hpp"
#include "apps/openmw/mwworld/livecellref.hpp"
#include "apps/openmw/mwworld/ptr.hpp"
#include "apps/openmw/mwworld/worldmodel.hpp"

#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadweap.hpp>
#include <components/esm3/readerscache.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace MWWorld
{
    namespace
    {
        using namespace testing;

        TEST(MWWorldPtrTest, preparesPointerReplacementWithoutPublishingOrConsumingLiveSerials)
        {
            MWClass::Weapon::registerSelf();
            MWWorld::ESMStore store;
            ESM::Weapon base;
            base.blank();
            base.mId = ESM::RefId::stringRefId("prepared_weapon");
            store.insertStatic(base);
            ESM::ReadersCache readers;
            MWWorld::WorldModel model(store, readers);
            MWWorld::ManualRef old(store, base.mId), unrelated(store, base.mId);
            model.registerPtr(old.getPtr());
            model.registerPtr(unrelated.getPtr());
            const auto oldId = old.getPtr().getCellRef().getRefNum();
            const auto unrelatedId = unrelated.getPtr().getCellRef().getRefNum();
            const auto revision = model.getPtrRegistryRevision();
            const auto serial = model.getLastGeneratedRefNum();
            const std::array removed{old.getPtr()};
            const auto checkUnchanged = [&]() {
                EXPECT_EQ(model.getPtr(oldId), old.getPtr());
                EXPECT_EQ(model.getPtr(unrelatedId), unrelated.getPtr());
                EXPECT_EQ(model.getPtrRegistryRevision(), revision);
                EXPECT_EQ(model.getLastGeneratedRefNum(), serial);
            };
            {
                MWWorld::ManualRef abandoned(store, base.mId);
                const std::array inserted{abandoned.getPtr()};
                auto discarded = model.preparePtrReplacement(removed, inserted);
                checkUnchanged();
                EXPECT_EQ(abandoned.getPtr().mRef->mWorldModel, nullptr);
            }
            checkUnchanged();
            MWWorld::ManualRef first(store, base.mId), second(store, base.mId);
            const std::array inserted{first.getPtr(), second.getPtr()};
            auto prepared = model.preparePtrReplacement(removed, inserted);
            checkUnchanged();
            const auto firstId = first.getPtr().getCellRef().getRefNum();
            const auto secondId = second.getPtr().getCellRef().getRefNum();
            EXPECT_TRUE(firstId.isSet());
            EXPECT_TRUE(secondId.isSet());
            EXPECT_NE(firstId, secondId);
            EXPECT_EQ(first.getPtr().mRef->mWorldModel, nullptr);
            auto moved = std::move(prepared);
            EXPECT_THROW(prepared.commit(), std::logic_error);
            moved.commit();
            EXPECT_TRUE(model.getPtr(oldId).isEmpty());
            EXPECT_EQ(model.getPtr(unrelatedId), unrelated.getPtr());
            EXPECT_EQ(model.getPtr(firstId), first.getPtr());
            EXPECT_EQ(model.getPtr(secondId), second.getPtr());
            EXPECT_EQ(first.getPtr().mRef->mWorldModel, &model);
            EXPECT_EQ(second.getPtr().mRef->mWorldModel, &model);
            EXPECT_EQ(model.getLastGeneratedRefNum(), secondId);
            const auto committedRevision = model.getPtrRegistryRevision();
            EXPECT_THROW(moved.commit(), std::logic_error);
            EXPECT_EQ(model.getPtrRegistryRevision(), committedRevision);
        }

        TEST(MWWorldPtrTest, pointerReplacementPreservesAliasesAndAdvancesPastDetachedReservations)
        {
            MWClass::Weapon::registerSelf();
            ESMStore store;
            ESM::Weapon base;
            base.blank();
            base.mId = ESM::RefId::stringRefId("prepared_weapon");
            store.insertStatic(base);
            ESM::ReadersCache readers;
            WorldModel model(store, readers);
            auto old = std::make_unique<ManualRef>(store, base.mId);
            model.registerPtr(old->getPtr());
            const auto oldId = old->getPtr().getCellRef().getRefNum();
            ManualRef replacement(store, base.mId);
            replacement.getPtr().getCellRef().setRefNum(oldId);
            const std::array removed{old->getPtr()}, inserted{replacement.getPtr()};
            auto prepared = model.preparePtrReplacement(removed, inserted);
            prepared.commit();
            const auto revision = model.getPtrRegistryRevision();
            old.reset();
            EXPECT_EQ(model.getPtr(oldId), replacement.getPtr());
            EXPECT_EQ(model.getPtrRegistryRevision(), revision);
            EXPECT_EQ(model.getLastGeneratedRefNum(), oldId);

            ManualRef retry(store, base.mId);
            const std::array retried{retry.getPtr()};
            {
                auto abandoned = model.preparePtrReplacement({}, retried);
                EXPECT_TRUE(retry.getPtr().getCellRef().getRefNum().isSet());
                EXPECT_EQ(model.getLastGeneratedRefNum(), oldId);
            }
            const auto reservedId = retry.getPtr().getCellRef().getRefNum();
            auto committedRetry = model.preparePtrReplacement({}, retried);
            committedRetry.commit();
            EXPECT_EQ(model.getLastGeneratedRefNum(), reservedId);
            ManualRef later(store, base.mId);
            model.registerPtr(later.getPtr());
            EXPECT_NE(later.getPtr().getCellRef().getRefNum(), reservedId);
            EXPECT_EQ(model.getPtr(reservedId), retry.getPtr());
            EXPECT_EQ(model.getPtr(oldId), replacement.getPtr());
        }

        TEST(MWWorldPtrTest, pointerReplacementRejectsInvalidReferencesAndExhaustedNamespace)
        {
            MWClass::Weapon::registerSelf();
            ESMStore store;
            ESM::Weapon base;
            base.blank();
            base.mId = ESM::RefId::stringRefId("prepared_weapon");
            store.insertStatic(base);
            for (int fault = 0; fault < 6; ++fault)
            {
                SCOPED_TRACE(fault);
                ESM::ReadersCache readers;
                WorldModel model(store, readers);
                ManualRef old(store, base.mId), fresh(store, base.mId), other(store, base.mId);
                model.registerPtr(old.getPtr());
                const auto oldId = old.getPtr().getCellRef().getRefNum();
                std::vector<Ptr> removed{old.getPtr()}, inserted{fresh.getPtr()};
                if (fault == 0)
                    inserted.push_back({});
                else if (fault == 1)
                    inserted.push_back(old.getPtr());
                else if (fault == 2)
                    removed.push_back(other.getPtr());
                else if (fault == 3)
                    inserted.push_back(fresh.getPtr());
                else if (fault == 4)
                {
                    removed.clear();
                    fresh.getPtr().getCellRef().setRefNum(oldId);
                }
                else
                    model.setLastGeneratedRefNum({std::numeric_limits<std::uint32_t>::max(),
                        std::numeric_limits<std::int32_t>::min()});
                const auto revision = model.getPtrRegistryRevision();
                const auto serial = model.getLastGeneratedRefNum();
                if (fault == 5)
                {
                    EXPECT_THROW(model.preparePtrReplacement(removed, inserted), std::overflow_error);
                }
                else
                {
                    EXPECT_THROW(model.preparePtrReplacement(removed, inserted), std::invalid_argument);
                }
                EXPECT_EQ(model.getPtr(oldId), old.getPtr());
                EXPECT_EQ(model.getPtrRegistryRevision(), revision);
                EXPECT_EQ(model.getLastGeneratedRefNum(), serial);
                EXPECT_EQ(fresh.getPtr().mRef->mWorldModel, nullptr);
            }
        }

        TEST(MWWorldPtrTest, pointerReplacementRejectsStaleRegistrySerialOrPreparedIdentity)
        {
            MWClass::Weapon::registerSelf();
            ESMStore store;
            ESM::Weapon base;
            base.blank();
            base.mId = ESM::RefId::stringRefId("prepared_weapon");
            store.insertStatic(base);
            for (int change = 0; change < 3; ++change)
            {
                SCOPED_TRACE(change);
                ESM::ReadersCache readers;
                WorldModel model(store, readers);
                ManualRef old(store, base.mId), fresh(store, base.mId), later(store, base.mId);
                model.registerPtr(old.getPtr());
                const auto oldId = old.getPtr().getCellRef().getRefNum();
                const std::array removed{old.getPtr()}, inserted{fresh.getPtr()};
                auto prepared = model.preparePtrReplacement(removed, inserted);
                if (change == 0)
                    model.registerPtr(later.getPtr());
                else if (change == 1)
                    model.setLastGeneratedRefNum({100, -1});
                else
                    fresh.getPtr().getCellRef().setRefNum({100, -1});
                const auto revision = model.getPtrRegistryRevision();
                const auto serial = model.getLastGeneratedRefNum();
                EXPECT_THROW(prepared.commit(), std::logic_error);
                EXPECT_EQ(model.getPtr(oldId), old.getPtr());
                EXPECT_EQ(model.getPtrRegistryRevision(), revision);
                EXPECT_EQ(model.getLastGeneratedRefNum(), serial);
                EXPECT_EQ(fresh.getPtr().mRef->mWorldModel, nullptr);
                if (change == 0)
                {
                    EXPECT_EQ(model.getPtr(later.getPtr().getCellRef().getRefNum()), later.getPtr());
                }
            }
        }

        TEST(MWWorldPtrTest, cellRefPreservesStableTes4InstanceIdentity)
        {
            ESM4::Reference reference;
            reference.mFormKey = ESM::FormKey::content("Oblivion.esm", 0x123);
            EXPECT_EQ(CellRef(reference).getFormKey(), reference.mFormKey);

            ESM4::ActorCharacter actor;
            actor.mFormKey = ESM::FormKey::content("Knights.esp", 0x456);
            EXPECT_EQ(CellRef(actor).getFormKey(), actor.mFormKey);

            ESM::CellRef legacy;
            legacy.blank();
            EXPECT_TRUE(CellRef(legacy).getFormKey().isNull());
        }

        TEST(MWWorldPtrTest, cellRefPreservesNativeRankPresenceThroughCompatibilitySetter)
        {
            ESM4::Reference native{};
            CellRef ref(native);
            EXPECT_EQ(ref.getFactionRank(), -1);
            EXPECT_FALSE(ref.getNativeOwnershipRank());
            ref.setFactionRank(-1);
            EXPECT_EQ(ref.getNativeOwnershipRank(), -1);
            for (int rank : {std::numeric_limits<int>::min(), -2, 0, 7,
                             std::numeric_limits<int>::max()})
            {
                ref.setFactionRank(rank);
                EXPECT_EQ(ref.getFactionRank(), rank);
                EXPECT_EQ(ref.getNativeOwnershipRank(), rank);
            }
            EXPECT_FALSE(native.mFactionRank);
            ESM::CellRef legacy;
            legacy.blank();
            CellRef tes3(legacy);
            EXPECT_FALSE(tes3.getNativeOwnershipRank());
            tes3.setFactionRank(-1);
            EXPECT_EQ(tes3.getFactionRank(), -1);
            EXPECT_FALSE(tes3.getNativeOwnershipRank());
        }

        TEST(MWWorldPtrTest, cellRefNativeOwnershipExtrasStayTypedAndSeparateFromLegacy)
        {
            ESM::CellRef projected;
            projected.blank();
            projected.mFactionRank = 9;
            projected.mGlobalVariable = "legacy_global";
            CellRef item(projected);
            EXPECT_FALSE(item.getNativeOwnershipRank());
            item.setNativeOwnershipRank(-1);
            EXPECT_EQ(item.getNativeOwnershipRank(), -1);
            EXPECT_EQ(item.getFactionRank(), 9);
            const ESM::RefId global(ESM::FormId{42, 1});
            item.setNativeOwnershipGlobal(global);
            EXPECT_EQ(item.getNativeOwnershipGlobal(), global);
            EXPECT_EQ(item.getGlobalVariable(), "legacy_global");
            item.setNativeOwnershipRank(std::nullopt);
            EXPECT_FALSE(item.getNativeOwnershipRank());
            EXPECT_EQ(item.getFactionRank(), 9);
            item.resetGlobalVariable();
            EXPECT_TRUE(item.getNativeOwnershipGlobal().empty());
            EXPECT_TRUE(item.getGlobalVariable().empty());
            EXPECT_EQ(projected.mFactionRank, 9);
            EXPECT_TRUE(projected.mNativeOwnershipGlobal.empty());

            ESM4::Reference placed{};
            placed.mFactionRank = -2;
            placed.mGlobal = {42, 1};
            CellRef native(placed);
            EXPECT_EQ(native.getNativeOwnershipRank(), -2);
            EXPECT_EQ(native.getNativeOwnershipGlobal(), global);
            native.setNativeOwnershipRank(std::nullopt);
            native.resetGlobalVariable();
            EXPECT_FALSE(native.getNativeOwnershipRank());
            EXPECT_TRUE(native.getNativeOwnershipGlobal().empty());
            EXPECT_EQ(placed.mFactionRank, -2);
            EXPECT_EQ(placed.mGlobal, (ESM::FormId{42, 1}));
        }

        TEST(MWWorldPtrTest, cellRefNativeOwnershipRejectsWrongGlobalAndUnsupportedActorRankBeforeMutation)
        {
            ESM4::Reference placed{};
            CellRef ref(placed);
            EXPECT_FALSE(ref.hasChanged());
            EXPECT_THROW(ref.setNativeOwnershipGlobal(ESM::RefId::stringRefId("legacy_name")), std::invalid_argument);
            EXPECT_THROW(ref.setNativeOwnershipGlobal(ESM::RefId::generated(1)), std::invalid_argument);
            EXPECT_FALSE(ref.hasChanged());
            EXPECT_TRUE(ref.getNativeOwnershipGlobal().empty());
            ESM4::ActorCharacter source{};
            CellRef actor(source);
            EXPECT_THROW(actor.setNativeOwnershipRank(0), std::invalid_argument);
            EXPECT_FALSE(actor.hasChanged());
            const ESM::RefId global(ESM::FormId{43, 0});
            actor.setNativeOwnershipGlobal(global);
            EXPECT_EQ(actor.getNativeOwnershipGlobal(), global);
            actor.resetGlobalVariable();
            EXPECT_TRUE(actor.getNativeOwnershipGlobal().empty());
        }

        TEST(MWWorldPtrTest, cellRefExposesSavedNpcAndCreatureRagdollPoses)
        {
            ESM4::ActorCharacter actor;
            actor.mRagdoll.mBones.emplace_back();
            ASSERT_NE(CellRef(actor).getRagdollPose(), nullptr);

            ESM4::ActorCreature creature;
            creature.mRagdoll.mBones.emplace_back();
            ASSERT_NE(CellRef(creature).getRagdollPose(), nullptr);

            ESM4::Reference reference;
            EXPECT_EQ(CellRef(reference).getRagdollPose(), nullptr);
        }

        TEST(MWWorldPtrTest, toStringShouldReturnHumanReadableTextRepresentationOfPtrWithNullRef)
        {
            Ptr ptr;
            EXPECT_EQ(ptr.toString(), "null object");
        }

        TEST(MWWorldPtrTest, toStringShouldReturnHumanReadableTextRepresentationOfPtrWithDeletedRef)
        {
            MWClass::Npc::registerSelf();
            ESM::NPC npc;
            npc.blank();
            npc.mId = ESM::RefId::stringRefId("Player");
            ESMStore store;
            store.insert(npc);
            ESM::CellRef cellRef;
            cellRef.blank();
            cellRef.mRefID = npc.mId;
            cellRef.mRefNum = ESM::RefNum{ .mIndex = 0x2a, .mContentFile = 0xd };
            LiveCellRef<ESM::NPC> liveCellRef(cellRef, &npc);
            liveCellRef.mData.setDeletedByContentFile(true);
            Ptr ptr(&liveCellRef);
            EXPECT_THAT(ptr.toString(), StrCaseEq("deleted object0xd00002a (NPC, \"player\")"));
        }

        TEST(MWWorldPtrTest, toStringShouldReturnHumanReadableTextRepresentationOfPtr)
        {
            MWClass::Npc::registerSelf();
            ESM::NPC npc;
            npc.blank();
            npc.mId = ESM::RefId::stringRefId("Player");
            ESMStore store;
            store.insert(npc);
            ESM::CellRef cellRef;
            cellRef.blank();
            cellRef.mRefID = npc.mId;
            cellRef.mRefNum = ESM::RefNum{ .mIndex = 0x2a, .mContentFile = 0xd };
            LiveCellRef<ESM::NPC> liveCellRef(cellRef, &npc);
            Ptr ptr(&liveCellRef);
            EXPECT_THAT(ptr.toString(), StrCaseEq("object0xd00002a (NPC, \"player\")"));
        }

        TEST(MWWorldPtrTest, underlyingLiveCellRefShouldBeDeregisteredOnDestruction)
        {
            MWClass::Npc::registerSelf();
            ESM::NPC npc;
            npc.blank();
            npc.mId = ESM::RefId::stringRefId("Player");
            ESMStore store;
            store.insert(npc);
            ESM::ReadersCache readersCache;
            WorldModel worldModel(store, readersCache);
            ESM::CellRef cellRef;
            cellRef.blank();
            cellRef.mRefID = npc.mId;
            cellRef.mRefNum = ESM::FormId{ .mIndex = 0x2a, .mContentFile = 0xd };
            {
                LiveCellRef<ESM::NPC> liveCellRef(cellRef, &npc);
                Ptr ptr(&liveCellRef);
                worldModel.registerPtr(ptr);
                ASSERT_EQ(worldModel.getPtr(cellRef.mRefNum), ptr);
            }
            EXPECT_EQ(worldModel.getPtr(cellRef.mRefNum), Ptr());
        }

        TEST(MWWorldPtrTest, residentLookupFindsUnregisteredDisabledReferencesWithoutLoadingCells)
        {
            MWClass::Npc::registerSelf();
            ESM::NPC npc;
            npc.blank();
            npc.mId = ESM::RefId::stringRefId("resident-test");
            ESMStore store;
            store.insert(npc);
            ESM::ReadersCache readers;
            WorldModel world(store, readers);
            CellStore& cell = world.getDraftCell();
            ESM::Cell unloaded;
            unloaded.blank();
            unloaded.mName = "resident-lookup-unloaded";
            unloaded.mData.mFlags = ESM::Cell::Interior;
            unloaded.updateId();
            store.insert(unloaded);
            CellStore& other = world.getCell(unloaded.mId, false);
            EXPECT_EQ(other.getState(), CellStore::State_Unloaded);
            ESM::CellRef reference;
            reference.blank();
            reference.mRefID = npc.mId;
            reference.mRefNum = ESM::FormId{ 0x42, 0 };
            LiveCellRef<ESM::NPC> live(reference, &npc);
            live.mData.disable();
            Ptr ptr(cell.insert(&live), &cell);
            EXPECT_TRUE(world.getPtr(reference.mRefNum).isEmpty());
            EXPECT_EQ(world.getResidentPtr(reference.mRefNum), ptr);
            EXPECT_THAT(world.getResidentPtrs(), ElementsAre(ptr));
            EXPECT_FALSE(ptr.getRefData().isEnabled());
            EXPECT_TRUE(world.getPtr(reference.mRefNum).isEmpty());
            ptr.getRefData().setDeletedByContentFile(true);
            EXPECT_EQ(world.getResidentPtr(reference.mRefNum), ptr);
            EXPECT_TRUE(ptr.mRef->isDeleted());
            EXPECT_THAT(world.getResidentPtrs(), ElementsAre(ptr));
            world.registerPtr(ptr);
            ESM::CellRef outsideReference = reference;
            outsideReference.mRefNum = ESM::FormId{ 0x44, 0 };
            LiveCellRef<ESM::NPC> outsideLive(outsideReference, &npc);
            Ptr outside(&outsideLive);
            world.registerPtr(outside);
            const auto revision = world.getPtrRegistryRevision();
            const auto serial = world.getLastGeneratedRefNum();
            EXPECT_THAT(world.getResidentPtrs(), UnorderedElementsAre(ptr, outside));
            EXPECT_EQ(world.getPtrRegistryRevision(), revision);
            EXPECT_EQ(world.getLastGeneratedRefNum(), serial);
            EXPECT_TRUE(world.getResidentPtr({}).isEmpty());
            EXPECT_TRUE(world.getResidentPtr(ESM::FormId{ 0x43, 0 }).isEmpty());
            EXPECT_EQ(other.getState(), CellStore::State_Unloaded);
        }
    }
}

TEST(MWWorldPtrTest, NativeConditionSurvivesCopyObjectStateAndLegacyDisplayWithoutChargeAliasing)
{
    MWClass::Weapon::registerSelf();
    MWWorld::ESMStore store;
    ESM::Weapon base;
    base.blank();
    base.mId = ESM::RefId::stringRefId("native_condition_weapon");
    base.mData.mHealth = 100;
    store.insertStatic(base);
    MWWorld::ManualRef source(store, base.mId);
    auto ptr = source.getPtr();
    ptr.getCellRef().setCharge(73);
    ptr.getCellRef().setEnchantmentCharge(17.5f);
    for (const std::uint32_t bits : {0u, 0x80000000u, 1u, 0x33800000u, 0x42c7ffffu, 0x43000000u})
    {
        const float value = std::bit_cast<float>(bits);
        ptr.getCellRef().setNativeItemCondition(value);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(ptr.getCellRef().getItemCondition(100)), bits);
        EXPECT_EQ(ptr.getCellRef().getEnchantmentCharge(), 17.5f);
        EXPECT_EQ(ptr.getClass().getItemHealth(ptr), static_cast<int>(std::ceil(value)));
        EXPECT_EQ(ptr.getClass().getItemNormalizedHealth(ptr), value / 100.f);
        ESM::ObjectState state;
        ptr.getCellRef().writeState(state);
        MWWorld::CellRef restored(state.mRef);
        auto copied = restored;
        ASSERT_TRUE(copied.getNativeItemCondition());
        EXPECT_EQ(std::bit_cast<std::uint32_t>(*copied.getNativeItemCondition()), bits);
        for (float bad : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            EXPECT_THROW(ptr.getCellRef().setNativeItemCondition(bad), std::invalid_argument);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(*ptr.getCellRef().getNativeItemCondition()), bits);
        }
    }
    ptr.getCellRef().setCharge(51);
    EXPECT_FALSE(ptr.getCellRef().getNativeItemCondition());
    EXPECT_EQ(ptr.getClass().getItemHealth(ptr), 51);
}


TEST(MWWorldPtrTest, preparedRegistrationsOwnNodesAcrossClearCopiesMovesAndOrdinaryGrowth)
{
    MWClass::Weapon::registerSelf();
    MWWorld::ESMStore store; ESM::Weapon base{}; base.blank();
    base.mId = ESM::RefId::stringRefId("reserved-registry-weapon"); store.insertStatic(base);
    MWWorld::PtrRegistry registry;
    const auto revision = registry.getRevision(); const auto serial = registry.getLastGenerated();
    auto pool = registry.prepareInsertions(64);
    EXPECT_EQ(registry.pendingInsertions(), 64u);
    EXPECT_EQ(registry.getRevision(), revision); EXPECT_EQ(registry.getLastGenerated(), serial);
    {
        auto copy = pool;
        EXPECT_EQ(registry.pendingInsertions(), 128u);
        auto moved = std::move(copy);
        EXPECT_EQ(moved.size(), 64u); EXPECT_EQ(copy.size(), 0u);
        auto& self = moved; moved = std::move(self);
        EXPECT_EQ(moved.size(), 64u);
        MWWorld::PtrRegistry::PreparedInsertions assigned; assigned = moved;
        EXPECT_EQ(registry.pendingInsertions(), 192u);
    }
    EXPECT_EQ(registry.pendingInsertions(), 64u);
    registry.clear();
    std::vector<std::unique_ptr<MWWorld::ManualRef>> refs;
    for (unsigned i = 0; i < 300; ++i)
    {
        refs.push_back(std::make_unique<MWWorld::ManualRef>(store, base.mId));
        registry.insert(refs.back()->getPtr());
    }
    const auto buckets = registry.bucketCount();
    const auto before = registry.getLastGenerated();
    for (unsigned i = 0; i < 64; ++i)
    {
        refs.push_back(std::make_unique<MWWorld::ManualRef>(store, base.mId));
        auto ptr = refs.back()->getPtr();
        registry.insertPrepared(ptr, pool);
        EXPECT_EQ(ptr.getCellRef().getRefNum(), (ESM::RefNum{before.mIndex + i + 1, before.mContentFile}));
        EXPECT_EQ(registry.getOrEmpty(ptr.getCellRef().getRefNum()), ptr);
        EXPECT_EQ(pool.size(), 63u - i); EXPECT_EQ(registry.pendingInsertions(), 63u - i);
        EXPECT_EQ(registry.bucketCount(), buckets);
        registry.insertPrepared(ptr, pool); // Repeat registration consumes no node or serial.
        EXPECT_EQ(pool.size(), 63u - i); EXPECT_EQ(registry.bucketCount(), buckets);
    }
    MWWorld::ManualRef exhausted(store, base.mId);
    const auto finalSerial = registry.getLastGenerated(); const auto finalRevision = registry.getRevision();
    EXPECT_THROW(registry.insertPrepared(exhausted.getPtr(), pool), std::logic_error);
    EXPECT_FALSE(exhausted.getPtr().getCellRef().getRefNum().isSet());
    EXPECT_EQ(registry.getLastGenerated(), finalSerial); EXPECT_EQ(registry.getRevision(), finalRevision);
}

TEST(MWWorldPtrTest, ordinaryRegistrationUpdatesPreserveFullyReservedCapacity)
{
    MWClass::Weapon::registerSelf();
    MWWorld::ESMStore store;
    ESM::Weapon base{};
    base.blank();
    base.mId = ESM::RefId::stringRefId("reserved-registry-update");
    store.insertStatic(base);
    for (bool detached : {false, true})
    {
        SCOPED_TRACE(detached);
        MWWorld::PtrRegistry registry;
        MWWorld::ManualRef original(store, base.mId), relocated(store, base.mId);
        registry.insert(original.getPtr());
        const auto id = original.getPtr().getCellRef().getRefNum();
        relocated.getPtr().getCellRef().setRefNum(id);
        auto pool = registry.prepareInsertions(detached ? registry.bucketCount() : 1);
        const auto capacity = std::max(registry.bucketCount(), registry.preparedBucketCount());
        auto remainder = registry.prepareInsertions(capacity - 1 - registry.pendingInsertions());
        ASSERT_EQ(registry.pendingInsertions() + 1, capacity);
        const auto buckets = registry.bucketCount();
        const auto spareBuckets = registry.preparedBucketCount();
        const auto pending = registry.pendingInsertions();
        const auto revision = registry.getRevision();
        const auto serial = registry.getLastGenerated();
        // Cell restoration repeats registration after moving an actor from a
        // temporary reference into its stable list node. Also cover a repeat
        // registration of that same node while every spare slot is reserved.
        for (unsigned repeat = 0; repeat < 2; ++repeat)
        {
            registry.insert(relocated.getPtr());
            EXPECT_EQ(registry.getOrEmpty(id), relocated.getPtr());
            EXPECT_EQ(registry.bucketCount(), buckets);
            EXPECT_EQ(registry.preparedBucketCount(), spareBuckets);
            EXPECT_EQ(registry.pendingInsertions(), pending);
            EXPECT_EQ(registry.getRevision(), revision + repeat + 1);
            EXPECT_EQ(registry.getLastGenerated(), serial);
        }
        registry.remove(*original.getPtr().mRef);
        EXPECT_EQ(registry.getOrEmpty(id), relocated.getPtr());
        std::vector<std::unique_ptr<MWWorld::ManualRef>> refs;
        for (auto* reservations : {&pool, &remainder})
        {
            while (reservations->size())
            {
                refs.push_back(std::make_unique<MWWorld::ManualRef>(store, base.mId));
                registry.insertPrepared(refs.back()->getPtr(), *reservations);
                EXPECT_EQ(registry.getOrEmpty(refs.back()->getPtr().getCellRef().getRefNum()), refs.back()->getPtr());
                EXPECT_EQ(registry.bucketCount(), capacity);
            }
        }
        EXPECT_EQ(registry.pendingInsertions(), 0u);
        EXPECT_EQ(refs.size(), pending);
        EXPECT_EQ(registry.getOrEmpty(id), relocated.getPtr());
    }
}

TEST(MWWorldPtrTest, preparedRegistrationsRejectForeignOwnersAndNamespaceFailureBeforeMutation)
{
    MWClass::Weapon::registerSelf();
    MWWorld::ESMStore store; ESM::Weapon base{}; base.blank();
    base.mId = ESM::RefId::stringRefId("reserved-registry-errors"); store.insertStatic(base);
    MWWorld::PtrRegistry registry, foreign;
    auto pool = registry.prepareInsertions(3);
    MWWorld::ManualRef ref(store, base.mId);
    EXPECT_THROW(foreign.insertPrepared(ref.getPtr(), pool), std::logic_error);
    EXPECT_FALSE(ref.getPtr().getCellRef().getRefNum().isSet());
    for (const auto serial : {ESM::RefNum{2, 0}, ESM::RefNum{std::numeric_limits<std::uint32_t>::max(),
             std::numeric_limits<std::int32_t>::min()}})
    {
        registry.setLastGenerated(serial); const auto revision = registry.getRevision();
        EXPECT_THROW(registry.insertPrepared(ref.getPtr(), pool), std::exception);
        EXPECT_EQ(registry.getLastGenerated(), serial); EXPECT_EQ(registry.getRevision(), revision);
        EXPECT_FALSE(ref.getPtr().getCellRef().getRefNum().isSet()); EXPECT_EQ(pool.size(), 3u);
    }
    registry.setLastGenerated({std::numeric_limits<std::uint32_t>::max(), -2});
    registry.insertPrepared(ref.getPtr(), pool);
    EXPECT_EQ(ref.getPtr().getCellRef().getRefNum(), (ESM::RefNum{0, -3}));
    EXPECT_EQ(pool.size(), 2u);
    MWWorld::ManualRef alias(store, base.mId); alias.getPtr().getCellRef().setRefNum({0, -3});
    registry.insertPrepared(alias.getPtr(), pool);
    EXPECT_EQ(registry.getOrEmpty({0, -3}), alias.getPtr()); EXPECT_EQ(pool.size(), 2u);
    MWWorld::PtrRegistry::PreparedInsertions orphan;
    { MWWorld::PtrRegistry dead; orphan = dead.prepareInsertions(1); }
    EXPECT_THROW(MWWorld::PtrRegistry::PreparedInsertions copy(orphan), std::logic_error);
    EXPECT_THROW(registry.insertPrepared(ref.getPtr(), orphan), std::logic_error);
}

TEST(MWWorldPtrTest, preparedRegistryReplacementRetainsReservationsAndRejectsUnreservedGrowth)
{
    MWClass::Weapon::registerSelf();
    MWWorld::ESMStore store; ESM::Weapon base{}; base.blank();
    base.mId = ESM::RefId::stringRefId("reserved-registry-replacement"); store.insertStatic(base);
    ESM::ReadersCache readers; MWWorld::WorldModel model(store, readers);
    MWWorld::ManualRef old(store, base.mId), inserted(store, base.mId), later(store, base.mId);
    model.registerPtr(old.getPtr());
    auto pool = model.preparePtrInsertions(1);
    auto replacement = model.preparePtrReplacement(std::array{old.getPtr()}, std::array{inserted.getPtr()});
    replacement.commit();
    model.registerPreparedPtr(later.getPtr(), pool);
    EXPECT_EQ(model.getPtr(later.getPtr().getCellRef().getRefNum()), later.getPtr());
    EXPECT_EQ(later.getPtr().mRef->mWorldModel, &model); EXPECT_EQ(model.pendingPtrInsertions(), 0u);
    auto stale = model.preparePtrReplacement({}, {});
    const auto revision = model.getPtrRegistryRevision();
    auto large = model.preparePtrInsertions(1024);
    EXPECT_EQ(model.getPtrRegistryRevision(), revision);
    EXPECT_FALSE(stale.isValid()); EXPECT_THROW(stale.commit(), std::logic_error);
    EXPECT_EQ(model.getPtr(later.getPtr().getCellRef().getRefNum()), later.getPtr());
    auto fresh = model.preparePtrReplacement({}, {}); fresh.commit();
    MWWorld::ManualRef after(store, base.mId); model.registerPreparedPtr(after.getPtr(), large);
    EXPECT_EQ(model.pendingPtrInsertions(), 1023u);
}


TEST(MWWorldPtrTest, singletonPreparedRegistrationMaterializesTheFirstHashTableBeforeConsumption)
{
    MWClass::Weapon::registerSelf();
    MWWorld::ESMStore store; ESM::Weapon base{}; base.blank();
    base.mId = ESM::RefId::stringRefId("reserved-singleton"); store.insertStatic(base);
    MWWorld::ManualRef ref(store, base.mId);
    MWWorld::PtrRegistry registry;
    auto pool = registry.prepareInsertions(1);
    EXPECT_EQ(registry.bucketCount(), 1u); // The live table is untouched by preparation.
    const auto reservedBuckets = registry.preparedBucketCount();
    EXPECT_GT(reservedBuckets, 1u);
    registry.insertPrepared(ref.getPtr(), pool);
    EXPECT_EQ(registry.bucketCount(), reservedBuckets);
    EXPECT_EQ(registry.pendingInsertions(), 0u);
    EXPECT_EQ(registry.getOrEmpty(ref.getPtr().getCellRef().getRefNum()), ref.getPtr());
}


TEST(MWWorldPtrTest, registryPreparationAndCancellationPreserveLiveIterationAndReferences)
{
    MWClass::Weapon::registerSelf();
    MWWorld::ESMStore store; ESM::Weapon base{}; base.blank();
    base.mId = ESM::RefId::stringRefId("reserved-order"); store.insertStatic(base);
    MWWorld::PtrRegistry registry;
    std::vector<std::unique_ptr<MWWorld::ManualRef>> refs;
    for (unsigned i = 0; i < 64; ++i)
    {
        refs.push_back(std::make_unique<MWWorld::ManualRef>(store, base.mId));
        registry.insert(refs.back()->getPtr());
    }
    const auto order = [&]() {
        std::vector<ESM::RefNum> ids;
        for (const auto& [id, ptr] : registry) ids.push_back(id);
        return ids;
    };
    const auto before = order(); const auto buckets = registry.bucketCount();
    const auto* first = &*registry.begin(); const auto revision = registry.getRevision();
    {
        auto huge = registry.prepareInsertions(1024);
        auto copy = huge;
        EXPECT_EQ(order(), before); EXPECT_EQ(registry.bucketCount(), buckets);
        EXPECT_EQ(&*registry.begin(), first); EXPECT_EQ(registry.getRevision(), revision);
    }
    EXPECT_EQ(order(), before); EXPECT_EQ(registry.bucketCount(), buckets);
    EXPECT_EQ(&*registry.begin(), first); EXPECT_EQ(registry.pendingInsertions(), 0u);
    auto pool = registry.prepareInsertions(1024);
    const auto reservedBuckets = registry.preparedBucketCount();
    MWWorld::ManualRef inserted(store, base.mId); registry.insertPrepared(inserted.getPtr(), pool);
    EXPECT_EQ(registry.bucketCount(), reservedBuckets);
    for (const auto& ref : refs)
        EXPECT_EQ(registry.getOrEmpty(ref->getPtr().getCellRef().getRefNum()), ref->getPtr());
}

#include "apps/openmw/mwclass/classes.hpp"
#include <components/esm3/loadarmo.hpp>
#include <components/esm3/loadclot.hpp>
#include <components/esm3/loadligh.hpp>
#include <components/esm3/loadlock.hpp>
#include <components/esm3/loadprob.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadclot.hpp>
#include "apps/openmw/mwworld/inventorystore.hpp"

TEST(MWWorldPtrTest, InlineEquipmentAlternativesPreserveLegacyAndNativeSlotOrdering)
{
    MWClass::registerClasses(); using I = MWWorld::InventoryStore;
    const auto check = [&]<class T>(const T& record, std::initializer_list<int> expected, bool stack = false) {
        ESM::CellRef cell; cell.blank(); MWWorld::LiveCellRef<T> live(cell, &record); MWWorld::Ptr ptr(&live);
        const auto result = ptr.getClass().getEquipmentSlots(ptr);
        EXPECT_THAT(result.first, ::testing::ElementsAreArray(expected)); EXPECT_EQ(result.second, stack);
    };
    ESM::Armor armor{}; armor.blank();
    const std::pair<int,int> armorSlots[] = {{ESM::Armor::Helmet,I::Slot_Helmet}, {ESM::Armor::Cuirass,I::Slot_Cuirass},
        {ESM::Armor::LPauldron,I::Slot_LeftPauldron}, {ESM::Armor::RPauldron,I::Slot_RightPauldron},
        {ESM::Armor::Greaves,I::Slot_Greaves}, {ESM::Armor::Boots,I::Slot_Boots},
        {ESM::Armor::LGauntlet,I::Slot_LeftGauntlet}, {ESM::Armor::RGauntlet,I::Slot_RightGauntlet},
        {ESM::Armor::Shield,I::Slot_CarriedLeft}, {ESM::Armor::LBracer,I::Slot_LeftGauntlet},
        {ESM::Armor::RBracer,I::Slot_RightGauntlet}};
    for (const auto& [type,slot] : armorSlots) { armor.mData.mType=type; check(armor,{slot}); }
    armor.mData.mType=999; check(armor,{});
    ESM::Clothing clothing{}; clothing.blank();
    const std::pair<int,int> clothingSlots[] = {{ESM::Clothing::Shirt,I::Slot_Shirt}, {ESM::Clothing::Belt,I::Slot_Belt},
        {ESM::Clothing::Robe,I::Slot_Robe}, {ESM::Clothing::Pants,I::Slot_Pants}, {ESM::Clothing::Shoes,I::Slot_Boots},
        {ESM::Clothing::LGlove,I::Slot_LeftGauntlet}, {ESM::Clothing::RGlove,I::Slot_RightGauntlet},
        {ESM::Clothing::Skirt,I::Slot_Skirt}, {ESM::Clothing::Amulet,I::Slot_Amulet}};
    for (const auto& [type,slot] : clothingSlots) { clothing.mData.mType=type; check(clothing,{slot}); }
    clothing.mData.mType=ESM::Clothing::Ring; check(clothing,{I::Slot_LeftRing,I::Slot_RightRing});
    clothing.mData.mType=999; check(clothing,{});
    ESM::Weapon weapon{}; weapon.blank();
    for (int type=ESM::Weapon::ShortBladeOneHand;type<=ESM::Weapon::Last;++type)
    {
        weapon.mData.mType=type;
        check(weapon,{type==ESM::Weapon::Arrow || type==ESM::Weapon::Bolt ? I::Slot_Ammunition : I::Slot_CarriedRight},
            type==ESM::Weapon::Arrow || type==ESM::Weapon::Bolt || type==ESM::Weapon::MarksmanThrown);
    }
    ESM::Light light{}; light.blank(); light.mData.mFlags=0; check(light,{});
    light.mData.mFlags=ESM::Light::Carry; check(light,{I::Slot_CarriedLeft});
    ESM::Lockpick pick{}; pick.blank(); check(pick,{I::Slot_CarriedRight});
    ESM::Probe probe{}; probe.blank(); check(probe,{I::Slot_CarriedRight});
    ESM4::Armor nativeArmor{}; ESM4::Clothing nativeClothing{};
    for (const auto& [flag,slot] : std::initializer_list<std::pair<unsigned,int>>{
        {ESM4::Armor::TES4_Head,I::Slot_Helmet}, {ESM4::Armor::TES4_LowerBody,I::Slot_Greaves},
        {ESM4::Armor::TES4_Hands,I::Slot_LeftGauntlet}, {ESM4::Armor::TES4_Feet,I::Slot_Boots},
        {ESM4::Armor::TES4_LeftRing,I::Slot_LeftRing}, {ESM4::Armor::TES4_RightRing,I::Slot_RightRing},
        {ESM4::Armor::TES4_Amulet,I::Slot_Amulet}, {ESM4::Armor::TES4_Shield,I::Slot_CarriedLeft}})
    {
        nativeArmor.mArmorFlags=flag; check(nativeArmor,{slot});
        if (flag!=ESM4::Armor::TES4_Shield)
        {
            nativeClothing.mClothingFlags=flag;
            check(nativeClothing,{flag==ESM4::Armor::TES4_LowerBody ? I::Slot_Pants : slot});
        }
    }
    nativeArmor.mArmorFlags=ESM4::Armor::TES4_RightRing|ESM4::Armor::TES4_LeftRing;
    nativeClothing.mClothingFlags=nativeArmor.mArmorFlags;
    check(nativeArmor,{I::Slot_RightRing,I::Slot_LeftRing}); check(nativeClothing,{I::Slot_RightRing,I::Slot_LeftRing});
    nativeArmor.mArmorFlags|=ESM4::Armor::TES4_Head; nativeClothing.mClothingFlags=nativeArmor.mArmorFlags;
    check(nativeArmor,{I::Slot_Helmet}); check(nativeClothing,{I::Slot_Helmet});
    nativeArmor.mArmorFlags=ESM4::Armor::TES4_UpperBody; nativeClothing.mClothingFlags=nativeArmor.mArmorFlags;
    check(nativeArmor,{I::Slot_Cuirass}); check(nativeClothing,{I::Slot_Shirt});
    nativeArmor.mArmorFlags=0; nativeClothing.mClothingFlags=0; check(nativeArmor,{}); check(nativeClothing,{});
}

TEST(MWWorldPtrTest, preparedCellPublicationRejectsBeforeMovingNodesAndCanRetry)
{
    MWClass::Weapon::registerSelf();
    for (int failure : {0, 1, 2}) // foreign reservation owner, invalid namespace, exhausted namespace
    {
        SCOPED_TRACE(failure);
        MWWorld::ESMStore store;
        ESM::ReadersCache readers;
        MWWorld::WorldModel model(store, readers), foreign(store, readers);
        auto plan = MWWorld::CellStore::prepareReference(ESM::REC_WEAP, model);
        ASSERT_TRUE(plan);
        auto* node = &plan->reference();
        MWWorld::CellRefList<ESM::Weapon> target;
        if (failure == 1) model.setLastGeneratedRefNum({1, 0});
        if (failure == 2)
            model.setLastGeneratedRefNum({std::numeric_limits<std::uint32_t>::max(),
                std::numeric_limits<std::int32_t>::min()});
        const auto serial = model.getLastGeneratedRefNum();
        const auto revision = model.getPtrRegistryRevision();
        const auto foreignRevision = foreign.getPtrRegistryRevision();
        EXPECT_THROW(plan->publish(target, failure == 0 ? foreign : model, nullptr), std::exception);
        EXPECT_TRUE(target.mList.empty());
        ASSERT_NO_THROW(plan->reference());
        EXPECT_EQ(&plan->reference(), node);
        EXPECT_EQ(node->mWorldModel, nullptr);
        EXPECT_FALSE(node->mRef.getRefNum().isSet());
        EXPECT_EQ(model.pendingPtrInsertions(), 1u);
        EXPECT_EQ(model.getLastGeneratedRefNum(), serial);
        EXPECT_EQ(model.getPtrRegistryRevision(), revision);
        EXPECT_EQ(foreign.getPtrRegistryRevision(), foreignRevision);
        model.setLastGeneratedRefNum({0, -1});
        ASSERT_NO_THROW(plan->publish(target, model, nullptr));
        ASSERT_EQ(target.mList.size(), 1u);
        EXPECT_EQ(&target.mList.front(), node);
        const ESM::RefNum id{1, -1};
        EXPECT_EQ(node->mRef.getRefNum(), id);
        EXPECT_EQ(model.getPtr(id), MWWorld::Ptr(node));
        EXPECT_EQ(model.pendingPtrInsertions(), 0u);
        EXPECT_THROW(plan->publish(target, model, nullptr), std::logic_error);
        EXPECT_EQ(target.mList.size(), 1u);
        EXPECT_EQ(model.getPtrRegistryRevision(), revision + 1);
        plan.reset();
        EXPECT_EQ(model.getPtr(id), MWWorld::Ptr(node));
        target.mList.clear();
        EXPECT_TRUE(model.getPtr(id).isEmpty());
        EXPECT_EQ(model.getPtrRegistryRevision(), revision + 2);
    }
}

TEST(MWWorldPtrTest, retainedRegistryRestoresOriginalNodesAndSeparatesReservationDomains)
{
    MWClass::Weapon::registerSelf();
    MWWorld::ESMStore store; ESM::Weapon base{}; base.blank();
    base.mId = ESM::RefId::stringRefId("retained_registry_weapon"); store.insertStatic(base);
    MWWorld::ManualRef old(store, base.mId), incoming(store, base.mId), later(store, base.mId);
    MWWorld::PtrRegistry registry; registry.insert(old.getPtr());
    const auto id = old.getPtr().getCellRef().getRefNum();
    const auto* node = &*registry.begin();
    const auto revision = registry.getRevision(); const auto serial = registry.getLastGenerated();
    auto oldCredits = registry.prepareInsertions(1);
    auto runtime = registry.retainRuntime();
    auto incomingCredits = registry.prepareInsertions(4096);
    auto oldCopy = oldCredits;
    EXPECT_EQ(registry.pendingInsertions(), 4096u);
    EXPECT_EQ(registry.getOrEmpty(id), old.getPtr()); EXPECT_EQ(&*registry.begin(), node);
    EXPECT_THROW(registry.clear(), std::logic_error);
    EXPECT_THROW(registry.insert(incoming.getPtr()), std::logic_error);
    EXPECT_THROW(registry.insertPrepared(incoming.getPtr(), incomingCredits), std::logic_error);
    EXPECT_THROW(registry.retainRuntime(), std::logic_error);
    EXPECT_FALSE(incoming.getPtr().getCellRef().getRefNum().isSet());
    ASSERT_TRUE(runtime.detach()); EXPECT_FALSE(runtime.detach());
    EXPECT_TRUE(registry.getOrEmpty(id).isEmpty());
    registry.insertPrepared(incoming.getPtr(), incomingCredits);
    EXPECT_EQ(incoming.getPtr().getCellRef().getRefNum(), id);
    EXPECT_EQ(registry.getOrEmpty(id), incoming.getPtr());
    auto moreIncoming = registry.prepareInsertions(4096);
    auto anotherOldCopy = oldCredits; // Reserve in the retained domain after detach too.
    ASSERT_TRUE(runtime.restore()); EXPECT_FALSE(runtime.restore());
    EXPECT_EQ(registry.getOrEmpty(id), old.getPtr()); EXPECT_EQ(&*registry.begin(), node);
    EXPECT_EQ(registry.getRevision(), revision); EXPECT_EQ(registry.getLastGenerated(), serial);
    EXPECT_EQ(registry.pendingInsertions(), 3u);
    EXPECT_THROW(registry.insertPrepared(later.getPtr(), incomingCredits), std::logic_error);
    EXPECT_THROW(MWWorld::PtrRegistry::PreparedInsertions{incomingCredits}, std::logic_error);
    EXPECT_THROW(MWWorld::PtrRegistry::PreparedInsertions{moreIncoming}, std::logic_error);
    registry.insertPrepared(later.getPtr(), oldCredits);
    EXPECT_EQ(later.getPtr().getCellRef().getRefNum(), (ESM::RefNum{serial.mIndex + 1, serial.mContentFile}));
    EXPECT_EQ(registry.pendingInsertions(), 2u);
}

TEST(MWWorldPtrTest, retainedRegistryCancelsPreparationAndRejectsChangedSources)
{
    MWClass::Weapon::registerSelf();
    MWWorld::ESMStore store; ESM::Weapon base{}; base.blank();
    base.mId = ESM::RefId::stringRefId("cancelled_registry_weapon"); store.insertStatic(base);
    MWWorld::ManualRef old(store, base.mId); MWWorld::PtrRegistry registry; registry.insert(old.getPtr());
    const auto id = old.getPtr().getCellRef().getRefNum(); const auto revision = registry.getRevision();
    auto oldCredits = registry.prepareInsertions(1);
    MWWorld::PtrRegistry::PreparedInsertions incoming;
    {
        auto runtime = registry.retainRuntime(); incoming = registry.prepareInsertions(8);
        auto moved = std::move(runtime); EXPECT_FALSE(runtime.detach());
        EXPECT_EQ(registry.getOrEmpty(id), old.getPtr());
    }
    EXPECT_EQ(registry.pendingInsertions(), 1u); EXPECT_EQ(registry.getRevision(), revision);
    EXPECT_EQ(registry.getOrEmpty(id), old.getPtr());
    EXPECT_THROW(MWWorld::PtrRegistry::PreparedInsertions{incoming}, std::logic_error);
    auto changed = registry.retainRuntime(); registry.remove(*old.getPtr().mRef);
    EXPECT_FALSE(changed.detach()); ASSERT_TRUE(changed.restore());
    EXPECT_TRUE(registry.getOrEmpty(id).isEmpty()); EXPECT_EQ(registry.getRevision(), revision + 1);
}

TEST(MWWorldPtrTest, retainedRegistryCommitKeepsIncomingNativeIndexAndRetiresOldCredits)
{
    MWClass::Weapon::registerSelf(); ESM::Weapon base{}; base.blank();
    base.mId = ESM::RefId::stringRefId("native_retention_weapon");
    ESM4::Reference reference{}; reference.mId = {31, -1}; reference.mBaseObj = {0x800, 0};
    reference.mFormKey = ESM::FormKey::dynamic("retained-native", 31);
    MWWorld::LiveCellRef<ESM::Weapon> old(reference, &base);
    reference.mId = {32, -1}; MWWorld::LiveCellRef<ESM::Weapon> incoming(reference, &base);
    MWWorld::PtrRegistry registry; registry.insert(MWWorld::Ptr(&old));
    auto oldCredits = registry.prepareInsertions(1);
    auto runtime = registry.retainRuntime(); auto incomingCredits = registry.prepareInsertions(1);
    ASSERT_TRUE(runtime.detach()); EXPECT_TRUE(registry.getDynamicNativePtr(reference.mFormKey).isEmpty());
    registry.insert(MWWorld::Ptr(&incoming)); runtime.discard();
    EXPECT_FALSE(runtime.restore()); EXPECT_FALSE(runtime.isDetached());
    EXPECT_EQ(registry.getDynamicNativePtr(reference.mFormKey), MWWorld::Ptr(&incoming));
    EXPECT_EQ(registry.getOrEmpty({32, -1}), MWWorld::Ptr(&incoming));
    EXPECT_EQ(registry.pendingInsertions(), 1u);
    EXPECT_THROW(MWWorld::PtrRegistry::PreparedInsertions{oldCredits}, std::logic_error);
    EXPECT_NO_THROW(MWWorld::PtrRegistry::PreparedInsertions{incomingCredits});
}

TEST(MWWorldPtrTest, retainedRegistryTracksMovedOwnersAndRejectsDestroyedOwners)
{
    MWClass::Weapon::registerSelf(); MWWorld::ESMStore store; ESM::Weapon base{}; base.blank();
    base.mId = ESM::RefId::stringRefId("moving_registry_weapon"); store.insertStatic(base);
    MWWorld::ManualRef old(store, base.mId), incoming(store, base.mId);
    auto owner = std::make_unique<MWWorld::PtrRegistry>(); owner->insert(old.getPtr());
    const auto id = old.getPtr().getCellRef().getRefNum(); auto runtime = owner->retainRuntime();
    auto incomingCredits = owner->prepareInsertions(1); ASSERT_TRUE(runtime.detach());
    MWWorld::PtrRegistry moved(std::move(*owner)); owner.reset();
    moved.insertPrepared(incoming.getPtr(), incomingCredits); ASSERT_TRUE(runtime.restore());
    EXPECT_EQ(moved.getOrEmpty(id), old.getPtr());
    owner = std::make_unique<MWWorld::PtrRegistry>();
    auto oldCredits = owner->prepareInsertions(1); auto destroyed = owner->retainRuntime();
    auto rejected = owner->prepareInsertions(1); ASSERT_TRUE(destroyed.detach()); owner.reset();
    EXPECT_FALSE(destroyed.restore()); EXPECT_FALSE(destroyed.detach());
    EXPECT_THROW(MWWorld::PtrRegistry::PreparedInsertions{oldCredits}, std::logic_error);
    EXPECT_THROW(MWWorld::PtrRegistry::PreparedInsertions{rejected}, std::logic_error);
}

TEST(MWWorldPtrTest, retainedWorldModelRestoresCellAddressesAndDisposesIncomingNodes)
{
    MWClass::Weapon::registerSelf(); MWWorld::ESMStore store; ESM::Weapon base{}; base.blank();
    base.mId = ESM::RefId::stringRefId("retained_cell_weapon"); store.insertStatic(base);
    ESM::ReadersCache readers; MWWorld::WorldModel model(store, readers);
    auto* oldCell = &model.getDraftCell();
    ESM::CellRef reference{}; reference.blank(); reference.mRefID = base.mId; reference.mRefNum = {77, 0};
    MWWorld::LiveCellRef<ESM::Weapon> live(reference, &base);
    MWWorld::Ptr old(oldCell->insert(&live), oldCell); old.getCellRef().setCount(3);
    old.getCellRef().setCharge(17); old.getRefData().disable(); model.registerPtr(old);
    model.setLastGeneratedRefNum({9, -1});
    const auto revision = model.getPtrRegistryRevision(); const auto serial = model.getLastGeneratedRefNum();
    auto runtime = model.retainRuntime(); auto incomingCredits = model.preparePtrInsertions(3);
    EXPECT_THROW(model.clear(), std::logic_error); EXPECT_EQ(model.getPtr(reference.mRefNum), old);
    EXPECT_THROW(model.getExterior(ESM::ExteriorCellLocation(123, 456, ESM::Cell::sDefaultWorldspaceId), false), std::logic_error);
    ASSERT_TRUE(runtime.detach()); EXPECT_FALSE(model.ownsCell(*oldCell));
    auto* incomingCell = &model.getDraftCell(); ASSERT_NE(incomingCell, oldCell);
    MWWorld::Ptr incoming(incomingCell->insert(&live), incomingCell); model.registerPreparedPtr(incoming, incomingCredits);
    EXPECT_EQ(model.getPtr(reference.mRefNum), incoming);
    EXPECT_EQ(old.getCellRef().getCount(false), 3); EXPECT_EQ(old.getCellRef().getCharge(), 17);
    ASSERT_TRUE(runtime.restore()); EXPECT_FALSE(runtime.restore());
    EXPECT_EQ(&model.getDraftCell(), oldCell); EXPECT_TRUE(model.ownsCell(*oldCell));
    EXPECT_EQ(model.getPtr(reference.mRefNum), old); EXPECT_EQ(model.getResidentPtr(reference.mRefNum), old);
    EXPECT_FALSE(old.getRefData().isEnabled()); EXPECT_EQ(old.getCellRef().getCount(false), 3);
    EXPECT_EQ(model.getPtrRegistryRevision(), revision); EXPECT_EQ(model.getLastGeneratedRefNum(), serial);
    EXPECT_EQ(model.pendingPtrInsertions(), 0u);
    EXPECT_THROW(MWWorld::PtrRegistry::PreparedInsertions{incomingCredits}, std::logic_error);
}

TEST(MWWorldPtrTest, retainedWorldModelCommitAndOwnerDestructionKeepRegistrationsSafe)
{
    MWClass::Weapon::registerSelf(); MWWorld::ESMStore store; ESM::Weapon base{}; base.blank();
    base.mId = ESM::RefId::stringRefId("discarded_cell_weapon"); store.insertStatic(base);
    ESM::ReadersCache readers; auto model = std::make_unique<MWWorld::WorldModel>(store, readers);
    ESM::CellRef reference{}; reference.blank(); reference.mRefID = base.mId; reference.mRefNum = {81, 0};
    MWWorld::LiveCellRef<ESM::Weapon> live(reference, &base);
    auto* cell = &model->getDraftCell(); MWWorld::Ptr old(cell->insert(&live), cell); model->registerPtr(old);
    auto runtime = model->retainRuntime(); ASSERT_TRUE(runtime.detach());
    cell = &model->getDraftCell(); MWWorld::Ptr incoming(cell->insert(&live), cell); model->registerPtr(incoming);
    runtime.discard(); EXPECT_FALSE(runtime.restore()); EXPECT_EQ(model->getPtr(reference.mRefNum), incoming);
    auto destroyed = model->retainRuntime(); ASSERT_TRUE(destroyed.detach()); model.reset();
    EXPECT_FALSE(destroyed.restore()); EXPECT_FALSE(destroyed.detach());
}

TEST(MWWorldPtrTest, retainedRegistryExchangeKeepsBothCreditDomainsAndOriginalNodes)
{
    MWClass::Weapon::registerSelf();
    MWWorld::ESMStore store; ESM::Weapon base{}; base.blank();
    base.mId = ESM::RefId::stringRefId("exchange_registry_weapon"); store.insertStatic(base);
    MWWorld::ManualRef old(store, base.mId), incoming(store, base.mId), oldExtra(store, base.mId), newExtra(store, base.mId);
    auto owner = std::make_unique<MWWorld::PtrRegistry>(); owner->insert(old.getPtr());
    const auto id = old.getPtr().getCellRef().getRefNum();
    const auto* node = &*owner->begin();
    auto oldCredits = owner->prepareInsertions(2);
    auto runtime = owner->retainRuntime();
    EXPECT_FALSE(runtime.exchange());
    auto incomingCredits = owner->prepareInsertions(2);
    ASSERT_TRUE(runtime.detach()); owner->insertPrepared(incoming.getPtr(), incomingCredits);
    ASSERT_TRUE(runtime.exchange());
    EXPECT_EQ(owner->getOrEmpty(id), old.getPtr()); EXPECT_EQ(&*owner->begin(), node);
    owner->insertPrepared(oldExtra.getPtr(), oldCredits);
    auto copiedIncoming = incomingCredits; // allocate against the parked candidate domain
    ASSERT_TRUE(runtime.exchange());
    EXPECT_EQ(owner->getOrEmpty(id), incoming.getPtr());
    EXPECT_THROW(owner->insertPrepared(oldExtra.getPtr(), oldCredits), std::logic_error);
    owner->insertPrepared(newExtra.getPtr(), copiedIncoming);
    ASSERT_TRUE(runtime.exchange());
    MWWorld::PtrRegistry moved(std::move(*owner)); owner.reset();
    ASSERT_TRUE(runtime.restore());
    EXPECT_EQ(moved.getOrEmpty(id), old.getPtr());
    EXPECT_EQ(moved.getOrEmpty(oldExtra.getPtr().getCellRef().getRefNum()), oldExtra.getPtr());
    EXPECT_EQ(moved.getLastGenerated(), oldExtra.getPtr().getCellRef().getRefNum());
    EXPECT_THROW(MWWorld::PtrRegistry::PreparedInsertions{incomingCredits}, std::logic_error);
    EXPECT_NO_THROW(MWWorld::PtrRegistry::PreparedInsertions{oldCredits});
    EXPECT_FALSE(runtime.exchange());
}

TEST(MWWorldPtrTest, retainedRegistryExchangeCommitAndOwnerDeathRetireCorrectDomains)
{
    MWClass::Weapon::registerSelf(); ESM::Weapon base{}; base.blank();
    ESM4::Reference reference{}; reference.mId = {31, -1}; reference.mBaseObj = {0x800, 0};
    reference.mFormKey = ESM::FormKey::dynamic("exchange-native", 31);
    MWWorld::LiveCellRef<ESM::Weapon> old(reference, &base);
    reference.mId = {32, -1}; MWWorld::LiveCellRef<ESM::Weapon> incoming(reference, &base);
    auto owner = std::make_unique<MWWorld::PtrRegistry>(); owner->insert(MWWorld::Ptr(&old));
    auto oldCredits = owner->prepareInsertions(1); auto runtime = owner->retainRuntime();
    auto newCredits = owner->prepareInsertions(1); ASSERT_TRUE(runtime.detach());
    owner->insert(MWWorld::Ptr(&incoming)); ASSERT_TRUE(runtime.exchange());
    EXPECT_EQ(owner->getDynamicNativePtr(reference.mFormKey), MWWorld::Ptr(&old));
    runtime.discard();
    EXPECT_EQ(owner->getDynamicNativePtr(reference.mFormKey), MWWorld::Ptr(&incoming));
    EXPECT_THROW(MWWorld::PtrRegistry::PreparedInsertions{oldCredits}, std::logic_error);
    EXPECT_NO_THROW(MWWorld::PtrRegistry::PreparedInsertions{newCredits});
    auto destroyed = owner->retainRuntime(); auto latestCredits = owner->prepareInsertions(1);
    ASSERT_TRUE(destroyed.detach()); ASSERT_TRUE(destroyed.exchange()); owner.reset();
    EXPECT_FALSE(destroyed.restore()); EXPECT_FALSE(destroyed.exchange());
    EXPECT_THROW(MWWorld::PtrRegistry::PreparedInsertions{newCredits}, std::logic_error);
    EXPECT_THROW(MWWorld::PtrRegistry::PreparedInsertions{latestCredits}, std::logic_error);
}

TEST(MWWorldPtrTest, retainedWorldModelExchangeSelectsCellsIndexesAndScopeRollbackTogether)
{
    MWClass::Weapon::registerSelf(); MWWorld::ESMStore store; ESM::Weapon base{}; base.blank();
    base.mId = ESM::RefId::stringRefId("exchange_model_weapon"); store.insertStatic(base);
    ESM::ReadersCache readers; MWWorld::WorldModel model(store, readers);
    auto* oldCell = &model.getDraftCell();
    ESM::CellRef reference{}; reference.blank(); reference.mRefID = base.mId; reference.mRefNum = {77, 0};
    MWWorld::LiveCellRef<ESM::Weapon> live(reference, &base);
    MWWorld::Ptr old(oldCell->insert(&live), oldCell); model.registerPtr(old);
    auto oldCredits = model.preparePtrInsertions(1);
    MWWorld::PtrRegistry::PreparedInsertions incomingCredits;
    {
        auto runtime = model.retainRuntime(); incomingCredits = model.preparePtrInsertions(1);
        ASSERT_TRUE(runtime.detach());
        auto* incomingCell = &model.getDraftCell();
        MWWorld::Ptr incoming(incomingCell->insert(&live), incomingCell); model.registerPreparedPtr(incoming, incomingCredits);
        ASSERT_TRUE(runtime.exchange());
        EXPECT_EQ(&model.getDraftCell(), oldCell); EXPECT_EQ(model.getPtr(reference.mRefNum), old);
        EXPECT_TRUE(model.ownsCell(*oldCell)); EXPECT_FALSE(model.ownsCell(*incomingCell));
        auto candidateCopy = incomingCredits;
        ASSERT_TRUE(runtime.exchange());
        EXPECT_EQ(&model.getDraftCell(), incomingCell); EXPECT_EQ(model.getPtr(reference.mRefNum), incoming);
        EXPECT_TRUE(model.ownsCell(*incomingCell)); EXPECT_FALSE(model.ownsCell(*oldCell));
        ASSERT_TRUE(runtime.exchange()); // scope rollback starts with original selected
    }
    EXPECT_EQ(&model.getDraftCell(), oldCell); EXPECT_EQ(model.getPtr(reference.mRefNum), old);
    EXPECT_NO_THROW(MWWorld::PtrRegistry::PreparedInsertions{oldCredits});
    EXPECT_THROW(MWWorld::PtrRegistry::PreparedInsertions{incomingCredits}, std::logic_error);
}

TEST(MWWorldPtrTest, retainedWorldModelExchangeDiscardAndDestructionPreserveCandidateRegistrations)
{
    MWClass::Weapon::registerSelf(); MWWorld::ESMStore store; ESM::Weapon base{}; base.blank();
    base.mId = ESM::RefId::stringRefId("exchange_discard_model_weapon"); store.insertStatic(base);
    ESM::ReadersCache readers; auto model = std::make_unique<MWWorld::WorldModel>(store, readers);
    ESM::CellRef reference{}; reference.blank(); reference.mRefID = base.mId; reference.mRefNum = {81, 0};
    MWWorld::LiveCellRef<ESM::Weapon> live(reference, &base);
    auto* oldCell = &model->getDraftCell(); MWWorld::Ptr old(oldCell->insert(&live), oldCell); model->registerPtr(old);
    auto runtime = model->retainRuntime(); ASSERT_TRUE(runtime.detach());
    auto* cell = &model->getDraftCell(); MWWorld::Ptr incoming(cell->insert(&live), cell); model->registerPtr(incoming);
    ASSERT_TRUE(runtime.exchange()); runtime.discard();
    EXPECT_EQ(model->getPtr(reference.mRefNum), incoming); EXPECT_EQ(&model->getDraftCell(), cell);
    auto destroyed = model->retainRuntime(); ASSERT_TRUE(destroyed.detach());
    ASSERT_TRUE(destroyed.exchange()); model.reset();
    EXPECT_FALSE(destroyed.restore()); EXPECT_FALSE(destroyed.exchange());
}
