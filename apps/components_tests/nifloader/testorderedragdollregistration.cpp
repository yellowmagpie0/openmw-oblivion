#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/nifbullet/nativedynamicsworld.hpp>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/NarrowPhaseCollision/btPersistentManifold.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <gtest/gtest.h>

#include <array>

namespace
{
    using Actor = NifBullet::ActorRagdollPhysics;
    using Context = NifBullet::RagdollNativeRegistrationContext;
    using Event = ESM4::PhysicalRegistrationEventKind;
    class RegistrationFaultBroadphase : public btDbvtBroadphase
    {
    public:
        int mCreates = 0, mFailAt = 0;
        btBroadphaseProxy* createProxy(const btVector3& minimum, const btVector3& maximum,
            int type, void* object, int group, int mask, btDispatcher* dispatcher) override
        {
            if (++mCreates == mFailAt) throw std::runtime_error("late actor proxy failure");
            return btDbvtBroadphase::createProxy(minimum, maximum, type, object, group, mask, dispatcher);
        }
    };
    struct OrderedRagdollRegistrationTest : ::testing::Test
    {
        btDefaultCollisionConfiguration mConfiguration;
        btCollisionDispatcher mDispatcher{&mConfiguration};
        RegistrationFaultBroadphase mBroadphase;
        btSequentialImpulseConstraintSolver mSolver;
        NifBullet::NativeDynamicsWorld mWorld{&mDispatcher, &mBroadphase, &mSolver, &mConfiguration};
        NifBullet::ActorRagdollDefinition mDefinition;
        std::array<btTransform, 2> mPoses;
        NifBullet::RagdollInternalCollisionFilter mMasks;
        const std::array<Context, 2> mContexts{{{12, 0, false, false}, {24, 0, false, false}}};
        const std::array<std::uint32_t, 2> mRequests{24, 12};
        OrderedRagdollRegistrationTest()
        {
            mWorld.setGravity(btVector3(0, 0, 0));
            mMasks.mSystemGroup = 1;
            mMasks.mMasks.mLayerMasks.fill(0xffffffffu);
            mMasks.mMasks.mBoneMasks.fill(0xffffffffu);
            NifBullet::RagdollBodyDefinition body{};
            body.mRecord = 12; body.mNodeRecord = 100; body.mMass = 2;
            body.mInertia = {1,0,0, 0,1,0, 0,0,1};
            body.mShape = NifBullet::RagdollSphere{.5f}; body.mInfoFilter.mLayer = 8;
            body.mBlend = NifBullet::RagdollBlendDefinition{1000, 1, 0, 0};
            body.mAssociatedConstraints = {25, 25};
            mDefinition.mBodies.push_back(body);
            body.mRecord = 24; body.mNodeRecord = 200;
            body.mBlend = NifBullet::RagdollBlendDefinition{2000, 1, 1, 1};
            body.mAssociatedConstraints.clear(); mDefinition.mBodies.push_back(body);
            NifBullet::RagdollHingeJoint hinge{};
            hinge.mA = {{0,0,0},{1,0,0},{0,1,0}}; hinge.mB = hinge.mA;
            hinge.mMin = -.2f; hinge.mMax = .2f;
            NifBullet::RagdollJointDefinition joint{};
            joint.mRecord = 25; joint.mBodyA = 0; joint.mBodyB = 1; joint.mJoint = hinge;
            mDefinition.mJoints.push_back(joint);
            mPoses = {btTransform(btQuaternion::getIdentity(),btVector3(0,0,0)),
                btTransform(btQuaternion::getIdentity(),btVector3(.5,0,0))};
        }
        std::unique_ptr<Actor> actor()
        {
            return std::make_unique<Actor>(mDefinition, mWorld, 1, mPoses, 1, -1, nullptr, &mMasks);
        }
        void onlyB(Actor& value)
        {
            value.setWorldRegistrationSelection({{24}, {}});
            auto words = value.captureInternalCollisionFilters();
            words[1].mPackedFilter = 0x24008;
            auto filters = value.prepareInternalCollisionFilters(words);
            ASSERT_TRUE(value.commitInternalCollisionFilters(*filters));
        }
        const btPersistentManifold* contact(const btCollisionObject* a, const btCollisionObject* b)
        {
            for (int i=0; i<mDispatcher.getNumManifolds(); ++i)
            {
                const auto* value = mDispatcher.getManifoldByIndexInternal(i);
                if ((value->getBody0()==a && value->getBody1()==b)
                    || (value->getBody0()==b && value->getBody1()==a)) return value;
            }
            return nullptr;
        }
    };

