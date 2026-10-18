#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/nifbullet/nativecollisiondispatcher.hpp>
#include <components/nifbullet/nativedynamicsworld.hpp>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/BroadphaseCollision/btOverlappingPairCache.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/NarrowPhaseCollision/btPersistentManifold.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <gtest/gtest.h>

#include <array>
#include <optional>

namespace
{
    using Actor = NifBullet::ActorRagdollPhysics;
    struct NativeCollisionDispatcherTest : ::testing::Test
    {
        btDefaultCollisionConfiguration mConfiguration;
        NifBullet::NativeCollisionDispatcher mDispatcher{&mConfiguration};
        btDbvtBroadphase mBroadphase;
        btSequentialImpulseConstraintSolver mSolver;
        NifBullet::NativeDynamicsWorld mWorld{&mDispatcher, &mBroadphase, &mSolver, &mConfiguration};
        NifBullet::RagdollInternalCollisionFilter mMasks;
        NifBullet::ActorRagdollDefinition mDefinition;
        const std::array<btTransform, 1> mPoses{btTransform::getIdentity()};
        NativeCollisionDispatcherTest()
        {
            NifBullet::RagdollBodyDefinition body{};
            body.mRecord = 12; body.mNodeRecord = 8; body.mMass = 2;
            body.mInertia = {1,0,0, 0,1,0, 0,0,1};
            body.mShape = NifBullet::RagdollSphere{.5f}; body.mInfoFilter.mLayer = 8;
            mDefinition.mBodies.push_back(body);
            mMasks.mMasks.mLayerMasks.fill(0xffffffffu);
            mMasks.mMasks.mBoneMasks.fill(0);
        }
        std::unique_ptr<Actor> actor(unsigned group, bool configured = true,
            Actor::Publication publication = Actor::Publication::Immediate)
        {
            auto masks = mMasks; masks.mSystemGroup = group;
            return std::make_unique<Actor>(mDefinition, mWorld, 1, mPoses, 1, -1,
                nullptr, configured ? &masks : nullptr, publication);
        }
        const btPersistentManifold* contact(const Actor& a, const Actor& b) const
        {
            for (int i = 0; i < mDispatcher.getNumManifolds(); ++i)
            {
                const auto* value = mDispatcher.getManifoldByIndexInternal(i);
                if ((value->getBody0() == a.collisionObjects()[0] && value->getBody1() == b.collisionObjects()[0])
                    || (value->getBody1() == a.collisionObjects()[0] && value->getBody0() == b.collisionObjects()[0]))
                    return value;
            }
            return nullptr;
        }
        void word(Actor& value, std::uint32_t packed)
        {
            auto words = value.captureInternalCollisionFilters(); words.front().mPackedFilter = packed;
            auto prepared = value.prepareInternalCollisionFilters(words);
            ASSERT_TRUE(value.commitInternalCollisionFilters(*prepared));
        }
    };

    TEST_F(NativeCollisionDispatcherTest, CrossOwnerChangesReleaseDeniedContactsAndReenableWithoutMovement)
    {
        auto a = actor(1), b = actor(2), outside = actor(3, false);
        mWorld.performDiscreteCollisionDetection();
        ASSERT_NE(contact(*a, *b), nullptr); ASSERT_GT(contact(*a, *b)->getNumContacts(), 0);
        const auto* external = contact(*a, *outside); ASSERT_NE(external, nullptr);
        auto words = b->captureInternalCollisionFilters(); words.front().mPackedFilter = 0x10008;
        auto prepared = b->prepareInternalCollisionFilters(words);
        EXPECT_NE(contact(*a, *b), nullptr);
        ASSERT_TRUE(b->commitInternalCollisionFilters(*prepared));
        EXPECT_EQ(contact(*a, *b), nullptr); EXPECT_EQ(contact(*a, *outside), external);
        mWorld.performDiscreteCollisionDetection(); EXPECT_EQ(contact(*a, *b), nullptr);
        word(*b, 0x20008);
        mWorld.performDiscreteCollisionDetection();
        ASSERT_NE(contact(*a, *b), nullptr); EXPECT_GT(contact(*a, *b)->getNumContacts(), 0);
        EXPECT_EQ(contact(*a, *outside), external);
        a->collisionObjects()[0]->setIgnoreCollisionCheck(b->collisionObjects()[0], true);
        EXPECT_FALSE(mDispatcher.needsCollision(a->collisionObjects()[0], b->collisionObjects()[0]));
        a->collisionObjects()[0]->setIgnoreCollisionCheck(b->collisionObjects()[0], false);
    }

    TEST_F(NativeCollisionDispatcherTest, LayerTwentyNineExceptionUsesTheSuppliedPairOrder)
    {
        auto a = actor(1), b = actor(2);
        word(*a, 0x1001d); word(*b, 0x24008);
        auto* left = a->collisionObjects()[0]; auto* right = b->collisionObjects()[0];
        EXPECT_TRUE(mDispatcher.needsCollision(left, right));
        EXPECT_FALSE(mDispatcher.needsCollision(right, left));
        mWorld.performDiscreteCollisionDetection();
        const auto& pairs = mWorld.getPairCache()->getOverlappingPairArray();
        ASSERT_EQ(pairs.size(), 1);
        const bool nativeOrder = pairs[0].m_pProxy0->m_clientObject == left;
        EXPECT_EQ(contact(*a, *b) != nullptr, nativeOrder);
    }

