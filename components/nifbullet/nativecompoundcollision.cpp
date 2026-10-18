#include "nativecompoundcollision.hpp"
#include "nativedynamicsworld.hpp"

#include <BulletCollision/BroadphaseCollision/btOverlappingPairCache.h>
#include <BulletCollision/CollisionDispatch/btManifoldResult.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletCollision/CollisionShapes/btConvexShape.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/NarrowPhaseCollision/btGjkEpa2.h>

#include <cmath>

namespace NifBullet
{
    namespace
    {
        btScalar centeredRadius(const btCollisionShape& shape, const btTransform& local)
        {
            if (shape.isCompound())
            {
                const auto& compound = static_cast<const btCompoundShape&>(shape);
                btScalar radius = 0;
                for (int i = 0; i < compound.getNumChildShapes(); ++i)
                    radius = btMax(radius, centeredRadius(*compound.getChildShape(i),
                        local * compound.getChildTransform(i)));
                return radius;
            }
            if (!shape.isConvex())
                return 0;
            btGjkEpaSolver2::sResults result;
            const auto distance = btGjkEpaSolver2::SignedDistance(btVector3(0, 0, 0), 0,
                static_cast<const btConvexShape*>(&shape), local, result);
            // Leave room inside the actual geometry for the CCD clearance.
            return std::isfinite(distance) && distance < 0 ? -distance / 2 : btScalar(0);
        }

        bool continuousCompound(const btRigidBody& body)
        {
            return body.isActive() && !body.isStaticOrKinematicObject()
                && body.getCollisionShape()->isCompound() && body.getCcdSquareMotionThreshold() > 0;
        }

        btVector3 worldNormal(const btCollisionWorld::LocalConvexResult& hit, bool normalInWorld)
        {
            return normalInWorld ? hit.m_hitNormalLocal
                : hit.m_hitCollisionObject->getWorldTransform().getBasis() * hit.m_hitNormalLocal;
        }

        class FilteredSweep : public btCollisionWorld::ClosestConvexResultCallback
        {
        protected:
            btRigidBody& mBody;
            btDispatcher& mDispatcher;
            btOverlappingPairCache& mPairs;

        public:
            FilteredSweep(btRigidBody& body, btDiscreteDynamicsWorld& world,
                const btVector3& from, const btVector3& to)
                : ClosestConvexResultCallback(from, to)
                , mBody(body)
                , mDispatcher(*world.getDispatcher())
                , mPairs(*world.getPairCache())
            {
                m_collisionFilterGroup = body.getBroadphaseHandle()->m_collisionFilterGroup;
                m_collisionFilterMask = body.getBroadphaseHandle()->m_collisionFilterMask;
            }

            bool needsCollision(btBroadphaseProxy* proxy) const override
            {
                const auto* other = static_cast<const btCollisionObject*>(proxy->m_clientObject);
                return other != &mBody && ClosestConvexResultCallback::needsCollision(proxy)
                    && (!mPairs.getOverlapFilterCallback()
                        || mPairs.needsBroadphaseCollision(proxy, mBody.getBroadphaseHandle()))
                    && mDispatcher.needsCollision(&mBody, other) && mDispatcher.needsResponse(&mBody, other);
            }
        };

        class ChildSweep final : public FilteredSweep
        {
            const btTransform mFrom, mTo;

        public:
            ChildSweep(btRigidBody& body, btDiscreteDynamicsWorld& world,
                const btTransform& from, const btTransform& to,
                const btTransform& childFrom, const btTransform& childTo)
                : FilteredSweep(body, world, childFrom.getOrigin(), childTo.getOrigin())
                , mFrom(from)
                , mTo(to)
            {
            }

            btVector3 startPoint(btScalar fraction, const btVector3& hitPoint) const
            {
                const btTransform atHit(mFrom.getRotation().slerp(mTo.getRotation(), fraction),
                    mFrom.getOrigin().lerp(mTo.getOrigin(), fraction));
                return mFrom * (atHit.inverse() * hitPoint);
            }

            btScalar addSingleResult(btCollisionWorld::LocalConvexResult& hit, bool normalInWorld) override
            {
                const auto point = startPoint(hit.m_hitFraction, hit.m_hitPointLocal);
                auto velocity = mBody.getVelocityInLocalPoint(point - mFrom.getOrigin());
                if (const auto* other = btRigidBody::upcast(hit.m_hitCollisionObject))
                    velocity -= other->getVelocityInLocalPoint(hit.m_hitPointLocal - other->getCenterOfMassPosition());
                if (velocity.dot(worldNormal(hit, normalInWorld)) >= 0)
                    return 1;
                return ClosestConvexResultCallback::addSingleResult(hit, normalInWorld);
            }
        };

        class CenterSweep final : public FilteredSweep
        {
            const btScalar mSeconds;

        public:
            CenterSweep(btRigidBody& body, btDiscreteDynamicsWorld& world,
                const btVector3& from, const btVector3& to, btScalar seconds)
                : FilteredSweep(body, world, from, to)
                , mSeconds(seconds)
            {
            }

            btScalar addSingleResult(btCollisionWorld::LocalConvexResult& hit, bool normalInWorld) override
            {
                auto displacement = m_convexToWorld - m_convexFromWorld;
                if (const auto* other = btRigidBody::upcast(hit.m_hitCollisionObject))
                    displacement -= other->getVelocityInLocalPoint(
                        hit.m_hitPointLocal - other->getCenterOfMassPosition()) * mSeconds;
                if (displacement.dot(worldNormal(hit, normalInWorld)) >= 0)
                    return 1;
                return ClosestConvexResultCallback::addSingleResult(hit, normalInWorld);
            }
        };

