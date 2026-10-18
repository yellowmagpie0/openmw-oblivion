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
#include <components/nifbullet/ragdollbonepose.hpp>
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
#include <components/esm4/physicalframe.hpp>
#include <components/nif/nativetransformmath.hpp>
#include <apps/openmw/mwphysics/projectile.hpp>
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
                case 2:
                    invalid.mBodies[0].mRecord = 99;
                    invalid.mWorldRegistration->mBodies = {99};
                    break;
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
        (*snapshot.mNativeBlends)[0] = {6, 0xf123, 0xffffffffu, -2.f, 3.f,
            ESM4::RuntimeRagdollBlendRegistration{17, true}};
        (*snapshot.mNativeBlends)[1] = {12, 8, 1, -0.f, 0.f,
            ESM4::RuntimeRagdollBlendRegistration{-9, false}};
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
        pose.mWorldRegistration.reset();
        scheduler.setActorRagdollWorldRegistrationSelection(mPtr, {});
        pose.mBodies[0].mNativeMotion.reset(); pose.mBodies[0].mNativePackedVelocity.reset();
        pose.mBodies[0].mPosition = {10, 20, 30};
        const ESM4::PhysicalBlendTimeCache retained{0xffffffffu, 1, -1, -0.f, -.25f};
        scheduler.restoreNativeBlendTimeCache(retained);
        scheduler.restoreActorRagdollSnapshots(legacy, bindings);
        EXPECT_EQ(scheduler.captureActorRagdoll(mPtr)[0].mPose.getOrigin(), btVector3(10, 20, 30));
        EXPECT_TRUE(scheduler.captureActorRagdollWorldRegistrationSelection(mPtr).mBodies.empty());
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
        source.setActorRagdollWorldRegistrationSelection(bindings[0].mPtr, {});
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
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 1);
        EXPECT_EQ(fresh.mWorld.getNumCollisionObjects(), 1);
        EXPECT_TRUE(resumed.captureActorRagdollWorldRegistrationSelection(freshBindings[0].mPtr).mBodies.empty());
        const std::array<std::uint32_t, 1> nodes{8};
        // Explicit physical-controller phases; neither Bullet world is stepped.
        // This is a current software envelope, not automatic World/gameplay saving.
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

    TEST_P(RagdollSchedulerTest, GroupRegistrationFailureRollsBackEveryOwnerAndCanRetry)
    {
        LooseAdmissionFailureWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        world.mFailAfterInsertion = false;
        mGraph.mSourceHash = std::string(16, 'a'); mGraph.mBodies[0].mNodeRecord = 8;
        MWWorld::LiveCellRef<ESM::Static> other(mReference, &mBase);
        const MWWorld::Ptr second(&other);
        const auto base = ESM::FormKey::content("actors.esm", 100);
        const std::string model = "characters/_male/skeleton.nif";
        const std::array<MWPhysics::NativeRagdollSnapshotBinding, 2> bindings{{
            {mPtr, ESM::FormKey::content("actors.esm", 10), base, model},
            {second, ESM::FormKey::content("actors.esm", 20), base, model}}};
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        scheduler.addActorRagdoll(second, mGraph, 1, mPoses, 1, -1);
        const auto* firstBody = world.getCollisionObjectArray()[0];
        const auto* secondBody = world.getCollisionObjectArray()[1];
        const auto registered = scheduler.captureActorRagdollWorldRegistrationSelection(second);
        scheduler.setActorRagdollWorldRegistrationSelection(second, {});
        const auto before = scheduler.captureActorRagdollSnapshots(bindings);
        auto desired = before;
        desired.mActors.at(bindings[0].mActor).mWorldRegistration = ESM4::RuntimeRagdollWorldRegistration{};
        desired.mActors.at(bindings[1].mActor).mWorldRegistration
            = ESM4::RuntimeRagdollWorldRegistration{registered.mBodies, registered.mConstraints};
        desired.mActors.at(bindings[0].mActor).mBodies[0].mPosition = {10, 0, 2};
        desired.mActors.at(bindings[1].mActor).mBodies[0].mPosition = {20, 0, 2};
        desired.mTimeCache = ESM4::PhysicalBlendTimeCache{2, 4, 0, 1, 3};
        auto prepared = scheduler.prepareActorRagdollSnapshots(desired, bindings);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshots(bindings), before);
        world.mFailAfterInsertion = true;
        EXPECT_THROW(scheduler.commitActorRagdollSnapshots(*prepared), std::runtime_error);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshots(bindings), before);
        ASSERT_EQ(world.getNumCollisionObjects(), 1);
        EXPECT_EQ(world.getCollisionObjectArray()[0], firstBody);
        EXPECT_NE(firstBody->getBroadphaseHandle(), nullptr);
        EXPECT_EQ(secondBody->getBroadphaseHandle(), nullptr);
        world.mFailAfterInsertion = false;
        scheduler.commitActorRagdollSnapshots(*prepared);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshots(bindings), desired);
        ASSERT_EQ(world.getNumCollisionObjects(), 1);
        EXPECT_EQ(world.getCollisionObjectArray()[0], secondBody);
        EXPECT_EQ(firstBody->getBroadphaseHandle(), nullptr);

        // A later owner's selection round trip invalidates the entire group,
        // even though its final selection equals the prepared baseline.
        auto stale = scheduler.prepareActorRagdollSnapshots(before, bindings);
        scheduler.setActorRagdollWorldRegistrationSelection(second, {});
        scheduler.setActorRagdollWorldRegistrationSelection(second, registered);
        EXPECT_THROW(scheduler.commitActorRagdollSnapshots(*stale), std::invalid_argument);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshots(bindings), desired);
        ASSERT_EQ(world.getNumCollisionObjects(), 1);
        EXPECT_EQ(world.getCollisionObjectArray()[0], secondBody);
    }

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

namespace
{
    TEST_P(RagdollSchedulerTest, LooseNativeRootPoseReversesOwnedBodyAndBindTransforms)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        auto& body = mGraph.mBodies.front();
        body.mNodeRecord = 3;
        body.mBoneBind = osg::Matrixf::rotate(osg::PI_2, osg::Vec3f(0, 0, 1))
            * osg::Matrixf::translate(30, -7, 15);
        body.mUsesRigidBodyTransform = true;
        body.mTranslation = {.5f, .25f, -.75f};
        body.mRotation = osg::Quat(osg::PI_2, osg::Vec3f(1, 0, 0));
        const auto root = osg::Matrixf::rotate(osg::PI_2, osg::Vec3f(0, 1, 0))
            * osg::Matrixf::translate(100, 40, 80);
        const std::array<NifBullet::RagdollBoneWorldPose, 1> bones{{
            {body.mNodeRecord, body.mBoneBind * root}}};
        const auto poses = NifBullet::ragdollBodyWorldPoses(mGraph, bones);
        auto prepared = scheduler.prepareLooseObject(mPtr, mGraph,
            NifBullet::RagdollNativeLengthScale, poses, 1, -1);
        // The authored file/definition may be released or reused after preparation.
        body.mBoneBind = osg::Matrixf::scale(2, 3, 4);
        body.mTranslation = {100, 100, 100};
        body.mRotation = {};
        ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
        const auto check = [&](const osg::Matrixf& expected) {
            const auto actual = scheduler.captureLooseObjectRootPose(mPtr);
            for (unsigned i = 0; i < 16; ++i)
                EXPECT_NEAR(actual.ptr()[i], expected.ptr()[i], .0003f) << i;
        };
        check(root);
        auto state = scheduler.captureLooseObject(mPtr);
        const auto velocities = scheduler.captureLooseObjectPackedVelocities(mPtr);
        state.front().mPose.setOrigin(state.front().mPose.getOrigin() + btVector3(10, 20, 30));
        scheduler.restoreLooseObject(mPtr, state, velocities);
        auto moved = root;
        moved.setTrans(root.getTrans() + osg::Vec3f(10, 20, 30));
        check(moved);
        const auto before = scheduler.captureLooseObject(mPtr);
        check(moved);
        const auto after = scheduler.captureLooseObject(mPtr);
        EXPECT_EQ(before.front().mPose, after.front().mPose);
        EXPECT_EQ(before.front().mLinearVelocity, after.front().mLinearVelocity);
        mPtr.getCellRef().setScale(2.f);
        EXPECT_THROW(scheduler.captureLooseObjectRootPose(mPtr), std::invalid_argument);
        EXPECT_THROW(scheduler.updateLooseObjectRootPose(mPtr, root), std::invalid_argument);
        EXPECT_EQ(scheduler.captureLooseObject(mPtr)[0].mPose, after.front().mPose);
        scheduler.removeLooseObject(mPtr);
        EXPECT_THROW(scheduler.captureLooseObjectRootPose(mPtr), std::invalid_argument);
    }

    TEST_P(RagdollSchedulerTest, LooseNativeRootPoseRejectsUnsupportedUnitsAndSceneBindingsWithoutMutation)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        for (const bool invalidBind : {false, true})
        {
            SCOPED_TRACE(invalidBind);
            mGraph.mBodies.front().mBoneBind = invalidBind
                ? osg::Matrixf::scale(2, 1, 1) : osg::Matrixf::identity();
            auto prepared = scheduler.prepareLooseObject(mPtr, mGraph,
                invalidBind ? NifBullet::RagdollNativeLengthScale : 1.f, mPoses, 1, -1);
            ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
            const auto before = scheduler.captureLooseObject(mPtr);
            EXPECT_THROW(scheduler.captureLooseObjectRootPose(mPtr), std::invalid_argument);
            const auto after = scheduler.captureLooseObject(mPtr);
            EXPECT_EQ(before.front().mPose, after.front().mPose);
            EXPECT_EQ(before.front().mLinearVelocity, after.front().mLinearVelocity);
            EXPECT_EQ(world.getNumCollisionObjects(), 1);
            scheduler.removeLooseObject(mPtr);
        }
    }
}

