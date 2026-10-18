#include "nativedynamicsworld.hpp"

#include <BulletCollision/BroadphaseCollision/btBroadphaseInterface.h>
#include <BulletCollision/CollisionShapes/btCollisionShape.h>
#include <BulletDynamics/ConstraintSolver/btTypedConstraint.h>

#include <cassert>
#include <cmath>
#include <limits>

namespace NifBullet
{
    struct NativeDynamicsWorld::PreparedRegistrationBatch::Data
    {
        enum class State { Prepared, Admitted, Consumed };
        struct Body
        {
            btRigidBody* mBody;
            btBroadphaseProxy* mProxy;
            const btCollisionShape* mShape;
            btTransform mPose;
            btVector3 mMinimum, mMaximum;
            int mCollisionFlags, mBodyFlags;
            bool mRegistered;
        };
        struct Constraint { btTypedConstraint* mConstraint; bool mRegistered; };
        struct Operation { RegistrationOperation mOperation; btBroadphaseProxy* mProxy = nullptr; };
        NativeDynamicsWorld* mWorld = nullptr;
        const void* mIdentity = nullptr;
        std::weak_ptr<const Lifetime> mLifetime;
        std::shared_ptr<const char> mTag = std::make_shared<const char>();
        State mState = State::Prepared;
        btVector3 mGravity;
        std::vector<Body> mBodies;
        std::vector<Constraint> mConstraints;
        std::vector<btTypedConstraint*> mIncidentConstraints;
        std::vector<Operation> mOperations;
        std::size_t mBodyAdditions = 0, mConstraintAdditions = 0;
        std::unique_ptr<PreparedMotionOwnerBodies> mRouting;
    };

    NativeDynamicsWorld::PreparedRegistrationBatch::PreparedRegistrationBatch(std::shared_ptr<Data> data)
        : mData(std::move(data)) {}

    NativeDynamicsWorld::PreparedRegistrationBatch::~PreparedRegistrationBatch()
    {
        if (mData && mData->mState == Data::State::Admitted)
            if (const auto lifetime = mData->mLifetime.lock(); lifetime && lifetime->mAlive)
                mData->mWorld->rollbackRegistrationBatch(*this);
    }

    NativeDynamicsWorld::~NativeDynamicsWorld()
    {
        for (const auto& weak : mRegistrationBatches)
            if (auto data = weak.lock())
            {
                discardRegistrationProxies(*data);
                data->mState = PreparedRegistrationBatch::Data::State::Consumed;
            }
        mRegistrationBatches.clear();
        mLifetime->mAlive = false;
    }

    bool NativeDynamicsWorld::hasAdmittedRegistrationBatch() const noexcept
    {
        return std::any_of(mRegistrationBatches.begin(), mRegistrationBatches.end(), [](const auto& weak) {
            const auto data = weak.lock();
            return data && data->mState == PreparedRegistrationBatch::Data::State::Admitted;
        });
    }

    int NativeDynamicsWorld::stepSimulation(btScalar timeStep, int maxSubSteps, btScalar fixedTimeStep)
    {
        if (hasAdmittedRegistrationBatch())
            throw std::logic_error("Simulation during provisional native registration");
        return btDiscreteDynamicsWorld::stepSimulation(timeStep, maxSubSteps, fixedTimeStep);
    }

    void NativeDynamicsWorld::performDiscreteCollisionDetection()
    {
        if (hasAdmittedRegistrationBatch())
            throw std::logic_error("Collision detection during provisional native registration");
        btDiscreteDynamicsWorld::performDiscreteCollisionDetection();
    }

    void NativeDynamicsWorld::discardRegistrationProxies(PreparedRegistrationBatch::Data& data) noexcept
    {
        for (auto& operation : data.mOperations)
            if (operation.mProxy)
            {
                getBroadphase()->destroyProxy(operation.mProxy, getDispatcher());
                operation.mProxy = nullptr;
            }
    }

    void NativeDynamicsWorld::discardOwnerRegistrationBatches(const void* identity) noexcept
    {
        std::erase_if(mRegistrationBatches, [&](const auto& weak) {
            auto data = weak.lock();
            if (!data) return true;
            if (data->mIdentity != identity) return false;
            discardRegistrationProxies(*data);
            data->mState = PreparedRegistrationBatch::Data::State::Consumed;
            return true;
        });
    }

