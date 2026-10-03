#ifndef OPENMW_COMPONENTS_NIFBULLET_NATIVEDYNAMICSWORLD_H
#define OPENMW_COMPONENTS_NIFBULLET_NATIVEDYNAMICSWORLD_H

#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <algorithm>
#include <functional>
#include <span>
#include <stdexcept>
#include <vector>

namespace NifBullet
{
    // Borrowed native owners integrate their keyframed motion once per actual
    // Bullet substep. Ordinary kinematic objects retain Bullet's pose-derived
    // velocity behavior. Owners must unregister before their bodies disappear.
    class NativeDynamicsWorld : public btDiscreteDynamicsWorld
    {
        struct Owner
        {
            const void* mIdentity;
            std::function<void(float)> mStep;
            std::vector<btCollisionObject*> mBodies;
        };
        std::vector<Owner> mNativeOwners;

        bool isNativeBody(const btCollisionObject* body) const
        {
            return std::any_of(mNativeOwners.begin(), mNativeOwners.end(), [&](const Owner& owner) {
                return std::find(owner.mBodies.begin(), owner.mBodies.end(), body) != owner.mBodies.end();
            });
        }

    protected:
        void saveKinematicState(btScalar timeStep) override
        {
            for (int i = 0; i < m_collisionObjects.size(); ++i)
            {
                auto* body = btRigidBody::upcast(m_collisionObjects[i]);
                if (body && body->getActivationState() != ISLAND_SLEEPING
                    && body->isKinematicObject() && !isNativeBody(body))
                    body->saveKinematicState(timeStep);
            }
        }

        void internalSingleStepSimulation(btScalar timeStep) override
        {
            for (const auto& owner : mNativeOwners)
                owner.mStep(static_cast<float>(timeStep));
            btDiscreteDynamicsWorld::internalSingleStepSimulation(timeStep);
        }

    public:
        using btDiscreteDynamicsWorld::btDiscreteDynamicsWorld;

        void registerNativeMotionOwner(const void* identity, std::span<btCollisionObject* const> bodies,
            std::function<void(float)> step)
        {
            if (!identity || !step || bodies.empty()
                || std::any_of(mNativeOwners.begin(), mNativeOwners.end(),
                    [&](const Owner& owner) { return owner.mIdentity == identity; }))
                throw std::invalid_argument("Invalid native motion owner");
            for (std::size_t i = 0; i < bodies.size(); ++i)
            {
                const auto* body = bodies[i];
                if (!body || !btRigidBody::upcast(body) || isNativeBody(body)
                    || std::find(bodies.begin(), bodies.begin() + i, body) != bodies.begin() + i
                    || m_collisionObjects.findLinearSearch(const_cast<btCollisionObject*>(body))
                        == m_collisionObjects.size())
                    throw std::invalid_argument("Invalid native motion owner body");
            }
            Owner owner{identity, std::move(step), {bodies.begin(), bodies.end()}};
            mNativeOwners.push_back(std::move(owner));
        }

        void unregisterNativeMotionOwner(const void* identity) noexcept
        {
            std::erase_if(mNativeOwners, [&](const Owner& owner) { return owner.mIdentity == identity; });
        }
    };
}

#endif