namespace
{
    TEST_P(RagdollSchedulerTest, ForcedLoosePoseRefreshesBoundsAndRetainsForcesIdentityAndSmallVelocities)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher,&mBroadphase,&mSolver,&mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f/60.f,&world,nullptr);
        auto prepared=scheduler.prepareLooseObject(mPtr,mGraph,NifBullet::RagdollNativeLengthScale,mPoses,1,-1);
        ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
        auto packed=scheduler.captureLooseObjectPackedVelocities(mPtr);
        packed[0].mVelocities={{2,0,0,99},{-0.f,.0005f,0,-99}};
        const auto initial=scheduler.captureLooseObject(mPtr);
        scheduler.restoreLooseObject(mPtr,initial,packed);
        auto* body=btRigidBody::upcast(scheduler.looseObjectCollisionObject(mPtr)); ASSERT_NE(body,nullptr);
        body->applyCentralForce(btVector3(3,4,5));
        const auto original=scheduler.captureLooseObject(mPtr)[0];
        const auto root=osg::Matrixf::translate(30,0,2);
        auto invalid=root; invalid(0,0)=2;
        EXPECT_THROW(scheduler.updateLooseObjectRootPose(mPtr,invalid),std::invalid_argument);
        EXPECT_EQ(scheduler.captureLooseObject(mPtr)[0].mPose,original.mPose);
        EXPECT_EQ(scheduler.captureLooseObjectPackedVelocities(mPtr)[0].mVelocities,packed[0].mVelocities);
        scheduler.updateLooseObjectRootPose(mPtr,root);
        EXPECT_EQ(scheduler.looseObjectCollisionObject(mPtr),body);
        EXPECT_EQ(body->getTotalForce(),btVector3(3,4,5));
        EXPECT_EQ(body->getInterpolationWorldTransform(),body->getWorldTransform());
        EXPECT_EQ(body->getLinearVelocity(),btVector3(0,0,0));
        const auto after=scheduler.captureLooseObjectPackedVelocities(mPtr)[0].mVelocities;
        EXPECT_EQ(after.mLinear,(std::array<float,4>{0,0,0,0}));
        for (unsigned axis=0;axis<4;++axis)
            EXPECT_EQ(std::bit_cast<unsigned>(after.mAngular[axis]),std::bit_cast<unsigned>(packed[0].mVelocities.mAngular[axis]));
        btCollisionWorld::ClosestRayResultCallback hit(btVector3(20,0,2),btVector3(40,0,2));
        world.rayTest(hit.m_rayFromWorld,hit.m_rayToWorld,hit);
        EXPECT_TRUE(hit.hasHit()); EXPECT_EQ(hit.m_collisionObject,body);
        btCollisionWorld::ClosestRayResultCallback old(btVector3(-10,0,2),btVector3(10,0,2));
        world.rayTest(old.m_rayFromWorld,old.m_rayToWorld,old); EXPECT_FALSE(old.hasHit());
        scheduler.removeLooseObject(mPtr);
    }

    TEST_P(RagdollSchedulerTest, PhysicsPositionAndRotationDispatchMoveOrdinaryAndBodyTLooseOwners)
    {
        for (bool bodyT : {false,true})
        {
            SCOPED_TRACE(bodyT);
            VFS::Manager vfs; Resource::ResourceSystem resources(&vfs,0.,nullptr);
            MWPhysics::PhysicsSystem physics(&resources,new osg::Group);
            auto graph=mGraph;
            auto& body=graph.mBodies.front(); body.mNodeRecord=3;
            body.mBoneBind=osg::Matrixf::rotate(osg::PI_2,osg::Vec3f(0,0,1))*osg::Matrixf::translate(3,4,5);
            body.mUsesRigidBodyTransform=bodyT;
            body.mTranslation={.25f,-.5f,.125f};body.mRotation=osg::Quat(osg::PI_2,osg::Vec3f(1,0,0));
            mPtr.getCellRef().setScale(2.f);
            auto scaled=NifBullet::ragdollDefinitionWithNativeScaledProperties(graph,2.f);
            auto bind=body.mBoneBind;bind.setTrans(bind.getTrans()*2.f);
            const auto root=osg::Matrixf::translate(100,40,80);
            const std::array<NifBullet::RagdollBoneWorldPose,1> bones{{{body.mNodeRecord,bind*root}}};
            const auto poses=NifBullet::ragdollBodyWorldPoses(scaled,bones);
            auto prepared=physics.prepareLooseObject(mPtr,scaled,NifBullet::RagdollNativeLengthScale,poses,1,-1);
            ASSERT_TRUE(physics.commitLooseObject(*prepared));
            auto position=mPtr.getRefData().getPosition();position.pos[0]=50;position.pos[1]=70;position.pos[2]=90;
            mPtr.getRefData().setPosition(position);
            physics.updatePosition(mPtr);
            auto expected=osg::Matrixf::translate(50,70,90);
            auto actual=physics.captureLooseObjectRootPose(mPtr);
            for (unsigned i=0;i<16;++i) EXPECT_NEAR(actual.ptr()[i],expected.ptr()[i],.0005f)<<i;
            const osg::Quat rotation(osg::PI_2,osg::Vec3f(0,1,0));
            physics.updateRotation(mPtr,rotation);
            expected=osg::Matrixf::rotate(rotation)*osg::Matrixf::translate(50,70,90);
            actual=physics.captureLooseObjectRootPose(mPtr);
            for (unsigned i=0;i<16;++i) EXPECT_NEAR(actual.ptr()[i],expected.ptr()[i],.0005f)<<i;
            EXPECT_EQ(physics.looseObjectOwners(),std::vector<MWWorld::Ptr>{mPtr});
            EXPECT_EQ(mPtr.getCellRef().getScale(),2.f);
            physics.removeLooseObject(mPtr);
        }
    }
}

namespace
{
    TEST_P(RagdollSchedulerTest, RegisteredLooseRootScaleKeepsAdmittedPropertiesBoundsAndForcedOffset)
    {
        for (bool bodyT : {false, true})
        {
            SCOPED_TRACE(bodyT);
            NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
            MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
            auto graph = mGraph;
            auto& body = graph.mBodies.front();
            graph.mReferenceRoot = 0; body.mNodeRecord = 0;
            body.mBoneBind = osg::Matrixf::identity();
            body.mUsesRigidBodyTransform = bodyT;
            body.mTranslation = {.25f, -.5f, .125f};
            body.mRotation = osg::Quat(osg::PI_2, osg::Vec3f(1, 0, 0));
            mPtr.getCellRef().setScale(2.f);
            const auto scaled = NifBullet::ragdollDefinitionWithNativeScaledProperties(graph, 2.f);
            const auto root = osg::Matrixf::translate(100, 40, 80);
            const std::array<NifBullet::RagdollBoneWorldPose, 1> bones{{{0, root}}};
            const auto poses = NifBullet::ragdollBodyWorldPoses(scaled, bones);
            auto prepared = scheduler.prepareLooseObject(mPtr, scaled, NifBullet::RagdollNativeLengthScale, poses, 1, -1);
            ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
            auto* rigid = btRigidBody::upcast(scheduler.looseObjectCollisionObject(mPtr)); ASSERT_NE(rigid, nullptr);
            const auto inverseMass = rigid->getInvMass();
            const auto inertia = rigid->getInvInertiaDiagLocal();
            auto* shape = rigid->getCollisionShape(); const auto shapeScale = shape->getLocalScaling();
            rigid->applyCentralForce(btVector3(3, 4, 5));
            btVector3 minimum, maximum; rigid->getAabb(minimum, maximum);
            const auto initial = scheduler.captureLooseObject(mPtr);
            const auto packed = scheduler.captureLooseObjectPackedVelocities(mPtr);
            const auto originalRoot = scheduler.captureLooseObjectRootPose(mPtr);
            for (float scale : {.63f, 1.f, 1.38f})
            {
                mPtr.getCellRef().setScale(scale);
                EXPECT_EQ(scheduler.captureLooseObjectRootPose(mPtr), originalRoot);
                EXPECT_EQ(scheduler.captureLooseObject(mPtr)[0].mPose, initial[0].mPose);
                EXPECT_EQ(scheduler.captureLooseObjectPackedVelocities(mPtr)[0].mVelocities, packed[0].mVelocities);
                btVector3 low, high; rigid->getAabb(low, high);
                EXPECT_EQ(low, minimum); EXPECT_EQ(high, maximum);
                EXPECT_EQ(rigid->getCollisionShape(), shape); EXPECT_EQ(shape->getLocalScaling(), shapeScale);
                EXPECT_EQ(rigid->getInvMass(), inverseMass); EXPECT_EQ(rigid->getInvInertiaDiagLocal(), inertia);
                EXPECT_EQ(rigid->getTotalForce(), btVector3(3, 4, 5));
            }
            for (float invalid : {0.f, -1.f, std::numeric_limits<float>::infinity(),
                     std::numeric_limits<float>::quiet_NaN()})
            {
                mPtr.getCellRef().setScale(invalid);
                EXPECT_THROW(scheduler.captureLooseObjectRootPose(mPtr), std::invalid_argument);
                EXPECT_THROW(scheduler.updateLooseObjectRootPose(mPtr, root), std::invalid_argument);
                EXPECT_EQ(scheduler.captureLooseObject(mPtr)[0].mPose, initial[0].mPose);
            }
            mPtr.getCellRef().setScale(1.38f);
            // A forced transform after scaling must use the admitted bodyT
            // offset, without multiplying it by the new visible scale.
            const auto moved = osg::Matrixf::rotate(osg::PI_2, osg::Vec3f(0, 0, 1))
                * osg::Matrixf::translate(30, 70, 90);
            scheduler.updateLooseObjectRootPose(mPtr, moved);
            const auto actual = scheduler.captureLooseObjectRootPose(mPtr);
            for (unsigned i = 0; i < 16; ++i) EXPECT_NEAR(actual.ptr()[i], moved.ptr()[i], .0005f) << i;
            const std::array<NifBullet::RagdollBoneWorldPose, 1> movedBones{{{0, moved}}};
            const auto expected = NifBullet::ragdollBodyWorldPoses(scaled, movedBones);
            const auto after = scheduler.captureLooseObject(mPtr)[0].mPose;
            EXPECT_NEAR((after.getOrigin() - expected[0].getOrigin()).length(), 0, .0005f);
            EXPECT_EQ(scheduler.looseObjectCollisionObject(mPtr), rigid);
            EXPECT_EQ(rigid->getInvMass(), inverseMass); EXPECT_EQ(rigid->getInvInertiaDiagLocal(), inertia);
            EXPECT_EQ(rigid->getTotalForce(), btVector3(3, 4, 5));
            scheduler.removeLooseObject(mPtr);
        }
    }
}

