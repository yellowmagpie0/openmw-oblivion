#include "actorplacement.hpp"

#include "collisiontype.hpp"
#include "mtphysics.hpp"
#include "ptrholder.hpp"

#include <components/misc/convert.hpp>

#include <BulletCollision/CollisionDispatch/btCollisionObjectWrapper.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/NarrowPhaseCollision/btManifoldPoint.h>

#include <cmath>
#include <stdexcept>

namespace MWPhysics
{
    namespace
    {
        struct PlacementContacts final : btCollisionWorld::ContactResultCallback
        {
            const btCollisionObject* mQuery;
            const btCollisionObject* mIgnored;
            bool mBlocked = false;

            PlacementContacts(const btCollisionObject* query, const btCollisionObject* ignored, bool actorsOnly)
                : mQuery(query), mIgnored(ignored)
            {
                m_collisionFilterGroup = CollisionType_Actor;
                m_collisionFilterMask = actorsOnly ? CollisionType_Actor
                    : CollisionType_World | CollisionType_Door | CollisionType_HeightMap | CollisionType_Actor;
            }

            btScalar addSingleResult(btManifoldPoint& point, const btCollisionObjectWrapper* a, int, int,
                const btCollisionObjectWrapper* b, int, int) override
            {
                const auto* other = a->getCollisionObject() == mQuery
                    ? b->getCollisionObject() : a->getCollisionObject();
                if (other == mIgnored || point.getDistance() >= -.01) return 0;
                if (const auto* proxy = other->getBroadphaseHandle(); proxy
                    && (proxy->m_collisionFilterGroup & CollisionType_Actor))
                    if (const auto* holder = static_cast<const PtrHolder*>(other->getUserPointer());
                        holder && !holder->blocksActorMovement()) return 0;
                mBlocked = true;
                return 0;
            }
        };

        bool finite(const osg::Vec3f& v)
        {
            return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z());
        }
    }

    bool isActorPlacementClear(PhysicsTaskScheduler& scheduler, const btCollisionObject* ignoredActor,
        const osg::Vec3f& fromFeet, const osg::Vec3f& toFeet, const osg::Vec3f& halfExtents, bool actorsOnly)
    {
        const float radius = std::max(halfExtents.x(), halfExtents.y());
        if (!finite(fromFeet) || !finite(toFeet) || !finite(halfExtents)
            || halfExtents.x() <= 0 || halfExtents.y() <= 0 || halfExtents.z() < radius)
            throw std::invalid_argument("invalid actor placement envelope");
        btCapsuleShapeZ shape(radius, 2.f * (halfExtents.z() - radius));
        shape.setMargin(.001);
        const osg::Vec3f center(0, 0, halfExtents.z());
        const btTransform from(btQuaternion::getIdentity(), Misc::Convert::toBullet(fromFeet + center));
        const btTransform to(btQuaternion::getIdentity(), Misc::Convert::toBullet(toFeet + center));
        btCollisionObject query;
        query.setCollisionShape(&shape);
        query.setWorldTransform(to);
        PlacementContacts contacts(&query, ignoredActor, actorsOnly);
        scheduler.contactTest(&query, contacts);
        if (contacts.mBlocked) return false;
        if (actorsOnly || fromFeet == toFeet) return true;

        // The occupied source actor must not block relocation out of that
        // overlap. Walls and closed doors still bound the entire swept hull.
        btCollisionWorld::ClosestConvexResultCallback sweep(from.getOrigin(), to.getOrigin());
        sweep.m_collisionFilterGroup = CollisionType_Actor;
        sweep.m_collisionFilterMask = CollisionType_World | CollisionType_Door | CollisionType_HeightMap;
        scheduler.convexSweepTest(&shape, from, to, sweep);
        return !sweep.hasHit();
    }
}
