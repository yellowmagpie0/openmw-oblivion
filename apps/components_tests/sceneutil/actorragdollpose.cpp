#include <components/sceneutil/actorragdollpose.hpp>
#include <components/sceneutil/skeleton.hpp>
#include <components/nifosg/matrixtransform.hpp>
#include <components/nifbullet/actorragdoll.hpp>
#include <components/misc/osguservalues.hpp>

#include <gtest/gtest.h>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>

#include <osg/MatrixTransform>
#include <limits>
#include <stdexcept>

namespace
{
    struct ActorRagdollPoseTest : ::testing::Test
    {
        osg::ref_ptr<SceneUtil::Skeleton> mRoot = new SceneUtil::Skeleton;
        osg::ref_ptr<NifOsg::MatrixTransform> mPelvis
            = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        osg::ref_ptr<NifOsg::MatrixTransform> mConnector
            = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        osg::ref_ptr<NifOsg::MatrixTransform> mHand
            = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        NifBullet::ActorRagdollDefinition mGraph;
        const osg::Matrixf mObjectWorld = osg::Matrixf::translate(100, 0, 0);
        ActorRagdollPoseTest()
        {
            mRoot->setUserValue(Misc::OsgUserValues::sFileHash, std::string("synthetic-ragdoll-skeleton"));
            mGraph.mSourceHash = "synthetic-ragdoll-skeleton";
            mRoot->addChild(mPelvis);
            mPelvis->addChild(mConnector);
            mConnector->addChild(mHand);
            mPelvis->setName("Pelvis");
            mPelvis->setUserValue("recordIndex", 8u);
            mConnector->setName("Connector");
            mConnector->setUserValue("recordIndex", 18u);
            mHand->setName("Hand");
            mHand->setUserValue("recordIndex", 28u);
            mPelvis->setTranslation({10, 0, 0});
            mConnector->setTranslation({0, 2, 0});
            mHand->setTranslation({1, 0, 0});
            NifBullet::RagdollBodyDefinition hand{};
            hand.mRecord = 30;
            hand.mNodeRecord = 28;
            hand.mBone = "Hand";
            hand.mBoneBind = osg::Matrixf::translate(999, 999, 999);
            auto pelvis = hand;
            pelvis.mRecord = 12;
            pelvis.mNodeRecord = 8;
            pelvis.mBone = "Pelvis";
            // Child-first graph order must not become renderer mutation order.
            mGraph.mBodies = {hand, pelvis};
        }
        std::vector<NifBullet::RagdollBoneWorldPose> desired() const
        {
            return {{28, osg::Matrixf::translate(120, 5, 0)}, {8, osg::Matrixf::translate(120, 0, 0)}};
        }
    };

    TEST_F(ActorRagdollPoseTest, CapturesLiveBonePlacementInsteadOfAuthoredBindPose)
    {
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        const auto poses = binding.captureWorldBones(mObjectWorld);
        ASSERT_EQ(poses.size(), 2);
        EXPECT_EQ(poses[0].mNodeRecord, 28);
        EXPECT_EQ(poses[0].mPose.getTrans(), osg::Vec3f(111, 2, 0));
        EXPECT_EQ(poses[1].mNodeRecord, 8);
        EXPECT_EQ(poses[1].mPose.getTrans(), osg::Vec3f(110, 0, 0));
        mHand->setTranslation({3, 4, 5});
        EXPECT_EQ(binding.captureWorldBones(mObjectWorld)[0].mPose.getTrans(), osg::Vec3f(113, 6, 5));
    }

    TEST_F(ActorRagdollPoseTest, AppliesChildFirstPhysicalSnapshotThroughUnsimulatedConnector)
    {
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        auto* hand = mRoot->getBone("Hand");
        ASSERT_NE(hand, nullptr);
        mRoot->updateBoneMatrices(0);
        EXPECT_EQ(hand->mMatrixInSkeletonSpace.getTrans(), osg::Vec3f(11, 2, 0));
        binding.applyWorldBones(desired(), mObjectWorld);
        EXPECT_EQ(mPelvis->getMatrix().getTrans(), osg::Vec3f(20, 0, 0));
        EXPECT_EQ(mConnector->getMatrix().getTrans(), osg::Vec3f(0, 2, 0));
        EXPECT_EQ(mHand->getMatrix().getTrans(), osg::Vec3f(0, 3, 0));
        // Pose changes must invalidate skinning even within the same traversal.
        mRoot->updateBoneMatrices(0);
        EXPECT_EQ(hand->mMatrixInSkeletonSpace.getTrans(), osg::Vec3f(20, 5, 0));
        const auto captured = binding.captureWorldBones(mObjectWorld);
        EXPECT_EQ(captured[0].mPose.getTrans(), osg::Vec3f(120, 5, 0));
        EXPECT_EQ(captured[1].mPose.getTrans(), osg::Vec3f(120, 0, 0));
    }