namespace
{
    TEST_P(RagdollSchedulerTest, RegisteredRootUsesLiveSinglePrecisionForPhysicalRotationProjection)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        mGraph.mReferenceRoot = 0;
        mGraph.mBodies[0].mNodeRecord = 0;
        mGraph.mBodies[0].mBoneBind = osg::Matrixf::identity();
        auto prepared = scheduler.prepareLooseObject(mPtr, mGraph, NifBullet::RagdollNativeLengthScale, mPoses, 1, -1);
        ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
        auto state = scheduler.captureLooseObject(mPtr);
        const auto velocities = scheduler.captureLooseObjectPackedVelocities(mPtr);
        // Original ordinary physical-to-scene case100,007F. These input bits
        // survive Bullet's basis/quaternion roundtrip, checked below.
        const std::array<std::uint32_t, 4> input{0x3e8b5948u, 0xbed105ecu, 0x3f0b5948u, 0x3f2e2f9au};
        const std::array<std::uint32_t, 9> expected{0x3d97b420u, 0xbf7684beu, 0xbe84bda1u,
            0x3f04bda2u, 0x3e84bd9eu, 0xbf5097b5u, 0x3f5a12f8u, 0xbd97b428u, 0x3f04bda1u};
        state[0].mPose.setRotation(btQuaternion(std::bit_cast<float>(input[0]), std::bit_cast<float>(input[1]),
            std::bit_cast<float>(input[2]), std::bit_cast<float>(input[3])));
        scheduler.restoreLooseObject(mPtr, state, velocities);
        const auto restored = scheduler.captureLooseObject(mPtr);
        const auto quaternion = restored[0].mPose.getRotation();
        for (unsigned i = 0; i < 4; ++i)
            ASSERT_EQ(std::bit_cast<std::uint32_t>(float(quaternion[i])), input[i]);
        const auto projected = scheduler.captureLooseObjectRootPose(mPtr);
        const auto frame = scheduler.captureLooseObjectRootFrame(mPtr, osg::Matrixf::identity());
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned column = 0; column < 3; ++column)
            {
                EXPECT_EQ(std::bit_cast<std::uint32_t>(projected(column, row)), expected[row * 3 + column]);
                EXPECT_EQ(std::bit_cast<std::uint32_t>(frame.mWorld.mRotation.mValues[row][column]),
                    expected[row * 3 + column]);
            }
        EXPECT_EQ(scheduler.captureLooseObject(mPtr)[0].mPose, restored[0].mPose);
        EXPECT_EQ(scheduler.captureLooseObjectPackedVelocities(mPtr)[0].mVelocities, velocities[0].mVelocities);
        scheduler.removeLooseObject(mPtr);
    }

    TEST_P(RagdollSchedulerTest, RegisteredRootFramePreparationRetainsLocalUntilCommitAndForcedTransformsResetIt)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        mGraph.mReferenceRoot = 0; mGraph.mBodies[0].mNodeRecord = 0;
        auto prepared = scheduler.prepareLooseObject(mPtr, mGraph, NifBullet::RagdollNativeLengthScale, mPoses, 1, -1);
        ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
        const auto initial = scheduler.captureLooseObjectRootPose(mPtr);
        auto state = scheduler.captureLooseObject(mPtr);
        const auto velocities = scheduler.captureLooseObjectPackedVelocities(mPtr);
        state[0].mPose.getOrigin() += btVector3(.005, 0, 0);
        scheduler.restoreLooseObject(mPtr, state, velocities);
        const auto physical = scheduler.captureLooseObjectRootPose(mPtr);
        const auto first = scheduler.captureLooseObjectRootFrame(mPtr, initial);
        const auto repeated = scheduler.captureLooseObjectRootFrame(mPtr, initial);
        EXPECT_EQ(first.mWorldChangeMask, 0u); EXPECT_EQ(repeated.mWorldChangeMask, 0u);
        EXPECT_EQ(first.mWorld.mTranslation, initial.getTrans());
        EXPECT_EQ(repeated.mWorld.mTranslation, initial.getTrans());
        EXPECT_EQ(first.mLocal.mTranslation, physical.getTrans());
        auto invalid = first; invalid.mLocal.mScale = -1;
        EXPECT_THROW(scheduler.commitLooseObjectRootFrame(mPtr, invalid), std::invalid_argument);
        EXPECT_EQ(scheduler.captureLooseObjectRootFrame(mPtr, initial).mWorld.mTranslation, initial.getTrans());
        scheduler.commitLooseObjectRootFrame(mPtr, first);
        const auto next = scheduler.captureLooseObjectRootFrame(mPtr, initial);
        EXPECT_EQ(next.mWorldChangeMask, 0u);
        EXPECT_EQ(next.mWorld.mTranslation, physical.getTrans());
        EXPECT_EQ(scheduler.captureLooseObject(mPtr)[0].mPose, state[0].mPose);
        EXPECT_EQ(scheduler.captureLooseObjectPackedVelocities(mPtr)[0].mVelocities, velocities[0].mVelocities);
        const auto forced = osg::Matrixf::translate(50, 60, 70);
        scheduler.updateLooseObjectRootPose(mPtr, forced);
        const auto afterForce = scheduler.captureLooseObjectRootFrame(mPtr, initial);
        EXPECT_EQ(afterForce.mWorldChangeMask, 0u);
        EXPECT_EQ(afterForce.mWorld.mTranslation, forced.getTrans());
        scheduler.removeLooseObject(mPtr);
        auto replacement = scheduler.prepareLooseObject(mPtr, mGraph, NifBullet::RagdollNativeLengthScale, mPoses, 1, -1);
        ASSERT_TRUE(scheduler.commitLooseObject(*replacement));
        const auto fresh = scheduler.captureLooseObjectRootFrame(mPtr, initial);
        EXPECT_EQ(fresh.mWorldChangeMask, 0u); EXPECT_EQ(fresh.mWorld.mTranslation, initial.getTrans());
        scheduler.removeLooseObject(mPtr);
    }
}

