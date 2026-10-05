#include <apps/openmw/mwworld/worldmodel.hpp>
#include <apps/openmw/mwbase/environment.hpp>
#include <apps/openmw/mwworld/esmstore.hpp>
#include <apps/openmw/mwclass/esm4interactive.hpp>
#include <apps/openmw/mwphysics/physicssystem.hpp>
#include <apps/openmw/mwrender/objects.hpp>
#include <apps/openmw/mwrender/vismask.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/loadachr.hpp>
#include <components/esm3/readerscache.hpp>
#include <memory>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/common.hpp>
#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/sceneutil/unrefqueue.hpp>
#include <components/testing/util.hpp>
#include <components/vfs/manager.hpp>
#include <gtest/gtest.h>
#include <osgDB/Registry>
#include <osgDB/ReaderWriter>
#include <bit>
#include <functional>
#include <new>
#include <limits>
#include <sstream>

namespace
{
    class LooseWeaponRoot : public osg::Group
    {
    public:
        enum class Failure { None, Reject, InsertThrow };
        Failure mFailure = Failure::None;
        std::function<void()> mAfterInsert;
        bool addChild(osg::Node* node) override
        {
            if (mFailure == Failure::Reject) return false;
            const auto result = osg::Group::addChild(node);
            if (mAfterInsert) mAfterInsert();
            if (mFailure == Failure::InsertThrow) throw std::runtime_error("injected loose weapon scene failure");
            return result;
        }
    };

    struct LooseWeaponAdmissionTest : testing::Test
    {
        // Native Cell resolves its optional World record through Environment.
        // Keep this real store context alive beyond cells and scene owners.
        MWBase::Environment mEnvironment;
        static std::string modelBytes()
        {
            osg::ref_ptr<osg::Group> node = new osg::Group;
            node->setName("Test owned native weapon model");
            std::ostringstream out;
            auto* writer = osgDB::Registry::instance()->getReaderWriterForExtension("osgt");
            if (!writer || !writer->writeNode(*node, out).success())
                throw std::runtime_error("cannot write owned weapon model");
            return out.str();
        }
        TestingOpenMW::VFSTestFile mFile{modelBytes()};
        std::unique_ptr<VFS::Manager> mVfs = TestingOpenMW::createTestVFS({
            {VFS::Path::NormalizedView("weapon.osgt"), &mFile}});
        Resource::ResourceSystem mResources{mVfs.get(), 0., nullptr};
        SceneUtil::UnrefQueue mUnref;
        osg::ref_ptr<LooseWeaponRoot> mRoot = new LooseWeaponRoot;
        osg::ref_ptr<osg::Group> mPhysicsRoot = new osg::Group;
        MWWorld::ESMStore mStore;
        ESM::ReadersCache mReaders;
        std::unique_ptr<MWWorld::WorldModel> mWorld = std::make_unique<MWWorld::WorldModel>(mStore, mReaders);
        const ESM::FormKey mBaseKey = ESM::FormKey::content("owned.esm", 0x940);
        const ESM::FormKey mCellKey = ESM::FormKey::content("owned.esm", 1);
        MWWorld::CellStore* mCell = nullptr;
        const ESM4::Weapon* mBase = nullptr;
        std::unique_ptr<MWRender::Objects> mObjects;
        std::unique_ptr<MWPhysics::PhysicsSystem> mPhysics;
        NifBullet::ActorRagdollDefinition mDefinition;
        std::array<btTransform, 1> mPoses{btTransform(btQuaternion::getIdentity(), btVector3(0, 0, 2))};
        LooseWeaponAdmissionTest()
        {
            MWClass::ESM4Takeable<ESM4::Weapon>::registerSelf();
            mEnvironment.setESMStore(mStore);
            mEnvironment.setWorldModel(*mWorld);
            mEnvironment.setResourceSystem(mResources);
            ESM4::Cell cell{}; cell.mId = ESM::RefId(ESM::FormId{1, 0}); cell.mFormKey = mCellKey;
            cell.mCellFlags = ESM4::CELL_Interior; cell.mEditorId = "LooseWeaponAdmissionCell";
            mStore.getWritable<ESM4::Cell>().insertStatic(cell, mCellKey);
            mCell = &mWorld->getCell(cell.mId);
            ESM4::Weapon base{}; base.mId = {0x940, 0}; base.mData.type = 5;
            base.mData.health = 100; base.mModel = "weapon.osgt";
            base.mEnchantment = {0x950, 0}; base.mEnchantmentPoints = 20;
            mStore.getWritable<ESM4::Weapon>().insertStatic(base, mBaseKey);
            mBase = mStore.get<ESM4::Weapon>().find(base.mId);
            NifBullet::RagdollBodyDefinition body{};
            body.mRecord = 12; body.mMass = 2; body.mInertia = {1,0,0,0,1,0,0,0,1};
            body.mShape = NifBullet::RagdollSphere{.5f};
            mDefinition.mBodies.push_back(body);
            mObjects = std::make_unique<MWRender::Objects>(&mResources, mRoot, mUnref);
            mPhysics = std::make_unique<MWPhysics::PhysicsSystem>(&mResources, mPhysicsRoot);
        }
        MWWorld::LiveCellRef<ESM4::Weapon> source(std::uint64_t serial = 1)
        {
            ESM4::Reference placed{};
            placed.mFormKey = ESM::FormKey::dynamic("owned-loose-weapon", serial);
            placed.mBaseObj = mBase->mId; placed.mBaseKey = mBaseKey;
            placed.mParent = mCell->getCell()->getId(); placed.mParentKey = mCellKey;
            placed.mPos.pos[2] = 2;
            placed.mOwner = {0x960, 0};
            MWWorld::LiveCellRef<ESM4::Weapon> result(placed, mBase);
            result.mRef.setNativeItemCondition(-0.f);
            result.mRef.setEnchantmentCharge(7.25f);
            return result;
        }
        auto prepare(const MWWorld::LiveCellRef<ESM4::Weapon>& value)
        {
            return mWorld->prepareLooseWeaponAdmission(*mCell, value, *mObjects, *mPhysics,
                "weapon.osgt", osg::Quat(), MWRender::Mask_Object, mDefinition, 1.f, mPoses, 1, -1);
        }
        void emptyPublication()
        {
            EXPECT_EQ(mCell->count(), 0u);
            EXPECT_EQ(mRoot->getNumChildren(), 0u);
            EXPECT_TRUE(mPhysics->looseObjectOwners().empty());
            EXPECT_EQ(mUnref.getSize(), 0u);
            EXPECT_EQ(mWorld->getPtrRegistryView().begin(), mWorld->getPtrRegistryView().end());
        }
    };