    TEST_F(NativeCollisionDispatcherTest, ConflictingWorldMasksRejectBeforePublishingBodies)
    {
        auto a = actor(1);
        mMasks.mMasks.mLayerMasks[8] = 0;
        EXPECT_THROW(actor(2), std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 1);
        a.reset();
        auto b = actor(2);
        EXPECT_EQ(mWorld.getNumCollisionObjects(), 1);
    }

    TEST_F(NativeCollisionDispatcherTest, DeferredAndRestoredWordsStayBoundToTheirVectorObject)
    {
        auto a = actor(1), b = actor(2, true, Actor::Publication::Deferred);
        word(*b, 0x10008);
        ASSERT_TRUE(b->publish(mWorld));
        mWorld.performDiscreteCollisionDetection(); EXPECT_EQ(contact(*a, *b), nullptr);
        auto saved = b->captureInternalCollisionFilters(); saved.front().mPackedFilter = 0x20008;
        auto prepared = b->prepareRestore(b->capture());
        b->prepareRestoreWorldRegistration(*prepared, b->captureWorldRegistrationSelection());
        b->prepareRestoreInternalCollisionFilters(*prepared, saved);
        b->admitPreparedRestore(*prepared);
        EXPECT_FALSE(mDispatcher.needsCollision(a->collisionObjects()[0], b->collisionObjects()[0]));
        b->rollbackPreparedRestore(*prepared);
        b->commitRestore(*prepared);
        EXPECT_TRUE(mDispatcher.needsCollision(a->collisionObjects()[0], b->collisionObjects()[0]));
        mWorld.performDiscreteCollisionDetection(); ASSERT_NE(contact(*a, *b), nullptr);
        b->setWorldRegistrationSelection({{}, {}});
        word(*b, 0x10008);
        b->setWorldRegistrationSelection({{12}, {}});
        mWorld.performDiscreteCollisionDetection(); EXPECT_EQ(contact(*a, *b), nullptr);
    }

    TEST_F(NativeCollisionDispatcherTest, WorldDestructionClearsBindingsBeforeRetainedActorsDie)
    {
        std::optional<NifBullet::NativeDynamicsWorld> world;
        world.emplace(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        mMasks.mSystemGroup = 7;
        auto retained = std::make_unique<Actor>(mDefinition, *world, 1, mPoses, 1, -1, nullptr, &mMasks);
        world.reset();
        world.emplace(&mDispatcher, &mBroadphase, &mSolver, &mConfiguration);
        mMasks.mMasks.mLayerMasks[8] = 0;
        auto replacement = std::make_unique<Actor>(mDefinition, *world, 1, mPoses, 1, -1, nullptr, &mMasks);
        retained.reset();
        EXPECT_TRUE(replacement->isPublished());
    }

    TEST_F(NativeCollisionDispatcherTest, FailedPublicationReleasesBorrowedCollisionBindings)
    {
        struct FailedWorld : NifBullet::NativeDynamicsWorld
        {
            using NativeDynamicsWorld::NativeDynamicsWorld;
            bool mFail = true;
            void addRigidBody(btRigidBody* body, int group, int mask) override
            {
                NativeDynamicsWorld::addRigidBody(body, group, mask);
                if (mFail) throw std::runtime_error("failure after body publication");
            }
        } failed{&mDispatcher, &mBroadphase, &mSolver, &mConfiguration};
        EXPECT_THROW((Actor(mDefinition, failed, 1, mPoses, 1, -1, nullptr, &mMasks)), std::runtime_error);
        EXPECT_EQ(failed.getNumCollisionObjects(), 0);
        mMasks.mMasks.mLayerMasks[8] = 0;
        failed.mFail = false;
        auto replacement = std::make_unique<Actor>(mDefinition, failed, 1, mPoses, 1, -1, nullptr, &mMasks);
        EXPECT_TRUE(replacement->isPublished());
    }

    TEST_F(NativeCollisionDispatcherTest, OrdinaryBulletMasksAndUnconfiguredGraphsRetainTheirPolicy)
    {
        mMasks.mMasks.mLayerMasks[8] = 0;
        auto a = actor(1), b = actor(2), unconfigured = actor(3, false);
        Actor excluded(mDefinition, mWorld, 1, mPoses, 1, 0);
        mWorld.performDiscreteCollisionDetection();
        EXPECT_EQ(contact(*a, *b), nullptr);
        ASSERT_NE(contact(*a, *unconfigured), nullptr);
        EXPECT_EQ(contact(*a, excluded), nullptr);
        EXPECT_EQ(contact(*unconfigured, excluded), nullptr);
    }
}