namespace
{
    TEST_P(RagdollSchedulerTest, AuthoredInactiveRootSkipsPhysicalProjectionButAcceptsExplicitForcedTransforms)
    {
        for (bool bodyT : {false, true})
        {
            SCOPED_TRACE(bodyT);
            NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
            MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
            auto graph = mGraph;
            graph.mReferenceRoot = 0;
            auto& definition = graph.mBodies[0]; definition.mNodeRecord = 0; definition.mCollisionFlags = 0;
            definition.mUsesRigidBodyTransform = bodyT;
            definition.mTranslation = {.25f, -.5f, .125f};
            definition.mRotation = osg::Quat(osg::PI_2, osg::Vec3f(1, 0, 0));
            auto prepared = scheduler.prepareLooseObject(mPtr, graph, NifBullet::RagdollNativeLengthScale, mPoses, 1, -1);
            ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
            const auto local = osg::Matrixf::translate(100, 200, 300);
            auto state = scheduler.captureLooseObject(mPtr);
            const auto velocities = scheduler.captureLooseObjectPackedVelocities(mPtr);
            for (float delta : {.005f, .1f, 10.f})
            {
                state[0].mPose.getOrigin() += btVector3(delta, 0, 0);
                state[0].mPose.setRotation(btQuaternion(btVector3(0, 0, 1), .2));
                scheduler.restoreLooseObject(mPtr, state, velocities);
                const auto frame = scheduler.captureLooseObjectRootFrame(mPtr, local);
                EXPECT_EQ(frame.mWorldChangeMask, 0u);
                EXPECT_EQ(frame.mLocal.mTranslation, local.getTrans());
                EXPECT_EQ(frame.mWorld.mTranslation, local.getTrans());
                for (unsigned row = 0; row < 3; ++row)
                    for (unsigned column = 0; column < 3; ++column)
                        EXPECT_EQ(frame.mWorld.mRotation.mValues[row][column], row == column ? 1.f : 0.f);
                scheduler.commitLooseObjectRootFrame(mPtr, frame);
                EXPECT_EQ(scheduler.captureLooseObject(mPtr)[0].mPose, state[0].mPose);
                EXPECT_EQ(scheduler.captureLooseObjectPackedVelocities(mPtr)[0].mVelocities, velocities[0].mVelocities);
            }
            const auto forced = osg::Matrixf::rotate(.3f, osg::Vec3f(0, 1, 0))
                * osg::Matrixf::translate(50, 60, 70);
            scheduler.updateLooseObjectRootPose(mPtr, forced);
            const auto afterForce = scheduler.captureLooseObjectRootFrame(mPtr, local);
            EXPECT_EQ(afterForce.mWorldChangeMask, 0u);
            EXPECT_EQ(afterForce.mWorld.mTranslation, forced.getTrans());
            scheduler.removeLooseObject(mPtr);
        }
    }
}

namespace
{
    TEST_P(RagdollSchedulerTest, LooseSceneTargetIsPhysicalNodeWorldBeforeInverseBind)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        for (const bool bodyT : {false, true})
        {
            SCOPED_TRACE(bodyT);
            auto& body = mGraph.mBodies.front();
            mGraph.mReferenceRoot = 0;
            body.mNodeRecord = 3;
            body.mBoneBind = osg::Matrixf::rotate(osg::PI_2, osg::Vec3f(0, 0, 1))
                * osg::Matrixf::translate(30, -7, 15);
            body.mUsesRigidBodyTransform = bodyT;
            body.mTranslation = {.5f, .25f, -.75f};
            body.mRotation = osg::Quat(osg::PI_2, osg::Vec3f(1, 0, 0));
            const auto root = osg::Matrixf::rotate(osg::PI_2, osg::Vec3f(0, 1, 0))
                * osg::Matrixf::translate(100, 40, 80);
            const auto nodeWorld = body.mBoneBind * root;
            const std::array<NifBullet::RagdollBoneWorldPose, 1> bones{{{3, nodeWorld}}};
            const auto poses = NifBullet::ragdollBodyWorldPoses(mGraph, bones);
            auto prepared = scheduler.prepareLooseObject(mPtr, mGraph, NifBullet::RagdollNativeLengthScale, poses, 1, -1);
            ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
            const auto state = scheduler.captureLooseObject(mPtr);
            const auto velocities = scheduler.captureLooseObjectPackedVelocities(mPtr);
            const auto target = scheduler.captureLooseObjectSceneTarget(mPtr);
            EXPECT_EQ(target.mBodyRecord, 12u); EXPECT_EQ(target.mNodeRecord, 3u);
            EXPECT_EQ(target.mReferenceRoot, 0u); EXPECT_EQ(target.mCollisionFlags, 1u);
            const auto pose = scheduler.captureLooseObjectTargetPose(mPtr, target); ASSERT_TRUE(pose);
            EXPECT_FLOAT_EQ(pose->mScale, 1);
            const auto matrix = pose->toMatrix();
            for (unsigned i = 0; i < 16; ++i) EXPECT_NEAR(matrix.ptr()[i], nodeWorld.ptr()[i], .0003f) << i;
            EXPECT_NEAR(pose->mTranslation.x(), 115, .0003f);
            EXPECT_NEAR(pose->mTranslation.y(), 33, .0003f);
            EXPECT_NEAR(pose->mTranslation.z(), 50, .0003f);
            const auto projectedRoot = scheduler.captureLooseObjectRootPose(mPtr);
            for (unsigned i = 0; i < 16; ++i) EXPECT_NEAR(projectedRoot.ptr()[i], root.ptr()[i], .0003f) << i;
            EXPECT_GT((pose->mTranslation - projectedRoot.getTrans()).length(), 10);
            // Visible scale does not change the admitted physical target. The
            // legacy child-to-root adapter still has its separate scale guard.
            mPtr.getCellRef().setScale(2);
            const auto scaled = scheduler.captureLooseObjectTargetPose(mPtr, target); ASSERT_TRUE(scaled);
            EXPECT_EQ(scaled->toMatrix(), matrix);
            EXPECT_THROW(scheduler.captureLooseObjectRootPose(mPtr), std::invalid_argument);
            mPtr.getCellRef().setScale(1);
            EXPECT_EQ(scheduler.captureLooseObject(mPtr)[0].mPose, state[0].mPose);
            EXPECT_EQ(scheduler.captureLooseObjectPackedVelocities(mPtr)[0].mVelocities, velocities[0].mVelocities);
            scheduler.removeLooseObject(mPtr);
        }
    }

    TEST_P(RagdollSchedulerTest, LooseSceneTargetRejectsTamperingAndReplacementButSurvivesOwnerTransfer)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        mGraph.mReferenceRoot = 0; mGraph.mBodies.front().mNodeRecord = 3;
        auto prepared = scheduler.prepareLooseObject(mPtr, mGraph, NifBullet::RagdollNativeLengthScale, mPoses, 1, -1);
        ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
        const auto target = scheduler.captureLooseObjectSceneTarget(mPtr);
        for (unsigned field = 0; field < 5; ++field)
        {
            auto wrong = target;
            if (field == 0) ++wrong.mBodyRecord;
            if (field == 1) ++wrong.mNodeRecord;
            if (field == 2) wrong.mReferenceRoot = 7;
            if (field == 3) wrong.mCollisionFlags ^= 8;
            if (field == 4) wrong = {};
            EXPECT_THROW(scheduler.captureLooseObjectTargetPose(mPtr, wrong), std::invalid_argument) << field;
        }
        scheduler.removeLooseObject(mPtr);
        auto replacement = scheduler.prepareLooseObject(mPtr, mGraph, NifBullet::RagdollNativeLengthScale, mPoses, 1, -1);
        ASSERT_TRUE(scheduler.commitLooseObject(*replacement));
        EXPECT_THROW(scheduler.captureLooseObjectTargetPose(mPtr, target), std::invalid_argument);
        const auto current = scheduler.captureLooseObjectSceneTarget(mPtr);
        ASSERT_TRUE(scheduler.captureLooseObjectTargetPose(mPtr, current));
        MWWorld::LiveCellRef<ESM::Static> newRef(mReference, &mBase);
        const MWWorld::Ptr updated(&newRef);
        scheduler.updateLooseObjectPtr(mPtr, updated);
        mLive.reset();
        EXPECT_THROW(scheduler.captureLooseObjectSceneTarget(mPtr), std::invalid_argument);
        EXPECT_THROW(scheduler.captureLooseObjectTargetPose(mPtr, current), std::invalid_argument);
        EXPECT_TRUE(scheduler.captureLooseObjectTargetPose(updated, current));
        scheduler.removeLooseObject(updated);
        EXPECT_THROW(scheduler.captureLooseObjectTargetPose(updated, current), std::invalid_argument);
    }

    TEST_P(RagdollSchedulerTest, LooseSceneTargetInactiveAndPendingDispatchDoNotConsumePhysicalInput)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        for (const std::uint16_t flags : {0, 1, 8, 0x40, 0x41})
        {
            SCOPED_TRACE(flags);
            mGraph.mBodies.front().mCollisionFlags = flags;
            // Inactive nodes need no unit conversion, even in a nonnative
            // physics fixture. Pending sync must precede the inactive branch.
            const float units = flags == 0 || flags == 8 ? 1.f : NifBullet::RagdollNativeLengthScale;
            auto prepared = scheduler.prepareLooseObject(mPtr, mGraph, units, mPoses, 1, -1);
            ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
            auto* rigid = btRigidBody::upcast(scheduler.looseObjectCollisionObject(mPtr)); ASSERT_NE(rigid, nullptr);
            const auto original = rigid->getWorldTransform();
            auto poison = original; poison.setOrigin(btVector3(std::numeric_limits<double>::quiet_NaN(), 0, 0));
            rigid->setWorldTransform(poison);
            const auto target = scheduler.captureLooseObjectSceneTarget(mPtr);
            EXPECT_EQ(target.mCollisionFlags, flags);
            if (flags == 0 || flags == 8)
            {
                EXPECT_FALSE(scheduler.captureLooseObjectTargetPose(mPtr, target));
            }
            else
            {
                try
                {
                    (void)scheduler.captureLooseObjectTargetPose(mPtr, target);
                    ADD_FAILURE() << "poisoned active or pending input accepted";
                }
                catch (const std::invalid_argument& error)
                {
                    EXPECT_NE(std::string(error.what()).find(flags & 0x40 ? "first synchronization" : "nonfinite world position"),
                        std::string::npos);
                }
            }
            EXPECT_TRUE(std::isnan(rigid->getWorldTransform().getOrigin().x()));
            rigid->setWorldTransform(original);
            scheduler.removeLooseObject(mPtr);
        }
    }

    TEST_P(RagdollSchedulerTest, LooseSceneTargetMatchesOriginalZeroOriginNoncommutingBodyTBits)
    {
        // Original full-route physical-target-zero-origin-oracle-01 cases4,
        // 28 and100, FPCW007F; physical input origin is exactly0 in both units.
        struct Case
        {
            bool mBodyT;
            std::array<std::uint32_t, 3> mOffset;
            std::array<std::uint32_t, 4> mLocalQuaternion;
            std::array<std::uint32_t, 12> mExpected;
        };
        const std::array<Case, 3> cases{{
            {true, {0x3e800000u,0xbf000000u,0x3e000000u}, {0x3f3504f3u,0,0,0x3f3504f3u},
                {0x3d97b430u,0x3e84bda0u,0xbf7684beu,0x3f04bda1u,0x3f5097b4u,0x3e84bda1u,
                 0x3f5a12f6u,0xbf04bda2u,0xbd97b420u,0x3fcf6103u,0x3fdbd25bu,0xc04f6105u}},
            {true, {0xbf9d70a4u,0x4091eb85u,0x410e6666u}, {0x3e8b5948u,0xbed105ecu,0x3f0b5948u,0x3f2e2f9au},
                {0x3f800000u,0,0,0,0x3f800000u,0,0,0,0x3f800000u,0x4109bdbeu,0xc1ff533bu,0xc2792a7au}},
            {false, {0x3e800000u,0xbf000000u,0x3e000000u}, {0x3f3504f3u,0,0,0x3f3504f3u},
                {0x3d97b420u,0xbf7684beu,0xbe84bda1u,0x3f04bda2u,0x3e84bd9eu,0xbf5097b5u,
                 0x3f5a12f8u,0xbd97b428u,0x3f04bda1u,0,0,0}}
        }};
        const std::array<std::uint32_t, 4> rotation{0x3e8b5948u,0xbed105ecu,0x3f0b5948u,0x3f2e2f9au};
        const auto f = [](std::uint32_t bits) { return std::bit_cast<float>(bits); };
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        for (const auto& input : cases)
        {
            auto& body = mGraph.mBodies.front();
            mGraph.mReferenceRoot = 0; body.mNodeRecord = 3;
            body.mUsesRigidBodyTransform = input.mBodyT;
            body.mBoneBind = osg::Matrixf::translate(123, 456, 789);
            body.mTranslation = {f(input.mOffset[0]), f(input.mOffset[1]), f(input.mOffset[2])};
            body.mRotation = {f(input.mLocalQuaternion[0]), f(input.mLocalQuaternion[1]),
                f(input.mLocalQuaternion[2]), f(input.mLocalQuaternion[3])};
            const std::array<btTransform, 1> poses{{btTransform(btQuaternion(f(rotation[0]), f(rotation[1]),
                f(rotation[2]), f(rotation[3])), btVector3(0, 0, 0))}};
            auto prepared = scheduler.prepareLooseObject(mPtr, mGraph, NifBullet::RagdollNativeLengthScale, poses, 1, -1);
            ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
            const auto before = scheduler.captureLooseObject(mPtr);
            const auto velocities = scheduler.captureLooseObjectPackedVelocities(mPtr);
            // Verify fixture inputs after Bullet's matrix/quaternion roundtrip.
            const auto physicalQuaternion = before[0].mPose.getRotation();
            for (unsigned i = 0; i < 4; ++i)
                ASSERT_EQ(std::bit_cast<std::uint32_t>(float(physicalQuaternion[i])), rotation[i]);
            EXPECT_EQ(before[0].mPose.getOrigin(), btVector3(0, 0, 0));
            const auto target = scheduler.captureLooseObjectSceneTarget(mPtr);
            const auto pose = scheduler.captureLooseObjectTargetPose(mPtr, target); ASSERT_TRUE(pose);
            for (unsigned i = 0; i < 9; ++i)
                EXPECT_EQ(std::bit_cast<std::uint32_t>(pose->mRotation.mValues[i / 3][i % 3]), input.mExpected[i]) << i;
            for (unsigned i = 0; i < 3; ++i)
                EXPECT_EQ(std::bit_cast<std::uint32_t>(pose->mTranslation[i]), input.mExpected[i + 9]) << i;
            EXPECT_FLOAT_EQ(pose->mScale, 1);
            EXPECT_EQ(scheduler.captureLooseObject(mPtr)[0].mPose, before[0].mPose);
            EXPECT_EQ(scheduler.captureLooseObjectPackedVelocities(mPtr)[0].mVelocities, velocities[0].mVelocities);
            scheduler.removeLooseObject(mPtr);
        }
    }

    TEST_P(RagdollSchedulerTest, PhysicsSystemSceneTargetRejectsWrongUnitsAndDoesNotConsumeAnInverseBind)
    {
        VFS::Manager vfs;
        Resource::ResourceSystem resources(&vfs, 0., nullptr);
        MWPhysics::PhysicsSystem physics(&resources, new osg::Group);
        mGraph.mBodies.front().mBoneBind = osg::Matrixf::scale(2, 1, 1);
        for (const float units : {1.f, NifBullet::RagdollNativeLengthScale})
        {
            auto prepared = physics.prepareLooseObject(mPtr, mGraph, units, mPoses, 1, -1);
            ASSERT_TRUE(physics.commitLooseObject(*prepared));
            const auto before = physics.captureLooseObject(mPtr);
            const auto target = physics.captureLooseObjectSceneTarget(mPtr);
            if (units == 1)
            {
                EXPECT_THROW(physics.captureLooseObjectTargetPose(mPtr, target), std::invalid_argument);
            }
            else
            {
                const auto pose = physics.captureLooseObjectTargetPose(mPtr, target); ASSERT_TRUE(pose);
                EXPECT_NEAR(pose->mTranslation.z(), 2, .00001f);
                EXPECT_FLOAT_EQ(pose->mScale, 1);
            }
            EXPECT_THROW(physics.captureLooseObjectRootPose(mPtr), std::invalid_argument);
            EXPECT_EQ(physics.captureLooseObject(mPtr)[0].mPose, before[0].mPose);
            physics.removeLooseObject(mPtr);
        }
    }
}

