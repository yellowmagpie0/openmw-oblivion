#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/nifbullet/nativedynamicsworld.hpp>
#include <components/nifbullet/ragdollconecoordinates.hpp>
#include <components/nifbullet/ragdollvelocity.hpp>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>

#include <gtest/gtest.h>

#include <limits>
#include <bit>

namespace
{
    NifBullet::RagdollConeJoint coneLimits()
    {
        NifBullet::RagdollConeJoint result{};
        result.mConeAngle = 0.5f;
        result.mPlaneMin = -0.2f;
        result.mPlaneMax = 0.3f;
        result.mTwistMin = -0.4f;
        result.mTwistMax = 0.6f;
        return result;
    }

    TEST(RagdollBodyTSceneCenter, SubtractsOffsetUsingPhysicalBasis)
    {
        NifBullet::RagdollBodyDefinition body{};
        body.mUsesRigidBodyTransform = true;
        body.mTranslation = {2, 3, 4};
        body.mRotation = osg::Quat(0, 0, 1, 0);
        const osg::Vec3f center(10, 20, 30);
        const auto result = NifBullet::ragdollNativeSceneCenterOfMass(center, {0, 0, 1, 0}, body);
        EXPECT_EQ(result, osg::Vec3f(12, 23, 26));
        EXPECT_EQ(center, osg::Vec3f(10, 20, 30));
        EXPECT_EQ(body.mTranslation, osg::Vec3f(2, 3, 4));
        // The local rotation is unused by the original COM getter.
        body.mRotation = osg::Quat(0, 0, 0, 0);
        EXPECT_EQ(NifBullet::ragdollNativeSceneCenterOfMass(center, {0, 0, 1, 0}, body), result);
    }

    TEST(RagdollBodyTSceneCenter, OrdinaryBodiesIgnoreUnusedOffsetMetadata)
    {
        NifBullet::RagdollBodyDefinition body{};
        body.mTranslation.x() = std::numeric_limits<float>::quiet_NaN();
        body.mRotation = osg::Quat(0, 0, 0, 0);
        EXPECT_EQ(NifBullet::ragdollNativeSceneCenterOfMass({1, 2, 3}, {0, 0, 0, 1}, body),
            osg::Vec3f(1, 2, 3));
    }

    TEST(RagdollBodyTSceneCenter, RejectsInvalidOrOverflowingInputsWithoutMutation)
    {
        NifBullet::RagdollBodyDefinition body{};
        body.mUsesRigidBodyTransform = true;
        const osg::Vec3f center(1, 2, 3);
        EXPECT_THROW(NifBullet::ragdollNativeSceneCenterOfMass(center, {0, 0, 0, 0}, body),
            std::invalid_argument);
        body.mTranslation.x() = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(NifBullet::ragdollNativeSceneCenterOfMass(center, {0, 0, 0, 1}, body),
            std::invalid_argument);
        body.mTranslation = {};
        EXPECT_THROW(NifBullet::ragdollNativeSceneCenterOfMass(
            {std::numeric_limits<float>::infinity(), 0, 0}, {0, 0, 0, 1}, body), std::invalid_argument);
        body.mTranslation.x() = -std::numeric_limits<float>::max();
        EXPECT_THROW(NifBullet::ragdollNativeSceneCenterOfMass(
            {std::numeric_limits<float>::max(), 0, 0}, {0, 0, 0, 1}, body), std::invalid_argument);
        EXPECT_EQ(center, osg::Vec3f(1, 2, 3));
        EXPECT_EQ(body.mTranslation.x(), -std::numeric_limits<float>::max());
    }

    TEST(RagdollBodyTReverseScene, RemovesNativeLocalRotationAndTranslation)
    {
        NifBullet::RagdollBodyDefinition body{};
        body.mUsesRigidBodyTransform = true;
        body.mTranslation = {2, 3, 4};
        body.mRotation = osg::Quat(0, 0, 1, 0);
        NifBullet::RagdollNativeTargetPose physical{{12, 23, 34}, {0, 0, 1, 0}};
        const auto result = NifBullet::ragdollNativeSceneTargetFromBodyPose(physical, body);
        EXPECT_EQ(result.mPosition, osg::Vec3f(10, 20, 30));
        EXPECT_EQ(result.mRotation, (std::array<float, 4>{0, 0, 0, 1}));
        EXPECT_EQ(physical.mPosition, osg::Vec3f(12, 23, 34));
        EXPECT_EQ(body.mTranslation, osg::Vec3f(2, 3, 4));
    }

    TEST(RagdollBodyTReverseScene, PreservesNoncommutingQuaternionOrderAndRotatedOffset)
    {
        NifBullet::RagdollBodyDefinition body{};
        body.mUsesRigidBodyTransform = true;
        body.mTranslation = {2, 3, 4};
        const float half = std::sqrt(.5f);
        body.mRotation = osg::Quat(0, 0, half, half);
        NifBullet::RagdollNativeTargetPose physical{{10, 20, 30}, {.5f, -.5f, .5f, .5f}};
        const auto result = NifBullet::ragdollNativeSceneTargetFromBodyPose(physical, body);
        EXPECT_EQ(result.mRotation, (std::array<float, 4>{half, 0, 0, half}));
        EXPECT_NEAR(result.mPosition.x(), 8.f, 1e-5);
        EXPECT_NEAR(result.mPosition.y(), 24.f, 1e-5);
        EXPECT_NEAR(result.mPosition.z(), 27.f, 1e-5);
        body.mUsesRigidBodyTransform = false;
        body.mTranslation.x() = std::numeric_limits<float>::quiet_NaN();
        body.mRotation = osg::Quat(0, 0, 0, 0);
        const auto ordinary = NifBullet::ragdollNativeSceneTargetFromBodyPose(physical, body);
        EXPECT_EQ(ordinary.mPosition, physical.mPosition);
        EXPECT_EQ(ordinary.mRotation, physical.mRotation);
    }

    TEST(RagdollBodyTReverseScene, RejectsMalformedOrOverflowingNativePosesAtomically)
    {
        NifBullet::RagdollBodyDefinition body{};
        body.mUsesRigidBodyTransform = true;
        body.mRotation = osg::Quat(0, 0, 0, 1);
        NifBullet::RagdollNativeTargetPose physical{{1, 2, 3}, {0, 0, 0, 1}};
        body.mTranslation.x() = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(NifBullet::ragdollNativeSceneTargetFromBodyPose(physical, body), std::invalid_argument);
        body.mTranslation = {};
        body.mRotation = osg::Quat(0, 0, 0, 0);
        EXPECT_THROW(NifBullet::ragdollNativeSceneTargetFromBodyPose(physical, body), std::invalid_argument);
        body.mRotation = osg::Quat(0, 0, 0, 1);
        physical.mRotation = {0, 0, 0, 0};
        EXPECT_THROW(NifBullet::ragdollNativeSceneTargetFromBodyPose(physical, body), std::invalid_argument);
        physical.mRotation = {0, 0, 0, 1};
        physical.mPosition.x() = std::numeric_limits<float>::infinity();
        EXPECT_THROW(NifBullet::ragdollNativeSceneTargetFromBodyPose(physical, body), std::invalid_argument);
        physical.mPosition.x() = std::numeric_limits<float>::max();
        body.mTranslation.x() = -std::numeric_limits<float>::max();
        EXPECT_THROW(NifBullet::ragdollNativeSceneTargetFromBodyPose(physical, body), std::invalid_argument);
        EXPECT_EQ(physical.mPosition.x(), std::numeric_limits<float>::max());
    }

    TEST(RagdollBodyTSceneTarget, AppliesLocalOffsetInNativeLengthsAndOrderedQuaternionProduct)
    {
        NifBullet::RagdollBodyDefinition body{};
        body.mUsesRigidBodyTransform = true;
        body.mTranslation = {2, 3, 4};
        body.mRotation = osg::Quat(0, 0, 1, 0);
        const auto world = osg::Matrixf::translate(7, 14, 21);
        const auto ordinary = NifBullet::ragdollNativeSceneTargetPose(world);
        const auto target = NifBullet::ragdollNativeSceneBodyTargetPose(world, body);
        EXPECT_EQ(target.mRotation, (std::array<float, 4>{0, 0, 1, 0}));
        for (unsigned i = 0; i < 3; ++i)
            EXPECT_EQ(target.mPosition[i], float(ordinary.mPosition[i] + body.mTranslation[i]));
        auto rotated = osg::Matrixf::identity();
        rotated(0, 0) = rotated(1, 1) = 0;
        rotated(0, 1) = 1;
        rotated(1, 0) = -1;
        body.mRotation = osg::Quat(1, 0, 0, 0);
        const auto parent = NifBullet::ragdollNativeSceneTargetPose(rotated);
        const auto product = NifBullet::ragdollNativeSceneBodyTargetPose(rotated, body);
        EXPECT_EQ(product.mRotation[0], parent.mRotation[3]);
        EXPECT_EQ(product.mRotation[1], parent.mRotation[2]);
        EXPECT_EQ(product.mRotation[2], 0.f);
        EXPECT_EQ(product.mRotation[3], 0.f);
        EXPECT_NEAR(product.mPosition.x(), -3.f, 1e-6);
        EXPECT_NEAR(product.mPosition.y(), 2.f, 1e-6);
        EXPECT_NEAR(product.mPosition.z(), 4.f, 1e-6);
    }

    TEST(RagdollBodyTSceneTarget, UsesAlreadyScaledLocalOffsetWithoutScalingItAgain)
    {
        NifBullet::RagdollBodyDefinition body{};
        body.mUsesRigidBodyTransform = true;
        body.mRotation = osg::Quat(0, 0, 0, 1);
        body.mTranslation = {6, -4, 2}; // Graph's scale2 offset, already in native units.
        const auto target = NifBullet::ragdollNativeSceneBodyTargetPose(osg::Matrixf::identity(), body);
        EXPECT_EQ(target.mPosition, body.mTranslation);
        body.mUsesRigidBodyTransform = false;
        body.mTranslation.x() = std::numeric_limits<float>::quiet_NaN();
        body.mRotation = osg::Quat(0, 0, 0, 0);
        const auto ordinary = NifBullet::ragdollNativeSceneBodyTargetPose(osg::Matrixf::identity(), body);
        EXPECT_EQ(ordinary.mPosition, osg::Vec3f());
        EXPECT_EQ(ordinary.mRotation, (std::array<float, 4>{0, 0, 0, 1}));
    }

    TEST(RagdollBodyTSceneTarget, RejectsInvalidSupportedOffsetsWithoutChangingInputs)
    {
        NifBullet::RagdollBodyDefinition body{};
        body.mUsesRigidBodyTransform = true;
        body.mRotation = osg::Quat(0, 0, 0, 1);
        body.mTranslation.x() = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(NifBullet::ragdollNativeSceneBodyTargetPose(osg::Matrixf::identity(), body), std::invalid_argument);
        EXPECT_TRUE(std::isnan(body.mTranslation.x()));
        body.mTranslation = {1, 2, 3};
        body.mRotation = osg::Quat(0, 0, 0, 0);
        EXPECT_THROW(NifBullet::ragdollNativeSceneBodyTargetPose(osg::Matrixf::identity(), body), std::invalid_argument);
        body.mRotation = osg::Quat(0, 0, 0, std::numeric_limits<float>::infinity());
        EXPECT_THROW(NifBullet::ragdollNativeSceneBodyTargetPose(osg::Matrixf::identity(), body), std::invalid_argument);
        EXPECT_EQ(body.mTranslation, osg::Vec3f(1, 2, 3));
        body.mRotation = osg::Quat(0, 0, 0, 1);
        EXPECT_THROW(NifBullet::ragdollNativeSceneBodyTargetPose(osg::Matrixf::scale(2, 2, 2), body),
            std::invalid_argument);
        body.mTranslation.x() = std::numeric_limits<float>::max();
        EXPECT_THROW(NifBullet::ragdollNativeSceneBodyTargetPose(
            osg::Matrixf::translate(std::numeric_limits<float>::max(), 0, 0), body), std::invalid_argument);
    }