    TEST_F(LooseWeaponAdmissionTest, CancellationAndSuccessfulPublicationOwnTheSameNativeInstance)
    {
        auto value = source();
        const auto revision = mWorld->getPtrRegistryRevision();
        const auto last = mWorld->getLastGeneratedRefNum();
        {
            auto cancelled = prepare(value);
            ASSERT_TRUE(cancelled->isValid());
            emptyPublication();
            EXPECT_EQ(mWorld->getLastGeneratedRefNum(), last);
            EXPECT_EQ(mWorld->getPtrRegistryRevision(), revision);
            EXPECT_FALSE(value.mRef.getRefNum().isSet());
        }
        emptyPublication();
        auto prepared = prepare(value);
        const auto ptr = prepared->commit();
        ASSERT_FALSE(ptr.isEmpty());
        EXPECT_EQ(mCell->count(), 1u);
        EXPECT_EQ(ptr.getCellRef().getFormKey(), value.mRef.getFormKey());
        EXPECT_EQ(ptr.getCellRef().getRefId(), ESM::RefId(mBase->mId));
        EXPECT_EQ(ptr.getCellRef().getOwner(), ESM::RefId(ESM::FormId{0x960, 0}));
        EXPECT_EQ(std::bit_cast<std::uint32_t>(*ptr.getCellRef().getNativeItemCondition()), 0x80000000u);
        EXPECT_FLOAT_EQ(ptr.getCellRef().getEnchantmentCharge(), 7.25f);
        EXPECT_EQ(mWorld->getPtr(ptr.getCellRef().getRefNum()), ptr);
        EXPECT_NE(mObjects->getAnimation(ptr), nullptr);
        EXPECT_NE(ptr.getRefData().getBaseNode(), nullptr);
        ASSERT_TRUE(mPhysics->hasLooseObject(ptr));
        EXPECT_EQ(mPhysics->captureLooseObject(ptr).size(), 1u);
        EXPECT_FALSE(prepared->isValid());
        EXPECT_TRUE(prepared->commit().isEmpty());
        prepared.reset();
        EXPECT_EQ(mCell->count(), 1u);
        EXPECT_TRUE(mPhysics->hasLooseObject(ptr));
        EXPECT_NE(mObjects->getAnimation(ptr), nullptr);
        EXPECT_THROW(prepare(value), std::invalid_argument);
    }