namespace
{
    TEST_P(RagdollSchedulerTest, PendingSceneInitializationPublishesOriginalFirstTargetsAndFreshFlags)
    {
        struct Case
        {
            bool mRoot, mBodyT;
            std::array<std::uint32_t, 13> mWorld;
            std::array<std::uint32_t, 3> mOffset;
            std::array<std::uint32_t, 4> mLocalRotation;
            std::array<std::uint32_t, 7> mMotion;
            std::uint16_t mBefore, mAfter;
        };
        // Original first-sync corpus02,007F. Values precede production conversion.
        const std::array<Case, 3> cases{{
            {true, true, {0xbeee13b1u, 0x3f4b899cu, 0x3ec761d7u, 0x3f6112c4u, 0x3eb9e473u, 0x3e9df98eu, 0x3dd4d7fcu, 0x3ef8c0b0u, 0xbf5e2b09u, 0x41200000u, 0x41a00000u, 0x41f00000u, 0x3f2147aeu},
                {0x3e800000u, 0xbf000000u, 0x3e000000u}, {0x3f3504f3u, 0x0u, 0x0u, 0x3f3504f3u},
                {0x3f76b28bu, 0x403bcca5u, 0x407d7f12u, 0x3ed80f3eu, 0x3f406663u, 0xbed1a3b6u, 0xbe991c18u}, 0x4c, 0xc},
            {false, true, {0xbf1d8dc8u, 0x3f3b0df5u, 0x3e974e6du, 0xbf3636ceu, 0xbf2d20bcu, 0x3e4259e6u, 0x3ead547au, 0xbdbf8f0cu, 0x3f6fb0edu, 0x417a8391u, 0x41af8af5u, 0x4204d06fu, 0x3f2147aeu},
                {0x3e800000u, 0xbf000000u, 0x3e000000u}, {0x3f3504f3u, 0x0u, 0x0u, 0x3f3504f3u},
                {0x3fe09c82u, 0x40546b38u, 0x409fc22bu, 0xbe22e5a8u, 0x3f277717u, 0x3f1dc43fu, 0xbed136d6u}, 0x40, 0x0},
            {false, false, {0x0u, 0x3f800000u, 0x0u, 0xbf800000u, 0x0u, 0x0u, 0x0u, 0x0u, 0x3f800000u, 0x413e3d71u, 0x41b428f6u, 0x4204999au, 0x3f2147aeu},
                {0x3e800000u, 0xbf000000u, 0x3e000000u}, {0x3f3504f3u, 0x0u, 0x0u, 0x3f3504f3u},
                {0x3fd9726cu, 0x404dece5u, 0x4097904bu, 0x0u, 0x0u, 0xbf3504f3u, 0x3f3504f3u}, 0x40, 0x0},
        }};
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        for (const auto& row : cases)
        {
            auto& body = mGraph.mBodies.front();
            mGraph.mReferenceRoot = 0; body.mNodeRecord = row.mRoot ? 0 : 3;
            body.mCollisionFlags = 0; body.mUsesRigidBodyTransform = row.mBodyT;
            for (unsigned i = 0; i < 3; ++i) body.mTranslation[i] = std::bit_cast<float>(row.mOffset[i]);
            for (unsigned i = 0; i < 4; ++i) body.mRotation[i] = std::bit_cast<float>(row.mLocalRotation[i]);
            auto prepared = scheduler.prepareLooseObject(mPtr, mGraph, NifBullet::RagdollNativeLengthScale,
                mPoses, 1, -1, {}, MWPhysics::LooseObjectSceneInitialization::PendingReference);
            EXPECT_EQ(world.getNumCollisionObjects(), 0);
            ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
            const auto before = scheduler.captureLooseObjectSceneTarget(mPtr);
            EXPECT_EQ(before.mCollisionFlags, row.mBefore);
            EXPECT_THROW(scheduler.captureLooseObjectTargetPose(mPtr, before), std::invalid_argument);
            auto scene = Nif::NiTransform::getIdentity();
            for (unsigned i = 0; i < 9; ++i) scene.mRotation.mValues[i / 3][i % 3] = std::bit_cast<float>(row.mWorld[i]);
            for (unsigned i = 0; i < 3; ++i) scene.mTranslation[i] = std::bit_cast<float>(row.mWorld[9 + i]);
            // Original scene-to-body consumes no separate scale, even when poisoned.
            scene.mScale = std::numeric_limits<float>::quiet_NaN();
            const auto settled = scheduler.synchronizeLooseObjectFirstScenePose(mPtr, before, scene);
            EXPECT_EQ(settled.mCollisionFlags, row.mAfter);
            EXPECT_EQ(scheduler.captureLooseObjectSceneTarget(mPtr).mCollisionFlags, row.mAfter);
            EXPECT_EQ(body.mCollisionFlags, 0); // Source metadata remains authored.
            const auto state = scheduler.captureLooseObject(mPtr).front();
            btQuaternion rotation;
            for (unsigned i = 0; i < 4; ++i) rotation[i] = std::bit_cast<float>(row.mMotion[3 + i]);
            const btMatrix3x3 expected(rotation);
            for (unsigned i = 0; i < 3; ++i)
            {
                EXPECT_DOUBLE_EQ(state.mPose.getOrigin()[i],
                    double(std::bit_cast<float>(row.mMotion[i])) * NifBullet::RagdollNativeLengthScale);
                for (unsigned j = 0; j < 3; ++j)
                    EXPECT_NEAR(state.mPose.getBasis()[i][j], expected[i][j], 1e-12);
            }
            EXPECT_THROW(scheduler.captureLooseObjectTargetPose(mPtr, before), std::invalid_argument);
            EXPECT_FALSE(scheduler.captureLooseObjectTargetPose(mPtr, settled)); // Inactive first sync still ran.
            EXPECT_THROW(scheduler.synchronizeLooseObjectFirstScenePose(mPtr, settled, scene), std::invalid_argument);
            scheduler.removeLooseObject(mPtr);
        }
    }