    TEST(RagdollConeCoordinates, ParallelAndOppositeTwistsUseNativeRowAdmissionAndFallback)
    {
        const NifBullet::RagdollJointFrame a{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
        auto b = a;
        auto rows = NifBullet::ragdollConeCoordinates(coneLimits(), a, b);
        ASSERT_EQ(rows.size(), 2);
        EXPECT_EQ(rows[0].mAxis, osg::Vec3f(0, 0, 1));
        EXPECT_FLOAT_EQ(rows[0].mAngle, 0);
        EXPECT_FLOAT_EQ(rows[0].mMin, -0.2f);
        EXPECT_FLOAT_EQ(rows[0].mMax, 0.3f);
        EXPECT_EQ(rows[1].mAxis, osg::Vec3f(1, 0, 0));
        b.mAxis = {-1, 0, 0};
        rows = NifBullet::ragdollConeCoordinates(coneLimits(), a, b);
        ASSERT_EQ(rows.size(), 2);
        EXPECT_EQ(rows[1].mAxis, osg::Vec3f(-1, 0, 0));
        EXPECT_FLOAT_EQ(rows[1].mAngle, 0);
    }

    TEST(RagdollConeCoordinates, PreservesIndependentAsymmetricLimitsAndOriginalAnglePolynomial)
    {
        // Original full builder corpus case 4, generated before this helper.
        const NifBullet::RagdollJointFrame a{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
        const NifBullet::RagdollJointFrame b{{0, 0, 0},
            {0.5849835872650146f, -0.49272486567497253f, 0.6442176699638367f},
            {0.8101469278335571f, 0.31762266159057617f, -0.49272486567497253f}};
        const auto rows = NifBullet::ragdollConeCoordinates(coneLimits(), a, b);
        ASSERT_EQ(rows.size(), 3);
        EXPECT_FLOAT_EQ(rows[0].mAngle, -0.9476068019866943f);
        EXPECT_FLOAT_EQ(rows[1].mAngle, 0.946022629737854f);
        EXPECT_FLOAT_EQ(rows[2].mAngle, 1.0299757719039917f);
        EXPECT_FLOAT_EQ(rows[0].mMin, -0.5f);
        EXPECT_FLOAT_EQ(rows[0].mMax, 100);
        EXPECT_FLOAT_EQ(rows[2].mMin, -0.4f);
        EXPECT_FLOAT_EQ(rows[2].mMax, 0.6f);
    }

    TEST(RagdollConeCoordinates, RejectsNonfiniteAndInvalidFramesOrLimits)
    {
        NifBullet::RagdollJointFrame a{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
        auto b = a;
        auto limits = coneLimits();
        b.mAxis.x() = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(NifBullet::ragdollConeCoordinates(limits, a, b), std::invalid_argument);
        b = a;
        limits.mTwistMin = 1;
        EXPECT_THROW(NifBullet::ragdollConeCoordinates(limits, a, b), std::invalid_argument);
        limits = coneLimits();
        a.mAxis = {0, 0, 0};
        EXPECT_THROW(NifBullet::ragdollConeCoordinates(limits, a, b), std::invalid_argument);
    }

    TEST(RagdollPositionUnits, UsesSeparateNativeConstantsAndPreservesSignedZero)
    {
        const auto native = NifBullet::ragdollWorldToNativePosition({1, -1, -0.0f});
        EXPECT_EQ(std::bit_cast<std::uint32_t>(native.x()), 1041387079u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(native.y()), 1041387079u | 0x80000000u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(native.z()), 0x80000000u);
        const auto world = NifBullet::ragdollNativeToWorldPosition({1, -1, -0.0f});
        EXPECT_EQ(std::bit_cast<std::uint32_t>(world.x()), 1088419875u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(world.y()), 1088419875u | 0x80000000u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(world.z()), 0x80000000u);
    }

    TEST(RagdollPositionUnits, RejectsNonfinitePositionsAndWorldConversionOverflow)
    {
        const auto invalid = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(NifBullet::ragdollWorldToNativePosition({0, invalid, 0}), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeToWorldPosition({0, invalid, 0}), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeToWorldPosition({0, std::numeric_limits<float>::max(), 0}),
            std::invalid_argument);
    }

    struct ActorRagdollPhysicsTest : ::testing::Test
    {
        btDefaultCollisionConfiguration mConfiguration;
        btCollisionDispatcher mDispatcher{ &mConfiguration };
        btDbvtBroadphase mBroadphase;
        btSequentialImpulseConstraintSolver mSolver;
        btDiscreteDynamicsWorld mWorld{ &mDispatcher, &mBroadphase, &mSolver, &mConfiguration };
        NifBullet::ActorRagdollDefinition mGraph;
        std::vector<btTransform> mPoses;

        ActorRagdollPhysicsTest()
        {
            mWorld.setGravity(btVector3(0, 0, 0));
            NifBullet::RagdollBodyDefinition body{};
            body.mRecord = 12;
            body.mBone = "Pelvis";
            body.mMass = 2;
            body.mInertia = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            body.mLinearDamping = 2;
            body.mAngularDamping = 2;
            body.mFriction = 0.6f;
            body.mRestitution = 0.2f;
            body.mShape = NifBullet::RagdollSphere{0.5f};
            mGraph.mBodies.push_back(body);
            mPoses.push_back(btTransform(btQuaternion::getIdentity(), btVector3(0, 0, 2)));
        }
        void addHinge()
        {
            auto body = mGraph.mBodies.front();
            body.mRecord = 24;
            body.mBone = "Spine";
            mGraph.mBodies.push_back(body);
            mPoses.push_back(btTransform(btQuaternion::getIdentity(), btVector3(0, 0, 1)));
            NifBullet::RagdollHingeJoint hinge{};
            hinge.mA = {{0, 0, -0.5f}, {1, 0, 0}, {0, 1, 0}};
            hinge.mB = {{0, 0, 0.5f}, {1, 0, 0}, {0, 1, 0}};
            hinge.mMin = -0.2f;
            hinge.mMax = 1.2f;
            NifBullet::RagdollJointDefinition joint{};
            joint.mRecord = 25;
            joint.mBodyA = 0;
            joint.mBodyB = 1;
            joint.mJoint = hinge;
            mGraph.mJoints.push_back(joint);
        }
    };

    TEST_F(ActorRagdollPhysicsTest, NativeKeyframedMotionIsExcludedFromDynamicVelocityIntegration)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.setNativeMotionModes(key);
        body->setLinearVelocity(btVector3(1, 2, 3));
        body->setAngularVelocity(btVector3(4, 5, 6));
        actor.applyNativeDamping(.25f);
        const std::array<osg::Vec3f, 1> deltas{{{7, 8, 9}}};
        actor.applyNativeVelocityStep(.25f, deltas);
        EXPECT_EQ(body->getLinearVelocity(), btVector3(1, 2, 3));
        EXPECT_EQ(body->getAngularVelocity(), btVector3(4, 5, 6));
        const std::array<NifBullet::RagdollNativeVelocityDrive, 1> drives{{{12, {{4, 5, 6}, {0, 0, 0, 1}}, 1.f}}};
        EXPECT_THROW(actor.driveNativePoseVelocities(drives, 1.f, 0.f), std::invalid_argument);
        EXPECT_EQ(body->getLinearVelocity(), btVector3(1, 2, 3));
        EXPECT_EQ(body->getAngularVelocity(), btVector3(4, 5, 6));
    }

    TEST_F(ActorRagdollPhysicsTest, NativeMotionHandoffKeepsCurrentPoseVelocitiesAndDynamicMassInertia)
    {
        mGraph.mBodies[0].mCenter = {1, -2, 3};
        mGraph.mBodies[0].mInertia = {2, .5f, 0, .5f, 3, 0, 0, 0, 4};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 2.5f, mPoses, 1, -1);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        const auto inverseMass = body->getInvMass();
        const auto inverseInertia = body->getInvInertiaDiagLocal();
        body->setLinearVelocity(btVector3(1, 2, 3));
        body->setAngularVelocity(btVector3(4, 5, 6));
        body->applyCentralForce(btVector3(7, 8, 9));
        const auto before = actor.capture()[0];
        const auto interpolation = body->getInterpolationWorldTransform();
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.setNativeMotionModes(key);
        EXPECT_EQ(actor.captureNativeMotionModes(), std::vector<NifBullet::RagdollNativeMotionRequest>(key.begin(), key.end()));
        EXPECT_TRUE(body->isKinematicObject());
        EXPECT_FALSE(body->isStaticObject());
        EXPECT_EQ(body->getInvMass(), 0);
        EXPECT_EQ(body->getInvInertiaDiagLocal(), btVector3(0, 0, 0));
        EXPECT_EQ(actor.capture()[0].mPose, before.mPose);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, before.mLinearVelocity);
        EXPECT_EQ(actor.capture()[0].mAngularVelocity, before.mAngularVelocity);
        EXPECT_EQ(body->getInterpolationWorldTransform(), interpolation);
        EXPECT_EQ(body->getTotalForce(), btVector3(7, 8, 9));
        auto changed = body->getWorldTransform();
        changed.setOrigin(btVector3(11, 12, 13));
        changed.setRotation(btQuaternion(btVector3(0, 0, 1), .7));
        body->setCenterOfMassTransform(changed);
        body->setLinearVelocity(btVector3(-1, -2, -3));
        body->setAngularVelocity(btVector3(-4, -5, -6));
        const auto current = actor.capture()[0];
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> dynamic{{{12, NifBullet::RagdollNativeMotion::Dynamic}}};
        actor.setNativeMotionModes(dynamic);
        EXPECT_FALSE(body->isKinematicObject());
        EXPECT_FALSE(body->isStaticObject());
        EXPECT_EQ(body->getInvMass(), inverseMass);
        EXPECT_EQ(body->getInvInertiaDiagLocal(), inverseInertia);
        EXPECT_EQ(actor.capture()[0].mPose, current.mPose);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, current.mLinearVelocity);
        EXPECT_EQ(actor.capture()[0].mAngularVelocity, current.mAngularVelocity);
        EXPECT_EQ(body->getTotalForce(), btVector3(7, 8, 9));
    }

    TEST_F(ActorRagdollPhysicsTest, NativeKeyframedMotionIgnoresImpulseAndRestoresDynamicResponse)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.setNativeMotionModes(key);
        const auto initial = actor.capture()[0];
        actor.applyImpulse(0, btVector3(4, 0, 0), mPoses[0].getOrigin());
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(0, 0, 0));
        mWorld.stepSimulation(.1, 0);
        EXPECT_EQ(actor.capture()[0].mPose, initial.mPose);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> dynamic{{{12, NifBullet::RagdollNativeMotion::Dynamic}}};
        actor.setNativeMotionModes(dynamic);
        actor.applyImpulse(0, btVector3(4, 0, 0), mPoses[0].getOrigin());
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(2, 0, 0));
        mWorld.stepSimulation(.1, 0);
        EXPECT_GT(actor.capture()[0].mPose.getOrigin().x(), initial.mPose.getOrigin().x());
    }

    TEST_F(ActorRagdollPhysicsTest, NativeMotionBatchValidationPreservesModesAndUnselectedSleepingBodies)
    {
        addHinge();
        mGraph.mJoints.clear();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        auto* second = btRigidBody::upcast(actor.collisionObjects()[1]);
        first->setActivationState(ISLAND_SLEEPING);
        second->setActivationState(ISLAND_SLEEPING);
        std::array<NifBullet::RagdollNativeMotionRequest, 2> requests{{
            {12, NifBullet::RagdollNativeMotion::Keyframed}, {999, NifBullet::RagdollNativeMotion::Keyframed}}};
        EXPECT_THROW(actor.setNativeMotionModes(requests), std::invalid_argument);
        requests[1].mRecord = 12;
        EXPECT_THROW(actor.setNativeMotionModes(requests), std::invalid_argument);
        requests[1].mRecord = 24;
        requests[1].mMotion = static_cast<NifBullet::RagdollNativeMotion>(0);
        EXPECT_THROW(actor.setNativeMotionModes(requests), std::invalid_argument);
        EXPECT_EQ(first->getInvMass(), .5);
        EXPECT_EQ(second->getInvMass(), .5);
        EXPECT_FALSE(first->isActive());
        EXPECT_FALSE(second->isActive());
        EXPECT_FALSE(first->isKinematicObject());
        EXPECT_FALSE(second->isKinematicObject());
        requests[1].mMotion = NifBullet::RagdollNativeMotion::Keyframed;
        actor.setNativeMotionModes(std::span(requests).subspan(1));
        actor.setNativeMotionModes({});
        EXPECT_FALSE(first->isActive());
        EXPECT_TRUE(second->isKinematicObject());
        EXPECT_EQ(actor.captureNativeMotionModes(), (std::vector<NifBullet::RagdollNativeMotionRequest>{
            {12, NifBullet::RagdollNativeMotion::Dynamic}, {24, NifBullet::RagdollNativeMotion::Keyframed}}));
    }

    TEST_F(ActorRagdollPhysicsTest, NativeMotionReturnWakesConnectedBonesAndKeepsGraphIdentity)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        auto* second = btRigidBody::upcast(actor.collisionObjects()[1]);
        auto* constraint = mWorld.getConstraint(0);
        auto* shape = first->getCollisionShape();
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> dynamic{{{12, NifBullet::RagdollNativeMotion::Dynamic}}};
        for (unsigned cycle = 0; cycle < 128; ++cycle)
        {
            actor.setNativeMotionModes(key);
            first->forceActivationState(ISLAND_SLEEPING);
            second->forceActivationState(ISLAND_SLEEPING);
            actor.setNativeMotionModes(dynamic);
            EXPECT_TRUE(first->isActive());
            EXPECT_TRUE(second->isActive());
            EXPECT_EQ(actor.collisionObjects()[0], first);
            EXPECT_EQ(actor.collisionObjects()[1], second);
            EXPECT_EQ(first->getCollisionShape(), shape);
            EXPECT_EQ(mWorld.getConstraint(0), constraint);
            EXPECT_EQ(mWorld.getNumCollisionObjects(), 2);
            EXPECT_EQ(mWorld.getNumConstraints(), 1);
        }
        first->forceActivationState(ISLAND_SLEEPING);
        second->forceActivationState(ISLAND_SLEEPING);
        actor.setNativeMotionModes(dynamic);
        EXPECT_FALSE(first->isActive());
        EXPECT_FALSE(second->isActive());
    }

    TEST_F(ActorRagdollPhysicsTest, NativeMotionRestoresPrincipalInertiaForOffCenterImpulse)
    {
        mGraph.mBodies[0].mCenter = {1, -2, 3};
        mGraph.mBodies[0].mInertia = {2, .5f, 0, .5f, 3, 0, 0, 0, 4};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 2.5f, mPoses, 1, -1);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        const auto inverseMass = body->getInvMass();
        const auto inverseInertia = body->getInvInertiaDiagLocal();
        actor.applyImpulse(0, btVector3(1, 2, 3), btVector3(4, 5, 6));
        const auto expected = actor.capture()[0];
        body->setLinearVelocity(btVector3(0, 0, 0));
        body->setAngularVelocity(btVector3(0, 0, 0));
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> dynamic{{{12, NifBullet::RagdollNativeMotion::Dynamic}}};
        actor.setNativeMotionModes(key);
        EXPECT_EQ(body->getInvMass(), 0);
        actor.setNativeMotionModes(dynamic);
        EXPECT_EQ(body->getInvMass(), inverseMass);
        EXPECT_EQ(body->getInvInertiaDiagLocal(), inverseInertia);
        actor.applyImpulse(0, btVector3(1, 2, 3), btVector3(4, 5, 6));
        EXPECT_EQ(actor.capture()[0].mPose, expected.mPose);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, expected.mLinearVelocity);
        EXPECT_EQ(actor.capture()[0].mAngularVelocity, expected.mAngularVelocity);
    }

    TEST_F(ActorRagdollPhysicsTest, OwnsLiveBodiesAndAppliesImpulseThroughDynamics)
    {
        {
            NifBullet::ActorRagdollPhysics body(mGraph, mWorld, 1, mPoses, 1, -1);
            ASSERT_EQ(mWorld.getNumCollisionObjects(), 1);
            body.applyImpulse(0, btVector3(2, 0, 0), btVector3(0, 0, 2));
            for (unsigned i = 0; i < 6; ++i)
                mWorld.stepSimulation(btScalar(1) / 60, 0);
            const auto state = body.capture();
            ASSERT_EQ(state.size(), 1);
            EXPECT_EQ(state[0].mRecord, 12);
            // Factual Newtonian bridge expectation: impulse 2 / mass 2 =
            // velocity 1, integrated for 0.1 seconds without gravity/damping.
            EXPECT_NEAR(state[0].mPose.getOrigin().x(), 0.1, 1e-6);
            EXPECT_NEAR(state[0].mLinearVelocity.x(), 1, 1e-6);
        }
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        EXPECT_EQ(mWorld.getNumConstraints(), 0);
    }
    TEST_F(ActorRagdollPhysicsTest, ConstrainsTwoBodiesAndConservesLinearMomentum)
    {
        addHinge();
        {
            NifBullet::ActorRagdollPhysics body(mGraph, mWorld, 1, mPoses, 1, -1);
            ASSERT_EQ(mWorld.getNumCollisionObjects(), 2);
            ASSERT_EQ(mWorld.getNumConstraints(), 1);
            body.applyImpulse(1, btVector3(0, 2, 0), btVector3(0, 0, 1));
            for (unsigned i = 0; i < 120; ++i)
                mWorld.stepSimulation(btScalar(1) / 120, 0);
            const auto states = body.capture();
            ASSERT_EQ(states.size(), 2);
            const auto pivotA = states[0].mPose * btVector3(0, 0, -0.5);
            const auto pivotB = states[1].mPose * btVector3(0, 0, 0.5);
            EXPECT_LT((pivotA - pivotB).length(), 0.01);
            EXPECT_NEAR(2 * (states[0].mLinearVelocity.y() + states[1].mLinearVelocity.y()), 2, 1e-6);
            EXPECT_GT(std::abs(states[1].mPose.getOrigin().y()), 0.01);
        }
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        EXPECT_EQ(mWorld.getNumConstraints(), 0);
    }

    TEST_F(ActorRagdollPhysicsTest, RestoreValidatesAllBodiesBeforeChangingAny)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics body(mGraph, mWorld, 1, mPoses, 1, -1);
        auto states = body.capture();
        states[0].mPose.setOrigin(btVector3(20, 30, 40));
        states[1].mRecord = 999;
        EXPECT_THROW(body.restore(states), std::invalid_argument);
        EXPECT_EQ(body.capture()[0].mPose.getOrigin(), btVector3(0, 0, 2));
        states[1].mRecord = 24;
        states[1].mLinearVelocity.setX(std::numeric_limits<btScalar>::quiet_NaN());
        EXPECT_THROW(body.restore(states), std::invalid_argument);
        EXPECT_EQ(body.capture()[0].mPose.getOrigin(), btVector3(0, 0, 2));
        states[1].mLinearVelocity.setX(1);
        body.restore(states);
        EXPECT_EQ(body.capture()[0].mPose.getOrigin(), btVector3(20, 30, 40));
        EXPECT_EQ(body.capture()[1].mLinearVelocity.x(), 1);
    }

    TEST_F(ActorRagdollPhysicsTest, PreservesShapePoseAcrossCenterAndPrincipalInertiaConversion)
    {
        mGraph.mBodies[0].mCenter = {0.1f, 0.2f, 0.3f};
        mGraph.mBodies[0].mInertia = {2, 0.1f, 0, 0.1f, 3, 0, 0, 0, 4};
        NifBullet::ActorRagdollPhysics body(mGraph, mWorld, 7, mPoses, 1, -1);
        const auto state = body.capture();
        EXPECT_LT((state[0].mPose.getOrigin() - mPoses[0].getOrigin()).length(), 1e-6);
        EXPECT_NEAR(state[0].mPose.getRotation().w(), 1, 1e-6);
    }

    TEST_F(ActorRagdollPhysicsTest, BindsCurrentWorldBonePoseInsteadOfAuthoredBodyInfo)
    {
        mGraph.mBodies[0].mNodeRecord = 8;
        mGraph.mBodies[0].mTranslation = {1000, 2000, 3000};
        mGraph.mBodies[0].mRotation = osg::Quat(0.7, osg::Vec3f(1, 0, 0));
        mGraph.mBodies[0].mBoneBind = osg::Matrixf::translate(500, 600, 700);
        const NifBullet::RagdollBoneWorldPose bone{8,
            osg::Matrixf::rotate(0.5, osg::Vec3f(0, 0, 1)) * osg::Matrixf::translate(10, 20, 30)};
        const auto poses = NifBullet::ragdollBodyWorldPoses(mGraph, std::span(&bone, 1));
        ASSERT_EQ(poses.size(), 1);
        const auto native = NifBullet::ragdollWorldToNativePosition({10, 20, 30});
        for (int axis = 0; axis < 3; ++axis)
            EXPECT_EQ(poses[0].getOrigin()[axis], btScalar(native[axis]) * NifBullet::RagdollNativeLengthScale);
        EXPECT_NEAR(poses[0].getRotation().z(), std::sin(0.25), 2e-6);
        EXPECT_NEAR(poses[0].getRotation().w(), std::cos(0.25), 2e-6);
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, NifBullet::RagdollNativeLengthScale, poses, 1, -1);
        const auto actual = actor.capture();
        EXPECT_LT((actual[0].mPose.getOrigin() - poses[0].getOrigin()).length(), 1e-10);
    }

    TEST_F(ActorRagdollPhysicsTest, BodyTGraphPreparesPhysicalPoseFromSceneBone)
    {
        auto& body = mGraph.mBodies[0];
        body.mNodeRecord = 8;
        body.mUsesRigidBodyTransform = true;
        body.mTranslation = {2, 3, 4};
        body.mRotation = osg::Quat(0, 0, 1, 0);
        const NifBullet::RagdollBoneWorldPose bone{8, osg::Matrixf::identity()};
        const auto poses = NifBullet::ragdollBodyWorldPoses(mGraph, std::span(&bone, 1));
        ASSERT_EQ(poses.size(), 1);
        const btScalar scale = NifBullet::RagdollNativeLengthScale;
        EXPECT_EQ(poses[0].getOrigin(), btVector3(2 * scale, 3 * scale, 4 * scale));
        EXPECT_EQ(poses[0].getRotation(), btQuaternion(0, 0, 1, 0));
        EXPECT_EQ(bone.mPose, osg::Matrixf::identity());
        EXPECT_EQ(body.mTranslation, osg::Vec3f(2, 3, 4));
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, NifBullet::RagdollNativeLengthScale, poses, 1, -1);
        EXPECT_EQ(actor.capture()[0].mPose, poses[0]);
    }

    TEST_F(ActorRagdollPhysicsTest, RejectsIncompleteAmbiguousAndMalformedBoneBindings)
    {
        mGraph.mBodies[0].mNodeRecord = 8;
        const NifBullet::RagdollBoneWorldPose bone{8, osg::Matrixf::identity()};
        EXPECT_THROW(NifBullet::ragdollBodyWorldPoses(mGraph, {}), std::invalid_argument);
        auto wrong = bone;
        wrong.mNodeRecord = 9;
        EXPECT_THROW(NifBullet::ragdollBodyWorldPoses(mGraph, std::span(&wrong, 1)), std::invalid_argument);
        const std::array duplicates{bone, bone};
        EXPECT_THROW(NifBullet::ragdollBodyWorldPoses(mGraph, duplicates), std::invalid_argument);
        mGraph.mBodies[0].mUsesRigidBodyTransform = true;
        mGraph.mBodies[0].mRotation = osg::Quat(0, 0, 0, 0);
        EXPECT_THROW(NifBullet::ragdollBodyWorldPoses(mGraph, std::span(&bone, 1)), std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST(RagdollBonePose, RejectsNonfiniteScaleShearReflectionAndProjectiveTransforms)
    {
        auto matrix = osg::Matrixf::identity();
        matrix(3, 0) = std::numeric_limits<float>::infinity();
        EXPECT_THROW(NifBullet::ragdollNativePoseFromBoneWorld(matrix), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativePoseFromBoneWorld(osg::Matrixf::scale(2, 2, 2)), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativePoseFromBoneWorld(osg::Matrixf::scale(-1, 1, 1)), std::invalid_argument);
        matrix.makeIdentity();
        matrix(1, 0) = 0.2f;
        EXPECT_THROW(NifBullet::ragdollNativePoseFromBoneWorld(matrix), std::invalid_argument);
        matrix.makeIdentity();
        matrix(0, 3) = 0.2f;
        EXPECT_THROW(NifBullet::ragdollNativePoseFromBoneWorld(matrix), std::invalid_argument);
    }

    TEST_F(ActorRagdollPhysicsTest, AngularDampingDoesNotConvertRadiansAsLengths)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7, mPoses, 1, -1);
        auto state = actor.capture();
        state[0].mAngularVelocity = {1, -1, 0};
        actor.restore(state);
        actor.applyNativeDamping(0.25f);
        const auto after = actor.capture();
        // Original sphere-motion stores from the independent damping corpus:
        // angular units stay radians/time even when body lengths are scaled.
        EXPECT_EQ(after[0].mAngularVelocity.x(), btScalar(0.5));
        EXPECT_EQ(after[0].mAngularVelocity.y(), btScalar(-0.5));
        EXPECT_EQ(after[0].mAngularVelocity.z(), btScalar(0));
    }

    TEST_F(ActorRagdollPhysicsTest, AppliesNativeDampingAboveOneWithoutBulletClamping)
    {
        NifBullet::ActorRagdollPhysics body(mGraph, mWorld, 1, mPoses, 1, -1);
        auto states = body.capture();
        states[0].mLinearVelocity = btVector3(10, 20, 30);
        states[0].mAngularVelocity = btVector3(1, 2, 3);
        body.restore(states);
        body.applyNativeDamping(0.25f);
        EXPECT_EQ(body.capture()[0].mLinearVelocity, btVector3(5, 10, 15));
        EXPECT_EQ(body.capture()[0].mAngularVelocity, btVector3(0.5, 1, 1.5));
        body.applyNativeDamping(1);
        EXPECT_EQ(body.capture()[0].mLinearVelocity, btVector3(0, 0, 0));
        EXPECT_THROW(body.applyNativeDamping(-1), std::invalid_argument);
        EXPECT_THROW(body.applyImpulse(4, btVector3(0, 0, 0), btVector3(0, 0, 0)), std::invalid_argument);
    }

    TEST_F(ActorRagdollPhysicsTest, RejectsInvalidMalleabilityAndMalformedConeBeforeWorldPublication)
    {
        addHinge();
        mGraph.mJoints[0].mMalleable = true;
        mGraph.mJoints[0].mTau = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW((NifBullet::ActorRagdollPhysics(mGraph, mWorld, 1, mPoses, 1, -1)), std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        EXPECT_EQ(mWorld.getNumConstraints(), 0);
        mGraph.mJoints[0].mMalleable = false;
        mGraph.mJoints[0].mJoint = NifBullet::RagdollConeJoint{};
        EXPECT_THROW((NifBullet::ActorRagdollPhysics(mGraph, mWorld, 1, mPoses, 1, -1)), std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_F(ActorRagdollPhysicsTest, EnforcesIndependentConePlaneAndTwistLimitsInLiveDynamics)
    {
        auto definition = mGraph.mBodies.front();
        definition.mRecord = 24;
        mGraph.mBodies.push_back(definition);
        mPoses.push_back(mPoses.front());
        auto cone = coneLimits();
        cone.mA = cone.mB = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
        NifBullet::RagdollJointDefinition joint{};
        joint.mRecord = 25;
        joint.mBodyA = 0;
        joint.mBodyB = 1;
        joint.mJoint = cone;
        mGraph.mJoints.push_back(joint);
        for (const auto& axis : {btVector3(1, 0, 0), btVector3(0, 1, 0), btVector3(0, 0, 1)})
        {
            for (btScalar angle : {btScalar(-1), btScalar(1)})
            {
                NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
                auto state = actor.capture();
                state[0].mPose.setRotation(btQuaternion(axis, angle));
                state[0].mAngularVelocity = axis;
                actor.restore(state);
                for (unsigned step = 0; step < 240; ++step)
                    mWorld.stepSimulation(btScalar(1) / 120, 0);
                state = actor.capture();
                const auto frame = [](const btTransform& pose) {
                    const auto a = pose.getBasis() * btVector3(1, 0, 0);
                    const auto p = pose.getBasis() * btVector3(0, 1, 0);
                    return NifBullet::RagdollJointFrame{{0, 0, 0},
                        {float(a.x()), float(a.y()), float(a.z())},
                        {float(p.x()), float(p.y()), float(p.z())}};
                };
                const auto rows = NifBullet::ragdollConeCoordinates(cone, frame(state[0].mPose), frame(state[1].mPose));
                for (const auto& row : rows)
                {
                    EXPECT_GE(row.mAngle, row.mMin - 0.003f);
                    EXPECT_LE(row.mAngle, row.mMax + 0.003f);
                }
                EXPECT_LT((state[0].mPose.getOrigin() - state[1].mPose.getOrigin()).length(), 1e-6);
                EXPECT_LT((state[0].mAngularVelocity + state[1].mAngularVelocity - axis).length(), 1e-6);
            }
        }
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        EXPECT_EQ(mWorld.getNumConstraints(), 0);
    }

    TEST_F(ActorRagdollPhysicsTest, JointFrictionTransfersBoundedImpulseAndStopsRelativeRotation)
    {
        addHinge();
        mPoses[1] = mPoses[0];
        auto& hinge = std::get<NifBullet::RagdollHingeJoint>(mGraph.mJoints[0].mJoint);
        hinge.mA.mPivot = hinge.mB.mPivot = {0, 0, 0};
        hinge.mFriction = 2;
        for (bool cone : {false, true})
        {
            if (cone)
            {
                auto limits = coneLimits();
                limits.mA = hinge.mA;
                limits.mB = hinge.mB;
                limits.mFriction = 2;
                mGraph.mJoints[0].mJoint = limits;
            }
            NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
            auto states = actor.capture();
            states[0].mAngularVelocity = btVector3(1, 0, 0);
            actor.restore(states);
            mWorld.stepSimulation(btScalar(0.01), 0);
            states = actor.capture();
            // Unit angular inertia: the original torque*dt cap is .02.
            EXPECT_NEAR(states[0].mAngularVelocity.x(), 0.98, 1e-6);
            EXPECT_NEAR(states[1].mAngularVelocity.x(), 0.02, 1e-6);
            for (unsigned step = 0; step < 120; ++step)
                mWorld.stepSimulation(btScalar(0.01), 0);
            states = actor.capture();
            EXPECT_LT((states[0].mAngularVelocity - states[1].mAngularVelocity).length(), 1e-6);
            EXPECT_NEAR(states[0].mAngularVelocity.x() + states[1].mAngularVelocity.x(), 1, 1e-6);
        }
    }

    TEST(ActorRagdollFriction, NativeFloatStorePrecedesWorldTorqueUnitConversion)
    {
        const float duration = 1.0f / 60;
        // Original full builder friction schema store, duration1/60/torque2.
        const float stored = std::bit_cast<float>(std::uint32_t(0x3d088889));
        EXPECT_EQ(NifBullet::ragdollFrictionImpulse(2, duration, 1), btScalar(stored));
        EXPECT_EQ(NifBullet::ragdollFrictionImpulse(2, duration, 7), btScalar(stored) * 49);
        EXPECT_EQ(NifBullet::ragdollFrictionImpulse(2, 0, 7), 0);
        EXPECT_THROW(NifBullet::ragdollFrictionImpulse(-1, duration, 1), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollFrictionImpulse(2, -1, 1), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollFrictionImpulse(2, duration, 0), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollFrictionImpulse(std::numeric_limits<float>::infinity(), duration, 1),
            std::invalid_argument);
    }

    TEST_F(ActorRagdollPhysicsTest, ZeroJointFrictionAllowsRotationInsideAngularLimits)
    {
        addHinge();
        mPoses[1] = mPoses[0];
        auto cone = coneLimits();
        cone.mA = cone.mB = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
        mGraph.mJoints[0].mJoint = cone;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto states = actor.capture();
        states[0].mAngularVelocity = btVector3(0.1, 0, 0);
        actor.restore(states);
        for (unsigned step = 0; step < 10; ++step)
            mWorld.stepSimulation(btScalar(0.01), 0);
        states = actor.capture();
        EXPECT_NEAR(states[0].mAngularVelocity.x(), 0.1, 1e-12);
        EXPECT_NEAR(states[1].mAngularVelocity.x(), 0, 1e-12);
        EXPECT_GT(states[0].mPose.getRotation().x(), 0.004);
    }

    TEST_F(ActorRagdollPhysicsTest, MalleableCoefficientsReplaceSolverDefaultsForTheirOwnJoint)
    {
        addHinge();
        mPoses[1] = mPoses[0];
        auto cone = coneLimits();
        cone.mA = cone.mB = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
        mGraph.mJoints[0].mJoint = cone;
        mGraph.mJoints[0].mMalleable = true;
        mGraph.mJoints[0].mTau = 0.5f;
        mGraph.mJoints[0].mDamping = 0.5f;
        mWorld.getSolverInfo().m_numIterations = 1;
        const auto originalErp = mWorld.getSolverInfo().m_erp;
        const auto originalDamping = mWorld.getSolverInfo().m_damping;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto states = actor.capture();
        states[0].mLinearVelocity = btVector3(1, 0, 0);
        actor.restore(states);
        mWorld.stepSimulation(btScalar(1) / 60, 0);
        states = actor.capture();
        // Full original wrapped builder and single sweep9202a0: .75 and
        // .2499999701976776, with its inverse-mass epsilon retained.
        EXPECT_NEAR(states[0].mLinearVelocity.x(), 0.75, 2e-6);
        EXPECT_NEAR(states[1].mLinearVelocity.x(), 0.2499999701976776, 2e-6);
        EXPECT_EQ(mWorld.getSolverInfo().m_erp, originalErp);
        EXPECT_EQ(mWorld.getSolverInfo().m_damping, originalDamping);
    }

    TEST_F(ActorRagdollPhysicsTest, FallsAndSettlesAgainstActualWorldCollision)
    {
        mWorld.setGravity(btVector3(0, 0, -10));
        btBoxShape groundShape(btVector3(20, 20, 0.5));
        btRigidBody::btRigidBodyConstructionInfo groundInfo(0, nullptr, &groundShape);
        groundInfo.m_startWorldTransform = btTransform(btQuaternion::getIdentity(), btVector3(0, 0, -0.5));
        btRigidBody ground(groundInfo);
        mWorld.addRigidBody(&ground);
        {
            NifBullet::ActorRagdollPhysics body(mGraph, mWorld, 1, mPoses, 1, -1);
            for (unsigned i = 0; i < 480; ++i)
                mWorld.stepSimulation(btScalar(1) / 120, 0);
            const auto state = body.capture();
            ASSERT_EQ(state.size(), 1);
            EXPECT_NEAR(state[0].mPose.getOrigin().z(), 0.5, 1e-3);
            EXPECT_LT(state[0].mLinearVelocity.length(), 1e-3);
        }
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 1);
        mWorld.removeRigidBody(&ground);
    }

    TEST_F(ActorRagdollPhysicsTest, RejectsInvalidGeometryAndIdentityBeforeWorldPublication)
    {
        EXPECT_THROW((NifBullet::ActorRagdollPhysics(mGraph, mWorld, 0, mPoses, 1, -1)), std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        mGraph.mBodies[0].mInertia[0] = -1;
        EXPECT_THROW((NifBullet::ActorRagdollPhysics(mGraph, mWorld, 1, mPoses, 1, -1)), std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        mGraph.mBodies[0].mInertia[0] = 1;
        addHinge();
        mGraph.mBodies[1].mRecord = 12;
        EXPECT_THROW((NifBullet::ActorRagdollPhysics(mGraph, mWorld, 1, mPoses, 1, -1)), std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

}


namespace
{
    TEST(RagdollNativeVelocityStep, GravityDeltaMatchesOriginalFloatStoresAndSignedZero)
    {
        for (const auto& [dt, expected] : std::array<std::pair<float, std::uint32_t>, 4>{{
                 {0.f, 2147483648u}, {1.f / 120.f, 3206346180u},
                 {0.1f, 3236655269u}, {1.f, 3264423527u}}})
        {
            const auto delta = NifBullet::ragdollNativeGravityDelta(
                {0, 0, NifBullet::RagdollNativeDefaultGravityZ}, dt);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(delta.z()), expected);
            EXPECT_EQ(delta.x(), 0);
            EXPECT_EQ(delta.y(), 0);
        }
        EXPECT_THROW(NifBullet::ragdollNativeGravityDelta({0, 0, 1}, -1), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeGravityDelta({0, 0, std::numeric_limits<float>::infinity()}, 1),
            std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeGravityDelta({0, 0, std::numeric_limits<float>::max()}, 2),
            std::invalid_argument);
    }

    TEST_F(ActorRagdollPhysicsTest, RestoredAnisotropicBodyUsesRotatedInertiaForOffCenterImpulse)
    {
        mGraph.mBodies[0].mInertia = {1, 0, 0, 0, 2, 0, 0, 0, 3};
        mGraph.mBodies[0].mCenter.set(0.2f, 0.3f, 0.4f);
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto state = actor.capture();
        state[0].mPose.setRotation(btQuaternion(btVector3(0, 0, 1), SIMD_HALF_PI));
        state[0].mLinearVelocity = btVector3(2, 3, 4);
        state[0].mAngularVelocity = btVector3(0, 0, 0);
        actor.restore(state);
        const auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        EXPECT_EQ(body->getInterpolationLinearVelocity(), state[0].mLinearVelocity);
        EXPECT_EQ(body->getInterpolationAngularVelocity(), state[0].mAngularVelocity);
        EXPECT_EQ(body->getInterpolationWorldTransform(), body->getWorldTransform());
        // Rotation by 90 degrees about Z puts inertia2 on the world X axis.
        // A unit Z impulse applied one unit along world Y gives unit X torque.
        const auto center = body->getCenterOfMassPosition();
        actor.applyImpulse(0, btVector3(0, 0, 1), center + btVector3(0, 1, 0));
        const auto actual = actor.capture()[0];
        EXPECT_NEAR(actual.mAngularVelocity.x(), 0.5, 1e-12);
        EXPECT_NEAR(actual.mAngularVelocity.y(), 0, 1e-12);
        EXPECT_NEAR(actual.mAngularVelocity.z(), 0, 1e-12);
        EXPECT_EQ(actual.mLinearVelocity, btVector3(2, 3, 4.5));
        EXPECT_NEAR((actual.mPose.getOrigin() - state[0].mPose.getOrigin()).length(), 0, 1e-12);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeVelocityStepPreservesSleepingBodiesUntilImpulse)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        body->setActivationState(ISLAND_SLEEPING);
        const auto initial = actor.capture()[0];
        const std::array<osg::Vec3f, 1> delta{{{0, 0, -1}}};
        actor.applyNativeVelocityStep(0.1f, delta);
        const auto sleeping = actor.capture()[0];
        EXPECT_EQ(sleeping.mPose, initial.mPose);
        EXPECT_EQ(sleeping.mLinearVelocity, initial.mLinearVelocity);
        EXPECT_FALSE(body->isActive());
        actor.applyImpulse(0, btVector3(1, 0, 0), mPoses[0].getOrigin());
        EXPECT_TRUE(body->isActive());
        actor.applyNativeVelocityStep(0.1f, delta);
        EXPECT_LT(actor.capture()[0].mLinearVelocity.z(), 0);
    }

    TEST(RagdollNativeVelocityStep, LoadedLimitsApplyBothOriginalPreparationBranches)
    {
        for (float linear : {0.f, std::nextafter(250.f, 0.f), 250.f,
                 std::nextafter(250.f, 300.f), 10000.f, std::numeric_limits<float>::max()})
            for (float angular : {0.f, std::numeric_limits<float>::denorm_min(), 31.41590118408203f})
            {
                const auto limits = NifBullet::ragdollLoadedMotionLimits(2, 0.1f, linear, angular);
                EXPECT_EQ(limits.mMaxLinearVelocity, 250.f);
                EXPECT_EQ(std::bit_cast<std::uint32_t>(limits.mAngularLimit), std::bit_cast<std::uint32_t>(angular));
                EXPECT_EQ(limits.mLinearDamping, 2);
                EXPECT_EQ(limits.mAngularDamping, 0.1f);
            }
        for (float invalid : {-1.f, std::numeric_limits<float>::infinity(),
                 std::numeric_limits<float>::quiet_NaN()})
        {
            EXPECT_THROW(NifBullet::ragdollLoadedMotionLimits(invalid, 0, 1, 1), std::invalid_argument);
            EXPECT_THROW(NifBullet::ragdollLoadedMotionLimits(0, invalid, 1, 1), std::invalid_argument);
            EXPECT_THROW(NifBullet::ragdollLoadedMotionLimits(0, 0, invalid, 1), std::invalid_argument);
            EXPECT_THROW(NifBullet::ragdollLoadedMotionLimits(0, 0, 1, invalid), std::invalid_argument);
        }
    }

    TEST_F(ActorRagdollPhysicsTest, AppliesLoadedCapsAfterNativeDeltaWithoutChangingPose)
    {
        mGraph.mBodies[0].mMaxLinearVelocity = 10000;
        mGraph.mBodies[0].mMaxAngularVelocity = 0.1f;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7, mPoses, 1, -1);
        auto states = actor.capture();
        states[0].mLinearVelocity = btVector3(7000, 0, 0);
        states[0].mAngularVelocity = btVector3(1, 0, 0);
        actor.restore(states);
        const std::array<osg::Vec3f, 1> delta{{{10, 0, 0}}};
        actor.applyNativeVelocityStep(0.25f, delta);
        const auto actual = actor.capture()[0];
        EXPECT_EQ(actual.mLinearVelocity, btVector3(1750, 0, 0));
        EXPECT_NEAR(actual.mAngularVelocity.x(), 0.31415927, 1e-7);
        EXPECT_EQ(actual.mAngularVelocity.y(), 0);
        EXPECT_EQ(actual.mPose, states[0].mPose);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 1);
    }

    TEST_F(ActorRagdollPhysicsTest, InvalidLaterBodyVelocityDeltaDoesNotPublishEarlierResults)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7, mPoses, 1, -1);
        auto states = actor.capture();
        for (auto& state : states)
        {
            state.mLinearVelocity = btVector3(70, 140, 210);
            state.mAngularVelocity = btVector3(1, 2, 3);
        }
        actor.restore(states);
        std::array<osg::Vec3f, 2> delta{{{1, 2, 3}, {0, 0, std::numeric_limits<float>::infinity()}}};
        EXPECT_THROW(actor.applyNativeVelocityStep(0.25f, delta), std::invalid_argument);
        EXPECT_THROW(actor.applyNativeVelocityStep(0.25f, std::span<const osg::Vec3f>(delta).first(1)),
            std::invalid_argument);
        delta[1].set(0, 0, 0);
        EXPECT_THROW(actor.applyNativeVelocityStep(-1, delta), std::invalid_argument);
        const auto actual = actor.capture();
        for (std::size_t i = 0; i < states.size(); ++i)
        {
            EXPECT_EQ(actual[i].mLinearVelocity, states[i].mLinearVelocity);
            EXPECT_EQ(actual[i].mAngularVelocity, states[i].mAngularVelocity);
            EXPECT_EQ(actual[i].mPose, states[i].mPose);
        }
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 2);
        EXPECT_EQ(mWorld.getNumConstraints(), 1);
    }

    TEST(RagdollNativeVelocityStep, MatchesOriginalGravityDampingAndBothCapStores)
    {
        // Independent full original 8e96c0 corpus cases3918 and3070.
        const NifBullet::RagdollNativeVelocities input{{10, 20, 30}, {10, 20, 30}};
        auto output = NifBullet::ragdollNativeVelocityStep(input, {0, 0, 1, 31.4159f}, 1, {});
        const std::array<std::uint32_t, 3> linear{1049155191, 1057543799, 1062027699};
        const std::array<std::uint32_t, 3> angular{1061253928, 1069642536, 1074861662};
        for (unsigned i = 0; i < 3; ++i)
        {
            EXPECT_EQ(std::bit_cast<std::uint32_t>(output.mLinear[i]), linear[i]);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(output.mAngular[i]), angular[i]);
        }
        output = NifBullet::ragdollNativeVelocityStep(input, {.1f, 0, 10000, .1f}, .5f,
            {0, 0, std::bit_cast<float>(3256034919u)});
        const std::array<std::uint32_t, 3> gravityLinear{1092091904, 1100480512, 3234748175};
        const std::array<std::uint32_t, 3> limitedAngular{1034679446, 1043068054, 1048639344};
        for (unsigned i = 0; i < 3; ++i)
        {
            EXPECT_EQ(std::bit_cast<std::uint32_t>(output.mLinear[i]), gravityLinear[i]);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(output.mAngular[i]), limitedAngular[i]);
        }
    }

    TEST(RagdollNativeVelocityStep, ZeroDurationDoesNotApplyAnAngularCapAndDampingClampsAtZero)
    {
        const NifBullet::RagdollNativeVelocities input{{10, 20, 30}, {10, 20, 30}};
        auto output = NifBullet::ragdollNativeVelocityStep(input, {0, 0, 0, 0}, 0, {});
        EXPECT_EQ(output.mLinear, osg::Vec3f());
        EXPECT_EQ(output.mAngular, input.mAngular);
        output = NifBullet::ragdollNativeVelocityStep(input, {2, 2, 10000, 31.4159f}, 1, {});
        EXPECT_EQ(output.mLinear, osg::Vec3f());
        EXPECT_EQ(output.mAngular, osg::Vec3f());
        const float tiny = std::numeric_limits<float>::denorm_min();
        const NifBullet::RagdollNativeVelocities zeros{{-0.0f, -tiny, tiny}, {-0.0f, -tiny, tiny}};
        output = NifBullet::ragdollNativeVelocityStep(zeros, {0, 0, 10000, 31.4159f}, 1, {});
        for (unsigned i = 0; i < 3; ++i)
            EXPECT_EQ(std::bit_cast<std::uint32_t>(output.mAngular[i]),
                std::bit_cast<std::uint32_t>(zeros.mAngular[i]));
    }

    TEST(RagdollNativeVelocityStep, RejectsInvalidCoefficientsVectorsAndOverflowWithoutChangingInputs)
    {
        const NifBullet::RagdollNativeVelocities input{{10, 20, 30}, {1, 2, 3}};
        for (float bad : {-1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
        {
            EXPECT_THROW(NifBullet::ragdollNativeVelocityStep(input, {0, 0, 1, 1}, bad, {}), std::invalid_argument);
            for (unsigned field = 0; field < 4; ++field)
            {
                NifBullet::RagdollMotionLimits limits{0, 0, 1, 1};
                switch (field)
                {
                    case 0: limits.mLinearDamping = bad; break;
                    case 1: limits.mAngularDamping = bad; break;
                    case 2: limits.mMaxLinearVelocity = bad; break;
                    case 3: limits.mAngularLimit = bad; break;
                }
                EXPECT_THROW(NifBullet::ragdollNativeVelocityStep(input, limits, 1, {}), std::invalid_argument);
            }
        }
        for (unsigned field = 0; field < 3; ++field)
        {
            auto invalid = input;
            osg::Vec3f delta;
            const float nan = std::numeric_limits<float>::quiet_NaN();
            if (field == 0) invalid.mLinear[1] = nan;
            if (field == 1) invalid.mAngular[2] = nan;
            if (field == 2) delta[0] = nan;
            EXPECT_THROW(NifBullet::ragdollNativeVelocityStep(invalid, {0, 0, 1, 1}, 1, delta), std::invalid_argument);
        }
        auto overflowing = input;
        overflowing.mLinear[0] = std::numeric_limits<float>::max();
        EXPECT_THROW(NifBullet::ragdollNativeVelocityStep(overflowing, {0, 0, 1, 1}, 1, {}), std::invalid_argument);
        EXPECT_EQ(input.mLinear, osg::Vec3f(10, 20, 30));
        EXPECT_EQ(input.mAngular, osg::Vec3f(1, 2, 3));
    }
    TEST_F(ActorRagdollPhysicsTest, NativeInternalFiltersControlActualOverlapWithoutBorrowingInput)
    {
        addHinge();
        mGraph.mJoints.clear();
        mPoses[1].setOrigin(mPoses[0].getOrigin() + btVector3(.5, 0, 0));
        mGraph.mBodies[0].mInfoFilter = {8, 2, 0};
        mGraph.mBodies[1].mInfoFilter = {8, 3, 0};
        for (const bool wildcard : {false, true})
        {
            NifBullet::RagdollInternalCollisionFilter filter;
            filter.mSystemGroup = wildcard ? 0 : 10;
            {
                NifBullet::ActorRagdollPhysics physics(mGraph, mWorld, 1, mPoses, 1, -1, nullptr, &filter);
                filter.mMasks.mBoneMasks.fill(0xffffffffu); // Admission already consumed the input.
                const auto objects = physics.collisionObjects();
                EXPECT_EQ(objects[0]->checkCollideWith(objects[1]), wildcard);
                EXPECT_EQ(objects[1]->checkCollideWith(objects[0]), wildcard);
                mWorld.performDiscreteCollisionDetection();
                EXPECT_EQ(mDispatcher.getNumManifolds(), wildcard ? 1 : 0);
            }
            EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
            EXPECT_EQ(mDispatcher.getNumManifolds(), 0);
        }
    }

    TEST_F(ActorRagdollPhysicsTest, ExplicitNativeMasksOwnJointPairFiltering)
    {
        addHinge();
        mGraph.mBodies[0].mInfoFilter = {8, 2, 0};
        mGraph.mBodies[1].mInfoFilter = {8, 6, 0};
        NifBullet::RagdollInternalCollisionFilter filter;
        filter.mSystemGroup = 10;
        NifBullet::ActorRagdollPhysics physics(mGraph, mWorld, 1, mPoses, 1, -1, nullptr, &filter);
        ASSERT_EQ(mWorld.getNumConstraints(), 1);
        const auto objects = physics.collisionObjects();
        EXPECT_TRUE(objects[0]->checkCollideWith(objects[1]));
        EXPECT_TRUE(objects[1]->checkCollideWith(objects[0]));
    }

    TEST_F(ActorRagdollPhysicsTest, RejectsInvalidOrAsymmetricFiltersBeforePublishingBodies)
    {
        addHinge();
        NifBullet::RagdollInternalCollisionFilter filter;
        filter.mSystemGroup = 10;
        mGraph.mBodies[0].mInfoFilter = {8, 2, 0};
        mGraph.mBodies[1].mInfoFilter = {32, 3, 0};
        EXPECT_THROW(NifBullet::ActorRagdollPhysics(mGraph, mWorld, 1, mPoses, 1, -1, nullptr, &filter),
            std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        EXPECT_EQ(mWorld.getNumConstraints(), 0);
        mGraph.mBodies[1].mInfoFilter = {8, 6, 0};
        filter.mMasks.mBoneMasks[2] = 0;
        EXPECT_THROW(NifBullet::ActorRagdollPhysics(mGraph, mWorld, 1, mPoses, 1, -1, nullptr, &filter),
            std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        EXPECT_EQ(mWorld.getNumConstraints(), 0);
    }

    TEST(RagdollSystemGroups, WrapsPastZeroToOriginalReservedStartingGroup)
    {
        EXPECT_EQ(NifBullet::nextRagdollSystemGroup(0), 1);
        EXPECT_EQ(NifBullet::nextRagdollSystemGroup(9), 10);
        EXPECT_EQ(NifBullet::nextRagdollSystemGroup(65534), 65535);
        EXPECT_EQ(NifBullet::nextRagdollSystemGroup(65535), 10);
    }


    TEST(RagdollNativeVelocityBlend, MatchesNativeIndependentLinearAngularAndGravityStores)
    {
        const NifBullet::RagdollNativeVelocities current{{1, -2, 3}, {-4, 5, -6}};
        const NifBullet::RagdollNativeVelocities target{{7, -8, 9}, {10, -11, 12}};
        const auto unchanged = NifBullet::ragdollNativeBlendVelocities(current, target, 0.f, 120.f, std::nullopt);
        EXPECT_EQ(unchanged.mLinear, current.mLinear);
        EXPECT_EQ(unchanged.mAngular, current.mAngular);
        const auto driven = NifBullet::ragdollNativeBlendVelocities(current, target, 1.f, 120.f, std::nullopt);
        EXPECT_EQ(driven.mLinear, target.mLinear);
        EXPECT_EQ(driven.mAngular, target.mAngular);
        const auto mixed = NifBullet::ragdollNativeBlendVelocities(current, target, 0.5f, 120.f,
            NifBullet::RagdollNativeDefaultGravityZ);
        // Original8A37E8..8A388C outputs in both x87 precision modes.
        EXPECT_EQ(std::bit_cast<std::uint32_t>(mixed.mLinear[0]), 1082130432u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(mixed.mAngular[0]), 1077936128u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(mixed.mLinear[1]), 3231711232u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(mixed.mAngular[1]), 3225419776u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(mixed.mLinear[2]), 1086967644u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(mixed.mAngular[2]), 1077936128u);
    }

    TEST(RagdollNativeVelocityBlend, DoesNotClampFiniteGainsOrCompensateMissingWorld)
    {
        const NifBullet::RagdollNativeVelocities current{{1, 2, 3}, {4, 5, 6}};
        const NifBullet::RagdollNativeVelocities target{{7, 8, 9}, {10, 11, 12}};
        const auto extended = NifBullet::ragdollNativeBlendVelocities(current, target, 1.5f, 1.f, std::nullopt);
        EXPECT_EQ(extended.mLinear, osg::Vec3f(10, 11, 12));
        EXPECT_EQ(extended.mAngular, osg::Vec3f(13, 14, 15));
        const auto negative = NifBullet::ragdollNativeBlendVelocities(current, target, -0.5f, 1.f, std::nullopt);
        EXPECT_EQ(negative.mLinear, osg::Vec3f(-2, -1, 0));
        EXPECT_EQ(negative.mAngular, osg::Vec3f(1, 2, 3));
        const NifBullet::RagdollNativeVelocities zero{};
        const auto gravity = NifBullet::ragdollNativeBlendVelocities(zero, zero, 0.5f, 1.f, -10.f);
        EXPECT_EQ(gravity.mLinear, osg::Vec3f(0, 0, 5));
        EXPECT_EQ(gravity.mAngular, osg::Vec3f());
    }

    TEST(RagdollNativeVelocityBlend, PreservesSseSignedZeroProducts)
    {
        const NifBullet::RagdollNativeVelocities current{{-0.f, -0.f, -0.f}, {-0.f, -0.f, -0.f}};
        const NifBullet::RagdollNativeVelocities positive{};
        const auto cancelled = NifBullet::ragdollNativeBlendVelocities(current, positive, 0.f, 120.f, std::nullopt);
        const auto retained = NifBullet::ragdollNativeBlendVelocities(current, current, 0.f, 120.f, std::nullopt);
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            EXPECT_EQ(std::bit_cast<std::uint32_t>(cancelled.mLinear[axis]), 0u);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(cancelled.mAngular[axis]), 0u);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(retained.mLinear[axis]), 0x80000000u);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(retained.mAngular[axis]), 0x80000000u);
        }
    }

    TEST(RagdollNativeVelocityBlend, RejectsInvalidInputsAndOverflowWithoutChangingVelocities)
    {
        const NifBullet::RagdollNativeVelocities current{{1, 2, 3}, {4, 5, 6}};
        const NifBullet::RagdollNativeVelocities target{{7, 8, 9}, {10, 11, 12}};
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        for (float inverse : {0.f, -1.f, nan, infinity})
            EXPECT_THROW(NifBullet::ragdollNativeBlendVelocities(current, target, 0.5f, inverse, std::nullopt),
                std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeBlendVelocities(current, target, nan, 120.f, std::nullopt),
            std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeBlendVelocities(current, target, 0.5f, 120.f, infinity),
            std::invalid_argument);
        auto invalid = current;
        invalid.mAngular[2] = nan;
        EXPECT_THROW(NifBullet::ragdollNativeBlendVelocities(invalid, target, 0.5f, 120.f, std::nullopt),
            std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeBlendVelocities(current, target,
            std::numeric_limits<float>::max(), 120.f, std::nullopt), std::invalid_argument);
        EXPECT_EQ(current.mLinear, osg::Vec3f(1, 2, 3));
        EXPECT_EQ(target.mAngular, osg::Vec3f(10, 11, 12));
    }

    TEST(RagdollNativeBlendTarget, SphericalTargetInterpolatesPositionAndShortestRotation)
    {
        const NifBullet::RagdollNativeTargetPose physical{{1, -2, 3}, {0, 0, 0, 1}};
        const NifBullet::RagdollNativeTargetPose animated{{7, -8, 9}, {0, 0, .7071067690849304f, .7071067690849304f}};
        // Original88F5A4..88F5EA, actual8B1C60 and two4D6830 calls.
        const auto result = NifBullet::ragdollNativeBlendTargetPose(physical, animated, .5f);
        EXPECT_EQ(result.mPosition, osg::Vec3f(4, -5, 6));
        EXPECT_NEAR(result.mRotation[2], .3826834261417389f, 2e-5f);
        EXPECT_NEAR(result.mRotation[3], .9238795042037964f, 2e-5f);
        EXPECT_EQ(physical.mPosition, osg::Vec3f(1, -2, 3));
        EXPECT_EQ(animated.mPosition, osg::Vec3f(7, -8, 9));
    }

    TEST(RagdollNativeBlendTarget, GainsExtrapolateAndEquivalentNegativeSignsChooseShortestPath)
    {
        const NifBullet::RagdollNativeTargetPose physical{{1, -2, 3}, {0, 0, 0, 1}};
        const NifBullet::RagdollNativeTargetPose animated{{7, -8, 9}, {0, 0, .7071067690849304f, .7071067690849304f}};
        const auto negative = NifBullet::ragdollNativeBlendTargetPose(physical, animated, -.5f);
        EXPECT_EQ(negative.mPosition, osg::Vec3f(-2, 1, 0));
        EXPECT_NEAR(negative.mRotation[2], -.3826834261417389f, 2e-5f);
        const auto extended = NifBullet::ragdollNativeBlendTargetPose(physical, animated, 1.5f);
        EXPECT_EQ(extended.mPosition, osg::Vec3f(10, -11, 12));
        EXPECT_NEAR(extended.mRotation[2], .9238795638084412f, 2e-5f);
        auto signedTarget = animated;
        for (float& component : signedTarget.mRotation)
            component = -component;
        const auto same = NifBullet::ragdollNativeBlendTargetPose(physical, signedTarget, .5f);
        EXPECT_NEAR(same.mRotation[2], .3826834261417389f, 2e-5f);
        EXPECT_NEAR(same.mRotation[3], .9238795042037964f, 2e-5f);
    }

    TEST(RagdollNativeBlendTarget, LinearThresholdAndSignedZeroUseNativeStores)
    {
        const NifBullet::RagdollNativeTargetPose physical{{-0.f, -0.f, -0.f}, {0, 0, 0, 1}};
        const NifBullet::RagdollNativeTargetPose animated{{-0.f, -0.f, -0.f}, {.04470989108085632f, 0, 0, .9990000128746033f}};
        const auto result = NifBullet::ragdollNativeBlendTargetPose(physical, animated, .5f);
        EXPECT_NEAR(result.mRotation[0], .022360535338521004f, 2e-7f);
        EXPECT_NEAR(result.mRotation[3], .9997499585151672f, 2e-7f);
        const auto zero = NifBullet::ragdollNativeBlendTargetPose(physical, physical, 0.f);
        const auto negativeZero = NifBullet::ragdollNativeBlendTargetPose(physical, physical, -0.f);
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            EXPECT_TRUE(std::signbit(zero.mPosition[axis]));
            EXPECT_FALSE(std::signbit(negativeZero.mPosition[axis]));
        }
    }

    TEST(RagdollNativeBlendTarget, RejectsInvalidInputsAndNonrepresentableResults)
    {
        const NifBullet::RagdollNativeTargetPose valid{{1, 2, 3}, {0, 0, 0, 1}};
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        for (float gain : {nan, infinity, -infinity})
        {
            EXPECT_THROW(NifBullet::ragdollNativeBlendTargetPose(valid, valid, gain), std::invalid_argument);
        }
        auto invalid = valid;
        invalid.mPosition[0] = nan;
        EXPECT_THROW(NifBullet::ragdollNativeBlendTargetPose(invalid, valid, .5f), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeBlendTargetPose(valid, invalid, .5f), std::invalid_argument);
        invalid = valid;
        invalid.mRotation = {0, 0, 0, 0};
        EXPECT_THROW(NifBullet::ragdollNativeBlendTargetPose(invalid, valid, .5f), std::invalid_argument);
        invalid.mRotation = {0, 0, 0, 2};
        EXPECT_THROW(NifBullet::ragdollNativeBlendTargetPose(valid, invalid, .5f), std::invalid_argument);
        invalid.mRotation = {nan, 0, 0, 1};
        EXPECT_THROW(NifBullet::ragdollNativeBlendTargetPose(valid, invalid, .5f), std::invalid_argument);
        invalid = valid;
        invalid.mPosition[0] = std::numeric_limits<float>::max();
        EXPECT_THROW(NifBullet::ragdollNativeBlendTargetPose(valid, invalid, 2.f), std::invalid_argument);
    }

    TEST(RagdollNativeTargetVelocity, RotatesLocalCenterOfMassAndCapsSpeedsIndependently)
    {
        const std::array<float, 4> identity{0, 0, 0, 1};
        const NifBullet::RagdollNativeTargetPose target{{7, -8, 9}, {0, 0, .7071067690849304f, .7071067690849304f}};
        // Actual original8A34C0..8A37E8 fixture, both precision words.
        const auto free = NifBullet::ragdollNativeTargetVelocities({1, -2, 3}, {4, 5, -6}, identity,
            target, 1.f, 250.f, 31.4159f);
        EXPECT_EQ(free.mLinear, osg::Vec3f(5, -12, 18));
        EXPECT_NEAR(free.mAngular.x(), 0.f, 2e-5f);
        EXPECT_NEAR(free.mAngular.y(), 0.f, 2e-5f);
        EXPECT_NEAR(free.mAngular.z(), 1.570796251296997f, 2e-5f);
        const auto capped = NifBullet::ragdollNativeTargetVelocities({1, -2, 3}, {4, 5, -6}, identity,
            target, 1.f, 1.f, 1.f);
        EXPECT_FLOAT_EQ(capped.mLinear.x(), .22518867254257202f);
        EXPECT_FLOAT_EQ(capped.mLinear.y(), -.5404528379440308f);
        EXPECT_FLOAT_EQ(capped.mLinear.z(), .8106792569160461f);
        EXPECT_NEAR(capped.mAngular.z(), 1.f, 2e-5f);
    }

    TEST(RagdollNativeTargetVelocity, EquivalentQuaternionSignsAndZeroCapsProduceNoRotation)
    {
        const std::array<float, 4> negativeIdentity{0, 0, 0, -1};
        const NifBullet::RagdollNativeTargetPose target{{1, 2, 3}, {0, 0, 0, 1}};
        const auto moved = NifBullet::ragdollNativeTargetVelocities({}, {}, negativeIdentity,
            target, 30.f, 250.f, 31.4159f);
        EXPECT_EQ(moved.mLinear, osg::Vec3f(30, 60, 90));
        EXPECT_EQ(moved.mAngular, osg::Vec3f());
        const auto capped = NifBullet::ragdollNativeTargetVelocities({}, {}, negativeIdentity,
            target, 30.f, 0.f, 0.f);
        EXPECT_EQ(capped.mLinear, osg::Vec3f());
        EXPECT_EQ(capped.mAngular, osg::Vec3f());
    }

    TEST(RagdollNativeTargetVelocity, RejectsInvalidPoseTimeLimitsAndOverflow)
    {
        const std::array<float, 4> identity{0, 0, 0, 1};
        const NifBullet::RagdollNativeTargetPose target{{1, 2, 3}, identity};
        const float nan = std::numeric_limits<float>::quiet_NaN();
        for (float inverse : {0.f, -1.f, nan})
            EXPECT_THROW(NifBullet::ragdollNativeTargetVelocities({}, {}, identity, target, inverse, 250.f, 31.f),
                std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeTargetVelocities({}, {}, identity, target, 1.f, -1.f, 31.f),
            std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeTargetVelocities({}, {}, identity, target, 1.f, 250.f, nan),
            std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeTargetVelocities({}, {}, std::array<float, 4>{}, target, 1.f, 250.f, 31.f),
            std::invalid_argument);
        auto malformed = target;
        malformed.mRotation[0] = nan;
        EXPECT_THROW(NifBullet::ragdollNativeTargetVelocities({}, {}, identity, malformed, 1.f, 250.f, 31.f),
            std::invalid_argument);
        malformed = target;
        malformed.mPosition[0] = std::numeric_limits<float>::max();
        EXPECT_THROW(NifBullet::ragdollNativeTargetVelocities({}, {}, identity, malformed, 120.f, 250.f, 31.f),
            std::invalid_argument);
        EXPECT_EQ(target.mPosition, osg::Vec3f(1, 2, 3));
    }

    TEST(RagdollNativeBlendTarget, RejectsUnsupportedTrigonometricAndNormalizationOverflowDomains)
    {
        const NifBullet::RagdollNativeTargetPose physical{{}, {0, 0, 0, 1}};
        const NifBullet::RagdollNativeTargetPose spherical{{}, {0, 0, .7071067690849304f, .7071067690849304f}};
        const NifBullet::RagdollNativeTargetPose linear{{}, {.04470989108085632f, 0, 0, .9990000128746033f}};
        const float gain = std::numeric_limits<float>::max();
        EXPECT_THROW(NifBullet::ragdollNativeBlendTargetPose(physical, spherical, gain), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeBlendTargetPose(physical, linear, gain), std::invalid_argument);
        EXPECT_EQ(physical.mRotation, (std::array<float, 4>{0, 0, 0, 1}));
    }

    TEST_F(ActorRagdollPhysicsTest, NativePoseDrivePublishesVelocitiesAndWakesWithoutMovingOrClearingForces)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        body->applyCentralForce(btVector3(3, 4, 5));
        body->setActivationState(ISLAND_SLEEPING);
        const auto initial = actor.capture()[0];
        const std::array<NifBullet::RagdollNativeVelocityDrive, 1> drives{{{12, {{1, 2, 5}, {0, 0, 0, 1}}, .5f}}};
        actor.driveNativePoseVelocities(drives, 2.f, -10.f);
        const auto result = actor.capture()[0];
        // Native displacement(1,2,3)*inverse2*gain.5 plus Z compensation2.5.
        EXPECT_EQ(result.mLinearVelocity, btVector3(1, 2, 5.5));
        EXPECT_EQ(result.mAngularVelocity, btVector3(0, 0, 0));
        EXPECT_EQ(result.mPose, initial.mPose);
        EXPECT_EQ(body->getTotalForce(), btVector3(3, 4, 5));
        EXPECT_TRUE(body->isActive());
    }

    TEST_F(ActorRagdollPhysicsTest, NativePoseDriveSelectsRecordsAndLeavesUnselectedSleepingBodiesAlone)
    {
        addHinge();
        mGraph.mJoints.clear();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        auto* second = btRigidBody::upcast(actor.collisionObjects()[1]);
        first->setActivationState(ISLAND_SLEEPING);
        second->setActivationState(ISLAND_SLEEPING);
        actor.driveNativePoseVelocities({}, 1.f, 0.f);
        EXPECT_FALSE(first->isActive());
        EXPECT_FALSE(second->isActive());
        const std::array<NifBullet::RagdollNativeVelocityDrive, 1> drives{{{24, {{4, 0, 1}, {0, 0, 0, 1}}, 1.f}}};
        actor.driveNativePoseVelocities(drives, 1.f, 0.f);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(0, 0, 0));
        EXPECT_EQ(actor.capture()[1].mLinearVelocity, btVector3(4, 0, 0));
        EXPECT_FALSE(first->isActive());
        EXPECT_TRUE(second->isActive());
        EXPECT_EQ(mWorld.getNumConstraints(), 0);
    }

    TEST_F(ActorRagdollPhysicsTest, NativePoseDriveRejectsWholeBatchBeforeVelocityOrActivationChanges)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        auto* second = btRigidBody::upcast(actor.collisionObjects()[1]);
        first->setActivationState(ISLAND_SLEEPING);
        second->setActivationState(ISLAND_SLEEPING);
        std::array<NifBullet::RagdollNativeVelocityDrive, 2> drives{{
            {12, {{4, 0, 2}, {0, 0, 0, 1}}, 1.f}, {999, {{4, 0, 1}, {0, 0, 0, 1}}, 1.f}}};
        EXPECT_THROW(actor.driveNativePoseVelocities(drives, 1.f, 0.f), std::invalid_argument);
        drives[1].mRecord = 12;
        EXPECT_THROW(actor.driveNativePoseVelocities(drives, 1.f, 0.f), std::invalid_argument);
        drives[1].mRecord = 24;
        drives[1].mTarget.mRotation = {};
        EXPECT_THROW(actor.driveNativePoseVelocities(drives, 1.f, 0.f), std::invalid_argument);
        drives[1].mTarget.mRotation = {0, 0, 0, 1};
        drives[1].mVelocityGain = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(actor.driveNativePoseVelocities(drives, 1.f, 0.f), std::invalid_argument);
        drives[1].mVelocityGain = 1.f;
        EXPECT_THROW(actor.driveNativePoseVelocities(drives, 0.f, 0.f), std::invalid_argument);
        EXPECT_THROW(actor.driveNativePoseVelocities(drives, 1.f, std::numeric_limits<float>::quiet_NaN()),
            std::invalid_argument);
        drives[1].mTarget.mPosition.x() = std::numeric_limits<float>::max();
        EXPECT_THROW(actor.driveNativePoseVelocities(drives, 2.f, 0.f), std::invalid_argument);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(0, 0, 0));
        EXPECT_EQ(actor.capture()[1].mLinearVelocity, btVector3(0, 0, 0));
        EXPECT_FALSE(first->isActive());
        EXPECT_FALSE(second->isActive());
    }

    TEST_F(ActorRagdollPhysicsTest, NativePoseDriveConvertsWorldUnitsAndPrincipalCenterFrameOnce)
    {
        mGraph.mBodies[0].mCenter = {1, -2, 3};
        mGraph.mBodies[0].mInertia = {2, .5f, 0, .5f, 3, 0, 0, 0, 4};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 2.5f, mPoses, 1, -1);
        const auto initial = actor.capture()[0];
        const std::array<NifBullet::RagdollNativeVelocityDrive, 1> drives{{{12, {{2, 0, .8f}, {0, 0, 0, 1}}, 1.f}}};
        actor.driveNativePoseVelocities(drives, 1.f, 0.f);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(5, 0, 0));
        EXPECT_EQ(actor.capture()[0].mAngularVelocity, btVector3(0, 0, 0));
        EXPECT_EQ(actor.capture()[0].mPose, initial.mPose);
    }

    TEST_F(ActorRagdollPhysicsTest, NativePoseDriveWakesConnectedBonesBeforeNativeGravityIntegration)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        auto* second = btRigidBody::upcast(actor.collisionObjects()[1]);
        first->setActivationState(ISLAND_SLEEPING);
        second->setActivationState(ISLAND_SLEEPING);
        const std::array<NifBullet::RagdollNativeVelocityDrive, 1> drives{{{24, {{0, 0, 1}, {0, 0, 0, 1}}, 0.f}}};
        actor.driveNativePoseVelocities(drives, 120.f, 0.f);
        EXPECT_TRUE(first->isActive());
        EXPECT_TRUE(second->isActive());
        const std::array<osg::Vec3f, 2> deltas{{{0, 0, -1}, {0, 0, -1}}};
        actor.applyNativeVelocityStep(1.f / 120.f, deltas);
        EXPECT_LT(actor.capture()[0].mLinearVelocity.z(), 0);
        EXPECT_LT(actor.capture()[1].mLinearVelocity.z(), 0);
    }

    TEST_F(ActorRagdollPhysicsTest, ImpulseWakesConnectedBonesBeforeNativeGravityIntegration)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        auto* second = btRigidBody::upcast(actor.collisionObjects()[1]);
        first->setActivationState(ISLAND_SLEEPING);
        second->setActivationState(ISLAND_SLEEPING);
        actor.applyImpulse(1, btVector3(2, 0, 0), mPoses[1].getOrigin());
        EXPECT_TRUE(first->isActive());
        EXPECT_TRUE(second->isActive());
        const std::array<osg::Vec3f, 2> deltas{{{0, 0, -1}, {0, 0, -1}}};
        actor.applyNativeVelocityStep(1.f / 120.f, deltas);
        EXPECT_LT(actor.capture()[0].mLinearVelocity.z(), 0);
        EXPECT_LT(actor.capture()[1].mLinearVelocity.z(), 0);
    }

}