    TEST_F(ActorRagdollPoseTest, RejectsIncompleteDuplicateAndMalformedSnapshotsWithoutPartialWrites)
    {
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        const auto initialPelvis = mPelvis->getMatrix();
        const auto initialHand = mHand->getMatrix();
        auto poses = desired();
        poses.pop_back();
        EXPECT_THROW(binding.applyWorldBones(poses, mObjectWorld), std::invalid_argument);
        poses = desired();
        poses[1].mNodeRecord = 28;
        EXPECT_THROW(binding.applyWorldBones(poses, mObjectWorld), std::invalid_argument);
        poses = desired();
        poses[0].mPose(0, 0) = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(binding.applyWorldBones(poses, mObjectWorld), std::invalid_argument);
        EXPECT_EQ(mPelvis->getMatrix(), initialPelvis);
        EXPECT_EQ(mHand->getMatrix(), initialHand);
    }

    TEST_F(ActorRagdollPoseTest, RejectsWrongAssetAndMismatchedNodeIdentity)
    {
        mGraph.mSourceHash = "different-skeleton";
        EXPECT_THROW((SceneUtil::ActorRagdollPoseBinding(mGraph, *mRoot)), std::invalid_argument);
        mGraph.mSourceHash = "synthetic-ragdoll-skeleton";
        mGraph.mBodies[0].mNodeRecord = 18;
        EXPECT_THROW((SceneUtil::ActorRagdollPoseBinding(mGraph, *mRoot)), std::invalid_argument);
    }

