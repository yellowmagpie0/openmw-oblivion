#include <BulletCollision/CollisionDispatch/btCollisionObject.h>

#include "collisiontype.hpp"
#include "projectile.hpp"
#include "projectileconvexcallback.hpp"

namespace MWPhysics
{
    btScalar ProjectileConvexCallback::addSingleResult(
        btCollisionWorld::LocalConvexResult& result, bool normalInWorldSpace)
    {
        const auto* hitObject = result.m_hitCollisionObject;
        if (hitObject == mCaster || hitObject == mMe)
            return 1.f;

        // Reject before updating the closest fraction. An ignored actor must
        // not hide an eligible wall or actor farther along the sweep.
        switch (hitObject->getBroadphaseHandle()->m_collisionFilterGroup)
        {
            case CollisionType_Actor:
                if (!mProjectile.isValidTarget(hitObject))
                    return 1.f;
                break;
            case CollisionType_Projectile:
            {
                const auto* target = static_cast<Projectile*>(hitObject->getUserPointer());
                const auto* caster = target->getCasterCollisionObject();
                if (caster && !mProjectile.isValidTarget(caster))
                    return 1.f;
                break;
            }
        }
        return btCollisionWorld::ClosestConvexResultCallback::addSingleResult(result, normalInWorldSpace);
    }

    void ProjectileConvexCallback::commitHit()
    {
        if (!hasHit())
            return;
        const auto group = m_hitCollisionObject->getBroadphaseHandle()->m_collisionFilterGroup;
        if (!mProjectile.hit(m_hitCollisionObject, m_hitPointWorld, m_hitNormalWorld,
                group == CollisionType_Water))
            return;
        if (group == CollisionType_Projectile)
        {
            auto* target = static_cast<Projectile*>(m_hitCollisionObject->getUserPointer());
            target->hit(mMe, m_hitPointWorld, m_hitNormalWorld);
        }
    }
}
