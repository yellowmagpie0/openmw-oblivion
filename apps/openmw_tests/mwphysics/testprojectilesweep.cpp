#include <apps/openmw/mwphysics/collisiontype.hpp>
#include <apps/openmw/mwphysics/movementsolver.hpp>
#include <apps/openmw/mwphysics/mtphysics.hpp>
#include <apps/openmw/mwphysics/physicssystem.hpp>
#include <apps/openmw/mwphysics/projectile.hpp>
#include <apps/openmw/mwphysics/projectileconvexcallback.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/settings/values.hpp>
#include <components/vfs/manager.hpp>
#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <gtest/gtest.h>
#include <array>
#include <thread>
#include <osg/Group>

namespace
{
    struct ProjectileSweepTest : testing::Test
    {
        struct Threads
        {
            int mOld = Settings::physics().mAsyncNumThreads;
            Threads() { Settings::physics().mAsyncNumThreads.set(0); }
            ~Threads() { Settings::physics().mAsyncNumThreads.set(mOld); }
        } mThreads;
        VFS::Manager mVfs;
        Resource::ResourceSystem mResources{&mVfs, 0., nullptr};
        MWPhysics::PhysicsSystem mPhysics{&mResources, new osg::Group};
        btDefaultCollisionConfiguration mConfiguration;
        btCollisionDispatcher mDispatcher{&mConfiguration};
        btDbvtBroadphase mBroadphase;
        btCollisionWorld mWorld{&mDispatcher, &mBroadphase, &mConfiguration};
        MWPhysics::PhysicsTaskScheduler mScheduler{1.f / 60.f, &mWorld, nullptr};
        MWPhysics::Projectile mProjectile{{}, osg::Vec3f{}, .02f, &mScheduler, &mPhysics};

        struct Wall
        {
            btCollisionWorld& mWorld;
            btBoxShape mShape{btVector3(.01, 1, 1)};
            btCollisionObject mObject;
            Wall(btCollisionWorld& world, float x, int group = MWPhysics::CollisionType_World)
                : mWorld(world)
            {
                mShape.setMargin(0);
                mObject.setCollisionShape(&mShape);
                mObject.setWorldTransform(btTransform(btQuaternion::getIdentity(), btVector3(x, 0, 0)));
                world.addCollisionObject(&mObject, group, MWPhysics::CollisionType_Projectile);
            }
            ~Wall() { mWorld.removeCollisionObject(&mObject); }
        };

        static void candidate(MWPhysics::ProjectileConvexCallback& callback,
            const btCollisionObject& object, float fraction, float x)
        {
            btCollisionWorld::LocalConvexResult result(&object, nullptr,
                btVector3(-1, 0, 0), btVector3(x, 0, 0), fraction);
            callback.addSingleResult(result, true);
        }
    };

    TEST_F(ProjectileSweepTest, PublishesOnlyTheFinalClosestCandidateOnce)
    {
        Wall far(mWorld, 8), near(mWorld, 3);
        MWPhysics::ProjectileConvexCallback callback(nullptr, mProjectile.getCollisionObject(),
            btVector3(0, 0, 0), btVector3(10, 0, 0), mProjectile);
        candidate(callback, far.mObject, .8f, 8);
        EXPECT_TRUE(mProjectile.isActive());
        candidate(callback, near.mObject, .3f, 3);
        EXPECT_TRUE(mProjectile.isActive());
        callback.commitHit();
        EXPECT_FALSE(mProjectile.isActive());
        EXPECT_EQ(mProjectile.getHitPosition(), btVector3(3, 0, 0));
        callback.commitHit();
        EXPECT_EQ(mProjectile.getHitPosition(), btVector3(3, 0, 0));
    }

    TEST_F(ProjectileSweepTest, IgnoresCasterAndSelfAndDoesNotPublishFartherWater)
    {
        Wall caster(mWorld, 1), water(mWorld, 8, MWPhysics::CollisionType_Water), near(mWorld, 3);
        MWPhysics::ProjectileConvexCallback callback(&caster.mObject, mProjectile.getCollisionObject(),
            btVector3(0, 0, 0), btVector3(10, 0, 0), mProjectile);
        candidate(callback, caster.mObject, .1f, 1);
        candidate(callback, *mProjectile.getCollisionObject(), .2f, 2);
        EXPECT_FALSE(callback.hasHit());
        EXPECT_TRUE(mProjectile.isActive());
        candidate(callback, water.mObject, .8f, 8);
        candidate(callback, near.mObject, .3f, 3);
        callback.commitHit();
        EXPECT_FALSE(mProjectile.isActive());
        EXPECT_FALSE(mProjectile.getHitWater());
        EXPECT_EQ(mProjectile.getHitPosition(), btVector3(3, 0, 0));
    }