namespace
{
    TEST(RagdollNativeSceneTarget, PreservesOriginalBranchQuaternionSignsAndFloatStores)
    {
        struct Case { std::array<std::uint32_t, 12> mScene; std::array<std::uint32_t, 7> mExpected; };
        // Full original89EAE0 scene-sync corpus; inputs and returns precede
        // production conversion. Scene scalar scale is a separate native field.
        const std::array<Case, 10> cases{{
        {{{3182157128u, 1056045612u, 1063294083u, 1064673025u, 1049449875u, 3177788088u, 3196699560u, 1062617218u, 3203663618u, 3297172727u, 1169021577u, 1148907522u}}, {{3273270915u, 1145485206u, 1125083548u, 1057440399u, 1059944274u, 1049843532u, 1054378868u}}},
        {{{1062863826u, 1055937932u, 1047451580u, 3204851674u, 1061213623u, 1053523921u, 1009920570u, 3203130094u, 1063465463u, 1163825424u, 1166399375u, 3259466735u}}, {{1140745618u, 1142487993u, 3236170880u, 3194691138u, 1030992394u, 3196589820u, 1064252517u}}},
        {{{1061061526u, 3201531164u, 3204866513u, 1033071318u, 1062515929u, 3205316804u, 1059717285u, 1052697801u, 1059448864u, 1165909382u, 3322348214u, 1131842812u}}, {{1141989149u, 3298447784u, 1108142027u, 1048827863u, 3198782271u, 1040864278u, 1063636187u}}},
        {{{3195360606u, 1064040861u, 1050416041u, 1060537485u, 3174875426u, 1060314829u, 1059632271u, 1053104869u, 3206900245u, 3315796085u, 1157960779u, 1162773326u}}, {{3292158297u, 1134042248u, 1139543056u, 1058686444u, 1059953782u, 1053604089u, 3188044185u}}},
        {{{1060416108u, 3193119406u, 3207431564u, 1046279215u, 1064913934u, 3180499540u, 1059897734u, 3183745049u, 1060852514u, 3277709178u, 1172638249u, 1173233816u}}, {{3254622870u, 1149429204u, 1149769575u, 3151397088u, 3199958036u, 1038742035u, 1064069465u}}},
        {{{1025592694u, 1060053407u, 3208278796u, 1059724063u, 3205493408u, 3204187679u, 3208579997u, 3203259276u, 3203666984u, 3300419197u, 3315339100u, 3311028234u}}, {{3276981675u, 3291635957u, 3287908264u, 1060669546u, 1055881397u, 3204639841u, 1008548040u}}},
        {{{1065333762u, 3167806168u, 1025983272u, 1020862862u, 1065342367u, 3167161487u, 3173293517u, 1020243799u, 1065334264u, 1157338782u, 3322845880u, 1172932094u}}, {{1133496532u, 3299016623u, 1149597138u, 1011577105u, 1017511955u, 1012209209u, 1065347058u}}},
        {{{1063088572u, 1054175861u, 3197039264u, 3198352621u, 1063442527u, 1051490013u, 1053200643u, 3192871860u, 1063662311u, 3321979859u, 1164232920u, 1174983741u}}, {{3298026749u, 1141031040u, 1151100355u, 3188754562u, 3190996878u, 3192194716u, 1064603170u}}},
        {{{1065282601u, 1026734527u, 1034228112u, 3176658219u, 1065219711u, 1038755607u, 3180956091u, 3186745032u, 1065188255u, 1164310681u, 3321120819u, 3323475113u}}, {{1141075481u, 3297483743u, 3299735845u, 3178146606u, 1025490497u, 3167085300u, 1065307017u}}},
        {{{3190519825u, 1054835071u, 1063406506u, 1036318173u, 3210909462u, 1055465432u, 1065037445u, 1042620528u, 1037668220u, 1175024363u, 3314796064u, 3305692310u}}, {{1151146786u, 3291015259u, 3281809229u, 1059224699u, 1045879721u, 1060901257u, 3186439724u}}}
        }};
        for (const auto& row : cases)
        {
            osg::Matrixf matrix = osg::Matrixf::identity();
            for (unsigned r = 0; r < 3; ++r)
                for (unsigned c = 0; c < 3; ++c)
                    matrix(c, r) = std::bit_cast<float>(row.mScene[r * 3 + c]);
            for (unsigned axis = 0; axis < 3; ++axis)
                matrix(3, axis) = std::bit_cast<float>(row.mScene[9 + axis]);
            const auto pose = NifBullet::ragdollNativeSceneTargetPose(matrix);
            for (unsigned axis = 0; axis < 3; ++axis)
                EXPECT_EQ(std::bit_cast<std::uint32_t>(pose.mPosition[axis]), row.mExpected[axis]);
            for (unsigned axis = 0; axis < 4; ++axis)
                EXPECT_EQ(std::bit_cast<std::uint32_t>(pose.mRotation[axis]), row.mExpected[3 + axis]);
        }
    }

    TEST(RagdollNativeSceneTarget, ExistingBoneAdapterUsesThePreparedNativeTarget)
    {
        osg::Matrixf matrix = osg::Matrixf::rotate(.8f, osg::Vec3f(1, 2, 3))
            * osg::Matrixf::translate(17, -23, 41);
        const auto target = NifBullet::ragdollNativeSceneTargetPose(matrix);
        const btTransform expected(btQuaternion(target.mRotation[0], target.mRotation[1],
            target.mRotation[2], target.mRotation[3]),
            btVector3(target.mPosition.x(), target.mPosition.y(), target.mPosition.z()));
        EXPECT_EQ(NifBullet::ragdollNativePoseFromBoneWorld(matrix), expected);
    }

    TEST(RagdollNativeSceneTarget, RigidAdmissionAndSignedZeroRemainExplicit)
    {
        auto matrix = osg::Matrixf::identity();
        matrix(3, 0) = -0.f;
        const auto target = NifBullet::ragdollNativeSceneTargetPose(matrix);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(target.mPosition.x()), 0x80000000u);
        EXPECT_EQ(target.mRotation, (std::array<float, 4>{0, 0, 0, 1}));
        for (const auto& invalid : {osg::Matrixf::scale(2, 2, 2), osg::Matrixf::scale(-1, 1, 1)})
            EXPECT_THROW(NifBullet::ragdollNativeSceneTargetPose(invalid), std::invalid_argument);
        matrix(0, 3) = .1f;
        EXPECT_THROW(NifBullet::ragdollNativeSceneTargetPose(matrix), std::invalid_argument);
        matrix = osg::Matrixf::identity();
        matrix(1, 1) = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(NifBullet::ragdollNativeSceneTargetPose(matrix), std::invalid_argument);
    }
}


namespace
{
    TEST_F(ActorRagdollPhysicsTest, NativeKeyframedScenePosePreservesVelocitiesForcesAndGraphIdentity)
    {
        mGraph.mBodies[0].mCenter = {.25f, -.5f, .75f};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.setNativeMotionModes(key);
        body->setLinearVelocity(btVector3(1, 2, 3));
        body->setAngularVelocity(btVector3(-1, -2, -3));
        body->applyCentralForce(btVector3(4, 5, 6));
        body->applyTorque(btVector3(7, 8, 9));
        const auto before = actor.capture()[0];
        const auto* shape = body->getCollisionShape();
        const auto* proxy = body->getBroadphaseHandle();
        const auto matrix = osg::Matrixf::rotate(.7f, osg::Vec3f(1, 2, 3))
            * osg::Matrixf::translate(21, -28, 35);
        const std::array<NifBullet::RagdollNativeScenePoseRequest, 1> request{{{12, matrix}}};
        actor.synchronizeNativeKeyframedPoses(request);
        const auto target = NifBullet::ragdollNativePoseFromBoneWorld(matrix);
        const auto after = actor.capture()[0];
        for (unsigned axis = 0; axis < 3; ++axis)
            EXPECT_NEAR(after.mPose.getOrigin()[axis], target.getOrigin()[axis], 1e-12);
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned col = 0; col < 3; ++col)
                EXPECT_NEAR(after.mPose.getBasis()[row][col], target.getBasis()[row][col], 1e-12);
        EXPECT_EQ(after.mLinearVelocity, before.mLinearVelocity);
        EXPECT_EQ(after.mAngularVelocity, before.mAngularVelocity);
        EXPECT_EQ(body->getInterpolationWorldTransform(), body->getWorldTransform());
        EXPECT_EQ(body->getInterpolationLinearVelocity(), before.mLinearVelocity);
        EXPECT_EQ(body->getInterpolationAngularVelocity(), before.mAngularVelocity);
        EXPECT_EQ(body->getTotalForce(), btVector3(4, 5, 6));
        EXPECT_EQ(body->getTotalTorque(), btVector3(7, 8, 9));
        EXPECT_EQ(body->getCollisionShape(), shape);
        EXPECT_EQ(body->getBroadphaseHandle(), proxy);
        EXPECT_TRUE(body->isKinematicObject());
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeKeyframedScenePoseRejectsEntireInvalidBatchBeforePublication)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeMotionRequest, 2> key{{
            {12, NifBullet::RagdollNativeMotion::Keyframed}, {24, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.setNativeMotionModes(key);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        auto* second = btRigidBody::upcast(actor.collisionObjects()[1]);
        first->applyCentralForce(btVector3(1, 2, 3));
        const auto firstPose = first->getWorldTransform();
        const auto secondPose = second->getWorldTransform();
        auto matrix = osg::Matrixf::translate(7, 14, 21);
        std::array<NifBullet::RagdollNativeScenePoseRequest, 2> requests{{{12, matrix}, {999, matrix}}};
        EXPECT_THROW(actor.synchronizeNativeKeyframedPoses(requests), std::invalid_argument);
        requests[1].mRecord = 12;
        EXPECT_THROW(actor.synchronizeNativeKeyframedPoses(requests), std::invalid_argument);
        requests[1].mRecord = 24;
        requests[1].mWorldPose = osg::Matrixf::scale(2, 2, 2);
        EXPECT_THROW(actor.synchronizeNativeKeyframedPoses(requests), std::invalid_argument);
        requests[1].mWorldPose = matrix;
        requests[1].mWorldPose(3, 0) = std::numeric_limits<float>::infinity();
        EXPECT_THROW(actor.synchronizeNativeKeyframedPoses(requests), std::invalid_argument);
        EXPECT_EQ(first->getWorldTransform(), firstPose);
        EXPECT_EQ(second->getWorldTransform(), secondPose);
        EXPECT_EQ(first->getTotalForce(), btVector3(1, 2, 3));
        requests[1].mWorldPose = matrix;
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> dynamic{{{24, NifBullet::RagdollNativeMotion::Dynamic}}};
        actor.setNativeMotionModes(dynamic);
        EXPECT_THROW(actor.synchronizeNativeKeyframedPoses(requests), std::invalid_argument);
        EXPECT_EQ(first->getWorldTransform(), firstPose);
        EXPECT_EQ(second->getWorldTransform(), secondPose);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeKeyframedScenePoseUpdatesCollisionQueriesAndKeepsUnselectedSleep)
    {
        auto other = mGraph.mBodies[0];
        other.mRecord = 24;
        other.mBone = "unconnected";
        mGraph.mBodies.push_back(other);
        mPoses.push_back(btTransform(btQuaternion::getIdentity(), btVector3(0, 0, 20)));
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        auto* second = btRigidBody::upcast(actor.collisionObjects()[1]);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.setNativeMotionModes(key);
        second->setActivationState(ISLAND_SLEEPING);
        actor.synchronizeNativeKeyframedPoses({});
        EXPECT_FALSE(second->isActive());
        const std::array<NifBullet::RagdollNativeScenePoseRequest, 1> request{{{12, osg::Matrixf::translate(70, 0, 0)}}};
        actor.synchronizeNativeKeyframedPoses(request);
        EXPECT_GT(first->getWorldTransform().getOrigin().x(), 9.9);
        EXPECT_EQ(second->getWorldTransform(), mPoses[1]);
        EXPECT_FALSE(second->isActive());
        btCollisionWorld::ClosestRayResultCallback oldRay(btVector3(-2, 0, 2), btVector3(2, 0, 2));
        mWorld.rayTest(oldRay.m_rayFromWorld, oldRay.m_rayToWorld, oldRay);
        EXPECT_FALSE(oldRay.hasHit());
        btCollisionWorld::ClosestRayResultCallback newRay(btVector3(8, 0, 0), btVector3(12, 0, 0));
        mWorld.rayTest(newRay.m_rayFromWorld, newRay.m_rayToWorld, newRay);
        ASSERT_TRUE(newRay.hasHit());
        EXPECT_EQ(newRay.m_collisionObject, first);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> dynamic{{{12, NifBullet::RagdollNativeMotion::Dynamic}}};
        actor.setNativeMotionModes(dynamic);
        EXPECT_FALSE(first->isKinematicObject());
        EXPECT_GT(first->getWorldTransform().getOrigin().x(), 9.9);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeKeyframedScenePoseConvertsCallerLengthsAndPrincipalFrameOnce)
    {
        mGraph.mBodies[0].mCenter = {.25f, -.5f, .75f};
        mGraph.mBodies[0].mInertia = {2, .3f, 0, .3f, 3, 0, 0, 0, 4};
        constexpr float scale = 3.f;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, scale, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.setNativeMotionModes(key);
        const auto matrix = osg::Matrixf::rotate(.7f, osg::Vec3f(0, 0, 1)) * osg::Matrixf::translate(7, 14, 21);
        const std::array<NifBullet::RagdollNativeScenePoseRequest, 1> request{{{12, matrix}}};
        actor.synchronizeNativeKeyframedPoses(request);
        auto expected = NifBullet::ragdollNativePoseFromBoneWorld(matrix);
        expected.setOrigin(expected.getOrigin() * scale);
        const auto actual = actor.capture()[0].mPose;
        for (unsigned axis = 0; axis < 3; ++axis)
            EXPECT_NEAR(actual.getOrigin()[axis], expected.getOrigin()[axis], 1e-12);
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned col = 0; col < 3; ++col)
                EXPECT_NEAR(actual.getBasis()[row][col], expected.getBasis()[row][col], 1e-12);
    }
}


namespace
{
    TEST(RagdollNativePhysicsScene, PreservesOriginalFloatRotationStoresAndLengthReturn)
    {
        struct Example { std::array<uint32_t, 7> mInput; std::array<uint32_t, 12> mWorld; };
        const std::array<Example, 10> examples{{
            {{0u, 2147483648u, 0u, 0u, 0u, 0u, 1065353216u}, {1065353216u, 0u, 0u, 0u, 1065353216u, 0u, 0u, 0u, 1065353216u, 0u, 2147483648u, 0u}},
            {{3290492696u, 1114959375u, 3292304527u, 1065353216u, 0u, 0u, 613232946u}, {1065353216u, 0u, 0u, 0u, 3212836864u, 2769105202u, 0u, 621621554u, 3212836864u, 3314338884u, 1138116807u, 3315924018u}},
            {{3244576084u, 3291920894u, 1128517087u, 0u, 1065353216u, 0u, 613232946u}, {3212836864u, 0u, 621621554u, 0u, 1065353216u, 0u, 2769105202u, 0u, 3212836864u, 3267869862u, 3315588385u, 1152077343u}},
            {{1146531072u, 1134003540u, 3280948261u, 0u, 0u, 1065353216u, 613232946u}, {3212836864u, 2769105202u, 0u, 621621554u, 3212836864u, 0u, 0u, 0u, 1065353216u, 1169936584u, 1157926914u, 3304766579u}},
            {{3273270915u, 1145485206u, 1125083548u, 1057440399u, 1059944274u, 1049843532u, 1054378868u}, {3182157124u, 1056045612u, 1063294084u, 1064673026u, 1049449873u, 3177788096u, 3196699560u, 1062617218u, 3203663618u, 3297172726u, 1169021577u, 1148907522u}},
            {{1140745618u, 1142487993u, 3236170880u, 3194691138u, 1030992394u, 3196589820u, 1064252517u}, {1062863826u, 1055937931u, 1047451579u, 3204851673u, 1061213623u, 1053523920u, 1009920560u, 3203130094u, 1063465463u, 1163825423u, 1166399375u, 3259466735u}},
            {{1141989149u, 3298447784u, 1108142027u, 1048827863u, 3198782271u, 1040864278u, 1063636187u}, {1061061525u, 3201531164u, 3204866513u, 1033071318u, 1062515930u, 3205316804u, 1059717285u, 1052697800u, 1059448864u, 1165909381u, 3322348214u, 1131842811u}},
            {{3292158297u, 1134042248u, 1139543056u, 1058686444u, 1059953782u, 1053604089u, 3188044185u}, {3195360604u, 1064040860u, 1050416039u, 1060537484u, 3174875376u, 1060314828u, 1059632270u, 1053104868u, 3206900244u, 3315796085u, 1157960779u, 1162773326u}},
            {{3254622870u, 1149429204u, 1149769575u, 3151397088u, 3199958036u, 1038742035u, 1064069465u}, {1060416107u, 3193119406u, 3207431565u, 1046279216u, 1064913934u, 3180499540u, 1059897735u, 3183745050u, 1060852514u, 3277709177u, 1172638248u, 1173233816u}},
            {{3276981675u, 3291635957u, 3287908264u, 1060669546u, 1055881397u, 3204639841u, 1008548040u}, {1025592712u, 1060053405u, 3208278794u, 1059724061u, 3205493404u, 3204187677u, 3208579996u, 3203259275u, 3203666975u, 3300419196u, 3315339100u, 3311028233u}},
        }};
        for (const auto& example : examples)
        {
            NifBullet::RagdollNativeTargetPose input;
            for (unsigned axis = 0; axis < 3; ++axis)
                input.mPosition[axis] = std::bit_cast<float>(example.mInput[axis]);
            for (unsigned axis = 0; axis < 4; ++axis)
                input.mRotation[axis] = std::bit_cast<float>(example.mInput[3 + axis]);
            const auto actual = NifBullet::ragdollBoneWorldFromNativePose(input);
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 3; ++column)
                    EXPECT_EQ(std::bit_cast<uint32_t>(actual(column, row)), example.mWorld[row * 3 + column]);
            for (unsigned axis = 0; axis < 3; ++axis)
                EXPECT_EQ(std::bit_cast<uint32_t>(actual(3, axis)), example.mWorld[9 + axis]);
            EXPECT_EQ(actual(0, 3), 0.f);
            EXPECT_EQ(actual(1, 3), 0.f);
            EXPECT_EQ(actual(2, 3), 0.f);
            EXPECT_EQ(actual(3, 3), 1.f);
        }
    }

    TEST(RagdollNativePhysicsScene, RejectsNonfiniteNonunitAndUnrepresentableOutput)
    {
        NifBullet::RagdollNativeTargetPose input{{0, 0, 0}, {0, 0, 0, 1}};
        input.mRotation[3] = 2;
        EXPECT_THROW(NifBullet::ragdollBoneWorldFromNativePose(input), std::invalid_argument);
        input.mRotation = {0, 0, 0, 0};
        EXPECT_THROW(NifBullet::ragdollBoneWorldFromNativePose(input), std::invalid_argument);
        input.mRotation = {0, 0, 0, std::numeric_limits<float>::quiet_NaN()};
        EXPECT_THROW(NifBullet::ragdollBoneWorldFromNativePose(input), std::invalid_argument);
        input.mRotation = {0, 0, 0, 1};
        input.mPosition[0] = std::numeric_limits<float>::infinity();
        EXPECT_THROW(NifBullet::ragdollBoneWorldFromNativePose(input), std::invalid_argument);
        input.mPosition[0] = std::numeric_limits<float>::max();
        EXPECT_THROW(NifBullet::ragdollBoneWorldFromNativePose(input), std::invalid_argument);
    }
}


namespace
{
    TEST(RagdollNativeBlendScene, MatchesOriginalUnroundedRootsAndQuaternionBranches)
    {
        struct Example { std::array<uint32_t, 12> mWorld; std::array<uint32_t, 7> mTarget; };
        const std::array<Example, 14> examples{{
            {{1065353216u, 0u, 0u, 0u, 1065353216u, 0u, 0u, 0u, 1065353216u, 0u, 2147483648u, 1u}, {0u, 2147483648u, 0u, 0u, 0u, 0u, 1065353216u}},
            {{1065353216u, 0u, 0u, 0u, 3212836864u, 2769105202u, 0u, 621621554u, 3212836864u, 3314338884u, 1138116808u, 3315924019u}, {3290492696u, 1114959375u, 3292304527u, 1065353216u, 0u, 0u, 613232946u}},
            {{3212836864u, 0u, 621621554u, 0u, 1065353216u, 0u, 2769105202u, 0u, 3212836864u, 3267869863u, 3315588386u, 1152077344u}, {3244576084u, 3291920894u, 1128517087u, 0u, 1065353216u, 0u, 613232946u}},
            {{3212836864u, 2769105202u, 0u, 621621554u, 3212836864u, 0u, 0u, 0u, 1065353216u, 1169936584u, 1157926914u, 3304766579u}, {1146531072u, 1134003540u, 3280948261u, 0u, 0u, 1065353216u, 613232946u}},
            {{1065307251u, 3168475669u, 1032666985u, 1022046214u, 1065339889u, 3168954819u, 3180044065u, 1022493549u, 1065306842u, 3305571379u, 1168841500u, 3305231177u}, {3281671004u, 1145279375u, 3281282148u, 1013605850u, 1024232347u, 1013142243u, 1065340003u}},
            {{3190770246u, 3194144129u, 1064682598u, 3206980294u, 1061274432u, 1030733362u, 3208467966u, 3206375676u, 3196855417u, 1167307176u, 1168142660u, 1154119745u}, {1143525622u, 1144480592u, 1130851580u, 3197534680u, 1061023358u, 3191863352u, 1058184483u}},
            {{1064835956u, 3189764924u, 3192070642u, 1042358808u, 1065143492u, 3155552391u, 1044523046u, 3165454338u, 1065045070u, 1147350821u, 3317450125u, 3309891825u}, {1124193882u, 3294048888u, 3286609333u, 3141902126u, 3183749593u, 1034013528u, 1065223322u}},
            {{1062901579u, 1036953602u, 3204626254u, 3198256997u, 1063350163u, 3199534172u, 1054077983u, 1055722268u, 1061725325u, 1171174668u, 3316537537u, 3278658283u}, {1147946219u, 3293005787u, 3255243807u, 1046405622u, 3195822309u, 3185791551u, 1064310490u}},
            {{1037113736u, 3211969776u, 3197752432u, 1046152475u, 1050786922u, 3211568055u, 1064875203u, 1022801362u, 1047575054u, 1155657437u, 3319471683u, 3322303075u}, {1132535631u, 3296359556u, 3298396190u, 1052642417u, 3204268671u, 1055352869u, 1059361398u}},
            {{3209445616u, 3206162134u, 3169483181u, 3190401618u, 1043435083u, 1064863572u, 3205784028u, 1061650863u, 3195262112u, 3321068162u, 1151696612u, 3315197105u}, {3297453649u, 1128081905u, 3291473655u, 3196337825u, 1061044802u, 1058453050u, 1044237150u}},
            {{1065321637u, 1024245537u, 1028657033u, 3170497410u, 1065298479u, 3180923052u, 3176785441u, 1033212506u, 1065284592u, 3314986088u, 3321034672u, 3316995201u}, {3291232459u, 3297434509u, 3293528903u, 1024948814u, 1020606946u, 3162833862u, 1065333837u}},
            {{1063545098u, 3189169736u, 1054510960u, 3181899070u, 1063284822u, 1056091243u, 3202571865u, 3203035365u, 1061496941u, 1167518039u, 3315738420u, 3322930968u}, {1143766641u, 3292092385u, 3299113880u, 3195903417u, 1047333370u, 1015955885u, 1064357043u}},
            {{1060444133u, 1043952163u, 3207522233u, 1053368080u, 1060368174u, 1058525220u, 1058434486u, 3207600361u, 1054483847u, 1158634428u, 1171774346u, 1169523514u}, {1134812238u, 1148631659u, 1146058927u, 3200431213u, 3200330947u, 1031846521u, 1062703601u}},
            {{1026121530u, 3182707978u, 1065273690u, 3206881031u, 1061299691u, 1036009411u, 3208861654u, 3206894860u, 3167761122u, 3319705356u, 3307487925u, 1168225423u}, {3296626648u, 3283861642u, 1144575191u, 3196987674u, 1059649139u, 3193312977u, 1059749949u}},
        }};
        for (const auto& example : examples)
        {
            osg::Matrixf world;
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned col = 0; col < 3; ++col)
                    world(col, row) = std::bit_cast<float>(example.mWorld[row * 3 + col]);
            for (unsigned axis = 0; axis < 3; ++axis)
                world(3, axis) = std::bit_cast<float>(example.mWorld[9 + axis]);
            const auto actual = NifBullet::ragdollNativeBlendSceneTargetPose(world);
            for (unsigned axis = 0; axis < 3; ++axis)
                EXPECT_EQ(std::bit_cast<uint32_t>(actual.mPosition[axis]), example.mTarget[axis]);
            for (unsigned axis = 0; axis < 4; ++axis)
                EXPECT_EQ(std::bit_cast<uint32_t>(actual.mRotation[axis]), example.mTarget[3 + axis]);
        }
    }

    TEST(RagdollNativeBlendScene, KeepsDistinctNativeBlendAndKeyframedPreparation)
    {
        const std::array<uint32_t, 12> input{1065307251u, 3168475669u, 1032666985u, 1022046214u, 1065339889u, 3168954819u, 3180044065u, 1022493549u, 1065306842u, 3305571379u, 1168841500u, 3305231177u};
        const std::array<uint32_t, 4> raw{1013605850u, 1024232347u, 1013142243u, 1065340003u};
        const std::array<uint32_t, 4> key{1013605851u, 1024232347u, 1013142243u, 1065340003u};
        osg::Matrixf world;
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned col = 0; col < 3; ++col)
                world(col, row) = std::bit_cast<float>(input[row * 3 + col]);
        for (unsigned axis = 0; axis < 3; ++axis)
            world(3, axis) = std::bit_cast<float>(input[9 + axis]);
        const auto blend = NifBullet::ragdollNativeBlendSceneTargetPose(world);
        const auto synchronized = NifBullet::ragdollNativeSceneTargetPose(world);
        for (unsigned axis = 0; axis < 4; ++axis)
        {
            EXPECT_EQ(std::bit_cast<uint32_t>(blend.mRotation[axis]), raw[axis]);
            EXPECT_EQ(std::bit_cast<uint32_t>(synchronized.mRotation[axis]), key[axis]);
        }
        EXPECT_NE(blend.mRotation, synchronized.mRotation);
    }

    TEST(RagdollNativeBlendScene, RejectsInvalidWorldMatricesBeforePreparingTargets)
    {
        EXPECT_THROW(NifBullet::ragdollNativeBlendSceneTargetPose(osg::Matrixf::scale(2, 1, 1)), std::invalid_argument);
        auto matrix = osg::Matrixf::identity();
        matrix(0, 3) = .1f;
        EXPECT_THROW(NifBullet::ragdollNativeBlendSceneTargetPose(matrix), std::invalid_argument);
        matrix = osg::Matrixf::identity();
        matrix(3, 2) = std::numeric_limits<float>::infinity();
        EXPECT_THROW(NifBullet::ragdollNativeBlendSceneTargetPose(matrix), std::invalid_argument);
        matrix = osg::Matrixf::identity();
        matrix(3, 0) = std::numeric_limits<float>::max();
        // This finite world position remains representable in native lengths.
        EXPECT_NO_THROW(NifBullet::ragdollNativeBlendSceneTargetPose(matrix));
    }
}


