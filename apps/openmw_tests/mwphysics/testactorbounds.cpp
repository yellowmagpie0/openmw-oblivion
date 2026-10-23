#include <apps/openmw/mwmechanics/oblivionmelee.hpp>
#include <apps/openmw/mwphysics/actor.hpp>
#include <apps/openmw/mwphysics/mtphysics.hpp>
#include <apps/openmw/mwbase/environment.hpp>
#include <apps/openmw/mwmechanics/creaturestats.hpp>
#include <apps/openmw/mwclass/static.hpp>
#include <apps/openmw/mwworld/esmstore.hpp>
#include <apps/openmw/mwworld/livecellref.hpp>

#include <components/esm3/loadstat.hpp>
#include <components/resource/bulletshape.hpp>
#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/settings/values.hpp>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>

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
