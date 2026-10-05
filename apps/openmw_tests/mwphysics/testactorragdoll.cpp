#include <apps/openmw/mwphysics/physicssystem.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/vfs/manager.hpp>
#include <osg/Group>
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
#include <components/nifbullet/nativedynamicsworld.hpp>
#include <cstddef>
#include <new>
#include <bit>
#include <cmath>
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
        ASSERT_TRUE(changed.mBodies[0].mNativePackedVelocity);
        changed.mBodies[0].mNativePackedVelocity->mLinear = {7, 8, 9, -8};
        scheduler.restoreActorRagdollSnapshot(mPtr, changed, base, model);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), changed);
        for (unsigned field = 0; field < 5; ++field)
        {
            auto invalid = changed;
            switch (field)
            {
                case 0: invalid.mAssetHash[0] = '0'; break;
                case 1: invalid.mBodies[0].mNodeRecord = 99; break;
                case 2: invalid.mBodies[0].mRecord = 99; break;
                case 3: invalid.mBodies[0].mLinearVelocity[0] = std::numeric_limits<float>::infinity(); break;
                case 4: invalid.mBodies[0].mNativePackedVelocity->mAngular[3] = std::numeric_limits<float>::quiet_NaN(); break;
            }
            if (field >= 3)
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

    TEST_P(RagdollSchedulerTest, NativeMotionModesSwitchAtWorkerBarrierAndEnterNextSubstep)
    {
        constexpr float dt = 1.f / 60.f;
        MWPhysics::PhysicsTaskScheduler scheduler(dt, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("native motion mode barrier");
        std::array<std::vector<MWPhysics::Simulation>, 2> frames;
        float time = dt;
        scheduler.applyQueuedMovements(time, frames[0], osg::Timer::instance()->tick(), 0,
            *stats, MWPhysics::WorldFrameData(false, {}));
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        scheduler.setActorRagdollNativeMotionModes(mPtr, key);
        const auto keyed = scheduler.captureActorRagdoll(mPtr)[0];
        EXPECT_LT(keyed.mPose.getOrigin().z(), 2);
        EXPECT_EQ(scheduler.captureActorRagdollNativeMotionModes(mPtr),
            std::vector<NifBullet::RagdollNativeMotionRequest>(key.begin(), key.end()));
        const std::array<NifBullet::RagdollNativeVelocityDrive, 1> drives{{{12, {{1, 0, 2}, {0, 0, 0, 1}}, 1.f}}};
        EXPECT_THROW(scheduler.driveActorRagdollPoseVelocities(mPtr, drives, 120.f), std::invalid_argument);
        time += dt;
        scheduler.applyQueuedMovements(time, frames[1], osg::Timer::instance()->tick(), 1,
            *stats, MWPhysics::WorldFrameData(false, {}));
        EXPECT_EQ(scheduler.captureActorRagdoll(mPtr)[0].mPose, keyed.mPose);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> dynamic{{{12, NifBullet::RagdollNativeMotion::Dynamic}}};
        scheduler.setActorRagdollNativeMotionModes(mPtr, dynamic);
        EXPECT_EQ(scheduler.captureActorRagdollNativeMotionModes(mPtr),
            std::vector<NifBullet::RagdollNativeMotionRequest>(dynamic.begin(), dynamic.end()));
        time += dt;
        scheduler.applyQueuedMovements(time, frames[0], osg::Timer::instance()->tick(), 2,
            *stats, MWPhysics::WorldFrameData(false, {}));
        EXPECT_LT(scheduler.captureActorRagdoll(mPtr)[0].mPose.getOrigin().z(), keyed.mPose.getOrigin().z());
        EXPECT_EQ(mWorld.getGravity(), btVector3(0, 0, -10));
        scheduler.removeActorRagdoll(mPtr);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        EXPECT_EQ(mWorld.getNumConstraints(), 0);
    }

    TEST_P(RagdollSchedulerTest, NativeMotionModesRejectStaleOwnersAndInvalidBatchesWithoutPublication)
    {
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        const auto before = scheduler.captureActorRagdoll(mPtr)[0];
        const auto modes = scheduler.captureActorRagdollNativeMotionModes(mPtr);
        std::array<NifBullet::RagdollNativeMotionRequest, 1> request{{{999, NifBullet::RagdollNativeMotion::Keyframed}}};
        EXPECT_THROW(scheduler.setActorRagdollNativeMotionModes(mPtr, request), std::invalid_argument);
        EXPECT_EQ(scheduler.captureActorRagdoll(mPtr)[0].mPose, before.mPose);
        EXPECT_EQ(scheduler.captureActorRagdollNativeMotionModes(mPtr), modes);
        request[0].mRecord = 12;
        request[0].mMotion = static_cast<NifBullet::RagdollNativeMotion>(99);
        EXPECT_THROW(scheduler.setActorRagdollNativeMotionModes(mPtr, request), std::invalid_argument);
        request[0].mMotion = NifBullet::RagdollNativeMotion::Keyframed;
        MWWorld::LiveCellRef<ESM::Static> replacement(mReference, &mBase);
        MWWorld::Ptr updated(&replacement);
        scheduler.updateActorRagdollPtr(mPtr, updated);
        EXPECT_THROW(scheduler.setActorRagdollNativeMotionModes(mPtr, request), std::invalid_argument);
        EXPECT_THROW(scheduler.captureActorRagdollNativeMotionModes(mPtr), std::invalid_argument);
        EXPECT_THROW(scheduler.setActorRagdollNativeMotionModes({}, request), std::invalid_argument);
        scheduler.setActorRagdollNativeMotionModes(updated, request);
        EXPECT_EQ(scheduler.captureActorRagdollNativeMotionModes(updated),
            std::vector<NifBullet::RagdollNativeMotionRequest>(request.begin(), request.end()));
        scheduler.removeActorRagdoll(updated);
        EXPECT_THROW(scheduler.setActorRagdollNativeMotionModes(updated, {}), std::invalid_argument);
        EXPECT_THROW(scheduler.captureActorRagdollNativeMotionModes(updated), std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, NativeMotionModesPreserveUnselectedBodiesAndWorldConfiguration)
    {
        auto other = mGraph.mBodies[0];
        other.mRecord = 24;
        mGraph.mBodies.push_back(other);
        mPoses.emplace_back(btQuaternion::getIdentity(), btVector3(0, 0, 4));
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        auto* first = btRigidBody::upcast(mWorld.getCollisionObjectArray()[0]);
        auto* second = btRigidBody::upcast(mWorld.getCollisionObjectArray()[1]);
        first->setActivationState(ISLAND_SLEEPING);
        const auto* firstOwner = scheduler.getUserPointer(first);
        const auto* secondOwner = scheduler.getUserPointer(second);
        const auto group = second->getBroadphaseHandle()->m_collisionFilterGroup;
        const auto mask = second->getBroadphaseHandle()->m_collisionFilterMask;
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{24, NifBullet::RagdollNativeMotion::Keyframed}}};
        scheduler.setActorRagdollNativeMotionModes(mPtr, key);
        EXPECT_FALSE(first->isActive());
        EXPECT_FALSE(first->isKinematicObject());
        EXPECT_TRUE(second->isKinematicObject());
        EXPECT_EQ(scheduler.captureActorRagdollNativeMotionModes(mPtr), (std::vector<NifBullet::RagdollNativeMotionRequest>{
            {12, NifBullet::RagdollNativeMotion::Dynamic}, {24, NifBullet::RagdollNativeMotion::Keyframed}}));
        EXPECT_EQ(scheduler.getUserPointer(first), firstOwner);
        EXPECT_EQ(scheduler.getUserPointer(second), secondOwner);
        EXPECT_EQ(second->getBroadphaseHandle()->m_collisionFilterGroup, group);
        EXPECT_EQ(second->getBroadphaseHandle()->m_collisionFilterMask, mask);
        EXPECT_TRUE(second->getFlags() & BT_DISABLE_WORLD_GRAVITY);
        EXPECT_EQ(second->getGravity(), btVector3(0, 0, 0));
        EXPECT_EQ(mWorld.getGravity(), btVector3(0, 0, -10));
        scheduler.removeActorRagdoll(mPtr);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        EXPECT_EQ(mWorld.getNumConstraints(), 0);
    }

    TEST_P(RagdollSchedulerTest, NativeKeyframedScenePublicationWaitsForWorkersAndSurvivesNextSubstep)
    {
        constexpr float dt = 1.f / 60.f;
        MWPhysics::PhysicsTaskScheduler scheduler(dt, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("native scene publication barrier");
        std::array<std::vector<MWPhysics::Simulation>, 2> frames;
        float time = dt;
        scheduler.applyQueuedMovements(time, frames[0], osg::Timer::instance()->tick(), 0,
            *stats, MWPhysics::WorldFrameData(false, {}));
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{12, NifBullet::RagdollNativeMotion::Keyframed}}};
        scheduler.setActorRagdollNativeMotionModes(mPtr, key);
        auto* body = btRigidBody::upcast(mWorld.getCollisionObjectArray()[0]);
        const auto* shape = body->getCollisionShape();
        const auto* proxy = body->getBroadphaseHandle();
        const auto* owner = scheduler.getUserPointer(body);
        time += dt;
        scheduler.applyQueuedMovements(time, frames[1], osg::Timer::instance()->tick(), 1,
            *stats, MWPhysics::WorldFrameData(false, {}));
        const osg::Matrixf target = osg::Matrixf::rotate(.7f, osg::Vec3f(0, 0, 1))
            * osg::Matrixf::translate(70, 14, 21);
        const std::array<NifBullet::RagdollNativeScenePoseRequest, 1> poses{{{12, target}}};
        scheduler.synchronizeActorRagdollKeyframedPoses(mPtr, poses);
        const auto expected = NifBullet::ragdollNativePoseFromBoneWorld(target);
        EXPECT_EQ(scheduler.captureActorRagdoll(mPtr)[0].mPose, expected);
        EXPECT_EQ(body->getInterpolationWorldTransform(), body->getWorldTransform());
        EXPECT_EQ(body->getCollisionShape(), shape);
        EXPECT_EQ(body->getBroadphaseHandle(), proxy);
        EXPECT_EQ(scheduler.getUserPointer(body), owner);
        btCollisionWorld::ClosestRayResultCallback ray(btVector3(8, 2, 3), btVector3(12, 2, 3));
        scheduler.rayTest(ray.m_rayFromWorld, ray.m_rayToWorld, ray);
        EXPECT_EQ(ray.m_collisionObject, body);
        time += dt;
        scheduler.applyQueuedMovements(time, frames[0], osg::Timer::instance()->tick(), 2,
            *stats, MWPhysics::WorldFrameData(false, {}));
        EXPECT_EQ(scheduler.captureActorRagdoll(mPtr)[0].mPose, expected);
        EXPECT_EQ(mWorld.getGravity(), btVector3(0, 0, -10));
        scheduler.removeActorRagdoll(mPtr);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        EXPECT_EQ(mWorld.getNumConstraints(), 0);
    }

    TEST_P(RagdollSchedulerTest, NativeKeyframedScenePublicationRejectsBatchAndStaleOwnersAtomically)
    {
        auto other = mGraph.mBodies[0];
        other.mRecord = 24;
        mGraph.mBodies.push_back(other);
        mPoses.emplace_back(btQuaternion::getIdentity(), btVector3(0, 0, 4));
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeMotionRequest, 2> key{{
            {12, NifBullet::RagdollNativeMotion::Keyframed}, {24, NifBullet::RagdollNativeMotion::Keyframed}}};
        scheduler.setActorRagdollNativeMotionModes(mPtr, key);
        const auto before = scheduler.captureActorRagdoll(mPtr);
        std::array<NifBullet::RagdollNativeScenePoseRequest, 2> poses{{
            {12, osg::Matrixf::translate(70, 0, 0)}, {999, osg::Matrixf::identity()}}};
        EXPECT_THROW(scheduler.synchronizeActorRagdollKeyframedPoses(mPtr, poses), std::invalid_argument);
        EXPECT_EQ(scheduler.captureActorRagdoll(mPtr)[0].mPose, before[0].mPose);
        poses[1].mRecord = 12;
        EXPECT_THROW(scheduler.synchronizeActorRagdollKeyframedPoses(mPtr, poses), std::invalid_argument);
        poses[1].mRecord = 24;
        poses[1].mWorldPose(0, 0) = std::numeric_limits<float>::infinity();
        EXPECT_THROW(scheduler.synchronizeActorRagdollKeyframedPoses(mPtr, poses), std::invalid_argument);
        EXPECT_EQ(scheduler.captureActorRagdoll(mPtr)[0].mPose, before[0].mPose);
        EXPECT_EQ(scheduler.captureActorRagdoll(mPtr)[1].mPose, before[1].mPose);
        MWWorld::LiveCellRef<ESM::Static> replacement(mReference, &mBase);
        const MWWorld::Ptr updated(&replacement);
        scheduler.updateActorRagdollPtr(mPtr, updated);
        const auto one = std::span<const NifBullet::RagdollNativeScenePoseRequest>(poses.data(), 1);
        EXPECT_THROW(scheduler.synchronizeActorRagdollKeyframedPoses(mPtr, one), std::invalid_argument);
        EXPECT_THROW(scheduler.synchronizeActorRagdollKeyframedPoses({}, one), std::invalid_argument);
        scheduler.synchronizeActorRagdollKeyframedPoses(updated, one);
        EXPECT_EQ(scheduler.captureActorRagdoll(updated)[0].mPose,
            NifBullet::ragdollNativePoseFromBoneWorld(poses[0].mWorldPose));
        scheduler.removeActorRagdoll(updated);
        EXPECT_THROW(scheduler.synchronizeActorRagdollKeyframedPoses(updated, {}), std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, NativeKeyframedScenePublicationPreservesUnselectedBodiesAndVelocities)
    {
        auto other = mGraph.mBodies[0];
        other.mRecord = 24;
        mGraph.mBodies.push_back(other);
        mPoses.emplace_back(btQuaternion::getIdentity(), btVector3(0, 0, 4));
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        const std::array<NifBullet::RagdollNativeMotionRequest, 1> key{{{24, NifBullet::RagdollNativeMotion::Keyframed}}};
        scheduler.setActorRagdollNativeMotionModes(mPtr, key);
        auto* first = btRigidBody::upcast(mWorld.getCollisionObjectArray()[0]);
        auto* second = btRigidBody::upcast(mWorld.getCollisionObjectArray()[1]);
        first->setActivationState(ISLAND_SLEEPING);
        second->setLinearVelocity(btVector3(1, 2, 3));
        second->setAngularVelocity(btVector3(4, 5, 6));
        second->applyCentralForce(btVector3(7, 8, 9));
        const auto group = second->getBroadphaseHandle()->m_collisionFilterGroup;
        const auto mask = second->getBroadphaseHandle()->m_collisionFilterMask;
        scheduler.synchronizeActorRagdollKeyframedPoses(mPtr, {});
        EXPECT_FALSE(first->isActive());
        const std::array<NifBullet::RagdollNativeScenePoseRequest, 1> poses{{{24, osg::Matrixf::translate(70, 0, 0)}}};
        scheduler.synchronizeActorRagdollKeyframedPoses(mPtr, poses);
        EXPECT_EQ(scheduler.captureActorRagdoll(mPtr)[0].mPose, mPoses[0]);
        EXPECT_FALSE(first->isActive());
        EXPECT_GT(scheduler.captureActorRagdoll(mPtr)[1].mPose.getOrigin().x(), 9.9);
        EXPECT_EQ(second->getLinearVelocity(), btVector3(1, 2, 3));
        EXPECT_EQ(second->getAngularVelocity(), btVector3(4, 5, 6));
        EXPECT_EQ(second->getTotalForce(), btVector3(7, 8, 9));
        EXPECT_EQ(second->getBroadphaseHandle()->m_collisionFilterGroup, group);
        EXPECT_EQ(second->getBroadphaseHandle()->m_collisionFilterMask, mask);
        EXPECT_EQ(mWorld.getGravity(), btVector3(0, 0, -10));
        scheduler.removeActorRagdoll(mPtr);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, NativePoseDriveUsesNativeGravityAndPreservesOtherWorldState)
    {
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        auto* body = btRigidBody::upcast(mWorld.getCollisionObjectArray()[0]);
        body->setActivationState(ISLAND_SLEEPING);
        const std::array<NifBullet::RagdollNativeVelocityDrive, 1> drives{{{12, {{1, 0, 2}, {0, 0, 0, 1}}, .5f}}};
        scheduler.driveActorRagdollPoseVelocities(mPtr, drives, 120.f);
        const auto result = scheduler.captureActorRagdoll(mPtr)[0];
        EXPECT_EQ(result.mLinearVelocity, btVector3(60, 0, std::bit_cast<float>(1050473923u)));
        EXPECT_EQ(result.mAngularVelocity, btVector3(0, 0, 0));
        EXPECT_EQ(result.mPose, mPoses[0]);
        EXPECT_TRUE(body->isActive());
        EXPECT_EQ(mWorld.getGravity(), btVector3(0, 0, -10));
        EXPECT_EQ(static_cast<MWPhysics::PtrHolder*>(scheduler.getUserPointer(body))->getPtr(), mPtr);
    }

    TEST_P(RagdollSchedulerTest, NativePoseDriveWaitsForQueuedWorkersAndEntersNextSubstep)
    {
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("native pose drive barrier");
        std::array<std::vector<MWPhysics::Simulation>, 2> frames;
        float time = 1.f / 60.f;
        scheduler.applyQueuedMovements(time, frames[0], osg::Timer::instance()->tick(), 0,
            *stats, MWPhysics::WorldFrameData(false, {}));
        const std::array<NifBullet::RagdollNativeVelocityDrive, 1> drives{{{12, {{1, 0, 2}, {0, 0, 0, 1}}, .5f}}};
        scheduler.driveActorRagdollPoseVelocities(mPtr, drives, 120.f);
        auto result = scheduler.captureActorRagdoll(mPtr)[0];
        EXPECT_EQ(result.mLinearVelocity.x(), 60);
        EXPECT_EQ(result.mPose.getOrigin().x(), 0);
        EXPECT_LT(result.mPose.getOrigin().z(), 2);
        time += 1.f / 60.f;
        scheduler.applyQueuedMovements(time, frames[1], osg::Timer::instance()->tick(), 1,
            *stats, MWPhysics::WorldFrameData(false, {}));
        result = scheduler.captureActorRagdoll(mPtr)[0];
        EXPECT_NEAR(result.mPose.getOrigin().x(), 60 * double(1.f / 60.f), 1e-9);
        EXPECT_EQ(result.mLinearVelocity.x(), 60);
        scheduler.removeActorRagdoll(mPtr);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, NativePoseDriveRejectsInvalidOrStaleOwnersAndRestoresProjectedState)
    {
        const auto base = ESM::FormKey::content("actors.esm", 100);
        const std::string model = "characters/_male/skeleton.nif";
        mGraph.mSourceHash = std::string(16, 'a');
        mGraph.mBodies[0].mNodeRecord = 7;
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        const auto original = scheduler.captureActorRagdollSnapshot(mPtr, base, model);
        std::array<NifBullet::RagdollNativeVelocityDrive, 1> drives{{{999, {{1, 0, 2}, {0, 0, 0, 1}}, .5f}}};
        EXPECT_THROW(scheduler.driveActorRagdollPoseVelocities(mPtr, drives, 120.f), std::invalid_argument);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), original);
        drives[0].mRecord = 12;
        EXPECT_THROW(scheduler.driveActorRagdollPoseVelocities(mPtr, drives, 0.f), std::invalid_argument);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), original);
        MWWorld::LiveCellRef<ESM::Static> replacement(mReference, &mBase);
        MWWorld::Ptr updated(&replacement);
        scheduler.updateActorRagdollPtr(mPtr, updated);
        EXPECT_THROW(scheduler.driveActorRagdollPoseVelocities(mPtr, drives, 120.f), std::invalid_argument);
        EXPECT_THROW(scheduler.driveActorRagdollPoseVelocities({}, drives, 120.f), std::invalid_argument);
        scheduler.driveActorRagdollPoseVelocities(updated, drives, 120.f);
        const auto driven = scheduler.captureActorRagdollSnapshot(updated, base, model);
        EXPECT_NE(driven, original);
        scheduler.restoreActorRagdollSnapshot(updated, original, base, model);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshot(updated, base, model), original);
        scheduler.removeActorRagdoll(updated);
        EXPECT_THROW(scheduler.driveActorRagdollPoseVelocities(updated, drives, 120.f), std::invalid_argument);
    }


    TEST_P(RagdollSchedulerTest, NativeBlendPublicationWaitsForWorkersAndUsesNativeGravity)
    {
        constexpr float dt = 1.f/60;
        MWPhysics::PhysicsTaskScheduler scheduler(dt, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("native blend publication barrier");
        std::array<std::vector<MWPhysics::Simulation>, 2> frames;
        float time = dt;
        scheduler.applyQueuedMovements(time, frames[0], osg::Timer::instance()->tick(), 0,
            *stats, MWPhysics::WorldFrameData(false, {}));
        std::array<NifBullet::RagdollNativeBlendUpdate, 1> requests{{{12, osg::Matrixf::identity(), 1, .5f, 8}}};
        const auto key = scheduler.updateActorRagdollBlends(mPtr, requests, dt, 0);
        ASSERT_EQ(key.size(), 1); EXPECT_EQ(key[0].mCollisionFlags, 0); EXPECT_FALSE(key[0].mSceneTarget);
        auto state = scheduler.captureActorRagdoll(mPtr)[0];
        EXPECT_EQ(state.mPose, btTransform::getIdentity()); EXPECT_EQ(state.mLinearVelocity, btVector3(0, 0, 0));
        time += dt;
        scheduler.applyQueuedMovements(time, frames[1], osg::Timer::instance()->tick(), 1,
            *stats, MWPhysics::WorldFrameData(false, {}));
        requests[0].mHierarchyGain = .25f; requests[0].mCollisionFlags = key[0].mCollisionFlags;
        const auto mixed = scheduler.updateActorRagdollBlends(mPtr, requests, 1.f/120, 0);
        ASSERT_EQ(mixed.size(), 1); EXPECT_EQ(mixed[0].mCollisionFlags, 8); ASSERT_TRUE(mixed[0].mSceneTarget);
        state = scheduler.captureActorRagdoll(mPtr)[0];
        EXPECT_EQ(state.mPose, btTransform::getIdentity());
        // Original composed frame inverse + velocity mix: inverse1/(1.f/120)
        // stores bits1123024895, so gravity compensation is bits1050473924.
        EXPECT_EQ(state.mLinearVelocity, btVector3(0, 0, std::bit_cast<float>(1050473924u)));
        EXPECT_EQ(state.mAngularVelocity, btVector3(0, 0, 0));
        EXPECT_EQ(scheduler.captureActorRagdollNativeMotionModes(mPtr)[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        EXPECT_EQ(mWorld.getGravity(), btVector3(0, 0, -10));
        scheduler.removeActorRagdoll(mPtr); EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, NativeBlendPublicationRejectsBatchesAndStaleOwnersBeforeMutation)
    {
        auto other = mGraph.mBodies[0]; other.mRecord = 24; mGraph.mBodies.push_back(other);
        mPoses.emplace_back(btQuaternion::getIdentity(), btVector3(0, 0, 4));
        MWPhysics::PhysicsTaskScheduler scheduler(1.f/60, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        std::array<NifBullet::RagdollNativeBlendUpdate, 2> updates{{
            {12, osg::Matrixf::identity(), 1, .5f, 8}, {24, osg::Matrixf::scale(2, 2, 2), 1, .5f, 8}}};
        EXPECT_THROW(scheduler.updateActorRagdollBlends(mPtr, updates, 1.f/120, 0), std::invalid_argument);
        updates[1].mAnimatedWorld.makeIdentity(); updates[1].mCollisionFlags = 0x20;
        EXPECT_THROW(scheduler.updateActorRagdollBlends(mPtr, updates, 1.f/120, 0), std::invalid_argument);
        updates[1].mCollisionFlags = 8; updates[1].mRecord = 12;
        EXPECT_THROW(scheduler.updateActorRagdollBlends(mPtr, updates, 1.f/120, 0), std::invalid_argument);
        updates[1].mRecord = 999;
        EXPECT_THROW(scheduler.updateActorRagdollBlends(mPtr, updates, 1.f/120, 0), std::invalid_argument);
        EXPECT_EQ(scheduler.captureActorRagdoll(mPtr)[0].mPose, mPoses[0]);
        EXPECT_EQ(scheduler.captureActorRagdollNativeMotionModes(mPtr)[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        MWWorld::LiveCellRef<ESM::Static> replacement(mReference, &mBase); const MWWorld::Ptr updated(&replacement);
        scheduler.updateActorRagdollPtr(mPtr, updated);
        const auto one = std::span<const NifBullet::RagdollNativeBlendUpdate>(updates.data(), 1);
        EXPECT_THROW(scheduler.updateActorRagdollBlends(mPtr, one, 1.f/120, 0), std::invalid_argument);
        EXPECT_THROW(scheduler.updateActorRagdollBlends({}, one, 1.f/120, 0), std::invalid_argument);
        ASSERT_EQ(scheduler.updateActorRagdollBlends(updated, one, 1.f/120, 0).size(), 1);
        scheduler.removeActorRagdoll(updated);
        EXPECT_THROW(scheduler.updateActorRagdollBlends(updated, {}, 1.f/120, 0), std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, NativeBlendPublicationKeepsUnselectedSleepAndCollisionIdentity)
    {
        auto other = mGraph.mBodies[0]; other.mRecord = 24; mGraph.mBodies.push_back(other);
        mPoses.emplace_back(btQuaternion::getIdentity(), btVector3(0, 0, 4));
        MWPhysics::PhysicsTaskScheduler scheduler(1.f/60, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        auto* first = btRigidBody::upcast(mWorld.getCollisionObjectArray()[0]);
        auto* second = btRigidBody::upcast(mWorld.getCollisionObjectArray()[1]);
        first->setActivationState(ISLAND_SLEEPING); second->applyCentralForce({1, 2, 3});
        const auto* proxy = second->getBroadphaseHandle(); const auto* shape = second->getCollisionShape();
        const auto* owner = scheduler.getUserPointer(second);
        EXPECT_TRUE(scheduler.updateActorRagdollBlends(mPtr, {}, 1.f/120, 0).empty());
        const std::array<NifBullet::RagdollNativeBlendUpdate, 1> updates{{{24, osg::Matrixf::translate(70, 0, 0), 1, .5f, 8}}};
        ASSERT_EQ(scheduler.updateActorRagdollBlends(mPtr, updates, 1.f/120, 0).size(), 1);
        EXPECT_FALSE(first->isActive()); EXPECT_EQ(scheduler.captureActorRagdoll(mPtr)[0].mPose, mPoses[0]);
        EXPECT_EQ(second->getTotalForce(), btVector3(1, 2, 3)); EXPECT_EQ(second->getBroadphaseHandle(), proxy);
        EXPECT_EQ(second->getCollisionShape(), shape); EXPECT_EQ(scheduler.getUserPointer(second), owner);
        EXPECT_EQ(mWorld.getGravity(), btVector3(0, 0, -10));
        scheduler.removeActorRagdoll(mPtr); EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, NativeBlendSnapshotRestoresFreshOwnerBeforeIndependentMotionDispatch)
    {
        const auto base = ESM::FormKey::content("actors.esm", 100);
        const std::string model = "characters/_male/skeleton.nif";
        mGraph.mSourceHash = std::string(16, 'a'); mGraph.mBodies[0].mNodeRecord = 8;
        mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, .2f, .8f};
        auto other = mGraph.mBodies[0]; other.mRecord = 6; other.mNodeRecord = 16;
        other.mBlend->mRecord = 31; mGraph.mBodies.push_back(other); mPoses.emplace_back(btQuaternion::getIdentity(), btVector3(0, 0, 4));
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("native blend save barrier");
        std::vector<MWPhysics::Simulation> frame;
        float time = 1.f / 60.f;
        scheduler.applyQueuedMovements(time, frame, osg::Timer::instance()->tick(), 0,
            *stats, MWPhysics::WorldFrameData(false, {}));
        auto snapshot = scheduler.captureActorRagdollSnapshot(mPtr, base, model);
        ASSERT_TRUE(snapshot.mNativeBlends); ASSERT_EQ(snapshot.mNativeBlends->size(), 2u);
        EXPECT_EQ((*snapshot.mNativeBlends)[0].mBodyRecord, 6);
        EXPECT_EQ((*snapshot.mNativeBlends)[1].mBodyRecord, 12);
        snapshot.mBodies[1].mNativeMotion = ESM4::RuntimeRagdollMotion::Keyframed;
        snapshot.mBodies[1].mNativePackedVelocity->mLinear = {1, 2, 3, 8};
        snapshot.mBodies[1].mNativePackedVelocity->mAngular = {4, 5, 6, -0.f};
        snapshot.mBodies[1].mLinearVelocity = {1, 2, 3}; snapshot.mBodies[1].mAngularVelocity = {4, 5, 6};
        (*snapshot.mNativeBlends)[0] = {6, 0xf123, 0xffffffffu, -2.f, 3.f};
        (*snapshot.mNativeBlends)[1] = {12, 8, 1, -0.f, 0.f};
        scheduler.restoreActorRagdollSnapshot(mPtr, snapshot, base, model);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), snapshot);
        scheduler.removeActorRagdoll(mPtr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        scheduler.restoreActorRagdollSnapshot(mPtr, snapshot, base, model);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), snapshot);
        const auto native = scheduler.captureActorRagdollBlendStates(mPtr);
        ASSERT_EQ(native.size(), 2u); EXPECT_EQ(native[0].mBodyRecord, 12);
        EXPECT_EQ(native[0].mRequestedMotion, 1u); EXPECT_TRUE(std::signbit(native[0].mGains.mHierarchy));
        EXPECT_EQ(native[1].mRequestedMotion, 0xffffffffu); EXPECT_EQ(native[1].mCollisionFlags, 0xf123);
        for (unsigned field = 0; field < 4; ++field)
        {
            auto invalid = snapshot; invalid.mBodies[1].mPosition = {10, 20, 30};
            if (field == 0) (*invalid.mNativeBlends)[1].mVelocityGain = std::numeric_limits<float>::quiet_NaN();
            if (field == 1) invalid.mNativeBlends->pop_back();
            if (field == 2) (*invalid.mNativeBlends)[1].mBodyRecord = 99;
            if (field == 3) (*invalid.mNativeBlends)[1].mBodyRecord = 6;
            if (field == 1)
                EXPECT_THROW(scheduler.restoreActorRagdollSnapshot(mPtr, invalid, base, model), std::invalid_argument);
            else
                EXPECT_THROW(scheduler.restoreActorRagdollSnapshot(mPtr, invalid, base, model), std::runtime_error);
            EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), snapshot);
        }
        // Stored request1 matching the next selected request preserves actual
        // KEY and packed velocities; constructor request8 would clear/handoff.
        const std::array updates{NifBullet::RagdollNativeBlendUpdate{12, osg::Matrixf::identity(), 0, 0, 8}};
        scheduler.updateActorRagdollBlends(mPtr, updates, .016f, 0);
        EXPECT_EQ(scheduler.captureActorRagdollNativeMotionModes(mPtr)[0].mMotion, NifBullet::RagdollNativeMotion::Keyframed);
        EXPECT_EQ(scheduler.captureActorRagdollNativePackedVelocities(mPtr)[0].mVelocities.mLinear[3], 8);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(scheduler.captureActorRagdollNativePackedVelocities(mPtr)[0].mVelocities.mAngular[3]), 0x80000000u);
        scheduler.removeActorRagdoll(mPtr);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, NativeControllerSnapshotsRestoreFreshOwnershipBeforeNextPhase)
    {
        const auto base = ESM::FormKey::content("actors.esm", 100); const std::string model = "characters/_male/skeleton.nif";
        mGraph.mSourceHash = std::string(16, 'a'); mGraph.mBodies[0].mNodeRecord = 8;
        mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, .5f, .5f};
        mGraph.mBodies[0].mBlendController = NifBullet::RagdollBlendControllerDefinition{78, 8, 0xd, 1, 0, 0, 4, {}};
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("native controller snapshot barrier");
        std::vector<MWPhysics::Simulation> frame; float time = 1.f / 60.f;
        scheduler.applyQueuedMovements(time, frame, osg::Timer::instance()->tick(), 0, *stats, MWPhysics::WorldFrameData(false, {}));
        auto snapshot = scheduler.captureActorRagdollSnapshot(mPtr, base, model);
        ASSERT_TRUE(snapshot.mNativeControllers); ASSERT_EQ(snapshot.mNativeControllers->mBlends.size(), 1u);
        auto& curve = snapshot.mNativeControllers->mBlends[0].mState;
        curve.mTiming = {0xd, 1, -0.f, 0, 4}; curve.mClock = {10, 11, 1};
        curve.mKeys = {{0, {1, 0}}, {1, {.5f, .5f}}, {1, {.25f, .75f}}, {4, {0, 1}}};
        curve.mCursor = 1; curve.mCachedGains = {-0.f, -2}; curve.mSetupState = 0xffffffffu;
        ESM4::RuntimeRagdollVelocityController generated; generated.mAttachedNode = 8; generated.mTargetNode = 8;
        generated.mPrecedesBlend = false; generated.mState.mTiming = {0xd, 1, -0.f, 0, 4};
        generated.mState.mClock = {10, 11, 1}; generated.mState.mForceVector = {1, 2, 3, 8};
        snapshot.mNativeControllers->mVelocities.push_back(generated);
        scheduler.restoreActorRagdollSnapshot(mPtr, snapshot, base, model);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), snapshot);
        scheduler.removeActorRagdoll(mPtr); scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        scheduler.restoreActorRagdollSnapshot(mPtr, snapshot, base, model);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), snapshot);
        EXPECT_TRUE(std::signbit(scheduler.captureActorRagdollBlendControllers(mPtr)[0].mState.mCachedGains.mHierarchy));
        const std::array<std::uint32_t, 1> nodes{8};
        const std::vector<NifBullet::RagdollNativeControllerReference> expectedOrder{
            {NifBullet::RagdollNativeControllerKind::Blend, 78}, {NifBullet::RagdollNativeControllerKind::Velocity, 8}};
        EXPECT_EQ(scheduler.captureActorRagdollControllerOrder(mPtr, nodes), expectedOrder);
        for (unsigned field = 0; field < 4; ++field)
        {
            auto invalid = snapshot; invalid.mBodies[0].mPosition = {10, 20, 30};
            if (field == 0) invalid.mNativeControllers->mBlends.clear();
            if (field == 1) invalid.mNativeControllers->mBlends[0].mRecord = 99;
            if (field == 2) invalid.mNativeControllers->mBlends[0].mTargetNode = 99;
            if (field == 3) invalid.mNativeControllers->mVelocities[0].mState.mClock.mElapsed = std::numeric_limits<float>::quiet_NaN();
            if (field <= 1)
                EXPECT_THROW(scheduler.restoreActorRagdollSnapshot(mPtr, invalid, base, model), std::invalid_argument);
            else
                EXPECT_THROW(scheduler.restoreActorRagdollSnapshot(mPtr, invalid, base, model), std::runtime_error);
            EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), snapshot);
        }
        // This tests restored per-owner state with a cold shared physical cache;
        // persistence of that shared cache is a separate authority boundary.
        scheduler.advanceActorRagdollPhysicalControllers(mPtr, expectedOrder, 12);
        const auto advanced = scheduler.captureActorRagdollSnapshot(mPtr, base, model);
        EXPECT_EQ(advanced.mNativeControllers->mBlends[0].mState.mClock.mElapsed, 2);
        EXPECT_EQ(advanced.mNativeControllers->mBlends[0].mState.mCursor, 2u);
        EXPECT_EQ(advanced.mNativeControllers->mVelocities[0].mState.mClock.mPreviousTime, 12);
        // Saved previous time11 and input12 select delta1, not sentinel fallback.
        EXPECT_EQ(advanced.mNativeControllers->mVelocities[0].mState.mFrameDelta, 1.f);
        EXPECT_EQ(advanced.mBodies[0].mNativePackedVelocity->mLinear[3], 400.f);
        scheduler.removeActorRagdoll(mPtr); EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, SavedSharedPhysicalCacheRestoresFreshSchedulerBeforeCrossOwnerHit)
    {
        const auto base = ESM::FormKey::content("actors.esm", 100); const std::string model = "characters/_male/skeleton.nif";
        mGraph.mSourceHash = std::string(16, 'a'); mGraph.mBodies[0].mNodeRecord = 8;
        mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, .5f, .5f};
        mGraph.mBodies[0].mBlendController = NifBullet::RagdollBlendControllerDefinition{78, 8, 0xd, 1, 0, 0, 4, {}};
        MWWorld::LiveCellRef<ESM::Static> other(mReference, &mBase); const MWWorld::Ptr consumer(&other);
        ESM4::RuntimeState runtime;
        runtime.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        runtime.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        runtime.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        runtime.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        ESM4::RuntimeActorRagdoll consumerStart;
        const std::array order{NifBullet::RagdollNativeControllerReference{NifBullet::RagdollNativeControllerKind::Blend, 78}};
        {
            MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
            scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
            scheduler.addActorRagdoll(consumer, mGraph, 1, mPoses, 1, -1);
            consumerStart = scheduler.captureActorRagdollSnapshot(consumer, base, model);
            auto& curve = consumerStart.mNativeControllers->mBlends[0].mState;
            curve.mTiming = {0xd, 1, 0, 0, 4}; curve.mClock = {10, 10, 0};
            curve.mKeys = {{0, {1, 0}}, {4, {0, 1}}};
            auto producer = consumerStart; producer.mNativeControllers->mBlends[0].mState.mTiming.mFlags = 0x1d;
            scheduler.restoreActorRagdollSnapshot(mPtr, producer, base, model);
            scheduler.advanceActorRagdollPhysicalControllers(mPtr, order, 11);
            const auto cache = scheduler.captureNativeBlendTimeCache();
            EXPECT_EQ(cache.mCycle, 2u); EXPECT_EQ(cache.mKeyTime, 1.f); EXPECT_EQ(cache.mResult, 3.f);
            runtime.mNativePhysicalBlendTimeCache = cache;
            scheduler.removeActorRagdoll(mPtr); scheduler.removeActorRagdoll(consumer);
            EXPECT_EQ(scheduler.captureNativeBlendTimeCache(), cache); // cache outlives owners
        }
        const auto decoded = ESM4::RuntimeState::deserializeBinary(runtime.serializeBinary());
        ASSERT_TRUE(decoded.mNativePhysicalBlendTimeCache);
        {
            MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
            scheduler.addActorRagdoll(consumer, mGraph, 1, mPoses, 1, -1);
            scheduler.restoreActorRagdollSnapshot(consumer, consumerStart, base, model);
            EXPECT_EQ(scheduler.captureNativeBlendTimeCache().mCycle, 0xffffffffu);
            osg::ref_ptr<osg::Stats> stats = new osg::Stats("shared physical cache save barrier");
            std::vector<MWPhysics::Simulation> frame; float time = 1.f / 60.f;
            scheduler.applyQueuedMovements(time, frame, osg::Timer::instance()->tick(), 0, *stats, MWPhysics::WorldFrameData(false, {}));
            scheduler.restoreNativeBlendTimeCache(*decoded.mNativePhysicalBlendTimeCache);
            ASSERT_EQ(scheduler.captureNativeBlendTimeCache(), *decoded.mNativePhysicalBlendTimeCache);
            const auto before = scheduler.captureActorRagdollSnapshot(consumer, base, model);
            for (unsigned field = 0; field < 4; ++field)
            {
                auto bad = *decoded.mNativePhysicalBlendTimeCache;
                switch (field)
                {
                    case 0: bad.mStopKey = std::numeric_limits<float>::quiet_NaN(); break;
                    case 1: bad.mStartKey = std::numeric_limits<float>::infinity(); break;
                    case 2: bad.mKeyTime = -std::numeric_limits<float>::infinity(); break;
                    case 3: bad.mResult = std::numeric_limits<float>::quiet_NaN(); break;
                }
                EXPECT_THROW(scheduler.restoreNativeBlendTimeCache(bad), std::invalid_argument);
                EXPECT_EQ(scheduler.captureNativeBlendTimeCache(), *decoded.mNativePhysicalBlendTimeCache);
                EXPECT_EQ(scheduler.captureActorRagdollSnapshot(consumer, base, model), before);
            }
            scheduler.advanceActorRagdollPhysicalControllers(consumer, order, 11);
            const auto gains = scheduler.captureActorRagdollBlendStates(consumer)[0].mGains;
            EXPECT_EQ(gains.mHierarchy, .25f); EXPECT_EQ(gains.mVelocity, .75f);
            scheduler.removeActorRagdoll(consumer);
            const ESM4::PhysicalBlendTimeCache raw{0xffffffffu, 1, -1, -0.f, -.25f};
            scheduler.restoreNativeBlendTimeCache(raw);
            EXPECT_EQ(scheduler.captureNativeBlendTimeCache(), raw);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(scheduler.captureNativeBlendTimeCache().mKeyTime), 0x80000000u);
        }
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, GroupSnapshotsPrepareEveryOwnerAndSharedCacheBeforePublication)
    {
        mGraph.mSourceHash = std::string(16, 'a'); mGraph.mBodies[0].mNodeRecord = 8;
        mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, .5f, .5f};
        mGraph.mBodies[0].mBlendController = NifBullet::RagdollBlendControllerDefinition{78, 8, 0xd, 1, 0, 0, 4, {}};
        MWWorld::LiveCellRef<ESM::Static> other(mReference, &mBase); const MWWorld::Ptr second(&other);
        const auto base = ESM::FormKey::content("actors.esm", 100); const std::string model = "characters/_male/skeleton.nif";
        const std::array<MWPhysics::NativeRagdollSnapshotBinding, 2> bindings{{
            {mPtr, ESM::FormKey::content("actors.esm", 20), base, model},
            {second, ESM::FormKey::content("actors.esm", 10), base, model}}};
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        scheduler.addActorRagdoll(second, mGraph, 1, mPoses, 1, -1);
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("group snapshot worker barrier");
        std::vector<MWPhysics::Simulation> frame; float time = 1.f / 60.f;
        scheduler.applyQueuedMovements(time, frame, osg::Timer::instance()->tick(), 0, *stats, MWPhysics::WorldFrameData(false, {}));
        auto group = scheduler.captureActorRagdollSnapshots(bindings);
        ASSERT_EQ(group.mActors.size(), 2u); ASSERT_TRUE(group.mTimeCache);
        EXPECT_EQ(group.mActors.begin()->first, bindings[1].mActor);
        group.mTimeCache = ESM4::PhysicalBlendTimeCache{2, 4, 0, 1, 3};
        unsigned index = 0;
        for (auto& [actor, pose] : group.mActors)
        {
            pose.mBodies[0].mPosition = {10.f + index++, 20, 30};
            pose.mBodies[0].mNativePackedVelocity->mLinear[3] = 8;
            auto& curve = pose.mNativeControllers->mBlends[0].mState;
            curve.mClock = {10, 11, 1}; curve.mCachedGains = {-0.f, -2}; curve.mSetupState = 0xffffffffu;
        }
        scheduler.restoreActorRagdollSnapshots(group, bindings);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshots(bindings), group);
        for (unsigned field = 0; field < 5; ++field)
        {
            auto invalid = group; invalid.mActors.at(bindings[0].mActor).mBodies[0].mPosition = {99, 99, 99};
            auto& last = invalid.mActors.at(bindings[1].mActor);
            if (field == 0) last.mNativeControllers->mBlends[0].mRecord = 99;
            if (field == 1) last.mBodies[0].mNativePackedVelocity->mAngular[3] = std::numeric_limits<float>::quiet_NaN();
            if (field == 2) invalid.mTimeCache->mResult = std::numeric_limits<float>::infinity();
            if (field == 3) invalid.mActors.erase(invalid.mActors.rbegin()->first);
            if (field == 4)
            {
                const auto extra = last; invalid.mActors.erase(bindings[1].mActor);
                invalid.mActors.emplace(ESM::FormKey::content("actors.esm", 99), extra);
            }
            EXPECT_ANY_THROW(scheduler.restoreActorRagdollSnapshots(invalid, bindings));
            EXPECT_EQ(scheduler.captureActorRagdollSnapshots(bindings), group);
        }
        scheduler.removeActorRagdoll(mPtr); scheduler.removeActorRagdoll(second);
    }

    TEST_P(RagdollSchedulerTest, GroupSnapshotsRejectIncompleteDuplicateAndUnknownBindings)
    {
        mGraph.mSourceHash = std::string(16, 'a'); mGraph.mBodies[0].mNodeRecord = 8;
        MWWorld::LiveCellRef<ESM::Static> other(mReference, &mBase); const MWWorld::Ptr second(&other);
        const auto base = ESM::FormKey::content("actors.esm", 100); const std::string model = "characters/_male/skeleton.nif";
        const std::array<MWPhysics::NativeRagdollSnapshotBinding, 2> bindings{{
            {mPtr, ESM::FormKey::content("actors.esm", 10), base, model},
            {second, ESM::FormKey::content("actors.esm", 20), base, model}}};
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        scheduler.addActorRagdoll(second, mGraph, 1, mPoses, 1, -1);
        const auto original = scheduler.captureActorRagdollSnapshots(bindings);
        ASSERT_EQ(original.mActors.size(), 2u);
        EXPECT_THROW(scheduler.captureActorRagdollSnapshots(std::span(bindings).first(1)), std::invalid_argument);
        for (unsigned field = 0; field < 5; ++field)
        {
            auto bad = bindings;
            if (field == 0) bad[1].mActor = bad[0].mActor;
            if (field == 1) bad[1].mPtr = bad[0].mPtr;
            if (field == 2) bad[1].mPtr = {};
            if (field == 3) bad[1].mActor = {};
            if (field == 4) bad[1].mBase = {};
            EXPECT_ANY_THROW(scheduler.captureActorRagdollSnapshots(bad));
            EXPECT_ANY_THROW(scheduler.restoreActorRagdollSnapshots(original, bad));
            EXPECT_EQ(scheduler.captureActorRagdollSnapshots(bindings), original);
        }
        scheduler.removeActorRagdoll(mPtr); scheduler.removeActorRagdoll(second);
    }

    TEST_P(RagdollSchedulerTest, GroupSnapshotsRetainLegacyAbsenceAndNoOwnerCache)
    {
        mGraph.mSourceHash = std::string(16, 'a'); mGraph.mBodies[0].mNodeRecord = 8;
        const auto base = ESM::FormKey::content("actors.esm", 100); const std::string model = "characters/_male/skeleton.nif";
        const std::array<MWPhysics::NativeRagdollSnapshotBinding, 1> bindings{{
            {mPtr, ESM::FormKey::content("actors.esm", 10), base, model}}};
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        const auto current = scheduler.captureActorRagdollSnapshots(bindings);
        ASSERT_EQ(current.mActors.size(), 1u); ASSERT_TRUE(current.mTimeCache);
        auto legacy = current; legacy.mTimeCache.reset();
        auto& pose = legacy.mActors.begin()->second; pose.mNativeControllers.reset(); pose.mNativeBlends.reset();
        pose.mBodies[0].mNativeMotion.reset(); pose.mBodies[0].mNativePackedVelocity.reset();
        pose.mBodies[0].mPosition = {10, 20, 30};
        const ESM4::PhysicalBlendTimeCache retained{0xffffffffu, 1, -1, -0.f, -.25f};
        scheduler.restoreNativeBlendTimeCache(retained);
        scheduler.restoreActorRagdollSnapshots(legacy, bindings);
        EXPECT_EQ(scheduler.captureActorRagdoll(mPtr)[0].mPose.getOrigin(), btVector3(10, 20, 30));
        EXPECT_EQ(scheduler.captureNativeBlendTimeCache(), retained);
        scheduler.removeActorRagdoll(mPtr);
        const auto empty = scheduler.captureActorRagdollSnapshots({});
        EXPECT_TRUE(empty.mActors.empty()); ASSERT_TRUE(empty.mTimeCache);
        EXPECT_EQ(*empty.mTimeCache, retained);
        auto loadedEmpty = empty; loadedEmpty.mTimeCache = ESM4::PhysicalBlendTimeCache{2, 4, 0, 1, 3};
        scheduler.restoreActorRagdollSnapshots(loadedEmpty, {});
        EXPECT_EQ(scheduler.captureNativeBlendTimeCache(), *loadedEmpty.mTimeCache);
    }

    TEST_P(RagdollSchedulerTest, GroupSnapshotWireResumesAllOwnersAndSharedCacheForSixtyExplicitPhases)
    {
        struct World
        {
            btDefaultCollisionConfiguration mConfiguration;
            btCollisionDispatcher mDispatcher{&mConfiguration};
            btDbvtBroadphase mBroadphase;
            btSequentialImpulseConstraintSolver mSolver;
            btDiscreteDynamicsWorld mWorld{&mDispatcher, &mBroadphase, &mSolver, &mConfiguration};
        } fresh;
        mGraph.mSourceHash = std::string(16, 'a'); mGraph.mBodies[0].mNodeRecord = 8;
        mGraph.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{30, 8, .5f, .5f};
        mGraph.mBodies[0].mBlendController = NifBullet::RagdollBlendControllerDefinition{78, 8, 0xd, 1, 0, 0, 4, {}};
        MWWorld::LiveCellRef<ESM::Static> sourceSecond(mReference, &mBase);
        MWWorld::LiveCellRef<ESM::Static> freshFirst(mReference, &mBase), freshSecond(mReference, &mBase);
        const auto base = ESM::FormKey::content("actors.esm", 100); const std::string model = "characters/_male/skeleton.nif";
        const std::array<MWPhysics::NativeRagdollSnapshotBinding, 2> bindings{{
            {mPtr, ESM::FormKey::content("actors.esm", 10), base, model},
            {MWWorld::Ptr(&sourceSecond), ESM::FormKey::content("actors.esm", 20), base, model}}};
        auto freshBindings = bindings; freshBindings[0].mPtr = MWWorld::Ptr(&freshFirst); freshBindings[1].mPtr = MWWorld::Ptr(&freshSecond);
        MWPhysics::PhysicsTaskScheduler source(1.f / 60.f, &mWorld, nullptr);
        MWPhysics::PhysicsTaskScheduler resumed(1.f / 60.f, &fresh.mWorld, nullptr);
        for (const auto& binding : bindings) source.addActorRagdoll(binding.mPtr, mGraph, 1, mPoses, 1, -1);
        for (const auto& binding : freshBindings) resumed.addActorRagdoll(binding.mPtr, mGraph, 1, mPoses, 1, -1);
        auto snapshot = source.captureActorRagdollSnapshots(bindings);
        ASSERT_EQ(snapshot.mActors.size(), 2u);
        snapshot.mTimeCache = ESM4::PhysicalBlendTimeCache{2, 4, 0, 1, 3};
        for (auto& [actor, pose] : snapshot.mActors)
        {
            auto& curve = pose.mNativeControllers->mBlends[0].mState;
            curve.mTiming = {0xd, 1, -0.f, 0, 4}; curve.mClock = {10, 10, 0};
            curve.mKeys = {{0, {1, 0}}, {4, {0, 1}}}; curve.mCachedGains = {-0.f, -2};
            curve.mSetupState = 0xffffffffu;
            ESM4::RuntimeRagdollVelocityController velocity;
            velocity.mAttachedNode = 8; velocity.mTargetNode = 8; velocity.mPrecedesBlend = false;
            velocity.mState.mTiming = {0xd, 1, -0.f, 0, 4}; velocity.mState.mClock = {10, 10, 0};
            velocity.mState.mForceVector = {1, 2, 3, 8};
            pose.mNativeControllers->mVelocities = {velocity};
        }
        source.restoreActorRagdollSnapshots(snapshot, bindings);
        ESM4::RuntimeState runtime;
        runtime.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
        runtime.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
        runtime.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
        runtime.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
        runtime.mNativeActorRagdolls = snapshot.mActors; runtime.mNativePhysicalBlendTimeCache = snapshot.mTimeCache;
        for (const auto& [actor, pose] : snapshot.mActors)
        {
            ESM4::RuntimeReferenceState ref; ref.mKey = actor; ref.mBase = base; ref.mCell = runtime.mPlayer.mCell;
            runtime.mReferences.push_back(ref);
            ESM4::RuntimeActorValues values; values.mActor = actor; values.mBase = base; runtime.mNativeActorValues.push_back(values);
            runtime.mNativeActorLife.push_back({actor, base, ESM4::ActorLifePhase::Dead, 0, {}});
        }
        const auto decoded = ESM4::RuntimeState::deserializeBinary(runtime.serializeBinary());
        const MWPhysics::NativeRagdollSnapshotGroup loaded{decoded.mNativeActorRagdolls, decoded.mNativePhysicalBlendTimeCache};
        resumed.restoreActorRagdollSnapshots(loaded, freshBindings);
        EXPECT_EQ(resumed.captureActorRagdollSnapshots(freshBindings), snapshot);
        const std::array<std::uint32_t, 1> nodes{8};
        // Explicit physical-controller phases; neither Bullet world is stepped.
        // This is a software v36 envelope, not automatic World/gameplay saving.
        for (unsigned frame = 0; frame < 60; ++frame)
        {
            const float input = 11.f + frame * .016f;
            for (unsigned owner = 0; owner < bindings.size(); ++owner)
            {
                const auto order = source.captureActorRagdollControllerOrder(bindings[owner].mPtr, nodes);
                EXPECT_EQ(resumed.captureActorRagdollControllerOrder(freshBindings[owner].mPtr, nodes), order);
                source.advanceActorRagdollPhysicalControllers(bindings[owner].mPtr, order, input);
                resumed.advanceActorRagdollPhysicalControllers(freshBindings[owner].mPtr, order, input);
            }
            const auto expected = source.captureActorRagdollSnapshots(bindings);
            const auto actual = resumed.captureActorRagdollSnapshots(freshBindings);
            EXPECT_EQ(actual, expected);
            for (const auto& binding : bindings)
            {
                const auto& a = actual.mActors.at(binding.mActor).mBodies[0].mNativePackedVelocity;
                const auto& e = expected.mActors.at(binding.mActor).mBodies[0].mNativePackedVelocity;
                for (unsigned lane = 0; lane < 4; ++lane)
                {
                    EXPECT_EQ(std::bit_cast<std::uint32_t>(a->mLinear[lane]), std::bit_cast<std::uint32_t>(e->mLinear[lane]));
                    EXPECT_EQ(std::bit_cast<std::uint32_t>(a->mAngular[lane]), std::bit_cast<std::uint32_t>(e->mAngular[lane]));
                }
            }
            if (frame == 0)
            {
                EXPECT_EQ(actual.mActors.begin()->second.mNativeBlends->front().mHierarchyGain, .25f);
            }
        }
        for (const auto& binding : bindings) source.removeActorRagdoll(binding.mPtr);
        for (const auto& binding : freshBindings) resumed.removeActorRagdoll(binding.mPtr);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0); EXPECT_EQ(fresh.mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, OwnerEnumerationFollowsQueuedWorkRebindingAndRemoval)
    {
        MWWorld::LiveCellRef<ESM::Static> other(mReference, &mBase);
        MWWorld::LiveCellRef<ESM::Static> rebound(mReference, &mBase);
        const MWWorld::Ptr second(&other), updated(&rebound);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        EXPECT_TRUE(scheduler.actorRagdollOwners().empty());
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        scheduler.addActorRagdoll(second, mGraph, 1, mPoses, 1, -1);
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("owner enumeration worker barrier");
        std::vector<MWPhysics::Simulation> frame;
        float time = 1.f / 60.f;
        scheduler.applyQueuedMovements(time, frame, osg::Timer::instance()->tick(), 0,
            *stats, MWPhysics::WorldFrameData(false, {}));
        auto owners = scheduler.actorRagdollOwners();
        ASSERT_EQ(owners.size(), 2u);
        EXPECT_NE(owners[0], owners[1]);
        EXPECT_NE(std::find(owners.begin(), owners.end(), mPtr), owners.end());
        EXPECT_NE(std::find(owners.begin(), owners.end(), second), owners.end());
        scheduler.updateActorRagdollPtr(mPtr, updated);
        owners = scheduler.actorRagdollOwners();
        ASSERT_EQ(owners.size(), 2u);
        EXPECT_EQ(std::find(owners.begin(), owners.end(), mPtr), owners.end());
        EXPECT_NE(std::find(owners.begin(), owners.end(), updated), owners.end());
        EXPECT_NE(std::find(owners.begin(), owners.end(), second), owners.end());
        scheduler.removeActorRagdoll(mPtr); // The stale reference has no owner.
        EXPECT_EQ(scheduler.actorRagdollOwners().size(), 2u);
        scheduler.removeActorRagdoll(updated);
        EXPECT_EQ(scheduler.actorRagdollOwners(), std::vector<MWWorld::Ptr>{second});
        scheduler.removeActorRagdoll(second);
        EXPECT_TRUE(scheduler.actorRagdollOwners().empty());
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, PreparedGroupOwnsStagedProjectionAndOnlyPublishesOnCommit)
    {
        mGraph.mSourceHash = std::string(16, 'a');
        mGraph.mBodies[0].mNodeRecord = 8;
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        const std::string model = "meshes/skeleton.nif";
        const std::array<MWPhysics::NativeRagdollSnapshotBinding, 1> bindings{{
            {mPtr, ESM::FormKey::content("actors.esm", 20),
                ESM::FormKey::content("actors.esm", 100), model}}};
        const auto before = scheduler.captureActorRagdollSnapshots(bindings);
        auto desired = before;
        desired.mActors.begin()->second.mBodies[0].mPosition = {9, 8, 7};
        desired.mActors.begin()->second.mBodies[0].mNativePackedVelocity->mLinear[3] = 8;
        desired.mActors.begin()->second.mBodies[0].mNativePackedVelocity->mAngular[3] = -0.f;
        desired.mTimeCache = ESM4::PhysicalBlendTimeCache{2, 4, 0, 1, 3};
        auto caller = desired;
        auto prepared = scheduler.prepareActorRagdollSnapshots(caller, bindings);
        ASSERT_NE(prepared, nullptr);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshots(bindings), before);
        caller.mActors.begin()->second.mBodies[0].mPosition[0] = std::numeric_limits<float>::quiet_NaN();
        caller.mTimeCache->mResult = std::numeric_limits<float>::infinity();
        scheduler.commitActorRagdollSnapshots(*prepared);
        const auto after = scheduler.captureActorRagdollSnapshots(bindings);
        EXPECT_EQ(after, desired);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(
            after.mActors.begin()->second.mBodies[0].mNativePackedVelocity->mAngular[3]), 0x80000000u);
        EXPECT_THROW(scheduler.commitActorRagdollSnapshots(*prepared), std::invalid_argument);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshots(bindings), after);
    }

    TEST_P(RagdollSchedulerTest, PreparedGroupRejectsLaterRemovedReadmittedOrReboundOwnersBeforePublication)
    {
        mGraph.mSourceHash = std::string(16, 'a');
        mGraph.mBodies[0].mNodeRecord = 8;
        MWWorld::LiveCellRef<ESM::Static> other(mReference, &mBase), rebound(mReference, &mBase);
        const MWWorld::Ptr second(&other), updated(&rebound);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        scheduler.addActorRagdoll(second, mGraph, 1, mPoses, 1, -1);
        const std::string model = "meshes/skeleton.nif";
        const auto base = ESM::FormKey::content("actors.esm", 100);
        std::array<MWPhysics::NativeRagdollSnapshotBinding, 2> bindings{{
            {mPtr, ESM::FormKey::content("actors.esm", 20), base, model},
            {second, ESM::FormKey::content("actors.esm", 10), base, model}}};
        auto desired = scheduler.captureActorRagdollSnapshots(bindings);
        desired.mActors.at(bindings[0].mActor).mBodies[0].mPosition = {99, 99, 99};
        desired.mTimeCache = ESM4::PhysicalBlendTimeCache{2, 4, 0, 1, 3};
        auto prepared = scheduler.prepareActorRagdollSnapshots(desired, bindings);
        ASSERT_NE(prepared, nullptr);
        scheduler.removeActorRagdoll(second);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 1); // The token cannot retain bodies.
        scheduler.addActorRagdoll(second, mGraph, 1, mPoses, 1, -1);
        const auto readmitted = scheduler.captureActorRagdollSnapshots(bindings);
        EXPECT_THROW(scheduler.commitActorRagdollSnapshots(*prepared), std::invalid_argument);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshots(bindings), readmitted);
        prepared = scheduler.prepareActorRagdollSnapshots(desired, bindings);
        ASSERT_NE(prepared, nullptr);
        scheduler.updateActorRagdollPtr(second, updated);
        bindings[1].mPtr = updated;
        const auto reboundState = scheduler.captureActorRagdollSnapshots(bindings);
        EXPECT_THROW(scheduler.commitActorRagdollSnapshots(*prepared), std::invalid_argument);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshots(bindings), reboundState);
        auto invalid = reboundState;
        invalid.mActors.at(bindings[0].mActor).mBodies[0].mPosition = {99, 99, 99};
        invalid.mActors.at(bindings[1].mActor).mBodies[0].mNativePackedVelocity->mAngular[3]
            = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(scheduler.prepareActorRagdollSnapshots(invalid, bindings), std::runtime_error);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshots(bindings), reboundState);
        prepared = scheduler.prepareActorRagdollSnapshots(desired, bindings);
        ASSERT_NE(prepared, nullptr);
        MWWorld::LiveCellRef<ESM::Static> added(mReference, &mBase);
        const MWWorld::Ptr third(&added);
        scheduler.addActorRagdoll(third, mGraph, 1, mPoses, 1, -1);
        const std::array<MWPhysics::NativeRagdollSnapshotBinding, 3> expanded{{
            bindings[0], bindings[1], {third, ESM::FormKey::content("actors.esm", 30), base, model}}};
        const auto expandedState = scheduler.captureActorRagdollSnapshots(expanded);
        EXPECT_THROW(scheduler.commitActorRagdollSnapshots(*prepared), std::invalid_argument);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshots(expanded), expandedState);
        scheduler.removeActorRagdoll(third);
    }

    TEST_P(RagdollSchedulerTest, PreparedEmptyGroupRejectsForeignOrDestroyedSchedulerAndPreservesLegacyCache)
    {
        auto scheduler = std::make_unique<MWPhysics::PhysicsTaskScheduler>(1.f / 60.f, &mWorld, nullptr);
        auto desired = scheduler->captureActorRagdollSnapshots({});
        desired.mTimeCache = ESM4::PhysicalBlendTimeCache{0xffffffffu, 1, -1, -0.f, -.25f};
        auto prepared = scheduler->prepareActorRagdollSnapshots(desired, {});
        ASSERT_NE(prepared, nullptr);
        {
            MWPhysics::PhysicsTaskScheduler other(1.f / 60.f, &mWorld, nullptr);
            const auto before = other.captureActorRagdollSnapshots({});
            EXPECT_THROW(other.commitActorRagdollSnapshots(*prepared), std::invalid_argument);
            EXPECT_EQ(other.captureActorRagdollSnapshots({}), before);
        }
        scheduler->commitActorRagdollSnapshots(*prepared);
        EXPECT_EQ(scheduler->captureActorRagdollSnapshots({}), desired);
        auto legacy = desired;
        legacy.mTimeCache.reset();
        auto legacyPrepared = scheduler->prepareActorRagdollSnapshots(legacy, {});
        ASSERT_NE(legacyPrepared, nullptr);
        scheduler->commitActorRagdollSnapshots(*legacyPrepared);
        EXPECT_EQ(scheduler->captureActorRagdollSnapshots({}), desired);
        prepared = scheduler->prepareActorRagdollSnapshots(desired, {});
        ASSERT_NE(prepared, nullptr);
        scheduler.reset();
        scheduler = std::make_unique<MWPhysics::PhysicsTaskScheduler>(1.f / 60.f, &mWorld, nullptr);
        const auto replacement = scheduler->captureActorRagdollSnapshots({});
        EXPECT_THROW(scheduler->commitActorRagdollSnapshots(*prepared), std::invalid_argument);
        EXPECT_EQ(scheduler->captureActorRagdollSnapshots({}), replacement);
        for (unsigned field = 0; field < 4; ++field)
        {
            auto bad = replacement;
            const auto nan = std::numeric_limits<float>::quiet_NaN();
            if (field == 0) bad.mTimeCache->mStopKey = nan;
            if (field == 1) bad.mTimeCache->mStartKey = nan;
            if (field == 2) bad.mTimeCache->mKeyTime = nan;
            if (field == 3) bad.mTimeCache->mResult = nan;
            EXPECT_THROW(scheduler->prepareActorRagdollSnapshots(bad, {}), std::invalid_argument);
            EXPECT_EQ(scheduler->captureActorRagdollSnapshots({}), replacement);
        }
    }

    INSTANTIATE_TEST_SUITE_P(WorkerCounts, RagdollSchedulerTest, ::testing::Values(0, 1, 2));

}