    TEST_F(OrderedRagdollRegistrationTest, CompositePublicationRetainsReadmissionAndAppliesInheritedInternalFilter)
    {
        auto value = actor(); auto* joint = mWorld.getConstraint(0); onlyB(*value);
        auto* a = value->collisionObjects()[0]; auto* b = value->collisionObjects()[1];
        const auto* oldProxy = b->getBroadphaseHandle();
        btSphereShape outsideShape(.5); btCollisionObject outside;
        outside.setCollisionShape(&outsideShape);
        outside.setWorldTransform(btTransform(btQuaternion::getIdentity(),btVector3(.5,.5,0)));
        mWorld.addCollisionObject(&outside,1,-1);
        struct Remove { btDynamicsWorld& w; btCollisionObject& b; ~Remove(){w.removeCollisionObject(&b);} } remove{mWorld,outside};
        mWorld.performDiscreteCollisionDetection();
        const auto* oldContact = contact(b,&outside); ASSERT_NE(oldContact,nullptr);
        const auto oldWords = value->captureInternalCollisionFilters();
        auto oldFilter = value->prepareInternalCollisionFilters(oldWords);
        auto token = value->prepareNativeRegistrationRequests(mRequests,mContexts);
        EXPECT_EQ(token->plan().mReturnsTrue,(std::vector<unsigned char>{1,1}));
        EXPECT_TRUE(token->plan().mGraph.mBodies[1].mHasRememberedWorldOwner);
        EXPECT_EQ(token->plan().mGraph.mBodies[1].mPackedFilter,0x14008u);
        EXPECT_EQ(std::count_if(token->plan().mEvents.begin(),token->plan().mEvents.end(),
            [](const auto& e){return e.mKind==Event::ConstraintRequest;}),2);
        EXPECT_EQ(token->plan().mGraph.mBodies[0].mAssociatedConstraints,(std::vector<std::size_t>{0,0}));
        value->admitNativeRegistration(*token);
        EXPECT_EQ(value->captureWorldRegistrationSelection(),(NifBullet::RagdollWorldRegistration{{24},{}}));
        EXPECT_EQ(value->captureInternalCollisionFilters(),oldWords);
        EXPECT_EQ(b->getBroadphaseHandle(),oldProxy); EXPECT_EQ(contact(b,&outside),oldContact);
        EXPECT_FALSE(value->commitInternalCollisionFilters(*oldFilter));
        EXPECT_THROW(value->prepareWorldRegistrationSelection({{12,24},{25}}),std::invalid_argument);
        value->rollbackNativeRegistration(*token);
        EXPECT_TRUE(value->validateNativeRegistration(*token));
        EXPECT_EQ(contact(b,&outside),oldContact);
        value->admitNativeRegistration(*token);
        ASSERT_TRUE(value->commitNativeRegistration(*token));
        EXPECT_FALSE(value->commitNativeRegistration(*token));
        EXPECT_EQ(value->captureWorldRegistrationSelection(),(NifBullet::RagdollWorldRegistration{{12,24},{25}}));
        EXPECT_NE(b->getBroadphaseHandle(),oldProxy); EXPECT_EQ(contact(b,&outside),nullptr);
        EXPECT_EQ(mWorld.getConstraint(0),joint); EXPECT_EQ(value->collisionObjects()[0],a);
        EXPECT_EQ(value->collisionObjects()[1],b);
        EXPECT_EQ(value->captureInternalCollisionFilters()[1].mPackedFilter,0x14008u);
        EXPECT_FALSE(a->checkCollideWith(b)); EXPECT_FALSE(b->checkCollideWith(a));
        EXPECT_FALSE(value->commitInternalCollisionFilters(*oldFilter));
        EXPECT_TRUE(token->plan().mGraph.mConstraints[0].mRegistered);
        mWorld.performDiscreteCollisionDetection();
        EXPECT_EQ(contact(a,b),nullptr); EXPECT_NE(contact(b,&outside),nullptr);
    }

