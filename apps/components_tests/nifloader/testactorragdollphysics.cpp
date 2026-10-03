#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/nifbullet/ragdollconecoordinates.hpp>
#include <components/nifbullet/ragdollvelocity.hpp>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
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

    TEST_F(ActorRagdollPhysicsTest, RejectsIncompleteAmbiguousAndUnsupportedBoneBindings)
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
