#include <components/nifbullet/nativedynamicsworld.hpp>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/NarrowPhaseCollision/btPersistentManifold.h>
#include <BulletDynamics/ConstraintSolver/btPoint2PointConstraint.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <LinearMath/btAlignedAllocator.h>
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <memory>

namespace
{
    using World = NifBullet::NativeDynamicsWorld;
    using Kind = World::RegistrationKind;
    using Op = World::RegistrationOperation;
    class FaultBroadphase : public btDbvtBroadphase
    {
    public:
        int mCreates = 0, mFailAt = 0, mNullAt = 0;
        std::vector<btBroadphaseProxy*> mAlive;
        FaultBroadphase() { mAlive.reserve(1024); }
        btBroadphaseProxy* createProxy(const btVector3& lo, const btVector3& hi, int type, void* object,
            int group, int mask, btDispatcher* dispatcher) override
        {
            ++mCreates;
            if (mCreates == mFailAt) throw std::runtime_error("fixture proxy creation failure");
            if (mCreates == mNullAt) return nullptr;
            auto* result = btDbvtBroadphase::createProxy(lo, hi, type, object, group, mask, dispatcher);
            mAlive.push_back(result); return result;
        }
        void destroyProxy(btBroadphaseProxy* proxy, btDispatcher* dispatcher) override
        {
            std::erase(mAlive, proxy); btDbvtBroadphase::destroyProxy(proxy, dispatcher);
        }
    };
    class InspectableWorld : public World
    {
    public:
        using World::World;
        int bodyCapacity() const { return m_collisionObjects.capacity(); }
        int dynamicCapacity() const { return m_nonStaticRigidBodies.capacity(); }
        int constraintCapacity() const { return m_constraints.capacity(); }
    };
    std::size_t bulletAllocations = 0;
    void* countBulletAllocation(std::size_t size) { ++bulletAllocations; return std::malloc(size); }
    struct CountBulletAllocations
    {
        CountBulletAllocations() { bulletAllocations = 0; btAlignedAllocSetCustom(countBulletAllocation, std::free); }
        ~CountBulletAllocations() { btAlignedAllocSetCustom(nullptr, nullptr); }
    };
    struct NativeWorldRegistrationTest : ::testing::Test
    {
        btDefaultCollisionConfiguration mConfiguration;
        btCollisionDispatcher mDispatcher{&mConfiguration};
        FaultBroadphase mBroadphase;
        btSequentialImpulseConstraintSolver mSolver;
        InspectableWorld mWorld{&mDispatcher, &mBroadphase, &mSolver, &mConfiguration};
        btSphereShape mShape{.5};
        std::vector<std::unique_ptr<btRigidBody>> mBodies;
        std::vector<std::unique_ptr<btTypedConstraint>> mConstraints;
        std::array<int, 3> mOwners{};
        NativeWorldRegistrationTest() { mWorld.setGravity(btVector3(0, 0, 0)); }
        ~NativeWorldRegistrationTest() override
        {
            for (auto& owner : mOwners) mWorld.unregisterNativeMotionOwner(&owner);
            while (mWorld.getNumConstraints()) mWorld.removeConstraint(mWorld.getConstraint(0));
            while (mWorld.getNumCollisionObjects()) mWorld.removeCollisionObject(mWorld.getCollisionObjectArray()[0]);
        }
        btRigidBody* body(float x, bool registered = true, btScalar mass = 2)
        {
            btVector3 inertia(0, 0, 0); if (mass) mShape.calculateLocalInertia(mass, inertia);
            auto value = std::make_unique<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(mass, nullptr, &mShape, inertia));
            value->setWorldTransform(btTransform(btQuaternion::getIdentity(), btVector3(x, 0, 0)));
            if (registered) mWorld.addRigidBody(value.get(), 1, -1);
            auto* result = value.get(); mBodies.push_back(std::move(value)); return result;
        }
        void owner(unsigned index, std::span<btRigidBody* const> selected)
        {
            std::vector<btCollisionObject*> objects(selected.begin(), selected.end());
            mWorld.registerNativeMotionOwner(&mOwners[index], objects, [](float) {});
        }
        btTypedConstraint* constraint(btRigidBody* a, btRigidBody* b, bool registered = true, bool linked = false)
        {
            auto value = std::make_unique<btPoint2PointConstraint>(*a, *b, btVector3(0, 0, 0), btVector3(0, 0, 0));
            auto* result = value.get(); mConstraints.push_back(std::move(value));
            if (registered) mWorld.addConstraint(result, linked);
            return result;
        }
        const btPersistentManifold* manifold(const btRigidBody* a, const btRigidBody* b)
        {
            for (int i = 0; i < mDispatcher.getNumManifolds(); ++i)
            {
                const auto* value = mDispatcher.getManifoldByIndexInternal(i);
                if ((value->getBody0() == a && value->getBody1() == b)
                    || (value->getBody0() == b && value->getBody1() == a)) return value;
            }
            return nullptr;
        }
    };

    TEST_F(NativeWorldRegistrationTest, LateProxyFailureAndDiscardPreserveExactLiveContactsAndPermitRetry)
    {
        const std::array bodies{body(0), body(.5)}; owner(0, bodies);
        const std::array operations{Op{Kind::RemoveBody, bodies[0]}, Op{Kind::AddBody, bodies[0], nullptr, 1, -1},
            Op{Kind::RemoveBody, bodies[1]}, Op{Kind::AddBody, bodies[1], nullptr, 1, -1}};
        mWorld.performDiscreteCollisionDetection();
        const auto* contact = manifold(bodies[0], bodies[1]); ASSERT_NE(contact, nullptr);
        const auto* first = bodies[0]->getBroadphaseHandle(); const auto* second = bodies[1]->getBroadphaseHandle();
        auto token = mWorld.prepareRegistrationBatch(&mOwners[0], bodies, {}, operations);
        mBroadphase.mFailAt = mBroadphase.mCreates + 2;
        EXPECT_THROW(mWorld.admitRegistrationBatch(*token), std::runtime_error);
        EXPECT_EQ(mBroadphase.mAlive.size(), 2u);
        EXPECT_EQ(manifold(bodies[0], bodies[1]), contact);
        EXPECT_EQ(bodies[0]->getBroadphaseHandle(), first); EXPECT_EQ(bodies[1]->getBroadphaseHandle(), second);
        ASSERT_TRUE(mWorld.validateRegistrationBatch(*token));
        mBroadphase.mFailAt = 0;
        mWorld.admitRegistrationBatch(*token);
        EXPECT_EQ(mBroadphase.mAlive.size(), 4u);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 2);
        EXPECT_EQ(manifold(bodies[0], bodies[1]), contact);
        EXPECT_THROW(mWorld.stepSimulation(0, 0), std::logic_error);
        EXPECT_THROW(mWorld.performDiscreteCollisionDetection(), std::logic_error);
        mWorld.rollbackRegistrationBatch(*token);
        EXPECT_EQ(mBroadphase.mAlive.size(), 2u);
        EXPECT_EQ(manifold(bodies[0], bodies[1]), contact);
        mWorld.admitRegistrationBatch(*token);
        bool committed;
        { CountBulletAllocations count; committed = mWorld.commitRegistrationBatch(*token); }
        ASSERT_TRUE(committed); EXPECT_EQ(bulletAllocations, 0u);
        EXPECT_NE(bodies[0]->getBroadphaseHandle(), first); EXPECT_NE(bodies[1]->getBroadphaseHandle(), second);
        EXPECT_EQ(mBroadphase.mAlive.size(), 2u); EXPECT_EQ(manifold(bodies[0], bodies[1]), nullptr);
        mWorld.performDiscreteCollisionDetection();
        ASSERT_NE(manifold(bodies[0], bodies[1]), nullptr);
        EXPECT_GT(manifold(bodies[0], bodies[1])->getNumContacts(), 0);
        EXPECT_FALSE(mWorld.validateRegistrationBatch(*token)); EXPECT_FALSE(mWorld.commitRegistrationBatch(*token));
    }

    TEST_F(NativeWorldRegistrationTest, CohortAdmissionSumsCapacityAndRollsBackALaterOwnerFailure)
    {
        const std::array first{body(0, false), body(2, false)}, second{body(4, false), body(6, false)};
        owner(0, {}); owner(1, {});
        const std::array firstConstraints{constraint(first[0], first[1], false)};
        const std::array secondConstraints{constraint(second[0], second[1], false)};
        const std::array firstOps{Op{Kind::AddBody, first[0], nullptr, 1, -1}, Op{Kind::AddBody, first[1], nullptr, 1, -1},
            Op{Kind::AddConstraint, nullptr, firstConstraints[0]}};
        const std::array secondOps{Op{Kind::AddBody, second[0], nullptr, 1, -1}, Op{Kind::AddBody, second[1], nullptr, 1, -1},
            Op{Kind::AddConstraint, nullptr, secondConstraints[0]}};
        auto a = mWorld.prepareRegistrationBatch(&mOwners[0], first, firstConstraints, firstOps);
        auto b = mWorld.prepareRegistrationBatch(&mOwners[1], second, secondConstraints, secondOps);
        mWorld.admitRegistrationBatch(*a); mBroadphase.mNullAt = mBroadphase.mCreates + 2;
        EXPECT_THROW(mWorld.admitRegistrationBatch(*b), std::runtime_error);
        EXPECT_EQ(mBroadphase.mAlive.size(), 2u); EXPECT_EQ(mWorld.getNumCollisionObjects(), 0);
        mWorld.rollbackRegistrationBatch(*a); EXPECT_TRUE(mBroadphase.mAlive.empty());
        mBroadphase.mNullAt = 0;
        mWorld.admitRegistrationBatch(*a); mWorld.admitRegistrationBatch(*b);
        EXPECT_GE(mWorld.bodyCapacity(), 4); EXPECT_GE(mWorld.dynamicCapacity(), 4); EXPECT_GE(mWorld.constraintCapacity(), 2);
        bool firstCommitted, secondCommitted;
        {
            CountBulletAllocations count;
            firstCommitted = mWorld.commitRegistrationBatch(*a);
            secondCommitted = mWorld.commitRegistrationBatch(*b);
        }
        EXPECT_TRUE(firstCommitted); EXPECT_TRUE(secondCommitted); EXPECT_EQ(bulletAllocations, 0u);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 4); EXPECT_EQ(mWorld.getNumConstraints(), 2);
        EXPECT_EQ(mBroadphase.mAlive.size(), 4u);
        mWorld.stepSimulation(.001, 0);
    }

    TEST_F(NativeWorldRegistrationTest, RepeatedReadmissionPreservesBodyAndConstraintIdentities)
    {
        const std::array bodies{body(0), body(2)}; owner(0, bodies);
        const std::array constraints{constraint(bodies[0], bodies[1])};
        const std::array bad{Op{Kind::RemoveBody, bodies[0]}};
        EXPECT_THROW(mWorld.prepareRegistrationBatch(&mOwners[0], bodies, constraints, bad), std::invalid_argument);
        const std::array operations{Op{Kind::RemoveConstraint, nullptr, constraints[0]}, Op{Kind::RemoveBody, bodies[0]},
            Op{Kind::AddBody, bodies[0], nullptr, 4, 7}, Op{Kind::RemoveBody, bodies[0]},
            Op{Kind::AddBody, bodies[0], nullptr, 8, 15}, Op{Kind::AddConstraint, nullptr, constraints[0]}};
        auto token = mWorld.prepareRegistrationBatch(&mOwners[0], bodies, constraints, operations);
        auto stale = mWorld.prepareRegistrationBatch(&mOwners[0], bodies, constraints, operations);
        mWorld.admitRegistrationBatch(*token);
        EXPECT_FALSE(mWorld.validateRegistrationBatch(*stale));
        bool committed;
        { CountBulletAllocations count; committed = mWorld.commitRegistrationBatch(*token); }
        ASSERT_TRUE(committed); EXPECT_EQ(bulletAllocations, 0u);
        EXPECT_EQ(mWorld.getConstraint(0), constraints[0]); EXPECT_EQ(mWorld.getNumCollisionObjects(), 2);
        EXPECT_EQ(bodies[0]->getBroadphaseHandle()->m_collisionFilterGroup, 8);
        EXPECT_EQ(bodies[0]->getBroadphaseHandle()->m_collisionFilterMask, 15);
        EXPECT_EQ(&constraints[0]->getRigidBodyA(), bodies[0]);
        EXPECT_FALSE(mWorld.validateRegistrationBatch(*stale));
        EXPECT_THROW(mWorld.admitRegistrationBatch(*stale), std::invalid_argument);
    }

    TEST_F(NativeWorldRegistrationTest, BodyFlagsAndGravityFollowExplicitGroupBulletAdmission)
    {
        mWorld.setGravity(btVector3(0, 0, -9));
        const std::array bodies{body(0, false), body(2, false), body(4, false, 0)}; owner(0, {});
        bodies[1]->setCollisionFlags(bodies[1]->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        bodies[1]->setGravity(btVector3(1, 2, 3));
        bodies[2]->setActivationState(ACTIVE_TAG);
        const std::array operations{Op{Kind::AddBody, bodies[0], nullptr, 1, -1},
            Op{Kind::AddBody, bodies[1], nullptr, 1, -1}, Op{Kind::AddBody, bodies[2], nullptr, 1, -1}};
        auto token = mWorld.prepareRegistrationBatch(&mOwners[0], bodies, {}, operations);
        mWorld.admitRegistrationBatch(*token); ASSERT_TRUE(mWorld.commitRegistrationBatch(*token));
        EXPECT_EQ(bodies[0]->getGravity(), mWorld.getGravity());
        EXPECT_EQ(bodies[1]->getGravity(), btVector3(1, 2, 3));
        EXPECT_EQ(bodies[2]->getActivationState(), ISLAND_SLEEPING);
    }

    TEST_F(NativeWorldRegistrationTest, SourceBodyBoundsAndIncidentConstraintsInvalidatePreparation)
    {
        const std::array bodies{body(0), body(2)}; owner(0, bodies);
        const std::array operations{Op{Kind::RemoveBody, bodies[0]}, Op{Kind::AddBody, bodies[0], nullptr, 1, -1}};
        auto token = mWorld.prepareRegistrationBatch(&mOwners[0], bodies, {}, operations);
        const auto original = bodies[0]->getWorldTransform();
        bodies[0]->translate(btVector3(1, 0, 0)); EXPECT_FALSE(mWorld.validateRegistrationBatch(*token));
        bodies[0]->setWorldTransform(original); EXPECT_TRUE(mWorld.validateRegistrationBatch(*token));
        mShape.setUnscaledRadius(.75); EXPECT_FALSE(mWorld.validateRegistrationBatch(*token));
        mShape.setUnscaledRadius(.5); EXPECT_TRUE(mWorld.validateRegistrationBatch(*token));
        mWorld.admitRegistrationBatch(*token);
        auto* extra = constraint(bodies[0], bodies[1]);
        EXPECT_FALSE(mWorld.validateRegistrationBatch(*token)); EXPECT_FALSE(mWorld.commitRegistrationBatch(*token));
        mWorld.rollbackRegistrationBatch(*token); EXPECT_EQ(mWorld.getNumConstraints(), 1);
        mWorld.removeConstraint(extra); EXPECT_TRUE(mWorld.validateRegistrationBatch(*token));
    }

    TEST_F(NativeWorldRegistrationTest, InvalidResourcesOrderingAndLinkedCollisionConstraintsReject)
    {
        const std::array bodies{body(0), body(2)}; owner(0, bodies); int foreign = 0;
        const std::array malformed{Op{static_cast<Kind>(99)}};
        EXPECT_THROW(mWorld.prepareRegistrationBatch(&foreign, bodies, {}, {}), std::invalid_argument);
        EXPECT_THROW(mWorld.prepareRegistrationBatch(&mOwners[0], {}, {}, {}), std::invalid_argument);
        EXPECT_THROW(mWorld.prepareRegistrationBatch(&mOwners[0], bodies, {}, malformed), std::invalid_argument);
        const std::array duplicates{bodies[0], bodies[0]};
        EXPECT_THROW(mWorld.prepareRegistrationBatch(&mOwners[0], duplicates, {}, {}), std::invalid_argument);
        const std::array constraints{constraint(bodies[0], bodies[1], false)};
        const std::array invalidAdd{Op{Kind::AddBody, bodies[0], nullptr, 1, -1}};
        EXPECT_THROW(mWorld.prepareRegistrationBatch(&mOwners[0], bodies, constraints, invalidAdd), std::invalid_argument);
        mWorld.addConstraint(constraints[0], true);
        EXPECT_THROW(mWorld.prepareRegistrationBatch(&mOwners[0], bodies, constraints, {}), std::invalid_argument);
        mWorld.removeConstraint(constraints[0]);
        const std::array foreignBodies{body(4)};
        EXPECT_THROW(mWorld.prepareRegistrationBatch(&mOwners[0], foreignBodies, {}, {}), std::invalid_argument);
    }

    TEST_F(NativeWorldRegistrationTest, OwnerUnregisterDiscardsProxiesBeforeBodiesDieAndInvalidatesReusedIdentity)
    {
        const std::array bodies{body(0)}; owner(0, bodies);
        const std::array operations{Op{Kind::RemoveBody, bodies[0]}, Op{Kind::AddBody, bodies[0], nullptr, 1, -1}};
        auto admitted = mWorld.prepareRegistrationBatch(&mOwners[0], bodies, {}, operations);
        auto prepared = mWorld.prepareRegistrationBatch(&mOwners[0], bodies, {}, operations);
        mWorld.admitRegistrationBatch(*admitted); EXPECT_EQ(mBroadphase.mAlive.size(), 2u);
        mWorld.unregisterNativeMotionOwner(&mOwners[0]); EXPECT_EQ(mBroadphase.mAlive.size(), 1u);
        mWorld.removeRigidBody(bodies[0]); mBodies.clear(); EXPECT_TRUE(mBroadphase.mAlive.empty());
        owner(0, {});
        EXPECT_FALSE(mWorld.validateRegistrationBatch(*admitted)); EXPECT_FALSE(mWorld.validateRegistrationBatch(*prepared));
        EXPECT_THROW(mWorld.admitRegistrationBatch(*prepared), std::invalid_argument);
        admitted.reset(); prepared.reset(); mWorld.performDiscreteCollisionDetection();
    }

    TEST_F(NativeWorldRegistrationTest, OwnerRemovalDoesNotInvalidateAnotherOwnersArrayIndices)
    {
        const std::array first{body(0)}, second{body(2)}; owner(0, first); owner(1, second);
        const std::array aOps{Op{Kind::RemoveBody, first[0]}, Op{Kind::AddBody, first[0], nullptr, 1, -1}};
        const std::array bOps{Op{Kind::RemoveBody, second[0]}, Op{Kind::AddBody, second[0], nullptr, 1, -1}};
        auto a = mWorld.prepareRegistrationBatch(&mOwners[0], first, {}, aOps);
        auto b = mWorld.prepareRegistrationBatch(&mOwners[1], second, {}, bOps);
        mWorld.admitRegistrationBatch(*a); mWorld.admitRegistrationBatch(*b);
        mWorld.unregisterNativeMotionOwner(&mOwners[0]); mWorld.removeRigidBody(first[0]);
        EXPECT_TRUE(mWorld.validateRegistrationBatch(*b)); ASSERT_TRUE(mWorld.commitRegistrationBatch(*b));
        EXPECT_EQ(second[0]->getWorldArrayIndex(), 0);
    }

    TEST_F(NativeWorldRegistrationTest, WorldDestructionCleansProvisionalProxiesAndRejectsAddressReuse)
    {
        const std::array bodies{body(0, false)};
        alignas(InspectableWorld) std::byte storage[sizeof(InspectableWorld)];
        auto* world = std::construct_at(reinterpret_cast<InspectableWorld*>(storage),
            &mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        world->addRigidBody(bodies[0], 1, -1);
        const std::array<btCollisionObject*, 1> selected{bodies[0]};
        world->registerNativeMotionOwner(&mOwners[0], selected, [](float) {});
        const std::array operations{Op{Kind::RemoveBody, bodies[0]}, Op{Kind::AddBody, bodies[0], nullptr, 1, -1}};
        auto token = world->prepareRegistrationBatch(&mOwners[0], bodies, {}, operations);
        world->admitRegistrationBatch(*token); EXPECT_EQ(mBroadphase.mAlive.size(), 2u);
        std::destroy_at(world); EXPECT_TRUE(mBroadphase.mAlive.empty());
        world = std::construct_at(reinterpret_cast<InspectableWorld*>(storage),
            &mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        world->registerNativeMotionOwner(&mOwners[0], {}, [](float) {});
        EXPECT_FALSE(world->validateRegistrationBatch(*token)); EXPECT_FALSE(world->commitRegistrationBatch(*token));
        token.reset(); std::destroy_at(world);
    }
}
