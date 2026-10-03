#include <apps/openmw/mwrender/animation.hpp>
#include <components/misc/osguservalues.hpp>
#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/nifosg/matrixtransform.hpp>
#include <components/nif/niftypes.hpp>
#include <components/sceneutil/skeleton.hpp>
#include <components/sceneutil/keyframe.hpp>

#include <gtest/gtest.h>
#include <osgUtil/UpdateVisitor>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace
{
    class PoseWriter : public osg::NodeCallback
    {
    public:
        unsigned mWrites = 0;
        bool mWriteTranslation = true;
        bool mWriteRotation = true;
        bool mInvalidTranslation = false;
        bool mThrowAfterWrite = false;
        osg::Vec3f mTranslation{1, 2, 3};
        bool mReadParent = false;
        double mObservedParentX = 0;
        double mObservedSimulationTime = 0;
        unsigned mObservedTraversal = 0;
        unsigned mObservedPathLength = 0;
        void operator()(osg::Node* node, osg::NodeVisitor* visitor) override
        {
            ++mWrites;
            if (visitor->getFrameStamp())
                mObservedSimulationTime = visitor->getFrameStamp()->getSimulationTime();
            mObservedTraversal = visitor->getTraversalNumber();
            mObservedPathLength = visitor->getNodePath().size();
            if (mReadParent)
                mObservedParentX = dynamic_cast<osg::MatrixTransform*>(node->getParent(0))->getMatrix().getTrans().x();
            auto& transform = *dynamic_cast<NifOsg::MatrixTransform*>(node);
            if (mWriteTranslation)
                transform.setTranslation(mInvalidTranslation
                    ? osg::Vec3f(std::numeric_limits<float>::quiet_NaN(), 0, 0) : mTranslation);
            if (mWriteRotation)
                transform.setRotation(osg::Quat(.4, osg::Vec3f(0, 0, 1)));
            if (mThrowAfterWrite)
                throw std::runtime_error("pose callback failed");
            traverse(node, visitor);
        }
    };

    class ResidentKeyframe : public SceneUtil::KeyframeController, public osg::NodeCallback
    {
    public:
        unsigned mWrites = 0;
        osg::Callback* getAsCallback() override { return this; }
        void operator()(osg::Node* node, osg::NodeVisitor* visitor) override
        {
            ++mWrites;
            auto& transform = *dynamic_cast<NifOsg::MatrixTransform*>(node);
            transform.setRotation(transform.mRotationScale);
            traverse(node, visitor);
        }
    };

    class SceneCounter : public osg::NodeCallback
    {
    public:
        unsigned mVisits = 0;
        void operator()(osg::Node* node, osg::NodeVisitor* visitor) override
        {
            ++mVisits;
            traverse(node, visitor);
        }
    };

    class TestAnimation : public MWRender::Animation
    {
    public:
        osg::ref_ptr<NifOsg::MatrixTransform> mBone;
        osg::ref_ptr<PoseWriter> mWriter = new PoseWriter;
        MWRender::ActiveControllersVector mBeforeControllers;
        bool mThrowAfterControllers = false;
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
        void bindRoots(std::span<const SceneUtil::ControllerSequenceMetadata> sequences)
        { bindNativeReactionRoot(sequences); }
    private:
        void addControllers() override
        {
            for (const auto& [node, callback] : mBeforeControllers)
            {
                node->addUpdateCallback(callback);
                mActiveControllers.emplace_back(node, callback);
            }
            mBone->addUpdateCallback(mWriter);
            mActiveControllers.emplace_back(mBone, mWriter);
            if (mThrowAfterControllers)
                throw std::runtime_error("controller rebuild failed");
        }
    };

    struct PhysicalPoseAnimationTest : ::testing::Test
    {
        osg::ref_ptr<osg::Group> mParent = new osg::Group;
        osg::ref_ptr<SceneUtil::Skeleton> mRoot = new SceneUtil::Skeleton;
        osg::ref_ptr<NifOsg::MatrixTransform> mBone = new NifOsg::MatrixTransform;
        NifBullet::ActorRagdollDefinition mGraph;
        std::unique_ptr<TestAnimation> mAnimation;
        osg::ref_ptr<osg::FrameStamp> mFrameStamp = new osg::FrameStamp;
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
            mVisitor.setFrameStamp(mFrameStamp);
        }
        void traverse() { mRoot->accept(mVisitor); }
    };


    TEST_F(PhysicalPoseAnimationTest, NativePlacementCarriesSeparateUniformScaleThroughOwnership)
    {
        auto placement = Nif::NiTransform::getIdentity();
        placement.mScale = 2.f;
        placement.mTranslation = {100, 20, 30};
        mBone->setTranslation({1, 2, 3});
        const auto initial = mAnimation->beginPhysicalPose(mGraph, placement);
        ASSERT_EQ(initial.size(), 1u);
        EXPECT_EQ(initial[0].mPose.getTrans(), osg::Vec3f(102, 24, 36));
        EXPECT_NEAR(initial[0].mPose(0, 0), 1.f, 1e-6);
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{{8, osg::Matrixf::translate(120, 30, 40)}};
        mAnimation->applyPhysicalPose(physical, placement);
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(10, 5, 5));
        EXPECT_EQ(mAnimation->capturePhysicalPose(placement)[0].mPose.getTrans(), osg::Vec3f(120, 30, 40));
        mAnimation->endPhysicalPose();
        EXPECT_FALSE(mAnimation->hasPhysicalPose());
    }

    TEST_F(PhysicalPoseAnimationTest, NativePlacementSamplingRestoresPhysicsAndReturnsScaledAnimationTarget)
    {
        auto placement = Nif::NiTransform::getIdentity();
        placement.mScale = .5f;
        placement.mTranslation = {100, 20, 30};
        mAnimation->beginPhysicalPose(mGraph, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{{8, osg::Matrixf::translate(120, 30, 40)}};
        mAnimation->applyPhysicalPose(physical, placement);
        const auto before = mBone->getMatrix();
        const auto writes = mAnimation->mWriter->mWrites;
        const auto target = mAnimation->samplePhysicalAnimationTarget(placement, mVisitor);
        ASSERT_EQ(target.size(), 1u);
        EXPECT_EQ(target[0].mPose.getTrans(), osg::Vec3f(100.5f, 21, 31.5f));
        EXPECT_EQ(mAnimation->mWriter->mWrites, writes + 1);
        EXPECT_EQ(std::memcmp(before.ptr(), mBone->getMatrix().ptr(), 16 * sizeof(double)), 0);
        EXPECT_EQ(mAnimation->capturePhysicalPose(placement)[0].mPose.getTrans(), osg::Vec3f(120, 30, 40));
        mAnimation->endPhysicalPose();
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(1, 2, 3));
    }

    TEST_F(PhysicalPoseAnimationTest, NativePlacementRejectsInvalidScaleBeforeControllerDetachOrCallback)
    {
        auto placement = Nif::NiTransform::getIdentity();
        for (float scale : {0.f, -1.f, std::numeric_limits<float>::infinity(),
                 std::numeric_limits<float>::quiet_NaN()})
        {
            placement.mScale = scale;
            EXPECT_THROW(mAnimation->beginPhysicalPose(mGraph, placement), std::invalid_argument);
            EXPECT_FALSE(mAnimation->hasPhysicalPose());
        }
        placement.mScale = 2.f;
        mAnimation->beginPhysicalPose(mGraph, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        const auto before = mBone->getMatrix();
        const auto writes = mAnimation->mWriter->mWrites;
        placement.mScale = -1.f;
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(placement, mVisitor), std::invalid_argument);
        EXPECT_EQ(mAnimation->mWriter->mWrites, writes);
        EXPECT_EQ(std::memcmp(before.ptr(), mBone->getMatrix().ptr(), 16 * sizeof(double)), 0);
        EXPECT_THROW(mAnimation->capturePhysicalPose(placement), std::invalid_argument);
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{{8, osg::Matrixf::identity()}};
        EXPECT_THROW(mAnimation->applyPhysicalPose(physical, placement), std::invalid_argument);
        mAnimation->endPhysicalPose();
    }

    TEST_F(PhysicalPoseAnimationTest, NativeReactionRootUsesLastExactNamedRecordIncludingHiddenNonBoneNodes)
    {
        osg::ref_ptr<osg::Group> duplicate = new osg::Group;
        duplicate->setName("Pelvis");
        duplicate->setUserValue("recordIndex", 21u);
        duplicate->setNodeMask(0);
        mRoot->addChild(duplicate);
        osg::ref_ptr<osg::Group> otherCase = new osg::Group;
        otherCase->setName("pelvis");
        otherCase->setUserValue("recordIndex", 22u);
        mRoot->addChild(otherCase);
        osg::ref_ptr<osg::Group> otherSpace = new osg::Group;
        otherSpace->setName("Pelvis ");
        otherSpace->setUserValue("recordIndex", 23u);
        mRoot->addChild(otherSpace);
        osg::ref_ptr<osg::Group> wrapper = new osg::Group;
        wrapper->setName("Pelvis"); // Renderer wrapper has no native record identity.
        mRoot->addChild(wrapper);
        SceneUtil::ControllerSequenceMetadata sequence;
        sequence.mAccumRootName = "Pelvis";
        mAnimation->bindRoots(std::span(&sequence, 1));
        ASSERT_TRUE(mAnimation->getNativeReactionRootRecord());
        EXPECT_EQ(*mAnimation->getNativeReactionRootRecord(), 21u);
        EXPECT_EQ(mBone->getName(), "Pelvis");
        EXPECT_EQ(duplicate->getNodeMask(), 0u);
    }

    TEST_F(PhysicalPoseAnimationTest, NativeReactionRootMissingFirstSequenceDoesNotUseLaterSequence)
    {
        std::array<SceneUtil::ControllerSequenceMetadata, 2> sequences;
        sequences[0].mAccumRootName = "Missing";
        sequences[1].mAccumRootName = "Pelvis";
        mAnimation->bindRoots(sequences);
        EXPECT_FALSE(mAnimation->getNativeReactionRootRecord());
        mAnimation->bindRoots(std::span(&sequences[1], 1));
        EXPECT_FALSE(mAnimation->getNativeReactionRootRecord());
    }

    TEST_F(PhysicalPoseAnimationTest, NativeReactionRootEmptySequenceSetDoesNotConsumeFirstSlot)
    {
        mAnimation->bindRoots({});
        EXPECT_FALSE(mAnimation->getNativeReactionRootRecord());
        osg::ref_ptr<osg::Group> attachedPart = new osg::Group;
        attachedPart->setName("Pelvis");
        attachedPart->setUserValue("recordIndex", 39u);
        mRoot->addChild(attachedPart); // Added after native model palette construction.
        SceneUtil::ControllerSequenceMetadata sequence;
        sequence.mAccumRootName = "Pelvis";
        mAnimation->bindRoots(std::span(&sequence, 1));
        EXPECT_EQ(mAnimation->getNativeReactionRootRecord(), 8u);
    }

    TEST_F(PhysicalPoseAnimationTest, NativeReactionRootAbsentAuthoredNameUsesModelRecordName)
    {
        mRoot->setName("RendererWrapper");
        osg::ref_ptr<osg::Group> model = new osg::Group;
        model->setName("ModelRoot");
        model->setUserValue("recordIndex", 0u);
        mRoot->removeChild(mBone);
        model->addChild(mBone);
        mRoot->addChild(model);
        SceneUtil::ControllerSequenceMetadata sequence;
        mAnimation->bindRoots(std::span(&sequence, 1));
        EXPECT_EQ(mAnimation->getNativeReactionRootRecord(), 0u);
    }

    TEST_F(PhysicalPoseAnimationTest, NativeReactionRootRetainsSelectionAndSceneRemovalReleasesIt)
    {
        SceneUtil::ControllerSequenceMetadata sequence;
        sequence.mAccumRootName = "Pelvis";
        mAnimation->bindRoots(std::span(&sequence, 1));
        EXPECT_EQ(mAnimation->getNativeReactionRootRecord(), 8u);
        mBone->setName("Changed");
        osg::ref_ptr<osg::Group> replacement = new osg::Group;
        replacement->setName("Pelvis");
        replacement->setUserValue("recordIndex", 29u);
        mRoot->addChild(replacement);
        mAnimation->bindRoots(std::span(&sequence, 1));
        EXPECT_EQ(mAnimation->getNativeReactionRootRecord(), 8u);
        mAnimation->removeFromScene();
        EXPECT_FALSE(mAnimation->getNativeReactionRootRecord());
    }

    TEST_F(PhysicalPoseAnimationTest, PhysicalOwnershipDetachesResidentAncestorKeyframesAndRestoresThem)
    {
        osg::ref_ptr<NifOsg::MatrixTransform> ancestor = new NifOsg::MatrixTransform;
        ancestor->setScale(1);
        ancestor->setRotation(osg::Quat());
        ancestor->setTranslation({0, 0, 0});
        ancestor->setUserValue("recordIndex", 17u);
        mRoot->removeChild(mBone);
        ancestor->addChild(mBone);
        mRoot->addChild(ancestor);
        osg::ref_ptr<ResidentKeyframe> resident = new ResidentKeyframe;
        osg::ref_ptr<SceneCounter> counter = new SceneCounter;
        ancestor->addUpdateCallback(resident);
        ancestor->addUpdateCallback(counter);
        const auto placement = osg::Matrixf::identity();
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{
            {8, osg::Matrixf::translate(20, 5, 6)}};
        for (auto mode : {MWRender::PhysicalPoseAnimation::Frozen, MWRender::PhysicalPoseAnimation::AnimatedTargets})
        {
            ancestor->setMatrix(osg::Matrixd::rotate(.3, osg::Vec3d(0, 0, 1)));
            const auto before = ancestor->getMatrix();
            const auto writes = resident->mWrites;
            mRoot->setNodeMask(0);
            mAnimation->beginPhysicalPose(mGraph, placement, mode);
            mRoot->setNodeMask(~0u);
            mAnimation->applyPhysicalPose(physical, placement);
            if (mode == MWRender::PhysicalPoseAnimation::AnimatedTargets)
            {
                EXPECT_EQ(mAnimation->samplePhysicalAnimationTarget(placement, mVisitor)[0].mPose.getTrans(),
                    osg::Vec3f(1, 2, 3));
                EXPECT_EQ(resident->mWrites, writes + 1);
            }
            const auto afterSample = resident->mWrites;
            traverse();
            EXPECT_EQ(resident->mWrites, afterSample);
            EXPECT_EQ(std::memcmp(before.ptr(), ancestor->getMatrix().ptr(), sizeof(double) * 16), 0);
            const auto captured = mAnimation->capturePhysicalPose(placement);
            for (unsigned r = 0; r < 4; ++r)
                for (unsigned c = 0; c < 4; ++c)
                    EXPECT_NEAR(captured[0].mPose(r, c), physical[0].mPose(r, c), 1e-4);
            mAnimation->endPhysicalPose();
            traverse();
            EXPECT_EQ(resident->mWrites, afterSample + 1);
        }
        EXPECT_EQ(counter->mVisits, 4u);
    }

    TEST_F(PhysicalPoseAnimationTest, AnimatedControllerRebuildPreservesPhysicsAndDetachesOnFailure)
    {
        traverse();
        mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity(), MWRender::PhysicalPoseAnimation::AnimatedTargets);
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{
            {8, osg::Matrixf::translate(20, 5, 6)}};
        mAnimation->applyPhysicalPose(physical, osg::Matrixf::identity());
        const auto matrix = mBone->getMatrix();
        mAnimation->rebuildControllers();
        EXPECT_EQ(std::memcmp(matrix.ptr(), mBone->getMatrix().ptr(), sizeof(double) * 16), 0);
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
        mAnimation->mThrowAfterControllers = true;
        EXPECT_THROW(mAnimation->rebuildControllers(), std::runtime_error);
        EXPECT_EQ(std::memcmp(matrix.ptr(), mBone->getMatrix().ptr(), sizeof(double) * 16), 0);
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
        mAnimation->mThrowAfterControllers = false;
        mAnimation->rebuildControllers();
        EXPECT_EQ(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), mVisitor)[0].mPose.getTrans(),
            osg::Vec3f(1, 2, 3));
    }

    TEST_F(PhysicalPoseAnimationTest, AnimatedSamplingHonorsAncestorMasksAndNodeMaskOverride)
    {
        traverse();
        mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity(), MWRender::PhysicalPoseAnimation::AnimatedTargets);
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{
            {8, osg::Matrixf::translate(20, 5, 6)}};
        mAnimation->applyPhysicalPose(physical, osg::Matrixf::identity());
        mAnimation->mWriter->mTranslation = {7, 8, 9};
        mRoot->setNodeMask(2);
        mVisitor.setTraversalMask(1);
        EXPECT_EQ(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), mVisitor)[0].mPose.getTrans(),
            osg::Vec3f(1, 2, 3));
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
        mVisitor.setNodeMaskOverride(1);
        EXPECT_EQ(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), mVisitor)[0].mPose.getTrans(),
            osg::Vec3f(7, 8, 9));
        EXPECT_EQ(mAnimation->mWriter->mWrites, 2u);
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(20, 5, 6));
    }

    TEST_F(PhysicalPoseAnimationTest, AnimatedSamplingCapturesTargetsAndPreservesPhysicalRendererAndSkinning)
    {
        traverse();
        const auto placement = osg::Matrixf::translate(100, 0, 0);
        mAnimation->beginPhysicalPose(mGraph, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{{8, osg::Matrixf::translate(120, 5, 6)}};
        mAnimation->applyPhysicalPose(physical, placement);
        const auto before = mBone->getMatrix();
        const auto cache = mBone->mRotationScale;
        mFrameStamp->setSimulationTime(.125);
        mVisitor.setTraversalNumber(91);
        const auto sampled = mAnimation->samplePhysicalAnimationTarget(placement, mVisitor);
        ASSERT_EQ(sampled.size(), 1);
        EXPECT_EQ(sampled[0].mPose.getTrans(), osg::Vec3f(101, 2, 3));
        EXPECT_EQ(mAnimation->mWriter->mWrites, 2);
        EXPECT_EQ(mAnimation->mWriter->mObservedSimulationTime, .125);
        EXPECT_EQ(mAnimation->mWriter->mObservedTraversal, 91u);
        EXPECT_EQ(mAnimation->mWriter->mObservedPathLength, 2u);
        EXPECT_EQ(std::memcmp(mBone->getMatrix().ptr(), before.ptr(), 16 * sizeof(double)), 0);
        EXPECT_EQ(std::memcmp(mBone->mRotationScale.mValues, cache.mValues, 9 * sizeof(float)), 0);
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 2);
        auto* bone = mRoot->getBone("Pelvis");
        ASSERT_NE(bone, nullptr);
        mRoot->updateBoneMatrices(0);
        EXPECT_EQ(bone->mMatrixInSkeletonSpace.getTrans(), osg::Vec3f(20, 5, 6));
        EXPECT_EQ(mAnimation->capturePhysicalPose(placement)[0].mPose.getTrans(), osg::Vec3f(120, 5, 6));
    }

    TEST_F(PhysicalPoseAnimationTest, AnimatedSamplingPreservesUndefinedChannelsFromAnimatedSource)
    {
        traverse();
        const auto placement = osg::Matrixf::translate(100, 0, 0);
        const auto animated = mBone->getMatrix();
        mAnimation->beginPhysicalPose(mGraph, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        mAnimation->applyPhysicalPose(std::vector<NifBullet::RagdollBoneWorldPose>{{8,
            osg::Matrixf::translate(120, 5, 6)}}, placement);
        mAnimation->mWriter->mWriteTranslation = false;
        mAnimation->mWriter->mWriteRotation = false;
        const auto target = mAnimation->samplePhysicalAnimationTarget(placement, mVisitor);
        EXPECT_EQ(target[0].mPose.getTrans(), osg::Vec3f(101, 2, 3));
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned column = 0; column < 3; ++column)
                EXPECT_FLOAT_EQ(target[0].mPose(row, column), float(animated(row, column)));
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(20, 5, 6));
    }

    TEST_F(PhysicalPoseAnimationTest, AnimatedSamplingDoesNotTraverseSceneChildrenAndRollsBackInvalidCallbacks)
    {
        traverse();
        osg::ref_ptr<osg::Group> child = new osg::Group;
        osg::ref_ptr<SceneCounter> counter = new SceneCounter;
        mBone->addChild(child);
        child->setUpdateCallback(counter);
        const auto placement = osg::Matrixf::translate(100, 0, 0);
        mAnimation->beginPhysicalPose(mGraph, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        mAnimation->applyPhysicalPose(std::vector<NifBullet::RagdollBoneWorldPose>{{8,
            osg::Matrixf::translate(120, 5, 6)}}, placement);
        const auto before = mBone->getMatrix();
        mAnimation->mWriter->mInvalidTranslation = true;
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(placement, mVisitor), std::invalid_argument);
        EXPECT_EQ(mBone->getMatrix(), before);
        mAnimation->mWriter->mInvalidTranslation = false;
        mAnimation->mWriter->mThrowAfterWrite = true;
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(placement, mVisitor), std::runtime_error);
        EXPECT_EQ(mBone->getMatrix(), before);
        mAnimation->mWriter->mThrowAfterWrite = false;
        EXPECT_EQ(mAnimation->samplePhysicalAnimationTarget(placement, mVisitor)[0].mPose.getTrans(),
            osg::Vec3f(101, 2, 3));
        EXPECT_EQ(mBone->getMatrix(), before);
        EXPECT_EQ(counter->mVisits, 0u);
        traverse();
        EXPECT_EQ(counter->mVisits, 1u);
    }

    TEST_F(PhysicalPoseAnimationTest, EndingAnimatedPhysicalModeRestoresLatestSampleAndReattachesControllers)
    {
        traverse();
        const auto placement = osg::Matrixf::translate(100, 0, 0);
        mAnimation->beginPhysicalPose(mGraph, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        mAnimation->applyPhysicalPose(std::vector<NifBullet::RagdollBoneWorldPose>{{8,
            osg::Matrixf::translate(120, 5, 6)}}, placement);
        mAnimation->mWriter->mTranslation = {2, 4, 6};
        mAnimation->samplePhysicalAnimationTarget(placement, mVisitor);
        mAnimation->endPhysicalPose();
        EXPECT_FALSE(mAnimation->hasPhysicalPose());
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(2, 4, 6));
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 3);
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(2, 4, 6));
    }

    TEST_F(PhysicalPoseAnimationTest, SamplingRejectsUnboundFrozenAndWrongVisitorAndInvalidMode)
    {
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), mVisitor), std::logic_error);
        EXPECT_THROW(mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity(),
            static_cast<MWRender::PhysicalPoseAnimation>(99)), std::invalid_argument);
        EXPECT_FALSE(mAnimation->hasPhysicalPose());
        mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity());
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), mVisitor), std::logic_error);
        mAnimation->endPhysicalPose();
        mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity(), MWRender::PhysicalPoseAnimation::AnimatedTargets);
        osg::NodeVisitor invalid;
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), invalid), std::invalid_argument);
        osgUtil::UpdateVisitor noStamp;
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), noStamp), std::invalid_argument);
        mFrameStamp->setSimulationTime(std::numeric_limits<double>::quiet_NaN());
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), mVisitor), std::invalid_argument);
        mFrameStamp->setSimulationTime(0);
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::scale(2, 2, 2), mVisitor), std::invalid_argument);
        EXPECT_EQ(mAnimation->mWriter->mWrites, 0);
    }

    TEST_F(PhysicalPoseAnimationTest, AnimatedSamplingExecutesParentBeforeChildDespiteRegistrationOrder)
    {
        osg::ref_ptr<NifOsg::MatrixTransform> child = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        child->setName("Child");
        child->setUserValue("recordIndex", 18u);
        mBone->addChild(child);
        osg::ref_ptr<PoseWriter> reader = new PoseWriter;
        reader->mReadParent = true;
        mAnimation->mBeforeControllers.emplace_back(child, reader);
        mAnimation->rebuildControllers();
        traverse();
        const auto placement = osg::Matrixf::translate(100, 0, 0);
        mAnimation->beginPhysicalPose(mGraph, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        mAnimation->applyPhysicalPose(std::vector<NifBullet::RagdollBoneWorldPose>{{8,
            osg::Matrixf::translate(120, 5, 6)}}, placement);
        mAnimation->mWriter->mTranslation = {3, 4, 5};
        mAnimation->samplePhysicalAnimationTarget(placement, mVisitor);
        EXPECT_EQ(reader->mObservedParentX, 3);
        EXPECT_EQ(reader->mWrites, 2u);
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(20, 5, 6));
        traverse();
        EXPECT_EQ(reader->mWrites, 2u);
    }

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

namespace
{
    TEST_F(PhysicalPoseAnimationTest, EndPhysicalPoseReleasesOwnershipWhenHierarchyChanged)
    {
        mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity(), MWRender::PhysicalPoseAnimation::AnimatedTargets);
        mRoot->removeChild(mBone);
        EXPECT_THROW(mAnimation->endPhysicalPose(), std::invalid_argument);
        EXPECT_FALSE(mAnimation->hasPhysicalPose());
        mRoot->addChild(mBone);
        EXPECT_NO_THROW(mAnimation->endPhysicalPose());
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
    }
}