    TEST_F(OrderedRagdollRegistrationTest, ExplicitCounterAndOverridePreventGainRemovalAndListsStayDirected)
    {
        for (int mode=0; mode<2; ++mode)
        {
            auto value = actor(); onlyB(*value);
            auto contexts = mContexts;
            contexts[1].mCounter = mode==0 ? 1 : 0; contexts[1].mRegistrationOverride = mode==1;
            const auto* original = value->collisionObjects()[1]->getBroadphaseHandle();
            auto token = value->prepareNativeRegistrationRequests(mRequests,contexts);
            EXPECT_FALSE(std::any_of(token->plan().mEvents.begin(),token->plan().mEvents.end(),
                [](const auto& e){return e.mKind==Event::BodyRemoved;}));
            value->admitNativeRegistration(*token); ASSERT_TRUE(value->commitNativeRegistration(*token));
            EXPECT_EQ(value->collisionObjects()[1]->getBroadphaseHandle(),original);
            EXPECT_EQ(value->captureInternalCollisionFilters()[1].mPackedFilter,0x24008u);
        }
        mDefinition.mBodies[0].mAssociatedConstraints.clear();
        mDefinition.mBodies[1].mAssociatedConstraints = {25};
        auto value = actor(); value->setWorldRegistrationSelection({{}, {}});
        const std::array<std::uint32_t,1> request{12};
        auto token = value->prepareNativeRegistrationRequests(request,mContexts);
        value->admitNativeRegistration(*token); ASSERT_TRUE(value->commitNativeRegistration(*token));
        EXPECT_EQ(value->captureWorldRegistrationSelection(),(NifBullet::RagdollWorldRegistration{{12},{}}));
    }

    TEST_F(OrderedRagdollRegistrationTest, LaterOwnerFailureRollsBackWithoutPublishingEarlierOwner)
    {
        auto first=actor(), second=actor(); onlyB(*first); onlyB(*second);
        auto a=first->prepareNativeRegistrationRequests(mRequests,mContexts);
        auto b=second->prepareNativeRegistrationRequests(mRequests,mContexts);
        first->admitNativeRegistration(*a); mBroadphase.mFailAt=mBroadphase.mCreates+2;
        EXPECT_THROW(second->admitNativeRegistration(*b),std::runtime_error);
        EXPECT_EQ(mWorld.getNumCollisionObjects(),2); EXPECT_EQ(mWorld.getNumConstraints(),0);
        EXPECT_EQ(first->captureWorldRegistrationSelection(),(NifBullet::RagdollWorldRegistration{{24},{}}));
        first->rollbackNativeRegistration(*a);
        EXPECT_TRUE(first->validateNativeRegistration(*a)); EXPECT_TRUE(second->validateNativeRegistration(*b));
        mBroadphase.mFailAt=0;
        first->admitNativeRegistration(*a); second->admitNativeRegistration(*b);
        ASSERT_TRUE(first->validateNativeRegistration(*a)); ASSERT_TRUE(second->validateNativeRegistration(*b));
        ASSERT_TRUE(first->commitNativeRegistration(*a)); ASSERT_TRUE(second->commitNativeRegistration(*b));
        EXPECT_EQ(mWorld.getNumCollisionObjects(),4); EXPECT_EQ(mWorld.getNumConstraints(),2);
    }

