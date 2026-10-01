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