        void addPrediction(btDiscreteDynamicsWorld& world,
            btAlignedObjectArray<btPersistentManifold*>& predictions, btRigidBody& body,
            const btCollisionWorld::ClosestConvexResultCallback& hit,
            const btVector3& pointA, const btVector3& pointB)
        {
            const auto distance = (pointA - pointB).dot(hit.m_hitNormalWorld);
            if (distance < 0)
                return; // Existing penetration belongs to the ordinary contact solver.
            auto* manifold = world.getDispatcher()->getNewManifold(&body, hit.m_hitCollisionObject);
            predictions.push_back(manifold);
            const btManifoldPoint contact(body.getWorldTransform().inverse() * pointA,
                hit.m_hitCollisionObject->getWorldTransform().inverse() * pointB, hit.m_hitNormalWorld, distance);
            auto& point = manifold->getContactPoint(manifold->addManifoldPoint(contact, true));
            point.m_positionWorldOnA = pointA;
            point.m_positionWorldOnB = pointB;
            // Match Bullet's predictive contact material policy; ordinary
            // contacts retain the authored restitution and friction.
            point.m_combinedRestitution = 0;
            point.m_combinedFriction = gCalculateCombinedFrictionCallback(&body, hit.m_hitCollisionObject);
        }

        void predictChildren(btDiscreteDynamicsWorld& world,
            btAlignedObjectArray<btPersistentManifold*>& predictions, btRigidBody& body,
            const btCollisionShape& shape, const btTransform& local, const btTransform& next)
        {
            if (shape.isCompound())
            {
                const auto& compound = static_cast<const btCompoundShape&>(shape);
                for (int i = 0; i < compound.getNumChildShapes(); ++i)
                    predictChildren(world, predictions, body, *compound.getChildShape(i),
                        local * compound.getChildTransform(i), next);
                return;
            }
            if (!shape.isConvex())
                return;
            const auto from = body.getWorldTransform();
            const auto childFrom = from * local, childTo = next * local;
            ChildSweep hit(body, world, from, next, childFrom, childTo);
            world.convexSweepTest(static_cast<const btConvexShape*>(&shape), childFrom, childTo, hit,
                world.getDispatchInfo().m_allowedCcdPenetration);
            if (hit.hasHit() && hit.m_closestHitFraction < 1)
                addPrediction(world, predictions, body, hit,
                    hit.startPoint(hit.m_closestHitFraction, hit.m_hitPointWorld), hit.m_hitPointWorld);
        }
    }

    btScalar centeredCompoundSweepRadius(const btCollisionShape& shape)
    {
        return centeredRadius(shape, btTransform::getIdentity());
    }

    void NativeDynamicsWorld::createPredictiveContacts(btScalar seconds)
    {
        mCompoundSweepStarts.clear();
        // Capture before contact solving: split-impulse position correction
        // itself can cross a thin surface, before integrateTransforms runs.
        for (int i = 0; i < m_nonStaticRigidBodies.size(); ++i)
            if (continuousCompound(*m_nonStaticRigidBodies[i])
                && m_nonStaticRigidBodies[i]->getCcdSweptSphereRadius() > 0)
                mCompoundSweepStarts.emplace_back(m_nonStaticRigidBodies[i],
                    m_nonStaticRigidBodies[i]->getWorldTransform());
        btDiscreteDynamicsWorld::createPredictiveContacts(seconds);
        if (!getDispatchInfo().m_useContinuous)
            return;
        for (int i = 0; i < m_nonStaticRigidBodies.size(); ++i)
        {
            auto& body = *m_nonStaticRigidBodies[i];
            if (!continuousCompound(body))
                continue;
            btTransform next;
            body.predictIntegratedTransform(seconds, next);
            const auto from = body.getWorldTransform();
            predictChildren(*this, m_predictiveManifolds, body, *body.getCollisionShape(),
                btTransform::getIdentity(), next);
            if (body.getCcdSweptSphereRadius() <= 0)
                continue;
            const btSphereShape sphere(body.getCcdSweptSphereRadius());
            CenterSweep hit(body, *this, from.getOrigin(), next.getOrigin(), seconds);
            convexSweepTest(&sphere, from, next, hit);
            if (hit.hasHit() && hit.m_closestHitFraction < 1)
                addPrediction(*this, m_predictiveManifolds, body, hit, from.getOrigin(),
                    from.getOrigin().lerp(next.getOrigin(), hit.m_closestHitFraction));
        }
    }

    void NativeDynamicsWorld::integrateTransforms(btScalar seconds)
    {
        btDiscreteDynamicsWorld::integrateTransforms(seconds);
        if (!getDispatchInfo().m_useContinuous)
            return;
        for (const auto& [body, from] : mCompoundSweepStarts)
        {
            const auto next = body->getWorldTransform();
            const btSphereShape sphere(body->getCcdSweptSphereRadius());
            CenterSweep hit(*body, *this, from.getOrigin(), next.getOrigin(), seconds);
            convexSweepTest(&sphere, from, next, hit);
            if (!hit.hasHit() || hit.m_closestHitFraction >= 1)
                continue;
            const auto travel = (next.getOrigin() - from.getOrigin()).length();
            // Keep the next sweep on the free side of the surface. The radius
            // was halved inside the real geometry, so this clearance also fits.
            const auto clearance = btMin(body->getCcdSweptSphereRadius() / 2,
                getDispatchInfo().m_allowedCcdPenetration);
            const auto fraction = travel > 0
                ? btMax(btScalar(0), hit.m_closestHitFraction - clearance / travel) : hit.m_closestHitFraction;
            body->proceedToTransform(btTransform(from.getRotation().slerp(next.getRotation(), fraction),
                from.getOrigin().lerp(next.getOrigin(), fraction)));
            body->setHitFraction(fraction);
        }
    }
}
