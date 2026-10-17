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
#include <cstring>
#include <array>
#include <bit>
#include <cstdint>

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

    TEST_F(ActorRagdollPoseTest, BodyTGraphAndControllerPublishScenePoseWithoutApplyingOffsetTwice)
    {
        mPelvis->setTranslation({0, 0, 0});
        mConnector->setTranslation({0, 0, 0});
        mHand->setTranslation({0, 0, 0});
        for (auto& body : mGraph.mBodies)
        {
            body.mMass = 2;
            body.mInertia = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            body.mShape = NifBullet::RagdollSphere{.5f};
        }
        auto& hand = mGraph.mBodies[0];
        hand.mUsesRigidBodyTransform = true;
        hand.mTranslation = {2, 3, 4};
        hand.mRotation = osg::Quat(0, 0, 1, 0);
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        auto scene = binding.captureWorldBones(osg::Matrixf::identity());
        const auto physical = NifBullet::ragdollBodyWorldPoses(mGraph, scene);
        const btScalar scale = NifBullet::RagdollNativeLengthScale;
        EXPECT_EQ(physical[0].getOrigin(), btVector3(2 * scale, 3 * scale, 4 * scale));
        btDefaultCollisionConfiguration configuration;
        btCollisionDispatcher dispatcher(&configuration);
        btDbvtBroadphase broadphase;
        btSequentialImpulseConstraintSolver solver;
        btDiscreteDynamicsWorld world(&dispatcher, &broadphase, &solver, &configuration);
        NifBullet::ActorRagdollPhysics actor(mGraph, world, NifBullet::RagdollNativeLengthScale, physical, 1, -1);
        std::array<NifBullet::RagdollNativeBlendUpdate, 1> updates{{
            {30, osg::Matrixf::identity(), 1, 0, 8}}};
        actor.updateNativeBlends(updates, 1, 0, 0);
        updates[0].mHierarchyGain = 0;
        const auto publication = actor.updateNativeBlends(updates, 1, 0, 0);
        ASSERT_EQ(publication.size(), 1);
        ASSERT_TRUE(publication[0].mSceneTarget);
        EXPECT_EQ(*publication[0].mSceneTarget, osg::Matrixf::identity());
        scene[0].mPose = *publication[0].mSceneTarget;
        binding.applyWorldBones(scene, osg::Matrixf::identity());
        EXPECT_EQ(mHand->getMatrix().getTrans(), osg::Vec3d(0, 0, 0));
        EXPECT_EQ(mPelvis->getMatrix().getTrans(), osg::Vec3d(0, 0, 0));
        EXPECT_EQ(binding.captureWorldBones(osg::Matrixf::identity())[0].mPose, osg::Matrixf::identity());
    }

    TEST_F(ActorRagdollPoseTest, NativePlacementScalesPositionsWithSeparateRigidPhysicalRotation)
    {
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        auto placement = Nif::NiTransform::getIdentity();
        placement.mTranslation = {100, 0, 0};
        placement.mScale = 2.f;
        auto poses = binding.captureWorldBones(placement);
        ASSERT_EQ(poses.size(), 2u);
        EXPECT_EQ(poses[0].mPose.getTrans(), osg::Vec3f(122, 4, 0));
        EXPECT_EQ(poses[1].mPose.getTrans(), osg::Vec3f(120, 0, 0));
        EXPECT_EQ(poses[0].mPose(0, 0), 1.f);
        placement.mScale = .5f;
        placement.mRotation.mValues[0][0] = 0;
        placement.mRotation.mValues[0][1] = -1;
        placement.mRotation.mValues[1][0] = 1;
        placement.mRotation.mValues[1][1] = 0;
        poses = binding.captureWorldBones(placement);
        EXPECT_EQ(poses[0].mPose.getTrans(), osg::Vec3f(99, 5.5, 0));
        EXPECT_EQ(poses[1].mPose.getTrans(), osg::Vec3f(100, 5, 0));
        EXPECT_EQ(poses[0].mPose(0, 1), 1.f);
    }

    TEST_F(ActorRagdollPoseTest, NativePlacementWritebackDividesParentScaleWithoutAccumulation)
    {
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        auto placement = Nif::NiTransform::getIdentity();
        placement.mTranslation = {100, 0, 0};
        placement.mScale = 2.f;
        const auto target = desired();
        binding.applyWorldBones(target, placement);
        EXPECT_EQ(mPelvis->getMatrix().getTrans(), osg::Vec3d(10, 0, 0));
        EXPECT_EQ(mHand->getMatrix().getTrans(), osg::Vec3d(0, .5, 0));
        for (unsigned iteration = 0; iteration < 3; ++iteration)
        {
            binding.applyWorldBones(target, placement);
            const auto actual = binding.captureWorldBones(placement);
            EXPECT_EQ(actual[0].mPose.getTrans(), target[0].mPose.getTrans());
            EXPECT_EQ(actual[1].mPose.getTrans(), target[1].mPose.getTrans());
            EXPECT_EQ(mPelvis->mScale, 1.f);
            EXPECT_EQ(mHand->mScale, 1.f);
        }
        EXPECT_EQ(mConnector->getMatrix().getTrans(), osg::Vec3d(0, 2, 0));
    }

    TEST_F(ActorRagdollPoseTest, NativePlacementRejectsInvalidTransformsBeforeWriting)
    {
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        const auto pelvis = mPelvis->getMatrix();
        const auto hand = mHand->getMatrix();
        for (const float scale : {0.f, -1.f, std::numeric_limits<float>::infinity(),
                 std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::max()})
        {
            auto placement = Nif::NiTransform::getIdentity();
            placement.mScale = scale;
            EXPECT_THROW(binding.captureWorldBones(placement), std::invalid_argument);
            EXPECT_THROW(binding.applyWorldBones(desired(), placement), std::invalid_argument);
            EXPECT_EQ(mPelvis->getMatrix(), pelvis);
            EXPECT_EQ(mHand->getMatrix(), hand);
        }
        auto placement = Nif::NiTransform::getIdentity();
        placement.mScale = 2.f;
        placement.mRotation.mValues[0][1] = .25f;
        EXPECT_THROW(binding.captureWorldBones(placement), std::invalid_argument);
        EXPECT_THROW(binding.applyWorldBones(desired(), placement), std::invalid_argument);
        placement.mRotation = Nif::NiTransform::getIdentity().mRotation;
        mHand->setScale(2.f);
        EXPECT_THROW(binding.captureWorldBones(placement), std::invalid_argument);
        EXPECT_THROW(binding.applyWorldBones(desired(), placement), std::invalid_argument);
        EXPECT_EQ(mPelvis->getMatrix(), pelvis);
        auto nonuniform = osg::Matrixf::scale(2.f, 3.f, 2.f);
        EXPECT_THROW(binding.captureWorldBones(nonuniform), std::invalid_argument);
    }

    TEST_F(ActorRagdollPoseTest, LocalCheckpointRestoresExactMatricesAndSeparateNifCaches)
    {
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        // A procedural rotation can differ from the cached NIF rotation.
        mPelvis->setMatrix(osg::Matrixd::rotate(.1234567890123, osg::Vec3d(0, 0, 1))
            * osg::Matrixd::translate(10, 0, 0));
        mConnector->setScale(1.000001f);
        mHand->setTranslation({1, -0.f, 0});
        const auto pelvisMatrix = mPelvis->getMatrix();
        const auto connectorMatrix = mConnector->getMatrix();
        const auto handMatrix = mHand->getMatrix();
        const auto pelvisCache = mPelvis->mRotationScale;
        const auto connectorCache = mConnector->mRotationScale;
        const float scale = mConnector->mScale;
        const auto local = binding.captureLocalPose();
        mPelvis->setRotation(osg::Quat(.7, osg::Vec3f(1, 0, 0)));
        mPelvis->setTranslation({20, 30, 40});
        mConnector->setRotation(osg::Quat(.4, osg::Vec3f(0, 1, 0)));
        mConnector->setScale(2);
        mHand->setTranslation({50, 60, 70});
        binding.restoreLocalPose(local);
        EXPECT_EQ(mPelvis->getMatrix(), pelvisMatrix);
        EXPECT_EQ(mConnector->getMatrix(), connectorMatrix);
        EXPECT_EQ(mHand->getMatrix(), handMatrix);
        EXPECT_EQ(std::memcmp(mPelvis->getMatrix().ptr(), pelvisMatrix.ptr(), 16 * sizeof(double)), 0);
        EXPECT_EQ(std::memcmp(mConnector->getMatrix().ptr(), connectorMatrix.ptr(), 16 * sizeof(double)), 0);
        EXPECT_EQ(std::memcmp(mHand->getMatrix().ptr(), handMatrix.ptr(), 16 * sizeof(double)), 0);
        EXPECT_EQ(mConnector->mScale, scale);
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned column = 0; column < 3; ++column)
            {
                EXPECT_EQ(mPelvis->mRotationScale.mValues[row][column], pelvisCache.mValues[row][column]);
                EXPECT_EQ(mConnector->mRotationScale.mValues[row][column], connectorCache.mValues[row][column]);
            }
    }

    TEST_F(ActorRagdollPoseTest, LocalCheckpointRejectsEmptyForeignAndRecreatedBindingsBeforeWriting)
    {
        auto first = std::make_unique<SceneUtil::ActorRagdollPoseBinding>(mGraph, *mRoot);
        const auto local = first->captureLocalPose();
        mPelvis->setTranslation({20, 0, 0});
        const auto previous = mPelvis->getMatrix();
        EXPECT_THROW(first->restoreLocalPose({}), std::invalid_argument);
        SceneUtil::ActorRagdollPoseBinding other(mGraph, *mRoot);
        EXPECT_THROW(other.restoreLocalPose(local), std::invalid_argument);
        EXPECT_EQ(mPelvis->getMatrix(), previous);
        first.reset();
        SceneUtil::ActorRagdollPoseBinding recreated(mGraph, *mRoot);
        EXPECT_THROW(recreated.restoreLocalPose(local), std::invalid_argument);
        EXPECT_EQ(mPelvis->getMatrix(), previous);
    }

    TEST_F(ActorRagdollPoseTest, LocalCheckpointRejectsChangedAncestorBodyAndHierarchyAtomically)
    {
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        const auto local = binding.captureLocalPose();
        binding.applyWorldBones(desired(), mObjectWorld);
        const auto previous = mPelvis->getMatrix();
        mConnector->setUserValue("recordIndex", 19u);
        EXPECT_THROW(binding.restoreLocalPose(local), std::invalid_argument);
        EXPECT_EQ(mPelvis->getMatrix(), previous);
        mConnector->setUserValue("recordIndex", 18u);
        mHand->setName("Changed");
        EXPECT_THROW(binding.restoreLocalPose(local), std::invalid_argument);
        EXPECT_EQ(mPelvis->getMatrix(), previous);
        mHand->setName("Hand");
        mConnector->removeChild(mHand);
        EXPECT_THROW(binding.restoreLocalPose(local), std::invalid_argument);
        EXPECT_EQ(mPelvis->getMatrix(), previous);
        mConnector->addChild(mHand);
        mRoot->setUserValue(Misc::OsgUserValues::sFileHash, std::string("changed-asset"));
        EXPECT_THROW(binding.restoreLocalPose(local), std::invalid_argument);
        EXPECT_EQ(mPelvis->getMatrix(), previous);
    }

    TEST_F(ActorRagdollPoseTest, LocalCheckpointRecoversInvalidLiveFieldsAndInvalidatesSkinningMatrices)
    {
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        auto* hand = mRoot->getBone("Hand");
        ASSERT_NE(hand, nullptr);
        mRoot->updateBoneMatrices(0);
        const auto original = hand->mMatrixInSkeletonSpace;
        const auto local = binding.captureLocalPose();
        binding.applyWorldBones(desired(), mObjectWorld);
        mRoot->updateBoneMatrices(0);
        EXPECT_NE(hand->mMatrixInSkeletonSpace, original);
        mHand->mScale = 0;
        mHand->setTranslation({std::numeric_limits<float>::quiet_NaN(), 0, 0});
        EXPECT_THROW(binding.captureLocalPose(), std::invalid_argument);
        binding.restoreLocalPose(local);
        EXPECT_EQ(mHand->mScale, 1.f);
        EXPECT_EQ(mHand->getMatrix().getTrans(), osg::Vec3d(1, 0, 0));
        mRoot->updateBoneMatrices(0);
        EXPECT_EQ(hand->mMatrixInSkeletonSpace, original);
        EXPECT_EQ(binding.captureWorldBones(mObjectWorld)[0].mPose.getTrans(), osg::Vec3f(111, 2, 0));
    }

    TEST_F(ActorRagdollPoseTest, LocalCheckpointRejectsInvalidCachedRotationWithValidRenderedMatrix)
    {
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        const auto valid = binding.captureLocalPose();
        const auto rendered = mHand->getMatrix();
        mHand->mRotationScale.mValues[2][2] = std::numeric_limits<float>::quiet_NaN();
        EXPECT_EQ(mHand->getMatrix(), rendered);
        EXPECT_THROW(binding.captureLocalPose(), std::invalid_argument);
        binding.restoreLocalPose(valid);
        EXPECT_EQ(mHand->mRotationScale.mValues[2][2], 1.f);
        mHand->mRotationScale.mValues[0][0] = 2;
        EXPECT_THROW(binding.captureLocalPose(), std::invalid_argument);
        binding.restoreLocalPose(valid);
        EXPECT_EQ(mHand->mRotationScale.mValues[0][0], 1.f);
    }

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
        mGraph.mBodies[0].mBone.clear();
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

    TEST_F(ActorRagdollPoseTest, PreservesUnrelatedEffectTransformsButRejectsThemOnPhysicalPaths)
    {
        osg::ref_ptr<osg::MatrixTransform> effect = new osg::MatrixTransform;
        mRoot->addChild(effect);
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        const auto checkpoint = binding.captureLocalPose();
        const auto effectMatrix = osg::Matrixd::translate(4, 5, 6);
        effect->setMatrix(effectMatrix);
        EXPECT_TRUE(binding.matchesLocalPose(checkpoint));
        binding.applyWorldBones(desired(), mObjectWorld);
        EXPECT_EQ(binding.captureWorldBones(mObjectWorld)[0].mPose.getTrans(), osg::Vec3f(120, 5, 0));
        EXPECT_EQ(effect->getMatrix(), effectMatrix);

        mConnector->removeChild(mHand);
        effect->addChild(mHand);
        EXPECT_FALSE(binding.isValid());
        EXPECT_THROW((SceneUtil::ActorRagdollPoseBinding(mGraph, *mRoot)), std::invalid_argument);
        EXPECT_EQ(effect->getMatrix(), effectMatrix);
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

    TEST_F(ActorRagdollPoseTest, CapturesQuaternionCallbackPrecisionAndProceduralRenderedRotation)
    {
        mPelvis->setRotation(osg::Quat(.4, osg::Vec3f(0, 0, 1)));
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        const auto checkRendered = [&] {
            const auto poses = binding.captureWorldBones(mObjectWorld);
            const osg::Matrixf expected = mHand->getMatrix() * mConnector->getMatrix()
                * mPelvis->getMatrix() * mObjectWorld;
            for (unsigned row = 0; row < 4; ++row)
                for (unsigned column = 0; column < 4; ++column)
                    EXPECT_NEAR(poses[0].mPose(row, column), expected(row, column), 1e-4);
        };
        checkRendered();
        const auto cached = mPelvis->mRotationScale;
        mPelvis->setMatrix(osg::Matrixd::rotate(-.7, osg::Vec3f(0, 0, 1))
            * osg::Matrixd::translate(10, 0, 0));
        EXPECT_FLOAT_EQ(mPelvis->mRotationScale.mValues[0][0], cached.mValues[0][0]);
        checkRendered();
    }

    TEST_F(ActorRagdollPoseTest, RejectsNonRigidRenderedMatrixEvenWhenCachedNifRotationIsValid)
    {
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        auto shear = mPelvis->getMatrix();
        shear(0, 1) = .5;
        mPelvis->setMatrix(shear);
        EXPECT_THROW(binding.captureWorldBones(mObjectWorld), std::invalid_argument);
        EXPECT_THROW(binding.applyWorldBones(desired(), mObjectWorld), std::invalid_argument);
        EXPECT_EQ(mPelvis->getMatrix(), shear);
        EXPECT_EQ(mHand->getMatrix().getTrans(), osg::Vec3f(1, 0, 0));
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

namespace
{
    TEST_F(ActorRagdollPoseTest, NativeBlendFrameScenePublishesRendererAndRejectsInvalidProjectionBeforePhysics)
    {
        for (auto& body : mGraph.mBodies)
        {
            body.mMass = 2;
            body.mInertia = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            body.mShape = NifBullet::RagdollSphere{.5f};
            body.mBlend = NifBullet::RagdollBlendDefinition{body.mRecord + 100, 8, 0.f, 0.f};
        }
        mGraph.mBodies[1].mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {{.25f, 0.f, 0.f}}};
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        const auto bones = binding.captureWorldBones(mObjectWorld);
        const auto physical = NifBullet::ragdollBodyWorldPoses(mGraph, desired());
        btDefaultCollisionConfiguration configuration;
        btCollisionDispatcher dispatcher(&configuration);
        btDbvtBroadphase broadphase;
        btSequentialImpulseConstraintSolver solver;
        btDiscreteDynamicsWorld world(&dispatcher, &broadphase, &solver, &configuration);
        NifBullet::ActorRagdollPhysics actor(mGraph, world, NifBullet::RagdollNativeLengthScale, physical, 1, -1);
        const std::array<std::uint32_t, 1> order{{78}};
        ESM4::PhysicalBlendTimeCache cache;
        unsigned calls = 0;
        bool reject = true;
        const auto publish = [&](std::span<const NifBullet::RagdollNativeBlendPublication> publications) {
            ++calls;
            auto scene = bones;
            for (const auto& publication : publications)
                if (publication.mSceneTarget)
                    for (std::size_t i = 0; i < mGraph.mBodies.size(); ++i)
                        if (mGraph.mBodies[i].mRecord == publication.mRecord)
                            scene[i].mPose = *publication.mSceneTarget;
            if (reject)
                scene.back().mPose = osg::Matrixf::scale(2, 2, 2);
            binding.applyWorldBones(scene, mObjectWorld);
        };
        const auto oldPelvis = mPelvis->getMatrix(), oldHand = mHand->getMatrix();
        EXPECT_THROW(actor.updateNativeBlendFrame(bones, order, 1.f, cache, 1.f / 120, 0, 0.f, publish),
            std::invalid_argument);
        EXPECT_EQ(calls, 1u);
        EXPECT_EQ(mPelvis->getMatrix(), oldPelvis);
        EXPECT_EQ(mHand->getMatrix(), oldHand);
        EXPECT_FLOAT_EQ(actor.captureNativeBlendControllers()[0].mState.mClock.mPreviousTime,
            -std::numeric_limits<float>::max());
        EXPECT_EQ(cache.mCycle, 0xffffffffu);
        EXPECT_EQ(actor.capture()[0].mPose, physical[0]);
        reject = false;
        ASSERT_EQ(actor.updateNativeBlendFrame(bones, order, 1.f, cache, 1.f / 120, 0, 0.f, publish).size(), 2u);
        EXPECT_EQ(calls, 2u);
        const auto rendered = binding.captureWorldBones(mObjectWorld);
        const auto expected = desired();
        for (std::size_t i = 0; i < rendered.size(); ++i)
            EXPECT_LT((rendered[i].mPose.getTrans() - expected[i].mPose.getTrans()).length(), .001f);
        EXPECT_FLOAT_EQ(actor.captureNativeBlendControllers()[0].mState.mClock.mPreviousTime, 1.f);
        EXPECT_EQ(cache.mCycle, 2u);
    }
    TEST_F(ActorRagdollPoseTest, RelocatedReferenceRootCanUseAnEmptyNameWithExactRecordIdentity)
    {
        osg::ref_ptr<NifOsg::MatrixTransform> root
            = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        root->setUserValue(Misc::OsgUserValues::sFileHash, mGraph.mSourceHash);
        root->setUserValue("recordIndex", 0u);
        auto body = mGraph.mBodies.front();
        body.mNodeRecord = 0;
        body.mBone.clear();
        mGraph.mBodies = {body};
        mGraph.mReferenceRoot = 0;
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *root);
        EXPECT_TRUE(binding.isValid());
        EXPECT_EQ(binding.captureWorldBones(mObjectWorld)[0].mPose.getTrans(), osg::Vec3f(100, 0, 0));
        root->setName("different root");
        EXPECT_FALSE(binding.isValid());
        root->setName("");
        mGraph.mReferenceRoot.reset();
        EXPECT_THROW((SceneUtil::ActorRagdollPoseBinding(mGraph, *root)), std::invalid_argument);
    }

    TEST_F(ActorRagdollPoseTest, PreparedLocalGuardRejectsForeignOrChangedStateWithoutWritingScene)
    {
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        const auto checkpoint = binding.captureLocalPose();
        SceneUtil::ActorRagdollPoseBinding other(mGraph, *mRoot);
        EXPECT_TRUE(binding.isValid());
        EXPECT_TRUE(binding.matchesLocalPose(checkpoint));
        EXPECT_FALSE(other.matchesLocalPose(checkpoint));
        EXPECT_FALSE(binding.matchesLocalPose({}));
        const auto matrix = mHand->getMatrix();
        mHand->setTranslation({9, 8, 7});
        EXPECT_TRUE(binding.isValid());
        EXPECT_FALSE(binding.matchesLocalPose(checkpoint));
        EXPECT_EQ(mHand->getMatrix().getTrans(), osg::Vec3d(9, 8, 7));
        binding.restoreLocalPose(checkpoint);
        EXPECT_EQ(mHand->getMatrix(), matrix);
        mHand->mRotationScale.mValues[0][1] = -0.f;
        EXPECT_FALSE(binding.matchesLocalPose(checkpoint));
        binding.restoreLocalPose(checkpoint);
        mHand->setName("retired");
        EXPECT_FALSE(binding.isValid());
        EXPECT_FALSE(binding.matchesLocalPose(checkpoint));
        mHand->setName("Hand");
        EXPECT_TRUE(binding.matchesLocalPose(checkpoint));
        mConnector->removeChild(mHand);
        EXPECT_FALSE(binding.isValid());
        mConnector->addChild(mHand);
        EXPECT_TRUE(binding.matchesLocalPose(checkpoint));
        mRoot->setUserValue(Misc::OsgUserValues::sFileHash, std::string("another resource hash"));
        EXPECT_FALSE(binding.isValid());
        EXPECT_FALSE(binding.matchesLocalPose(checkpoint));
        mRoot->setUserValue(Misc::OsgUserValues::sFileHash, mGraph.mSourceHash);
        EXPECT_TRUE(binding.matchesLocalPose(checkpoint));
    }

}

