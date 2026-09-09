#include <apps/openmw/mwphysics/collisiontype.hpp>
#include <apps/openmw/mwphysics/contacttestresultcallback.hpp>
#include <apps/openmw/mwphysics/ptrholder.hpp>
#include <apps/openmw/mwphysics/mtphysics.hpp>
#include <apps/openmw/mwphysics/object.hpp>
#include <apps/openmw/mwworld/livecellref.hpp>
#include <apps/openmw/mwclass/static.hpp>

#include <components/esm3/loadstat.hpp>
#include <components/resource/bulletshape.hpp>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btCollisionObjectWrapper.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>

#include <gtest/gtest.h>

namespace
{
    TEST(NativeTriggerTest, QueryOnlyVolumeUsesRotatedShapeNotItsBoundingBox)
    {
        btDefaultCollisionConfiguration configuration;
        btCollisionDispatcher dispatcher(&configuration);
        btDbvtBroadphase broadphase;
        btCollisionWorld world(&dispatcher, &broadphase, &configuration);
        btBoxShape volumeShape(btVector3(4, .25, .25));
        btBoxShape actorShape(btVector3(.1, .1, .1));
        btCollisionObject volume;
        volume.setCollisionShape(&volumeShape);
        volume.setWorldTransform(btTransform(btQuaternion(btVector3(0, 0, 1), SIMD_PI / 4)));
        volume.setCollisionFlags(volume.getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
        btCollisionObject actor;
        actor.setCollisionShape(&actorShape);
        MWPhysics::PtrHolder holder({}, {});
        actor.setUserPointer(&holder);
        world.addCollisionObject(&volume, 0, 0);
        world.addCollisionObject(&actor, MWPhysics::CollisionType_Actor, MWPhysics::CollisionType_World);
        auto overlaps = [&](btVector3 position) {
            actor.setWorldTransform(btTransform(btQuaternion::getIdentity(), position));
            world.updateSingleAabb(&actor);
            MWPhysics::ContactTestResultCallback callback(&volume, true);
            callback.m_collisionFilterGroup = MWPhysics::CollisionType_World;
            callback.m_collisionFilterMask = MWPhysics::CollisionType_Actor;
            world.contactTest(&volume, callback);
            return !callback.mResult.empty();
        };
        EXPECT_TRUE(overlaps(btVector3(1, 1, 0)));
        EXPECT_FALSE(overlaps(btVector3(2, -2, 0))); // Inside AABB, outside the rotated box.
        EXPECT_FALSE(overlaps(btVector3(0, 0, 1)));
        volumeShape.setLocalScaling(btVector3(1, 1, 8));
        world.updateSingleAabb(&volume);
        EXPECT_TRUE(overlaps(btVector3(0, 0, 1)));
        world.performDiscreteCollisionDetection();
        EXPECT_EQ(dispatcher.getNumManifolds(), 0); // No physical actor/volume pair.
        world.removeCollisionObject(&actor);
        world.removeCollisionObject(&volume);
    }

    TEST(NativeTriggerTest, OverlapQueryRejectsPositiveDistanceWithoutChangingLegacyContacts)
    {
        btBoxShape shape(btVector3(1, 1, 1));
        btCollisionObject volume;
        btCollisionObject actor;
        volume.setCollisionShape(&shape);
        actor.setCollisionShape(&shape);
        MWPhysics::PtrHolder holder({}, {});
        actor.setUserPointer(&holder);
        btCollisionObjectWrapper volumeWrapper(nullptr, &shape, &volume, btTransform::getIdentity(), -1, -1);
        btCollisionObjectWrapper actorWrapper(nullptr, &shape, &actor, btTransform::getIdentity(), -1, -1);
        btManifoldPoint point;
        point.m_distance1 = .01;
        MWPhysics::ContactTestResultCallback overlap(&volume, true);
        MWPhysics::ContactTestResultCallback legacy(&volume);
        overlap.addSingleResult(point, &volumeWrapper, 0, 0, &actorWrapper, 0, 0);
        legacy.addSingleResult(point, &volumeWrapper, 0, 0, &actorWrapper, 0, 0);
        EXPECT_TRUE(overlap.mResult.empty());
        EXPECT_EQ(legacy.mResult.size(), 1);
        point.m_distance1 = 0;
        overlap.addSingleResult(point, &actorWrapper, 0, 0, &volumeWrapper, 0, 0);
        EXPECT_EQ(overlap.mResult.size(), 1);
    }

    TEST(NativeTriggerTest, QueryOnlyObjectTracksTransformsAndLeavesNoCollisionObjectAfterRemoval)
    {
        MWClass::Static::registerSelf();
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
        MWWorld::Ptr ptr(&live);
        osg::ref_ptr<Resource::BulletShape> source = new Resource::BulletShape;
        source->mCollisionShape.reset(new btBoxShape(btVector3(1, 2, 3)));
        auto object = std::make_shared<MWPhysics::Object>(ptr, Resource::makeInstance(source), osg::Quat(),
            MWPhysics::CollisionType_World, &scheduler, true);
        EXPECT_FALSE(object->isSolid());
        ASSERT_EQ(world.getNumCollisionObjects(), 1);
        const auto* collision = object->getCollisionObject();
        EXPECT_FALSE(collision->hasContactResponse());
        EXPECT_EQ(collision->getBroadphaseHandle()->m_collisionFilterGroup, 0);
        EXPECT_EQ(collision->getBroadphaseHandle()->m_collisionFilterMask, 0);
        object->setScale(1.3f);
        ESM::Position position = ptr.getRefData().getPosition();
        position.pos[0] = 42.f;
        ptr.getRefData().setPosition(position);
        object->updatePosition();
        const osg::Quat rotation(1.0, osg::Vec3f(0, 0, 1));
        object->setRotation(rotation);
        scheduler.updateSingleAabb(object, true);
        EXPECT_EQ(collision->getWorldTransform().getOrigin(), btVector3(42, 0, 0));
        EXPECT_NEAR(collision->getWorldTransform().getRotation().z(), rotation.z(), 1e-6);
        EXPECT_EQ(collision->getCollisionShape()->getLocalScaling(), btVector3(1.3f, 1.3f, 1.3f));
        EXPECT_EQ(source->mCollisionShape->getLocalScaling(), btVector3(1, 1, 1));
        object.reset();
        EXPECT_EQ(world.getNumCollisionObjects(), 0);
    }
}
