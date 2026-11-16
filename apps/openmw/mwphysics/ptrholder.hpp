#ifndef OPENMW_MWPHYSICS_PTRHOLDER_H
#define OPENMW_MWPHYSICS_PTRHOLDER_H

#include <memory>
#include <atomic>
#include <mutex>
#include <utility>

#include <osg/Vec3d>

#include <BulletCollision/CollisionDispatch/btCollisionObject.h>

#include "../mwworld/ptr.hpp"

namespace MWPhysics
{
    class PtrHolder
    {
    public:
        explicit PtrHolder(const MWWorld::Ptr& ptr, const osg::Vec3f& position)
            : mPtr(ptr)
            , mSimulationPosition(position)
            , mPosition(position)
            , mPreviousPosition(position)
        {
        }

        virtual ~PtrHolder() = default;

        void updatePtr(const MWWorld::Ptr& updated) { mPtr = updated; }

        MWWorld::Ptr getPtr() const { return mPtr; }

        btCollisionObject* getCollisionObject() const { return mCollisionObject.get(); }

        // Main-thread life state projected for physics workers. Movement
        // filtering must not read mutable CreatureStats from a worker.
        void setBlocksActorMovement(bool blocks) { mBlocksActorMovement.store(blocks, std::memory_order_relaxed); }
        bool blocksActorMovement() const { return mBlocksActorMovement.load(std::memory_order_relaxed); }

        void setVelocity(osg::Vec3f velocity) { mVelocity = velocity; }

        osg::Vec3f velocity() { return std::exchange(mVelocity, osg::Vec3f()); }

        void setSimulationPosition(const osg::Vec3f& position) { mSimulationPosition = position; }

        osg::Vec3f getSimulationPosition() const { return mSimulationPosition; }

        void setPosition(const osg::Vec3f& position)
        {
            mPreviousPosition = mPosition;
            mPosition = position;
        }

        osg::Vec3d getPosition() const { return mPosition; }

        osg::Vec3d getPreviousPosition() const { return mPreviousPosition; }

    protected:
        MWWorld::Ptr mPtr;
        std::atomic_bool mBlocksActorMovement{true};
        std::unique_ptr<btCollisionObject> mCollisionObject;
        osg::Vec3f mVelocity;
        osg::Vec3f mSimulationPosition;
        osg::Vec3d mPosition;
        osg::Vec3d mPreviousPosition;
    };
}

#endif