    TEST_F(ActorRagdollPoseTest, RejectsExpiredOrDetachedBonesAndUnadmittedTransforms)
    {
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        mConnector->removeChild(mHand);
        EXPECT_THROW(binding.captureWorldBones(mObjectWorld), std::invalid_argument);
        EXPECT_THROW(binding.applyWorldBones(desired(), mObjectWorld), std::invalid_argument);
        mConnector->addChild(mHand);
        mHand->setScale(2);
        EXPECT_THROW(binding.captureWorldBones(mObjectWorld), std::invalid_argument);
        mHand->setScale(1);
        mGraph.mBodies[0].mUsesRigidBodyTransform = true;
        EXPECT_THROW((SceneUtil::ActorRagdollPoseBinding(mGraph, *mRoot)), std::invalid_argument);
        mRoot = nullptr;
        EXPECT_THROW(binding.captureWorldBones(mObjectWorld), std::invalid_argument);
    }
    TEST_F(ActorRagdollPoseTest, HandlesSkeletonWrapperAndSeparateEquipmentRecordNamespace)
    {
        osg::ref_ptr<SceneUtil::Skeleton> wrapper = new SceneUtil::Skeleton;
        wrapper->addChild(mRoot);
        osg::ref_ptr<osg::Group> equipment = new osg::Group;
        equipment->setUserValue(Misc::OsgUserValues::sFileHash, std::string("other-asset"));
        osg::ref_ptr<NifOsg::MatrixTransform> unrelated
            = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        unrelated->setName("Hand");
        unrelated->setUserValue("recordIndex", 28u);
        unrelated->setTranslation({99, 99, 99});
        equipment->addChild(unrelated);
        mHand->addChild(equipment);
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *wrapper);
        binding.applyWorldBones(desired(), mObjectWorld);
        EXPECT_EQ(binding.captureWorldBones(mObjectWorld)[0].mPose.getTrans(), osg::Vec3f(120, 5, 0));
        EXPECT_EQ(unrelated->getMatrix().getTrans(), osg::Vec3f(99, 99, 99));
    }

    TEST_F(ActorRagdollPoseTest, PreservesUnsimulatedScaledHelpersOutsidePhysicalAncestorPaths)
    {
        osg::ref_ptr<NifOsg::MatrixTransform> helper
            = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        helper->setName("Unsimulated Helper");
        helper->setUserValue("recordIndex", 99u);
        helper->setScale(.1f);
        mPelvis->addChild(helper);
        const auto previous = helper->getMatrix();
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        binding.applyWorldBones(desired(), mObjectWorld);
        EXPECT_EQ(helper->getMatrix(), previous);
        EXPECT_FLOAT_EQ(helper->mScale, .1f);
    }

    TEST_F(ActorRagdollPoseTest, RetainsStockNearUnitSeparateBoneScale)
    {
        const float scale = .9999999403953552f; // Stock SideWeapon NiTransform.
        mHand->setScale(scale);
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        const auto captured = binding.captureWorldBones(mObjectWorld);
        EXPECT_EQ(captured[0].mPose.getTrans(), osg::Vec3f(111, 2, 0));
        binding.applyWorldBones(desired(), mObjectWorld);
        EXPECT_FLOAT_EQ(mHand->mScale, scale);
        EXPECT_EQ(binding.captureWorldBones(mObjectWorld)[0].mPose.getTrans(), osg::Vec3f(120, 5, 0));
    }

    TEST_F(ActorRagdollPoseTest, RotatedPhysicalParentPreservesUnsimulatedLocalConnector)
    {
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        auto poses = desired();
        // Parent turns ninety degrees; connector local +Y becomes world -X.
        poses[1].mPose = osg::Matrixf::rotate(osg::PI_2, osg::Vec3f(0, 0, 1))
            * osg::Matrixf::translate(120, 0, 0);
        binding.applyWorldBones(poses, mObjectWorld);
        const auto captured = binding.captureWorldBones(mObjectWorld);
        for (unsigned i = 0; i < 2; ++i)
            for (unsigned row = 0; row < 4; ++row)
                for (unsigned column = 0; column < 4; ++column)
                    EXPECT_NEAR(captured[i].mPose(row, column), poses[i].mPose(row, column), 1e-4);
        EXPECT_EQ(mConnector->getMatrix().getTrans(), osg::Vec3f(0, 2, 0));
    }

    TEST_F(ActorRagdollPoseTest, PhysicalBodyMotionDrivesSkeletonMatrices)
    {
        for (auto& body : mGraph.mBodies)
        {
            body.mMass = 2;
            body.mInertia = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            body.mShape = NifBullet::RagdollSphere{.1f};
        }
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        const auto bones = binding.captureWorldBones(mObjectWorld);
        const auto poses = NifBullet::ragdollBodyWorldPoses(mGraph, bones);
        btDefaultCollisionConfiguration configuration;
        btCollisionDispatcher dispatcher(&configuration);
        btDbvtBroadphase broadphase;
        btSequentialImpulseConstraintSolver solver;
        btDiscreteDynamicsWorld world(&dispatcher, &broadphase, &solver, &configuration);
        world.setGravity(btVector3(0, 0, 0));
        NifBullet::ActorRagdollPhysics physics(mGraph, world, NifBullet::RagdollNativeLengthScale, poses, 1, -1);
        physics.applyImpulse(0, btVector3(2, 0, 0), poses[0].getOrigin());
        physics.applyNativeDamping(.1f);
        world.stepSimulation(.1f, 0);
        const auto state = physics.capture();
        ASSERT_EQ(state.size(), 2);
        std::vector<NifBullet::RagdollBoneWorldPose> moved;
        for (unsigned i = 0; i < 2; ++i)
        {
            osg::Matrixf matrix;
            for (unsigned row = 0; row < 3; ++row)
            {
                for (unsigned column = 0; column < 3; ++column)
                    matrix(column, row) = float(state[i].mPose.getBasis()[row][column]);
                matrix(3, row) = float(state[i].mPose.getOrigin()[row]);
            }
            moved.push_back({mGraph.mBodies[i].mNodeRecord, matrix});
        }
        auto* hand = mRoot->getBone("Hand");
        mRoot->updateBoneMatrices(0);
        const auto previous = hand->mMatrixInSkeletonSpace.getTrans();
        binding.applyWorldBones(moved, mObjectWorld);
        mRoot->updateBoneMatrices(0);
        EXPECT_GT(hand->mMatrixInSkeletonSpace.getTrans().x() - previous.x(), .09);
        EXPECT_NEAR(hand->mMatrixInSkeletonSpace.getTrans().x(), 11.1, 1e-4);
        EXPECT_NEAR(binding.captureWorldBones(mObjectWorld)[0].mPose.getTrans().x(),
            float(state[0].mPose.getOrigin().x()), 1e-4);
    }

}