    TEST_F(LooseWeaponAdmissionTest, InvalidNativeIdentityExtrasAndBodyNeverPublish)
    {
        for (int field = 0; field < 11; ++field)
        {
            auto value = source();
            if (field == 0) value.mRef.setCount(2);
            if (field == 1) value.mRef.setScale(2);
            if (field == 2) value.mRef.setNativeItemCondition(101);
            if (field == 3) value.mRef.setEnchantmentCharge(21);
            if (field == 4)
            {
                ESM4::Reference placed = *value.mRef.getNativeReference(); placed.mBaseKey = mCellKey;
                value = MWWorld::LiveCellRef<ESM4::Weapon>(placed, mBase);
            }
            if (field == 5)
            {
                ESM4::Reference placed = *value.mRef.getNativeReference(); placed.mParentKey = mBaseKey;
                value = MWWorld::LiveCellRef<ESM4::Weapon>(placed, mBase);
            }
            if (field >= 7)
            {
                ESM4::Reference placed = *value.mRef.getNativeReference();
                if (field == 7) placed.mFormKey.mNamespace.clear();
                if (field == 8) placed.mFormKey.mNamespace = "bad:namespace";
                if (field == 9) placed.mFlags = ESM4::Rec_Disabled;
                if (field == 10) placed.mFlags = ESM4::Rec_Deleted;
                value = MWWorld::LiveCellRef<ESM4::Weapon>(placed, mBase);
            }
            // Test malformed reference fields independently of the body guard.
            mDefinition.mBodies.resize(1);
            if (field == 6) mDefinition.mBodies.push_back(mDefinition.mBodies.front());
            EXPECT_THROW(prepare(value), std::invalid_argument);
            emptyPublication();
        }
        mDefinition.mBodies.resize(1);
        mPoses[0].setOrigin(btVector3(std::numeric_limits<float>::quiet_NaN(), 0, 2));
        EXPECT_THROW(prepare(source()), std::invalid_argument);
        emptyPublication();
    }

    TEST_F(LooseWeaponAdmissionTest, SceneFailuresRollBackBeforeAnyRegistryCellOrPhysicsPublication)
    {
        for (auto failure : {LooseWeaponRoot::Failure::Reject, LooseWeaponRoot::Failure::InsertThrow})
        {
            auto prepared = prepare(source());
            const auto revision = mWorld->getPtrRegistryRevision();
            const auto last = mWorld->getLastGeneratedRefNum();
            mRoot->mFailure = failure;
            EXPECT_THROW(prepared->commit(), std::runtime_error);
            emptyPublication();
            EXPECT_EQ(mWorld->getPtrRegistryRevision(), revision);
            EXPECT_EQ(mWorld->getLastGeneratedRefNum(), last);
            EXPECT_FALSE(prepared->isValid());
            mRoot->mFailure = LooseWeaponRoot::Failure::None;
        }
        auto fresh = prepare(source());
        ASSERT_FALSE(fresh->commit().isEmpty());
    }

    TEST_F(LooseWeaponAdmissionTest, LateRegistryChangesRollBackActualPublishedModel)
    {
        auto prepared = prepare(source());
        auto existing = source(2);
        const MWWorld::Ptr unrelated(mCell->insert(&existing), mCell);
        mRoot->mAfterInsert = [&] { mWorld->registerPtr(unrelated); };
        EXPECT_THROW(prepared->commit(), std::logic_error);
        EXPECT_EQ(mCell->count(), 1u);
        EXPECT_EQ(mWorld->getPtr(unrelated.getCellRef().getRefNum()), unrelated);
        EXPECT_EQ(mRoot->getNumChildren(), 0u);
        EXPECT_TRUE(mPhysics->looseObjectOwners().empty());
        EXPECT_EQ(mUnref.getSize(), 0u);
        EXPECT_FALSE(prepared->isValid());
        mRoot->mAfterInsert = {};
        auto fresh = prepare(source());
        ASSERT_FALSE(fresh->commit().isEmpty());
        EXPECT_EQ(mCell->count(), 2u);
    }

