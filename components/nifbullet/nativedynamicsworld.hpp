#ifndef OPENMW_COMPONENTS_NIFBULLET_NATIVEDYNAMICSWORLD_H
#define OPENMW_COMPONENTS_NIFBULLET_NATIVEDYNAMICSWORLD_H

#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace NifBullet
{
    // Borrowed native owners integrate their keyframed motion once per actual
    // Bullet substep. Ordinary kinematic objects retain Bullet's pose-derived
    // velocity behavior. Owners must unregister before their bodies disappear.
    class NativeDynamicsWorld : public btDiscreteDynamicsWorld
    {
    public:
        struct Lifetime { bool mAlive = true; };

        enum class RegistrationKind { AddBody, RemoveBody, AddConstraint, RemoveConstraint };
        struct RegistrationOperation
        {
            RegistrationKind mKind;
            btRigidBody* mBody = nullptr;
            btTypedConstraint* mConstraint = nullptr;
            int mGroup = 0, mMask = 0;
        };
        class PreparedRegistrationBatch
        {
        public:
            ~PreparedRegistrationBatch();
            PreparedRegistrationBatch(const PreparedRegistrationBatch&) = delete;
            PreparedRegistrationBatch& operator=(const PreparedRegistrationBatch&) = delete;
        private:
            friend class NativeDynamicsWorld;
            struct Data;
            explicit PreparedRegistrationBatch(std::shared_ptr<Data> data);
            std::shared_ptr<Data> mData;
        };

    private:
        struct Owner
        {
            const void* mIdentity;
            std::function<void(float)> mStep;
            std::vector<btCollisionObject*> mBodies;
            std::shared_ptr<const char> mGeneration;
            std::weak_ptr<const char> mPendingRegistration = {};
        };
        const std::shared_ptr<Lifetime> mLifetime = std::make_shared<Lifetime>();
        std::vector<Owner> mNativeOwners;
        const void* mNativeSceneOwner = nullptr;
        std::vector<std::weak_ptr<PreparedRegistrationBatch::Data>> mRegistrationBatches;
        std::vector<std::pair<btRigidBody*, btTransform>> mCompoundSweepStarts;

        bool hasAdmittedRegistrationBatch() const noexcept;
        void discardRegistrationProxies(PreparedRegistrationBatch::Data& data) noexcept;
        void discardOwnerRegistrationBatches(const void* identity) noexcept;

        bool isNativeBody(const btCollisionObject* body) const
        {
            return std::any_of(mNativeOwners.begin(), mNativeOwners.end(), [&](const Owner& owner) {
                return std::find(owner.mBodies.begin(), owner.mBodies.end(), body) != owner.mBodies.end();
            });
        }

    protected:
        void createPredictiveContacts(btScalar timeStep) override;
        void integrateTransforms(btScalar timeStep) override;

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
                if (!owner.mBodies.empty())
                    owner.mStep(static_cast<float>(timeStep));
            btDiscreteDynamicsWorld::internalSingleStepSimulation(timeStep);
        }

    public:
        using btDiscreteDynamicsWorld::btDiscreteDynamicsWorld;

        // Independent identity for detached graphs; retained handles cannot
        // match a new World constructed at the same address.
        std::weak_ptr<const Lifetime> lifetimeIdentity() const noexcept { return mLifetime; }
        ~NativeDynamicsWorld() override;

        int stepSimulation(btScalar timeStep, int maxSubSteps = 1,
            btScalar fixedTimeStep = btScalar(1.) / btScalar(60.)) override;
        void performDiscreteCollisionDetection() override;

        // Complete owned resources and ordered physical operations for an
        // existing native motion owner. No linked-body collision references:
        // the native internal mask policy controls those pairs separately.
        // Preparation validates membership, endpoint order and source bodies.
        std::unique_ptr<PreparedRegistrationBatch> prepareRegistrationBatch(const void* identity,
            std::span<btRigidBody* const> bodies, std::span<btTypedConstraint* const> constraints,
            std::span<const RegistrationOperation> operations);
        // Admission allocates provisional proxies and reserves summed capacity
        // for every admitted owner. Live handles/contacts remain unchanged.
        // Hold the exclusive World barrier: no steps, detection or queries
        // until every admitted batch commits or rolls back.
        void admitRegistrationBatch(PreparedRegistrationBatch& prepared);
        bool validateRegistrationBatch(const PreparedRegistrationBatch& prepared) const noexcept;
        void rollbackRegistrationBatch(PreparedRegistrationBatch& prepared) noexcept;
        // Validate before releasing any old registration. Publication reuses
        // admitted proxies, performs no allocations and updates native routing.
        // Uses Bullet's existing nonthrowing removal/cleanup contract.
        bool commitRegistrationBatch(PreparedRegistrationBatch& prepared) noexcept;

        // Engine wrapper binding, analogous to original889BB0's World+2B0
        // backreference. This is ownership presence, not simulation enablement.
        void bindNativeSceneOwner(const void* identity)
        {
            if (!identity || mNativeSceneOwner)
                throw std::invalid_argument("Invalid native scene World owner binding");
            mNativeSceneOwner = identity;
        }

        void unbindNativeSceneOwner(const void* identity) noexcept
        {
            if (identity && mNativeSceneOwner == identity)
                mNativeSceneOwner = nullptr;
        }

        bool hasNativeSceneOwner(const void* identity) const noexcept
        {
            return identity && mNativeSceneOwner == identity;
        }

        void registerNativeMotionOwner(const void* identity, std::span<btCollisionObject* const> bodies,
            std::function<void(float)> step)
        {
            if (!identity || !step
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
            Owner owner{identity, std::move(step), {bodies.begin(), bodies.end()}, std::make_shared<const char>()};
            mNativeOwners.push_back(std::move(owner));
        }

        class PreparedMotionOwnerBodies
        {
            friend class NativeDynamicsWorld;
            const NativeDynamicsWorld* mWorld = nullptr;
            const void* mIdentity = nullptr;
            std::weak_ptr<const Lifetime> mWorldLifetime;
            std::weak_ptr<const char> mExpected;
            std::shared_ptr<const char> mNext;
            std::vector<btCollisionObject*> mBodies;
        };

        // Pure storage/identity preparation. New members may still be absent
        // from the World. Their owning graph retains the synchronization barrier
        // across admission, final validation and the nonallocating commit.
        std::unique_ptr<PreparedMotionOwnerBodies> prepareNativeMotionOwnerBodies(
            const void* identity, std::vector<btCollisionObject*> bodies) const
        {
            const auto found = std::find_if(mNativeOwners.begin(), mNativeOwners.end(),
                [&](const Owner& owner) { return owner.mIdentity == identity; });
            if (!identity || found == mNativeOwners.end())
                throw std::invalid_argument("Invalid native motion owner replacement");
            for (std::size_t i = 0; i < bodies.size(); ++i)
            {
                auto* body = bodies[i];
                if (!body || !btRigidBody::upcast(body)
                    || std::find(bodies.begin(), bodies.begin() + i, body) != bodies.begin() + i
                    || std::any_of(mNativeOwners.begin(), mNativeOwners.end(), [&](const Owner& owner) {
                        return owner.mIdentity != identity
                            && std::find(owner.mBodies.begin(), owner.mBodies.end(), body) != owner.mBodies.end();
                    }))
                    throw std::invalid_argument("Invalid native motion owner replacement body");
            }
            auto result = std::make_unique<PreparedMotionOwnerBodies>();
            result->mWorld = this; result->mIdentity = identity; result->mWorldLifetime = mLifetime;
            result->mExpected = found->mGeneration;
            result->mNext = std::make_shared<const char>(); result->mBodies = std::move(bodies);
            return result;
        }

        bool validateNativeMotionOwnerBodies(const PreparedMotionOwnerBodies& prepared,
            bool requireRegistration = true) const noexcept
        {
            if (prepared.mWorld != this || prepared.mWorldLifetime.lock() != mLifetime || !prepared.mNext)
                return false;
            const auto found = std::find_if(mNativeOwners.begin(), mNativeOwners.end(),
                [&](const Owner& owner) { return owner.mIdentity == prepared.mIdentity; });
            if (found == mNativeOwners.end() || prepared.mExpected.lock() != found->mGeneration) return false;
            for (auto* body : prepared.mBodies)
                if ((requireRegistration && m_collisionObjects.findLinearSearch(body) == m_collisionObjects.size())
                    || std::any_of(mNativeOwners.begin(), mNativeOwners.end(), [&](const Owner& owner) {
                        return owner.mIdentity != prepared.mIdentity
                            && std::find(owner.mBodies.begin(), owner.mBodies.end(), body) != owner.mBodies.end();
                    })) return false;
            return true;
        }

        bool commitNativeMotionOwnerBodies(PreparedMotionOwnerBodies& prepared) noexcept
        {
            if (!validateNativeMotionOwnerBodies(prepared)) return false;
            const auto found = std::find_if(mNativeOwners.begin(), mNativeOwners.end(),
                [&](const Owner& owner) { return owner.mIdentity == prepared.mIdentity; });
            found->mBodies.swap(prepared.mBodies);
            found->mGeneration.swap(prepared.mNext);
            prepared.mNext.reset();
            return true;
        }

        void replaceNativeMotionOwnerBodies(const void* identity, std::vector<btCollisionObject*> bodies)
        {
            auto prepared = prepareNativeMotionOwnerBodies(identity, std::move(bodies));
            if (!commitNativeMotionOwnerBodies(*prepared))
                throw std::invalid_argument("Unregistered or changed native motion owner replacement");
        }

        void unregisterNativeMotionOwner(const void* identity) noexcept
        {
            discardOwnerRegistrationBatches(identity);
            std::erase_if(mNativeOwners, [&](const Owner& owner) { return owner.mIdentity == identity; });
        }
    };
}

#endif