    std::unique_ptr<NativeDynamicsWorld::PreparedRegistrationBatch> NativeDynamicsWorld::prepareRegistrationBatch(
        const void* identity, std::span<btRigidBody* const> bodies, std::span<btTypedConstraint* const> constraints,
        std::span<const RegistrationOperation> operations)
    {
        const auto owner = std::find_if(mNativeOwners.begin(), mNativeOwners.end(),
            [&](const Owner& value) { return value.mIdentity == identity; });
        if (!identity || owner == mNativeOwners.end() || !owner->mPendingRegistration.expired())
            throw std::invalid_argument("Unavailable native registration owner");
        auto data = std::make_shared<PreparedRegistrationBatch::Data>();
        data->mWorld = this; data->mIdentity = identity; data->mLifetime = mLifetime; data->mGravity = getGravity();
        std::vector<unsigned char> selectedBodies, selectedConstraints;
        std::vector<btCollisionObject*> allBodies;
        for (auto* body : bodies)
        {
            if (!body || !body->getCollisionShape() || std::find(allBodies.begin(), allBodies.end(), body) != allBodies.end())
                throw std::invalid_argument("Invalid native registration body");
            const bool registered = m_collisionObjects.findLinearSearch(body) != m_collisionObjects.size();
            if (registered != (std::find(owner->mBodies.begin(), owner->mBodies.end(), body) != owner->mBodies.end())
                || registered != (body->getBroadphaseHandle() != nullptr)
                || (!registered && body->getWorldArrayIndex() != -1))
                throw std::invalid_argument("Foreign or inconsistent native registration body");
            btVector3 minimum, maximum;
            body->getCollisionShape()->getAabb(body->getWorldTransform(), minimum, maximum);
            for (int axis = 0; axis < 3; ++axis)
                if (!std::isfinite(minimum[axis]) || !std::isfinite(maximum[axis]) || minimum[axis] > maximum[axis])
                    throw std::invalid_argument("Invalid native registration body bounds");
            data->mBodies.push_back({body, body->getBroadphaseHandle(), body->getCollisionShape(),
                body->getWorldTransform(), minimum, maximum, body->getCollisionFlags(), body->getFlags(), registered});
            selectedBodies.push_back(registered); allBodies.push_back(body);
        }
        for (auto* body : owner->mBodies)
            if (std::find(allBodies.begin(), allBodies.end(), body) == allBodies.end())
                throw std::invalid_argument("Incomplete native registration body resources");
        for (int i = 0; i < m_constraints.size(); ++i)
            if (std::find(bodies.begin(), bodies.end(), &m_constraints[i]->getRigidBodyA()) != bodies.end()
                || std::find(bodies.begin(), bodies.end(), &m_constraints[i]->getRigidBodyB()) != bodies.end())
                data->mIncidentConstraints.push_back(m_constraints[i]);
        const auto bodyIndex = [&](const btRigidBody* body) {
            const auto found = std::find(bodies.begin(), bodies.end(), body);
            if (found == bodies.end()) throw std::invalid_argument("Unknown native registration endpoint");
            return std::size_t(found - bodies.begin());
        };
        for (auto* constraint : constraints)
        {
            if (!constraint || std::any_of(data->mConstraints.begin(), data->mConstraints.end(),
                    [&](const auto& value) { return value.mConstraint == constraint; }))
                throw std::invalid_argument("Invalid native registration constraint");
            const auto a = bodyIndex(&constraint->getRigidBodyA()), b = bodyIndex(&constraint->getRigidBodyB());
            if (a == b) throw std::invalid_argument("Self native registration constraint");
            for (auto* body : {bodies[a], bodies[b]})
                for (int i = 0; i < body->getNumConstraintRefs(); ++i)
                    if (body->getConstraintRef(i) == constraint)
                        throw std::invalid_argument("Linked-collision constraint in native mask registration");
            int occurrences = 0;
            for (int i = 0; i < m_constraints.size(); ++i) occurrences += m_constraints[i] == constraint;
            if (occurrences > 1 || (occurrences && (!selectedBodies[a] || !selectedBodies[b])))
                throw std::invalid_argument("Inconsistent native constraint membership");
            data->mConstraints.push_back({constraint, occurrences != 0}); selectedConstraints.push_back(occurrences != 0);
        }
        const auto constraintIndex = [&](const btTypedConstraint* constraint) {
            const auto found = std::find(constraints.begin(), constraints.end(), constraint);
            if (found == constraints.end()) throw std::invalid_argument("Unknown native registration constraint");
            return std::size_t(found - constraints.begin());
        };
        for (const auto& operation : operations)
        {
            const auto kind = operation.mKind;
            if (kind == RegistrationKind::AddBody || kind == RegistrationKind::RemoveBody)
            {
                if (!operation.mBody || operation.mConstraint)
                    throw std::invalid_argument("Invalid native body operation");
                const auto index = bodyIndex(operation.mBody);
                if (kind == RegistrationKind::AddBody)
                {
                    if (selectedBodies[index]) throw std::invalid_argument("Already registered native body");
                    selectedBodies[index] = 1; ++data->mBodyAdditions;
                }
                else
                {
                    if (!selectedBodies[index]) throw std::invalid_argument("Absent native body removal");
                    for (std::size_t i = 0; i < constraints.size(); ++i)
                        if (selectedConstraints[i] && (&constraints[i]->getRigidBodyA() == operation.mBody
                                || &constraints[i]->getRigidBodyB() == operation.mBody))
                            throw std::invalid_argument("Native body removal before incident constraint");
                    for (int i = 0; i < m_constraints.size(); ++i)
                        if (std::find(constraints.begin(), constraints.end(), m_constraints[i]) == constraints.end()
                            && (&m_constraints[i]->getRigidBodyA() == operation.mBody
                                || &m_constraints[i]->getRigidBodyB() == operation.mBody))
                            throw std::invalid_argument("Unowned incident constraint in native body removal");
                    selectedBodies[index] = 0;
                }
            }
            else if (kind == RegistrationKind::AddConstraint || kind == RegistrationKind::RemoveConstraint)
            {
                if (operation.mBody || !operation.mConstraint)
                    throw std::invalid_argument("Invalid native constraint operation");
                const auto index = constraintIndex(operation.mConstraint);
                if (kind == RegistrationKind::AddConstraint)
                {
                    if (selectedConstraints[index]
                        || !selectedBodies[bodyIndex(&operation.mConstraint->getRigidBodyA())]
                        || !selectedBodies[bodyIndex(&operation.mConstraint->getRigidBodyB())])
                        throw std::invalid_argument("Unavailable native constraint admission endpoints");
                    selectedConstraints[index] = 1; ++data->mConstraintAdditions;
                }
                else
                {
                    if (!selectedConstraints[index]) throw std::invalid_argument("Absent native constraint removal");
                    selectedConstraints[index] = 0;
                }
            }
            else throw std::invalid_argument("Unknown native World registration operation");
            data->mOperations.push_back({operation});
        }
        std::vector<btCollisionObject*> finalBodies;
        for (std::size_t i = 0; i < bodies.size(); ++i)
            if (selectedBodies[i]) finalBodies.push_back(bodies[i]);
        data->mRouting = prepareNativeMotionOwnerBodies(identity, std::move(finalBodies));
        auto result = std::unique_ptr<PreparedRegistrationBatch>(new PreparedRegistrationBatch(std::move(data)));
        if (!validateRegistrationBatch(*result)) throw std::invalid_argument("Changed native registration preparation");
        return result;
    }