namespace
{
    TEST_F(ActorRagdollPoseTest, TraversalProjectionIsCheckpointedWithoutFeedingBackIntoAnimationTargets)
    {
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot);
        const auto original = binding.captureLocalPose();
        const auto author = binding.captureWorldBones(osg::Matrixf::identity());
        mPelvis->setTraversalMatrix(osg::Matrixd::translate(40, 0, 0));
        EXPECT_FALSE(binding.matchesLocalPose(original));
        EXPECT_EQ(mPelvis->getMatrix().getTrans(), osg::Vec3d(10, 0, 0));
        const auto targets = binding.captureWorldBones(osg::Matrixf::identity());
        ASSERT_EQ(targets.size(), author.size());
        for (unsigned i = 0; i < targets.size(); ++i) EXPECT_EQ(targets[i].mPose, author[i].mPose);
        const auto worlds = mPelvis->getWorldMatrices(); ASSERT_EQ(worlds.size(), 1u);
        EXPECT_EQ(worlds[0].getTrans(), osg::Vec3d(40, 0, 0));
        const auto projected = binding.captureLocalPose();
        mPelvis->clearTraversalMatrix(); mPelvis->setTranslation({7, 8, 9});
        EXPECT_FALSE(binding.matchesLocalPose(projected));
        binding.restoreLocalPose(projected);
        EXPECT_TRUE(binding.matchesLocalPose(projected));
        EXPECT_EQ(mPelvis->getMatrix().getTrans(), osg::Vec3d(10, 0, 0));
        EXPECT_EQ(mPelvis->getWorldMatrices()[0].getTrans(), osg::Vec3d(40, 0, 0));
        mPelvis->setTraversalMatrix(osg::Matrixd::translate(40, 0, 0));
        EXPECT_TRUE(binding.matchesLocalPose(projected));
        auto negativeZero = *mPelvis->getTraversalMatrix().getMatrix(); negativeZero(0, 1) = -0.;
        mPelvis->setTraversalMatrix(negativeZero);
        EXPECT_FALSE(binding.matchesLocalPose(projected));
        binding.restoreLocalPose(original);
        EXPECT_EQ(mPelvis->getTraversalMatrix().getMatrix(), nullptr);
        EXPECT_TRUE(binding.matchesLocalPose(original));
    }
}

