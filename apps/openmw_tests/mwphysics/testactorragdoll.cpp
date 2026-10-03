#include <apps/openmw/mwphysics/mtphysics.hpp>
#include <apps/openmw/mwphysics/ptrholder.hpp>
#include <apps/openmw/mwphysics/oblivionragdoll.hpp>
#include <apps/openmw/mwclass/static.hpp>
#include <apps/openmw/mwworld/livecellref.hpp>
#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/nifbullet/ragdollvelocity.hpp>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <components/esm3/loadstat.hpp>
#include <components/settings/values.hpp>
#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionShapes/btStaticPlaneShape.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <gtest/gtest.h>
#include <limits>
#include <osg/Stats>

namespace
{
    struct RagdollSchedulerTest : ::testing::TestWithParam<int>
    {
        const int mPreviousThreadCount = Settings::physics().mAsyncNumThreads;
        btDefaultCollisionConfiguration mConfiguration;
        btCollisionDispatcher mDispatcher{&mConfiguration};
        btDbvtBroadphase mBroadphase;
        btSequentialImpulseConstraintSolver mSolver;
        btDiscreteDynamicsWorld mWorld{&mDispatcher, &mBroadphase, &mSolver, &mConfiguration};
        ESM::Static mBase;
        ESM::CellRef mReference;
        std::unique_ptr<MWWorld::LiveCellRef<ESM::Static>> mLive;
        MWWorld::Ptr mPtr;
        NifBullet::ActorRagdollDefinition mGraph;
        std::vector<btTransform> mPoses;
        RagdollSchedulerTest()
        {
            Settings::physics().mAsyncNumThreads.set(GetParam());
            MWClass::Static::registerSelf();
            mBase.blank();
            mReference.blank();
            mLive = std::make_unique<MWWorld::LiveCellRef<ESM::Static>>(mReference, &mBase);
            mPtr = MWWorld::Ptr(mLive.get());
            NifBullet::RagdollBodyDefinition body{};
            body.mRecord = 12;
            body.mMass = 2;
            body.mInertia = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            body.mShape = NifBullet::RagdollSphere{0.5f};
            mGraph.mBodies.push_back(body);
            mPoses.emplace_back(btQuaternion::getIdentity(), btVector3(0, 0, 2));
            mWorld.setGravity(btVector3(0, 0, -10));
        }
        ~RagdollSchedulerTest() override
        {
            Settings::physics().mAsyncNumThreads.set(mPreviousThreadCount);
        }
    };