    bool NativeDynamicsWorld::validateRegistrationBatch(const PreparedRegistrationBatch& prepared) const noexcept
    {
        const auto* data = prepared.mData.get();
        if (!data || data->mWorld != this || data->mLifetime.lock() != mLifetime || !mLifetime->mAlive
            || data->mState == PreparedRegistrationBatch::Data::State::Consumed || data->mGravity != getGravity()
            || !validateNativeMotionOwnerBodies(*data->mRouting, false)) return false;
        const auto owner = std::find_if(mNativeOwners.begin(), mNativeOwners.end(),
            [&](const Owner& value) { return value.mIdentity == data->mIdentity; });
        const auto pending = owner->mPendingRegistration.lock();
        if (data->mState == PreparedRegistrationBatch::Data::State::Admitted ? pending != data->mTag : bool(pending))
            return false;
        for (const auto& weak : mRegistrationBatches)
            if (const auto other = weak.lock(); other && other.get() != data && other->mIdentity != data->mIdentity)
                for (const auto& body : data->mBodies)
                    if (std::any_of(other->mBodies.begin(), other->mBodies.end(),
                            [&](const auto& value) { return value.mBody == body.mBody; })) return false;
        for (const auto& value : data->mBodies)
        {
            auto& body = *value.mBody;
            if (body.getBroadphaseHandle() != value.mProxy || body.getCollisionShape() != value.mShape
                || body.getWorldTransform() != value.mPose || body.getCollisionFlags() != value.mCollisionFlags
                || body.getFlags() != value.mBodyFlags
                || (m_collisionObjects.findLinearSearch(&body) != m_collisionObjects.size()) != value.mRegistered)
                return false;
            const auto worldIndex = body.getWorldArrayIndex();
            if (value.mRegistered ? (worldIndex < 0 || worldIndex >= m_collisionObjects.size()
                    || m_collisionObjects[worldIndex] != &body) : worldIndex != -1) return false;
            try
            {
                btVector3 minimum, maximum; body.getCollisionShape()->getAabb(body.getWorldTransform(), minimum, maximum);
                if (minimum != value.mMinimum || maximum != value.mMaximum) return false;
            }
            catch (...) { return false; }
        }
        std::size_t incidentCount = 0;
        for (int i = 0; i < m_constraints.size(); ++i)
            if (std::any_of(data->mBodies.begin(), data->mBodies.end(), [&](const auto& value) {
                    return &m_constraints[i]->getRigidBodyA() == value.mBody
                        || &m_constraints[i]->getRigidBodyB() == value.mBody;
                }))
            {
                if (std::find(data->mIncidentConstraints.begin(), data->mIncidentConstraints.end(), m_constraints[i])
                    == data->mIncidentConstraints.end()) return false;
                ++incidentCount;
            }
        if (incidentCount != data->mIncidentConstraints.size()) return false;
        for (const auto& value : data->mConstraints)
        {
            int occurrences = 0;
            for (int i = 0; i < m_constraints.size(); ++i) occurrences += m_constraints[i] == value.mConstraint;
            if (occurrences != int(value.mRegistered)) return false;
            for (auto* body : {&value.mConstraint->getRigidBodyA(), &value.mConstraint->getRigidBodyB()})
                for (int i = 0; i < body->getNumConstraintRefs(); ++i)
                    if (body->getConstraintRef(i) == value.mConstraint) return false;
        }
        if (data->mState == PreparedRegistrationBatch::Data::State::Admitted)
        {
            std::size_t bodies = std::size_t(m_collisionObjects.size());
            std::size_t dynamic = std::size_t(m_nonStaticRigidBodies.size());
            std::size_t constraints = std::size_t(m_constraints.size());
            for (const auto& weak : mRegistrationBatches)
                if (const auto pendingBatch = weak.lock())
                {
                    const auto count = pendingBatch->mBodyAdditions;
                    if (count > std::size_t(m_collisionObjects.capacity()) - bodies
                        || count > std::size_t(m_nonStaticRigidBodies.capacity()) - dynamic
                        || pendingBatch->mConstraintAdditions > std::size_t(m_constraints.capacity()) - constraints)
                        return false;
                    bodies += count; dynamic += count; constraints += pendingBatch->mConstraintAdditions;
                }
        }
        return true;
    }

