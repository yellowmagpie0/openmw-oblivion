#include <apps/openmw/mwworld/cellstore.hpp>
#include <apps/openmw/mwworld/esmstore.hpp>
#include <components/esm3/readerscache.hpp>
#include <components/esm3/loadcell.hpp>
#include <apps/openmw/mwrender/objects.hpp>
#include <apps/openmw/mwrender/vismask.hpp>
#include <apps/openmw/mwclass/static.hpp>
#include <apps/openmw/mwworld/livecellref.hpp>
#include <components/esm3/loadstat.hpp>
#include <components/sceneutil/unrefqueue.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>
#include <components/testing/util.hpp>
#include <osgDB/Registry>
#include <osgDB/ReaderWriter>
#include <sstream>
#include <limits>
#include <new>
#include <cstddef>
#include <apps/openmw/mwrender/animation.hpp>
#include <apps/openmw/mwrender/util.hpp>
#include <components/nif/niftypes.hpp>
#include <components/nifosg/matrixtransform.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/vfs/manager.hpp>

#include <gtest/gtest.h>
#include <osg/Group>

namespace
{
    class PreparedModelFailureRoot : public osg::Group
    {
    public:
        enum class Failure { None, Reject, InsertReject, InsertThrow };
        Failure mFailure = Failure::None;
        bool addChild(osg::Node* child) override
        {
            if (mFailure == Failure::Reject) return false;
            const bool result = osg::Group::addChild(child);
            if (mFailure == Failure::InsertReject) return false;
            if (mFailure == Failure::InsertThrow) throw std::runtime_error("scene insertion failed after parenting");
            return result;
        }
    };

    struct PreparedObjectModelTest : testing::Test
    {
        static std::string modelBytes()
        {
            osg::ref_ptr<osg::Group> model = new osg::Group;
            model->setName("Prepared actual model");
            std::ostringstream out;
            auto* writer = osgDB::Registry::instance()->getReaderWriterForExtension("osgt");
            if (!writer || !writer->writeNode(*model, out).success())
                throw std::runtime_error("could not write test-owned model");
            return out.str();
        }
        TestingOpenMW::VFSTestFile mFile{modelBytes()};
        std::unique_ptr<VFS::Manager> mVfs = TestingOpenMW::createTestVFS({
            {VFS::Path::NormalizedView("prepared.osgt"), &mFile}});
        Resource::ResourceSystem mResources{mVfs.get(), 0., nullptr};
        SceneUtil::UnrefQueue mUnref;
        osg::ref_ptr<PreparedModelFailureRoot> mRoot = new PreparedModelFailureRoot;
        static ESM::Cell modelCell()
        {
            ESM::Cell cell;
            cell.blank();
            cell.mId = ESM::RefId::stringRefId("PreparedObjectModelCell");
            cell.mData.mFlags = ESM::Cell::Interior;
            return cell;
        }
        MWWorld::ESMStore mStore;
        ESM::ReadersCache mReaders;
        // CellVariant borrows its source record; keep that record alive for
        // the complete CellStore lifetime instead of wrapping a local copy.
        ESM::Cell mCellRecord = modelCell();
        MWWorld::CellStore mCell{MWWorld::Cell(mCellRecord), mStore, mReaders};
        ESM::Static mBase;
        ESM::CellRef mReference;
        std::unique_ptr<MWWorld::LiveCellRef<ESM::Static>> mLive;
        MWWorld::Ptr mPtr;
        PreparedObjectModelTest()
        {
            MWClass::Static::registerSelf();
            mBase.blank();
            mReference.blank();
            mLive = std::make_unique<MWWorld::LiveCellRef<ESM::Static>>(mReference, &mBase);
            mCell.load();
            mPtr = MWWorld::Ptr(mLive.get(), &mCell);
        }
        static constexpr unsigned Mask = MWRender::Mask_Object;
        std::unique_ptr<MWRender::Objects::PreparedModel> prepare(MWRender::Objects& objects,
            const MWWorld::Ptr& ptr)
        { return objects.prepareModel(ptr, "prepared.osgt", osg::Quat(), Mask); }
    };