    TEST_P(RagdollSchedulerTest, FirstScenePoseKeepsStrictNativeVelocityBoundaryAndIndependentPackedLanes)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        mGraph.mReferenceRoot = 0; mGraph.mBodies.front().mNodeRecord = 3;
        const std::array<std::pair<std::uint32_t, bool>, 6> samples{{
            {0u, false}, {0x80000000u, false}, {0x3a83126eu, false}, {0x3a83126fu, false},
            {0x3a831270u, true}, {0xba831270u, true}}};
        for (const auto [bits, resets] : samples)
        for (unsigned selected = 0; selected < 6; ++selected)
        {
            auto prepared = scheduler.prepareLooseObject(mPtr, mGraph, NifBullet::RagdollNativeLengthScale,
                mPoses, 1, -1, {}, MWPhysics::LooseObjectSceneInitialization::PendingReference);
            ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
            auto state = scheduler.captureLooseObject(mPtr);
            auto packed = scheduler.captureLooseObjectPackedVelocities(mPtr);
            auto& input = packed[0].mVelocities;
            input.mLinear = {-0.f, 0.f, -0.f, 7.f}; input.mAngular = {0.f, -0.f, 0.f, 9.f};
            (selected < 3 ? input.mLinear : input.mAngular)[selected % 3] = std::bit_cast<float>(bits);
            for (unsigned i = 0; i < 3; ++i)
            {
                state[0].mLinearVelocity[i] = double(input.mLinear[i]) * NifBullet::RagdollNativeLengthScale;
                state[0].mAngularVelocity[i] = input.mAngular[i];
            }
            scheduler.restoreLooseObject(mPtr, state, packed);
            auto* rigid = btRigidBody::upcast(world.getCollisionObjectArray()[0]);
            rigid->applyCentralForce(btVector3(3, 4, 5)); rigid->applyTorque(btVector3(6, 7, 8));
            const auto force = rigid->getTotalForce(), torque = rigid->getTotalTorque();
            const auto target = scheduler.captureLooseObjectSceneTarget(mPtr);
            auto scene = Nif::NiTransform::getIdentity(); scene.mTranslation = {10, 20, 30};
            scheduler.synchronizeLooseObjectFirstScenePose(mPtr, target, scene);
            const auto output = scheduler.captureLooseObjectPackedVelocities(mPtr)[0].mVelocities;
            for (unsigned i = 0; i < 4; ++i)
            {
                EXPECT_EQ(std::bit_cast<std::uint32_t>(output.mLinear[i]),
                    resets && selected < 3 ? 0u : std::bit_cast<std::uint32_t>(input.mLinear[i]));
                EXPECT_EQ(std::bit_cast<std::uint32_t>(output.mAngular[i]),
                    resets && selected >= 3 ? 0u : std::bit_cast<std::uint32_t>(input.mAngular[i]));
            }
            EXPECT_EQ(rigid->getTotalForce(), force); EXPECT_EQ(rigid->getTotalTorque(), torque);
            scheduler.removeLooseObject(mPtr);
        }
    }

    TEST_P(RagdollSchedulerTest, FirstScenePoseRejectsStaleAndInvalidInputsBeforeStateChanges)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        mGraph.mReferenceRoot = 0;
        auto prepare = [&] { return scheduler.prepareLooseObject(mPtr, mGraph, NifBullet::RagdollNativeLengthScale,
            mPoses, 1, -1, {}, MWPhysics::LooseObjectSceneInitialization::PendingReference); };
        auto prepared = prepare(); ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
        const auto target = scheduler.captureLooseObjectSceneTarget(mPtr);
        auto scene = Nif::NiTransform::getIdentity();
        const auto before = scheduler.captureLooseObject(mPtr);
        const auto packed = scheduler.captureLooseObjectPackedVelocities(mPtr);
        for (unsigned field = 0; field < 5; ++field)
        {
            auto invalid = target;
            if (field == 0) ++invalid.mBodyRecord;
            if (field == 1) ++invalid.mNodeRecord;
            if (field == 2) invalid.mReferenceRoot = 999;
            if (field == 3) invalid.mCollisionFlags ^= 8;
            if (field == 4) invalid = {};
            EXPECT_THROW(scheduler.synchronizeLooseObjectFirstScenePose(mPtr, invalid, scene), std::invalid_argument);
        }
        scene.mRotation.mValues[0][0] = 2;
        EXPECT_THROW(scheduler.synchronizeLooseObjectFirstScenePose(mPtr, target, scene), std::invalid_argument);
        EXPECT_EQ(scheduler.captureLooseObjectSceneTarget(mPtr).mCollisionFlags, target.mCollisionFlags);
        EXPECT_EQ(scheduler.captureLooseObject(mPtr)[0].mPose, before[0].mPose);
        EXPECT_EQ(scheduler.captureLooseObjectPackedVelocities(mPtr)[0].mVelocities, packed[0].mVelocities);
        auto* rigid = btRigidBody::upcast(world.getCollisionObjectArray()[0]);
        rigid->setLinearVelocity(btVector3(std::numeric_limits<btScalar>::quiet_NaN(), 0, 0));
        scene = Nif::NiTransform::getIdentity();
        EXPECT_THROW(scheduler.synchronizeLooseObjectFirstScenePose(mPtr, target, scene), std::invalid_argument);
        EXPECT_EQ(scheduler.captureLooseObjectSceneTarget(mPtr).mCollisionFlags, target.mCollisionFlags);
        EXPECT_EQ(scheduler.captureLooseObject(mPtr)[0].mPose, before[0].mPose);
        rigid->setLinearVelocity(btVector3(0, 0, 0));
        scheduler.removeLooseObject(mPtr);
        auto replacement = prepare(); ASSERT_TRUE(scheduler.commitLooseObject(*replacement));
        EXPECT_THROW(scheduler.synchronizeLooseObjectFirstScenePose(mPtr, target, scene), std::invalid_argument);
        scheduler.removeLooseObject(mPtr);
    }

    TEST_P(RagdollSchedulerTest, PhysicsSystemFirstScenePoseRequiresNativeReferenceInitialization)
    {
        VFS::Manager vfs;
        Resource::ResourceSystem resources(&vfs, 0., nullptr);
        MWPhysics::PhysicsSystem physics(&resources, new osg::Group);
        using Initialization = MWPhysics::LooseObjectSceneInitialization;
        EXPECT_THROW(physics.prepareLooseObject(mPtr, mGraph, NifBullet::RagdollNativeLengthScale,
            mPoses, 1, -1, {}, Initialization::PendingReference), std::invalid_argument);
        mGraph.mReferenceRoot = 0;
        EXPECT_THROW(physics.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1, {}, Initialization::PendingReference),
            std::invalid_argument);
        EXPECT_THROW(physics.prepareLooseObject(mPtr, mGraph, NifBullet::RagdollNativeLengthScale,
            mPoses, 1, -1, {}, static_cast<Initialization>(99)), std::invalid_argument);
        auto prepared = physics.prepareLooseObject(mPtr, mGraph, NifBullet::RagdollNativeLengthScale,
            mPoses, 1, -1, {}, Initialization::PendingReference);
        ASSERT_TRUE(physics.commitLooseObject(*prepared));
        const auto target = physics.captureLooseObjectSceneTarget(mPtr);
        const auto settled = physics.synchronizeLooseObjectFirstScenePose(mPtr, target, Nif::NiTransform::getIdentity());
        EXPECT_EQ(target.mCollisionFlags, 0x4d); EXPECT_EQ(settled.mCollisionFlags, 0xd);
        EXPECT_TRUE(physics.captureLooseObjectTargetPose(mPtr, settled));
        EXPECT_THROW(physics.synchronizeLooseObjectFirstScenePose(mPtr, target, Nif::NiTransform::getIdentity()),
            std::invalid_argument);
        physics.removeLooseObject(mPtr);
        // Existing supplied-pose mode keeps its supplied collision flags.
        prepared = physics.prepareLooseObject(mPtr, mGraph, NifBullet::RagdollNativeLengthScale, mPoses, 1, -1);
        ASSERT_TRUE(physics.commitLooseObject(*prepared));
        const auto supplied = physics.captureLooseObjectSceneTarget(mPtr);
        EXPECT_EQ(supplied.mCollisionFlags, 1);
        EXPECT_THROW(physics.synchronizeLooseObjectFirstScenePose(mPtr, supplied, Nif::NiTransform::getIdentity()),
            std::invalid_argument);
        physics.removeLooseObject(mPtr);
        mGraph.mBodies.front().mCollisionFlags = 0x40;
        prepared = physics.prepareLooseObject(mPtr, mGraph, 1, mPoses, 1, -1);
        ASSERT_TRUE(physics.commitLooseObject(*prepared));
        const auto wrongUnits = physics.captureLooseObjectSceneTarget(mPtr);
        EXPECT_THROW(physics.synchronizeLooseObjectFirstScenePose(mPtr, wrongUnits, Nif::NiTransform::getIdentity()),
            std::invalid_argument);
        EXPECT_EQ(physics.captureLooseObjectSceneTarget(mPtr).mCollisionFlags, 0x40);
        physics.removeLooseObject(mPtr);
    }
}