    void NativeDynamicsWorld::admitRegistrationBatch(PreparedRegistrationBatch& prepared)
    {
        if (!validateRegistrationBatch(prepared)) throw std::invalid_argument("Stale native registration batch");
        auto& data = *prepared.mData;
        if (data.mState == PreparedRegistrationBatch::Data::State::Admitted) return;
        mRegistrationBatches.push_back(prepared.mData);
        const auto owner = std::find_if(mNativeOwners.begin(), mNativeOwners.end(),
            [&](const Owner& value) { return value.mIdentity == data.mIdentity; });
        owner->mPendingRegistration = data.mTag;
        data.mState = PreparedRegistrationBatch::Data::State::Admitted;
        try
        {
            std::size_t bodyCapacity = std::size_t(m_collisionObjects.size());
            std::size_t dynamicCapacity = std::size_t(m_nonStaticRigidBodies.size());
            std::size_t constraintCapacity = std::size_t(m_constraints.size());
            const auto add = [](std::size_t& capacity, std::size_t increment) {
                if (increment > std::size_t(std::numeric_limits<int>::max()) - capacity)
                    throw std::length_error("Native registration capacity overflow");
                capacity += increment;
            };
            for (const auto& weak : mRegistrationBatches)
                if (const auto pending = weak.lock())
                {
                    add(bodyCapacity, pending->mBodyAdditions); add(dynamicCapacity, pending->mBodyAdditions);
                    add(constraintCapacity, pending->mConstraintAdditions);
                }
            m_collisionObjects.reserve(static_cast<int>(bodyCapacity));
            m_nonStaticRigidBodies.reserve(static_cast<int>(dynamicCapacity));
            m_constraints.reserve(static_cast<int>(constraintCapacity));
            for (auto& value : data.mOperations)
                if (value.mOperation.mKind == RegistrationKind::AddBody)
                {
                    auto* body = value.mOperation.mBody;
                    const auto source = std::find_if(data.mBodies.begin(), data.mBodies.end(),
                        [&](const auto& item) { return item.mBody == body; });
                    value.mProxy = getBroadphase()->createProxy(source->mMinimum, source->mMaximum,
                        body->getCollisionShape()->getShapeType(), body, value.mOperation.mGroup,
                        value.mOperation.mMask, getDispatcher());
                    if (!value.mProxy) throw std::runtime_error("Native registration proxy admission failed");
                }
        }
        catch (...) { rollbackRegistrationBatch(prepared); throw; }
    }