    TEST_F(PreparedObjectModelTest, CancellationBuildsPrivateActualModelWithoutReferenceOrScenePublication)
    {
        MWRender::Objects objects(&mResources, mRoot, mUnref);
        auto original = mResources.getSceneManager()->getTemplate(VFS::Path::NormalizedView("prepared.osgt"));
        const auto originalParents = original->getNumParents();
        {
            auto prepared = prepare(objects, mPtr);
            EXPECT_TRUE(objects.validatePreparedModel(*prepared));
            EXPECT_EQ(mPtr.getRefData().getBaseNode(), nullptr);
            EXPECT_EQ(objects.getAnimation(mPtr), nullptr);
            EXPECT_EQ(mRoot->getNumChildren(), 0u);
            EXPECT_EQ(mUnref.getSize(), 0u);
            EXPECT_EQ(mPtr.getCellRef().getCount(), 1);
        }
        EXPECT_EQ(original->getNumParents(), originalParents);
        EXPECT_EQ(mPtr.getRefData().getBaseNode(), nullptr);
        EXPECT_EQ(mRoot->getNumChildren(), 0u);
        EXPECT_THROW(objects.prepareModel(mPtr, "missing.nif", osg::Quat(), Mask), std::runtime_error);
        EXPECT_EQ(objects.getAnimation(mPtr), nullptr);
        EXPECT_EQ(mRoot->getNumChildren(), 0u);
    }

    TEST_F(PreparedObjectModelTest, PlacementCountScaleAndOwnerChangesRejectBeforeScenePublication)
    {
        MWRender::Objects objects(&mResources, mRoot, mUnref);
        osg::ref_ptr<osg::Group> foreignRoot = new osg::Group;
        MWRender::Objects foreign(&mResources, foreignRoot, mUnref);
        auto prepared = prepare(objects, mPtr);
        EXPECT_FALSE(foreign.commitModel(*prepared));
        mPtr.getCellRef().setCount(2);
        EXPECT_FALSE(objects.commitModel(*prepared));
        mPtr.getCellRef().setCount(1);
        const auto pos = mPtr.getRefData().getPosition();
        auto changed = pos;
        changed.pos[0] = 1;
        mPtr.getRefData().setPosition(changed);
        EXPECT_FALSE(objects.commitModel(*prepared));
        mPtr.getRefData().setPosition(pos);
        mPtr.getCellRef().setScale(2);
        EXPECT_FALSE(objects.commitModel(*prepared));
        mPtr.getCellRef().setScale(1);
        EXPECT_TRUE(objects.validatePreparedModel(*prepared));
        EXPECT_EQ(mRoot->getNumChildren(), 0u);
        changed.pos[0] = std::numeric_limits<float>::quiet_NaN();
        mPtr.getRefData().setPosition(changed);
        EXPECT_THROW(prepare(objects, mPtr), std::invalid_argument);
        mPtr.getRefData().setPosition(pos);
        osg::Quat bad;
        bad.x() = std::numeric_limits<double>::quiet_NaN();
        EXPECT_THROW(objects.prepareModel(mPtr, "", bad, Mask), std::invalid_argument);
        EXPECT_THROW(objects.prepareModel({}, "", osg::Quat(), Mask), std::invalid_argument);
        const MWWorld::Ptr orphan(mLive.get());
        EXPECT_THROW(objects.prepareModel(orphan, "", osg::Quat(), Mask), std::invalid_argument);
        EXPECT_EQ(mPtr.getRefData().getBaseNode(), nullptr);
        EXPECT_EQ(objects.getAnimation(mPtr), nullptr);
        EXPECT_EQ(mRoot->getNumChildren(), 0u);
    }

    TEST_F(PreparedObjectModelTest, SceneRejectionAndPostInsertionFailureRollBackBeforeRetry)
    {
        MWRender::Objects objects(&mResources, mRoot, mUnref);
        auto prepared = prepare(objects, mPtr);
        for (auto failure : {PreparedModelFailureRoot::Failure::Reject,
                 PreparedModelFailureRoot::Failure::InsertReject, PreparedModelFailureRoot::Failure::InsertThrow})
        {
            mRoot->mFailure = failure;
            EXPECT_THROW(objects.commitModel(*prepared), std::runtime_error);
            EXPECT_TRUE(objects.validatePreparedModel(*prepared));
            EXPECT_EQ(mRoot->getNumChildren(), 0u);
            EXPECT_EQ(mPtr.getRefData().getBaseNode(), nullptr);
            EXPECT_EQ(objects.getAnimation(mPtr), nullptr);
            EXPECT_EQ(mUnref.getSize(), 0u);
            EXPECT_EQ(mPtr.getCellRef().getCount(), 1);
        }
        mRoot->mFailure = PreparedModelFailureRoot::Failure::None;
        ASSERT_TRUE(objects.commitModel(*prepared));
        EXPECT_FALSE(objects.commitModel(*prepared));
        auto* node = mPtr.getRefData().getBaseNode();
        ASSERT_NE(node, nullptr);
        EXPECT_EQ(node->getNodeMask(), Mask);
        EXPECT_EQ(node->getNumParents(), 1u);
        ASSERT_NE(objects.getAnimation(mPtr), nullptr);
        EXPECT_EQ(objects.getAnimation(mPtr)->getObjectRoot()->getName(), "Prepared actual model");
        EXPECT_EQ(mRoot->getNumChildren(), 1u);
        ASSERT_TRUE(objects.removeObject(mPtr));
        EXPECT_EQ(mPtr.getRefData().getBaseNode(), nullptr);
        EXPECT_EQ(objects.getAnimation(mPtr), nullptr);
        EXPECT_EQ(node->getNumParents(), 0u);
        EXPECT_EQ(mUnref.getSize(), 1u);
    }