namespace
{
    TEST(RagdollNativeMixedRenderer, OrdinaryReturnSumsDiagonalsBeforeSubtraction)
    {
        const std::array<uint32_t, 4> quaternion{0u, 864090906u, 1060439284u, 1060439282u};
        const std::array<uint32_t, 9> expected{3019898880u, 3212836863u, 867592206u, 1065353215u, 3019898880u, 867592208u, 3015075854u, 867592208u, 1065353216u};
        NifBullet::RagdollNativeTargetPose input{{0, 0, 0}, {}};
        for (unsigned axis = 0; axis < 4; ++axis)
            input.mRotation[axis] = std::bit_cast<float>(quaternion[axis]);
        const auto actual = NifBullet::ragdollBoneWorldFromNativePose(input);
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned col = 0; col < 3; ++col)
                EXPECT_EQ(std::bit_cast<uint32_t>(actual(col, row)), expected[row * 3 + col]);
    }

    TEST(RagdollNativeMixedRenderer, KeepsUnstoredWYAndWZProducts)
    {
        const std::array<uint32_t, 4> quaternion{3144308502u, 3154936512u, 3143845009u, 1065352391u};
        const std::array<uint32_t, 9> expected{1065350345u, 1004880946u, 3163311353u, 3152101175u, 1065352383u, 1005340441u, 1015854334u, 3152568621u, 1065350319u};
        NifBullet::RagdollNativeTargetPose input{{7, -14, 21}, {}};
        for (unsigned axis = 0; axis < 4; ++axis)
            input.mRotation[axis] = std::bit_cast<float>(quaternion[axis]);
        const auto actual = NifBullet::ragdollBoneWorldFromNativeBlendPose(input);
        const auto ordinary = NifBullet::ragdollBoneWorldFromNativePose(input);
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned col = 0; col < 3; ++col)
                EXPECT_EQ(std::bit_cast<uint32_t>(actual(col, row)), expected[row * 3 + col]);
        EXPECT_NE(actual, ordinary);
        EXPECT_FLOAT_EQ(actual(3, 0), 48.993282318115234f);
        EXPECT_FLOAT_EQ(actual(3, 1), -97.98656463623047f);
        EXPECT_FLOAT_EQ(actual(3, 2), 146.97984313964844f);
    }

    TEST(RagdollNativeMixedRenderer, SelectsNativeAnimatedOrMixedPositionAndKeepsDriveTarget)
    {
        struct Example
        {
            std::array<uint32_t, 7> mPhysical;
            std::array<uint32_t, 12> mAnimated;
            float mGain;
            uint16_t mFlags;
            std::array<uint32_t, 7> mDrive;
            std::array<uint32_t, 12> mScene;
        };
        const std::array<Example, 4> examples{{
            {{0u, 2147483648u, 0u, 0u, 0u, 0u, 1065353216u}, {1065307251u, 3168475669u, 1032666985u, 1022046214u, 1065339889u, 3168954819u, 3180044065u, 1022493549u, 1065306842u, 3305571379u, 1168841500u, 3305231177u}, 0.25f, 8u, {3264893788u, 1128502159u, 3264504932u, 996830902u, 1007456490u, 996367226u, 1065352390u}, {1065350342u, 3152106935u, 1015857969u, 1004886913u, 1065352383u, 3152574567u, 3163314967u, 1005346589u, 1065350317u, 3305571379u, 1168841499u, 3305231177u}},
            {{0u, 2147483648u, 0u, 0u, 0u, 0u, 1065353216u}, {1065307251u, 3168475669u, 1032666985u, 1022046214u, 1065339889u, 3168954819u, 3180044065u, 1022493549u, 1065306842u, 3305571379u, 1168841500u, 3305231177u}, 0.25f, 264u, {3264893788u, 1128502159u, 3264504932u, 996830902u, 1007456490u, 996367226u, 1065352390u}, {1065350342u, 3152106935u, 1015857969u, 1004886913u, 1065352383u, 3152574567u, 3163314967u, 1005346589u, 1065350317u, 3288794163u, 1152064283u, 3288453961u}},
            {{1140745618u, 1142487993u, 3236170880u, 3194691138u, 1030992394u, 3196589820u, 1064252517u}, {3209445616u, 3206162134u, 3169483181u, 3190401618u, 1043435083u, 1064863572u, 3205784028u, 1061650863u, 3195262112u, 3321068162u, 1151696612u, 3315197105u}, -0.25f, 8u, {1147441828u, 1144219656u, 1126628986u, 3188158504u, 3193196672u, 3203450622u, 1062795766u}, {1056015598u, 1062870382u, 3194520584u, 3208513680u, 1057345589u, 1054253856u, 1056154737u, 3169790456u, 1063323747u, 3321068161u, 1151696611u, 3315197105u}},
            {{1140745618u, 1142487993u, 3236170880u, 3194691138u, 1030992394u, 3196589820u, 1064252517u}, {3209445616u, 3206162134u, 3169483181u, 3190401618u, 1043435083u, 1064863572u, 3205784028u, 1061650863u, 3195262112u, 3321068162u, 1151696612u, 3315197105u}, 1.25f, 264u, {3300775486u, 1118237032u, 3294326447u, 3190728606u, 1060413149u, 1059828387u, 3189521375u}, {3211079640u, 3172174291u, 3202572003u, 3202614081u, 1026337567u, 1063581038u, 3160217916u, 1065327406u, 3176942017u, 3324384674u, 1142033946u, 3317692956u}},
        }};
        for (const auto& example : examples)
        {
            NifBullet::RagdollNativeTargetPose physical;
            for (unsigned axis = 0; axis < 3; ++axis)
                physical.mPosition[axis] = std::bit_cast<float>(example.mPhysical[axis]);
            for (unsigned axis = 0; axis < 4; ++axis)
                physical.mRotation[axis] = std::bit_cast<float>(example.mPhysical[3 + axis]);
            osg::Matrixf animated;
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned col = 0; col < 3; ++col)
                    animated(col, row) = std::bit_cast<float>(example.mAnimated[row * 3 + col]);
            for (unsigned axis = 0; axis < 3; ++axis)
                animated(3, axis) = std::bit_cast<float>(example.mAnimated[9 + axis]);
            const auto actual = NifBullet::ragdollNativeBlendPoseTargets(physical, animated, example.mGain, example.mFlags);
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                EXPECT_EQ(std::bit_cast<uint32_t>(actual.mDriveTarget.mPosition[axis]), example.mDrive[axis]);
                EXPECT_EQ(std::bit_cast<uint32_t>(actual.mSceneTarget(3, axis)), example.mScene[9 + axis]);
            }
            for (unsigned axis = 0; axis < 4; ++axis)
                EXPECT_EQ(std::bit_cast<uint32_t>(actual.mDriveTarget.mRotation[axis]), example.mDrive[3 + axis]);
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned col = 0; col < 3; ++col)
                    EXPECT_EQ(std::bit_cast<uint32_t>(actual.mSceneTarget(col, row)), example.mScene[row * 3 + col]);
        }
    }

    TEST(RagdollNativeMixedRenderer, RejectsInvalidPoseGainAndUnrepresentableScene)
    {
        NifBullet::RagdollNativeTargetPose physical{{0, 0, 0}, {0, 0, 0, 1}};
        EXPECT_THROW(NifBullet::ragdollNativeBlendPoseTargets(physical, osg::Matrixf::identity(),
            std::numeric_limits<float>::infinity(), 8), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeBlendPoseTargets(physical, osg::Matrixf::scale(2, 1, 1), .5f, 8),
            std::invalid_argument);
        physical.mRotation[3] = 2;
        EXPECT_THROW(NifBullet::ragdollNativeBlendPoseTargets(physical, osg::Matrixf::identity(), .5f, 8),
            std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollBoneWorldFromNativeBlendPose(physical), std::invalid_argument);
        physical.mRotation = {0, 0, 0, 1};
        physical.mPosition[0] = std::numeric_limits<float>::max();
        EXPECT_THROW(NifBullet::ragdollBoneWorldFromNativeBlendPose(physical), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeBlendPoseTargets(physical, osg::Matrixf::identity(), 0.f, 0x108),
            std::invalid_argument);
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, BodyTControllerSynchronizesAndProjectsSceneOffsets)
    {
        auto& definition = mGraph.mBodies[0];
        definition.mUsesRigidBodyTransform = true;
        definition.mTranslation = {2, 3, 4};
        definition.mRotation = osg::Quat(0, 0, 1, 0);
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        std::array<NifBullet::RagdollNativeBlendUpdate, 1> updates{{
            {12, osg::Matrixf::identity(), 1, .5f, 8}}};
        const auto key = actor.updateNativeBlends(updates, 1, 0, 0);
        ASSERT_EQ(key.size(), 1);
        EXPECT_FALSE(key[0].mSceneTarget);
        const auto state = actor.capture()[0];
        EXPECT_EQ(state.mPose.getOrigin(), btVector3(2, 3, 4));
        EXPECT_NEAR(std::abs(state.mPose.getRotation().z()), 1, 1e-6);
        updates[0].mHierarchyGain = 0;
        updates[0].mVelocityGain = 0;
        updates[0].mCollisionFlags = 0;
        const auto dynamic = actor.updateNativeBlends(updates, 1, 0, 0);
        ASSERT_EQ(dynamic.size(), 1);
        ASSERT_TRUE(dynamic[0].mSceneTarget);
        EXPECT_EQ(*dynamic[0].mSceneTarget, osg::Matrixf::identity());
        EXPECT_EQ(actor.capture()[0].mPose, state.mPose);
    }

    TEST_F(ActorRagdollPhysicsTest, BodyTControllerDriveUsesSceneTargetAndPhysicalBasisCenter)
    {
        auto& definition = mGraph.mBodies[0];
        definition.mUsesRigidBodyTransform = true;
        definition.mTranslation = {2, 3, 4};
        definition.mRotation = osg::Quat(0, 0, 1, 0);
        mPoses[0] = btTransform(btQuaternion(0, 0, 1, 0), btVector3(2, 3, 4));
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeBlendUpdate, 1> updates{{
            {12, osg::Matrixf::identity(), 0, .5f, 8}}};
        const auto result = actor.updateNativeBlends(updates, 1, 0, 0);
        ASSERT_EQ(result.size(), 1);
        ASSERT_TRUE(result[0].mSceneTarget);
        EXPECT_EQ(*result[0].mSceneTarget, osg::Matrixf::identity());
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(-2, -3, 0));
        EXPECT_EQ(actor.capture()[0].mAngularVelocity, btVector3(0, 0, 0));
        EXPECT_EQ(actor.capture()[0].mPose, mPoses[0]);
    }

    TEST_F(ActorRagdollPhysicsTest, BodyTOwnedOffsetsSurviveSourceChangesAndServeExplicitAdapters)
    {
        auto& definition = mGraph.mBodies[0];
        definition.mUsesRigidBodyTransform = true;
        definition.mTranslation = {2, 3, 4};
        definition.mRotation = osg::Quat(0, 0, 1, 0);
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        definition.mTranslation = {100, 200, 300};
        definition.mRotation = osg::Quat(0, 0, 0, 0);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{
            {12, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.setNativeMotionModes(key);
        const std::array<NifBullet::RagdollNativeScenePoseRequest, 1> poses{{
            {12, osg::Matrixf::identity()}}};
        actor.synchronizeNativeKeyframedPoses(poses);
        EXPECT_EQ(actor.capture()[0].mPose.getOrigin(), btVector3(2, 3, 4));
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> dynamic{{
            {12, NifBullet::RagdollNativeMotion::Dynamic}}};
        actor.setNativeMotionModes(dynamic);
        const std::array<NifBullet::RagdollNativeVelocityDrive, 1> drives{{
            {12, {{0, 0, 0}, {0, 0, 0, 1}}, .5f}}};
        actor.driveNativePoseVelocities(drives, 1, 0);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(-2, -3, 0));
        EXPECT_EQ(actor.capture()[0].mAngularVelocity, btVector3(0, 0, 0));
    }

    TEST_F(ActorRagdollPhysicsTest, BodyTConstructionRejectsLateInvalidOffsetBeforeRegistration)
    {
        addHinge();
        for (auto& body : mGraph.mBodies)
        {
            body.mUsesRigidBodyTransform = true;
            body.mRotation = osg::Quat(0, 0, 0, 1);
        }
        mGraph.mBodies.back().mRotation = osg::Quat(0, 0, 0, 0);
        EXPECT_THROW((NifBullet::ActorRagdollPhysics(mGraph, mWorld, 1, mPoses, 1, -1)), std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        EXPECT_EQ(mWorld.getNumConstraints(), 0);
        mGraph.mBodies.back().mRotation = osg::Quat(0, 0, 0, 1);
        mGraph.mBodies.back().mTranslation.x() = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW((NifBullet::ActorRagdollPhysics(mGraph, mWorld, 1, mPoses, 1, -1)), std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeBlendControllerEntersKeyframedClearsVelocitiesAndSyncsScene)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        body->setLinearVelocity({1, 2, 3}); body->setAngularVelocity({4, 5, 6});
        body->applyCentralForce({7, 8, 9});
        const auto* shape = body->getCollisionShape(); const auto* proxy = body->getBroadphaseHandle();
        const auto anim = osg::Matrixf::translate(70, -35, 21);
        const std::array<NifBullet::RagdollNativeBlendUpdate, 1> requests{{{12, anim, 1, .5f, 0x108}}};
        const auto result = actor.updateNativeBlends(requests, 1.f/120, 0, 0);
        ASSERT_EQ(result.size(), 1); EXPECT_EQ(result[0].mRecord, 12); EXPECT_EQ(result[0].mCollisionFlags, 0x100);
        EXPECT_FALSE(result[0].mSceneTarget); EXPECT_TRUE(body->isKinematicObject());
        EXPECT_EQ(body->getLinearVelocity(), btVector3(0, 0, 0)); EXPECT_EQ(body->getAngularVelocity(), btVector3(0, 0, 0));
        const auto expected = NifBullet::ragdollNativePoseFromBoneWorld(anim);
        EXPECT_EQ(body->getWorldTransform(), expected);
        EXPECT_EQ(body->getInterpolationWorldTransform(), body->getWorldTransform());
        EXPECT_EQ(body->getTotalForce(), btVector3(7, 8, 9)); EXPECT_EQ(body->getCollisionShape(), shape);
        EXPECT_EQ(body->getBroadphaseHandle(), proxy);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeBlendControllerLeavesKeyframedSyncsBeforeOrdinaryReturn)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.setNativeMotionModes(key);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        body->setLinearVelocity({1, 2, 3}); body->setAngularVelocity({4, 5, 6});
        const auto anim = osg::Matrixf::translate(70, -35, 21);
        const std::array<NifBullet::RagdollNativeBlendUpdate, 1> requests{{{12, anim, 0, 0, 0}}};
        const auto result = actor.updateNativeBlends(requests, 1.f/120, 0, 0);
        ASSERT_EQ(result.size(), 1); EXPECT_EQ(result[0].mCollisionFlags, 8);
        ASSERT_TRUE(result[0].mSceneTarget); EXPECT_FALSE(body->isKinematicObject());
        const auto expected = NifBullet::ragdollNativeSceneTargetPose(anim);
        EXPECT_EQ(actor.capture()[0].mPose, NifBullet::ragdollNativePoseFromBoneWorld(anim));
        EXPECT_EQ(*result[0].mSceneTarget, NifBullet::ragdollBoneWorldFromNativePose(expected));
        EXPECT_EQ(body->getLinearVelocity(), btVector3(1, 2, 3)); EXPECT_EQ(body->getAngularVelocity(), btVector3(4, 5, 6));
    }

    TEST_F(ActorRagdollPhysicsTest, NativeBlendControllerDrivesMixedPoseAndSuppressesNonzeroSelectorScene)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        const auto pose = actor.capture()[0].mPose;
        std::array<NifBullet::RagdollNativeBlendUpdate, 1> requests{{{12, osg::Matrixf::translate(70, 0, 14), .25f, .5f, 8}}};
        const auto result = actor.updateNativeBlends(requests, 1.f/120, 0, 0);
        ASSERT_EQ(result.size(), 1); ASSERT_TRUE(result[0].mSceneTarget);
        EXPECT_GT(actor.capture()[0].mLinearVelocity.x(), 0); EXPECT_EQ(actor.capture()[0].mPose, pose);
        EXPECT_NEAR(result[0].mSceneTarget->getTrans().x(), 70, .0001);
        const auto other = actor.updateNativeBlends(requests, 1.f/120, 1, 0);
        ASSERT_EQ(other.size(), 1); EXPECT_FALSE(other[0].mSceneTarget);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeBlendControllerRejectsLateInvalidRequestWithoutPartialMutation)
    {
        addHinge(); NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        first->setLinearVelocity({1, 2, 3}); first->applyCentralForce({4, 5, 6});
        std::array<NifBullet::RagdollNativeBlendUpdate, 2> requests{{
            {12, osg::Matrixf::translate(70, 0, 14), 1, .5f, 8},
            {24, osg::Matrixf::scale(2, 2, 2), 1, .5f, 8}}};
        const auto pose = first->getWorldTransform();
        EXPECT_THROW(actor.updateNativeBlends(requests, 1.f/120, 0, 0), std::invalid_argument);
        requests[1].mAnimatedWorld.makeIdentity(); requests[1].mRecord = 999;
        EXPECT_THROW(actor.updateNativeBlends(requests, 1.f/120, 0, 0), std::invalid_argument);
        requests[1].mRecord = 12;
        EXPECT_THROW(actor.updateNativeBlends(requests, 1.f/120, 0, 0), std::invalid_argument);
        requests[1].mRecord = 24; requests[1].mVelocityGain = std::numeric_limits<float>::infinity();
        EXPECT_THROW(actor.updateNativeBlends(requests, 1.f/120, 0, 0), std::invalid_argument);
        EXPECT_EQ(first->getWorldTransform(), pose); EXPECT_EQ(first->getLinearVelocity(), btVector3(1, 2, 3));
        EXPECT_EQ(first->getTotalForce(), btVector3(4, 5, 6)); EXPECT_FALSE(first->isKinematicObject());
    }

    TEST_F(ActorRagdollPhysicsTest, NativeBlendControllerSkippedAndEmptyBatchesLeaveSleepingBodiesAlone)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]); body->setActivationState(ISLAND_SLEEPING);
        const std::array<NifBullet::RagdollNativeBlendUpdate, 1> skip{{{12, osg::Matrixf::scale(2, 2, 2), 0, .5f, 8}}};
        EXPECT_TRUE(actor.updateNativeBlends(skip, 1.f/120, 1, 0).empty());
        EXPECT_TRUE(actor.updateNativeBlends({}, 1.f/120, 0, 0).empty()); EXPECT_FALSE(body->isActive());
        EXPECT_THROW(actor.updateNativeBlends({}, -1, 0, 0), std::invalid_argument);
        EXPECT_THROW(actor.updateNativeBlends({}, 1.f/120, 0, std::numeric_limits<float>::infinity()), std::invalid_argument);
    }
    TEST_F(ActorRagdollPhysicsTest, NativeWorldAdvancesKeyframedCenterWithoutOverwritingVelocity)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        world.setGravity({0, 0, -1000});
        mGraph.mBodies[0].mCenter = {1, 0, 0};
        NifBullet::ActorRagdollPhysics actor(mGraph, world, 2, mPoses, 1, -1);
        actor.setNativeMotionModes(std::array<NifBullet::RagdollNativeMotionRequest, 1>{{
            {12, NifBullet::RagdollNativeMotion::Keyframed}}});
        auto states = actor.capture(); states[0].mLinearVelocity = {2, 4, 6}; actor.restore(states);
        world.stepSimulation(.05, 0);
        const auto state = actor.capture()[0];
        EXPECT_NEAR(state.mPose.getOrigin().x(), .1, 1e-6);
        EXPECT_NEAR(state.mPose.getOrigin().y(), .2, 1e-6);
        EXPECT_NEAR(state.mPose.getOrigin().z(), 2.3, 1e-6);
        EXPECT_EQ(state.mLinearVelocity, btVector3(2, 4, 6));
        EXPECT_EQ(state.mAngularVelocity, btVector3(0, 0, 0));
    }

    TEST_F(ActorRagdollPhysicsTest, NativeWorldUsesActualSubstepsAndDoesNotAdvanceAnEmptyStep)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        NifBullet::ActorRagdollPhysics actor(mGraph, world, 1, mPoses, 1, -1);
        actor.setNativeMotionModes(std::array<NifBullet::RagdollNativeMotionRequest, 1>{{
            {12, NifBullet::RagdollNativeMotion::Keyframed}}});
        auto states = actor.capture(); states[0].mLinearVelocity = {4, 0, 0}; actor.restore(states);
        world.stepSimulation(.01, 2, .02);
        EXPECT_EQ(actor.capture()[0].mPose.getOrigin(), states[0].mPose.getOrigin());
        world.stepSimulation(.01, 2, .02);
        EXPECT_NEAR(actor.capture()[0].mPose.getOrigin().x(), .08, 1e-6);
        world.stepSimulation(.04, 2, .02);
        EXPECT_NEAR(actor.capture()[0].mPose.getOrigin().x(), .24, 1e-6);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(4, 0, 0));
    }

    TEST_F(ActorRagdollPhysicsTest, NativeWorldRemovesOwnerBeforeDestructionAndAllowsReplacement)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        for (unsigned i = 0; i < 3; ++i)
        {
            {
                NifBullet::ActorRagdollPhysics actor(mGraph, world, 1, mPoses, 1, -1);
                actor.setNativeMotionModes(std::array<NifBullet::RagdollNativeMotionRequest, 1>{{
                    {12, NifBullet::RagdollNativeMotion::Keyframed}}});
                auto states = actor.capture(); states[0].mLinearVelocity = {1, 0, 0}; actor.restore(states);
                world.stepSimulation(.05, 0);
                EXPECT_NEAR(actor.capture()[0].mPose.getOrigin().x(), .05, 1e-6);
            }
            EXPECT_EQ(world.getNumCollisionObjects(), 0);
            world.stepSimulation(.05, 0);
        }
    }

    TEST_F(ActorRagdollPhysicsTest, NativeKeyframedStepRejectsBatchBeforeMutatingAnyBody)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        actor.setNativeMotionModes(std::array<NifBullet::RagdollNativeMotionRequest, 2>{{
            {12, NifBullet::RagdollNativeMotion::Keyframed}, {24, NifBullet::RagdollNativeMotion::Keyframed}}});
        auto states = actor.capture(); states[0].mLinearVelocity = {1, 0, 0};
        states[1].mLinearVelocity = {std::numeric_limits<float>::max(), 0, 0}; actor.restore(states);
        EXPECT_THROW(actor.stepNativeKeyframedMotion(.05f), std::invalid_argument);
        EXPECT_EQ(actor.capture()[0].mPose.getOrigin(), states[0].mPose.getOrigin());
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, states[0].mLinearVelocity);
        EXPECT_THROW(actor.stepNativeKeyframedMotion(-1), std::invalid_argument);
        EXPECT_THROW(actor.stepNativeKeyframedMotion(std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeWorldPreservesDynamicAndUnrelatedKinematicBehavior)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        world.setGravity({0, 0, 0});
        NifBullet::ActorRagdollPhysics actor(mGraph, world, 1, mPoses, 1, -1);
        auto states = actor.capture(); states[0].mLinearVelocity = {3, 0, 0}; actor.restore(states);
        btSphereShape shape(.25); btRigidBody body(0, nullptr, &shape);
        body.setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT);
        body.forceActivationState(DISABLE_DEACTIVATION);
        body.setWorldTransform(btTransform(btQuaternion::getIdentity(), {20, 0, 0}));
        body.setInterpolationWorldTransform(btTransform(btQuaternion::getIdentity(), {18, 0, 0}));
        world.addRigidBody(&body);
        world.stepSimulation(.05, 0);
        EXPECT_NEAR(actor.capture()[0].mPose.getOrigin().x(), .15, 1e-6);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(3, 0, 0));
        EXPECT_NEAR(body.getLinearVelocity().x(), 40, 1e-6);
        world.removeRigidBody(&body);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeWorldContactUsesPreservedKeyframedVelocity)
    {
        addHinge();
        mGraph.mJoints.clear(); // Contact response must not come from the hinge.
        mPoses[0].setOrigin({0, 0, 2});
        mPoses[1].setOrigin({1, 0, 2});
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        world.setGravity({0, 0, 0});
        NifBullet::ActorRagdollPhysics actor(mGraph, world, 1, mPoses, 1, -1);
        actor.setNativeMotionModes(std::array<NifBullet::RagdollNativeMotionRequest, 1>{{
            {12, NifBullet::RagdollNativeMotion::Keyframed}}});
        auto states = actor.capture(); states[0].mLinearVelocity = {10, 0, 0}; actor.restore(states);
        world.stepSimulation(.05, 0);
        const auto result = actor.capture();
        EXPECT_NEAR(result[0].mPose.getOrigin().x(), .5, 1e-6);
        EXPECT_EQ(result[0].mLinearVelocity, btVector3(10, 0, 0));
        EXPECT_GT(result[1].mLinearVelocity.x(), 0);
        EXPECT_GT(mDispatcher.getNumManifolds(), 0);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeWorldRestoresDynamicMotionAfterKeyframedSubstep)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        world.setGravity({0, 0, 0});
        NifBullet::ActorRagdollPhysics actor(mGraph, world, 1, mPoses, 1, -1);
        actor.setNativeMotionModes(std::array<NifBullet::RagdollNativeMotionRequest, 1>{{
            {12, NifBullet::RagdollNativeMotion::Keyframed}}});
        auto states = actor.capture(); states[0].mLinearVelocity = {1000, 0, 0}; actor.restore(states);
        world.stepSimulation(.05, 0);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(250, 0, 0));
        EXPECT_NEAR(actor.capture()[0].mPose.getOrigin().x(), 12.5, 1e-6);
        actor.setNativeMotionModes(std::array<NifBullet::RagdollNativeMotionRequest, 1>{{
            {12, NifBullet::RagdollNativeMotion::Dynamic}}});
        states = actor.capture(); states[0].mLinearVelocity = {3, 0, 0}; actor.restore(states);
        actor.applyImpulse(0, {2, 0, 0}, states[0].mPose.getOrigin());
        world.stepSimulation(.05, 0);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(4, 0, 0));
        EXPECT_NEAR(actor.capture()[0].mPose.getOrigin().x(), 12.7, 1e-6);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeWorldRejectsDuplicateForeignAndMissingOwnerRegistrations)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        NifBullet::ActorRagdollPhysics actor(mGraph, world, 1, mPoses, 1, -1);
        const auto bodies = actor.collisionObjects();
        const auto step = [](float) {};
        int identity = 0;
        EXPECT_THROW(world.registerNativeMotionOwner(nullptr, bodies, step), std::invalid_argument);
        EXPECT_THROW(world.registerNativeMotionOwner(&actor, bodies, step), std::invalid_argument);
        EXPECT_THROW(world.registerNativeMotionOwner(&identity, bodies, step), std::invalid_argument);
        EXPECT_THROW(world.registerNativeMotionOwner(&identity, {}, step), std::invalid_argument);
        EXPECT_THROW(world.registerNativeMotionOwner(&identity, bodies, {}), std::invalid_argument);
        btSphereShape shape(.25); btRigidBody foreign(1, nullptr, &shape, {1, 1, 1});
        const std::array<btCollisionObject*, 1> foreignBodies{&foreign};
        const std::array<btCollisionObject*, 1> nullBodies{nullptr};
        EXPECT_THROW(world.registerNativeMotionOwner(&identity, foreignBodies, step), std::invalid_argument);
        EXPECT_THROW(world.registerNativeMotionOwner(&identity, nullBodies, step), std::invalid_argument);
        EXPECT_EQ(world.getNumCollisionObjects(), 1);
        world.unregisterNativeMotionOwner(&identity);
        actor.setNativeMotionModes(std::array<NifBullet::RagdollNativeMotionRequest, 1>{{
            {12, NifBullet::RagdollNativeMotion::Keyframed}}});
        auto states = actor.capture(); states[0].mLinearVelocity = {1, 0, 0}; actor.restore(states);
        world.stepSimulation(.05, 0);
        EXPECT_NEAR(actor.capture()[0].mPose.getOrigin().x(), .05, 1e-6);
    }

    TEST(RagdollNativeKeyframedStep, AdvancesPhysicalCenterAndRebuildsOriginFromLocalCenter)
    {
        const auto step = NifBullet::ragdollNativeKeyframedMotionStep({4, 5, 6}, {0, 0, 0, 1},
            {1, 0, 0}, {{1, 2, 3}, {0, 0, 0}}, .05f, 250.f, 31.4159f);
        EXPECT_EQ(step.mCenterOfMass, osg::Vec3f(std::bit_cast<float>(1082235290u),
            std::bit_cast<float>(1084437299u), std::bit_cast<float>(1086639309u)));
        EXPECT_EQ(step.mBodyPose.mPosition, osg::Vec3f(std::bit_cast<float>(1078145844u),
            std::bit_cast<float>(1084437299u), std::bit_cast<float>(1086639309u)));
        EXPECT_EQ(step.mVelocities.mLinear, osg::Vec3f(1, 2, 3));
        EXPECT_EQ(step.mVelocities.mAngular, osg::Vec3f());
        EXPECT_EQ(step.mBodyPose.mRotation, (std::array<float, 4>{0, 0, 0, 1}));
    }

    TEST(RagdollNativeKeyframedStep, UsesOriginalQuaternionIncrementAndAngularCache)
    {
        // Independently captured original8EA4B0 identity fixture, oracle02 row1868.
        const auto step = NifBullet::ragdollNativeKeyframedMotionStep({4, 5, 6}, {0, 0, 0, 1},
            {1, 0, 0}, {{1, 2, 3}, {.1f, .2f, .3f}}, .05f, 250.f, 31.4159f);
        const std::array<std::uint32_t, 4> q{992204399u, 1000593007u, 1005961638u, 1065352482u};
        const std::array<std::uint32_t, 4> delta{1000593163u, 1008981771u, 1014350480u, 1016676896u};
        for (unsigned i = 0; i < 4; ++i)
        {
            EXPECT_NEAR(step.mBodyPose.mRotation[i], std::bit_cast<float>(q[i]), 1e-6);
            EXPECT_NEAR(step.mAngularDelta[i], std::bit_cast<float>(delta[i]), 1e-6);
        }
        const auto capped = NifBullet::ragdollNativeKeyframedMotionStep({4, 5, 6}, {0, 0, 0, 1},
            {1, 0, 0}, {{1000, -2000, 3000}, {1000, 2000, -3000}}, .2f, 0.f, 0.f);
        EXPECT_EQ(capped.mVelocities.mLinear, osg::Vec3f());
        EXPECT_EQ(capped.mVelocities.mAngular, osg::Vec3f());
        EXPECT_EQ(capped.mCenterOfMass, osg::Vec3f(4, 5, 6));
        EXPECT_EQ(capped.mBodyPose.mRotation, (std::array<float, 4>{0, 0, 0, 1}));
    }

    TEST(RagdollNativeKeyframedStep, AppliesWorldAngularIncrementBeforeCurrentRotation)
    {
        // Full original8EA4B0 oracle02 row116: a rotated physical body.
        const auto f = [](std::uint32_t bits) { return std::bit_cast<float>(bits); };
        const auto step = NifBullet::ragdollNativeKeyframedMotionStep(
            {f(1147962206u), f(3298255023u), f(1125857666u)},
            {f(1060439283u), f(3207922931u), f(608677126u), f(608677126u)},
            {f(1066608646u), f(1072082607u), f(3218266049u)},
            {{1, 2, 3}, {.1f, .2f, .3f}}, f(1007192201u), 1.f, 1.f);
        const std::array<std::uint32_t, 4> expectedQ{1060454098u, 3207908088u, 3127358567u, 966424644u};
        const std::array<std::uint32_t, 3> expectedOrigin{1147991677u, 3298245521u, 1125750019u};
        for (unsigned i = 0; i < 4; ++i)
            EXPECT_NEAR(step.mBodyPose.mRotation[i], f(expectedQ[i]), 1e-6);
        for (unsigned i = 0; i < 3; ++i)
            EXPECT_NEAR(step.mBodyPose.mPosition[i], f(expectedOrigin[i]), .001);
    }

    TEST(RagdollNativeKeyframedStep, ZeroFrameRetainsVelocitySignedZeros)
    {
        const auto step = NifBullet::ragdollNativeKeyframedMotionStep({4, 5, 6}, {0, 0, 0, 1},
            {1, 0, 0}, {{-0.f, 0.f, -0.f}, {-0.f, 0.f, -0.f}}, 0.f, 250.f, 31.4159f);
        for (unsigned i : {0u, 2u})
        {
            EXPECT_EQ(std::bit_cast<std::uint32_t>(step.mVelocities.mLinear[i]), 0x80000000u);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(step.mVelocities.mAngular[i]), 0x80000000u);
        }
        EXPECT_EQ(step.mCenterOfMass, osg::Vec3f(4, 5, 6));
        EXPECT_EQ(step.mBodyPose.mPosition, osg::Vec3f(3, 5, 6));
    }

    TEST(RagdollNativeKeyframedStep, RejectsMalformedPhysicalInputsAndUnrepresentableResults)
    {
        const auto call = [](osg::Vec3f center, std::array<float, 4> q, osg::Vec3f local,
                              NifBullet::RagdollNativeVelocities velocity, float frame, float linear, float angular) {
            return NifBullet::ragdollNativeKeyframedMotionStep(center, q, local, velocity, frame, linear, angular);
        };
        const float bad = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(call({bad, 0, 0}, {0, 0, 0, 1}, {}, {}, 1, 250, 1), std::invalid_argument);
        EXPECT_THROW(call({}, {0, 0, 0, 0}, {}, {}, 1, 250, 1), std::invalid_argument);
        EXPECT_THROW(call({}, {0, 0, 0, bad}, {}, {}, 1, 250, 1), std::invalid_argument);
        EXPECT_THROW(call({}, {0, 0, 0, 1}, {bad, 0, 0}, {}, 1, 250, 1), std::invalid_argument);
        EXPECT_THROW(call({}, {0, 0, 0, 1}, {}, {{bad, 0, 0}, {}}, 1, 250, 1), std::invalid_argument);
        EXPECT_THROW(call({}, {0, 0, 0, 1}, {}, {}, -1, 250, 1), std::invalid_argument);
        EXPECT_THROW(call({}, {0, 0, 0, 1}, {}, {}, 1, -1, 1), std::invalid_argument);
        EXPECT_THROW(call({}, {0, 0, 0, 1}, {}, {}, 1, 250, bad), std::invalid_argument);
        EXPECT_THROW(call({}, {0, 0, 0, 1}, {}, {{std::numeric_limits<float>::max(), 0, 0}, {}},
            1, 250, 1), std::invalid_argument);
    }

}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, OwnedBlendControllersRetainLoadedBoundsAndDetachedSourceKeys)
    {
        auto& body = mGraph.mBodies.front();
        body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        const auto max = std::numeric_limits<float>::max();
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xcd, 1.f, 0.f, max, -max, {{0.f, 1.f, 1.f}, {.25f, 0.f, 0.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        mGraph.mBodies.clear();
        const auto controllers = actor.captureNativeBlendControllers();
        ASSERT_EQ(controllers.size(), 1u);
        EXPECT_EQ(controllers[0].mRecord, 78u);
        EXPECT_EQ(controllers[0].mTargetNode, 8u);
        EXPECT_EQ(controllers[0].mState.mKeys.size(), 2u);
        EXPECT_FLOAT_EQ(controllers[0].mState.mTiming.mStartKey, 0.f);
        EXPECT_FLOAT_EQ(controllers[0].mState.mTiming.mStopKey, 0.f);
        EXPECT_FLOAT_EQ(controllers[0].mState.mClock.mPreviousTime, -max);
        const auto gains = actor.captureNativeBlendStates();
        ASSERT_EQ(gains.size(), 1u);
        EXPECT_FLOAT_EQ(gains[0].mGains.mHierarchy, .9f);
    }

    TEST_F(ActorRagdollPhysicsTest, OwnedBlendControllersPublishClockAndKeyframedBodyTogether)
    {
        auto& body = mGraph.mBodies.front();
        body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xcd, 1.f, 0.f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalBlendTimeCache cache;
        const std::array<NifBullet::RagdollNativeBlendControllerTarget, 1> targets{{{78, osg::Matrixf::identity()}}};
        const auto publications = actor.updateNativeBlendControllers(targets, 1.f, cache, 1.f / 120, 0, 0.f);
        ASSERT_EQ(publications.size(), 1u);
        EXPECT_EQ(publications[0].mRecord, 12u);
        EXPECT_FALSE(publications[0].mSceneTarget);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
        EXPECT_EQ(actor.capture()[0].mPose.getOrigin(), btVector3(0, 0, 0));
        const auto controllers = actor.captureNativeBlendControllers();
        ASSERT_EQ(controllers.size(), 1u);
        EXPECT_FLOAT_EQ(controllers[0].mState.mClock.mPreviousTime, 1.f);
        EXPECT_FLOAT_EQ(controllers[0].mState.mCachedGains.mHierarchy, .9f);
        EXPECT_EQ(cache.mCycle, 2u);
        const auto finished = actor.updateNativeBlendControllers(targets, 1.25f, cache, 1.f / 120, 0, 0.f);
        EXPECT_EQ(finished.size(), 1u);
        EXPECT_TRUE(actor.captureNativeBlendControllers()[0].mState.mKeys.empty());
        EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mTiming.mFlags, 0xc5);
    }

    TEST_F(ActorRagdollPhysicsTest, OwnedBlendControllersResolveTargetNodeRatherThanAttachedBody)
    {
        addHinge();
        mGraph.mJoints.clear();
        mGraph.mBodies[0].mNodeRecord = 8;
        mGraph.mBodies[1].mNodeRecord = 20;
        mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        mGraph.mBodies[1].mBlend = NifBullet::RagdollBlendDefinition{31, 8, .7f, .6f};
        mGraph.mBodies[0].mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 20, 0xcd, 1.f, 0.f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalBlendTimeCache cache;
        const std::array<NifBullet::RagdollNativeBlendControllerTarget, 1> targets{{{78, osg::Matrixf::identity()}}};
        const auto publications = actor.updateNativeBlendControllers(targets, 0.f, cache, 1.f / 120, 0, 0.f);
        ASSERT_EQ(publications.size(), 1u);
        EXPECT_EQ(publications[0].mRecord, 24u);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        EXPECT_EQ(actor.captureNativeMotionModes()[1].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
        EXPECT_EQ(actor.capture()[1].mPose.getOrigin(), btVector3(0, 0, 0));
        const auto gains = actor.captureNativeBlendStates();
        ASSERT_EQ(gains.size(), 2u);
        EXPECT_FLOAT_EQ(gains[0].mGains.mHierarchy, .9f);
        EXPECT_FLOAT_EQ(gains[1].mGains.mHierarchy, 1.f);
        EXPECT_FLOAT_EQ(actor.captureNativeBlendControllers()[0].mState.mCachedGains.mHierarchy, .7f);
    }

    TEST_F(ActorRagdollPhysicsTest, OwnedBlendControllersRejectLatePhysicsFailureWithoutPublishingClocks)
    {
        addHinge();
        mGraph.mJoints.clear();
        for (std::size_t i = 0; i < 2; ++i)
        {
            auto& body = mGraph.mBodies[i];
            body.mNodeRecord = 8 + 12 * i;
            body.mBlend = NifBullet::RagdollBlendDefinition{static_cast<std::uint32_t>(30 + i), 8, .9f, .8f};
            body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
                static_cast<std::uint32_t>(78 + i), body.mNodeRecord, 0xcd, 1.f, 0.f, 0.f, .25f,
                {{.25f, 1.f, 1.f}}};
        }
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalBlendTimeCache cache;
        const std::array<NifBullet::RagdollNativeBlendControllerTarget, 2> targets{{
            {78, osg::Matrixf::identity()}, {79, osg::Matrixf::scale(2, 2, 2)}}};
        EXPECT_THROW(actor.updateNativeBlendControllers(targets, 1.f, cache, 1.f / 120, 0, 0.f), std::invalid_argument);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        EXPECT_EQ(actor.captureNativeMotionModes()[1].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        EXPECT_EQ(actor.capture()[0].mPose.getOrigin(), mPoses[0].getOrigin());
        EXPECT_EQ(cache.mCycle, 0xffffffffu);
        const auto controllers = actor.captureNativeBlendControllers();
        ASSERT_EQ(controllers.size(), 2u);
        EXPECT_FLOAT_EQ(controllers[0].mState.mClock.mPreviousTime, -std::numeric_limits<float>::max());
        EXPECT_FLOAT_EQ(controllers[1].mState.mClock.mPreviousTime, -std::numeric_limits<float>::max());
        EXPECT_FLOAT_EQ(actor.captureNativeBlendStates()[0].mGains.mHierarchy, .9f);
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, OwnedBlendControllersRejectBadIdentitiesBeforeRegistrationOrPublication)
    {
        auto& body = mGraph.mBodies.front();
        body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 999, 0xcd, 1.f, 0.f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        EXPECT_THROW((NifBullet::ActorRagdollPhysics{mGraph, mWorld, 1.f, mPoses, 1, -1}), std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        body.mBlendController->mTargetRecord = 8;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalBlendTimeCache cache;
        const auto matrix = osg::Matrixf::identity();
        for (const auto& requests : std::array<std::array<NifBullet::RagdollNativeBlendControllerTarget, 2>, 2>{{
                 {{{78, matrix}, {999, matrix}}}, {{{78, matrix}, {78, matrix}}}}})
            EXPECT_THROW(actor.updateNativeBlendControllers(requests, 1.f, cache, 1.f / 120, 0, 0.f),
                std::invalid_argument);
        EXPECT_EQ(cache.mCycle, 0xffffffffu);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        EXPECT_FLOAT_EQ(actor.captureNativeBlendControllers()[0].mState.mClock.mPreviousTime,
            -std::numeric_limits<float>::max());
    }

    TEST_F(ActorRagdollPhysicsTest, OwnedBlendControllersUseCallerSharedCacheAcrossActors)
    {
        auto& body = mGraph.mBodies.front();
        body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        NifBullet::ActorRagdollPhysics first(mGraph, mWorld, 1.f, mPoses, 1, -1);
        body.mBlendController->mRecord = 79;
        body.mBlendController->mFlags = 0x1d;
        NifBullet::ActorRagdollPhysics second(mGraph, mWorld, 1.f, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeBlendControllerTarget, 1> firstTarget{{{78, osg::Matrixf::identity()}}};
        const std::array<NifBullet::RagdollNativeBlendControllerTarget, 1> secondTarget{{{79, osg::Matrixf::identity()}}};
        ESM4::PhysicalBlendTimeCache shared;
        first.updateNativeBlendControllers(firstTarget, 0.f, shared, 1.f / 120, 0, 0.f);
        second.updateNativeBlendControllers(secondTarget, 0.f, shared, 1.f / 120, 0, 0.f);
        EXPECT_EQ(second.captureNativeBlendControllers()[0].mState.mTiming.mFlags, 0x1d);
        ESM4::PhysicalBlendTimeCache fresh;
        second.updateNativeBlendControllers(secondTarget, 0.f, fresh, 1.f / 120, 0, 0.f);
        EXPECT_EQ(second.captureNativeBlendControllers()[0].mState.mTiming.mFlags, 0x15);
        EXPECT_EQ(first.captureNativeBlendControllers()[0].mState.mTiming.mFlags, 0xd);
    }

    TEST_F(ActorRagdollPhysicsTest, OwnedBlendControllersMissingBlendOrTargetIgnoreUnusedAnimationPose)
    {
        auto& body = mGraph.mBodies.front();
        body.mNodeRecord = 8;
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xcd, 1.f, 0.f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        NifBullet::ActorRagdollPhysics noBlend(mGraph, mWorld, 1.f, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeBlendControllerTarget, 1> targets{{{78, osg::Matrixf::scale(2, 2, 2)}}};
        ESM4::PhysicalBlendTimeCache cache;
        EXPECT_TRUE(noBlend.updateNativeBlendControllers(targets, .125f, cache, 1.f / 120, 0, 0.f).empty());
        EXPECT_FLOAT_EQ(noBlend.captureNativeBlendControllers()[0].mState.mClock.mPreviousTime, .125f);
        EXPECT_FLOAT_EQ(noBlend.captureNativeBlendControllers()[0].mState.mClock.mStartTime,
            -std::numeric_limits<float>::max());
        EXPECT_EQ(noBlend.capture()[0].mPose.getOrigin(), mPoses[0].getOrigin());
        EXPECT_EQ(cache.mCycle, 0xffffffffu);
        body.mBlendController->mTargetRecord.reset();
        NifBullet::ActorRagdollPhysics noTarget(mGraph, mWorld, 1.f, mPoses, 1, -1);
        EXPECT_TRUE(noTarget.updateNativeBlendControllers(targets, .125f, cache, 1.f / 120, 0, 0.f).empty());
        EXPECT_FLOAT_EQ(noTarget.captureNativeBlendControllers()[0].mState.mClock.mPreviousTime,
            -std::numeric_limits<float>::max());
        EXPECT_TRUE(noTarget.captureNativeBlendStates().empty());
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, NativeBlendFrameIncludesUncontrolledBonesAndResolvesControllerTargets)
    {
        addHinge();
        mGraph.mJoints.clear();
        mGraph.mBodies[0].mNodeRecord = 8;
        mGraph.mBodies[1].mNodeRecord = 20;
        mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, 1.f, 1.f};
        mGraph.mBodies[1].mBlend = NifBullet::RagdollBlendDefinition{31, 8, .7f, .6f};
        mGraph.mBodies[0].mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 20, 0xd, 1.f, 0.f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, NifBullet::RagdollNativeLengthScale, mPoses, 1, -1);
        ESM4::PhysicalBlendTimeCache cache;
        const std::array<NifBullet::RagdollBoneWorldPose, 2> bones{{
            {20, osg::Matrixf::translate(70, 0, 140)}, {8, osg::Matrixf::translate(140, 0, 210)}}};
        const std::array<std::uint32_t, 1> order{{78}};
        const auto publications = actor.updateNativeBlendFrame(bones, order, 1.f, cache, 1.f / 120, 0, 0.f);
        ASSERT_EQ(publications.size(), 2u);
        EXPECT_EQ(publications[0].mRecord, 24u);
        EXPECT_EQ(publications[1].mRecord, 12u);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
        EXPECT_EQ(actor.captureNativeMotionModes()[1].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
        const auto expected = NifBullet::ragdollBodyWorldPoses(mGraph, bones);
        const auto states = actor.capture();
        EXPECT_EQ(states[0].mPose, expected[0]);
        EXPECT_EQ(states[1].mPose, expected[1]);
        EXPECT_FLOAT_EQ(actor.captureNativeBlendControllers()[0].mState.mCachedGains.mHierarchy, .7f);
        EXPECT_FLOAT_EQ(actor.captureNativeBlendStates()[0].mGains.mHierarchy, 1.f);
        EXPECT_FLOAT_EQ(actor.captureNativeBlendStates()[1].mGains.mHierarchy, 1.f);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeBlendFrameLateUncontrolledFailureRollsBackControllersAndBodies)
    {
        addHinge();
        mGraph.mJoints.clear();
        mGraph.mBodies[0].mNodeRecord = 8;
        mGraph.mBodies[1].mNodeRecord = 20;
        for (auto& body : mGraph.mBodies)
            body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, 1.f, 1.f};
        mGraph.mBodies[0].mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalBlendTimeCache cache;
        const std::array<NifBullet::RagdollBoneWorldPose, 2> bones{{
            {8, osg::Matrixf::identity()}, {20, osg::Matrixf::scale(2, 2, 2)}}};
        const std::array<std::uint32_t, 1> order{{78}};
        EXPECT_THROW(actor.updateNativeBlendFrame(bones, order, 1.f, cache, 1.f / 120, 0, 0.f),
            std::invalid_argument);
        EXPECT_EQ(actor.capture()[0].mPose, mPoses[0]);
        EXPECT_EQ(actor.capture()[1].mPose, mPoses[1]);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        EXPECT_EQ(actor.captureNativeMotionModes()[1].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        EXPECT_EQ(cache.mCycle, 0xffffffffu);
        EXPECT_FLOAT_EQ(actor.captureNativeBlendControllers()[0].mState.mClock.mPreviousTime,
            -std::numeric_limits<float>::max());
        EXPECT_FLOAT_EQ(actor.captureNativeBlendControllers()[0].mState.mCachedGains.mHierarchy, -1.f);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeBlendFrameRequiresCompleteUniqueNodeAndControllerIdentities)
    {
        auto& body = mGraph.mBodies[0];
        body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, 1.f, 1.f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalBlendTimeCache cache;
        const std::array<NifBullet::RagdollBoneWorldPose, 1> bones{{{8, osg::Matrixf::identity()}}};
        const std::array<NifBullet::RagdollBoneWorldPose, 1> wrong{{{999, osg::Matrixf::identity()}}};
        const std::array<NifBullet::RagdollBoneWorldPose, 2> duplicate{{bones[0], bones[0]}};
        const std::array<std::uint32_t, 1> order{{78}}, unknown{{999}};
        const std::array<std::uint32_t, 2> repeated{{78, 78}};
        EXPECT_THROW(actor.updateNativeBlendFrame({}, order, 1.f, cache, 1.f / 120, 0, 0.f), std::invalid_argument);
        EXPECT_THROW(actor.updateNativeBlendFrame(wrong, order, 1.f, cache, 1.f / 120, 0, 0.f), std::invalid_argument);
        EXPECT_THROW(actor.updateNativeBlendFrame(duplicate, order, 1.f, cache, 1.f / 120, 0, 0.f), std::invalid_argument);
        EXPECT_THROW(actor.updateNativeBlendFrame(bones, {}, 1.f, cache, 1.f / 120, 0, 0.f), std::invalid_argument);
        EXPECT_THROW(actor.updateNativeBlendFrame(bones, unknown, 1.f, cache, 1.f / 120, 0, 0.f), std::invalid_argument);
        EXPECT_THROW(actor.updateNativeBlendFrame(bones, repeated, 1.f, cache, 1.f / 120, 0, 0.f), std::invalid_argument);
        EXPECT_EQ(actor.capture()[0].mPose, mPoses[0]);
        EXPECT_EQ(cache.mCycle, 0xffffffffu);
        EXPECT_FLOAT_EQ(actor.captureNativeBlendControllers()[0].mState.mClock.mPreviousTime,
            -std::numeric_limits<float>::max());
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, NativeBlendFrameUsesExplicitControllerOrderForSharedReverseCache)
    {
        addHinge();
        mGraph.mJoints.clear();
        for (std::size_t i = 0; i < 2; ++i)
        {
            auto& body = mGraph.mBodies[i];
            body.mNodeRecord = 8 + 12 * i;
            body.mBlend = NifBullet::RagdollBlendDefinition{static_cast<std::uint32_t>(30 + i), 8, .9f, .8f};
            body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
                static_cast<std::uint32_t>(78 + i), body.mNodeRecord,
                static_cast<std::uint16_t>(i == 0 ? 0xd : 0x1d), 1.f, 0.f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        }
        NifBullet::ActorRagdollPhysics forwardFirst(mGraph, mWorld, 1.f, mPoses, 1, -1);
        NifBullet::ActorRagdollPhysics reverseFirst(mGraph, mWorld, 1.f, mPoses, 1, -1);
        const std::array<NifBullet::RagdollBoneWorldPose, 2> bones{{
            {8, osg::Matrixf::identity()}, {20, osg::Matrixf::identity()}}};
        const std::array<std::uint32_t, 2> forward{{78, 79}}, reverse{{79, 78}};
        ESM4::PhysicalBlendTimeCache forwardCache, reverseCache;
        ASSERT_EQ(forwardFirst.updateNativeBlendFrame(bones, forward, 1.f, forwardCache, 1.f / 120, 0, 0.f).size(), 2u);
        ASSERT_EQ(reverseFirst.updateNativeBlendFrame(bones, reverse, 1.f, reverseCache, 1.f / 120, 0, 0.f).size(), 2u);
        EXPECT_FLOAT_EQ(forwardCache.mResult, 0.f);
        EXPECT_FLOAT_EQ(reverseCache.mResult, .25f);
        for (const auto& controller : forwardFirst.captureNativeBlendControllers())
            EXPECT_FLOAT_EQ(controller.mState.mClock.mPreviousTime, 1.f);
        for (const auto& controller : reverseFirst.captureNativeBlendControllers())
            EXPECT_FLOAT_EQ(controller.mState.mClock.mPreviousTime, -std::numeric_limits<float>::max());
    }

    TEST_F(ActorRagdollPhysicsTest, NativeBlendFrameWithNoControllersPublishesOnlyBlendBodies)
    {
        addHinge();
        mGraph.mJoints.clear();
        mGraph.mBodies[0].mNodeRecord = 8;
        mGraph.mBodies[1].mNodeRecord = 20;
        mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, 1.f, 1.f};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalBlendTimeCache cache;
        const std::array<NifBullet::RagdollBoneWorldPose, 2> bones{{
            {8, osg::Matrixf::identity()}, {20, osg::Matrixf::scale(2, 2, 2)}}};
        const auto publications = actor.updateNativeBlendFrame(bones, {}, 1.f, cache, 1.f / 120, 0, 0.f);
        ASSERT_EQ(publications.size(), 1u);
        EXPECT_EQ(publications[0].mRecord, 12u);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
        EXPECT_EQ(actor.captureNativeMotionModes()[1].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        EXPECT_EQ(actor.capture()[1].mPose, mPoses[1]);
        EXPECT_EQ(cache.mCycle, 0xffffffffu);
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, NativeBlendFrameSceneFailureRollsBackControllerCacheAndPhysicalState)
    {
        auto& body = mGraph.mBodies[0];
        body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalBlendTimeCache cache;
        const std::array<NifBullet::RagdollBoneWorldPose, 1> bones{{{8, osg::Matrixf::identity()}}};
        const std::array<std::uint32_t, 1> order{{78}};
        unsigned calls = 0;
        const auto reject = [&](std::span<const NifBullet::RagdollNativeBlendPublication> publications) {
            ++calls;
            EXPECT_EQ(publications.size(), 1u);
            EXPECT_EQ(actor.capture()[0].mPose, mPoses[0]);
            EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
            EXPECT_FLOAT_EQ(actor.captureNativeBlendControllers()[0].mState.mClock.mPreviousTime,
                -std::numeric_limits<float>::max());
            throw std::runtime_error("invalid renderer publication");
        };
        EXPECT_THROW(actor.updateNativeBlendFrame(bones, order, 1.f, cache, 1.f / 120, 0, 0.f, reject),
            std::runtime_error);
        EXPECT_EQ(calls, 1u);
        EXPECT_EQ(actor.capture()[0].mPose, mPoses[0]);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        EXPECT_FLOAT_EQ(actor.captureNativeBlendStates()[0].mGains.mHierarchy, .9f);
        EXPECT_FLOAT_EQ(actor.captureNativeBlendControllers()[0].mState.mCachedGains.mHierarchy, -1.f);
        EXPECT_EQ(cache.mCycle, 0xffffffffu);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeBlendFrameSceneRunsOnceAfterPreparationAndBeforePhysicalCommit)
    {
        auto& body = mGraph.mBodies[0];
        body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, 1.f, 1.f};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalBlendTimeCache cache;
        const std::array<NifBullet::RagdollBoneWorldPose, 1> bones{{{8, osg::Matrixf::identity()}}};
        unsigned calls = 0;
        const auto publish = [&](std::span<const NifBullet::RagdollNativeBlendPublication> publications) {
            ++calls;
            EXPECT_EQ(publications.size(), 1u);
            EXPECT_EQ(actor.capture()[0].mPose, mPoses[0]);
            EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        };
        ASSERT_EQ(actor.updateNativeBlendFrame(bones, {}, 1.f, cache, 1.f / 120, 0, 0.f, publish).size(), 1u);
        EXPECT_EQ(calls, 1u);
        EXPECT_EQ(actor.capture()[0].mPose.getOrigin(), btVector3(0, 0, 0));
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
        const std::array<NifBullet::RagdollBoneWorldPose, 1> bad{{{8, osg::Matrixf::scale(2, 2, 2)}}};
        EXPECT_THROW(actor.updateNativeBlendFrame(bad, {}, 1.f, cache, 1.f / 120, 0, 0.f, publish),
            std::invalid_argument);
        EXPECT_EQ(calls, 1u);
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, KnockdownSetupOwnsCurveAndAttachmentIdentity)
    {
        auto& body = mGraph.mBodies.front();
        body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0x1d, .25f, -.125f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalBlendTimeCache cache;
        const std::array<NifBullet::RagdollNativeBlendControllerTarget, 1> targets{{{78, osg::Matrixf::identity()}}};
        actor.updateNativeBlendControllers(targets, 0.f, cache, 1.f / 120, 0, 0.f);
        actor.updateNativeBlendControllers(targets, .125f, cache, 1.f / 120, 0, 0.f);
        const auto before = actor.capture();
        const auto modes = actor.captureNativeMotionModes();
        const float elapsed = actor.captureNativeBlendControllers()[0].mState.mClock.mElapsed;
        const std::array<NifBullet::RagdollNativeKnockdownBlendRequest, 1> requests{{{8, .25f}}};
        const auto result = actor.prepareNativeKnockdownBlends(requests);
        ASSERT_EQ(result.size(), 1u);
        ASSERT_EQ(result[0], NifBullet::RagdollNativeKnockdownBlendDisposition::Started);
        const auto controllers = actor.captureNativeBlendControllers();
        ASSERT_EQ(controllers.size(), 1u);
        const auto& controller = controllers[0];
        EXPECT_EQ(controller.mAttachedNode, 8u);
        EXPECT_EQ(controller.mTargetNode, 8u);
        const auto& state = controller.mState;
        ASSERT_EQ(state.mKeys.size(), 2u);
        EXPECT_FLOAT_EQ(state.mKeys[0].mGains.mHierarchy, 1.f);
        EXPECT_FLOAT_EQ(state.mKeys[1].mTime, .25f);
        EXPECT_FLOAT_EQ(state.mKeys[1].mGains.mHierarchy, 0.f);
        EXPECT_EQ(state.mTiming.mFlags, 0xdd); // Preserve original reverse bit0x10.
        EXPECT_FLOAT_EQ(state.mTiming.mFrequency, 1.f);
        EXPECT_FLOAT_EQ(state.mTiming.mPhase, 0.f);
        EXPECT_FLOAT_EQ(state.mTiming.mStartKey, 0.f);
        EXPECT_FLOAT_EQ(state.mTiming.mStopKey, .25f);
        EXPECT_FLOAT_EQ(state.mClock.mPreviousTime, -std::numeric_limits<float>::max());
        EXPECT_FLOAT_EQ(state.mClock.mElapsed, elapsed);
        EXPECT_FLOAT_EQ(state.mCachedGains.mHierarchy, -1.f);
        EXPECT_EQ(state.mCursor, 0u);
        EXPECT_EQ(state.mSetupState, 2u);
        EXPECT_EQ(actor.captureNativeMotionModes(), modes);
        EXPECT_EQ(actor.capture()[0].mPose, before[0].mPose);
        actor.updateNativeBlendControllers(targets, 1.f, cache, 1.f / 120, 0, 0.f);
        actor.updateNativeBlendControllers(targets, 1.25f, cache, 1.f / 120, 0, 0.f);
        EXPECT_TRUE(actor.captureNativeBlendControllers()[0].mState.mKeys.empty());
        EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mSetupState, 0u);
    }

    TEST_F(ActorRagdollPhysicsTest, KnockdownSetupUsesAttachmentGainsForRedirectedController)
    {
        auto& body = mGraph.mBodies.front();
        body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 20, 0xd, 1.f, 0.f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        auto other = body;
        other.mRecord = 14; other.mNodeRecord = 20;
        other.mBlend = NifBullet::RagdollBlendDefinition{31, 8, .7f, .6f};
        other.mBlendController.reset();
        mGraph.mBodies.push_back(other); mPoses.push_back(mPoses.front());
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeKnockdownBlendRequest, 1> requests{{{8, 0.f}}};
        ASSERT_EQ(actor.prepareNativeKnockdownBlends(requests)[0],
            NifBullet::RagdollNativeKnockdownBlendDisposition::Started);
        const auto state = actor.captureNativeBlendControllers()[0];
        EXPECT_EQ(state.mTargetNode, 20u);
        EXPECT_EQ(state.mAttachedNode, 8u);
        ASSERT_EQ(state.mState.mKeys.size(), 2u);
        EXPECT_FLOAT_EQ(state.mState.mKeys[0].mGains.mHierarchy, .9f);
        EXPECT_FLOAT_EQ(state.mState.mKeys[0].mGains.mVelocity, .8f);
        EXPECT_EQ(state.mState.mKeys[1].mTime, 0.f);
        EXPECT_EQ(actor.captureNativeBlendStates()[1].mGains.mHierarchy, .7f);
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, KnockdownSetupSkipsMissingAndDisabledWithoutCreatingControllers)
    {
        auto& body = mGraph.mBodies.front();
        body.mNodeRecord = 8;
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, .25f, -.125f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        const float bad = std::numeric_limits<float>::quiet_NaN();
        const std::array<NifBullet::RagdollNativeKnockdownBlendRequest, 1> unused{{{8, bad}}};
        {
            NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
            EXPECT_EQ(actor.prepareNativeKnockdownBlends(unused)[0],
                NifBullet::RagdollNativeKnockdownBlendDisposition::MissingBlend);
            EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mKeys.size(), 1u);
            EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mSetupState, 0u);
        }
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        body.mBlendController.reset();
        {
            NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
            EXPECT_EQ(actor.prepareNativeKnockdownBlends(unused)[0],
                NifBullet::RagdollNativeKnockdownBlendDisposition::MissingController);
            EXPECT_TRUE(actor.captureNativeBlendControllers().empty());
        }
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, std::nullopt, 0xd, .25f, -.125f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeKnockdownBlendRequest, 1> disabled{{{8, -1.f}}};
        EXPECT_EQ(actor.prepareNativeKnockdownBlends(disabled)[0],
            NifBullet::RagdollNativeKnockdownBlendDisposition::Disabled);
        EXPECT_FLOAT_EQ(actor.captureNativeBlendControllers()[0].mState.mTiming.mFrequency, .25f);
        const std::array<NifBullet::RagdollNativeKnockdownBlendRequest, 1> start{{{8, .25f}}};
        EXPECT_EQ(actor.prepareNativeKnockdownBlends(start)[0],
            NifBullet::RagdollNativeKnockdownBlendDisposition::Started);
        EXPECT_FALSE(actor.captureNativeBlendControllers()[0].mTargetNode);
        EXPECT_EQ(actor.prepareNativeKnockdownBlends(disabled)[0],
            NifBullet::RagdollNativeKnockdownBlendDisposition::Disabled);
        EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mSetupState, 2u);
        EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mKeys.size(), 2u);
    }

    TEST_F(ActorRagdollPhysicsTest, KnockdownSetupRejectsLateInvalidRequestsAtomically)
    {
        auto& body = mGraph.mBodies.front();
        body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, .25f, -.125f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        auto other = body;
        other.mRecord = 14; other.mNodeRecord = 20;
        other.mBlend = NifBullet::RagdollBlendDefinition{31, 8, .7f, .6f};
        other.mBlendController->mRecord = 79; other.mBlendController->mTargetRecord = 20;
        mGraph.mBodies.push_back(other); mPoses.push_back(mPoses.front());
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        const float bad = std::numeric_limits<float>::quiet_NaN();
        for (const auto& requests : std::array<std::array<NifBullet::RagdollNativeKnockdownBlendRequest, 2>, 3>{{
                 {{{8, .25f}, {20, bad}}}, {{{8, .25f}, {8, .25f}}}, {{{8, .25f}, {999, .25f}}}}})
        {
            EXPECT_THROW(actor.prepareNativeKnockdownBlends(requests), std::invalid_argument);
            for (const auto& value : actor.captureNativeBlendControllers())
            {
                EXPECT_EQ(value.mState.mKeys.size(), 1u);
                EXPECT_EQ(value.mState.mSetupState, 0u);
                EXPECT_FLOAT_EQ(value.mState.mTiming.mFrequency, .25f);
                EXPECT_FLOAT_EQ(value.mState.mTiming.mPhase, -.125f);
                EXPECT_FLOAT_EQ(value.mState.mClock.mPreviousTime, -std::numeric_limits<float>::max());
            }
        }
        EXPECT_TRUE(actor.prepareNativeKnockdownBlends({}).empty());
        const std::array<NifBullet::RagdollNativeKnockdownBlendRequest, 2> valid{{{8, .25f}, {20, 1.f}}};
        ASSERT_EQ(actor.prepareNativeKnockdownBlends(valid).size(), 2u);
        EXPECT_EQ(actor.captureNativeBlendControllers()[1].mState.mSetupState, 2u);
    }
}