namespace
{
    TEST_P(RagdollSchedulerTest, RetainedRegistrationWaitsForWorkersAndKeepsAbsentSnapshotsAndRouting)
    {
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        mGraph.mSourceHash = std::string(16, 'a');
        mGraph.mBodies[0].mNodeRecord = 7;
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        auto* body = world.getCollisionObjectArray()[0];
        auto* identity = scheduler.getUserPointer(body);
        const auto registered = scheduler.captureActorRagdollWorldRegistrationSelection(mPtr);
        EXPECT_EQ(registered, (NifBullet::RagdollWorldRegistration{{12}, {}}));
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("retained physical World membership");
        std::array<std::vector<MWPhysics::Simulation>, 2> frames;
        float time = 1.f / 60.f;
        scheduler.applyQueuedMovements(time, frames[0], osg::Timer::instance()->tick(), 0,
            *stats, MWPhysics::WorldFrameData(false, {}));
        // Selection joins the outstanding movement frame before unregistering.
        scheduler.setActorRagdollWorldRegistrationSelection(mPtr, {});
        EXPECT_TRUE(scheduler.hasActorRagdoll(mPtr));
        EXPECT_EQ(world.getNumCollisionObjects(), 0);
        EXPECT_EQ(scheduler.getUserPointer(body), identity);
        EXPECT_EQ(scheduler.actorRagdollOwners(), std::vector{mPtr});
        const auto base = ESM::FormKey::content("actors.esm", 100);
        const std::string model = "characters/_male/skeleton.nif";
        const auto snapshot = scheduler.captureActorRagdollSnapshot(mPtr, base, model);
        NifBullet::RagdollNativeWorldSceneRequest request{};
        request.mRecord = 12;
        request.mInput.mHasWorld = request.mInput.mHasAuthority = true;
        request.mInput.mCurrentRotation = request.mInput.mTargetRotation = {0, 0, 0, 1};
        request.mInput.mTargetPosition = {10, 0, 0, 0};
        request.mInput.mFrameSeconds = .1f;
        const std::array requests{request};
        EXPECT_TRUE(scheduler.synchronizeActorRagdollWorldScenes(mPtr, requests).empty());
        EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), snapshot);
        for (unsigned frame = 1; frame < 31; ++frame)
        {
            time += 1.f / 60.f;
            scheduler.applyQueuedMovements(time, frames[frame % 2], osg::Timer::instance()->tick(), frame,
                *stats, MWPhysics::WorldFrameData(false, {}));
        }
        EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), snapshot);
        scheduler.restoreActorRagdollSnapshot(mPtr, snapshot, base, model);
        EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), snapshot);
        EXPECT_EQ(world.getNumCollisionObjects(), 0);
        scheduler.setActorRagdollWorldRegistrationSelection(mPtr, registered);
        ASSERT_EQ(world.getNumCollisionObjects(), 1);
        EXPECT_EQ(world.getCollisionObjectArray()[0], body);
        EXPECT_EQ(scheduler.getUserPointer(body), identity);
        auto resumedSnapshot = snapshot;
        resumedSnapshot.mWorldRegistration = ESM4::RuntimeRagdollWorldRegistration{registered.mBodies, registered.mConstraints};
        EXPECT_EQ(scheduler.captureActorRagdollSnapshot(mPtr, base, model), resumedSnapshot);
        const auto resumedPosition = scheduler.captureActorRagdoll(mPtr)[0].mPose.getOrigin();
        time += 1.f / 60.f;
        scheduler.applyQueuedMovements(time, frames[1], osg::Timer::instance()->tick(), 31,
            *stats, MWPhysics::WorldFrameData(false, {}));
        EXPECT_NE(scheduler.captureActorRagdoll(mPtr)[0].mPose.getOrigin(), resumedPosition);
        scheduler.setActorRagdollWorldRegistrationSelection(mPtr, {});
        scheduler.removeActorRagdoll(mPtr);
        EXPECT_FALSE(scheduler.hasActorRagdoll(mPtr));
        EXPECT_EQ(scheduler.getUserPointer(body), nullptr);
        EXPECT_THROW(scheduler.setActorRagdollWorldRegistrationSelection(mPtr, registered), std::invalid_argument);
        EXPECT_EQ(world.getNumCollisionObjects(), 0);
    }

    TEST_P(RagdollSchedulerTest, FirstScenePoseUsesLivePositionStoresBeforeBulletPublication)
    {
        const std::array<std::array<std::uint32_t, 2>, 8> cases{{
            {0x1c423e6u, 0x701872u},
            {0x139f77bu, 0x352400u},
            {0x177ea76u, 0x46d7c2u},
            {0xf2a13eu, 0x22aa8au},
            {0x19310ceu, 0x540c8eu},
            {0x76a5a2u, 0x10f3b0u},
            {0x79636du, 0x1157f4u},
            {0xa1cdecu, 0x171e3au},
        }};
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        mGraph.mReferenceRoot = 0; mGraph.mBodies.front().mNodeRecord = 3;
        for (const auto& row : cases)
        {
            auto prepared = scheduler.prepareLooseObject(mPtr, mGraph, NifBullet::RagdollNativeLengthScale,
                mPoses, 1, -1, {}, MWPhysics::LooseObjectSceneInitialization::PendingReference);
            ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
            const auto target = scheduler.captureLooseObjectSceneTarget(mPtr);
            auto scene = Nif::NiTransform::getIdentity(); scene.mTranslation.x() = std::bit_cast<float>(row[0]);
            scheduler.synchronizeLooseObjectFirstScenePose(mPtr, target, scene);
            const auto pose = scheduler.captureLooseObject(mPtr)[0].mPose;
            EXPECT_DOUBLE_EQ(pose.getOrigin().x(),
                double(std::bit_cast<float>(row[1])) * NifBullet::RagdollNativeLengthScale);
            scheduler.removeLooseObject(mPtr);
        }
    }
    TEST_P(RagdollSchedulerTest, PreparedNativeStepsShareMovementActorAndLooseBodyWorldPass)
    {
        VFS::Manager vfs;
        Resource::ResourceSystem resources(&vfs, 0., nullptr);
        MWPhysics::PhysicsSystem physics(&resources, new osg::Group);
        NifBullet::NativeDynamicsWorld world(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        struct Ticks
        {
            MWPhysics::Projectile* mProjectile = nullptr;
            std::vector<std::pair<btScalar, float>> mValues;
        } ticks;
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        MWWorld::LiveCellRef<ESM::Static> other(mReference, &mBase);
        const MWWorld::Ptr loose(&other);
        auto loosePoses = mPoses; loosePoses[0].getOrigin().setX(20);
        auto prepared = scheduler.prepareLooseObject(loose, mGraph, 1, loosePoses, 1, -1);
        ASSERT_TRUE(scheduler.commitLooseObject(*prepared));
        auto projectile = std::make_shared<MWPhysics::Projectile>(MWWorld::Ptr(), osg::Vec3f(0, 0, 1000),
            .02f, &scheduler, &physics);
        ticks.mProjectile = projectile.get();
        world.setInternalTickCallback([](btDynamicsWorld* w, btScalar dt) {
            auto& t = *static_cast<Ticks*>(w->getWorldUserInfo());
            t.mValues.emplace_back(dt, t.mProjectile->getPosition().x());
        }, &ticks);
        projectile->setVelocity({10, 0, 0});
        std::vector<MWPhysics::Simulation> jobs;
        jobs.emplace_back(MWPhysics::ProjectileSimulation{projectile, MWPhysics::ProjectileFrameData{*projectile}});
        ESM4::PhysicalFrameSettings settings; settings.mFixedSubstep = .0167f;
        ESM4::PhysicalFrameClock clock;
        const auto frame = ESM4::preparePhysicalFrame(clock, settings, .1f, 0, false,
            Nif::NativeTransformPrecision::Single);
        ASSERT_EQ(frame.mSubstepCount, 3u);
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("prepared native shared world");
        auto invalid = frame; invalid.mSubstepCount = 4;
        EXPECT_THROW(scheduler.applyPreparedNativeFrame(invalid, clock.mRemaining, jobs,
            osg::Timer::instance()->tick(), 1, *stats, MWPhysics::WorldFrameData(false, {})), std::invalid_argument);
        EXPECT_EQ(jobs.size(), 1u);
        EXPECT_TRUE(ticks.mValues.empty());
        EXPECT_EQ(projectile->getPosition().x(), 0);
        scheduler.applyPreparedNativeFrame(frame, clock.mRemaining, jobs, osg::Timer::instance()->tick(),
            1, *stats, MWPhysics::WorldFrameData(false, {}));
        const auto actorState = scheduler.captureActorRagdoll(mPtr)[0]; // worker barrier
        const auto looseState = scheduler.captureLooseObject(loose)[0];
        ASSERT_EQ(ticks.mValues.size(), 3u);
        for (std::size_t i = 0; i < ticks.mValues.size(); ++i)
        {
            EXPECT_EQ(ticks.mValues[i].first, btScalar(.0167f));
            EXPECT_NEAR(ticks.mValues[i].second, 10 * .0167f * (i + 1), 1e-6);
        }
        EXPECT_EQ(actorState.mLinearVelocity, looseState.mLinearVelocity);
        EXPECT_EQ(actorState.mPose.getOrigin().z(), looseState.mPose.getOrigin().z());
        EXPECT_LT(actorState.mPose.getOrigin().z(), 2);
        EXPECT_NEAR(projectile->getPosition().x(), .501f, 1e-6);
        // Async jobs publish to the main thread at the next prepare boundary.
        // A zero-step pass must synchronize those jobs without another body step.
        std::vector<MWPhysics::Simulation> nextJobs;
        scheduler.applyPreparedNativeFrame({0, 0, 0, 0}, 0, nextJobs, osg::Timer::instance()->tick(),
            2, *stats, MWPhysics::WorldFrameData(false, {}));
        scheduler.captureActorRagdoll(mPtr);
        EXPECT_TRUE(jobs.empty());
        EXPECT_EQ(ticks.mValues.size(), 3u);
        EXPECT_NEAR(projectile->getSimulationPosition().x(), .501f, 1e-6);
        world.setInternalTickCallback(nullptr);
        projectile.reset();
        scheduler.removeLooseObject(loose); scheduler.removeActorRagdoll(mPtr);
    }

    TEST_P(RagdollSchedulerTest, PreparedNativeVariableAndSmoothedDurationsReachEveryWorldStep)
    {
        std::vector<btScalar> ticks;
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        mWorld.setInternalTickCallback([](btDynamicsWorld* w, btScalar dt) {
            static_cast<std::vector<btScalar>*>(w->getWorldUserInfo())->push_back(dt);
        }, &ticks);
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("native variable steps");
        std::array<std::vector<MWPhysics::Simulation>, 2> jobs;
        ESM4::PhysicalFrameClock clock;
        ESM4::PhysicalFrameSettings settings; settings.mFixedSubstep = .0167f;
        unsigned frameNumber = 0;
        for (const auto [mode, limited] : std::array<std::pair<unsigned, bool>, 4>{{
                 {1, false}, {10, false}, {0, false}, {0, true}}})
        {
            const auto frame = ESM4::preparePhysicalFrame(clock, settings, .1f, mode, limited,
                Nif::NativeTransformPrecision::Single);
            const auto first = ticks.size();
            scheduler.applyPreparedNativeFrame(frame, clock.mRemaining, jobs[frameNumber % 2],
                osg::Timer::instance()->tick(), frameNumber, *stats, MWPhysics::WorldFrameData(false, {}));
            ++frameNumber;
            scheduler.captureActorRagdoll(mPtr);
            ASSERT_EQ(ticks.size() - first, frame.mSubstepCount);
            for (auto i = first; i < ticks.size(); ++i) EXPECT_EQ(ticks[i], btScalar(frame.mSubstepSeconds));
        }
        mWorld.setInternalTickCallback(nullptr);
        scheduler.removeActorRagdoll(mPtr);
    }

    TEST_P(RagdollSchedulerTest, PreparedNativeNonpositiveFrameDoesNotStepOrConsumeSignedClock)
    {
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("native signed carry");
        std::array<std::vector<MWPhysics::Simulation>, 2> jobs;
        ESM4::PhysicalFrameClock clock; clock.mRemaining = -.0001f;
        const auto before = clock;
        const auto frame = ESM4::preparePhysicalFrame(clock, {}, 0, 0, false,
            Nif::NativeTransformPrecision::Single);
        ASSERT_EQ(frame.mSubstepCount, 0u);
        scheduler.applyPreparedNativeFrame(frame, clock.mRemaining, jobs[0], osg::Timer::instance()->tick(),
            1, *stats, MWPhysics::WorldFrameData(false, {}));
        EXPECT_EQ(scheduler.captureActorRagdoll(mPtr)[0].mPose, mPoses[0]);
        EXPECT_EQ(clock, before);
        const auto next = ESM4::preparePhysicalFrame(clock, {}, .02f, 0, false,
            Nif::NativeTransformPrecision::Single);
        scheduler.applyPreparedNativeFrame(next, clock.mRemaining, jobs[1], osg::Timer::instance()->tick(),
            2, *stats, MWPhysics::WorldFrameData(false, {}));
        EXPECT_LT(scheduler.captureActorRagdoll(mPtr)[0].mPose.getOrigin().z(), 2);
    }

    TEST_P(RagdollSchedulerTest, InvalidPreparedNativeFrameRejectsBeforeBodyOrJobsChangeAndAllowsRetry)
    {
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &mWorld, nullptr);
        scheduler.addActorRagdoll(mPtr, mGraph, 1, mPoses, 1, -1);
        osg::ref_ptr<osg::Stats> stats = new osg::Stats("invalid prepared native frame");
        std::vector<MWPhysics::Simulation> jobs;
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        const float maximum = std::numeric_limits<float>::max();
        for (const auto& frame : std::vector<ESM4::PhysicalFrameResult>{
                 {nan, .02f, .02f, 1}, {-.1f, .02f, .02f, 1}, {.1f, infinity, .02f, 1},
                 {.1f, .02f, nan, 1}, {.1f, .02f, 0, 1}, {.1f, .02f, -.02f, 1},
                 {.1f, .02f, .02f, 4}, {.1f, 0, .02f, 1}, {.1f, -.1f, .02f, 1},
                 {.1f, .02f, 0, 0}, {.1f, 0, .02f, 0}, {maximum, maximum, maximum, 1}})
        {
            EXPECT_THROW(scheduler.applyPreparedNativeFrame(frame, 0, jobs, osg::Timer::instance()->tick(),
                1, *stats, MWPhysics::WorldFrameData(false, {})), std::invalid_argument);
            EXPECT_EQ(scheduler.captureActorRagdoll(mPtr)[0].mPose, mPoses[0]);
            EXPECT_TRUE(jobs.empty());
        }
        const ESM4::PhysicalFrameResult valid{.02f, .02f, .02f, 1};
        for (float carry : {nan, infinity, -infinity})
            EXPECT_THROW(scheduler.applyPreparedNativeFrame(valid, carry, jobs, osg::Timer::instance()->tick(),
                1, *stats, MWPhysics::WorldFrameData(false, {})), std::invalid_argument);
        scheduler.applyPreparedNativeFrame(valid, 0, jobs, osg::Timer::instance()->tick(),
            1, *stats, MWPhysics::WorldFrameData(false, {}));
        EXPECT_LT(scheduler.captureActorRagdoll(mPtr)[0].mPose.getOrigin().z(), 2);
    }

}