    TEST_F(LooseWeaponAdmissionTest, RetiredWorldAndExternalOwnersRejectBeforeTheirBorrowedData)
    {
        auto prepared = prepare(source());
        mWorld->clear();
        EXPECT_FALSE(prepared->isValid());
        EXPECT_TRUE(prepared->commit().isEmpty());
        prepared.reset();
        mCell = &mWorld->getCell(ESM::RefId(ESM::FormId{1, 0}));
        prepared = prepare(source());
        mObjects.reset();
        EXPECT_FALSE(prepared->isValid());
        EXPECT_TRUE(prepared->commit().isEmpty());
        prepared.reset();
        mObjects = std::make_unique<MWRender::Objects>(&mResources, mRoot, mUnref);
        prepared = prepare(source());
        mPhysics.reset();
        EXPECT_FALSE(prepared->isValid());
        EXPECT_TRUE(prepared->commit().isEmpty());
        prepared.reset();
        mPhysics = std::make_unique<MWPhysics::PhysicsSystem>(&mResources, mPhysicsRoot);
        prepared = prepare(source());
        mWorld.reset();
        EXPECT_FALSE(prepared->isValid());
        EXPECT_TRUE(prepared->commit().isEmpty());
        prepared.reset();
    }

    TEST_F(LooseWeaponAdmissionTest, SceneCallbackClearingManagedCellsRollsBackBeforeBorrowedCellAccess)
    {
        auto prepared = prepare(source());
        mRoot->mAfterInsert = [&] { mWorld->clear(); };
        EXPECT_THROW(prepared->commit(), std::logic_error);
        EXPECT_EQ(mRoot->getNumChildren(), 0u);
        EXPECT_TRUE(mPhysics->looseObjectOwners().empty());
        EXPECT_EQ(mUnref.getSize(), 0u);
        EXPECT_FALSE(prepared->isValid());
        EXPECT_TRUE(prepared->commit().isEmpty());
        prepared.reset();
        mRoot->mAfterInsert = {};
        mCell = &mWorld->getCell(ESM::RefId(ESM::FormId{1, 0}));
        emptyPublication();
        auto fresh = prepare(source());
        ASSERT_FALSE(fresh->commit().isEmpty());
    }

    TEST_F(LooseWeaponAdmissionTest, RegistryLifetimeGuardSurvivesAddressReuseAndDeletedReference)
    {
        alignas(MWWorld::WorldModel) std::byte storage[sizeof(MWWorld::WorldModel)];
        auto* owner = new (storage) MWWorld::WorldModel(mStore, mReaders);
        auto value = std::make_unique<MWWorld::LiveCellRef<ESM4::Weapon>>(source());
        const std::array<MWWorld::Ptr, 1> inserted{MWWorld::Ptr(value.get(), mCell)};
        auto pending = owner->preparePtrReplacement({}, inserted);
        ASSERT_TRUE(pending.isValid());
        std::destroy_at(owner);
        value.reset();
        owner = new (storage) MWWorld::WorldModel(mStore, mReaders);
        EXPECT_FALSE(pending.isValid());
        EXPECT_THROW(pending.commit(), std::logic_error);
        std::destroy_at(owner);
    }
}

TEST(CellRefNativeCondition, SwapsOnlySupportedConditionStorageAcrossItemVariants)
{
    ESM::CellRef projectedRecord{};
    projectedRecord.mOwner = ESM::RefId(ESM::FormId{0x801, 0});
    projectedRecord.mEnchantmentCharge = 7.25f;
    ESM4::Reference placedRecord{};
    placedRecord.mOwner = {0x802, 0};
    placedRecord.mFormKey = ESM::FormKey::dynamic("condition-swap", 1);
    MWWorld::CellRef projected(projectedRecord), placed(placedRecord);
    placed.setNativeItemCondition(-0.f);
    placed.setEnchantmentCharge(6.5f);
    ASSERT_TRUE(projected.supportsNativeItemCondition());
    ASSERT_TRUE(placed.supportsNativeItemCondition());
    ASSERT_TRUE(projected.swapNativeItemCondition(placed));
    EXPECT_EQ(std::bit_cast<std::uint32_t>(*projected.getNativeItemCondition()), 0x80000000u);
    EXPECT_FALSE(placed.getNativeItemCondition());
    EXPECT_FLOAT_EQ(projected.getEnchantmentCharge(), 7.25f);
    EXPECT_FLOAT_EQ(placed.getEnchantmentCharge(), 6.5f);
    EXPECT_EQ(projected.getOwner(), projectedRecord.mOwner);
    EXPECT_EQ(placed.getOwner(), ESM::RefId(placedRecord.mOwner));
    EXPECT_EQ(placed.getFormKey(), placedRecord.mFormKey);
    ESM4::ActorCharacter actorRecord{};
    MWWorld::CellRef actor(actorRecord);
    EXPECT_FALSE(actor.supportsNativeItemCondition());
    EXPECT_FALSE(projected.swapNativeItemCondition(actor));
    EXPECT_FALSE(actor.swapNativeItemCondition(projected));
    EXPECT_EQ(std::bit_cast<std::uint32_t>(*projected.getNativeItemCondition()), 0x80000000u);
    ASSERT_TRUE(projected.swapNativeItemCondition(placed));
    EXPECT_FALSE(projected.getNativeItemCondition());
    EXPECT_EQ(std::bit_cast<std::uint32_t>(*placed.getNativeItemCondition()), 0x80000000u);
}