namespace
{
    TEST(RagdollNativeForce, StoresInverseMassAndSeparateForceProducts)
    {
        EXPECT_FLOAT_EQ(NifBullet::ragdollNativeInverseMass(2.f), .5f);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(NifBullet::ragdollNativeInverseMass(-0.f)), 0u);
        const auto result = NifBullet::ragdollNativeLinearVelocityAfterForce({1.f, -2.f, 3.f, 0.f},
            .5f, .25f, {2.f, -3.f, 4.f, 0.f});
        EXPECT_EQ(result, (std::array<float, 4>{1.25f, -2.375f, 3.5f, 0.f}));
    }

    TEST(RagdollNativeForce, RejectsInvalidUsedInputsAndArithmeticOverflow)
    {
        const float bad = std::numeric_limits<float>::quiet_NaN();
        const float max = std::numeric_limits<float>::max();
        EXPECT_THROW(NifBullet::ragdollNativeInverseMass(-1.f), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeInverseMass(bad), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeInverseMass(std::numeric_limits<float>::denorm_min()), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeLinearVelocityAfterForce({bad, 0, 0, 0}, 1, 1, {}), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeLinearVelocityAfterForce({}, bad, 1, {}), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeLinearVelocityAfterForce({}, -1, 1, {}), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeLinearVelocityAfterForce({}, 1, -1, {}), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeLinearVelocityAfterForce({}, 1, 1, {bad, 0, 0, 0}), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeLinearVelocityAfterForce({}, 1, 2, {max, 0, 0, 0}), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeLinearVelocityAfterForce({max, 0, 0, 0}, 1, 1, {max, 0, 0, 0}), std::invalid_argument);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeForcesPublishImmediateScaledVelocityWithoutChangingOtherState)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 2.f, mPoses, 1, -1);
        auto state = actor.capture();
        state[0].mLinearVelocity = {2, -4, 6};
        state[0].mAngularVelocity = {1, 2, 3};
        actor.restore(state);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        body->applyCentralForce({7, 8, 9});
        const auto shape = body->getCollisionShape();
        const std::array<NifBullet::RagdollNativeForceRequest, 1> requests{{{12, {2, -3, 4}, .25f}}};
        actor.applyNativeForces(requests);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(2.5, -4.75, 7));
        EXPECT_EQ(actor.capture()[0].mAngularVelocity, state[0].mAngularVelocity);
        EXPECT_EQ(actor.capture()[0].mPose, state[0].mPose);
        EXPECT_EQ(body->getTotalForce(), btVector3(7, 8, 9));
        EXPECT_EQ(body->getCollisionShape(), shape);
        const std::array<NifBullet::RagdollNativeForceRequest, 1> uncapped{{{12, {1000, 0, 0}, 1.f}}};
        actor.applyNativeForces(uncapped);
        EXPECT_GT(actor.capture()[0].mLinearVelocity.x(), 500);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeForcesIgnoreKeyframedInputsThenRestoreDynamicForceResponse)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.setNativeMotionModes(key);
        const auto initial = actor.capture()[0];
        const float bad = std::numeric_limits<float>::quiet_NaN();
        const std::array<NifBullet::RagdollNativeForceRequest, 1> unused{{{12, {bad, bad, bad}, bad}}};
        EXPECT_NO_THROW(actor.applyNativeForces(unused));
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, initial.mLinearVelocity);
        EXPECT_EQ(actor.capture()[0].mPose, initial.mPose);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> dynamic{{{12, NifBullet::RagdollNativeMotion::Dynamic}}};
        actor.setNativeMotionModes(dynamic);
        const std::array<NifBullet::RagdollNativeForceRequest, 1> requests{{{12, {2, 0, 0}, 1.f}}};
        actor.applyNativeForces(requests);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(1, 0, 0));
    }

    TEST_F(ActorRagdollPhysicsTest, NativeForceBatchRollsBackLateFailureAndWakesOnlyConnectedGroups)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        auto* second = btRigidBody::upcast(actor.collisionObjects()[1]);
        first->setActivationState(ISLAND_SLEEPING); second->setActivationState(ISLAND_SLEEPING);
        const auto before = actor.capture();
        const float bad = std::numeric_limits<float>::quiet_NaN();
        for (const auto& requests : std::array<std::array<NifBullet::RagdollNativeForceRequest, 2>, 3>{{
                 {{{12, {2, 0, 0}, 1.f}, {24, {bad, 0, 0}, 1.f}}},
                 {{{12, {2, 0, 0}, 1.f}, {12, {}, 1.f}}},
                 {{{12, {2, 0, 0}, 1.f}, {999, {}, 1.f}}}}})
        {
            EXPECT_THROW(actor.applyNativeForces(requests), std::invalid_argument);
            EXPECT_EQ(actor.capture()[0].mLinearVelocity, before[0].mLinearVelocity);
            EXPECT_FALSE(first->isActive()); EXPECT_FALSE(second->isActive());
        }
        actor.applyNativeForces({});
        EXPECT_FALSE(first->isActive()); EXPECT_FALSE(second->isActive());
        const std::array<NifBullet::RagdollNativeForceRequest, 1> valid{{{12, {2, 0, 0}, 1.f}}};
        actor.applyNativeForces(valid);
        EXPECT_TRUE(first->isActive()); EXPECT_TRUE(second->isActive());
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(1, 0, 0));
        EXPECT_EQ(actor.capture()[1].mLinearVelocity, btVector3(0, 0, 0));
        EXPECT_EQ(mWorld.getNumConstraints(), 1);
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, NativeForceDoesNotWakeUnconnectedBody)
    {
        addHinge(); mGraph.mJoints.clear();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        auto* second = btRigidBody::upcast(actor.collisionObjects()[1]);
        first->setActivationState(ISLAND_SLEEPING); second->setActivationState(ISLAND_SLEEPING);
        const std::array<NifBullet::RagdollNativeForceRequest, 1> requests{{{12, {2, 0, 0}, 1.f}}};
        actor.applyNativeForces(requests);
        EXPECT_TRUE(first->isActive()); EXPECT_FALSE(second->isActive());
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(1, 0, 0));
        EXPECT_EQ(actor.capture()[1].mLinearVelocity, btVector3(0, 0, 0));
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, OwnedVelocitySetupCreatesOnNonblendBodyAndReadsArchivedMass)
    {
        mGraph.mBodies.front().mNodeRecord = 8;
        mGraph.mBodies.front().mLinearDamping = 2.f;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 2.f, mPoses, 1, -1);
        const auto initial = actor.capture();
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.setNativeMotionModes(key);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        const auto activation = body->getActivationState();
        const std::array<NifBullet::RagdollNativeVelocitySetupRequest, 1> setup{{{8, {1, -2, .5f, 4}, .25f}}};
        actor.prepareNativeVelocityControllers(setup);
        const auto controllers = actor.captureNativeVelocityControllers();
        ASSERT_EQ(controllers.size(), 1u);
        EXPECT_EQ(controllers[0].mAttachedNode, 8u);
        EXPECT_EQ(controllers[0].mTargetNode, 8u);
        EXPECT_TRUE(controllers[0].mPrecedesBlend);
        EXPECT_EQ(controllers[0].mState.mForceVector, (std::array<float, 4>{2.f, -4.f, 1.f, 8.f}));
        EXPECT_EQ(controllers[0].mState.mFrameDelta, 0.f);
        EXPECT_EQ(actor.capture()[0].mPose, initial[0].mPose);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, initial[0].mLinearVelocity);
        EXPECT_EQ(body->getActivationState(), activation);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
    }

    TEST_F(ActorRagdollPhysicsTest, OwnedVelocitySetupReusesTargetClockAndExistingListPosition)
    {
        addHinge(); mGraph.mBodies[0].mNodeRecord = 8; mGraph.mBodies[1].mNodeRecord = 20;
        mGraph.mBodies[0].mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalVelocityControllerState old;
        old.mTiming.mFlags = 0xffff; old.mClock.mElapsed = 7.f; old.mFrameDelta = 99.f;
        const std::array<NifBullet::RagdollNativeVelocityControllerState, 1> saved{{{8, 20, old, false}}};
        actor.restoreNativeVelocityControllers(saved);
        const std::array<NifBullet::RagdollNativeVelocitySetupRequest, 1> setup{{{8, {1, 0, 0, 0}, .25f}}};
        actor.prepareNativeVelocityControllers(setup);
        const auto controllers = actor.captureNativeVelocityControllers();
        ASSERT_EQ(controllers.size(), 1u);
        EXPECT_EQ(controllers[0].mTargetNode, 20u);
        EXPECT_FALSE(controllers[0].mPrecedesBlend);
        EXPECT_EQ(controllers[0].mState.mClock.mElapsed, 7.f);
        EXPECT_EQ(controllers[0].mState.mFrameDelta, 99.f);
        EXPECT_EQ(controllers[0].mState.mTiming.mFlags, 0xfffdu);
        const std::array<std::uint32_t, 2> nodes{8, 20};
        EXPECT_EQ(actor.captureNativeControllerOrder(nodes), (std::vector<NifBullet::RagdollNativeControllerReference>{
            {NifBullet::RagdollNativeControllerKind::Blend, 78}, {NifBullet::RagdollNativeControllerKind::Velocity, 8}}));
    }

    TEST_F(ActorRagdollPhysicsTest, OwnedVelocitySetupRollsBackLateInvalidIdentityAndVectorWithoutWake)
    {
        addHinge(); mGraph.mBodies[0].mNodeRecord = 8; mGraph.mBodies[1].mNodeRecord = 20;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        const float bad = std::numeric_limits<float>::quiet_NaN();
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        auto* second = btRigidBody::upcast(actor.collisionObjects()[1]);
        first->setActivationState(ISLAND_SLEEPING); second->setActivationState(ISLAND_SLEEPING);
        for (const auto& requests : std::array<std::array<NifBullet::RagdollNativeVelocitySetupRequest, 2>, 4>{{
            {{{8, {1, 0, 0, 0}, .25f}, {20, {bad, 0, 0, 0}, .25f}}},
            {{{8, {1, 0, 0, 0}, .25f}, {20, {}, -1.f}}},
            {{{8, {1, 0, 0, 0}, .25f}, {8, {}, .25f}}},
            {{{8, {1, 0, 0, 0}, .25f}, {999, {}, .25f}}}}})
        {
            EXPECT_THROW(actor.prepareNativeVelocityControllers(requests), std::invalid_argument);
            EXPECT_TRUE(actor.captureNativeVelocityControllers().empty());
            EXPECT_FALSE(first->isActive()); EXPECT_FALSE(second->isActive());
        }
        actor.prepareNativeVelocityControllers({});
        EXPECT_TRUE(actor.captureNativeVelocityControllers().empty());
        const std::array<NifBullet::RagdollNativeVelocitySetupRequest, 2> setup{{{8, {1, 0, 0, 0}, .25f}, {20, {}, .5f}}};
        actor.prepareNativeVelocityControllers(setup);
        ASSERT_EQ(actor.captureNativeVelocityControllers().size(), 2u);
        const std::array<NifBullet::RagdollNativeVelocitySetupRequest, 2> late{{{8, {2, 0, 0, 0}, 1.f}, {20, {}, bad}}};
        EXPECT_THROW(actor.prepareNativeVelocityControllers(late), std::invalid_argument);
        EXPECT_EQ(actor.captureNativeVelocityControllers()[0].mState.mTiming.mStopKey, .25f);
    }

    TEST_F(ActorRagdollPhysicsTest, OwnedVelocityRestoreAndOrderRejectMalformedStateAtomically)
    {
        mGraph.mBodies.front().mNodeRecord = 8;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalVelocityControllerState old;
        old.mTiming = {0xd, 1, 0, 0, 1};
        const std::array<NifBullet::RagdollNativeVelocityControllerState, 1> saved{{{8, std::nullopt, old, true}}};
        actor.restoreNativeVelocityControllers(saved);
        ASSERT_EQ(actor.captureNativeVelocityControllers().size(), 1u);
        EXPECT_FALSE(actor.captureNativeVelocityControllers()[0].mTargetNode);
        auto invalid = saved;
        invalid[0].mTargetNode = 999;
        EXPECT_THROW(actor.restoreNativeVelocityControllers(invalid), std::invalid_argument);
        invalid = saved; invalid[0].mState.mTiming.mStopKey = -1.f;
        EXPECT_THROW(actor.restoreNativeVelocityControllers(invalid), std::invalid_argument);
        invalid = saved; invalid[0].mState.mForceVector[3] = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(actor.restoreNativeVelocityControllers(invalid), std::invalid_argument);
        invalid = saved; invalid[0].mState.mFrameDelta = -1.f;
        EXPECT_THROW(actor.restoreNativeVelocityControllers(invalid), std::invalid_argument);
        const std::array<NifBullet::RagdollNativeVelocityControllerState, 2> duplicate{{saved[0], saved[0]}};
        EXPECT_THROW(actor.restoreNativeVelocityControllers(duplicate), std::invalid_argument);
        const std::array<std::uint32_t, 2> repeated{8, 8};
        EXPECT_THROW(actor.captureNativeControllerOrder(repeated), std::invalid_argument);
        const std::array<std::uint32_t, 1> missing{999};
        EXPECT_THROW(actor.captureNativeControllerOrder(missing), std::invalid_argument);
        EXPECT_FALSE(actor.captureNativeVelocityControllers()[0].mTargetNode);
        actor.restoreNativeVelocityControllers({});
        EXPECT_TRUE(actor.captureNativeVelocityControllers().empty());
    }

    TEST_F(ActorRagdollPhysicsTest, OwnedVelocityCreationPrependsAndRejectsAmbiguousBodyNodes)
    {
        addHinge(); mGraph.mBodies[0].mNodeRecord = 8; mGraph.mBodies[1].mNodeRecord = 20;
        mGraph.mBodies[0].mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {}};
        {
            NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
            const std::array<NifBullet::RagdollNativeVelocitySetupRequest, 1> setup{{{8, {}, .25f}}};
            actor.prepareNativeVelocityControllers(setup);
            const std::array<std::uint32_t, 2> nodes{20, 8};
            EXPECT_EQ(actor.captureNativeControllerOrder(nodes), (std::vector<NifBullet::RagdollNativeControllerReference>{
                {NifBullet::RagdollNativeControllerKind::Velocity, 8}, {NifBullet::RagdollNativeControllerKind::Blend, 78}}));
        }
        mGraph.mBodies[0].mBlendController.reset(); mGraph.mBodies[1].mNodeRecord = 8;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeVelocitySetupRequest, 1> setup{{{8, {}, .25f}}};
        EXPECT_THROW(actor.prepareNativeVelocityControllers(setup), std::invalid_argument);
        EXPECT_TRUE(actor.captureNativeVelocityControllers().empty());
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, JoinedControllerPhasePublishesOriginalForceClockAndGainWithoutPose)
    {
        auto& body = mGraph.mBodies.front(); body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .5f, 0.f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {{.25f, .5f, 0.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 2.f, mPoses, 1, -1);
        ESM4::PhysicalVelocityControllerState velocity;
        velocity.mTiming = {0xd, 1, 0, 0, .25f}; velocity.mForceVector = {2, -3, 4, 0};
        const std::array<NifBullet::RagdollNativeVelocityControllerState, 1> controllers{{{8, 8, velocity, true}}};
        actor.restoreNativeVelocityControllers(controllers);
        const auto before = actor.capture()[0];
        const std::array<std::uint32_t, 1> nodes{8};
        const auto order = actor.captureNativeControllerOrder(nodes);
        ESM4::PhysicalBlendTimeCache cache;
        actor.advanceNativePhysicalControllers(order, 0.f, cache);
        EXPECT_FLOAT_EQ(float(actor.capture()[0].mLinearVelocity.x()), 3.2f);
        EXPECT_FLOAT_EQ(float(actor.capture()[0].mLinearVelocity.y()), -4.8f);
        EXPECT_FLOAT_EQ(float(actor.capture()[0].mLinearVelocity.z()), 6.4f);
        EXPECT_EQ(actor.capture()[0].mPose, before.mPose);
        EXPECT_EQ(actor.capture()[0].mAngularVelocity, before.mAngularVelocity);
        EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mClock.mPreviousTime, 0.f);
        EXPECT_EQ(actor.captureNativeVelocityControllers()[0].mState.mFrameDelta, .016f);
        EXPECT_NE(cache.mCycle, 0xffffffffu);
    }

    TEST_F(ActorRagdollPhysicsTest, JoinedControllerPhaseRemovesLaterVelocityBeforeItCanApplyForce)
    {
        auto& body = mGraph.mBodies.front(); body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .5f, 0.f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xcd, 1.f, 0.f, 0.f, 0.f, {{0.f, .5f, 0.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalVelocityControllerState velocity;
        velocity.mTiming = {0xd, 1, 0, 0, 0}; velocity.mForceVector = {2, 0, 0, 0};
        const std::array<NifBullet::RagdollNativeVelocityControllerState, 1> controllers{{{8, 8, velocity, false}}};
        actor.restoreNativeVelocityControllers(controllers);
        const std::array<std::uint32_t, 1> nodes{8}; const auto order = actor.captureNativeControllerOrder(nodes);
        ESM4::PhysicalBlendTimeCache cache;
        actor.advanceNativePhysicalControllers(order, 0.f, cache);
        EXPECT_TRUE(actor.captureNativeVelocityControllers().empty());
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(0, 0, 0));
        EXPECT_TRUE(actor.captureNativeBlendControllers()[0].mState.mKeys.empty());
        EXPECT_FALSE(actor.captureNativeBlendControllers()[0].mState.mTiming.mFlags & 8);
    }

    TEST_F(ActorRagdollPhysicsTest, JoinedControllerPhasePreservesEarlierForceWhenLaterBlendRemovesVelocity)
    {
        auto& body = mGraph.mBodies.front(); body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .5f, 0.f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xcd, 1.f, 0.f, 0.f, 0.f, {{0.f, .5f, 0.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalVelocityControllerState velocity;
        velocity.mTiming = {0xd, 1, 0, 0, 0}; velocity.mForceVector = {2, 0, 0, 0};
        const std::array<NifBullet::RagdollNativeVelocityControllerState, 1> controllers{{{8, 8, velocity, true}}};
        actor.restoreNativeVelocityControllers(controllers);
        const std::array<std::uint32_t, 1> nodes{8}; const auto order = actor.captureNativeControllerOrder(nodes);
        ESM4::PhysicalBlendTimeCache cache;
        actor.advanceNativePhysicalControllers(order, 0.f, cache);
        EXPECT_TRUE(actor.captureNativeVelocityControllers().empty());
        EXPECT_FLOAT_EQ(float(actor.capture()[0].mLinearVelocity.x()), 1.6f);
    }

    TEST_F(ActorRagdollPhysicsTest, JoinedControllerPhaseRollsBackLateForceOverflowAndBadOrderWithoutWake)
    {
        addHinge(); auto& body = mGraph.mBodies[0]; body.mNodeRecord = 8;
        mGraph.mBodies[1].mNodeRecord = 20;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .5f, 0.f};
        mGraph.mBodies[1].mBlend = NifBullet::RagdollBlendDefinition{31, 8, .5f, 0.f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {{0.f, .5f, 0.f}, {.25f, 0.f, 0.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalVelocityControllerState velocity;
        velocity.mTiming = {0xd, 1, 0, 0, 1}; velocity.mForceVector = {2, 0, 0, 0};
        auto late = velocity; late.mForceVector[0] = std::numeric_limits<float>::max();
        const std::array<NifBullet::RagdollNativeVelocityControllerState, 2> controllers{{{8, 8, velocity, true}, {20, 20, late, true}}};
        actor.restoreNativeVelocityControllers(controllers);
        const std::array<std::uint32_t, 2> nodes{8, 20}; const auto order = actor.captureNativeControllerOrder(nodes);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        auto* second = btRigidBody::upcast(actor.collisionObjects()[1]);
        first->setActivationState(ISLAND_SLEEPING); second->setActivationState(ISLAND_SLEEPING);
        ESM4::PhysicalBlendTimeCache cache;
        EXPECT_THROW(actor.advanceNativePhysicalControllers(order, 0.f, cache), std::invalid_argument);
        EXPECT_EQ(cache.mCycle, 0xffffffffu);
        EXPECT_EQ(actor.captureNativeVelocityControllers()[0].mState.mClock.mPreviousTime, -std::numeric_limits<float>::max());
        EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mClock.mPreviousTime, -std::numeric_limits<float>::max());
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(0, 0, 0));
        EXPECT_FALSE(first->isActive()); EXPECT_FALSE(second->isActive());
        auto invalid = order; invalid.back() = invalid.front();
        EXPECT_THROW(actor.advanceNativePhysicalControllers(invalid, 0.f, cache), std::invalid_argument);
        invalid = order; invalid.back().mIdentity = 999;
        EXPECT_THROW(actor.advanceNativePhysicalControllers(invalid, 0.f, cache), std::invalid_argument);
        EXPECT_THROW(actor.advanceNativePhysicalControllers({}, 0.f, cache), std::invalid_argument);
    }

    TEST_F(ActorRagdollPhysicsTest, JoinedControllerPhaseKeyframedForceNoopStillAdvancesOwnedClocks)
    {
        auto& body = mGraph.mBodies.front(); body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .5f, 0.f};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.setNativeMotionModes(key);
        ESM4::PhysicalVelocityControllerState velocity;
        velocity.mTiming = {0xd, 1, 0, 0, .25f}; velocity.mForceVector = {2, 0, 0, 0};
        const std::array<NifBullet::RagdollNativeVelocityControllerState, 1> controllers{{{8, 8, velocity, true}}};
        actor.restoreNativeVelocityControllers(controllers);
        const std::array<std::uint32_t, 1> nodes{8}; const auto order = actor.captureNativeControllerOrder(nodes);
        ESM4::PhysicalBlendTimeCache cache;
        actor.advanceNativePhysicalControllers(order, 0.f, cache);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(0, 0, 0));
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
        EXPECT_EQ(actor.captureNativeVelocityControllers()[0].mState.mClock.mPreviousTime, 0.f);
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, JoinedControllerPhaseRemovesVelocityFromRedirectedBlendTargetNode)
    {
        addHinge(); auto& body = mGraph.mBodies[0]; body.mNodeRecord = 8;
        mGraph.mBodies[1].mNodeRecord = 20;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .5f, 0.f};
        mGraph.mBodies[1].mBlend = NifBullet::RagdollBlendDefinition{31, 8, .5f, 0.f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 20, 0xcd, 1.f, 0.f, 0.f, 0.f, {{0.f, .5f, 0.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalVelocityControllerState velocity;
        velocity.mTiming = {0xd, 1, 0, 0, 1}; velocity.mForceVector = {2, 0, 0, 0};
        const std::array<NifBullet::RagdollNativeVelocityControllerState, 2> controllers{{{8, 8, velocity, false}, {20, 20, velocity, true}}};
        actor.restoreNativeVelocityControllers(controllers);
        const std::array<std::uint32_t, 2> nodes{8, 20}; const auto order = actor.captureNativeControllerOrder(nodes);
        ESM4::PhysicalBlendTimeCache cache;
        actor.advanceNativePhysicalControllers(order, 0.f, cache);
        const auto remaining = actor.captureNativeVelocityControllers();
        ASSERT_EQ(remaining.size(), 1u);
        EXPECT_EQ(remaining[0].mAttachedNode, 8u);
        EXPECT_FLOAT_EQ(float(actor.capture()[0].mLinearVelocity.x()), 1.6f);
        EXPECT_EQ(actor.capture()[1].mLinearVelocity, btVector3(0, 0, 0));
    }

    TEST_F(ActorRagdollPhysicsTest, JoinedControllerPhaseComposesForcesFromDistinctRedirectedControllers)
    {
        addHinge(); mGraph.mBodies[0].mNodeRecord = 8; mGraph.mBodies[1].mNodeRecord = 20;
        mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, .5f, 0.f};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalVelocityControllerState first;
        first.mTiming = {0xd, 1, 0, 0, 1}; first.mForceVector = {2, 0, 0, 0};
        auto second = first; second.mForceVector[0] = 1.f;
        const std::array<NifBullet::RagdollNativeVelocityControllerState, 2> controllers{{{8, 8, first, true}, {20, 8, second, true}}};
        actor.restoreNativeVelocityControllers(controllers);
        const std::array<std::uint32_t, 2> nodes{8, 20}; const auto order = actor.captureNativeControllerOrder(nodes);
        ESM4::PhysicalBlendTimeCache cache;
        actor.advanceNativePhysicalControllers(order, 0.f, cache);
        EXPECT_FLOAT_EQ(float(actor.capture()[0].mLinearVelocity.x()), 2.4f);
        EXPECT_EQ(actor.capture()[1].mLinearVelocity, btVector3(0, 0, 0));
        ASSERT_EQ(actor.captureNativeVelocityControllers().size(), 2u);
        EXPECT_EQ(actor.captureNativeVelocityControllers()[1].mState.mClock.mPreviousTime, 0.f);
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, FullDownSetupCreatesIndependentVelocityDurationAndNativeVector)
    {
        auto& body = mGraph.mBodies.front(); body.mNodeRecord = 8; body.mLinearDamping = .1f;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0x1d, .25f, -.125f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 2.f, mPoses, 1, -1);
        const auto before = actor.capture()[0];
        const std::array<NifBullet::RagdollNativeKnockdownControllerSetupRequest, 1> requests{{{8, {1, -2, .5f}, .25f}}};
        const auto result = actor.prepareNativeKnockdownControllerSetup(requests, {20000.f, 1.2f});
        ASSERT_EQ(result.size(), 1u);
        EXPECT_EQ(result[0], NifBullet::RagdollNativeKnockdownBlendDisposition::Started);
        const auto blend = actor.captureNativeBlendControllers()[0];
        EXPECT_EQ(blend.mState.mTiming.mFlags, 0xddu);
        EXPECT_EQ(blend.mState.mTiming.mStopKey, .25f);
        EXPECT_EQ(blend.mState.mSetupState, 2u);
        ASSERT_EQ(blend.mState.mKeys.size(), 2u);
        const auto velocity = actor.captureNativeVelocityControllers();
        ASSERT_EQ(velocity.size(), 1u);
        EXPECT_EQ(velocity[0].mState.mTiming.mFlags, 0xdu);
        EXPECT_EQ(velocity[0].mState.mTiming.mStopKey, 1.2f);
        // Independent complete initial-PE-cache original8AB440 oracle03, case57, executable
        // a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6.
        const std::array<std::uint32_t, 4> expected{1169771283, 3325643539, 1161382675, 1057279181};
        for (unsigned i = 0; i < 4; ++i)
            EXPECT_EQ(std::bit_cast<std::uint32_t>(velocity[0].mState.mForceVector[i]), expected[i]);
        EXPECT_EQ(actor.capture()[0].mPose, before.mPose);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, before.mLinearVelocity);
        EXPECT_EQ(actor.capture()[0].mAngularVelocity, before.mAngularVelocity);
        EXPECT_EQ(actor.captureNativeBlendStates()[0].mGains.mHierarchy, .9f);
    }

    TEST_F(ActorRagdollPhysicsTest, FullDownSetupSkipsExistingVelocityCompletelyIncludingUnusedWorldVector)
    {
        addHinge(); auto& body = mGraph.mBodies[0]; body.mNodeRecord = 8; mGraph.mBodies[1].mNodeRecord = 20;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, .25f, -.125f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalVelocityControllerState velocity;
        velocity.mTiming = {0xffff, 9.f, 8.f, 0.f, 6.f}; velocity.mClock = {5, 4, 3};
        velocity.mForceVector = {2, -3, 4, 99}; velocity.mFrameDelta = 99;
        const std::array<NifBullet::RagdollNativeVelocityControllerState, 1> existing{{{8, 20, velocity, false}}};
        actor.restoreNativeVelocityControllers(existing);
        const float bad = std::numeric_limits<float>::quiet_NaN();
        const std::array<NifBullet::RagdollNativeKnockdownControllerSetupRequest, 1> requests{{{8, {bad, bad, bad}, .25f}}};
        const auto result = actor.prepareNativeKnockdownControllerSetup(requests, {bad, bad});
        ASSERT_EQ(result.size(), 1u);
        EXPECT_EQ(result[0], NifBullet::RagdollNativeKnockdownBlendDisposition::Started);
        const auto after = actor.captureNativeVelocityControllers()[0];
        EXPECT_EQ(after.mTargetNode, 20u); EXPECT_FALSE(after.mPrecedesBlend);
        EXPECT_EQ(after.mState.mTiming.mFlags, 0xffffu);
        EXPECT_EQ(after.mState.mTiming.mFrequency, 9.f); EXPECT_EQ(after.mState.mTiming.mPhase, 8.f);
        EXPECT_EQ(after.mState.mTiming.mStartKey, 0.f); EXPECT_EQ(after.mState.mTiming.mStopKey, 6.f);
        EXPECT_EQ(after.mState.mClock.mStartTime, 5.f); EXPECT_EQ(after.mState.mClock.mPreviousTime, 4.f);
        EXPECT_EQ(after.mState.mClock.mElapsed, 3.f); EXPECT_EQ(after.mState.mFrameDelta, 99.f);
        EXPECT_EQ(after.mState.mForceVector, velocity.mForceVector);
        EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mSetupState, 2u);
    }

    TEST_F(ActorRagdollPhysicsTest, FullDownSetupRollsBackBothControllerKindsOnLateFailureBeforeWake)
    {
        addHinge(); auto& body = mGraph.mBodies[0]; body.mNodeRecord = 8;
        mGraph.mBodies[1].mNodeRecord = 20;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, .25f, -.125f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        mGraph.mBodies[1].mBlend = NifBullet::RagdollBlendDefinition{31, 8, .7f, .6f};
        mGraph.mBodies[1].mBlendController = NifBullet::RagdollBlendControllerDefinition{
            79, 20, 0xd, .25f, -.125f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        auto* second = btRigidBody::upcast(actor.collisionObjects()[1]);
        first->setActivationState(ISLAND_SLEEPING); second->setActivationState(ISLAND_SLEEPING);
        const float bad = std::numeric_limits<float>::quiet_NaN();
        const float max = std::numeric_limits<float>::max();
        for (const auto& requests : std::array<std::array<NifBullet::RagdollNativeKnockdownControllerSetupRequest, 2>, 5>{{
            {{{8, {1, 0, 0}, .25f}, {20, {bad, 0, 0}, .25f}}},
            {{{8, {1, 0, 0}, .25f}, {20, {max, 0, 0}, .25f}}},
            {{{8, {1, 0, 0}, .25f}, {20, {}, bad}}},
            {{{8, {1, 0, 0}, .25f}, {8, {}, .25f}}},
            {{{8, {1, 0, 0}, .25f}, {999, {}, .25f}}}}})
        {
            EXPECT_THROW(actor.prepareNativeKnockdownControllerSetup(requests, {20000.f, 1.2f}), std::invalid_argument);
            EXPECT_TRUE(actor.captureNativeVelocityControllers().empty());
            EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mSetupState, 0u);
            EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mKeys.size(), 1u);
            EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mTiming.mFrequency, .25f);
            EXPECT_FALSE(first->isActive()); EXPECT_FALSE(second->isActive());
        }
        EXPECT_TRUE(actor.prepareNativeKnockdownControllerSetup({}, {20000.f, 1.2f}).empty());
        const std::array<NifBullet::RagdollNativeKnockdownControllerSetupRequest, 2> valid{{
            {8, {1, 0, 0}, .25f}, {20, {}, 1.f}}};
        ASSERT_EQ(actor.prepareNativeKnockdownControllerSetup(valid, {20000.f, 1.2f}).size(), 2u);
        ASSERT_EQ(actor.captureNativeVelocityControllers().size(), 2u);
        EXPECT_EQ(actor.captureNativeVelocityControllers()[1].mState.mTiming.mStopKey, 1.2f);
    }

    TEST_F(ActorRagdollPhysicsTest, FullDownSetupSkipsMissingAndDisabledBeforeUnusedInputs)
    {
        auto& body = mGraph.mBodies.front(); body.mNodeRecord = 8;
        const float bad = std::numeric_limits<float>::quiet_NaN();
        const std::array<NifBullet::RagdollNativeKnockdownControllerSetupRequest, 1> unused{{{8, {bad, bad, bad}, bad}}};
        {
            NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
            const auto result = actor.prepareNativeKnockdownControllerSetup(unused, {bad, bad});
            ASSERT_EQ(result.size(), 1u);
            EXPECT_EQ(result[0], NifBullet::RagdollNativeKnockdownBlendDisposition::MissingBlend);
            EXPECT_TRUE(actor.captureNativeVelocityControllers().empty());
        }
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        {
            NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
            const auto result = actor.prepareNativeKnockdownControllerSetup(unused, {bad, bad});
            ASSERT_EQ(result.size(), 1u);
            EXPECT_EQ(result[0], NifBullet::RagdollNativeKnockdownBlendDisposition::MissingController);
            EXPECT_TRUE(actor.captureNativeVelocityControllers().empty());
        }
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, .25f, -.125f, 0.f, .25f, {{.25f, 1.f, 1.f}}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeKnockdownControllerSetupRequest, 1> disabled{{{8, {bad, bad, bad}, -1.f}}};
        ASSERT_EQ(actor.prepareNativeKnockdownControllerSetup(disabled, {bad, bad})[0],
            NifBullet::RagdollNativeKnockdownBlendDisposition::Disabled);
        EXPECT_TRUE(actor.captureNativeVelocityControllers().empty());
        EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mSetupState, 0u);
    }

    TEST_F(ActorRagdollPhysicsTest, FullDownSetupZeroDurationStillCreatesOnePointTwoVelocityController)
    {
        auto& body = mGraph.mBodies.front(); body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, 1.f, 1.f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeKnockdownControllerSetupRequest, 1> requests{{{8, {}, 0.f}}};
        ASSERT_EQ(actor.prepareNativeKnockdownControllerSetup(requests, {20000.f, 1.2f}).size(), 1u);
        EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mTiming.mStopKey, 0.f);
        const auto velocity = actor.captureNativeVelocityControllers();
        ASSERT_EQ(velocity.size(), 1u);
        EXPECT_EQ(velocity[0].mState.mTiming.mStopKey, 1.2f);
        EXPECT_EQ(velocity[0].mState.mForceVector, (std::array<float, 4>{}));
    }
    TEST_F(ActorRagdollPhysicsTest, FullDownSetupUsesResolvedSignedForceAndIndependentTime)
    {
        auto& body = mGraph.mBodies.front(); body.mNodeRecord = 8; body.mLinearDamping = 0.f;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, 1.f, 1.f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {}};
        const std::array<NifBullet::RagdollNativeKnockdownControllerSetupRequest, 1> requests{{{8, {1, -2, .5f}, .25f}}};
        // Original settings-copy prefix and full normal leaf, oracle04.
        {
            NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
            actor.prepareNativeKnockdownControllerSetup(requests, {-10.0f, 1.2f});
            const auto velocity = actor.captureNativeVelocityControllers()[0];
            EXPECT_EQ(velocity.mState.mTiming.mStopKey, 1.2f);
            const std::array<std::uint32_t, 4> expected{3224822233u, 1085727193u, 3216433625u, 1056964608u};
            for (unsigned i = 0; i < 4; ++i)
                EXPECT_EQ(std::bit_cast<std::uint32_t>(velocity.mState.mForceVector[i]), expected[i]);
        }
        {
            NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
            actor.prepareNativeKnockdownControllerSetup(requests, {0.125f, 2.0f});
            const auto velocity = actor.captureNativeVelocityControllers()[0];
            EXPECT_EQ(velocity.mState.mTiming.mStopKey, 2.0f);
            const std::array<std::uint32_t, 4> expected{1024609863u, 3180482119u, 1016221255u, 1056964608u};
            for (unsigned i = 0; i < 4; ++i)
                EXPECT_EQ(std::bit_cast<std::uint32_t>(velocity.mState.mForceVector[i]), expected[i]);
        }
        {
            NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
            actor.prepareNativeKnockdownControllerSetup(requests, {0.0f, 0.0f});
            const auto velocity = actor.captureNativeVelocityControllers()[0];
            EXPECT_EQ(velocity.mState.mTiming.mStopKey, 0.0f);
            const std::array<std::uint32_t, 4> expected{0u, 2147483648u, 0u, 1056964608u};
            for (unsigned i = 0; i < 4; ++i)
                EXPECT_EQ(std::bit_cast<std::uint32_t>(velocity.mState.mForceVector[i]), expected[i]);
        }
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        const float bad = std::numeric_limits<float>::quiet_NaN();
        const float huge = std::numeric_limits<float>::max();
        for (const auto settings : std::array<NifBullet::RagdollNativePassOutSettings, 5>{{
            {bad, 1.f}, {1.f, bad}, {1.f, -1.f}, {huge, 1.f}, {-huge, 1.f}}})
        {
            EXPECT_THROW(actor.prepareNativeKnockdownControllerSetup(requests, settings), std::invalid_argument);
            EXPECT_TRUE(actor.captureNativeVelocityControllers().empty());
            EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mSetupState, 0u);
            EXPECT_TRUE(actor.captureNativeBlendControllers()[0].mState.mKeys.empty());
        }
    }

}


