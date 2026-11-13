#include <apps/openmw/mwphysics/stepper.hpp>
#include <apps/openmw/mwphysics/movementsolver.hpp>
#include <apps/openmw/mwphysics/physicssystem.hpp>
#include "../mwworld/oblivionworldfixture.hpp"
#include <apps/openmw/mwphysics/collisiontype.hpp>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <apps/openmw/mwmechanics/oblivionmelee.hpp>
#include <apps/openmw/mwphysics/actor.hpp>
#include <apps/openmw/mwphysics/trace.h>
#include <apps/openmw/mwphysics/mtphysics.hpp>
#include <apps/openmw/mwbase/environment.hpp>
#include <apps/openmw/mwmechanics/creaturestats.hpp>
#include <apps/openmw/mwclass/static.hpp>
#include <apps/openmw/mwworld/esmstore.hpp>
#include <apps/openmw/mwworld/livecellref.hpp>

#include <components/esm3/loadstat.hpp>
#include <components/resource/bulletshape.hpp>
#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/nifbullet/nativedynamicsworld.hpp>
#include <components/settings/values.hpp>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>

#include <gtest/gtest.h>
#include <osg/Stats>
#include <limits>

namespace
{
    // Only the reference-class services required by the real physics Actor.
    // Local to this reference: never replace the process-wide class registry.
    class BoundsActorClass final : public MWWorld::Class
    {
        bool mNpc;
        mutable MWMechanics::CreatureStats mStats;
    public:
        explicit BoundsActorClass(bool npc) : Class(ESM::REC_STAT), mNpc(npc) {}
        bool isNpc() const override { return mNpc; }
        std::string_view getName(const MWWorld::ConstPtr&) const override { return "bounds fixture"; }
        MWMechanics::CreatureStats& getCreatureStats(const MWWorld::Ptr&) const override { return mStats; }
    };

    TEST(ActorBoundsTest, PhysicalOwnershipSuspendsCapsuleAndRestoresCurrentMaskAndPosition)
    {
        struct RestoreThreads
        {
            int mPrevious = Settings::physics().mAsyncNumThreads;
            ~RestoreThreads() { Settings::physics().mAsyncNumThreads.set(mPrevious); }
        } restoreThreads;
        MWClass::Static::registerSelf();
        MWBase::Environment environment;
        MWWorld::ESMStore store;
        environment.setESMStore(store);
        const BoundsActorClass type(false);
        for (int threads : {0, 1, 2})
        {
            SCOPED_TRACE(threads);
            Settings::physics().mAsyncNumThreads.set(threads);
            btDefaultCollisionConfiguration configuration;
            btCollisionDispatcher dispatcher(&configuration);
            btDbvtBroadphase broadphase;
            btSequentialImpulseConstraintSolver solver;
            btDiscreteDynamicsWorld world(&dispatcher, &broadphase, &solver, &configuration);
            world.setGravity(btVector3(0, 0, 0));
            ESM::Static base;
            base.blank();
            ESM::CellRef reference;
            reference.blank();
            MWWorld::LiveCellRef<ESM::Static> live(reference, &base);
            live.mClass = &type;
            MWWorld::Ptr ptr(&live);
            MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
            osg::ref_ptr<Resource::BulletShape> shape = new Resource::BulletShape;
            shape->mCollisionBox.mExtents = osg::Vec3f(1, 1, 2);
            auto actor = std::make_shared<MWPhysics::Actor>(ptr, shape, &scheduler, false,
                DetourNavigator::CollisionShapeType::Aabb, osg::Vec3f{});
            actor->setVelocity(osg::Vec3f(1, 2, 3));
            actor->setInertialForce(osg::Vec3f(4, 5, 6));
            actor->setStandingOnPtr(ptr);
            NifBullet::ActorRagdollDefinition graph;
            NifBullet::RagdollBodyDefinition body{};
            body.mRecord = 12;
            body.mMass = 2;
            body.mInertia = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            body.mShape = NifBullet::RagdollSphere{.5f};
            graph.mBodies.push_back(body);
            const std::array<btTransform, 1> poses{
                btTransform(btQuaternion::getIdentity(), btVector3(0, 0, 20))};
            scheduler.addActorRagdoll(ptr, graph, 1, poses, MWPhysics::CollisionType_Actor,
                MWPhysics::CollisionType_World);
            EXPECT_EQ(world.getNumCollisionObjects(), 2);
            actor->suspendCollision(true);
            actor->suspendCollision(true);
            EXPECT_TRUE(actor->isCollisionSuspended());
            EXPECT_EQ(world.getNumCollisionObjects(), 1);
            EXPECT_EQ(actor->getCollisionObject()->getBroadphaseHandle(), nullptr);
            EXPECT_EQ(scheduler.getUserPointer(actor->getCollisionObject()), nullptr);
            EXPECT_EQ(actor->velocity(), osg::Vec3f{});
            EXPECT_EQ(actor->getInertialForce(), osg::Vec3f{});
            EXPECT_TRUE(actor->getStandingOnPtr().isEmpty());
            EXPECT_FALSE(actor->getOnGround());
            actor->enableCollisionBody(false);
            actor->setCanWaterWalk(true);
            auto position = ptr.getRefData().getPosition();
            position.pos[0] = 100;
            position.pos[2] = 50;
            ptr.getRefData().setPosition(position);
            scheduler.updateSingleAabb(actor, true);
            EXPECT_EQ(actor->getCollisionObject()->getBroadphaseHandle(), nullptr);
            osg::ref_ptr<osg::Stats> stats = new osg::Stats("physical capsule ownership");
            std::vector<MWPhysics::Simulation> simulations;
            float time = 1.f / 60.f;
            scheduler.applyQueuedMovements(time, simulations, osg::Timer::instance()->tick(), 0,
                *stats, MWPhysics::WorldFrameData(false, {}));
            EXPECT_LT(scheduler.captureActorRagdoll(ptr)[0].mPose.getOrigin().z(), 20);
            scheduler.removeActorRagdoll(ptr);
            EXPECT_EQ(world.getNumCollisionObjects(), 0);
            actor->suspendCollision(false);
            actor->suspendCollision(false);
            ASSERT_NE(actor->getCollisionObject()->getBroadphaseHandle(), nullptr);
            EXPECT_FALSE(actor->isCollisionSuspended());
            EXPECT_EQ(world.getNumCollisionObjects(), 1);
            EXPECT_EQ(actor->getSimulationPosition(), position.asVec3());
            const int mask = actor->getCollisionObject()->getBroadphaseHandle()->m_collisionFilterMask;
            EXPECT_TRUE(mask & MWPhysics::CollisionType_Water);
            EXPECT_TRUE(mask & MWPhysics::CollisionType_World);
            EXPECT_FALSE(mask & MWPhysics::CollisionType_Actor);
            EXPECT_FALSE(mask & MWPhysics::CollisionType_Projectile);
            actor.reset();
            EXPECT_EQ(world.getNumCollisionObjects(), 0);
        }
    }

