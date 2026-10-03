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
