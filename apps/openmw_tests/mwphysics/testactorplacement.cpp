#include <apps/openmw/mwphysics/actorplacement.hpp>
#include <apps/openmw/mwphysics/collisiontype.hpp>
#include <apps/openmw/mwphysics/mtphysics.hpp>
#include <apps/openmw/mwphysics/ptrholder.hpp>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>

#include <gtest/gtest.h>

#include <limits>

namespace
{
    struct PlacementWorld
    {
        btDefaultCollisionConfiguration mConfiguration;
        btCollisionDispatcher mDispatcher{&mConfiguration};
        btDbvtBroadphase mBroadphase;
        btCollisionWorld mWorld{&mDispatcher, &mBroadphase, &mConfiguration};
        MWPhysics::PhysicsTaskScheduler mScheduler{1.f / 60.f, &mWorld, nullptr};
        MWPhysics::PtrHolder mHolder{{}, {}};
        std::vector<std::unique_ptr<btBoxShape>> mShapes;
        std::vector<std::unique_ptr<btCollisionObject>> mObjects;

        ~PlacementWorld()
        {
            for (const auto& object : mObjects) mWorld.removeCollisionObject(object.get());
        }

        btCollisionObject* box(btVector3 center, btVector3 halfExtents, int group)
        {
            mShapes.push_back(std::make_unique<btBoxShape>(halfExtents));
            auto object = std::make_unique<btCollisionObject>();
            object->setCollisionShape(mShapes.back().get());
            object->setWorldTransform(btTransform(btQuaternion::getIdentity(), center));
            if (group == MWPhysics::CollisionType_Actor) object->setUserPointer(&mHolder);
            auto* result = object.get();
            mWorld.addCollisionObject(result, group, MWPhysics::CollisionType_Actor);
            mObjects.push_back(std::move(object));
            return result;
        }

        bool clear(osg::Vec3f from, osg::Vec3f to, const btCollisionObject* ignored = nullptr)
        {
            return MWPhysics::isActorPlacementClear(mScheduler, ignored, from, to, {1, 1, 2});
        }
    };

    TEST(ActorPlacementTest, OccupiedArrivalRejectsActualBodyAndIgnoresOnlySelfOrNonblockingActors)
    {
        PlacementWorld scene;
        auto* actor = scene.box({0, 0, 2}, {1, 1, 2}, MWPhysics::CollisionType_Actor);
        EXPECT_FALSE(scene.clear({}, {}));
        EXPECT_TRUE(scene.clear({}, {}, actor));
        EXPECT_TRUE(scene.clear({}, {4, 0, 0}));
        EXPECT_EQ(actor->getWorldTransform().getOrigin(), btVector3(0, 0, 2));
        EXPECT_EQ(scene.mWorld.getNumCollisionObjects(), 1);
        scene.mHolder.setBlocksActorMovement(false);
        EXPECT_TRUE(scene.clear({}, {}));
    }

    TEST(ActorPlacementTest, FreeEndpointCannotRelocateThroughWallOrClosedDoor)
    {
        for (const int group : {MWPhysics::CollisionType_World, MWPhysics::CollisionType_Door})
        {
            SCOPED_TRACE(group);
            PlacementWorld scene;
            scene.box({4, 0, 4}, {.25, 4, 4}, group);
            EXPECT_TRUE(scene.clear({8, 0, 0}, {8, 0, 0}));
            EXPECT_FALSE(scene.clear({}, {8, 0, 0}));
            EXPECT_FALSE(scene.clear({}, {4, 0, 0}));
            EXPECT_TRUE(scene.clear({}, {0, 8, 0}));
        }
    }

    TEST(ActorPlacementTest, FloorTouchIsAllowedButPenetrationAndInvalidEnvelopeAreRejected)
    {
        PlacementWorld scene;
        scene.box({0, 0, -1}, {100, 100, 1}, MWPhysics::CollisionType_World);
        EXPECT_TRUE(scene.clear({}, {}));
        EXPECT_FALSE(scene.clear({}, {0, 0, -.1}));
        scene.box({0, 0, 4}, {5, 5, .5}, MWPhysics::CollisionType_World);
        EXPECT_FALSE(scene.clear({}, {}));
        EXPECT_THROW(MWPhysics::isActorPlacementClear(scene.mScheduler, nullptr, {}, {}, {0, 1, 2}),
            std::invalid_argument);
        EXPECT_THROW(scene.clear({}, {std::numeric_limits<float>::infinity(), 0, 0}), std::invalid_argument);
    }

    TEST(ActorPlacementTest, ActorOnlyOccupancyDoesNotTurnAnUnoccupiedMarkerIntoSceneRepair)
    {
        PlacementWorld scene;
        scene.box({0, 0, 2}, {2, 2, 2}, MWPhysics::CollisionType_World);
        EXPECT_FALSE(scene.clear({}, {}));
        EXPECT_TRUE(MWPhysics::isActorPlacementClear(scene.mScheduler, nullptr, {}, {}, {1, 1, 2}, true));
        auto* actor = scene.box({0, 0, 2}, {1, 1, 2}, MWPhysics::CollisionType_Actor);
        EXPECT_FALSE(MWPhysics::isActorPlacementClear(scene.mScheduler, nullptr, {}, {}, {1, 1, 2}, true));
        EXPECT_TRUE(MWPhysics::isActorPlacementClear(scene.mScheduler, actor, {}, {}, {1, 1, 2}, true));
    }
}
