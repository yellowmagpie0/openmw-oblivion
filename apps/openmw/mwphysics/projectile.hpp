#ifndef OPENMW_MWPHYSICS_PROJECTILE_H
#define OPENMW_MWPHYSICS_PROJECTILE_H

#include <atomic>
#include <memory>
#include <mutex>

#include <LinearMath/btVector3.h>

#include "ptrholder.hpp"

class btCollisionObject;
class btCollisionShape;
class btConvexShape;

namespace osg
{
    class Vec3f;
}

namespace MWPhysics
{
    class PhysicsTaskScheduler;
    class PhysicsSystem;

    class Projectile final : public PtrHolder
    {
    public:
        Projectile(const MWWorld::Ptr& caster, const osg::Vec3f& position, float radius,
            PhysicsTaskScheduler* scheduler, PhysicsSystem* physicssystem, bool registerCollision = true);
        ~Projectile() override;

        btConvexShape* getConvexShape() const { return mConvexShape; }

        void updateCollisionObjectPosition();

        bool isActive() const { return mActive.load(std::memory_order_acquire); }

        MWWorld::Ptr getTarget() const;

        MWWorld::Ptr getCaster() const;
        void setCaster(const MWWorld::Ptr& caster);
        const btCollisionObject* getCasterCollisionObject() const { return mCasterColObj; }

        bool getHitWater() const { return mHitWater; }

        // One producer claims the hit; publish complete metadata with the
        // inactive release store. False leaves the previous hit untouched.
        bool hit(const btCollisionObject* target, btVector3 pos, btVector3 normal, bool water = false);

        void setValidTargets(const std::vector<MWWorld::Ptr>& targets);
        bool isValidTarget(const btCollisionObject* target) const;

        btVector3 getHitPosition() const { return mHitPosition; }

    private:
        friend class PhysicsSystem;
        void registerCollision();
        bool mRegistered = false;
        std::unique_ptr<btCollisionShape> mShape;
        btConvexShape* mConvexShape;

        bool mHitWater;
        std::atomic<bool> mActive;
        std::atomic<bool> mHitClaimed;
        MWWorld::Ptr mCaster;
        const btCollisionObject* mCasterColObj;
        const btCollisionObject* mHitTarget;
        btVector3 mHitPosition;
        btVector3 mHitNormal;

        std::vector<const btCollisionObject*> mValidTargets;

        mutable std::mutex mMutex;

        PhysicsSystem* mPhysics;
        PhysicsTaskScheduler* mTaskScheduler;

        Projectile(const Projectile&);
        Projectile& operator=(const Projectile&);
    };

}

#endif