namespace
{
    using RootSpace = SceneUtil::ActorRagdollPoseBinding::RootSpace;

    TEST_F(ActorRagdollPoseTest, NativeObjectLocalUsesOriginalSingleCompositionWithPlacementOutsideRoot)
    {
        // Same independently executed 707370 fixture as ReferenceWorld below;
        // here the global transform comes from placement, not an authored root.
        const std::array<std::uint32_t, 9> rotation{0x3f3b0df5u,0x3f1d8dc8u,0x3e974e6du,
            0xbf2d20bcu,0x3f3636ceu,0x3e4259e6u,0xbdbf8f0cu,0xbead547au,0x3f6fb0edu};
        const std::array<std::uint32_t, 3> position{0x41b21a51u,0x41bc4696u,0x42105fb0u};
        auto placement = Nif::NiTransform::getIdentity();
        for (unsigned i = 0; i < 9; ++i)
            placement.mRotation.mValues[i / 3][i % 3] = std::bit_cast<float>(rotation[i]);
        placement.mTranslation = {10,20,30}; placement.mScale = 2;
        mPelvis->setTranslation({0,0,0}); mConnector->setTranslation({3,4,5}); mHand->setTranslation({0,0,0});
        mPelvis->setTraversalMatrix(osg::Matrixd::translate(999,999,999));
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot, RootSpace::NativeObjectLocal);
        const auto before = binding.captureLocalPose();
        const auto result = binding.captureWorldBones(placement);
        ASSERT_EQ(result.size(), 2u);
        for (unsigned i = 0; i < 9; ++i)
            EXPECT_EQ(std::bit_cast<std::uint32_t>(result[0].mPose(i % 3,i / 3)), rotation[i]);
        for (unsigned i = 0; i < 3; ++i)
            EXPECT_EQ(std::bit_cast<std::uint32_t>(result[0].mPose(3,i)), position[i]);
        EXPECT_EQ(result[1].mPose.getTrans(), placement.mTranslation);
        EXPECT_TRUE(binding.matchesLocalPose(before));
        EXPECT_FALSE(mGraph.mReferenceRoot); // Object-local does not require a global reference root.
    }

    TEST_F(ActorRagdollPoseTest, NativeObjectLocalWritebackKeepsRootRelativeToPlacement)
    {
        auto placement = Nif::NiTransform::getIdentity();
        placement.mTranslation = {100,20,30}; placement.mScale = 2;
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot, RootSpace::NativeObjectLocal);
        const auto before = binding.captureLocalPose();
        const std::array<NifBullet::RagdollBoneWorldPose, 2> poses{{
            {28,osg::Matrixf::translate(145,26,30)}, {8,osg::Matrixf::translate(140,20,30)}}};
        binding.applyWorldBones(poses, placement);
        EXPECT_EQ(mPelvis->getMatrix().getTrans(), osg::Vec3d(20,0,0));
        EXPECT_EQ(mHand->getMatrix().getTrans(), osg::Vec3d(2.5,1,0));
        EXPECT_FLOAT_EQ(mPelvis->mScale, 1); EXPECT_FLOAT_EQ(mHand->mScale, 1);
        const auto result = binding.captureWorldBones(placement);
        EXPECT_EQ(result[0].mPose, poses[0].mPose); EXPECT_EQ(result[1].mPose, poses[1].mPose);
        binding.restoreLocalPose(before);
        EXPECT_TRUE(binding.matchesLocalPose(before));
    }

    TEST_F(ActorRagdollPoseTest, ReferenceWorldRootCapturesOriginalGlobalHierarchyWithoutDoublePlacement)
    {
        // reference-root-world-capture-oracle-01: actual root initialization
        // segments then707370 ordinary composition,007F, angles(.2,-.3,.7),scale2.
        const std::array<std::uint32_t, 9> rotation{0x3f3b0df5u,0x3f1d8dc8u,0x3e974e6du,
            0xbf2d20bcu,0x3f3636ceu,0x3e4259e6u,0xbdbf8f0cu,0xbead547au,0x3f6fb0edu};
        const std::array<std::uint32_t, 3> childPosition{0x41b21a51u,0x41bc4696u,0x42105fb0u};
        auto root = Nif::NiTransform::getIdentity();
        for (unsigned i = 0; i < 9; ++i) root.mRotation.mValues[i / 3][i % 3] = std::bit_cast<float>(rotation[i]);
        root.mTranslation = {10,20,30}; root.mScale = 2;
        mGraph.mReferenceRoot = 8;
        mPelvis->setScale(root.mScale); mPelvis->setRotation(root.mRotation); mPelvis->setTranslation(root.mTranslation);
        mConnector->setTranslation({3,4,5}); mHand->setTranslation({0,0,0});
        auto placement = Nif::NiTransform::getIdentity(); placement.mScale = 2;
        placement.mTranslation = {10000,20000,30000};
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot, RootSpace::ReferenceWorld);
        const auto result = binding.captureWorldBones(placement); ASSERT_EQ(result.size(), 2u);
        for (unsigned i = 0; i < 9; ++i)
        {
            EXPECT_EQ(std::bit_cast<std::uint32_t>(result[1].mPose(i % 3,i / 3)), rotation[i]);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(result[0].mPose(i % 3,i / 3)), rotation[i]);
        }
        EXPECT_EQ(result[1].mPose.getTrans(), osg::Vec3f(10,20,30));
        for (unsigned i = 0; i < 3; ++i)
            EXPECT_EQ(std::bit_cast<std::uint32_t>(result[0].mPose(3,i)), childPosition[i]);
        EXPECT_FLOAT_EQ(mPelvis->mScale, 2);
        EXPECT_EQ(mPelvis->getMatrix().getTrans(), osg::Vec3d(10,20,30));
    }

    TEST_F(ActorRagdollPoseTest, ReferenceWorldPhysicalSnapshotWritesGlobalRootAndRelativeChildren)
    {
        mGraph.mReferenceRoot = 8;
        mPelvis->setScale(2); mPelvis->setTranslation({100,20,30});
        auto placement = Nif::NiTransform::getIdentity(); placement.mScale = 2; placement.mTranslation = {100,20,30};
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot, RootSpace::ReferenceWorld);
        const auto saved = binding.captureLocalPose();
        const std::array<NifBullet::RagdollBoneWorldPose, 2> poses{{
            {28,osg::Matrixf::translate(125,26,30)}, {8,osg::Matrixf::translate(120,20,30)}}};
        binding.applyWorldBones(poses, placement);
        EXPECT_EQ(mPelvis->getMatrix().getTrans(), osg::Vec3d(120,20,30));
        EXPECT_EQ(mHand->getMatrix().getTrans(), osg::Vec3d(2.5,1,0));
        EXPECT_FLOAT_EQ(mPelvis->mScale, 2); EXPECT_FLOAT_EQ(mHand->mScale, 1);
        const auto result = binding.captureWorldBones(placement);
        EXPECT_EQ(result[0].mPose, poses[0].mPose); EXPECT_EQ(result[1].mPose, poses[1].mPose);
        binding.restoreLocalPose(saved);
        EXPECT_EQ(mPelvis->getMatrix().getTrans(), osg::Vec3d(100,20,30));
        EXPECT_EQ(mHand->getMatrix().getTrans(), osg::Vec3d(1,0,0));
        EXPECT_TRUE(binding.matchesLocalPose(saved));
    }

    TEST_F(ActorRagdollPoseTest, ReferenceWorldReaderPreservesNativeRotationBitsAndIgnoresTraversalProjection)
    {
        const std::array<std::uint32_t, 9> rotation{0x3f3b0df4u,0xbe974e6du,0x3f1d8dc8u,
            0x3e677372u,0x3f7490efu,0x3e42f2ceu,0xbf24eb73u,0u,0x3f43ccb3u};
        Nif::Matrix3 native;
        for (unsigned i = 0; i < 9; ++i) native.mValues[i / 3][i % 3] = std::bit_cast<float>(rotation[i]);
        mGraph.mReferenceRoot = 8;
        mPelvis->setScale(1.25f); mPelvis->setRotation(native);
        mPelvis->setTraversalMatrix(osg::Matrixd::translate(999,999,999));
        auto placement = Nif::NiTransform::getIdentity(); placement.mScale = 1.25f;
        SceneUtil::ActorRagdollPoseBinding binding(mGraph, *mRoot, RootSpace::ReferenceWorld);
        const auto result = binding.captureWorldBones(placement);
        for (unsigned i = 0; i < 9; ++i)
            EXPECT_EQ(std::bit_cast<std::uint32_t>(result[1].mPose(i % 3,i / 3)), rotation[i]);
        EXPECT_EQ(result[1].mPose.getTrans(), osg::Vec3f(10,0,0));
        // A raw procedural write leaves the separate NIF rotation cache stale.
        osg::Matrixd raw; raw(0,0)=raw(1,1)=0; raw(0,1)=1.25; raw(1,0)=-1.25; raw(2,2)=1.25;
        mPelvis->setMatrix(raw);
        const auto procedural = binding.captureWorldBones(placement);
        EXPECT_FLOAT_EQ(procedural[1].mPose(0,1), 1); EXPECT_FLOAT_EQ(procedural[1].mPose(1,0), -1);
        EXPECT_FLOAT_EQ(procedural[1].mPose(0,0), 0);
        for (unsigned i = 0; i < 9; ++i)
            EXPECT_EQ(std::bit_cast<std::uint32_t>(mPelvis->mRotationScale.mValues[i / 3][i % 3]), rotation[i]);
    }

    TEST_F(ActorRagdollPoseTest, NativeNamedWorldPoseReadsCurrentLocalsWithSeparateScale)
    {
        auto placement = Nif::NiTransform::getIdentity();
        placement.mTranslation = {100, 200, 300};
        placement.mScale = 3;
        placement.mRotation.mValues[2][2] = 2; // Native morphology stays in rotation.
        mPelvis->setScale(2);
        const auto before = mHand->getMatrix();
        const auto capture = [&] { return SceneUtil::captureNativeNodeWorldPose(*mRoot, "Hand", placement); };
        auto pose = capture();
        ASSERT_TRUE(pose);
        EXPECT_EQ(pose->mTranslation, osg::Vec3f(136, 212, 300));
        EXPECT_EQ(pose->mScale, 6);
        EXPECT_EQ(pose->mRotation.mValues[2][2], 2);
        // A procedural write changes the drawing/controller local while the
        // separate NIF cache deliberately retains its previous identity.
        auto rotated = before;
        rotated(1, 1) = rotated(2, 2) = -1;
        mHand->setMatrix(rotated);
        EXPECT_EQ(mHand->mRotationScale.mValues[2][2], 1);
        mHand->setTraversalMatrix(NifOsg::MatrixTransform::TraversalMatrix(osg::Matrixd::identity()));
        pose = capture();
        ASSERT_TRUE(pose);
        EXPECT_EQ(pose->mRotation.mValues[2][2], -2);
        EXPECT_EQ(pose->mTranslation, osg::Vec3f(136, 212, 300));
        EXPECT_EQ(mHand->getMatrix(), rotated);
        EXPECT_EQ(mHand->mRotationScale.mValues[2][2], 1);
        EXPECT_NE(mHand->getTraversalMatrix().getMatrix(), nullptr);
        EXPECT_FALSE(SceneUtil::captureNativeNodeWorldPose(*mRoot, "hand", placement));
        EXPECT_FALSE(SceneUtil::captureNativeNodeWorldPose(*mRoot, "Bip01 Spine", placement));
        EXPECT_THROW(SceneUtil::captureNativeNodeWorldPose(*mRoot, "", placement), std::invalid_argument);
    }

    TEST_F(ActorRagdollPoseTest, NativeNamedWorldPoseRejectsAmbiguousOrUnsupportedSources)
    {
        const auto placement = Nif::NiTransform::getIdentity();
        const auto capture = [&] { return SceneUtil::captureNativeNodeWorldPose(*mRoot, "Hand", placement); };
        osg::ref_ptr<NifOsg::MatrixTransform> duplicate
            = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        duplicate->setName("Hand");
        mRoot->addChild(duplicate);
        EXPECT_THROW(capture(), std::invalid_argument);
        mRoot->removeChild(duplicate);
        mPelvis->addChild(mHand); // Same target has two scene paths.
        EXPECT_THROW(capture(), std::invalid_argument);
        mPelvis->removeChild(mHand);
        auto malformed = mHand->getMatrix();
        malformed(2, 2) = std::numeric_limits<double>::quiet_NaN();
        mHand->setMatrix(malformed);
        EXPECT_THROW(capture(), std::invalid_argument);
        mHand->setMatrix(osg::Matrixd::identity());
        osg::ref_ptr<osg::MatrixTransform> wrapper = new osg::MatrixTransform;
        mConnector->removeChild(mHand);
        mConnector->addChild(wrapper);
        wrapper->addChild(mHand);
        EXPECT_THROW(capture(), std::invalid_argument);
        EXPECT_EQ(mHand->getMatrix(), osg::Matrixd::identity());
    }

    TEST_F(ActorRagdollPoseTest, ReferenceWorldModeRequiresADeclaredSingleHierarchyRoot)
    {
        EXPECT_THROW((SceneUtil::ActorRagdollPoseBinding(mGraph, *mRoot, RootSpace::ReferenceWorld)), std::invalid_argument);
        mGraph.mReferenceRoot = 18;
        EXPECT_THROW((SceneUtil::ActorRagdollPoseBinding(mGraph, *mRoot, RootSpace::ReferenceWorld)), std::invalid_argument);
        mGraph.mReferenceRoot = 8;
        EXPECT_THROW((SceneUtil::ActorRagdollPoseBinding(mGraph, *mRoot, static_cast<RootSpace>(99))), std::invalid_argument);
        osg::ref_ptr<NifOsg::MatrixTransform> other = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        other->setName("Other"); other->setUserValue("recordIndex", 48u); mRoot->addChild(other);
        auto body = mGraph.mBodies.back(); body.mNodeRecord=48; body.mRecord=49; body.mBone="Other";
        mGraph.mBodies.push_back(body);
        EXPECT_THROW((SceneUtil::ActorRagdollPoseBinding(mGraph, *mRoot, RootSpace::ReferenceWorld)), std::invalid_argument);
        EXPECT_EQ(mPelvis->getMatrix().getTrans(), osg::Vec3d(10,0,0));
        EXPECT_EQ(mPelvis->getTraversalMatrix().getMatrix(), nullptr);
    }
}