TEST_F(LooseWeaponAdmissionTest, CompoundHooksRejectIncompletePairsAndStaleResourcesBeforeLogicalPublication)
{
    struct Context
    {
        LooseWeaponAdmissionTest* fixture;
        unsigned validations = 0, publications = 0;
        bool rejectInitially = false;
        static bool validate(void* opaque) noexcept
        {
            auto& c = *static_cast<Context*>(opaque);
            ++c.validations;
            if (c.validations == 2)
            {
                EXPECT_EQ(c.fixture->mRoot->getNumChildren(), 1u);
                EXPECT_EQ(c.fixture->mPhysics->looseObjectOwners().size(), 1u);
                EXPECT_EQ(c.fixture->mCell->count(), 0u);
            }
            return !c.rejectInitially && c.validations != 2;
        }
        static void publish(void* opaque) noexcept
        { ++static_cast<Context*>(opaque)->publications; }
    } context{this};
    auto prepared = prepare(source());
    MWWorld::WorldModel::LooseWeaponPublicationHooks incomplete{&context, Context::validate, nullptr};
    EXPECT_THROW(prepared->commit(incomplete), std::invalid_argument);
    EXPECT_EQ(context.validations, 0u);
    emptyPublication();
    MWWorld::WorldModel::LooseWeaponPublicationHooks hooks{&context, Context::validate, Context::publish};
    context.rejectInitially = true;
    EXPECT_TRUE(prepared->commit(hooks).isEmpty());
    EXPECT_EQ(context.validations, 1u);
    EXPECT_EQ(context.publications, 0u);
    emptyPublication();
    context.rejectInitially = false;
    context.validations = 0;
    EXPECT_THROW(prepared->commit(hooks), std::logic_error);
    EXPECT_EQ(context.validations, 2u);
    EXPECT_EQ(context.publications, 0u);
    emptyPublication();
    EXPECT_FALSE(prepared->isValid());
}

TEST_F(LooseWeaponAdmissionTest, CompoundPublicationHookRunsOnceAfterFallibleAdmissionsBeforeCellSplice)
{
    struct Context
    {
        LooseWeaponAdmissionTest* fixture;
        unsigned validations = 0, publications = 0;
        static bool validate(void* opaque) noexcept
        { ++static_cast<Context*>(opaque)->validations; return true; }
        static void publish(void* opaque) noexcept
        {
            auto& c = *static_cast<Context*>(opaque);
            ++c.publications;
            EXPECT_EQ(c.fixture->mRoot->getNumChildren(), 1u);
            EXPECT_EQ(c.fixture->mCell->count(), 0u);
        }
    } context{this};
    MWWorld::WorldModel::LooseWeaponPublicationHooks hooks{&context, Context::validate, Context::publish};
    auto prepared = prepare(source());
    const auto published = prepared->commit(hooks);
    ASSERT_FALSE(published.isEmpty());
    EXPECT_EQ(context.validations, 2u);
    EXPECT_EQ(context.publications, 1u);
    EXPECT_EQ(mCell->count(), 1u);
    EXPECT_EQ(mWorld->getPtr(published.getCellRef().getRefNum()), published);
    EXPECT_TRUE(prepared->commit(hooks).isEmpty());
    EXPECT_EQ(context.publications, 1u);
}