    TEST(ActorBoundsTest, SuspendedCapsuleDestructionAndForeignSchedulerPreserveWorldMembership)
    {
        MWClass::Static::registerSelf();
        MWBase::Environment environment;
        MWWorld::ESMStore store;
        environment.setESMStore(store);
        btDefaultCollisionConfiguration configuration;
        btCollisionDispatcher dispatcher(&configuration);
        btDbvtBroadphase broadphase;
        btCollisionWorld world(&dispatcher, &broadphase, &configuration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        MWPhysics::PhysicsTaskScheduler foreign(1.f / 60.f, &world, nullptr);
        ESM::Static base;
        base.blank();
        ESM::CellRef reference;
        reference.blank();
        MWWorld::LiveCellRef<ESM::Static> live(reference, &base);
        const BoundsActorClass type(false);
        live.mClass = &type;
        osg::ref_ptr<Resource::BulletShape> shape = new Resource::BulletShape;
        shape->mCollisionBox.mExtents = osg::Vec3f(1, 1, 2);
        auto actor = std::make_shared<MWPhysics::Actor>(MWWorld::Ptr(&live), shape, &scheduler, false,
            DetourNavigator::CollisionShapeType::Aabb, osg::Vec3f{});
        EXPECT_THROW(foreign.suspendActorCollision(*actor, true), std::invalid_argument);
        EXPECT_FALSE(actor->isCollisionSuspended());
        EXPECT_EQ(world.getNumCollisionObjects(), 1);
        actor->suspendCollision(true);
        EXPECT_EQ(world.getNumCollisionObjects(), 0);
        actor.reset();
        EXPECT_EQ(world.getNumCollisionObjects(), 0);
    }

    TEST(ActorBoundsTest, MovementRestorePreservesHistoryAndContactAndAcceptsFirstSolverUpdate)
    {
        struct RestoreThreads
        {
            int mPrevious = Settings::physics().mAsyncNumThreads;
            ~RestoreThreads() { Settings::physics().mAsyncNumThreads.set(mPrevious); }
        } restoreThreads;
        MWClass::Static::registerSelf();
        MWBase::Environment environment;
        MWWorld::ESMStore store;
        environment.setESMStore(store);
        const BoundsActorClass type(false);
        for (int threads : {0, 1, 2})
        {
            SCOPED_TRACE(threads);
            Settings::physics().mAsyncNumThreads.set(threads);
            btDefaultCollisionConfiguration configuration;
            btCollisionDispatcher dispatcher(&configuration);
            btDbvtBroadphase broadphase;
            btCollisionWorld world(&dispatcher, &broadphase, &configuration);
            MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
            ESM::Static base; base.blank();
            ESM::CellRef reference; reference.blank();
            MWWorld::LiveCellRef<ESM::Static> live(reference, &base), other(reference, &base);
            live.mClass = other.mClass = &type;
            MWWorld::Ptr ptr(&live), standing(&other);
            osg::ref_ptr<Resource::BulletShape> shape = new Resource::BulletShape;
            shape->mCollisionBox.mExtents = osg::Vec3f(1, 1, 2);
            auto actor = std::make_shared<MWPhysics::Actor>(ptr, shape, &scheduler, false,
                DetourNavigator::CollisionShapeType::Aabb, osg::Vec3f{});
            const MWPhysics::ActorMap actors{{ptr.mRef, actor}};
            // A fresh legacy capsule suppresses its first solver position.
            const auto original = actor->getPosition();
            actor->setPosition({100, 200, 300});
            EXPECT_EQ(actor->getPosition(), original);
            MWPhysics::ActorMovementSnapshot saved{ptr, standing, {}};
            auto& state = saved.mState;
            state.mBase = ESM::FormKey::content("fixture.esm", 1);
            state.mStandingOn = ESM::FormKey::content("fixture.esm", 2);
            state.mPosition = {1. + 0x1p-40, 2, 3};
            state.mPreviousPosition = {-.5, -1, -2};
            state.mSimulationPosition = {.5f, .25f, .125f};
            state.mVelocity = {4, 5, 6};
            state.mInertia = {7, 8, -9};
            state.mLastStuckPosition = {-4, -5, -6};
            state.mStuckFrames = 17;
            state.mOnGround = state.mOnSlope = state.mWalkingOnWater = state.mActive = true;
            state.mCollisionBody = false;
            state.mCanWaterWalk = true;
            state.mSkipSimulation = false;
            scheduler.restoreActorMovements(actors, std::span(&saved, 1));
            auto capture = [&] {
                auto result = scheduler.captureActorMovements(actors);
                EXPECT_EQ(result.size(), 1u);
                result.front().mState.mBase = state.mBase;
                result.front().mState.mStandingOn = state.mStandingOn;
                return result.front();
            };
            EXPECT_EQ(capture().mState, state);
            EXPECT_EQ(actor->getStandingOnPtr(), standing);
            EXPECT_EQ(actor->getCollisionObject()->getBroadphaseHandle()->m_collisionFilterMask,
                MWPhysics::CollisionType_World | MWPhysics::CollisionType_HeightMap | MWPhysics::CollisionType_Water);
            EXPECT_EQ(actor->velocity(), osg::Vec3f(4, 5, 6));
            EXPECT_EQ(actor->velocity(), osg::Vec3f{});
            actor->setPosition({100, 200, 300});
            EXPECT_EQ(actor->getPosition(), osg::Vec3d(100, 200, 300));
            EXPECT_EQ(actor->getPreviousPosition(), osg::Vec3d(1. + 0x1p-40, 2, 3));
            actor->setSimulationPosition({90, 180, 270});
            EXPECT_EQ(actor->getSimulationPosition(), osg::Vec3f(90, 180, 270));
            // Validate all entries before touching even the first valid actor.
            const auto before = capture();
            auto changed = saved;
            changed.mState.mPosition = {999, 999, 999};
            auto invalid = saved;
            invalid.mState.mInertia[0] = std::numeric_limits<float>::infinity();
            const std::array bad{changed, invalid};
            EXPECT_THROW(scheduler.restoreActorMovements(actors, bad), std::runtime_error);
            EXPECT_EQ(capture().mState, before.mState);
            const std::array duplicate{changed, changed};
            EXPECT_THROW(scheduler.restoreActorMovements(actors, duplicate), std::invalid_argument);
            EXPECT_EQ(capture().mState, before.mState);
            invalid = changed;
            invalid.mActor = standing;
            EXPECT_THROW(scheduler.restoreActorMovements(actors, std::span(&invalid, 1)), std::invalid_argument);
            EXPECT_EQ(capture().mState, before.mState);
            actor->suspendCollision(true);
            EXPECT_TRUE(scheduler.captureActorMovements(actors).empty());
            EXPECT_THROW(scheduler.restoreActorMovements(actors, std::span(&saved, 1)), std::invalid_argument);
        }
    }

    TEST(ActorBoundsTest, FallbackIsOptInAndPreservesLegacyNpcAndCreatureBounds)
    {
        MWClass::Static::registerSelf();
        MWBase::Environment environment;
        MWWorld::ESMStore store;
        environment.setESMStore(store);
        btDefaultCollisionConfiguration configuration;
        btCollisionDispatcher dispatcher(&configuration);
        btDbvtBroadphase broadphase;
        btCollisionWorld world(&dispatcher, &broadphase, &configuration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        ESM::Static base;
        base.blank();
        ESM::CellRef reference;
        reference.blank();
        MWWorld::LiveCellRef<ESM::Static> live(reference, &base);
        const BoundsActorClass npc(true), creature(false);
        MWWorld::Ptr ptr(&live);
        const osg::Vec3f fallback(16, 16, 64);
        osg::ref_ptr<Resource::BulletShape> source = new Resource::BulletShape;
        auto check = [&](const BoundsActorClass& type, const osg::Vec3f& supplied, const osg::Vec3f& expected) {
            live.mClass = &type;
            {
                MWPhysics::Actor actor(ptr, source, &scheduler, false,
                    DetourNavigator::CollisionShapeType::Aabb, supplied);
                EXPECT_EQ(actor.getOriginalHalfExtents(), expected);
                EXPECT_EQ(actor.getHalfExtents(), expected);
                EXPECT_EQ(world.getNumCollisionObjects(), 1);
            }
            EXPECT_EQ(world.getNumCollisionObjects(), 0);
        };
        check(npc, {}, {});
        check(npc, fallback, fallback); // Includes Oblivion's projected TES3 player.
        check(creature, {}, {}); // Legacy malformed creature: report, don't invent bounds.
        check(creature, fallback, fallback);
        source->mCollisionShape.reset(new btBoxShape(btVector3(4, 5, 6)));
        check(npc, {}, {}); // TES3 NPCs must not derive bounds from this shape.
        check(creature, {}, osg::Vec3f(4, 5, 6));
        check(npc, fallback, fallback);
        source->mCollisionBox.mExtents = osg::Vec3f(7, 8, 9);
        check(npc, {}, osg::Vec3f(7, 8, 9));
        check(npc, fallback, fallback);
        check(creature, fallback, osg::Vec3f(7, 8, 9));
    }
    TEST(ActorBoundsTest, NativeNpcRoundedHullClearsOverheadCornerButBlocksWallsAndSupportsFloor)
    {
        MWClass::Static::registerSelf();
        MWBase::Environment environment;
        MWWorld::ESMStore store;
        environment.setESMStore(store);
        btDefaultCollisionConfiguration configuration;
        btCollisionDispatcher dispatcher(&configuration);
        btDbvtBroadphase broadphase;
        btCollisionWorld world(&dispatcher, &broadphase, &configuration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        ESM::Static base;
        base.blank();
        ESM::CellRef reference;
        reference.blank();
        MWWorld::LiveCellRef<ESM::Static> live(reference, &base);
        const BoundsActorClass npc(true), creature(false);
        MWWorld::Ptr ptr(&live);
        osg::ref_ptr<Resource::BulletShape> source = new Resource::BulletShape;
        source->mCollisionBox.mExtents = osg::Vec3f(29, 28, 66);
        source->mCollisionBox.mCenter = osg::Vec3f(0, 0, 66);
        const osg::Vec3f supplied(29, 28, 66);
        for (int kind : {0, 1, 2})
        {
            SCOPED_TRACE(kind); // native NPC, legacy NPC, native creature
            live.mClass = kind == 2 ? &creature : &npc;
            MWPhysics::Actor actor(ptr, source, &scheduler, false,
                DetourNavigator::CollisionShapeType::Aabb, kind == 1 ? osg::Vec3f{} : supplied);
            const auto* object = actor.getCollisionObject();
            EXPECT_EQ(object->getCollisionShape()->getShapeType(),
                kind == 0 ? CAPSULE_SHAPE_PROXYTYPE : BOX_SHAPE_PROXYTYPE);
            btVector3 minimum, maximum;
            object->getCollisionShape()->getAabb(btTransform::getIdentity(), minimum, maximum);
            EXPECT_NEAR(maximum.x(), 29, .01);
            EXPECT_NEAR(maximum.y(), kind == 0 ? 29 : 28, .01);
            EXPECT_NEAR(maximum.z(), 66, .01);
            EXPECT_NEAR(minimum.z(), -66, .01);
            EXPECT_EQ(actor.getHalfExtents(), kind == 0 ? osg::Vec3f(29, 29, 66) : supplied);

            auto sweep = [&](const btVector3& extent, const btVector3& origin,
                             const osg::Vec3f& from, const osg::Vec3f& to) {
                btBoxShape shape(extent);
                btCollisionObject obstacle;
                obstacle.setCollisionShape(&shape);
                obstacle.setWorldTransform(btTransform(btQuaternion::getIdentity(), origin));
                world.addCollisionObject(&obstacle, MWPhysics::CollisionType_World, MWPhysics::CollisionType_Actor);
                MWPhysics::ActorTracer tracer;
                tracer.doTrace(object, from, to, &world);
                world.removeCollisionObject(&obstacle);
                return tracer;
            };
            // A corner near the head clips a flat-ended box, but lies outside
            // the rounded head. A full-height wall must still stop both hulls.
            auto corner = sweep(btVector3(10, 100, 5), btVector3(60, 0, 67), {}, osg::Vec3f(30, 0, 0));
            if (kind == 0) EXPECT_EQ(corner.mFraction, 1.f);
            else EXPECT_LT(corner.mFraction, 1.f);
            auto wall = sweep(btVector3(10, 100, 100), btVector3(60, 0, 0), {}, osg::Vec3f(40, 0, 0));
            EXPECT_GT(wall.mFraction, .4f);
            EXPECT_LT(wall.mFraction, .6f);
            auto floor = sweep(btVector3(100, 100, 5), btVector3(0, 0, -5),
                osg::Vec3f(0, 0, 70), osg::Vec3f(0, 0, 60));
            EXPECT_NEAR(floor.mFraction, .4f, .01f);
            EXPECT_GT(floor.mPlaneNormal.z(), .99f);
        }
    }

    TEST(ActorBoundsTest, NativeMeleeAdapterUsesPhysicalVerticalBoundsAndOriginalMaximumY)
    {
        MWClass::Static::registerSelf();
        MWBase::Environment environment;
        MWWorld::ESMStore store;
        environment.setESMStore(store);
        btDefaultCollisionConfiguration configuration;
        btCollisionDispatcher dispatcher(&configuration);
        btDbvtBroadphase broadphase;
        btCollisionWorld world(&dispatcher, &broadphase, &configuration);
        MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
        ESM::Static base;
        base.blank();
        ESM::CellRef reference;
        reference.blank();
        MWWorld::LiveCellRef<ESM::Static> live(reference, &base);
        const BoundsActorClass type(false);
        live.mClass = &type;
        MWWorld::Ptr ptr(&live);
        ptr.getCellRef().setScale(2);
        ESM::Position position;
        position.pos[0] = 100;
        position.pos[1] = 200;
        position.pos[2] = 300;
        position.rot[0] = position.rot[1] = position.rot[2] = 0;
        ptr.getRefData().setPosition(position);
        osg::ref_ptr<Resource::BulletShape> shape = new Resource::BulletShape;
        shape->mCollisionBox.mCenter = osg::Vec3f(1, 2, 9);
        shape->mCollisionBox.mExtents = osg::Vec3f(7, 8, 9);
        MWPhysics::Actor body(ptr, shape, &scheduler, false, DetourNavigator::CollisionShapeType::Aabb, {});
        EXPECT_EQ(body.getOriginalCollisionCenter(), osg::Vec3f(1, 2, 9));
        const auto geometry = MWMechanics::oblivionMeleeBody(body, 1.5f, true);
        const std::array<float, 3> expectedPosition{100, 200, 300};
        EXPECT_EQ(geometry.mPosition, expectedPosition);
        EXPECT_FLOAT_EQ(geometry.mMinimumZ, 0);
        EXPECT_FLOAT_EQ(geometry.mMaximumZ, 36);
        EXPECT_FLOAT_EQ(geometry.mMaximumY, 10); // Local center Y + half extent, not scaled width.
        EXPECT_FLOAT_EQ(geometry.mScale, 1.5f); // Native reach scale supplied independently of body scale2.
        EXPECT_TRUE(geometry.mSwimming);
        EXPECT_TRUE(geometry.mIsActor);
    }

}

namespace
{
    TEST(ActorBoundsTest, RetainedCapsuleIgnoresOnlyOwnedBodiesAcrossPublicationAndRemoval)
    {
        struct RestoreThreads
        {
            int mPrevious = Settings::physics().mAsyncNumThreads;
            ~RestoreThreads() { Settings::physics().mAsyncNumThreads.set(mPrevious); }
        } restoreThreads;
        MWClass::Static::registerSelf();
        MWBase::Environment environment;
        MWWorld::ESMStore store;
        environment.setESMStore(store);
        const BoundsActorClass type(false);
        for (int threads : {0, 1, 2})
        {
            SCOPED_TRACE(threads);
            Settings::physics().mAsyncNumThreads.set(threads);
            btDefaultCollisionConfiguration configuration;
            btCollisionDispatcher dispatcher(&configuration);
            btDbvtBroadphase broadphase;
            btSequentialImpulseConstraintSolver solver;
            NifBullet::NativeDynamicsWorld world(&dispatcher, &broadphase, &solver, &configuration);
            world.setGravity({0, 0, 0});
            ESM::Static base; base.blank();
            ESM::CellRef reference; reference.blank();
            MWWorld::LiveCellRef<ESM::Static> live(reference, &base); live.mClass = &type;
            MWWorld::Ptr ptr(&live);
            MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
            osg::ref_ptr<Resource::BulletShape> shape = new Resource::BulletShape;
            shape->mCollisionBox.mExtents = {1, 1, 2};
            btCollisionObject unrelated;
            auto actor = std::make_shared<MWPhysics::Actor>(ptr, shape, &scheduler, false,
                DetourNavigator::CollisionShapeType::Aabb, osg::Vec3f{});
            auto* capsule = actor->getCollisionObject();
            capsule->setIgnoreCollisionCheck(&unrelated, true);
            NifBullet::ActorRagdollDefinition graph;
            NifBullet::RagdollBodyDefinition body{};
            body.mRecord = 12; body.mNodeRecord = 8; body.mMass = 2;
            body.mInertia = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            body.mShape = NifBullet::RagdollSphere{.5f}; graph.mBodies.push_back(body);
            const std::array<btTransform, 1> poses{btTransform::getIdentity()};
            const auto prepare = [&] { return scheduler.prepareActorRagdoll(ptr, graph, 1, poses,
                MWPhysics::CollisionType_Actor, MWPhysics::CollisionType_Actor | MWPhysics::CollisionType_World,
                nullptr, actor); };
            {
                auto cancelled = prepare();
                EXPECT_EQ(capsule->getNumObjectsWithoutCollision(), 1);
                EXPECT_TRUE(cancelled->physics().collisionObjects()[0]->checkCollideWith(capsule));
            }
            EXPECT_EQ(capsule->getNumObjectsWithoutCollision(), 1);
            auto prepared = prepare(); auto* physical = prepared->physics().collisionObjects()[0];
            ASSERT_TRUE(scheduler.commitActorRagdoll(*prepared));
            EXPECT_FALSE(actor->isCollisionSuspended());
            EXPECT_NE(capsule->getBroadphaseHandle(), nullptr);
            EXPECT_FALSE(capsule->checkCollideWith(physical));
            EXPECT_FALSE(physical->checkCollideWith(capsule));
            EXPECT_FALSE(dispatcher.needsCollision(physical, capsule));
            EXPECT_TRUE(physical->checkCollideWith(&unrelated));
            EXPECT_EQ(capsule->getNumObjectsWithoutCollision(), 2);
            scheduler.removeActorRagdoll(ptr);
            EXPECT_EQ(capsule->getNumObjectsWithoutCollision(), 1);
            EXPECT_FALSE(capsule->checkCollideWith(&unrelated));
            auto stale = prepare();
            actor.reset();
            EXPECT_FALSE(scheduler.validatePreparedActorRagdoll(*stale));
            EXPECT_FALSE(scheduler.commitActorRagdoll(*stale));
        }
    }
}

// Exercise real capsule sweeps, including rejection when headroom is insufficient.
TEST(ActorBoundsTest, CapsuleStepsAcrossLowThresholdUnderOverhang)
{
    for (const float ceiling : {150.f, 136.f})
    {
        SCOPED_TRACE(ceiling);
        btDefaultCollisionConfiguration configuration;
        btCollisionDispatcher dispatcher(&configuration);
        btDbvtBroadphase broadphase;
        btCollisionWorld world(&dispatcher, &broadphase, &configuration);
        btBoxShape floorShape(btVector3(100, 200, 10));
        btBoxShape stepShape(btVector3(100, 100, 4));
        btBoxShape roofShape(btVector3(100, 100, 10));
        btCapsuleShapeZ capsule(29.28f, 2.f * (66.5f - 29.28f));
        floorShape.setMargin(.001f);
        stepShape.setMargin(.001f);
        roofShape.setMargin(.001f);
        capsule.setMargin(.001f);
        btCollisionObject floor, threshold, roof, actor;
        const auto add = [&](btCollisionObject& object, btCollisionShape& shape,
                             const btVector3& origin, int group) {
            object.setCollisionShape(&shape);
            object.setWorldTransform(btTransform(btQuaternion::getIdentity(), origin));
            world.addCollisionObject(&object, group, MWPhysics::CollisionType_World | MWPhysics::CollisionType_Actor);
        };
        add(floor, floorShape, btVector3(0, 0, -10), MWPhysics::CollisionType_World);
        add(threshold, stepShape, btVector3(0, 100, 4), MWPhysics::CollisionType_World);
        add(roof, roofShape, btVector3(0, 100, ceiling + 10), MWPhysics::CollisionType_World);
        osg::Vec3f position(0, -21, 66.5f);
        add(actor, capsule, btVector3(0, -21, 66.5f), MWPhysics::CollisionType_Actor);
        MWPhysics::Stepper stepper(&world, &actor);
        for (int tick = 0; tick < 150; ++tick)
        {
            osg::Vec3f velocity(0, 52, 0);
            float remaining = 1.f / 60.f;
            MWPhysics::ActorTracer trace;
            const auto destination = position + velocity * remaining;
            trace.doTrace(&actor, position, destination, &world, true);
            if (trace.mFraction >= 1.f)
                position = destination;
            else
                stepper.step(position, velocity, remaining, true, true);
            actor.setWorldTransform(btTransform(btQuaternion::getIdentity(),
                btVector3(position.x(), position.y(), position.z())));
            world.updateSingleAabb(&actor);
        }
        if (ceiling == 150.f)
        {
            EXPECT_GT(position.y(), 50.f);
            EXPECT_NEAR(position.z(), 74.5f, 1.f);
        }
        else
        {
            EXPECT_LT(position.y(), 0.f);
            // The rounded cap can rise outside the lip of the overhang.
            // Check actual overlap rather than its enclosing box's top.
            struct Overlap : btCollisionWorld::ContactResultCallback
            {
                btScalar mDeepest = 0;
                btScalar addSingleResult(btManifoldPoint& contact,
                    const btCollisionObjectWrapper*, int, int,
                    const btCollisionObjectWrapper*, int, int) override
                {
                    mDeepest = std::min(mDeepest, contact.getDistance());
                    return 0;
                }
            } overlap;
            world.contactPairTest(&actor, &roof, overlap);
            EXPECT_GE(overlap.mDeepest, -.05f);
        }
        world.removeCollisionObject(&actor);
        world.removeCollisionObject(&roof);
        world.removeCollisionObject(&threshold);
        world.removeCollisionObject(&floor);
    }
}

TEST(ActorBoundsTest, GroundedOverlapRecoveryRetainsLateralSeparationBetweenOverlappingWalls)
{
    MWClass::Static::registerSelf();
    TestingOpenMW::NativeWorldFixture fixture;
    const BoundsActorClass type(false);
    btDefaultCollisionConfiguration configuration;
    btCollisionDispatcher dispatcher(&configuration);
    btDbvtBroadphase broadphase;
    btCollisionWorld world(&dispatcher, &broadphase, &configuration);
    MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
    btBoxShape wallShape(btVector3(10, 100, 100));
    btCollisionObject wall, oppositeWall;
    const auto add = [&](btCollisionObject& object, float x) {
        object.setCollisionShape(&wallShape);
        object.setWorldTransform(btTransform(btQuaternion::getIdentity(), btVector3(x, 0, 0)));
        world.addCollisionObject(&object, MWPhysics::CollisionType_World, MWPhysics::CollisionType_Actor);
    };
    add(wall, -20);
    add(oppositeWall, 25);
    ESM::Static base;
    base.blank();
    ESM::CellRef reference;
    reference.blank();
    reference.mPos.pos[2] = 1;
    MWWorld::LiveCellRef<ESM::Static> live(reference, &base);
    live.mClass = &type;
    osg::ref_ptr<Resource::BulletShape> source = new Resource::BulletShape;
    source->mCollisionBox.mExtents = osg::Vec3f(20, 30, 22);
    source->mCollisionBox.mCenter = osg::Vec3f(0, 0, 22);
    {
        MWPhysics::Actor actor(MWWorld::Ptr(&live), source, &scheduler, false,
            DetourNavigator::CollisionShapeType::Aabb, {});
        actor.setOnGround(true);
        MWPhysics::ActorFrameData frame(actor, false, false, 1.f, -1000.f, false);
        // Both walls initially overlap the hull. Preserve the solver's
        // velocity-filtered separation from the deeper contact: a sweep from
        // this penetrated start must not veto it at the opposite wall.
        frame.mMovement = osg::Vec3f(-1, 0, 0);
        frame.mPosition = osg::Vec3f(0, 0, 1);
        const auto transform = actor.getCollisionObject()->getWorldTransform();
        MWPhysics::MovementSolver::unstuck(frame, &world);
        EXPECT_GT(frame.mPosition.x(), 9.f);
        EXPECT_FLOAT_EQ(frame.mPosition.z(), 1.f);
        EXPECT_EQ(actor.getCollisionObject()->getWorldTransform(), transform);
    }
    world.removeCollisionObject(&oppositeWall);
    world.removeCollisionObject(&wall);
}

TEST(ActorBoundsTest, GroundedOverlapRejectionDoesNotPushActorThroughSupportingFloor)
{
    MWClass::Static::registerSelf();
    TestingOpenMW::NativeWorldFixture fixture;
    const BoundsActorClass type(false);
    btDefaultCollisionConfiguration configuration;
    btCollisionDispatcher dispatcher(&configuration);
    btDbvtBroadphase broadphase;
    btCollisionWorld world(&dispatcher, &broadphase, &configuration);
    MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
    btBoxShape floorShape(btVector3(100, 100, 10));
    btBoxShape lipShape(btVector3(100, 100, 20));
    btCollisionObject floor, lip;
    const auto add = [&](btCollisionObject& object, btCollisionShape& shape, float z) {
        object.setCollisionShape(&shape);
        object.setWorldTransform(btTransform(btQuaternion::getIdentity(), btVector3(0, 0, z)));
        world.addCollisionObject(&object, MWPhysics::CollisionType_World, MWPhysics::CollisionType_Actor);
    };
    add(floor, floorShape, -10);
    add(lip, lipShape, 45);
    ESM::Static base;
    base.blank();
    ESM::CellRef reference;
    reference.blank();
    reference.mPos.pos[2] = 1;
    MWWorld::LiveCellRef<ESM::Static> live(reference, &base);
    live.mClass = &type;
    osg::ref_ptr<Resource::BulletShape> source = new Resource::BulletShape;
    source->mCollisionBox.mExtents = osg::Vec3f(20, 30, 22);
    source->mCollisionBox.mCenter = osg::Vec3f(0, 0, 22);
    {
        MWPhysics::Actor actor(MWWorld::Ptr(&live), source, &scheduler, false,
            DetourNavigator::CollisionShapeType::Aabb, {});
        for (float floorHeight : {0.f, -100.f})
        {
            SCOPED_TRACE(floorHeight);
            floor.setWorldTransform(btTransform(btQuaternion::getIdentity(), btVector3(0, 0, floorHeight - 10)));
            world.updateSingleAabb(&floor);
            for (bool grounded : {false, true})
            {
                SCOPED_TRACE(grounded);
                actor.setOnGround(grounded);
                MWPhysics::ActorFrameData frame(actor, false, false, 1.f, -1000.f, false);
                frame.mMovement = osg::Vec3f(0, 1, 0);
                frame.mPosition = osg::Vec3f(0, 0, 1);
                frame.mStuckFrames = 0;
                const auto transform = actor.getCollisionObject()->getWorldTransform();
                MWPhysics::MovementSolver::unstuck(frame, &world);
                // Less overlap with the lip must not put either a grounded or
                // falling actor below a floor. With the floor farther away,
                // falling actors may still recover downward into open air.
                EXPECT_GE(frame.mPosition.z(), floorHeight - .01f);
                if (grounded)
                {
                    EXPECT_GE(frame.mPosition.z(), 1.f);
                }
                else
                {
                    EXPECT_LT(frame.mPosition.z(), 1.f);
                    if (floorHeight < 0)
                    {
                        EXPECT_LT(frame.mPosition.z(), 0.f);
                    }
                }
                EXPECT_EQ(actor.getCollisionObject()->getWorldTransform(), transform);
            }
        }
    }
    world.removeCollisionObject(&lip);
    world.removeCollisionObject(&floor);
}

TEST(ActorBoundsTest, NativeCharacterPushesOnlyOwnedDynamicSceneryAtTheSolverBarrier)
{
    struct RestoreThreads
    {
        int mPrevious = Settings::physics().mAsyncNumThreads;
        ~RestoreThreads() { Settings::physics().mAsyncNumThreads.set(mPrevious); }
    } restoreThreads;
    MWClass::Static::registerSelf();
    TestingOpenMW::NativeWorldFixture fixture;
    const BoundsActorClass type(true);
    std::optional<btVector3> incomingWithPush;
    for (int threads : {0, 1, 2})
        for (int course = 0; course < 6; ++course)
        {
            SCOPED_TRACE(threads);
            SCOPED_TRACE(course); // owned/native, legacy, fixed, foreign, incoming native/legacy
            Settings::physics().mAsyncNumThreads.set(threads);
            btDefaultCollisionConfiguration configuration;
            btCollisionDispatcher dispatcher(&configuration);
            btDbvtBroadphase broadphase;
            btSequentialImpulseConstraintSolver solver;
            NifBullet::NativeDynamicsWorld world(&dispatcher, &broadphase, &solver, &configuration);
            world.setGravity({0, 0, 0});
            MWPhysics::PhysicsTaskScheduler scheduler(1.f / 60.f, &world, nullptr);
            btBoxShape floorShape({1000, 1000, 10});
            btCollisionObject floor;
            floor.setCollisionShape(&floorShape);
            floor.setWorldTransform(btTransform(btQuaternion::getIdentity(), {0, 0, -10}));
            world.addCollisionObject(&floor, MWPhysics::CollisionType_World, MWPhysics::CollisionType_AnyPhysical);
            ESM::Static base;
            base.blank();
            ESM::CellRef reference;
            reference.blank();
            reference.mPos.pos[1] = course >= 4 ? -43 : -100;
            MWWorld::LiveCellRef<ESM::Static> live(reference, &base);
            live.mClass = &type;
            osg::ref_ptr<Resource::BulletShape> source = new Resource::BulletShape;
            source->mCollisionBox.mCenter = {0, 0, 66.5f};
            source->mCollisionBox.mExtents = {20, 20, 66.5f};
            auto actor = std::make_shared<MWPhysics::Actor>(MWWorld::Ptr(&live), source, &scheduler, false,
                DetourNavigator::CollisionShapeType::Aabb, source->mCollisionBox.mExtents);
            actor->setOnGround(true);
            ESM::CellRef sceneryReference;
            sceneryReference.blank();
            MWWorld::LiveCellRef<ESM::Static> scenery(sceneryReference, &base), distant(sceneryReference, &base);
            const MWWorld::Ptr target(&scenery), other(&distant);
            NifBullet::ActorRagdollDefinition graph;
            graph.mSourceHash = std::string(16, 'a');
            NifBullet::RagdollBodyDefinition anchor{};
            anchor.mRecord = 10; anchor.mNodeRecord = 1; anchor.mFixed = true;
            anchor.mShape = NifBullet::RagdollSphere{1};
            NifBullet::RagdollBodyDefinition bob{};
            bob.mRecord = 11; bob.mNodeRecord = 2; bob.mMass = 1;
            bob.mInertia = {300, 0, 0, 0, 300, 0, 0, 0, 300};
            bob.mShape = NifBullet::RagdollSphere{28};
            bob.mMaxLinearVelocity = bob.mMaxAngularVelocity = 1000;
            if (course == 2)
            {
                bob.mFixed = true;
                bob.mMass = 0;
                bob.mInertia = {};
            }
            graph.mBodies = {anchor, bob};
            NifBullet::RagdollConeJoint joint{};
            joint.mA = {{0, 0, 90}, {0, 0, 1}, {1, 0, 0}};
            joint.mB = {{0, 0, 0}, {0, 0, 1}, {1, 0, 0}};
            joint.mConeAngle = 1.5;
            joint.mPlaneMin = joint.mTwistMin = -1.5;
            joint.mPlaneMax = joint.mTwistMax = 1.5;
            if (course != 2) graph.mJoints.push_back({12, 1, 0, false, 1, 1, joint});
            const std::array poses{btTransform(btQuaternion::getIdentity(), btVector3(0, 0, 180)),
                btTransform(btQuaternion::getIdentity(), btVector3(0, 0, 90))};
            const std::array motions{
                NifBullet::RagdollNativeMotionRequest{10, NifBullet::RagdollNativeMotion::Fixed},
                NifBullet::RagdollNativeMotionRequest{11, course == 2
                    ? NifBullet::RagdollNativeMotion::Fixed : NifBullet::RagdollNativeMotion::Dynamic}};
            const int group = MWPhysics::CollisionType_World | MWPhysics::CollisionType_LooseObject;
            if (course == 3)
            {
                scheduler.addActorRagdoll(target, graph, 1, poses, group, MWPhysics::CollisionType_AnyPhysical);
                auto farPoses = poses;
                for (auto& pose : farPoses) pose.getOrigin().setX(1000);
                scheduler.addAnimatedReferenceBodies(other, graph, 1, farPoses, group,
                    MWPhysics::CollisionType_AnyPhysical, motions);
            }
            else
                scheduler.addAnimatedReferenceBodies(target, graph, 1, poses, group,
                    MWPhysics::CollisionType_AnyPhysical, motions);
            if (course >= 4)
            {
                const auto key = ESM::FormKey::content("props.esm", 100);
                auto saved = scheduler.captureAnimatedReferenceSnapshot(target, key, "trap.nif");
                saved.mBodies[1].mLinearVelocity = {0, -250, 0};
                saved.mBodies[1].mNativePackedVelocity->mLinear = {0, -250, 0, 0};
                scheduler.restoreAnimatedReferenceSnapshot(target, saved, key, "trap.nif");
            }
            MWPhysics::WorldFrameData worldData(false, {});
            worldData.mNativeCharacterPushes = course != 1 && course != 5;
            osg::ref_ptr<osg::Stats> stats = new osg::Stats("character contact");
            std::array<std::vector<MWPhysics::Simulation>, 2> simulations;
            float maximumDisplacement = 0;
            for (unsigned frame = 0; frame < (course >= 4 ? 1u : 150u); ++frame)
            {
                auto& current = simulations[frame % 2];
                actor->setVelocity({0, 80, 0});
                current.emplace_back(MWPhysics::ActorSimulation{actor,
                    MWPhysics::ActorFrameData(*actor, false, false, 1, -1000, false)});
                float elapsed = 1.f / 60.f;
                scheduler.applyQueuedMovements(elapsed, current, osg::Timer::instance()->tick(), frame,
                    *stats, worldData);
                const auto states = course == 3 ? scheduler.captureActorRagdoll(target)
                    : scheduler.captureAnimatedReferenceBodies(target);
                EXPECT_NEAR(states[0].mPose.getOrigin().z(), 180, .01);
                maximumDisplacement = std::max(maximumDisplacement, float(states[1].mPose.getOrigin().y()));
                if (course == 4) incomingWithPush = states[1].mLinearVelocity;
                if (course == 5)
                {
                    ASSERT_TRUE(incomingWithPush);
                    EXPECT_LT((states[1].mLinearVelocity - *incomingWithPush).length(), 1e-5);
                }
            }
            if (course == 0)
            {
                EXPECT_GT(maximumDisplacement, 20);
                EXPECT_GT(actor->getPosition().y(), 20);
                const auto key = ESM::FormKey::content("props.esm", 100);
                const auto saved = scheduler.captureAnimatedReferenceSnapshot(target, key, "trap.nif");
                scheduler.removeAnimatedReferenceBodies(target);
                scheduler.addAnimatedReferenceBodies(target, graph, 1, poses, group,
                    MWPhysics::CollisionType_AnyPhysical, motions);
                scheduler.restoreAnimatedReferenceSnapshot(target, saved, key, "trap.nif");
                EXPECT_EQ(scheduler.captureAnimatedReferenceSnapshot(target, key, "trap.nif"), saved);
            }
            else
            {
                if (course < 4)
                {
                    EXPECT_LT(maximumDisplacement, 1);
                }
                EXPECT_LT(actor->getPosition().y(), -30);
            }
            actor.reset();
            if (course == 3)
            {
                scheduler.removeActorRagdoll(target);
                scheduler.removeAnimatedReferenceBodies(other);
            }
            else scheduler.removeAnimatedReferenceBodies(target);
            world.removeCollisionObject(&floor);
        }
}