    TEST_F(OrderedRagdollRegistrationTest, StaleBlendPoseMembershipAndIgnoreStateRejectBeforeRelease)
    {
        auto value=actor(); onlyB(*value);
        auto token=value->prepareNativeRegistrationRequests(mRequests,mContexts);
        auto blends=value->captureNativeBlendStates(); const auto original=blends;
        blends[1].mGains={.5f,.5f}; value->restoreNativeBlendStates(blends);
        EXPECT_FALSE(value->validateNativeRegistration(*token));
        EXPECT_THROW(value->admitNativeRegistration(*token),std::invalid_argument);
        value->restoreNativeBlendStates(original); ASSERT_TRUE(value->validateNativeRegistration(*token));
        value->admitNativeRegistration(*token);
        auto* a=value->collisionObjects()[0]; auto* b=value->collisionObjects()[1];
        a->setIgnoreCollisionCheck(b,true);
        EXPECT_FALSE(value->commitNativeRegistration(*token));
        EXPECT_EQ(mWorld.getNumCollisionObjects(),1);
        value->rollbackNativeRegistration(*token); a->setIgnoreCollisionCheck(b,false);
        ASSERT_TRUE(value->validateNativeRegistration(*token));
        auto* rigid=btRigidBody::upcast(b); const auto pose=rigid->getWorldTransform();
        rigid->translate(btVector3(1,0,0)); EXPECT_FALSE(value->validateNativeRegistration(*token));
        rigid->setWorldTransform(pose); ASSERT_TRUE(value->validateNativeRegistration(*token));
        value->setWorldRegistrationSelection({{12,24},{25}});
        EXPECT_FALSE(value->validateNativeRegistration(*token));
        EXPECT_EQ(mWorld.getNumCollisionObjects(),2);
    }

    TEST_F(OrderedRagdollRegistrationTest, MalformedInputsAndTraversalBoundsLeavePublishedOwnerUntouched)
    {
        auto value=actor(); onlyB(*value); const auto before=value->captureWorldRegistrationSelection();
        EXPECT_THROW(value->prepareNativeRegistrationRequests(mRequests,{}),std::invalid_argument);
        auto contexts=mContexts; std::swap(contexts[0],contexts[1]);
        EXPECT_THROW(value->prepareNativeRegistrationRequests(mRequests,contexts),std::invalid_argument);
        const std::array<std::uint32_t,1> unknown{999};
        EXPECT_THROW(value->prepareNativeRegistrationRequests(unknown,mContexts),std::invalid_argument);
        EXPECT_THROW(value->prepareNativeRegistrationRequests(mRequests,mContexts,1),std::length_error);
        EXPECT_EQ(value->captureWorldRegistrationSelection(),before);
        EXPECT_EQ(mWorld.getNumCollisionObjects(),1);
        mDefinition.mBodies[0].mAssociatedConstraints={999};
        EXPECT_THROW(actor(),std::invalid_argument);
        EXPECT_EQ(mWorld.getNumCollisionObjects(),1);
    }

    TEST_F(OrderedRagdollRegistrationTest, PreparedAndAdmittedHandlesOutliveActorAndWorld)
    {
        std::unique_ptr<Actor::PreparedNativeRegistration> admitted, prepared;
        {
            auto value=actor(); onlyB(*value);
            admitted=value->prepareNativeRegistrationRequests(mRequests,mContexts);
            prepared=value->prepareNativeRegistrationRequests(mRequests,mContexts);
            value->admitNativeRegistration(*admitted);
        }
        EXPECT_EQ(mWorld.getNumCollisionObjects(),0); EXPECT_EQ(mWorld.getNumConstraints(),0);
        admitted.reset(); prepared.reset(); mWorld.performDiscreteCollisionDetection();
        auto world=std::make_unique<NifBullet::NativeDynamicsWorld>(&mDispatcher,&mBroadphase,&mSolver,&mConfiguration);
        auto value=std::make_unique<Actor>(mDefinition,*world,1,mPoses,1,-1,nullptr,&mMasks);
        onlyB(*value);
        admitted=value->prepareNativeRegistrationRequests(mRequests,mContexts); value->admitNativeRegistration(*admitted);
        world.reset(); EXPECT_FALSE(value->validateNativeRegistration(*admitted));
        value.reset(); admitted.reset();
    }
}