    TEST_F(PreparedObjectModelTest, SynchronousRollbackRemovesOnlyItsPublicationWithoutUnrefAllocation)
    {
        MWRender::Objects objects(&mResources, mRoot, mUnref);
        auto prepared = prepare(objects, mPtr);
        ASSERT_TRUE(objects.commitModel(*prepared));
        ASSERT_TRUE(objects.rollbackModelAdmission(*prepared));
        EXPECT_EQ(mRoot->getNumChildren(), 0u);
        EXPECT_EQ(mPtr.getRefData().getBaseNode(), nullptr);
        EXPECT_EQ(objects.getAnimation(mPtr), nullptr);
        EXPECT_EQ(mUnref.getSize(), 0u);
        EXPECT_EQ(mPtr.getCellRef().getCount(), 1);
        EXPECT_FALSE(objects.rollbackModelAdmission(*prepared));
        EXPECT_FALSE(objects.commitModel(*prepared));
        auto fresh = prepare(objects, mPtr);
        EXPECT_TRUE(objects.commitModel(*fresh));
        fresh.reset(); // Successful publication is owned by Objects.
        EXPECT_NE(objects.getAnimation(mPtr), nullptr);
        EXPECT_NE(mPtr.getRefData().getBaseNode(), nullptr);
    }

    TEST_F(PreparedObjectModelTest, RollbackPreservesExistingCellAndRejectsAReplacementModel)
    {
        MWRender::Objects objects(&mResources, mRoot, mUnref);
        MWWorld::LiveCellRef<ESM::Static> other(mReference, &mBase);
        const MWWorld::Ptr otherPtr(&other, &mCell);
        auto first = prepare(objects, otherPtr);
        ASSERT_TRUE(objects.commitModel(*first));
        auto* otherNode = otherPtr.getRefData().getBaseNode();
        auto second = prepare(objects, mPtr);
        ASSERT_TRUE(objects.commitModel(*second));
        EXPECT_EQ(mRoot->getChild(0)->asGroup()->getNumChildren(), 2u);
        ASSERT_TRUE(objects.rollbackModelAdmission(*second));
        EXPECT_EQ(mRoot->getNumChildren(), 1u);
        EXPECT_EQ(mRoot->getChild(0)->asGroup()->getNumChildren(), 1u);
        EXPECT_EQ(otherPtr.getRefData().getBaseNode(), otherNode);
        auto old = prepare(objects, mPtr);
        ASSERT_TRUE(objects.commitModel(*old));
        ASSERT_TRUE(objects.removeObject(mPtr));
        auto replacement = prepare(objects, mPtr);
        ASSERT_TRUE(objects.commitModel(*replacement));
        auto* replacementNode = mPtr.getRefData().getBaseNode();
        EXPECT_FALSE(objects.rollbackModelAdmission(*old));
        EXPECT_EQ(mPtr.getRefData().getBaseNode(), replacementNode);
        EXPECT_NE(objects.getAnimation(mPtr), nullptr);
        objects.removeObject(mPtr);
        objects.removeObject(otherPtr);
    }

    TEST_F(PreparedObjectModelTest, ReusedObjectsOwnerRejectsStaleTokenBeforeAccessingDeletedReference)
    {
        using Objects = MWRender::Objects;
        alignas(Objects) std::byte storage[sizeof(Objects)];
        auto* objects = new (storage) Objects(&mResources, mRoot, mUnref);
        auto retired = prepare(*objects, mPtr);
        objects->~Objects();
        mLive.reset();
        objects = new (storage) Objects(&mResources, mRoot, mUnref);
        EXPECT_FALSE(objects->validatePreparedModel(*retired));
        EXPECT_FALSE(objects->commitModel(*retired));
        EXPECT_FALSE(objects->rollbackModelAdmission(*retired));
        retired.reset();
        EXPECT_EQ(mRoot->getNumChildren(), 0u);
        objects->~Objects();
    }
}