    void NativeDynamicsWorld::rollbackRegistrationBatch(PreparedRegistrationBatch& prepared) noexcept
    {
        auto* data = prepared.mData.get();
        if (!data || data->mWorld != this || data->mLifetime.lock() != mLifetime
            || data->mState != PreparedRegistrationBatch::Data::State::Admitted) return;
        discardRegistrationProxies(*data);
        data->mState = PreparedRegistrationBatch::Data::State::Prepared;
        const auto owner = std::find_if(mNativeOwners.begin(), mNativeOwners.end(),
            [&](const Owner& value) { return value.mIdentity == data->mIdentity; });
        if (owner != mNativeOwners.end() && owner->mPendingRegistration.lock() == data->mTag)
            owner->mPendingRegistration.reset();
        std::erase_if(mRegistrationBatches, [&](const auto& weak) {
            const auto value = weak.lock(); return !value || value.get() == data;
        });
    }

    bool NativeDynamicsWorld::commitRegistrationBatch(PreparedRegistrationBatch& prepared) noexcept
    {
        if (!validateRegistrationBatch(prepared)
            || prepared.mData->mState != PreparedRegistrationBatch::Data::State::Admitted) return false;
        auto& data = *prepared.mData;
        for (auto& value : data.mOperations)
        {
            const auto& operation = value.mOperation;
            switch (operation.mKind)
            {
                case RegistrationKind::RemoveConstraint:
                    btDiscreteDynamicsWorld::removeConstraint(operation.mConstraint); break;
                case RegistrationKind::AddConstraint:
                    assert(m_constraints.size() < m_constraints.capacity());
                    btDiscreteDynamicsWorld::addConstraint(operation.mConstraint, false); break;
                case RegistrationKind::RemoveBody:
                    btDiscreteDynamicsWorld::removeRigidBody(operation.mBody); break;
                case RegistrationKind::AddBody:
                {
                    auto& body = *operation.mBody;
                    assert(!body.getBroadphaseHandle() && body.getWorldArrayIndex() == -1 && value.mProxy);
                    if (!body.isStaticOrKinematicObject() && !(body.getFlags() & BT_DISABLE_WORLD_GRAVITY))
                        body.setGravity(getGravity());
                    if (!body.isStaticObject())
                    {
                        assert(m_nonStaticRigidBodies.size() < m_nonStaticRigidBodies.capacity());
                        m_nonStaticRigidBodies.push_back(&body);
                    }
                    else body.setActivationState(ISLAND_SLEEPING);
                    assert(m_collisionObjects.size() < m_collisionObjects.capacity());
                    body.setWorldArrayIndex(m_collisionObjects.size()); m_collisionObjects.push_back(&body);
                    body.setBroadphaseHandle(value.mProxy); value.mProxy = nullptr;
                    break;
                }
            }
        }
        const bool routed = commitNativeMotionOwnerBodies(*data.mRouting);
        assert(routed); (void)routed;
        const auto owner = std::find_if(mNativeOwners.begin(), mNativeOwners.end(),
            [&](const Owner& value) { return value.mIdentity == data.mIdentity; });
        owner->mPendingRegistration.reset();
        data.mState = PreparedRegistrationBatch::Data::State::Consumed;
        std::erase_if(mRegistrationBatches, [&](const auto& weak) {
            const auto value = weak.lock(); return !value || value.get() == &data;
        });
        return true;
    }
}
