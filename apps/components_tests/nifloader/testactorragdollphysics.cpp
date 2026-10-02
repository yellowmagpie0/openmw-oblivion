#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/nifbullet/ragdollconecoordinates.hpp>

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