namespace
{
    class LooseAdmissionFailureWorld : public NifBullet::NativeDynamicsWorld
    {
    public:
        using NifBullet::NativeDynamicsWorld::NativeDynamicsWorld;
        bool mFailAfterInsertion = true;
        void addRigidBody(btRigidBody* body, int group, int mask) override
        {
            NifBullet::NativeDynamicsWorld::addRigidBody(body, group, mask);
            if (mFailAfterInsertion) throw std::runtime_error("loose fixture admission failed");
        }
    };

    TEST_P(RagdollSchedulerTest, LooseBodyPreparationRejectsStaleAndDuplicateOwnersWithoutActorPublication)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        NifBullet::NativeDynamicsWorld foreignWorld(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        MWPhysics::PhysicsTaskScheduler foreign(1.f / 60.f, &foreignWorld, nullptr);
        {
            auto cancelled = scheduler.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
            EXPECT_TRUE(scheduler.validatePreparedLooseObject(*cancelled));
            EXPECT_FALSE(scheduler.hasLooseObject(mPtr));
            EXPECT_TRUE(scheduler.looseObjectOwners().empty());
            EXPECT_TRUE(scheduler.actorRagdollOwners().empty());
            EXPECT_EQ(world.getNumCollisionObjects(), 0);
            EXPECT_FALSE(foreign.commitLooseObject(*cancelled));
            EXPECT_EQ(foreignWorld.getNumCollisionObjects(), 0);
        }
        auto first = scheduler.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
        auto second = scheduler.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
        mPtr.getCellRef().setCount(2);
        EXPECT_FALSE(scheduler.commitLooseObject(*first));
        EXPECT_EQ(world.getNumCollisionObjects(), 0);
        mPtr.getCellRef().setCount(1);
        ASSERT_TRUE(scheduler.commitLooseObject(*first));
        EXPECT_FALSE(scheduler.commitLooseObject(*first));
        EXPECT_FALSE(scheduler.validatePreparedLooseObject(*second));
        EXPECT_FALSE(scheduler.commitLooseObject(*second));
        EXPECT_EQ(scheduler.looseObjectOwners(), std::vector<MWWorld::Ptr>{mPtr});
        EXPECT_TRUE(scheduler.actorRagdollOwners().empty());
        EXPECT_THROW(scheduler.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1), std::invalid_argument);
        EXPECT_THROW(scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1), std::invalid_argument);
        MWWorld::LiveCellRef<ESM::Static> other(mReference, &mBase);
        const MWWorld::Ptr otherPtr(&other);
        scheduler.addActorRagdoll(otherPtr, mGraph, 1, mPoses, 1, -1);
        EXPECT_THROW(scheduler.updateActorRagdollPtr(otherPtr, mPtr), std::invalid_argument);
        EXPECT_TRUE(scheduler.hasActorRagdoll(otherPtr));
        EXPECT_TRUE(scheduler.hasLooseObject(mPtr));
        EXPECT_EQ(world.getNumCollisionObjects(), 2);
        scheduler.removeActorRagdoll(otherPtr);

        ASSERT_EQ(world.getNumCollisionObjects(), 1);
        const auto* body = world.getCollisionObjectArray()[0];
        auto* holder = static_cast<MWPhysics::PtrHolder*>(scheduler.getUserPointer(body));
        ASSERT_NE(holder, nullptr);
        EXPECT_EQ(holder->getPtr(), mPtr);
        scheduler.removeLooseObject(mPtr);
        EXPECT_EQ(world.getNumCollisionObjects(), 0);
        EXPECT_EQ(scheduler.getUserPointer(body), nullptr);
        EXPECT_THROW(scheduler.captureLooseObject(mPtr), std::invalid_argument);
        EXPECT_THROW(scheduler.prepareLooseObject({}, mGraph, 1, mPoses, 1, -1), std::invalid_argument);
        auto bad = mGraph;
        bad.mBodies.push_back(bad.mBodies[0]);
        EXPECT_THROW(scheduler.prepareLooseObject(mPtr, bad, 1, mPoses, 1, -1), std::invalid_argument);
        bad = mGraph;
        bad.mBodies[0].mBlend = NifBullet::RagdollBlendDefinition{};
        EXPECT_THROW(scheduler.prepareLooseObject(mPtr, bad, 1, mPoses, 1, -1), std::invalid_argument);
        MWPhysics::PhysicsTaskScheduler legacy(1.f / 60.f, &mWorld, nullptr);
        EXPECT_THROW(legacy.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1), std::invalid_argument);
    }

    TEST_P(RagdollSchedulerTest, LooseBodiesFallOntoRealFloorWithNoActorRagdollsAndCleanUpOnUnload)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        world.setGravity(btVector3(0, 0, 0));
        btStaticPlaneShape floorShape(btVector3(0, 0, 1), 0);
        btCollisionObject floor;
        floor.setCollisionShape(&floorShape);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        scheduler.addCollisionObject(&floor, 1, -1);
        const auto removeFloor = [&](btCollisionObject* object) { scheduler.removeCollisionObject(object); };
        std::unique_ptr<btCollisionObject, decltype(removeFloor)> floorGuard(&floor, removeFloor);
        auto prepared = scheduler.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
        ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
        ASSERT_EQ(world.getNumCollisionObjects(), 2);
        auto* body = btRigidBody::upcast(world.getCollisionObjectArray()[1]);
        ASSERT_NE(body, nullptr);
        EXPECT_TRUE(body->getFlags() & BT_DISABLE_WORLD_GRAVITY);
        const auto* identity = body;
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("loose-body floor");
        std::array<std::vector<MWPhysics::Simulation>, 2> frames;
        float time = 0;
        for (unsigned frame = 0; frame < 180; ++frame)
        {
            time += 1.f / 60.f;
            scheduler.applyQueuedMovements(time, frames[frame % 2], osg::Timer::instance()->tick(), frame,
                *stats, MWPhysics::WorldFrameData(false, {}));
        }
        const auto state = scheduler.captureLooseObject(mPtr);
        ASSERT_EQ(state.size(), 1);
        EXPECT_NEAR(state[0].mPose.getOrigin().z(), .5, .03);
        EXPECT_LT(std::abs(state[0].mLinearVelocity.z()), .2);
        EXPECT_EQ(world.getGravity(), btVector3(0, 0, 0));
        EXPECT_TRUE(scheduler.actorRagdollOwners().empty());
        scheduler.releaseSharedStates();
        EXPECT_TRUE(scheduler.looseObjectOwners().empty());
        EXPECT_EQ(world.getNumCollisionObjects(), 1);
        EXPECT_EQ(scheduler.getUserPointer(identity), nullptr);
        EXPECT_EQ(world.getNumConstraints(), 0);
    }

    TEST_P(RagdollSchedulerTest, MixedLooseAndActorBodiesShareExactlyOneNativeDynamicsStep)
    {
        constexpr float dt = 1.f / 60.f;
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        world.setGravity(btVector3(0, 0, 0));
        MWWorld::LiveCellRef<ESM::Static> actorRef(mReference, &mBase);
        const MWWorld::Ptr actor(&actorRef);
        auto actorPoses = mPoses;
        actorPoses[0].setOrigin(btVector3(100, 0, 2));
        MWPhysics::PhysicsTaskScheduler scheduler(dt, &world, nullptr);
        mGraph.mBodies[0].mLinearDamping = 2;
        scheduler.addActorRagdoll(actor, mGraph, 1, actorPoses, 1, -1);
        auto loose = scheduler.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
        ASSERT_TRUE(scheduler.commitLooseObject(*loose));
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("shared native dynamics step");
        std::vector<MWPhysics::Simulation> frames;
        float time = dt;
        scheduler.applyQueuedMovements(time, frames, osg::Timer::instance()->tick(), 0,
            *stats, MWPhysics::WorldFrameData(false, {}));
        const auto a = scheduler.captureActorRagdoll(actor)[0];
        const auto b = scheduler.captureLooseObject(mPtr)[0];
        // Independently pinned original native gravity, separate float stores.
        constexpr float nativeGravity = -73.57500457763672f;
        const float delta = float(double(nativeGravity) * double(dt));
        const float factor = float(1. - double(dt) * 2.);
        const float velocity = float(double(delta) * double(factor));
        EXPECT_DOUBLE_EQ(a.mLinearVelocity.z(), velocity);
        EXPECT_DOUBLE_EQ(b.mLinearVelocity.z(), velocity);
        EXPECT_NEAR(a.mPose.getOrigin().z(), 2 + double(velocity) * double(dt), 1e-12);
        EXPECT_NEAR(b.mPose.getOrigin().z(), 2 + double(velocity) * double(dt), 1e-12);
        EXPECT_EQ(scheduler.actorRagdollOwners(), std::vector<MWWorld::Ptr>{actor});
        EXPECT_EQ(scheduler.looseObjectOwners(), std::vector<MWWorld::Ptr>{mPtr});
        scheduler.removeLooseObject(mPtr);
        scheduler.removeActorRagdoll(actor);
        EXPECT_EQ(world.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, LooseBodySnapshotsValidateWholeStateAndResumeTheSameFreeFlight)
    {
        constexpr float dt = 1.f / 60.f;
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        world.setGravity(btVector3(0, 0, 0));
        mGraph.mBodies[0].mLinearDamping = .1f;
        mPoses[0].setOrigin(btVector3(1, 2, 500));
        MWPhysics::PhysicsTaskScheduler scheduler(dt, &world, nullptr);
        auto admitted = scheduler.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
        ASSERT_TRUE(scheduler.commitLooseObject(*admitted));
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("loose-body snapshot continuation");
        std::array<std::vector<MWPhysics::Simulation>, 2> frames;
        float time = 0;
        const auto advance = [&](unsigned start, unsigned stop) {
            for (unsigned frame = start; frame < stop; ++frame)
            {
                time += dt;
                scheduler.applyQueuedMovements(time, frames[frame % 2], osg::Timer::instance()->tick(), frame,
                    *stats, MWPhysics::WorldFrameData(false, {}));
            }
        };
        advance(0, 30);
        const auto saved = scheduler.captureLooseObject(mPtr);
        const auto packed = scheduler.captureLooseObjectPackedVelocities(mPtr);
        auto bad = saved;
        bad[0].mPose.getOrigin().setX(std::numeric_limits<btScalar>::quiet_NaN());
        EXPECT_THROW(scheduler.restoreLooseObject(mPtr, bad, packed), std::invalid_argument);
        EXPECT_EQ(scheduler.captureLooseObject(mPtr)[0].mPose, saved[0].mPose);
        auto badPacked = packed;
        badPacked[0].mRecord += 1;
        EXPECT_THROW(scheduler.restoreLooseObject(mPtr, saved, badPacked), std::invalid_argument);
        EXPECT_EQ(scheduler.captureLooseObjectPackedVelocities(mPtr)[0].mVelocities, packed[0].mVelocities);
        advance(30, 60);
        const auto uninterrupted = scheduler.captureLooseObject(mPtr)[0];
        scheduler.removeLooseObject(mPtr);
        admitted = scheduler.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
        ASSERT_TRUE(scheduler.commitLooseObject(*admitted));
        scheduler.restoreLooseObject(mPtr, saved, packed);
        advance(60, 90);
        const auto resumed = scheduler.captureLooseObject(mPtr)[0];
        EXPECT_NEAR(resumed.mPose.getOrigin().z(), uninterrupted.mPose.getOrigin().z(), 1e-9);
        EXPECT_EQ(resumed.mLinearVelocity, uninterrupted.mLinearVelocity);
        EXPECT_EQ(resumed.mAngularVelocity, uninterrupted.mAngularVelocity);
        EXPECT_EQ(world.getNumCollisionObjects(), 1);
        scheduler.removeLooseObject(mPtr);
    }

    TEST_P(RagdollSchedulerTest, LooseAdmissionFailureRemovesRoutingAndPhysicsBeforeRetry)
    {
        LooseAdmissionFailureWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        world.setGravity(btVector3(0, 0, 0));
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        auto prepared = scheduler.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
        EXPECT_THROW(scheduler.commitLooseObject(*prepared), std::runtime_error);
        EXPECT_EQ(world.getNumCollisionObjects(), 0);
        EXPECT_TRUE(scheduler.looseObjectOwners().empty());
        EXPECT_TRUE(scheduler.actorRagdollOwners().empty());
        EXPECT_TRUE(scheduler.validatePreparedLooseObject(*prepared));
        world.mFailAfterInsertion = false;
        ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
        ASSERT_EQ(world.getNumCollisionObjects(), 1);
        auto* body = world.getCollisionObjectArray()[0];
        ASSERT_NE(scheduler.getUserPointer(body), nullptr);
        scheduler.removeLooseObject(mPtr);
        EXPECT_EQ(scheduler.getUserPointer(body), nullptr);
        EXPECT_EQ(world.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, PendingLooseBodiesRejectClearedAndReusedSchedulersBeforeTouchingDeletedReferences)
    {
        using Scheduler = MWPhysics::PhysicsTaskScheduler;
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        alignas(Scheduler) std::byte storage[sizeof(Scheduler)];
        auto* scheduler = new (storage) Scheduler(1.f / 60.f, &world, nullptr);
        auto cleared = scheduler->prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
        scheduler->releaseSharedStates();
        EXPECT_FALSE(scheduler->validatePreparedLooseObject(*cleared));
        EXPECT_FALSE(scheduler->commitLooseObject(*cleared));
        auto retired = scheduler->prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
        scheduler->~Scheduler();
        mLive.reset(); // Borrowed ptr is now invalid; reject identity first.
        scheduler = new (storage) Scheduler(1.f / 60.f, &world, nullptr);
        EXPECT_FALSE(scheduler->validatePreparedLooseObject(*retired));
        EXPECT_FALSE(scheduler->commitLooseObject(*retired));
        EXPECT_FALSE(scheduler->validatePreparedLooseObject(*cleared));
        EXPECT_FALSE(scheduler->commitLooseObject(*cleared));
        cleared.reset();
        retired.reset();
        EXPECT_EQ(world.getNumCollisionObjects(), 0);
        scheduler->~Scheduler();
    }
}

namespace
{
    TEST_P(RagdollSchedulerTest, LooseOwnerTransferKeepsBodyAndRetiresPendingOldReferenceAdmission)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        MWWorld::LiveCellRef<ESM::Static> newRef(mReference, &mBase), occupiedRef(mReference, &mBase);
        const MWWorld::Ptr updated(&newRef), occupied(&occupiedRef);
        auto first = scheduler.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
        auto stale = scheduler.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
        ASSERT_TRUE(scheduler.commitLooseObject(*first));
        auto other = scheduler.prepareLooseObject(occupied, mGraph, 1, mPoses, 1, -1);
        ASSERT_TRUE(scheduler.commitLooseObject(*other));
        auto* body = scheduler.looseObjectCollisionObject(mPtr);
        const auto saved = scheduler.captureLooseObject(mPtr)[0];
        EXPECT_THROW(scheduler.updateLooseObjectPtr(mPtr, {}), std::invalid_argument);
        EXPECT_THROW(scheduler.updateLooseObjectPtr(mPtr, occupied), std::invalid_argument);
        EXPECT_TRUE(scheduler.hasLooseObject(mPtr));
        EXPECT_EQ(scheduler.looseObjectCollisionObject(mPtr), body);
        scheduler.updateLooseObjectPtr(mPtr, updated);
        EXPECT_FALSE(scheduler.hasLooseObject(mPtr));
        EXPECT_TRUE(scheduler.hasLooseObject(updated));
        EXPECT_EQ(scheduler.looseObjectCollisionObject(updated), body);
        EXPECT_EQ(static_cast<MWPhysics::PtrHolder*>(scheduler.getUserPointer(body))->getPtr(), updated);
        EXPECT_EQ(scheduler.captureLooseObject(updated)[0].mPose, saved.mPose);
        EXPECT_EQ(scheduler.captureLooseObject(updated)[0].mLinearVelocity, saved.mLinearVelocity);
        mLive.reset();
        EXPECT_FALSE(scheduler.validatePreparedLooseObject(*stale));
        EXPECT_FALSE(scheduler.commitLooseObject(*stale));
        stale.reset();
        scheduler.removeLooseObject(updated);
        scheduler.removeLooseObject(occupied);
        EXPECT_EQ(world.getNumCollisionObjects(), 0);
        EXPECT_EQ(scheduler.getUserPointer(body), nullptr);
    }

    TEST_P(RagdollSchedulerTest, PhysicsSystemOwnsLooseAdmissionRayRoutingTransferAndRemoval)
    {
        VFS::Manager vfs;
        Resource::ResourceSystem resources(&vfs, 0., nullptr);
        MWPhysics::PhysicsSystem physics(&resources, new osg::Group);
        MWPhysics::PhysicsSystem foreign(&resources, new osg::Group);
        const osg::Vec3f from(-2, 0, 2), to(2, 0, 2);
        {
            auto cancelled = physics.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
            EXPECT_TRUE(physics.validatePreparedLooseObject(*cancelled));
            EXPECT_FALSE(foreign.commitLooseObject(*cancelled));
            EXPECT_FALSE(physics.castRay(from, to).mHit);
            EXPECT_THROW(physics.captureLooseObject(mPtr), std::invalid_argument);
        }
        auto prepared = physics.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
        ASSERT_TRUE(physics.commitLooseObject(*prepared));
        EXPECT_FALSE(physics.commitLooseObject(*prepared));
        const VFS::Path::Normalized missingModel("does-not-exist.nif");
        EXPECT_THROW(physics.addObject(mPtr, missingModel, osg::Quat(), 1), std::invalid_argument);
        EXPECT_THROW(physics.addActor(mPtr, missingModel), std::invalid_argument);
        const auto hit = physics.castRay(from, to);
        ASSERT_TRUE(hit.mHit);
        EXPECT_EQ(hit.mHitObject, mPtr);
        EXPECT_NEAR(hit.mHitPos.x(), -.5, 1e-6);
        EXPECT_FALSE(physics.castRay(from, to, {mPtr}).mHit);
        EXPECT_EQ(physics.getObject(mPtr), nullptr);
        EXPECT_EQ(physics.looseObjectOwners(), std::vector<MWWorld::Ptr>{mPtr});
        const auto saved = physics.captureLooseObject(mPtr);
        const auto packed = physics.captureLooseObjectPackedVelocities(mPtr);
        auto wrong = packed;
        wrong[0].mRecord += 1;
        EXPECT_THROW(physics.restoreLooseObject(mPtr, saved, wrong), std::invalid_argument);
        physics.restoreLooseObject(mPtr, saved, packed);
        MWWorld::LiveCellRef<ESM::Static> newRef(mReference, &mBase);
        const MWWorld::Ptr updated(&newRef);
        physics.updatePtr(mPtr, updated);
        EXPECT_FALSE(physics.hasLooseObject(mPtr));
        EXPECT_TRUE(physics.hasLooseObject(updated));
        mLive.reset();
        EXPECT_EQ(physics.castRay(from, to).mHitObject, updated);
        EXPECT_FALSE(physics.castRay(from, to, {updated}).mHit);
        EXPECT_EQ(physics.captureLooseObject(updated)[0].mPose, saved[0].mPose);
        physics.remove(updated);
        EXPECT_FALSE(physics.hasLooseObject(updated));
        EXPECT_TRUE(physics.looseObjectOwners().empty());
        EXPECT_FALSE(physics.castRay(from, to).mHit);
        EXPECT_THROW(physics.captureLooseObject(updated), std::invalid_argument);
        physics.remove(updated);
    }

    TEST_P(RagdollSchedulerTest, PhysicsSystemRetiredLooseTokenRejectsReusedOwnerBeforeDeletedReference)
    {
        VFS::Manager vfs;
        Resource::ResourceSystem resources(&vfs, 0., nullptr);
        using Physics = MWPhysics::PhysicsSystem;
        alignas(Physics) std::byte storage[sizeof(Physics)];
        auto* physics = new (storage) Physics(&resources, new osg::Group);
        auto retired = physics->prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
        physics->~Physics();
        mLive.reset();
        physics = new (storage) Physics(&resources, new osg::Group);
        EXPECT_FALSE(physics->validatePreparedLooseObject(*retired));
        EXPECT_FALSE(physics->commitLooseObject(*retired));
        retired.reset();
        EXPECT_TRUE(physics->looseObjectOwners().empty());
        physics->~Physics();
    }
}
