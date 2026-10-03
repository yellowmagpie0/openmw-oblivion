#include <apps/openmw/mwrender/animation.hpp>
#include <components/misc/osguservalues.hpp>
#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/nifosg/matrixtransform.hpp>
#include <components/sceneutil/skeleton.hpp>

#include <gtest/gtest.h>
#include <osgUtil/UpdateVisitor>

namespace
{
    class PoseWriter : public osg::NodeCallback
    {
    public:
        unsigned mWrites = 0;
        void operator()(osg::Node* node, osg::NodeVisitor* visitor) override
        {
            ++mWrites;
            dynamic_cast<NifOsg::MatrixTransform*>(node)->setTranslation(osg::Vec3f(1, 2, 3));
            dynamic_cast<NifOsg::MatrixTransform*>(node)->setRotation(osg::Quat(.4, osg::Vec3f(0, 0, 1)));
            traverse(node, visitor);
        }
    };

    class TestAnimation : public MWRender::Animation
    {
    public:
        osg::ref_ptr<NifOsg::MatrixTransform> mBone;
        osg::ref_ptr<PoseWriter> mWriter = new PoseWriter;
        TestAnimation(osg::ref_ptr<osg::Group> parent, osg::ref_ptr<SceneUtil::Skeleton> root,
            osg::ref_ptr<NifOsg::MatrixTransform> bone)
            : Animation({}, parent, nullptr), mBone(std::move(bone))
        {
            mObjectRoot = root;
            mSkeleton = root;
            parent->addChild(root);
            resetActiveGroups();
        }
        void rebuildControllers() { resetActiveGroups(); }
    private:
        void addControllers() override
        {
            mBone->addUpdateCallback(mWriter);
            mActiveControllers.emplace_back(mBone, mWriter);
        }
    };

    struct PhysicalPoseAnimationTest : ::testing::Test
    {
        osg::ref_ptr<osg::Group> mParent = new osg::Group;
        osg::ref_ptr<SceneUtil::Skeleton> mRoot = new SceneUtil::Skeleton;
        osg::ref_ptr<NifOsg::MatrixTransform> mBone = new NifOsg::MatrixTransform;
        NifBullet::ActorRagdollDefinition mGraph;
        std::unique_ptr<TestAnimation> mAnimation;
        osgUtil::UpdateVisitor mVisitor;
        PhysicalPoseAnimationTest()
        {
            mRoot->setUserValue(Misc::OsgUserValues::sFileHash, std::string("physical-pose-test"));
            mBone->setName("Pelvis");
            mBone->setRotation(osg::Quat());
            mBone->setScale(1);
            mBone->setTranslation(osg::Vec3f());
            mBone->setUserValue("recordIndex", 8u);
            mRoot->addChild(mBone);
            mGraph.mSourceHash = "physical-pose-test";
            NifBullet::RagdollBodyDefinition body{};
            body.mRecord = 12;
            body.mNodeRecord = 8;
            body.mBone = "Pelvis";
            mGraph.mBodies.push_back(body);
            mAnimation = std::make_unique<TestAnimation>(mParent, mRoot, mBone);
        }
        void traverse() { mRoot->accept(mVisitor); }
    };

    TEST_F(PhysicalPoseAnimationTest, PhysicalSnapshotOwnsBonesAcrossAnimationAndSceneTraversal)
    {
        traverse();
        ASSERT_EQ(mAnimation->mWriter->mWrites, 1);
        const auto placement = osg::Matrixf::translate(100, 0, 0);
        const auto initial = mAnimation->beginPhysicalPose(mGraph, placement);
        ASSERT_EQ(initial.size(), 1);
        EXPECT_EQ(initial[0].mPose.getTrans(), osg::Vec3f(101, 2, 3));
        EXPECT_TRUE(mAnimation->hasPhysicalPose());
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{{8, osg::Matrixf::translate(120, 5, 6)}};
        mAnimation->applyPhysicalPose(physical, placement);
        mAnimation->rebuildControllers();
        EXPECT_EQ(mAnimation->runAnimation(.25f), osg::Vec3f());
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1);
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3f(20, 5, 6));
        EXPECT_EQ(mAnimation->capturePhysicalPose(placement)[0].mPose.getTrans(), physical[0].mPose.getTrans());
        mAnimation->endPhysicalPose();
        EXPECT_FALSE(mAnimation->hasPhysicalPose());
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 2);
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3f(1, 2, 3));
        mAnimation->endPhysicalPose();
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 3);
    }

    TEST_F(PhysicalPoseAnimationTest, FailedAdmissionLeavesAnimationControllerAttached)
    {
        auto invalid = mGraph;
        invalid.mSourceHash = "different-asset";
        EXPECT_THROW(mAnimation->beginPhysicalPose(invalid, osg::Matrixf::identity()), std::invalid_argument);
        EXPECT_FALSE(mAnimation->hasPhysicalPose());
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1);
        EXPECT_THROW(mAnimation->capturePhysicalPose(osg::Matrixf::identity()), std::logic_error);
        const std::vector<NifBullet::RagdollBoneWorldPose> empty;
        EXPECT_THROW(mAnimation->applyPhysicalPose(empty, osg::Matrixf::identity()), std::logic_error);
    }

    TEST_F(PhysicalPoseAnimationTest, InvalidSnapshotAndDuplicateAdmissionPreservePhysicalOwnership)
    {
        mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity());
        EXPECT_THROW(mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity()), std::logic_error);
        const std::vector<NifBullet::RagdollBoneWorldPose> empty;
        EXPECT_THROW(mAnimation->applyPhysicalPose(empty, osg::Matrixf::identity()), std::invalid_argument);
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 0);
        EXPECT_TRUE(mAnimation->hasPhysicalPose());
        mAnimation->endPhysicalPose();
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1);
    }

    TEST_F(PhysicalPoseAnimationTest, SceneRemovalReleasesBindingWithoutReattachingControllers)
    {
        mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity());
        mAnimation->removeFromScene();
        EXPECT_FALSE(mAnimation->hasPhysicalPose());
        EXPECT_EQ(mParent->getNumChildren(), 0);
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 0);
    }
}