namespace
{
    TEST_F(ActorRagdollPhysicsTest, NativeLinkedDefinitionUsesExplicitFiltersAndPreservesAuthoredProperties)
    {
        auto& body = mGraph.mBodies[0]; body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 1, .9f, .8f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{78, 8, 0xd, 1, 0, 0, .25f, {}};
        auto gains = ESM4::InitialPhysicalBlendGainTable;
        gains[17] = {.53125f, .875f};
        const std::array<std::uint32_t, 1> filters{0x1108};
        const auto linked = NifBullet::ragdollDefinitionWithNativeLinkedBlendState(mGraph, filters, gains);
        EXPECT_EQ(linked.mBodies[0].mBlend->mFlags, 9u);
        EXPECT_EQ(linked.mBodies[0].mBlend->mHierarchyGain, .53125f);
        EXPECT_EQ(linked.mBodies[0].mBlend->mVelocityGain, .875f);
        EXPECT_EQ(linked.mSourceHash, mGraph.mSourceHash);
        EXPECT_EQ(linked.mBodies[0].mMass, body.mMass);
        EXPECT_EQ(linked.mBodies[0].mInertia, body.mInertia);
        EXPECT_EQ(linked.mBodies[0].mBlendController->mRecord, 78u);
        EXPECT_TRUE(linked.mBodies[0].mBlendController->mKeys.empty());
        EXPECT_EQ(body.mBlend->mFlags, 1u);
        EXPECT_EQ(body.mBlend->mHierarchyGain, .9f);
        NifBullet::ActorRagdollPhysics actor(linked, mWorld, 1.f, mPoses, 1, -1);
        EXPECT_EQ(actor.captureNativeBlendStates()[0].mGains.mHierarchy, .53125f);
        EXPECT_EQ(actor.captureNativeBlendStates()[0].mRequestedMotion, 8u);
        const std::array<NifBullet::RagdollNativeKnockdownBlendRequest, 1> down{{{8, .25f}}};
        actor.prepareNativeKnockdownBlends(down);
        EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mKeys[0].mGains.mHierarchy, .53125f);
        EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState.mKeys[0].mGains.mVelocity, .875f);
    }

    TEST_F(ActorRagdollPhysicsTest, NativeLinkedDefinitionRejectsUsedSettingsBeforeAdmissionAndIgnoresUnused)
    {
        mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 1, .9f, .8f};
        const std::array<std::uint32_t, 1> filters{0x1108};
        auto gains = ESM4::InitialPhysicalBlendGainTable;
        gains[17].mVelocity = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(NifBullet::ragdollDefinitionWithNativeLinkedBlendState(mGraph, filters, gains), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollDefinitionWithNativeLinkedBlendState(mGraph, {}, gains), std::invalid_argument);
        EXPECT_EQ(mGraph.mBodies[0].mBlend->mFlags, 1u);
        EXPECT_EQ(mGraph.mBodies[0].mBlend->mVelocityGain, .8f);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        gains[17] = {-0.f, 2.f};
        gains[0].mHierarchy = std::numeric_limits<float>::quiet_NaN();
        const auto linked = NifBullet::ragdollDefinitionWithNativeLinkedBlendState(mGraph, filters, gains);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(linked.mBodies[0].mBlend->mHierarchyGain), 0x80000000u);
        EXPECT_EQ(linked.mBodies[0].mBlend->mVelocityGain, 2.f);
        mGraph.mBodies[0].mBlend.reset();
        EXPECT_NO_THROW(NifBullet::ragdollDefinitionWithNativeLinkedBlendState(mGraph, filters, gains));
    }

    TEST_F(ActorRagdollPhysicsTest, NativeLinkedRequestedMotionTracksOwnedUpdatesAndSceneRollback)
    {
        auto& body = mGraph.mBodies[0]; body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 1, 1.f, 1.f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{78, 8, 0, 1, 0, 0, .25f, {}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        const std::array<NifBullet::RagdollBoneWorldPose, 1> bones{{{8, osg::Matrixf::translate(0, 0, 2)}}};
        const std::array<std::uint32_t, 1> order{78};
        ESM4::PhysicalBlendTimeCache cache;
        actor.updateNativeBlendFrame(bones, order, 0.f, cache, .016f, 2, NifBullet::RagdollNativeDefaultGravityZ);
        EXPECT_EQ(actor.captureNativeBlendStates()[0].mRequestedMotion, 8u);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        EXPECT_THROW(actor.updateNativeBlendFrame(bones, order, 0.f, cache, .016f, 0,
            NifBullet::RagdollNativeDefaultGravityZ, [](auto) { throw std::runtime_error("scene rejects request"); }),
            std::runtime_error);
        EXPECT_EQ(actor.captureNativeBlendStates()[0].mRequestedMotion, 8u);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        actor.updateNativeBlendFrame(bones, order, 0.f, cache, .016f, 0, NifBullet::RagdollNativeDefaultGravityZ);
        EXPECT_EQ(actor.captureNativeBlendStates()[0].mRequestedMotion, 6u);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
        const std::array<NifBullet::RagdollNativeKnockdownBlendRequest, 1> down{{{8, .25f}}};
        actor.prepareNativeKnockdownBlends(down);
        actor.updateNativeBlendFrame(bones, order, 1.f, cache, .016f, 0, NifBullet::RagdollNativeDefaultGravityZ);
        EXPECT_EQ(actor.captureNativeBlendStates()[0].mRequestedMotion, 6u);
        actor.updateNativeBlendFrame(bones, order, 1.25f, cache, .016f, 0, NifBullet::RagdollNativeDefaultGravityZ);
        EXPECT_EQ(actor.captureNativeBlendStates()[0].mRequestedMotion, 1u);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, CurrentMotionVelocitySetupUsesKeyframedDampingAndRestoresDynamicDamping)
    {
        auto& body = mGraph.mBodies.front(); body.mNodeRecord = 8; body.mLinearDamping = .1f;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 2.f, mPoses, 1, -1);
        ESM4::PhysicalVelocityControllerState old; old.mClock.mElapsed = 7.f; old.mFrameDelta = 99.f;
        const std::array<NifBullet::RagdollNativeVelocityControllerState, 1> saved{{{8, std::nullopt, old, false}}};
        actor.restoreNativeVelocityControllers(saved);
        const std::array<NifBullet::RagdollNativeVelocitySetupRequest, 1> requests{{{8, {1, -2, .5f, 0}, .25f}}};
        // Original full converter + generic setup oracle03 cases16/88/160.
        const std::array<std::array<std::uint32_t, 4>, 3> expected{{
            {1074056397, 3229928653, 1065667789, 0},
            {1073741824, 3229614080, 1065353216, 0},
            {1074056397, 3229928653, 1065667789, 0}}};
        for (unsigned mode = 0; mode < 3; ++mode)
        {
            if (mode)
            {
                const std::array<NifBullet::RagdollNativeMotionRequest, 1> motion{{{12, mode == 1
                    ? NifBullet::RagdollNativeMotion::Keyframed : NifBullet::RagdollNativeMotion::Dynamic}}};
                actor.setNativeMotionModes(motion);
            }
            const auto before = actor.capture()[0];
            const auto activation = actor.collisionObjects()[0]->getActivationState();
            actor.prepareNativeVelocityControllers(requests);
            const auto controllers = actor.captureNativeVelocityControllers(); ASSERT_EQ(controllers.size(), 1u);
            const auto& controller = controllers[0];
            for (unsigned lane = 0; lane < 4; ++lane)
                EXPECT_EQ(std::bit_cast<std::uint32_t>(controller.mState.mForceVector[lane]), expected[mode][lane]);
            EXPECT_FALSE(controller.mTargetNode); EXPECT_FALSE(controller.mPrecedesBlend);
            EXPECT_EQ(controller.mState.mClock.mElapsed, 7.f); EXPECT_EQ(controller.mState.mFrameDelta, 99.f);
            EXPECT_EQ(actor.capture()[0].mPose, before.mPose);
            EXPECT_EQ(actor.capture()[0].mLinearVelocity, before.mLinearVelocity);
            EXPECT_EQ(actor.collisionObjects()[0]->getActivationState(), activation);
            EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, mode == 1
                ? NifBullet::RagdollNativeMotion::Keyframed : NifBullet::RagdollNativeMotion::Dynamic);
        }
    }

    TEST_F(ActorRagdollPhysicsTest, CurrentMotionFullDownUsesKeyframedDampingWithoutChangingMode)
    {
        auto& body = mGraph.mBodies.front(); body.mNodeRecord = 8; body.mLinearDamping = .1f;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 2.f, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> motion{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.setNativeMotionModes(motion);
        const auto before = actor.capture()[0];
        const std::array<NifBullet::RagdollNativeKnockdownControllerSetupRequest, 1> requests{{{8, {1, -2, .5f}, .25f}}};
        ASSERT_EQ(actor.prepareNativeKnockdownControllerSetup(requests, {-10.f, 1.2f}).size(), 1u);
        const auto controllers = actor.captureNativeVelocityControllers(); ASSERT_EQ(controllers.size(), 1u);
        // Original converter + configured settings copy + full normalDown oracle01 case592.
        const std::array<std::uint32_t, 4> expected{3224822233, 1085727193, 3216433625, 1056964608};
        for (unsigned lane = 0; lane < 4; ++lane)
            EXPECT_EQ(std::bit_cast<std::uint32_t>(controllers[0].mState.mForceVector[lane]), expected[lane]);
        EXPECT_EQ(controllers[0].mState.mTiming.mStopKey, 1.2f);
        EXPECT_EQ(actor.capture()[0].mPose, before.mPose);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
        actor.prepareNativeKnockdownControllerSetup(requests, {20000.f, .25f});
        EXPECT_EQ(actor.captureNativeVelocityControllers()[0].mState.mForceVector, controllers[0].mState.mForceVector);
        EXPECT_EQ(actor.captureNativeVelocityControllers()[0].mState.mTiming.mStopKey, 1.2f);
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, RequestedMotionDownSynchronizesBeforeDisabledOrMissingController)
    {
        auto& body = mGraph.mBodies[0]; body.mNodeRecord = 8;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        for (bool controller : {false, true})
        {
            body.mBlendController.reset();
            if (controller) body.mBlendController = NifBullet::RagdollBlendControllerDefinition{78, 8, 0xd, 1, 0, 0, .25f, {}};
            for (unsigned requested : {1u, 6u})
            {
                NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 2.f, mPoses, 1, -1);
                auto states = actor.captureNativeBlendStates(); states[0].mRequestedMotion = requested;
                actor.restoreNativeBlendStates(states);
                const auto desired = requested == 6 ? NifBullet::RagdollNativeMotion::Keyframed : NifBullet::RagdollNativeMotion::Dynamic;
                const std::array<NifBullet::RagdollNativeMotionRequest, 1> opposite{{{12, requested == 6
                    ? NifBullet::RagdollNativeMotion::Dynamic : NifBullet::RagdollNativeMotion::Keyframed}}};
                actor.setNativeMotionModes(opposite);
                const auto before = actor.capture()[0];
                const auto bad = std::numeric_limits<float>::quiet_NaN();
                const std::array<NifBullet::RagdollNativeKnockdownControllerSetupRequest, 1> requests{{{8, {bad, bad, bad}, -1}}};
                const auto result = actor.prepareNativeKnockdownControllerSetup(requests, {bad, bad});
                ASSERT_EQ(result.size(), 1u);
                EXPECT_EQ(result[0], controller ? NifBullet::RagdollNativeKnockdownBlendDisposition::Disabled
                    : NifBullet::RagdollNativeKnockdownBlendDisposition::MissingController);
                EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, desired);
                EXPECT_EQ(actor.captureNativeBlendStates()[0].mRequestedMotion, requested);
                EXPECT_EQ(actor.captureNativeBlendStates()[0].mCollisionFlags, 8u);
                EXPECT_EQ(actor.capture()[0].mPose, before.mPose);
                EXPECT_EQ(actor.capture()[0].mLinearVelocity, before.mLinearVelocity);
                EXPECT_TRUE(actor.captureNativeVelocityControllers().empty());
            }
        }
    }

    TEST_F(ActorRagdollPhysicsTest, RequestedMotionDownStagesConversionAndCurrentDampingAtomically)
    {
        addHinge(); auto& body = mGraph.mBodies[0]; body.mNodeRecord = 8; body.mLinearDamping = .1f;
        body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        body.mBlendController = NifBullet::RagdollBlendControllerDefinition{78, 8, 0xd, 1, 0, 0, .25f, {}};
        mGraph.mBodies[1].mNodeRecord = 20;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 2.f, mPoses, 1, -1);
        auto states = actor.captureNativeBlendStates(); states[0].mRequestedMotion = 6;
        actor.restoreNativeBlendStates(states);
        const auto before = actor.capture()[0]; const auto activation = actor.collisionObjects()[0]->getActivationState();
        const std::array<NifBullet::RagdollNativeKnockdownControllerSetupRequest, 2> late{{{8, {1, -2, .5f}, .25f}, {999, {}, .25f}}};
        EXPECT_THROW(actor.prepareNativeKnockdownControllerSetup(late, {-10.f, 1.2f}), std::invalid_argument);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        EXPECT_EQ(actor.collisionObjects()[0]->getActivationState(), activation);
        EXPECT_TRUE(actor.captureNativeVelocityControllers().empty());
        EXPECT_TRUE(actor.captureNativeBlendControllers()[0].mState.mKeys.empty());
        const std::array<NifBullet::RagdollNativeKnockdownControllerSetupRequest, 1> valid{{late[0]}};
        actor.prepareNativeKnockdownControllerSetup(valid, {-10.f, 1.2f});
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
        const auto velocity = actor.captureNativeVelocityControllers(); ASSERT_EQ(velocity.size(), 1u);
        // Original214 case592, and215 request6/current Dynamic full-entry captures.
        const std::array<std::uint32_t, 4> expected{3224822233, 1085727193, 3216433625, 1056964608};
        for (unsigned lane = 0; lane < 4; ++lane)
            EXPECT_EQ(std::bit_cast<std::uint32_t>(velocity[0].mState.mForceVector[lane]), expected[lane]);
        EXPECT_EQ(actor.capture()[0].mPose, before.mPose);
    }

    TEST_F(ActorRagdollPhysicsTest, RequestedMotionMetadataRestoreIsCompleteAtomicAndIndependentOfBodies)
    {
        addHinge(); mGraph.mBodies[0].mNodeRecord = 8; mGraph.mBodies[1].mNodeRecord = 20;
        for (auto& body : mGraph.mBodies) body.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 2.f, mPoses, 1, -1);
        auto states = actor.captureNativeBlendStates(); ASSERT_EQ(states.size(), 2u);
        states[0].mRequestedMotion = 0xffffffff; states[0].mCollisionFlags = 0xffff;
        states[0].mGains = {-0.f, 2.f}; states[1].mRequestedMotion = 6;
        actor.restoreNativeBlendStates(states);
        auto restored = actor.captureNativeBlendStates();
        EXPECT_EQ(restored[0].mRequestedMotion, 0xffffffffu); EXPECT_EQ(restored[0].mCollisionFlags, 0xffffu);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(restored[0].mGains.mHierarchy), 0x80000000u);
        EXPECT_EQ(restored[0].mGains.mVelocity, 2.f);
        EXPECT_EQ(actor.captureNativeMotionModes()[1].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        auto invalid = states; invalid[1].mBodyRecord = states[0].mBodyRecord;
        EXPECT_THROW(actor.restoreNativeBlendStates(invalid), std::invalid_argument);
        invalid = states; invalid[1].mBodyRecord = 999;
        EXPECT_THROW(actor.restoreNativeBlendStates(invalid), std::invalid_argument);
        invalid = states; invalid[1].mGains.mVelocity = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(actor.restoreNativeBlendStates(invalid), std::invalid_argument);
        EXPECT_THROW(actor.restoreNativeBlendStates(std::span<const NifBullet::RagdollNativeBlendState>(states).first(1)), std::invalid_argument);
        restored = actor.captureNativeBlendStates();
        EXPECT_EQ(restored[0].mRequestedMotion, states[0].mRequestedMotion);
        EXPECT_EQ(restored[1].mRequestedMotion, states[1].mRequestedMotion);
        EXPECT_EQ(restored[1].mGains.mVelocity, states[1].mGains.mVelocity);
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, IndependentRequestedFrameSkipsConversionWhenStoredRequestMatches)
    {
        auto& definition = mGraph.mBodies[0]; definition.mNodeRecord = 8;
        definition.mBlend = NifBullet::RagdollBlendDefinition{30, 8, 1.f, 1.f};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        auto states = actor.captureNativeBlendStates(); states[0].mRequestedMotion = 6;
        actor.restoreNativeBlendStates(states);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        body->setLinearVelocity({1, 2, 3}); body->setAngularVelocity({4, 5, 6});
        const std::array<NifBullet::RagdollNativeBlendUpdate, 1> updates{{{12, osg::Matrixf::translate(0, 0, 5), 1.f, 1.f, 8}}};
        const auto result = actor.updateNativeBlends(updates, .016f, 0, 0.f);
        ASSERT_EQ(result.size(), 1u);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        EXPECT_EQ(result[0].mCollisionFlags, 8u);
        EXPECT_EQ(body->getLinearVelocity(), btVector3(1, 2, 3));
        EXPECT_EQ(body->getAngularVelocity(), btVector3(4, 5, 6));
        EXPECT_EQ(actor.captureNativeBlendStates()[0].mRequestedMotion, 6u);
        // Original89EAE0 selects8A3900 for actual Dynamic motion. With no
        // native World/authority binding that setter preserves the body pose.
        EXPECT_EQ(actor.capture()[0].mPose.getOrigin(), mPoses[0].getOrigin());
    }

    TEST_F(ActorRagdollPhysicsTest, IndependentRequestedFrameTransitionsMetadataEvenWhenActualModeMatches)
    {
        auto& definition = mGraph.mBodies[0]; definition.mNodeRecord = 8;
        definition.mBlend = NifBullet::RagdollBlendDefinition{30, 0, 0.f, 0.f};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        body->setLinearVelocity({1, 2, 3}); body->setAngularVelocity({4, 5, 6});
        const std::array<NifBullet::RagdollNativeBlendUpdate, 1> updates{{{12, osg::Matrixf::identity(), 0.f, 0.f, 0}}};
        const auto result = actor.updateNativeBlends(updates, .016f, 0, 0.f);
        ASSERT_EQ(result.size(), 1u);
        EXPECT_EQ(result[0].mCollisionFlags, 8u);
        EXPECT_EQ(body->getLinearVelocity(), btVector3(0, 0, 0));
        EXPECT_EQ(body->getAngularVelocity(), btVector3(0, 0, 0));
        const auto state = actor.captureNativeBlendStates()[0];
        EXPECT_EQ(state.mRequestedMotion, 1u); EXPECT_EQ(state.mCollisionFlags, 8u);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
    }
}