    TEST_P(RagdollSchedulerTest, AdvancesOwnedBodyThroughQueuedPhysicsAndPreservesStaticWorld)
    {
        btStaticPlaneShape floorShape(btVector3(0, 0, 1), 0);
        btCollisionObject floor;
        floor.setCollisionShape(&floorShape);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addCollisionObject(&floor, 1, -1);
        const auto removeFloor = [&](btCollisionObject* object) { scheduler.removeCollisionObject(object); };
        std::unique_ptr<btCollisionObject, decltype(removeFloor)> floorGuard(&floor, removeFloor);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        ASSERT_EQ(mWorld.getNumCollisionObjects(), 2);
        const auto* body = mWorld.getCollisionObjectArray()[1];
        auto* owner = static_cast<MWPhysics::PtrHolder*>(scheduler.getUserPointer(body));
        ASSERT_NE(owner, nullptr);
        EXPECT_EQ(owner->getPtr(), mPtr);
        scheduler.applyActorRagdollImpulse(mPtr, 0, btVector3(2, 0, 0), mPoses[0].getOrigin());
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("ragdoll scheduler fixture");
        std::array<std::vector<MWPhysics::Simulation>, 2> frames;
        float time = 0;
        for (unsigned frame = 0; frame < 120; ++frame)
        {
            time += 1.f / 60.f;
            scheduler.applyQueuedMovements(time, frames[frame % 2], osg::Timer::instance()->tick(), frame,
                *stats, MWPhysics::WorldFrameData(false, {}));
        }
        const auto state = scheduler.captureActorRagdoll(mPtr);
        ASSERT_EQ(state.size(), 1);
        EXPECT_GT(state[0].mPose.getOrigin().x(), 0.2);
        EXPECT_NEAR(state[0].mPose.getOrigin().z(), 0.5, 0.03);
        // The fixture has zero friction: horizontal sliding is intentional.
        EXPECT_LT(std::abs(state[0].mLinearVelocity.z()), 0.2);
        EXPECT_DOUBLE_EQ(state[0].mLinearVelocity.x(), 1);
        scheduler.removeActorRagdoll(mPtr);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 1);
        EXPECT_EQ(mWorld.getNumConstraints(), 0);
        EXPECT_EQ(scheduler.getUserPointer(body), nullptr);
        floorGuard.reset();
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, NativeGravityPrecedesDampingAndDoesNotAlterGlobalWorldGravity)
    {
        constexpr float dt = 1.f / 60.f;
        constexpr float scale = 7;
        mGraph.mBodies[0].mLinearDamping = 2;
        MWPhysics::PhysicsTaskScheduler scheduler(dt, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, scale, mPoses, 1, -1);
        auto* body = btRigidBody::upcast(mWorld.getCollisionObjectArray()[0]);
        ASSERT_NE(body, nullptr);
        EXPECT_TRUE(body->getFlags() & BT_DISABLE_WORLD_GRAVITY);
        EXPECT_EQ(body->getGravity(), btVector3(0, 0, 0));
        EXPECT_EQ(mWorld.getGravity(), btVector3(0, 0, -10));
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("native gravity order");
        std::array<std::vector<MWPhysics::Simulation>, 2> frames;
        float time = dt;
        scheduler.applyQueuedMovements(time, frames[0], osg::Timer::instance()->tick(), 0,
            *stats, MWPhysics::WorldFrameData(false, {}));
        const auto state = scheduler.captureActorRagdoll(mPtr)[0];
        const float nativeDelta = NifBullet::RagdollNativeDefaultGravityZ * dt;
        const float factor = float(1. - double(dt) * 2.);
        const btScalar expected = btScalar(nativeDelta * factor) * scale;
        EXPECT_DOUBLE_EQ(state.mLinearVelocity.z(), expected);
        EXPECT_NEAR(state.mPose.getOrigin().z(), 2 + expected * dt, 1e-12);
        EXPECT_EQ(mWorld.getGravity(), btVector3(0, 0, -10));
        scheduler.removeActorRagdoll(mPtr);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, AppliesVelocityCapsAndKeepsSleepingBodiesSettled)
    {
        constexpr float dt = 1.f / 60.f;
        mGraph.mBodies[0].mMaxLinearVelocity = 10000;
        mGraph.mBodies[0].mMaxAngularVelocity = 0.1f;
        MWPhysics::PhysicsTaskScheduler scheduler(dt, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        auto* body = btRigidBody::upcast(mWorld.getCollisionObjectArray()[0]);
        body->setActivationState(ISLAND_SLEEPING);
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("native sleeping and caps");
        std::array<std::vector<MWPhysics::Simulation>, 2> frames;
        float time = dt;
        scheduler.applyQueuedMovements(time, frames[0], osg::Timer::instance()->tick(), 0,
            *stats, MWPhysics::WorldFrameData(false, {}));
        auto state = scheduler.captureActorRagdoll(mPtr)[0];
        EXPECT_EQ(state.mPose, mPoses[0]);
        EXPECT_EQ(state.mLinearVelocity, btVector3(0, 0, 0));
        EXPECT_FALSE(body->isActive());
        scheduler.applyActorRagdollImpulse(mPtr, 0, btVector3(1000, 0, 0), mPoses[0].getOrigin());
        time += dt;
        scheduler.applyQueuedMovements(time, frames[1], osg::Timer::instance()->tick(), 1,
            *stats, MWPhysics::WorldFrameData(false, {}));
        state = scheduler.captureActorRagdoll(mPtr)[0];
        EXPECT_TRUE(body->isActive());
        EXPECT_NEAR(state.mLinearVelocity.length(), 250, 3e-5);
        EXPECT_LT(state.mLinearVelocity.z(), 0);
        scheduler.removeActorRagdoll(mPtr);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, SnapshotsUseOwnedGraphAndRejectChangedBindingsAtomically)
    {
        const auto base = ESM::FormKey::content("actors.esm", 100);
        const std::string model = "characters/_male/skeleton.nif";
        mGraph.mSourceHash = std::string(16, 'a');
        mGraph.mBodies[0].mNodeRecord = 7;
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        EXPECT_FALSE(scheduler.hasActorRagdoll(mPtr));
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        ASSERT_TRUE(scheduler.hasActorRagdoll(mPtr));
        const auto original = scheduler.captureActorRagdollSnapshot(mPtr, base, model);
        // Neither the admission argument nor an inspection copy can change the
        // identity used by future capture/restore operations.
        mGraph.mSourceHash.assign(16, 'b');
        mGraph.mBodies[0].mNodeRecord = 99;
        auto inspected = scheduler.actorRagdollDefinition(mPtr);
        inspected.mSourceHash.assign(16, 'c');
        inspected.mBodies.clear();
        EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), original);
        EXPECT_EQ(scheduler.actorRagdollDefinition(mPtr).mBodies[0].mNodeRecord, 7);
        auto changed = original;
        changed.mBodies[0].mPosition = {4, 5, 6};
        changed.mBodies[0].mLinearVelocity = {7, 8, 9};
        scheduler.restoreActorRagdollSnapshot(mPtr, changed, base, model);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), changed);
        for (unsigned field = 0; field < 4; ++field)
        {
            auto invalid = changed;
            switch (field)
            {
                case 0: invalid.mAssetHash[0] = '0'; break;
                case 1: invalid.mBodies[0].mNodeRecord = 99; break;
                case 2: invalid.mBodies[0].mRecord = 99; break;
                case 3: invalid.mBodies[0].mLinearVelocity[0] = std::numeric_limits<float>::infinity(); break;
            }
            if (field == 3)
                EXPECT_THROW(scheduler.restoreActorRagdollSnapshot(mPtr, invalid, base, model), std::runtime_error);
            else
                EXPECT_THROW(scheduler.restoreActorRagdollSnapshot(mPtr, invalid, base, model), std::invalid_argument);
            EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), changed);
        }
        scheduler.removeActorRagdoll(mPtr);
        EXPECT_FALSE(scheduler.hasActorRagdoll(mPtr));
        EXPECT_THROW(scheduler.captureActorRagdollSnapshot(mPtr, base, model), std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, SnapshotOwnershipFollowsReboundActorReference)
    {
        const auto base = ESM::FormKey::content("actors.esm", 100);
        const std::string model = "characters/_male/skeleton.nif";
        mGraph.mSourceHash = std::string(16, 'a');
        mGraph.mBodies[0].mNodeRecord = 7;
        MWWorld::LiveCellRef<ESM::Static> replacement(mReference, &mBase);
        MWWorld::Ptr updated(&replacement);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        const auto original = scheduler.captureActorRagdollSnapshot(mPtr, base, model);
        scheduler.updateActorRagdollPtr(mPtr, updated);
        EXPECT_FALSE(scheduler.hasActorRagdoll(mPtr));
        ASSERT_TRUE(scheduler.hasActorRagdoll(updated));
        EXPECT_EQ(scheduler.captureActorRagdollSnapshot(updated, base, model), original);
        EXPECT_THROW(scheduler.captureActorRagdollSnapshot(mPtr, base, model), std::invalid_argument);
        scheduler.removeActorRagdoll(mPtr);
        EXPECT_TRUE(scheduler.hasActorRagdoll(updated));
        scheduler.removeActorRagdoll(updated);
        EXPECT_FALSE(scheduler.hasActorRagdoll(updated));
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, RestoresAtomicallyRejectsDuplicatesAndRemovesAllOwnedBodies)
    {
        {
            MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
            scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
            EXPECT_THROW(scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1), std::invalid_argument);
            EXPECT_EQ(mWorld.getNumCollisionObjects(), 1);
            auto state = scheduler.captureActorRagdoll(mPtr);
            state[0].mPose.setOrigin(btVector3(4, 5, 6));
            state[0].mLinearVelocity = btVector3(7, 8, 9);
            scheduler.restoreActorRagdoll(mPtr, state);
            auto invalid = state;
            invalid[0].mRecord = 13;
            EXPECT_THROW(scheduler.restoreActorRagdoll(mPtr, invalid), std::invalid_argument);
            const auto after = scheduler.captureActorRagdoll(mPtr);
            EXPECT_EQ(after[0].mPose.getOrigin(), state[0].mPose.getOrigin());
            EXPECT_EQ(after[0].mLinearVelocity, state[0].mLinearVelocity);
        }
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        EXPECT_EQ(mWorld.getNumConstraints(), 0);
    }

    TEST_P(RagdollSchedulerTest, CollisionOnlyWorldRejectsRagdollWithoutChangingLegacyObjects)
    {
        btCollisionWorld collisionWorld(&mDispatcher, &mBroadphase, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &collisionWorld, nullptr);
        EXPECT_THROW(scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1), std::invalid_argument);
        EXPECT_EQ(collisionWorld.getNumCollisionObjects(), 0);
    }
    TEST_P(RagdollSchedulerTest, StepsOncePerSchedulerSubstepAndReleasesWhileWorkersAreActive)
    {
        mWorld.setGravity(btVector3(0, 0, 0));
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        scheduler.applyActorRagdollImpulse(mPtr, 0, btVector3(2, 0, 0), mPoses[0].getOrigin());
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("ragdoll scheduler step count");
        std::array<std::vector<MWPhysics::Simulation>, 2> frames;
        float time = 0;
        for (unsigned frame = 0; frame < 120; ++frame)
        {
            time += 1.f / 60.f;
            scheduler.applyQueuedMovements(time, frames[frame % 2], osg::Timer::instance()->tick(), frame,
                *stats, MWPhysics::WorldFrameData(false, {}));
        }
        const auto state = scheduler.captureActorRagdoll(mPtr);
        ASSERT_EQ(state.size(), 1);
        EXPECT_NEAR(state[0].mPose.getOrigin().x(), 120 * double(1.f / 60.f), 1e-9);
        EXPECT_DOUBLE_EQ(state[0].mLinearVelocity.x(), 1);
        EXPECT_FLOAT_EQ(time, 0);
        const auto* body = mWorld.getCollisionObjectArray()[0];
        time += 1.f / 60.f;
        scheduler.applyQueuedMovements(time, frames[0], osg::Timer::instance()->tick(), 120,
            *stats, MWPhysics::WorldFrameData(false, {}));
        scheduler.releaseSharedStates();
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        EXPECT_EQ(scheduler.getUserPointer(body), nullptr);
    }

    TEST_P(RagdollSchedulerTest, RejectsInvalidGraphWithoutPublishingAndRebindsReferenceIdentity)
    {
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        auto invalid = mGraph;
        invalid.mBodies[0].mMass = 0;
        EXPECT_THROW(scheduler.addActorRagdoll(mPtr, invalid, 1, mPoses, 1, -1), std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        const auto* body = mWorld.getCollisionObjectArray()[0];
        MWWorld::LiveCellRef<ESM::Static> other(mReference, &mBase);
        MWWorld::Ptr updated(&other);
        scheduler.updateActorRagdollPtr(mPtr, updated);
        EXPECT_EQ(static_cast<MWPhysics::PtrHolder*>(scheduler.getUserPointer(body))->getPtr(), updated);
        EXPECT_THROW(scheduler.captureActorRagdoll(mPtr), std::invalid_argument);
        EXPECT_EQ(scheduler.captureActorRagdoll(updated).size(), 1);
        scheduler.removeActorRagdoll(mPtr);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 1);
        scheduler.removeActorRagdoll(updated);
        scheduler.removeActorRagdoll(updated);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, ShutsDownWithoutEnteringAWorkerBarrier)
    {
        // Repeated immediate shutdown exercises workers that have and have not
        // reached their initial wait. Neither may run a job after stopWorkers.
        for (unsigned iteration = 0; iteration < 32; ++iteration)
        {
            MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        }
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    INSTANTIATE_TEST_SUITE_P(WorkerCounts, RagdollSchedulerTest, ::testing::Values(0, 1, 2));

}