    TEST_F(ProjectileSweepTest, ProductionMovementSweepsAThousandUnitsIntoAThinWall)
    {
        Wall caster(mWorld, 2), target(mWorld, 5);
        mProjectile.setVelocity(osg::Vec3f(100000, 0, 0));
        MWPhysics::ProjectileFrameData frame(mProjectile);
        frame.mCaster = &caster.mObject;
        MWPhysics::MovementSolver::move(frame, .01f, &mWorld);
        EXPECT_FALSE(mProjectile.isActive());
        EXPECT_NEAR(mProjectile.getHitPosition().x(), 4.99, .005);
        EXPECT_NEAR(frame.mPosition.x(), 4.99, .005);
        EXPECT_NEAR(frame.mPosition.y(), 0, .005);
        EXPECT_NEAR(frame.mPosition.z(), 0, .005);
        EXPECT_FALSE(mProjectile.getHitWater());
    }

    TEST_F(ProjectileSweepTest, EmptyAndZeroMotionSweepsLeaveTheProjectileActive)
    {
        MWPhysics::ProjectileFrameData frame(mProjectile);
        MWPhysics::MovementSolver::move(frame, .1f, &mWorld);
        EXPECT_TRUE(mProjectile.isActive());
        EXPECT_EQ(frame.mPosition, osg::Vec3f{});
        mProjectile.setVelocity(osg::Vec3f(100, 0, 0));
        frame = MWPhysics::ProjectileFrameData(mProjectile);
        MWPhysics::MovementSolver::move(frame, .1f, &mWorld);
        EXPECT_TRUE(mProjectile.isActive());
        EXPECT_EQ(frame.mPosition, osg::Vec3f(10, 0, 0));
    }
    TEST_F(ProjectileSweepTest, ConcurrentHitsPublishOneCompleteResultAndCannotOverwriteIt)
    {
        Wall target(mWorld, 5);
        std::array<bool, 16> won{};
        std::array<std::thread, 16> producers;
        std::atomic<bool> start{false};
        for (unsigned i = 0; i < producers.size(); ++i)
            producers[i] = std::thread([&, i] {
                while (!start.load(std::memory_order_acquire))
                    std::this_thread::yield();
                won[i] = mProjectile.hit(&target.mObject, btVector3(i + 1, i + 2, i + 3),
                    btVector3(0, 0, 1), i % 2);
            });
        start.store(true, std::memory_order_release);
        while (mProjectile.isActive())
            std::this_thread::yield();
        // Read immediately after the acquire sees inactive, before joining.
        const auto observed = mProjectile.getHitPosition();
        const bool water = mProjectile.getHitWater();
        for (auto& producer : producers)
            producer.join();
        unsigned winners = 0;
        for (unsigned i = 0; i < won.size(); ++i)
            if (won[i])
            {
                ++winners;
                EXPECT_EQ(observed, btVector3(i + 1, i + 2, i + 3));
                EXPECT_EQ(water, bool(i % 2));
            }
        EXPECT_EQ(winners, 1);
        EXPECT_FALSE(mProjectile.hit(&target.mObject, btVector3(100, 200, 300),
            btVector3(1, 0, 0), !water));
        EXPECT_EQ(mProjectile.getHitPosition(), observed);
        EXPECT_EQ(mProjectile.getHitWater(), water);
    }

    TEST_F(ProjectileSweepTest, FartherArrowIsNotHitWhenTheWallWins)
    {
        MWPhysics::Projectile other({}, osg::Vec3f(8, 0, 0), .02f, &mScheduler, &mPhysics);
        Wall near(mWorld, 3);
        MWPhysics::ProjectileConvexCallback callback(nullptr, mProjectile.getCollisionObject(),
            btVector3(0, 0, 0), btVector3(10, 0, 0), mProjectile);
        candidate(callback, *other.getCollisionObject(), .8f, 8);
        candidate(callback, near.mObject, .3f, 3);
        callback.commitHit();
        EXPECT_FALSE(mProjectile.isActive());
        EXPECT_TRUE(other.isActive());
        EXPECT_EQ(mProjectile.getHitPosition(), btVector3(3, 0, 0));
    }

    TEST_F(ProjectileSweepTest, ClosestArrowPublishesBothHitsWithoutAResolvedShooter)
    {
        MWPhysics::Projectile other({}, osg::Vec3f(3, 0, 0), .02f, &mScheduler, &mPhysics);
        Wall far(mWorld, 8);
        MWPhysics::ProjectileConvexCallback callback(nullptr, mProjectile.getCollisionObject(),
            btVector3(0, 0, 0), btVector3(10, 0, 0), mProjectile);
        candidate(callback, far.mObject, .8f, 8);
        candidate(callback, *other.getCollisionObject(), .3f, 3);
        callback.commitHit();
        EXPECT_FALSE(mProjectile.isActive());
        EXPECT_FALSE(other.isActive());
        EXPECT_EQ(mProjectile.getHitPosition(), btVector3(3, 0, 0));
        EXPECT_EQ(other.getHitPosition(), btVector3(3, 0, 0));
        callback.commitHit();
        EXPECT_EQ(other.getHitPosition(), btVector3(3, 0, 0));
    }

}