namespace
{
    NifBullet::RagdollNativeWorldSceneRequest preparedWorldScene(std::uint32_t record)
    {
        NifBullet::RagdollNativeWorldSceneRequest result{record, {}};
        result.mInput.mHasWorld = result.mInput.mHasAuthority = true;
        result.mInput.mCurrentRotation = {0, 0, 0, 1};
        result.mInput.mTargetRotation = {0, 0, .70710677f, .70710677f};
        result.mInput.mTargetPosition = {142.87672424316406f, -285.7534484863281f, 71.43836212158203f, 0};
        result.mInput.mFrameSeconds = .016f;
        return result;
    }

    TEST_F(ActorRagdollPhysicsTest, WorldScenePackedPublicationPreservesPoseModeForcesAndIdentity)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7.f, mPoses, 1, -1);
        const std::array modes{NifBullet::RagdollNativeMotionRequest{12, NifBullet::RagdollNativeMotion::Keyframed}};
        actor.setNativeMotionModes(modes);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        body->applyCentralForce({1, 2, 3}); body->applyTorque({4, 5, 6});
        const auto pose = body->getWorldTransform();
        const auto* shape = body->getCollisionShape(); const auto* proxy = body->getBroadphaseHandle();
        const std::array requests{preparedWorldScene(12)};
        bool called = false;
        const auto written = actor.synchronizeNativeWorldScenes(requests, [&](auto ids) {
            called = true; ASSERT_EQ(ids.size(), 1u); EXPECT_EQ(ids[0], 12u);
            EXPECT_EQ(body->getLinearVelocity(), btVector3(0, 0, 0));
            EXPECT_EQ(actor.captureNativePackedVelocities()[0].mVelocities.mAngular[3], 0.f);
        });
        EXPECT_TRUE(called); ASSERT_EQ(written.size(), 1u); EXPECT_EQ(written[0], 12u);
        const auto state = actor.captureNativePackedVelocities()[0];
        // Original03 uncapped input/output capture; fourth angular lane is real.
        EXPECT_EQ(state.mVelocities.mLinear[0], 8929.794921875f);
        EXPECT_EQ(state.mVelocities.mLinear[1], -17859.58984375f);
        EXPECT_EQ(state.mVelocities.mLinear[2], 4464.8974609375f);
        EXPECT_EQ(state.mVelocities.mLinear[3], 0.f);
        EXPECT_NEAR(state.mVelocities.mAngular[2], 98.17475891113281f, .001f);
        EXPECT_NEAR(state.mVelocities.mAngular[3], 98.17475891113281f, .001f);
        EXPECT_EQ(body->getLinearVelocity()[0], btScalar(8929.794921875f) * 7);
        EXPECT_EQ(body->getWorldTransform(), pose);
        EXPECT_EQ(body->getTotalForce(), btVector3(1, 2, 3)); EXPECT_EQ(body->getTotalTorque(), btVector3(4, 5, 6));
        EXPECT_EQ(body->getCollisionShape(), shape); EXPECT_EQ(body->getBroadphaseHandle(), proxy);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
    }

    TEST_F(ActorRagdollPhysicsTest, WorldScenePackedBatchRejectsLateInvalidAndThrowingHookBeforeMutation)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 2.5f, mPoses, 1, -1);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        body->forceActivationState(ISLAND_SLEEPING);
        auto invalid = preparedWorldScene(24); invalid.mInput.mTargetRotation = {0, 0, 0, 0};
        std::array requests{preparedWorldScene(12), invalid};
        bool called = false;
        EXPECT_THROW(actor.synchronizeNativeWorldScenes(requests, [&](auto) { called = true; }), std::invalid_argument);
        EXPECT_FALSE(called); EXPECT_EQ(body->getActivationState(), ISLAND_SLEEPING);
        EXPECT_EQ(body->getLinearVelocity(), btVector3(0, 0, 0));
        requests[1] = preparedWorldScene(24);
        EXPECT_THROW(actor.synchronizeNativeWorldScenes(requests, [](auto) { throw std::runtime_error("scene failure"); }), std::runtime_error);
        EXPECT_EQ(body->getActivationState(), ISLAND_SLEEPING);
        EXPECT_EQ(actor.captureNativePackedVelocities()[0].mVelocities.mAngular[3], 0.f);
        requests[1].mRecord = 12;
        EXPECT_THROW(actor.synchronizeNativeWorldScenes(requests), std::invalid_argument);
        requests[1].mRecord = 99;
        EXPECT_THROW(actor.synchronizeNativeWorldScenes(requests), std::invalid_argument);
        requests[1] = preparedWorldScene(24);
        requests[0].mInput.mHasWorld = false; requests[0].mInput.mTargetRotation[0] = std::numeric_limits<float>::quiet_NaN();
        requests[1].mInput.mFrameSeconds = 0;
        EXPECT_TRUE(actor.synchronizeNativeWorldScenes(requests).empty());
        EXPECT_EQ(body->getActivationState(), ISLAND_SLEEPING);
    }

    TEST_F(ActorRagdollPhysicsTest, WorldScenePackedSnapshotRestoresAllLanesAtomically)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7.f, mPoses, 1, -1);
        auto states = actor.captureNativePackedVelocities(); ASSERT_EQ(states.size(), 2u);
        states[0].mVelocities = {{1, 2, 3, -0.f}, {5, 6, 7, 8}};
        states[1].mVelocities = {{-1, -2, -3, 4}, {-5, -6, -7, -8}};
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]); body->forceActivationState(ISLAND_SLEEPING);
        const auto pose = body->getWorldTransform();
        actor.restoreNativePackedVelocities(states);
        auto actual = actor.captureNativePackedVelocities();
        EXPECT_EQ(actual[0].mVelocities.mLinear, states[0].mVelocities.mLinear);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(actual[0].mVelocities.mLinear[3]), 0x80000000u);
        EXPECT_EQ(actual[1].mVelocities.mAngular, states[1].mVelocities.mAngular);
        EXPECT_EQ(body->getActivationState(), ISLAND_SLEEPING); EXPECT_EQ(body->getWorldTransform(), pose);
        auto invalid = states; invalid[0].mVelocities.mLinear[0] = 99;
        invalid[1].mVelocities.mAngular[3] = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(actor.restoreNativePackedVelocities(invalid), std::invalid_argument);
        invalid = states; invalid[1].mRecord = 12;
        EXPECT_THROW(actor.restoreNativePackedVelocities(invalid), std::invalid_argument);
        EXPECT_THROW(actor.restoreNativePackedVelocities(std::span<const NifBullet::RagdollNativePackedVelocityState>(states).first(1)), std::invalid_argument);
        actual = actor.captureNativePackedVelocities(); EXPECT_EQ(actual[0].mVelocities.mLinear[0], 1.f);
        auto near = preparedWorldScene(12); near.mInput.mTargetPosition = {}; near.mInput.mTargetRotation = {0, 0, 0, 1}; near.mInput.mFrameSeconds = 0;
        const std::array requests{near};
        EXPECT_EQ(actor.synchronizeNativeWorldScenes(requests).size(), 1u);
        actual = actor.captureNativePackedVelocities();
        EXPECT_EQ(actual[0].mVelocities.mLinear, (std::array<float, 4>{}));
        EXPECT_EQ(actual[0].mVelocities.mAngular, (std::array<float, 4>{}));
        EXPECT_EQ(actual[1].mVelocities.mAngular, states[1].mVelocities.mAngular);
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, WorldScenePackedRequestedMotionResetClearsBothFourthLanes)
    {
        auto& definition = mGraph.mBodies[0]; definition.mNodeRecord = 8;
        definition.mBlend = NifBullet::RagdollBlendDefinition{30, 0, 0.f, 0.f};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        auto state = actor.captureNativePackedVelocities();
        state[0].mVelocities = {{1, 2, 3, 4}, {5, 6, 7, 8}};
        actor.restoreNativePackedVelocities(state);
        const std::array<NifBullet::RagdollNativeBlendUpdate, 1> updates{{{12, osg::Matrixf::identity(), 0.f, 0.f, 0}}};
        actor.updateNativeBlends(updates, .016f, 0, 0.f);
        state = actor.captureNativePackedVelocities();
        EXPECT_EQ(state[0].mVelocities.mLinear, (std::array<float, 4>{}));
        EXPECT_EQ(state[0].mVelocities.mAngular, (std::array<float, 4>{}));
    }
}

namespace
{
    TEST(RagdollNativePackedKeyframedStep, CapsFourthLanesUsingOriginalXYZFactors)
    {
        const NifBullet::RagdollNativeVelocities velocity{{1, 2, 3}, {1000, 2000, -3000}};
        const auto result = NifBullet::ragdollNativeKeyframedMotionStep(
            {4, 5, 6}, {0, 0, 0, 1}, {1, 0, 0}, velocity, .2f, 1.f, 1.f, 8.f, 8.f);
        // Original packed corpus case8468, captured before implementation.
        EXPECT_EQ(result.mLinearW, 2.138089895248413f);
        EXPECT_NEAR(result.mAngularW, .006717008072882891f, .001f);
        EXPECT_EQ(result.mVelocities.mLinear.x(), .26726123690605164f);
        EXPECT_NEAR(result.mVelocities.mAngular.z(), -2.5188779830932617f, .001f);
    }

    TEST(RagdollNativePackedKeyframedStep, PreservesUncappedSignedZeroAndRejectsUsedInvalidLanes)
    {
        const NifBullet::RagdollNativeVelocities velocity{};
        const auto result = NifBullet::ragdollNativeKeyframedMotionStep(
            {}, {0, 0, 0, 1}, {}, velocity, 0.f, 250.f, 31.4159f, -0.f, -8.f);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(result.mLinearW), 0x80000000u);
        EXPECT_EQ(result.mAngularW, -8.f);
        EXPECT_THROW(NifBullet::ragdollNativeKeyframedMotionStep(
            {}, {0, 0, 0, 1}, {}, velocity, 0.f, 250.f, 31.4159f,
            std::numeric_limits<float>::quiet_NaN(), 0.f), std::invalid_argument);
        EXPECT_THROW(NifBullet::ragdollNativeKeyframedMotionStep(
            {}, {0, 0, 0, 1}, {}, velocity, 0.f, 250.f, 31.4159f,
            0.f, std::numeric_limits<float>::infinity()), std::invalid_argument);
    }

    TEST_F(ActorRagdollPhysicsTest, PackedKeyframedSubstepPublishesCappedLanesWithPose)
    {
        mGraph.mBodies[0].mMaxLinearVelocity = 1.f;
        mGraph.mBodies[0].mMaxAngularVelocity = 1.f;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7.f, mPoses, 1, -1);
        const std::array modes{NifBullet::RagdollNativeMotionRequest{12, NifBullet::RagdollNativeMotion::Keyframed}};
        actor.setNativeMotionModes(modes);
        auto states = actor.captureNativePackedVelocities();
        states[0].mVelocities = {{1000, 2000, 3000, 8}, {1000, 2000, -3000, 8}};
        actor.restoreNativePackedVelocities(states);
        actor.stepNativeKeyframedMotion(.2f);
        const auto actual = actor.captureNativePackedVelocities()[0].mVelocities;
        // Loaded factory raises linear limit to250, angular limit remains1.
        EXPECT_NEAR(actual.mLinear[3], .5345224738121033f, .001f);
        EXPECT_NEAR(actual.mAngular[3], .006717008072882891f, .001f);
        EXPECT_NEAR(actual.mAngular[2], -2.5188779830932617f, .001f);
        EXPECT_NE(actor.capture()[0].mPose, mPoses[0]);
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, NativeWorldSceneWrapperBindingRejectsAmbiguousOwnersAndClearsOnRelease)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        int first = 0, second = 0;
        EXPECT_FALSE(world.hasNativeSceneOwner(&first)); EXPECT_FALSE(world.hasNativeSceneOwner(nullptr));
        EXPECT_THROW(world.bindNativeSceneOwner(nullptr), std::invalid_argument);
        world.bindNativeSceneOwner(&first);
        EXPECT_TRUE(world.hasNativeSceneOwner(&first)); EXPECT_FALSE(world.hasNativeSceneOwner(&second));
        EXPECT_THROW(world.bindNativeSceneOwner(&first), std::invalid_argument);
        EXPECT_THROW(world.bindNativeSceneOwner(&second), std::invalid_argument);
        world.unbindNativeSceneOwner(&second); EXPECT_TRUE(world.hasNativeSceneOwner(&first));
        world.unbindNativeSceneOwner(&first); EXPECT_FALSE(world.hasNativeSceneOwner(&first));
        world.bindNativeSceneOwner(&second); EXPECT_TRUE(world.hasNativeSceneOwner(&second));
        world.unbindNativeSceneOwner(&second);
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, PackedNativeForcePublishesFourthLaneAndRejectsLateInvalidBeforeMutation)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7.f, mPoses, 1, -1);
        auto state = actor.captureNativePackedVelocities();
        state[0].mVelocities = {{1, 2, 3, 4}, {5, 6, 7, 8}};
        actor.restoreNativePackedVelocities(state);
        const auto pose = actor.capture()[0].mPose;
        const std::array<NifBullet::RagdollNativeForceRequest, 1> force{{{12, {2, 0, 0}, .25f, 8.f}}};
        actor.applyNativeForces(force);
        state = actor.captureNativePackedVelocities();
        EXPECT_EQ(state[0].mVelocities.mLinear[3], 5.f);
        EXPECT_EQ(state[0].mVelocities.mLinear[0], 1.25f);
        EXPECT_EQ(state[0].mVelocities.mAngular[3], 8.f);
        EXPECT_EQ(actor.capture()[0].mPose, pose);
        const auto saved = state;
        const std::array<NifBullet::RagdollNativeForceRequest, 2> invalid{{
            force[0], {24, {}, .25f, std::numeric_limits<float>::quiet_NaN()}}};
        EXPECT_THROW(actor.applyNativeForces(invalid), std::invalid_argument);
        state = actor.captureNativePackedVelocities();
        EXPECT_EQ(state[0].mVelocities.mLinear, saved[0].mVelocities.mLinear);
        const std::array modes{NifBullet::RagdollNativeMotionRequest{12, NifBullet::RagdollNativeMotion::Keyframed}};
        actor.setNativeMotionModes(modes);
        auto unused = force; unused[0].mFrameSeconds = unused[0].mForceW = std::numeric_limits<float>::quiet_NaN();
        unused[0].mForce.x() = std::numeric_limits<float>::quiet_NaN();
        EXPECT_NO_THROW(actor.applyNativeForces(unused));
        EXPECT_EQ(actor.captureNativePackedVelocities()[0].mVelocities.mLinear, saved[0].mVelocities.mLinear);
    }

    TEST_F(ActorRagdollPhysicsTest, PackedVelocityControllerForceRetainsFourthSourceAndBodyLanes)
    {
        mGraph.mBodies[0].mNodeRecord = 8;
        mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, 0.f, 1.f};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7.f, mPoses, 1, -1);
        auto bodies = actor.captureNativePackedVelocities(); bodies[0].mVelocities.mLinear[3] = 4.f;
        actor.restoreNativePackedVelocities(bodies);
        const std::array<NifBullet::RagdollNativeVelocitySetupRequest, 1> setup{{{8, {0, 0, 0, 8}, 1.f}}};
        actor.prepareNativeVelocityControllers(setup);
        auto controllers = actor.captureNativeVelocityControllers(); controllers[0].mState.mForceVector = {0, 0, 0, 8};
        actor.restoreNativeVelocityControllers(controllers);
        const std::array order{NifBullet::RagdollNativeControllerReference{NifBullet::RagdollNativeControllerKind::Velocity, 8}};
        ESM4::PhysicalBlendTimeCache cache;
        actor.advanceNativePhysicalControllers(order, 10.f, cache);
        // Original force case4483: frame .016, inverse mass .5, forceW800,
        // currentW4. Controller's original force producer multiplies W by100.
        EXPECT_EQ(actor.captureNativePackedVelocities()[0].mVelocities.mLinear[3], 10.399999618530273f);
        EXPECT_EQ(actor.captureNativeVelocityControllers()[0].mState.mFrameDelta, .016f);
    }
}

namespace
{
    TEST_F(ActorRagdollPhysicsTest, PackedVelocityControllerForcesAccumulateSequentiallyOnSharedTarget)
    {
        mGraph.mBodies[0].mNodeRecord = 8;
        mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, 0.f, 1.f};
        addHinge(); mGraph.mBodies[1].mNodeRecord = 16; mGraph.mBodies[1].mBlend->mRecord = 31;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7.f, mPoses, 1, -1);
        auto bodies = actor.captureNativePackedVelocities(); bodies[0].mVelocities.mLinear[3] = 4.f;
        actor.restoreNativePackedVelocities(bodies);
        const std::array<NifBullet::RagdollNativeVelocitySetupRequest, 2> setup{{
            {8, {0, 0, 0, 8}, 1.f}, {16, {0, 0, 0, 8}, 1.f}}};
        actor.prepareNativeVelocityControllers(setup);
        auto controllers = actor.captureNativeVelocityControllers();
        for (auto& controller : controllers)
        {
            controller.mTargetNode = 8;
            controller.mState.mForceVector = {0, 0, 0, 8};
        }
        actor.restoreNativeVelocityControllers(controllers);
        const std::array<NifBullet::RagdollNativeControllerReference, 2> order{{
            {NifBullet::RagdollNativeControllerKind::Velocity, 8},
            {NifBullet::RagdollNativeControllerKind::Velocity, 16}}};
        ESM4::PhysicalBlendTimeCache cache;
        actor.advanceNativePhysicalControllers(order, 10.f, cache);
        bodies = actor.captureNativePackedVelocities();
        // Original sequential force case48, second call: preserve the first
        // rounded update as the next current value, not the original snapshot.
        EXPECT_EQ(bodies[0].mVelocities.mLinear[3], 16.799999237060547f);
        EXPECT_EQ(bodies[1].mVelocities.mLinear[3], 0.f);
        EXPECT_EQ(actor.capture()[0].mPose, mPoses[0]);
    }
}

namespace
{
    TEST(RagdollNativePackedVelocityStep, MatchesOriginalDampingAndCapsAcrossAllLanes)
    {
        const ESM4::PhysicalWorldSceneVelocities input{{1000, -2000, 3000, 8}, {1000, -2000, 3000, 8}};
        const NifBullet::RagdollMotionLimits limits{2, 2, 250, 31.4159f};
        const auto result = NifBullet::ragdollNativePackedVelocityStep(
            input, limits, .016f, {0, 0, -1.1772000789642334f, 8});
        // Original sphere/box packed corpus case20122, captured independently.
        EXPECT_EQ(result.mLinear[0], 66.83216857910156f);
        EXPECT_EQ(result.mLinear[3], 1.06931471824646f);
        EXPECT_EQ(result.mAngular[0], 26.377605438232422f);
        EXPECT_EQ(result.mAngular[3], .2110208421945572f);
    }

    TEST(RagdollNativePackedVelocityStep, RejectsUsedInvalidPackedInputAndDelta)
    {
        ESM4::PhysicalWorldSceneVelocities input{};
        const NifBullet::RagdollMotionLimits limits{2, 2, 250, 31.4159f};
        input.mAngular[3] = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(NifBullet::ragdollNativePackedVelocityStep(input, limits, .016f, {}), std::invalid_argument);
        input.mAngular[3] = 0;
        EXPECT_THROW(NifBullet::ragdollNativePackedVelocityStep(input, limits, .016f,
            {0, 0, 0, std::numeric_limits<float>::infinity()}), std::invalid_argument);
    }

    TEST_F(ActorRagdollPhysicsTest, PackedDynamicStepStagesAllLanesAndSkipsInactiveDeltas)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7.f, mPoses, 1, -1);
        const std::array key{NifBullet::RagdollNativeMotionRequest{24, NifBullet::RagdollNativeMotion::Keyframed}};
        actor.setNativeMotionModes(key);
        auto states = actor.captureNativePackedVelocities();
        states[0].mVelocities = {{1, 2, 3, 4}, {5, 6, 7, 8}};
        states[1].mVelocities = {{9, 10, 11, 12}, {13, 14, 15, 16}};
        actor.restoreNativePackedVelocities(states);
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const std::array<std::array<float, 4>, 2> deltas{{{0, 0, 0, 8}, {nan, nan, nan, nan}}};
        const auto pose = actor.capture()[0].mPose;
        actor.applyNativePackedVelocityStep(.5f, deltas);
        auto actual = actor.captureNativePackedVelocities();
        EXPECT_EQ(actual[0].mVelocities.mLinear, (std::array<float, 4>{}));
        EXPECT_EQ(actual[0].mVelocities.mAngular, (std::array<float, 4>{}));
        EXPECT_EQ(actual[1].mVelocities.mLinear, states[1].mVelocities.mLinear);
        EXPECT_EQ(actual[1].mVelocities.mAngular, states[1].mVelocities.mAngular);
        EXPECT_EQ(actor.capture()[0].mPose, pose);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]); first->forceActivationState(ISLAND_SLEEPING);
        const std::array<std::array<float, 4>, 2> unused{{{nan, nan, nan, nan}, {nan, nan, nan, nan}}};
        EXPECT_NO_THROW(actor.applyNativePackedVelocityStep(.016f, unused));
        EXPECT_EQ(first->getActivationState(), ISLAND_SLEEPING);
        const std::array dynamic{NifBullet::RagdollNativeMotionRequest{24, NifBullet::RagdollNativeMotion::Dynamic}};
        actor.setNativeMotionModes(dynamic);
        actor.restoreNativePackedVelocities(states); first->activate(true);
        const std::array<std::array<float, 4>, 2> invalid{{{0, 0, 0, 8}, {0, 0, 0, nan}}};
        EXPECT_THROW(actor.applyNativePackedVelocityStep(.016f, invalid), std::invalid_argument);
        EXPECT_EQ(actor.captureNativePackedVelocities()[0].mVelocities.mLinear, states[0].mVelocities.mLinear);
        EXPECT_THROW(actor.applyNativePackedVelocityStep(.016f, {}), std::invalid_argument);
        const std::array<osg::Vec3f, 2> spatialDelta{};
        actor.applyNativeVelocityStep(.5f, spatialDelta);
        actual = actor.captureNativePackedVelocities();
        EXPECT_EQ(actual[0].mVelocities.mLinear[3], 0.f);
        EXPECT_EQ(actual[0].mVelocities.mAngular[3], 0.f);
    }

    TEST_F(ActorRagdollPhysicsTest, PackedStandaloneDampingRetainsModesPoseActivationAndSignedZero)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7.f, mPoses, 1, -1);
        const std::array key{NifBullet::RagdollNativeMotionRequest{24, NifBullet::RagdollNativeMotion::Keyframed}};
        actor.setNativeMotionModes(key);
        auto states = actor.captureNativePackedVelocities();
        states[0].mVelocities = {{1, 2, 3, -8}, {5, 6, 7, -8}};
        states[1].mVelocities = {{9, 10, 11, 12}, {13, 14, 15, 16}};
        actor.restoreNativePackedVelocities(states);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]); first->forceActivationState(ISLAND_SLEEPING);
        const auto pose = actor.capture()[0].mPose;
        actor.applyNativeDamping(.25f);
        auto actual = actor.captureNativePackedVelocities();
        EXPECT_EQ(actual[0].mVelocities.mLinear[3], -4.f); EXPECT_EQ(actual[0].mVelocities.mAngular[3], -4.f);
        EXPECT_EQ(actual[1].mVelocities.mLinear, states[1].mVelocities.mLinear);
        EXPECT_EQ(actual[1].mVelocities.mAngular, states[1].mVelocities.mAngular);
        EXPECT_EQ(first->getActivationState(), ISLAND_SLEEPING); EXPECT_EQ(actor.capture()[0].mPose, pose);
        actor.applyNativeDamping(.5f); actual = actor.captureNativePackedVelocities();
        EXPECT_EQ(std::bit_cast<std::uint32_t>(actual[0].mVelocities.mLinear[3]), 0x80000000u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(actual[0].mVelocities.mAngular[3]), 0x80000000u);
    }
}


namespace
{
    TEST_F(ActorRagdollPhysicsTest, PackedPoseRestoreUsesNativeVelocitiesAndInterpolation)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, NifBullet::RagdollNativeLengthScale, mPoses, 1, -1);
        const std::array key{NifBullet::RagdollNativeMotionRequest{24, NifBullet::RagdollNativeMotion::Keyframed}};
        actor.setNativeMotionModes(key);
        const auto modes = actor.captureNativeMotionModes();
        auto spatial = actor.capture();
        auto packed = actor.captureNativePackedVelocities();
        spatial[0].mPose.setOrigin({1, 2, 3}); spatial[1].mPose.setOrigin({4, 5, 6});
        for (auto& state : spatial)
        {
            state.mLinearVelocity = {0, 0, 0}; state.mAngularVelocity = {0, 0, 0};
        }
        // Independently captured original case20122 velocities, plus signed-zero
        // and independent fourth lanes on the KEY body.
        packed[0].mVelocities = {{66.83216857910156f, -133.66433715820312f, 200.41783142089844f, 1.06931471824646f},
            {26.377605438232422f, -52.755210876464844f, 79.13282012939453f, .2110208421945572f}};
        packed[1].mVelocities = {{-0.f, 2, 3, -8}, {4, 5, 6, -0.f}};
        actor.restore(spatial, packed);
        const auto actual = actor.captureNativePackedVelocities();
        for (std::size_t body = 0; body < packed.size(); ++body)
        {
            for (unsigned axis = 0; axis < 4; ++axis)
            {
                EXPECT_EQ(std::bit_cast<std::uint32_t>(actual[body].mVelocities.mLinear[axis]),
                    std::bit_cast<std::uint32_t>(packed[body].mVelocities.mLinear[axis]));
                EXPECT_EQ(std::bit_cast<std::uint32_t>(actual[body].mVelocities.mAngular[axis]),
                    std::bit_cast<std::uint32_t>(packed[body].mVelocities.mAngular[axis]));
            }
            const auto* rigid = btRigidBody::upcast(actor.collisionObjects()[body]);
            EXPECT_EQ(rigid->getInterpolationLinearVelocity(), rigid->getLinearVelocity());
            EXPECT_EQ(rigid->getInterpolationAngularVelocity(), rigid->getAngularVelocity());
            EXPECT_EQ(actor.capture()[body].mPose, spatial[body].mPose);
        }
        EXPECT_EQ(actor.captureNativeMotionModes(), modes);
    }

    TEST_F(ActorRagdollPhysicsTest, PackedPoseRestoreRejectsEntireInvalidBatchBeforeMutation)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7.f, mPoses, 1, -1);
        auto before = actor.capture(); auto packed = actor.captureNativePackedVelocities();
        packed[0].mVelocities = {{1, 2, 3, 4}, {5, 6, 7, 8}};
        actor.restoreNativePackedVelocities(packed); before = actor.capture();
        auto next = before; next[0].mPose.setOrigin({10, 20, 30});
        auto invalid = packed; invalid[1].mVelocities.mAngular[3] = std::numeric_limits<float>::quiet_NaN();
        auto* rigid = btRigidBody::upcast(actor.collisionObjects()[0]);
        rigid->applyCentralForce({1, 2, 3}); rigid->forceActivationState(ISLAND_SLEEPING);
        const auto check = [&] {
            EXPECT_EQ(actor.capture()[0].mPose, before[0].mPose);
            EXPECT_EQ(actor.capture()[0].mLinearVelocity, before[0].mLinearVelocity);
            EXPECT_EQ(actor.captureNativePackedVelocities()[0].mVelocities.mLinear, packed[0].mVelocities.mLinear);
            EXPECT_EQ(rigid->getTotalForce(), btVector3(1, 2, 3));
            EXPECT_EQ(rigid->getActivationState(), ISLAND_SLEEPING);
        };
        EXPECT_THROW(actor.restore(next, invalid), std::invalid_argument); check();
        invalid = packed; invalid[1].mRecord = 999;
        EXPECT_THROW(actor.restore(next, invalid), std::invalid_argument); check();
        EXPECT_THROW(actor.restore(next, std::span<const NifBullet::RagdollNativePackedVelocityState>{}), std::invalid_argument); check();
        next[1].mPose.getBasis()[0][0] = 2;
        EXPECT_THROW(actor.restore(next, packed), std::invalid_argument); check();
    }
}


namespace
{
    TEST_F(ActorRagdollPhysicsTest, PackedPoseMotionRestoreChangesBothDirectionsAndKeepsIdentity)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7.f, mPoses, 1, -1);
        const std::array key{NifBullet::RagdollNativeMotionRequest{12, NifBullet::RagdollNativeMotion::Keyframed}};
        actor.setNativeMotionModes(key);
        const auto first = actor.collisionObjects()[0], second = actor.collisionObjects()[1];
        const auto firstShape = first->getCollisionShape(), secondShape = second->getCollisionShape();
        auto spatial = actor.capture(); spatial[0].mPose.setOrigin({1, 2, 3}); spatial[1].mPose.setOrigin({4, 5, 6});
        auto packed = actor.captureNativePackedVelocities();
        packed[0].mVelocities = {{1, 2, 3, -8}, {4, 5, 6, -0.f}};
        packed[1].mVelocities = {{7, 8, 9, 8}, {10, 11, 12, 4}};
        const std::array<NifBullet::RagdollNativeMotionRequest, 2> motions{{
            {12, NifBullet::RagdollNativeMotion::Dynamic}, {24, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.restore(spatial, packed, motions);
        EXPECT_EQ(actor.captureNativeMotionModes(), std::vector<NifBullet::RagdollNativeMotionRequest>(motions.begin(), motions.end()));
        EXPECT_EQ(btRigidBody::upcast(first)->getInvMass(), .5);
        EXPECT_EQ(btRigidBody::upcast(second)->getInvMass(), 0);
        EXPECT_FALSE(first->isKinematicObject()); EXPECT_TRUE(second->isKinematicObject());
        EXPECT_EQ(actor.collisionObjects()[0], first); EXPECT_EQ(actor.collisionObjects()[1], second);
        EXPECT_EQ(first->getCollisionShape(), firstShape); EXPECT_EQ(second->getCollisionShape(), secondShape);
        EXPECT_EQ(mWorld.getNumConstraints(), 1);
        const auto actual = actor.captureNativePackedVelocities();
        for (std::size_t body = 0; body < packed.size(); ++body)
        {
            EXPECT_EQ(actor.capture()[body].mPose, spatial[body].mPose);
            for (unsigned axis = 0; axis < 4; ++axis)
            {
                EXPECT_EQ(std::bit_cast<std::uint32_t>(actual[body].mVelocities.mLinear[axis]), std::bit_cast<std::uint32_t>(packed[body].mVelocities.mLinear[axis]));
                EXPECT_EQ(std::bit_cast<std::uint32_t>(actual[body].mVelocities.mAngular[axis]), std::bit_cast<std::uint32_t>(packed[body].mVelocities.mAngular[axis]));
            }
            const auto* bodyObject = btRigidBody::upcast(actor.collisionObjects()[body]);
            EXPECT_EQ(bodyObject->getInterpolationLinearVelocity(), bodyObject->getLinearVelocity());
            EXPECT_EQ(bodyObject->getInterpolationAngularVelocity(), bodyObject->getAngularVelocity());
        }
        actor.applyNativeDamping(.25f);
        const auto damped = actor.captureNativePackedVelocities();
        EXPECT_EQ(damped[0].mVelocities.mLinear[3], -4.f);
        EXPECT_EQ(damped[1].mVelocities.mLinear[3], 8.f);
    }

    TEST_F(ActorRagdollPhysicsTest, PackedPoseMotionRestoreRejectsLateInvalidWithoutAnyHandoff)
    {
        addHinge();
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7.f, mPoses, 1, -1);
        auto packed = actor.captureNativePackedVelocities(); packed[0].mVelocities.mLinear[3] = 8;
        actor.restoreNativePackedVelocities(packed);
        const auto original = actor.capture(); const auto originalModes = actor.captureNativeMotionModes();
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        first->applyCentralForce({1, 2, 3}); first->forceActivationState(ISLAND_SLEEPING);
        auto spatial = original; spatial[0].mPose.setOrigin({1, 2, 3});
        const std::array<NifBullet::RagdollNativeMotionRequest, 2> motions{{
            {12, NifBullet::RagdollNativeMotion::Keyframed}, {24, NifBullet::RagdollNativeMotion::Dynamic}}};
        const auto check = [&] {
            EXPECT_EQ(actor.capture()[0].mPose, original[0].mPose);
            EXPECT_EQ(actor.captureNativeMotionModes(), originalModes);
            EXPECT_EQ(first->getInvMass(), .5); EXPECT_FALSE(first->isKinematicObject());
            EXPECT_EQ(first->getTotalForce(), btVector3(1, 2, 3));
            EXPECT_EQ(first->getActivationState(), ISLAND_SLEEPING);
            EXPECT_EQ(actor.captureNativePackedVelocities()[0].mVelocities.mLinear[3], 8);
        };
        auto invalid = motions; invalid[1].mMotion = static_cast<NifBullet::RagdollNativeMotion>(255);
        EXPECT_THROW(actor.restore(spatial, packed, invalid), std::invalid_argument); check();
        invalid = motions; invalid[1].mRecord = 999;
        EXPECT_THROW(actor.restore(spatial, packed, invalid), std::invalid_argument); check();
        EXPECT_THROW(actor.restore(spatial, packed, std::span<const NifBullet::RagdollNativeMotionRequest>(motions.data(), 1)), std::invalid_argument); check();
        auto badPacked = packed; badPacked[1].mVelocities.mAngular[3] = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(actor.restore(spatial, badPacked, motions), std::invalid_argument); check();
        spatial[1].mPose.getBasis()[0][0] = 2;
        EXPECT_THROW(actor.restore(spatial, packed, motions), std::invalid_argument); check();
    }
}


namespace
{
    TEST_F(ActorRagdollPhysicsTest, PackedBlendRestorePreservesIndependentRequestsBeforeNextDispatch)
    {
        mGraph.mBodies[0].mNodeRecord = 8;
        mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, .2f, .8f};
        addHinge(); mGraph.mBodies[1].mNodeRecord = 16; mGraph.mBodies[1].mBlend->mRecord = 31;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7.f, mPoses, 1, -1);
        auto spatial = actor.capture(); auto packed = actor.captureNativePackedVelocities();
        packed[0].mVelocities = {{1, 2, 3, 8}, {4, 5, 6, -0.f}};
        const std::array<NifBullet::RagdollNativeMotionRequest, 2> motions{{
            {12, NifBullet::RagdollNativeMotion::Keyframed}, {24, NifBullet::RagdollNativeMotion::Dynamic}}};
        auto blends = actor.captureNativeBlendStates(); ASSERT_EQ(blends.size(), 2);
        blends[0].mCollisionFlags = 8; blends[0].mRequestedMotion = 1; blends[0].mGains = {-0.f, 0.f};
        blends[1].mCollisionFlags = 0xf123; blends[1].mRequestedMotion = std::numeric_limits<std::uint32_t>::max();
        blends[1].mGains = {-2.f, 3.f};
        actor.restore(spatial, packed, motions, blends);
        const auto actual = actor.captureNativeBlendStates();
        for (std::size_t i = 0; i < blends.size(); ++i)
        {
            EXPECT_EQ(actual[i].mCollisionFlags, blends[i].mCollisionFlags);
            EXPECT_EQ(actual[i].mRequestedMotion, blends[i].mRequestedMotion);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(actual[i].mGains.mHierarchy), std::bit_cast<std::uint32_t>(blends[i].mGains.mHierarchy));
            EXPECT_EQ(std::bit_cast<std::uint32_t>(actual[i].mGains.mVelocity), std::bit_cast<std::uint32_t>(blends[i].mGains.mVelocity));
        }
        // Original216 full caller: matching stored request1 skips conversion,
        // even when actual mode is KEY. A fresh constructor request8 would
        // instead hand off and clear packed velocity lanes on this update.
        const std::array updates{NifBullet::RagdollNativeBlendUpdate{12, osg::Matrixf::identity(), 0, 0, 8}};
        actor.updateNativeBlends(updates, .016f, 0, 0);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
        EXPECT_EQ(actor.captureNativePackedVelocities()[0].mVelocities.mLinear[3], 8);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(actor.captureNativePackedVelocities()[0].mVelocities.mAngular[3]), 0x80000000u);
    }

    TEST_F(ActorRagdollPhysicsTest, PackedBlendRestoreRejectsLateMetadataBeforePhysicalPublication)
    {
        mGraph.mBodies[0].mNodeRecord = 8;
        mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, .2f, .8f};
        addHinge(); mGraph.mBodies[1].mNodeRecord = 16; mGraph.mBodies[1].mBlend->mRecord = 31;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 7.f, mPoses, 1, -1);
        const auto original = actor.capture(); const auto originalModes = actor.captureNativeMotionModes();
        auto packed = actor.captureNativePackedVelocities(); packed[0].mVelocities.mLinear[3] = 8;
        auto spatial = original; spatial[0].mPose.setOrigin({1, 2, 3});
        const std::array<NifBullet::RagdollNativeMotionRequest, 2> motions{{
            {12, NifBullet::RagdollNativeMotion::Keyframed}, {24, NifBullet::RagdollNativeMotion::Dynamic}}};
        const auto blends = actor.captureNativeBlendStates();
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        first->applyCentralForce({1, 2, 3}); first->forceActivationState(ISLAND_SLEEPING);
        const auto check = [&] {
            EXPECT_EQ(actor.capture()[0].mPose, original[0].mPose);
            EXPECT_EQ(actor.captureNativeMotionModes(), originalModes);
            EXPECT_EQ(first->getTotalForce(), btVector3(1, 2, 3));
            EXPECT_EQ(first->getActivationState(), ISLAND_SLEEPING);
            const auto actual = actor.captureNativeBlendStates();
            EXPECT_EQ(actual[0].mRequestedMotion, blends[0].mRequestedMotion);
            EXPECT_EQ(actual[0].mGains.mHierarchy, blends[0].mGains.mHierarchy);
        };
        auto invalid = blends; invalid[1].mGains.mVelocity = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(actor.restore(spatial, packed, motions, invalid), std::invalid_argument); check();
        invalid = blends; invalid[1].mBodyRecord = 999;
        EXPECT_THROW(actor.restore(spatial, packed, motions, invalid), std::invalid_argument); check();
        invalid = blends; invalid[1].mBodyRecord = invalid[0].mBodyRecord;
        EXPECT_THROW(actor.restore(spatial, packed, motions, invalid), std::invalid_argument); check();
        EXPECT_THROW(actor.restore(spatial, packed, motions, std::span<const NifBullet::RagdollNativeBlendState>{}), std::invalid_argument); check();
    }
}


namespace
{
    std::vector<std::uint32_t> savedBlendControllerWords(const NifBullet::RagdollNativeBlendControllerState& controller)
    {
        const auto& state = controller.mState; const auto& t = state.mTiming; const auto& c = state.mClock;
        const auto bits = [](float value) { return std::bit_cast<std::uint32_t>(value); };
        std::vector<std::uint32_t> result{controller.mRecord, controller.mTargetNode.has_value(),
            controller.mTargetNode.value_or(0), controller.mAttachedNode, t.mFlags,
            bits(t.mFrequency), bits(t.mPhase), bits(t.mStartKey), bits(t.mStopKey),
            bits(c.mStartTime), bits(c.mPreviousTime), bits(c.mElapsed), state.mCursor,
            bits(state.mCachedGains.mHierarchy), bits(state.mCachedGains.mVelocity), state.mSetupState,
            static_cast<std::uint32_t>(state.mKeys.size())};
        for (const auto& key : state.mKeys)
            for (float value : {key.mTime, key.mGains.mHierarchy, key.mGains.mVelocity}) result.push_back(bits(value));
        return result;
    }

    std::vector<std::uint32_t> savedVelocityControllerWords(const NifBullet::RagdollNativeVelocityControllerState& controller)
    {
        const auto& state = controller.mState; const auto& t = state.mTiming; const auto& c = state.mClock;
        const auto bits = [](float value) { return std::bit_cast<std::uint32_t>(value); };
        std::vector<std::uint32_t> result{controller.mAttachedNode, controller.mTargetNode.has_value(),
            controller.mTargetNode.value_or(0), controller.mPrecedesBlend, t.mFlags,
            bits(t.mFrequency), bits(t.mPhase), bits(t.mStartKey), bits(t.mStopKey),
            bits(c.mStartTime), bits(c.mPreviousTime), bits(c.mElapsed), bits(state.mFrameDelta)};
        for (float value : state.mForceVector) result.push_back(bits(value));
        return result;
    }

    struct CompleteControllerRestoreTest : ActorRagdollPhysicsTest
    {
        CompleteControllerRestoreTest()
        {
            mGraph.mBodies[0].mNodeRecord = 8;
            mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, .5f, .5f};
            mGraph.mBodies[0].mBlendController = NifBullet::RagdollBlendControllerDefinition{
                78, 8, 0xd, 1, 0, 0, 4, {}};
            addHinge(); mGraph.mBodies[1].mNodeRecord = 20;
            mGraph.mBodies[1].mBlend->mRecord = 31;
            mGraph.mBodies[1].mBlendController->mRecord = 79;
            mGraph.mBodies[1].mBlendController->mTargetRecord = 20;
        }
        std::vector<NifBullet::RagdollNativeBlendControllerState> savedBlendControllers(
            const NifBullet::ActorRagdollPhysics& actor)
        {
            auto controllers = actor.captureNativeBlendControllers(); auto& state = controllers[0].mState;
            state.mTiming = {0xd, 1, -0.f, 0, 4}; state.mClock = {10, 11, 1};
            state.mKeys = {{0, {1, 0}}, {1, {.5f, .5f}}, {1, {.25f, .75f}}, {4, {0, 1}}};
            state.mCursor = 1; state.mCachedGains = {-0.f, -2}; state.mSetupState = 0xffffffffu;
            controllers[1].mTargetNode.reset(); controllers[1].mState.mCursor = 0xffffffffu;
            // Finite reversed raw bounds on a missing-target controller are
            // retained; no active clock is advanced for that unused target.
            controllers[1].mState.mTiming = {0xffff, -1, -0.f, 1, -1};
            return controllers;
        }
        std::vector<NifBullet::RagdollNativeVelocityControllerState> savedVelocityControllers()
        {
            ESM4::PhysicalVelocityControllerState state;
            state.mTiming = {0xd, 1, -0.f, 0, 4}; state.mClock = {10, 11, 1};
            state.mForceVector = {1, 2, 3, 8}; state.mFrameDelta = .016f;
            auto missing = state; missing.mTiming.mFlags = 0; missing.mClock = {};
            missing.mClock.mElapsed = -0.f; missing.mForceVector = {-0.f, 0, 0, -0.f}; missing.mFrameDelta = 0;
            return {{8, 20, state, false}, {20, std::nullopt, missing, true}};
        }
    };

    TEST_F(CompleteControllerRestoreTest, RestoresControllerFieldsAndPerNodeListPositionWithBodyState)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto spatial = actor.capture(); spatial[0].mPose.setOrigin({1, 2, 3});
        auto packed = actor.captureNativePackedVelocities(); packed[0].mVelocities = {{1, 2, 3, 8}, {4, 5, 6, -0.f}};
        auto modes = actor.captureNativeMotionModes(); modes[0].mMotion = NifBullet::RagdollNativeMotion::Keyframed;
        const auto blends = actor.captureNativeBlendStates(); const auto controls = savedBlendControllers(actor);
        const auto velocities = savedVelocityControllers();
        auto reversed = controls; std::reverse(reversed.begin(), reversed.end());
        actor.restore(spatial, packed, modes, blends, reversed, velocities);
        const auto actual = actor.captureNativeBlendControllers(); ASSERT_EQ(actual.size(), controls.size());
        for (std::size_t i = 0; i < controls.size(); ++i)
            EXPECT_EQ(savedBlendControllerWords(actual[i]), savedBlendControllerWords(controls[i]));
        const auto actualVelocity = actor.captureNativeVelocityControllers(); ASSERT_EQ(actualVelocity.size(), velocities.size());
        for (std::size_t i = 0; i < velocities.size(); ++i)
            EXPECT_EQ(savedVelocityControllerWords(actualVelocity[i]), savedVelocityControllerWords(velocities[i]));
        const std::array<std::uint32_t, 2> nodes{8, 20};
        const std::vector<NifBullet::RagdollNativeControllerReference> order{
            {NifBullet::RagdollNativeControllerKind::Blend, 78}, {NifBullet::RagdollNativeControllerKind::Velocity, 8},
            {NifBullet::RagdollNativeControllerKind::Velocity, 20}, {NifBullet::RagdollNativeControllerKind::Blend, 79}};
        EXPECT_EQ(actor.captureNativeControllerOrder(nodes), order);
        EXPECT_EQ(actor.captureNativeMotionModes(), modes); EXPECT_EQ(actor.capture()[0].mPose, spatial[0].mPose);
        EXPECT_EQ(actor.captureNativePackedVelocities()[0].mVelocities.mLinear[3], 8);
    }

    TEST_F(CompleteControllerRestoreTest, RejectsLateControllerDataBeforePhysicalOrMetadataPublication)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        const auto original = actor.capture(); const auto originalModes = actor.captureNativeMotionModes();
        const auto originalControls = actor.captureNativeBlendControllers(); const auto blends = actor.captureNativeBlendStates();
        auto spatial = original; spatial[0].mPose.setOrigin({1, 2, 3});
        auto packed = actor.captureNativePackedVelocities(); packed[0].mVelocities.mLinear[3] = 8;
        auto modes = originalModes; modes[0].mMotion = NifBullet::RagdollNativeMotion::Keyframed;
        const auto controls = savedBlendControllers(actor); const auto velocities = savedVelocityControllers();
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]); body->applyCentralForce({1, 2, 3});
        body->forceActivationState(ISLAND_SLEEPING);
        const auto check = [&] {
            EXPECT_EQ(actor.capture()[0].mPose, original[0].mPose); EXPECT_EQ(actor.captureNativeMotionModes(), originalModes);
            EXPECT_EQ(body->getTotalForce(), btVector3(1, 2, 3)); EXPECT_EQ(body->getActivationState(), ISLAND_SLEEPING);
            EXPECT_TRUE(actor.captureNativeVelocityControllers().empty());
            const auto actual = actor.captureNativeBlendControllers(); ASSERT_EQ(actual.size(), originalControls.size());
            for (std::size_t i = 0; i < actual.size(); ++i)
                EXPECT_EQ(savedBlendControllerWords(actual[i]), savedBlendControllerWords(originalControls[i]));
            EXPECT_EQ(actor.captureNativeBlendStates()[0].mRequestedMotion, blends[0].mRequestedMotion);
        };
        for (unsigned field = 0; field < 11; ++field)
        {
            auto invalid = controls; auto& late = invalid[1];
            switch (field)
            {
                case 0: invalid.pop_back(); break;
                case 1: late.mRecord = invalid[0].mRecord; break;
                case 2: late.mRecord = 999; break;
                case 3: late.mAttachedNode = 8; break;
                case 4: late.mTargetNode = 999; break;
                case 5: late.mState.mTiming.mFrequency = std::numeric_limits<float>::infinity(); break;
                case 6: late.mState.mClock.mElapsed = std::numeric_limits<float>::quiet_NaN(); break;
                case 7: late.mState.mCachedGains.mVelocity = std::numeric_limits<float>::quiet_NaN(); break;
                case 8: late.mState.mKeys = {{2, {0, 0}}, {1, {0, 0}}}; late.mState.mCursor = 0; break;
                case 9: late.mState.mKeys = {{1, {0, 0}}, {2, {0, std::numeric_limits<float>::infinity()}}}; late.mState.mCursor = 0; break;
                case 10: late.mState.mKeys = {{1, {0, 0}}, {2, {0, 0}}}; late.mState.mCursor = 1; break;
            }
            SCOPED_TRACE(field); EXPECT_THROW(actor.restore(spatial, packed, modes, blends, invalid, velocities), std::invalid_argument); check();
        }
        for (unsigned field = 0; field < 5; ++field)
        {
            auto invalid = velocities;
            switch (field)
            {
                case 0: invalid[1].mAttachedNode = 8; break;
                case 1: invalid[1].mTargetNode = 999; break;
                case 2: invalid[1].mState.mClock.mElapsed = std::numeric_limits<float>::quiet_NaN(); break;
                case 3: invalid[1].mState.mForceVector[3] = std::numeric_limits<float>::infinity(); break;
                case 4: invalid[1].mState.mFrameDelta = -1; break;
            }
            SCOPED_TRACE(field); EXPECT_THROW(actor.restore(spatial, packed, modes, blends, controls, invalid), std::invalid_argument); check();
        }
    }

    TEST_F(CompleteControllerRestoreTest, FreshOwnerContinuesSavedControllerClocksCursorAndPackedForces)
    {
        NifBullet::ActorRagdollPhysics original(mGraph, mWorld, 1, mPoses, 1, -1);
        const auto spatial = original.capture(); const auto packed = original.captureNativePackedVelocities();
        const auto modes = original.captureNativeMotionModes(); const auto blends = original.captureNativeBlendStates();
        const auto controls = savedBlendControllers(original); const auto velocities = savedVelocityControllers();
        original.restore(spatial, packed, modes, blends, controls, velocities);
        NifBullet::ActorRagdollPhysics resumed(mGraph, mWorld, 1, mPoses, 1, -1);
        resumed.restore(spatial, packed, modes, blends, controls, velocities);
        const std::array<std::uint32_t, 2> nodes{8, 20};
        const auto order = original.captureNativeControllerOrder(nodes);
        ESM4::PhysicalBlendTimeCache originalCache, resumedCache;
        for (unsigned frame = 0; frame < 60; ++frame)
        {
            const float time = 12.f + float(frame) * .016f;
            original.advanceNativePhysicalControllers(order, time, originalCache);
            resumed.advanceNativePhysicalControllers(order, time, resumedCache);
            const auto expected = original.captureNativeBlendControllers(); const auto actual = resumed.captureNativeBlendControllers();
            ASSERT_EQ(actual.size(), expected.size());
            for (std::size_t i = 0; i < actual.size(); ++i)
                EXPECT_EQ(savedBlendControllerWords(actual[i]), savedBlendControllerWords(expected[i]));
            const auto expectedVelocity = original.captureNativeVelocityControllers(); const auto actualVelocity = resumed.captureNativeVelocityControllers();
            ASSERT_EQ(actualVelocity.size(), expectedVelocity.size());
            for (std::size_t i = 0; i < actualVelocity.size(); ++i)
                EXPECT_EQ(savedVelocityControllerWords(actualVelocity[i]), savedVelocityControllerWords(expectedVelocity[i]));
            const auto expectedPacked = original.captureNativePackedVelocities(); const auto actualPacked = resumed.captureNativePackedVelocities();
            for (std::size_t i = 0; i < actualPacked.size(); ++i)
                for (unsigned lane = 0; lane < 4; ++lane)
                {
                    EXPECT_EQ(std::bit_cast<std::uint32_t>(actualPacked[i].mVelocities.mLinear[lane]), std::bit_cast<std::uint32_t>(expectedPacked[i].mVelocities.mLinear[lane]));
                    EXPECT_EQ(std::bit_cast<std::uint32_t>(actualPacked[i].mVelocities.mAngular[lane]), std::bit_cast<std::uint32_t>(expectedPacked[i].mVelocities.mAngular[lane]));
                }
            if (frame == 0)
            {
                EXPECT_EQ(actual[0].mState.mClock.mElapsed, 2.f); EXPECT_EQ(actual[0].mState.mCursor, 2u);
                EXPECT_EQ(actual[0].mState.mClock.mPreviousTime, 12.f);
                EXPECT_EQ(actualVelocity.size(), 2u);
            }
        }
    }
}


namespace
{
    TEST_F(CompleteControllerRestoreTest, PreparedRestoreStagesOwnedBuffersUntilExplicitCommit)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto spatial = actor.capture(); const auto before = spatial;
        spatial[0].mPose.setOrigin({10, 20, 30});
        auto packed = actor.captureNativePackedVelocities();
        packed[0].mVelocities = {{1, 2, 3, 8}, {4, 5, 6, -0.f}};
        auto motions = actor.captureNativeMotionModes();
        motions[0].mMotion = NifBullet::RagdollNativeMotion::Keyframed;
        auto blends = actor.captureNativeBlendStates(); blends[0].mRequestedMotion = 0xffffffffu;
        const auto controls = savedBlendControllers(actor); const auto velocities = savedVelocityControllers();
        const auto initialControls = actor.captureNativeBlendControllers();
        auto pending = actor.prepareRestore(spatial, packed, motions, blends, controls, velocities);
        ASSERT_TRUE(pending);
        EXPECT_EQ(actor.capture()[0].mPose, before[0].mPose);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        EXPECT_EQ(savedBlendControllerWords(actor.captureNativeBlendControllers()[0]), savedBlendControllerWords(initialControls[0]));
        // The token must own its data; caller buffers can change before commit.
        spatial[0].mPose.setOrigin({99, 99, 99}); packed[0].mVelocities = {};
        motions.clear(); blends.clear();
        actor.commitRestore(*pending);
        EXPECT_EQ(actor.capture()[0].mPose.getOrigin(), btVector3(10, 20, 30));
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
        EXPECT_EQ(actor.captureNativeBlendStates()[0].mRequestedMotion, 0xffffffffu);
        EXPECT_EQ(actor.captureNativePackedVelocities()[0].mVelocities.mLinear, (std::array<float, 4>{1, 2, 3, 8}));
        EXPECT_EQ(std::bit_cast<std::uint32_t>(actor.captureNativePackedVelocities()[0].mVelocities.mAngular[3]), 0x80000000u);
        EXPECT_EQ(savedBlendControllerWords(actor.captureNativeBlendControllers()[0]), savedBlendControllerWords(controls[0]));
        ASSERT_EQ(actor.captureNativeVelocityControllers().size(), velocities.size());
        EXPECT_EQ(savedVelocityControllerWords(actor.captureNativeVelocityControllers()[0]), savedVelocityControllerWords(velocities[0]));
    }

    TEST_F(CompleteControllerRestoreTest, PreparedRestoreRejectsLateSecondOwnerBeforeEitherPublishes)
    {
        NifBullet::ActorRagdollPhysics first(mGraph, mWorld, 1, mPoses, 1, -1);
        NifBullet::ActorRagdollPhysics second(mGraph, mWorld, 1, mPoses, 1, -1);
        auto spatial = first.capture(); spatial[0].mPose.setOrigin({10, 20, 30});
        const auto packed = first.captureNativePackedVelocities(); const auto motions = first.captureNativeMotionModes();
        const auto blends = first.captureNativeBlendStates(); const auto controls = savedBlendControllers(first);
        const auto velocities = savedVelocityControllers();
        auto firstReady = first.prepareRestore(spatial, packed, motions, blends, controls, velocities);
        ASSERT_TRUE(firstReady);
        const auto beforeFirst = first.capture(); const auto beforeSecond = second.capture();
        for (unsigned field = 0; field < 6; ++field)
        {
            auto badSpatial = spatial; auto badPacked = packed; auto badModes = motions;
            auto badBlends = blends; auto badControls = controls; auto badVelocities = velocities;
            switch (field)
            {
                case 0: badSpatial.back().mPose.getOrigin().setX(std::numeric_limits<btScalar>::infinity()); break;
                case 1: badPacked.back().mVelocities.mAngular[3] = std::numeric_limits<float>::quiet_NaN(); break;
                case 2: badModes.back().mMotion = static_cast<NifBullet::RagdollNativeMotion>(2); break;
                case 3: badBlends.back().mGains.mVelocity = std::numeric_limits<float>::infinity(); break;
                case 4: badControls.back().mAttachedNode = 8; break;
                case 5: badVelocities.back().mState.mFrameDelta = -1; break;
            }
            EXPECT_THROW(second.prepareRestore(badSpatial, badPacked, badModes, badBlends, badControls, badVelocities), std::invalid_argument);
            EXPECT_EQ(first.capture()[0].mPose, beforeFirst[0].mPose);
            EXPECT_EQ(second.capture()[0].mPose, beforeSecond[0].mPose);
        }
        auto secondReady = second.prepareRestore(spatial, packed, motions, blends, controls, velocities);
        ASSERT_TRUE(secondReady); first.commitRestore(*firstReady); second.commitRestore(*secondReady);
        EXPECT_EQ(first.capture()[0].mPose.getOrigin(), btVector3(10, 20, 30));
        EXPECT_EQ(second.capture()[0].mPose.getOrigin(), btVector3(10, 20, 30));
    }

    TEST_F(CompleteControllerRestoreTest, PreparedRestoreRejectsWrongOwnerAndConsumedToken)
    {
        NifBullet::ActorRagdollPhysics first(mGraph, mWorld, 1, mPoses, 1, -1);
        NifBullet::ActorRagdollPhysics second(mGraph, mWorld, 1, mPoses, 1, -1);
        auto spatial = first.capture(); spatial[0].mPose.setOrigin({10, 20, 30});
        const auto packed = first.captureNativePackedVelocities(); const auto motions = first.captureNativeMotionModes();
        const auto blends = first.captureNativeBlendStates(); const auto controls = savedBlendControllers(first);
        const auto velocities = savedVelocityControllers();
        auto pending = first.prepareRestore(spatial, packed, motions, blends, controls, velocities);
        ASSERT_TRUE(pending);
        const auto beforeSecond = second.capture();
        EXPECT_THROW(second.commitRestore(*pending), std::invalid_argument);
        EXPECT_EQ(second.capture()[0].mPose, beforeSecond[0].mPose);
        first.commitRestore(*pending);
        EXPECT_EQ(first.capture()[0].mPose.getOrigin(), btVector3(10, 20, 30));
        const auto committed = first.capture();
        EXPECT_THROW(first.commitRestore(*pending), std::invalid_argument);
        EXPECT_EQ(first.capture()[0].mPose, committed[0].mPose);
    }
}


namespace
{
    TEST_F(CompleteControllerRestoreTest, PreparedLegacySpatialKeepsNativeWModeAndControllers)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 2, mPoses, 1, -1);
        auto packed = actor.captureNativePackedVelocities(); packed[0].mVelocities = {{1, 2, 3, 8}, {4, 5, 6, -0.f}};
        auto modes = actor.captureNativeMotionModes(); modes[0].mMotion = NifBullet::RagdollNativeMotion::Keyframed;
        auto blends = actor.captureNativeBlendStates(); blends[0].mRequestedMotion = 0xffffffffu;
        const auto controls = savedBlendControllers(actor); const auto velocities = savedVelocityControllers();
        actor.restore(actor.capture(), packed, modes, blends, controls, velocities);
        auto spatial = actor.capture(); const auto original = spatial;
        spatial[0].mPose.setOrigin({10, 20, 30}); spatial[0].mLinearVelocity = {7, 8, 9};
        auto prepared = actor.prepareRestore(spatial); ASSERT_TRUE(prepared);
        EXPECT_EQ(actor.capture()[0].mPose, original[0].mPose);
        actor.commitRestore(*prepared);
        EXPECT_EQ(actor.capture()[0].mPose.getOrigin(), btVector3(10, 20, 30));
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, btVector3(7, 8, 9));
        const auto native = actor.captureNativePackedVelocities()[0].mVelocities;
        EXPECT_EQ(native.mLinear, (std::array<float, 4>{3.5f, 4, 4.5f, 8}));
        EXPECT_EQ(std::bit_cast<std::uint32_t>(native.mAngular[3]), 0x80000000u);
        EXPECT_EQ(actor.captureNativeMotionModes(), modes);
        EXPECT_EQ(actor.captureNativeBlendStates()[0].mRequestedMotion, 0xffffffffu);
        EXPECT_EQ(savedBlendControllerWords(actor.captureNativeBlendControllers()[0]), savedBlendControllerWords(controls[0]));
        ASSERT_EQ(actor.captureNativeVelocityControllers().size(), velocities.size());
        EXPECT_EQ(savedVelocityControllerWords(actor.captureNativeVelocityControllers()[0]), savedVelocityControllerWords(velocities[0]));
    }

    TEST_F(CompleteControllerRestoreTest, PreparedLegacyPackedModesAndBlendKeepAbsentControllers)
    {
        for (unsigned kind = 0; kind < 3; ++kind)
        {
            NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
            const auto controls = savedBlendControllers(actor); const auto velocities = savedVelocityControllers();
            actor.restore(actor.capture(), actor.captureNativePackedVelocities(), actor.captureNativeMotionModes(),
                actor.captureNativeBlendStates(), controls, velocities);
            auto spatial = actor.capture(); spatial[0].mPose.setOrigin({10, 20, 30});
            auto packed = actor.captureNativePackedVelocities(); packed[0].mVelocities = {{1, 2, 3, 8}, {4, 5, 6, -0.f}};
            auto modes = actor.captureNativeMotionModes(); modes[0].mMotion = NifBullet::RagdollNativeMotion::Keyframed;
            auto blends = actor.captureNativeBlendStates(); blends[0].mRequestedMotion = 0xffffffffu;
            std::unique_ptr<NifBullet::ActorRagdollPhysics::PreparedRestore> prepared;
            if (kind == 0) prepared = actor.prepareRestore(spatial, packed);
            if (kind == 1) prepared = actor.prepareRestore(spatial, packed, modes);
            if (kind == 2) prepared = actor.prepareRestore(spatial, packed, modes, blends);
            ASSERT_TRUE(prepared); EXPECT_NE(actor.capture()[0].mPose.getOrigin(), btVector3(10, 20, 30));
            actor.commitRestore(*prepared);
            EXPECT_EQ(actor.capture()[0].mPose.getOrigin(), btVector3(10, 20, 30));
            EXPECT_EQ(actor.captureNativePackedVelocities()[0].mVelocities.mLinear, packed[0].mVelocities.mLinear);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(actor.captureNativePackedVelocities()[0].mVelocities.mAngular[3]), 0x80000000u);
            EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, kind ? NifBullet::RagdollNativeMotion::Keyframed : NifBullet::RagdollNativeMotion::Dynamic);
            EXPECT_EQ(actor.captureNativeBlendStates()[0].mRequestedMotion, kind == 2 ? 0xffffffffu : 8u);
            EXPECT_EQ(savedBlendControllerWords(actor.captureNativeBlendControllers()[0]), savedBlendControllerWords(controls[0]));
            ASSERT_EQ(actor.captureNativeVelocityControllers().size(), velocities.size());
            EXPECT_EQ(savedVelocityControllerWords(actor.captureNativeVelocityControllers()[0]), savedVelocityControllerWords(velocities[0]));
        }
    }

    TEST_F(CompleteControllerRestoreTest, PreparedLegacyLateInvalidProjectionNeverPublishes)
    {
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        const auto before = actor.capture(); auto spatial = before; spatial[0].mPose.setOrigin({10, 20, 30});
        auto packed = actor.captureNativePackedVelocities(); auto modes = actor.captureNativeMotionModes();
        auto blends = actor.captureNativeBlendStates();
        // Valid preparation itself must work before testing malformed variants.
        auto valid = actor.prepareRestore(spatial); ASSERT_TRUE(valid);
        auto badSpatial = spatial; badSpatial.back().mPose.getOrigin().setX(std::numeric_limits<btScalar>::infinity());
        EXPECT_THROW(actor.prepareRestore(badSpatial), std::invalid_argument);
        auto badPacked = packed; badPacked.back().mVelocities.mLinear[3] = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(actor.prepareRestore(spatial, badPacked), std::invalid_argument);
        auto badModes = modes; badModes.back().mMotion = static_cast<NifBullet::RagdollNativeMotion>(2);
        EXPECT_THROW(actor.prepareRestore(spatial, packed, badModes), std::invalid_argument);
        auto badBlends = blends; badBlends.back().mGains.mHierarchy = std::numeric_limits<float>::infinity();
        EXPECT_THROW(actor.prepareRestore(spatial, packed, modes, badBlends), std::invalid_argument);
        EXPECT_EQ(actor.capture()[0].mPose, before[0].mPose);
        EXPECT_EQ(actor.captureNativePackedVelocities()[0].mVelocities.mLinear, packed[0].mVelocities.mLinear);
    }
}


namespace
{
    TEST_F(ActorRagdollPhysicsTest, OwnedHitVelocitySetupUsesArchivedMassAndCurrentDamping)
    {
        auto& definition = mGraph.mBodies.front();
        definition.mNodeRecord = 8;
        definition.mLinearDamping = 2;
        definition.mBlend = NifBullet::RagdollBlendDefinition{30, 8, .5f, 0.f};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        body->setActivationState(ISLAND_SLEEPING);
        const auto before = actor.capture()[0];
        const std::array<NifBullet::RagdollNativeHitVelocitySetupRequest, 1> request{{{8, {1, -2, .5f, 0}, .5f}}};
        actor.prepareNativeHitVelocityControllers(request);
        auto stored = actor.captureNativeVelocityControllers();
        ASSERT_EQ(stored.size(), 1u);
        EXPECT_EQ(stored[0].mAttachedNode, 8u);
        EXPECT_EQ(stored[0].mTargetNode, 8u);
        EXPECT_TRUE(stored[0].mPrecedesBlend);
        // Complete original250 oracle01 case1841, no production-derived expectation.
        const std::array<std::uint32_t, 4> expected{1075838976u, 3231711232u, 1067450368u, 0u};
        for (unsigned axis = 0; axis < 4; ++axis)
            EXPECT_EQ(std::bit_cast<std::uint32_t>(stored[0].mState.mForceVector[axis]), expected[axis]);
        EXPECT_EQ(stored[0].mState.mTiming.mStopKey, .2f);
        EXPECT_EQ(stored[0].mState.mTiming.mFlags, 0xdu);
        EXPECT_EQ(actor.capture()[0].mPose, before.mPose);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, before.mLinearVelocity);
        EXPECT_FALSE(body->isActive());
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.setNativeMotionModes(key);
        actor.prepareNativeHitVelocityControllers(request);
        stored = actor.captureNativeVelocityControllers();
        EXPECT_EQ(stored[0].mState.mForceVector, (std::array<float, 4>{1, -2, .5f, 0}));
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
    }

    TEST_F(ActorRagdollPhysicsTest, OwnedHitVelocitySetupReplacesExistingStateAndRetainsRedirectedTargetAndOrder)
    {
        addHinge();
        mGraph.mBodies[0].mNodeRecord = 8; mGraph.mBodies[1].mNodeRecord = 20;
        mGraph.mBodies[0].mLinearDamping = 2;
        mGraph.mBodies[0].mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 8, 0xd, 1.f, 0.f, 0.f, .25f, {}};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        ESM4::PhysicalVelocityControllerState old;
        old.mTiming = {0xffff, 8, 9, 10, 11};
        old.mClock = {3, 4, 7}; old.mFrameDelta = 99; old.mForceVector = {8, 9, 10, 11};
        const std::array<NifBullet::RagdollNativeVelocityControllerState, 1> saved{{{8, 20, old, false}}};
        actor.restoreNativeVelocityControllers(saved);
        const std::array<std::uint32_t, 2> nodes{8, 20};
        const auto beforeOrder = actor.captureNativeControllerOrder(nodes);
        const std::array<NifBullet::RagdollNativeHitVelocitySetupRequest, 1> request{{{8, {1, -2, .5f, -0.f}, -.5f}}};
        actor.prepareNativeHitVelocityControllers(request);
        const auto stored = actor.captureNativeVelocityControllers();
        ASSERT_EQ(stored.size(), 1u);
        EXPECT_EQ(stored[0].mTargetNode, 20u);
        EXPECT_FALSE(stored[0].mPrecedesBlend);
        EXPECT_EQ(stored[0].mState.mForceVector, (std::array<float, 4>{-2.5f, 5, -1.25f, 0}));
        EXPECT_EQ(stored[0].mState.mTiming.mFlags, 0xfffdu);
        EXPECT_EQ(stored[0].mState.mTiming.mStopKey, .2f);
        EXPECT_EQ(stored[0].mState.mClock.mStartTime, -std::numeric_limits<float>::max());
        EXPECT_EQ(stored[0].mState.mClock.mPreviousTime, -std::numeric_limits<float>::max());
        EXPECT_EQ(stored[0].mState.mClock.mElapsed, 7);
        EXPECT_EQ(stored[0].mState.mFrameDelta, 99);
        EXPECT_EQ(actor.captureNativeControllerOrder(nodes), beforeOrder);
        auto noTarget = stored; noTarget[0].mTargetNode.reset();
        actor.restoreNativeVelocityControllers(noTarget);
        actor.prepareNativeHitVelocityControllers(request);
        EXPECT_FALSE(actor.captureNativeVelocityControllers()[0].mTargetNode);
    }

    TEST_F(ActorRagdollPhysicsTest, OwnedHitVelocitySetupRejectsLateFailuresBeforePublishingOrWaking)
    {
        addHinge(); mGraph.mBodies[0].mNodeRecord = 8; mGraph.mBodies[1].mNodeRecord = 20;
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1.f, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeHitVelocitySetupRequest, 1> good{{{8, {1, 0, 0, 0}, .5f}}};
        actor.prepareNativeHitVelocityControllers(good);
        const auto before = actor.captureNativeVelocityControllers()[0];
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        auto* second = btRigidBody::upcast(actor.collisionObjects()[1]);
        first->setActivationState(ISLAND_SLEEPING); second->setActivationState(ISLAND_SLEEPING);
        const float bad = std::numeric_limits<float>::quiet_NaN();
        const float max = std::numeric_limits<float>::max();
        for (const auto& requests : std::array<std::array<NifBullet::RagdollNativeHitVelocitySetupRequest, 2>, 5>{{
            {{{8, {2, 0, 0, 0}, 1}, {20, {bad, 0, 0, 0}, 1}}},
            {{{8, {2, 0, 0, 0}, 1}, {20, {0, 0, 0, max}, 2}}},
            {{{8, {2, 0, 0, 0}, 1}, {20, {}, bad}}},
            {{{8, {2, 0, 0, 0}, 1}, {8, {}, 1}}},
            {{{8, {2, 0, 0, 0}, 1}, {999, {}, 1}}}}})
        {
            EXPECT_THROW(actor.prepareNativeHitVelocityControllers(requests), std::invalid_argument);
            const auto after = actor.captureNativeVelocityControllers();
            ASSERT_EQ(after.size(), 1u);
            EXPECT_EQ(after[0].mState, before.mState);
            EXPECT_EQ(after[0].mTargetNode, before.mTargetNode);
            EXPECT_FALSE(first->isActive()); EXPECT_FALSE(second->isActive());
        }
        actor.prepareNativeHitVelocityControllers({});
        EXPECT_EQ(actor.captureNativeVelocityControllers()[0].mState, before.mState);
    }
}


namespace
{
    void defineOwnedHitBlends(NifBullet::ActorRagdollDefinition& graph)
    {
        graph.mBodies[0].mNodeRecord = 8;
        graph.mBodies[1].mNodeRecord = 20;
        graph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        graph.mBodies[1].mBlend = NifBullet::RagdollBlendDefinition{31, 8, .7f, .6f};
        graph.mBodies[0].mBlendController = NifBullet::RagdollBlendControllerDefinition{
            78, 20, 0xd, .25f, -.125f, 0, .25f, {{.25f, 1, 1}}};
        graph.mBodies[1].mBlendController = NifBullet::RagdollBlendControllerDefinition{
            79, 20, 0xd, .25f, -.125f, 0, .25f, {{.25f, 1, 1}}};
    }

    void restoreOwnedHitBlendControllers(NifBullet::ActorRagdollPhysics& actor,
        const std::vector<NifBullet::RagdollNativeBlendControllerState>& controllers)
    {
        actor.restore(actor.capture(), actor.captureNativePackedVelocities(), actor.captureNativeMotionModes(),
            actor.captureNativeBlendStates(), controllers, actor.captureNativeVelocityControllers());
    }

    TEST_F(ActorRagdollPhysicsTest, OwnedHitBlendUsesAttachmentGainsAndRetainsRedirectedTargetWithoutMotionSync)
    {
        addHinge(); defineOwnedHitBlends(mGraph);
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto controllers = actor.captureNativeBlendControllers();
        controllers[0].mState.mClock = {3, 4, 7};
        controllers[0].mState.mCachedGains = {.75f, .6f};
        controllers[0].mState.mCursor = 1;
        restoreOwnedHitBlendControllers(actor, controllers);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        actor.setNativeMotionModes(key);
        auto blends = actor.captureNativeBlendStates(); blends[0].mRequestedMotion = 1;
        actor.restoreNativeBlendStates(blends);
        auto* body = btRigidBody::upcast(actor.collisionObjects()[0]);
        body->setActivationState(ISLAND_SLEEPING);
        const auto beforeActivation = body->getActivationState();
        const auto before = actor.capture()[0];
        const std::array<NifBullet::RagdollNativeHitBlendSetupRequest, 1> requests{{{8, {1, 1}}}};
        const auto results = actor.prepareNativeHitBlends(requests);
        ASSERT_EQ(results.size(), 1u);
        EXPECT_EQ(results[0], NifBullet::RagdollNativeHitBlendDisposition::Started);
        const auto after = actor.captureNativeBlendControllers()[0];
        EXPECT_EQ(after.mRecord, 78u); EXPECT_EQ(after.mAttachedNode, 8u); EXPECT_EQ(after.mTargetNode, 20u);
        ASSERT_EQ(after.mState.mKeys.size(), 3u);
        // Original252 case1732, independent attachment node gains .9/.8.
        EXPECT_EQ(after.mState.mKeys[0].mGains, (ESM4::PhysicalBlendGains{.9f, .8f}));
        EXPECT_EQ(after.mState.mKeys[1].mGains, (ESM4::PhysicalBlendGains{1, 1}));
        EXPECT_EQ(after.mState.mTiming.mFlags, 0x1cdu);
        EXPECT_EQ(after.mState.mTiming.mStopKey, 1);
        EXPECT_EQ(after.mState.mClock.mElapsed, 7);
        EXPECT_EQ(after.mState.mCursor, 0u);
        EXPECT_EQ(after.mState.mSetupState, 1u);
        EXPECT_EQ(actor.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
        EXPECT_EQ(actor.captureNativeBlendStates()[0].mRequestedMotion, 1u);
        EXPECT_EQ(actor.captureNativeBlendStates()[0].mGains, blends[0].mGains);
        EXPECT_EQ(actor.capture()[0].mPose, before.mPose);
        EXPECT_EQ(actor.capture()[0].mLinearVelocity, before.mLinearVelocity);
        EXPECT_EQ(body->getActivationState(), beforeActivation);
        EXPECT_EQ(actor.captureNativeBlendControllers()[1].mState, controllers[1].mState);
    }

    TEST_F(ActorRagdollPhysicsTest, OwnedHitBlendSkipsStrongerSetupAndRollsBackLateFailures)
    {
        addHinge(); defineOwnedHitBlends(mGraph);
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        auto saved = actor.captureNativeBlendControllers();
        saved[0].mState.mSetupState = 2;
        restoreOwnedHitBlendControllers(actor, saved);
        const float bad = std::numeric_limits<float>::quiet_NaN();
        const std::array<NifBullet::RagdollNativeHitBlendSetupRequest, 1> stronger{{{8, {bad, bad}}}};
        const auto skipped = actor.prepareNativeHitBlends(stronger);
        ASSERT_EQ(skipped.size(), 1u);
        EXPECT_EQ(skipped[0], NifBullet::RagdollNativeHitBlendDisposition::StrongerSetup);
        EXPECT_EQ(actor.captureNativeBlendControllers()[0].mState, saved[0].mState);
        saved[0].mState.mSetupState = 0;
        restoreOwnedHitBlendControllers(actor, saved);
        auto* first = btRigidBody::upcast(actor.collisionObjects()[0]);
        auto* second = btRigidBody::upcast(actor.collisionObjects()[1]);
        first->setActivationState(ISLAND_SLEEPING); second->setActivationState(ISLAND_SLEEPING);
        for (const auto& requests : std::array<std::array<NifBullet::RagdollNativeHitBlendSetupRequest, 2>, 3>{{
            {{{8, {.4f, .6f}}, {20, {1, bad}}}},
            {{{8, {.4f, .6f}}, {999, {1, 1}}}},
            {{{8, {.4f, .6f}}, {8, {1, 1}}}}}})
        {
            EXPECT_THROW(actor.prepareNativeHitBlends(requests), std::invalid_argument);
            const auto current = actor.captureNativeBlendControllers();
            for (unsigned i = 0; i < 2; ++i) EXPECT_EQ(current[i].mState, saved[i].mState);
            EXPECT_FALSE(first->isActive()); EXPECT_FALSE(second->isActive());
        }
        EXPECT_TRUE(actor.prepareNativeHitBlends({}).empty());
    }

    TEST_F(ActorRagdollPhysicsTest, OwnedHitBlendSkipsMissingBlendAndControllerWithoutUsingGains)
    {
        addHinge(); mGraph.mBodies[0].mNodeRecord = 8; mGraph.mBodies[1].mNodeRecord = 20;
        mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, .9f, .8f};
        NifBullet::ActorRagdollPhysics actor(mGraph, mWorld, 1, mPoses, 1, -1);
        const float bad = std::numeric_limits<float>::quiet_NaN();
        const std::array<NifBullet::RagdollNativeHitBlendSetupRequest, 2> requests{{{8, {bad, bad}}, {20, {bad, bad}}}};
        const auto results = actor.prepareNativeHitBlends(requests);
        ASSERT_EQ(results.size(), 2u);
        EXPECT_EQ(results[0], NifBullet::RagdollNativeHitBlendDisposition::MissingController);
        EXPECT_EQ(results[1], NifBullet::RagdollNativeHitBlendDisposition::MissingBlend);
        EXPECT_TRUE(actor.captureNativeBlendControllers().empty());
        EXPECT_TRUE(actor.captureNativeVelocityControllers().empty());
    }
}
