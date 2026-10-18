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
#include <limits>

namespace
{
    using Actor = NifBullet::ActorRagdollPhysics;
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
        auto token = value->prepareNativeRegistrationRequests(mRequests,false);
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
        const auto committedStates = value->captureNativeBlendStates();
        EXPECT_TRUE(committedStates[0].mHasRememberedWorldOwner);
        EXPECT_TRUE(committedStates[1].mHasRememberedWorldOwner);
        EXPECT_EQ(committedStates[1].mRegistrationCounter,0);
        mWorld.performDiscreteCollisionDetection();
        EXPECT_EQ(contact(a,b),nullptr); EXPECT_NE(contact(b,&outside),nullptr);
    }

    TEST_F(OrderedRagdollRegistrationTest, ExplicitCounterAndOverridePreventGainRemovalAndListsStayDirected)
    {
        for (int mode=0; mode<2; ++mode)
        {
            auto value = actor(); onlyB(*value);
            auto states = value->captureNativeBlendStates();
            states[1].mRegistrationCounter = mode==0 ? 1 : 0;
            value->restoreNativeBlendStates(states);
            const auto* original = value->collisionObjects()[1]->getBroadphaseHandle();
            auto token = value->prepareNativeRegistrationRequests(mRequests,mode==1);
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
        auto token = value->prepareNativeRegistrationRequests(request,false);
        value->admitNativeRegistration(*token); ASSERT_TRUE(value->commitNativeRegistration(*token));
        EXPECT_EQ(value->captureWorldRegistrationSelection(),(NifBullet::RagdollWorldRegistration{{12},{}}));
    }

    TEST_F(OrderedRagdollRegistrationTest, LaterOwnerFailureRollsBackWithoutPublishingEarlierOwner)
    {
        auto first=actor(), second=actor(); onlyB(*first); onlyB(*second);
        auto a=first->prepareNativeRegistrationRequests(mRequests,false);
        auto b=second->prepareNativeRegistrationRequests(mRequests,false);
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
        auto token=value->prepareNativeRegistrationRequests(mRequests,false);
        auto blends=value->captureNativeBlendStates(); const auto original=blends;
        blends[1].mGains={.5f,.5f}; value->restoreNativeBlendStates(blends);
        EXPECT_FALSE(value->validateNativeRegistration(*token));
        EXPECT_THROW(value->admitNativeRegistration(*token),std::invalid_argument);
        value->restoreNativeBlendStates(original); ASSERT_TRUE(value->validateNativeRegistration(*token));
        blends = original; blends[1].mRegistrationCounter = 1;
        value->restoreNativeBlendStates(blends); EXPECT_FALSE(value->validateNativeRegistration(*token));
        blends = original; blends[1].mHasRememberedWorldOwner = true;
        value->restoreNativeBlendStates(blends); EXPECT_FALSE(value->validateNativeRegistration(*token));
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
        const std::array<std::uint32_t,1> unknown{999};
        EXPECT_THROW(value->prepareNativeRegistrationRequests(unknown,false),std::invalid_argument);
        EXPECT_THROW(value->prepareNativeRegistrationRequests(mRequests,false,1),std::length_error);
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
            admitted=value->prepareNativeRegistrationRequests(mRequests,false);
            prepared=value->prepareNativeRegistrationRequests(mRequests,false);
            value->admitNativeRegistration(*admitted);
        }
        EXPECT_EQ(mWorld.getNumCollisionObjects(),0); EXPECT_EQ(mWorld.getNumConstraints(),0);
        admitted.reset(); prepared.reset(); mWorld.performDiscreteCollisionDetection();
        auto world=std::make_unique<NifBullet::NativeDynamicsWorld>(&mDispatcher,&mBroadphase,&mSolver,&mConfiguration);
        auto value=std::make_unique<Actor>(mDefinition,*world,1,mPoses,1,-1,nullptr,&mMasks);
        onlyB(*value);
        admitted=value->prepareNativeRegistrationRequests(mRequests,false); value->admitNativeRegistration(*admitted);
        world.reset(); EXPECT_FALSE(value->validateNativeRegistration(*admitted));
        value.reset(); admitted.reset();
    }
    TEST_F(OrderedRagdollRegistrationTest, FreshInitializationIsOnceAndRestorePreservesSignedCountersAndRememberedOwner)
    {
        auto value = actor();
        auto initial = value->captureNativeBlendStates();
        for (const auto& state : initial)
        {
            EXPECT_EQ(state.mRegistrationCounter,0);
            EXPECT_FALSE(state.mHasRememberedWorldOwner);
        }
        value->setWorldRegistrationSelection({{24},{}});
        auto states = initial;
        states[0].mRegistrationCounter = std::numeric_limits<std::int32_t>::min();
        states[0].mHasRememberedWorldOwner = true;
        states[1].mRegistrationCounter = std::numeric_limits<std::int32_t>::max();
        states[1].mHasRememberedWorldOwner = true;
        value->restoreNativeBlendStates(states);
        const std::array<std::uint32_t,1> request{24};
        auto token = value->prepareNativeRegistrationRequests(request,false);
        value->admitNativeRegistration(*token);
        value->rollbackNativeRegistration(*token);
        auto after = value->captureNativeBlendStates();
        EXPECT_EQ(after[0].mRegistrationCounter,std::numeric_limits<std::int32_t>::min());
        EXPECT_TRUE(after[0].mHasRememberedWorldOwner);
        value->admitNativeRegistration(*token); ASSERT_TRUE(value->commitNativeRegistration(*token));
        after = value->captureNativeBlendStates();
        EXPECT_EQ(after[0].mRegistrationCounter,std::numeric_limits<std::int32_t>::min());
        EXPECT_EQ(after[1].mRegistrationCounter,std::numeric_limits<std::int32_t>::max());
        EXPECT_TRUE(after[0].mHasRememberedWorldOwner); EXPECT_TRUE(after[1].mHasRememberedWorldOwner);
        EXPECT_EQ(value->captureWorldRegistrationSelection(),(NifBullet::RagdollWorldRegistration{{24},{}}));
    }

    TEST_F(OrderedRagdollRegistrationTest, CompositeFilterRestorePublishesAcrossItsOwnRegistrationEpochAndRecreatesContacts)
    {
        auto value = actor(); auto* a = value->collisionObjects()[0]; auto* b = value->collisionObjects()[1];
        btSphereShape outsideShape(.5); btCollisionObject outside;
        outside.setCollisionShape(&outsideShape);
        outside.setWorldTransform(btTransform(btQuaternion::getIdentity(),btVector3(.5,.5,0)));
        mWorld.addCollisionObject(&outside,1,-1);
        struct Remove {btDynamicsWorld& w;btCollisionObject& o;~Remove(){w.removeCollisionObject(&o);}} remove{mWorld,outside};
        mWorld.performDiscreteCollisionDetection();
        const auto* internal = contact(a,b); const auto* external = contact(b,&outside);
        ASSERT_NE(internal,nullptr); ASSERT_NE(external,nullptr);
        const auto before = value->captureInternalCollisionFilters(); auto disabled = before;
        disabled[1].mPackedFilter |= 0x4000;
        auto independent = value->prepareInternalCollisionFilters(disabled);
        auto prepared = value->prepareRestore(value->capture());
        value->prepareRestoreWorldRegistration(*prepared,value->captureWorldRegistrationSelection());
        value->prepareRestoreInternalCollisionFilters(*prepared,disabled);
        value->admitPreparedRestore(*prepared);
        EXPECT_TRUE(value->validatePreparedRestore(*prepared));
        EXPECT_FALSE(value->commitInternalCollisionFilters(*independent));
        EXPECT_EQ(value->captureInternalCollisionFilters(),before);
        EXPECT_EQ(contact(a,b),internal); EXPECT_EQ(contact(b,&outside),external);
        value->rollbackPreparedRestore(*prepared); ASSERT_TRUE(value->validatePreparedRestore(*prepared));
        value->commitRestore(*prepared);
        EXPECT_EQ(value->captureInternalCollisionFilters(),disabled);
        EXPECT_FALSE(a->checkCollideWith(b)); EXPECT_EQ(contact(a,b),nullptr);
        EXPECT_EQ(contact(b,&outside),external);
        EXPECT_FALSE(value->commitInternalCollisionFilters(*independent));
        auto enabled = value->prepareRestore(value->capture());
        value->prepareRestoreInternalCollisionFilters(*enabled,before);
        value->commitRestore(*enabled);
        mWorld.performDiscreteCollisionDetection();
        EXPECT_TRUE(a->checkCollideWith(b)); ASSERT_NE(contact(a,b),nullptr);
        EXPECT_GT(contact(a,b)->getNumContacts(),0); EXPECT_EQ(contact(b,&outside),external);
    }

    TEST_F(OrderedRagdollRegistrationTest, WholeCohortFilterPoseAndCounterRestoreWaitsForEveryAdmission)
    {
        auto first = actor(), second = actor(); onlyB(*first); onlyB(*second);
        const auto prepare = [&](Actor& value) {
            auto poses = value.capture(); poses[0].mPose.setOrigin(btVector3(7,8,9));
            auto blends = value.captureNativeBlendStates(); blends[0].mRegistrationCounter = 42;
            blends[0].mHasRememberedWorldOwner = true;
            auto prepared = value.prepareRestore(poses,value.captureNativePackedVelocities(),
                value.captureNativeMotionModes(),blends);
            value.prepareRestoreWorldRegistration(*prepared,{{12,24},{25}});
            auto filters = value.captureInternalCollisionFilters(); filters[0].mPackedFilter |= 0x4000;
            value.prepareRestoreInternalCollisionFilters(*prepared,filters);
            return prepared;
        };
        const auto firstWords = first->captureInternalCollisionFilters(), secondWords = second->captureInternalCollisionFilters();
        const auto before = first->capture();
        auto a = prepare(*first), b = prepare(*second);
        first->admitPreparedRestore(*a); mBroadphase.mFailAt = mBroadphase.mCreates+1;
        EXPECT_THROW(second->admitPreparedRestore(*b),std::runtime_error);
        first->rollbackPreparedRestore(*a);
        EXPECT_EQ(mWorld.getNumCollisionObjects(),2); EXPECT_EQ(mWorld.getNumConstraints(),0);
        EXPECT_EQ(first->capture()[0].mPose,before[0].mPose);
        EXPECT_EQ(first->captureInternalCollisionFilters(),firstWords); EXPECT_EQ(second->captureInternalCollisionFilters(),secondWords);
        EXPECT_EQ(first->captureNativeBlendStates()[0].mRegistrationCounter,0);
        EXPECT_FALSE(first->captureNativeBlendStates()[0].mHasRememberedWorldOwner);
        ASSERT_TRUE(first->validatePreparedRestore(*a)); ASSERT_TRUE(second->validatePreparedRestore(*b));
        mBroadphase.mFailAt = 0;
        first->admitPreparedRestore(*a); second->admitPreparedRestore(*b);
        ASSERT_TRUE(first->validatePreparedRestore(*a)); ASSERT_TRUE(second->validatePreparedRestore(*b));
        first->commitRestore(*a); second->commitRestore(*b);
        EXPECT_EQ(mWorld.getNumCollisionObjects(),4); EXPECT_EQ(mWorld.getNumConstraints(),2);
        for (auto* value : {first.get(),second.get()})
        {
            EXPECT_EQ(value->capture()[0].mPose.getOrigin(),btVector3(7,8,9));
            EXPECT_EQ(value->captureNativeBlendStates()[0].mRegistrationCounter,42);
            EXPECT_TRUE(value->captureNativeBlendStates()[0].mHasRememberedWorldOwner);
            EXPECT_NE(value->captureInternalCollisionFilters()[0].mPackedFilter&0x4000u,0u);
            EXPECT_FALSE(value->collisionObjects()[0]->checkCollideWith(value->collisionObjects()[1]));
        }
    }

    TEST_F(OrderedRagdollRegistrationTest, CompositeRestoreRejectsStalePolicyAndIgnoreListsBeforePoseOrSelectionPublication)
    {
        auto value = actor(); auto words = value->captureInternalCollisionFilters();
        auto disabled = words; disabled[0].mPackedFilter |= 0x4000;
        auto poses = value->capture(); poses[0].mPose.setOrigin(btVector3(10,20,30));
        auto prepared = value->prepareRestore(poses);
        value->prepareRestoreWorldRegistration(*prepared,{{24},{}});
        value->prepareRestoreInternalCollisionFilters(*prepared,disabled);
        auto other = value->prepareInternalCollisionFilters(words);
        ASSERT_TRUE(value->commitInternalCollisionFilters(*other));
        EXPECT_FALSE(value->validatePreparedRestore(*prepared));
        EXPECT_THROW(value->commitRestore(*prepared),std::invalid_argument);
        EXPECT_EQ(value->capture()[0].mPose,mPoses[0]); EXPECT_EQ(mWorld.getNumCollisionObjects(),2);
        prepared = value->prepareRestore(poses);
        value->prepareRestoreWorldRegistration(*prepared,{{24},{}});
        value->prepareRestoreInternalCollisionFilters(*prepared,disabled);
        value->admitPreparedRestore(*prepared);
        auto* a = value->collisionObjects()[0]; auto* b = value->collisionObjects()[1];
        a->setIgnoreCollisionCheck(b,true);
        EXPECT_FALSE(value->validatePreparedRestore(*prepared));
        EXPECT_THROW(value->commitRestore(*prepared),std::invalid_argument);
        value->rollbackPreparedRestore(*prepared);
        EXPECT_EQ(value->capture()[0].mPose,mPoses[0]); EXPECT_EQ(mWorld.getNumCollisionObjects(),2);
        EXPECT_EQ(a->getNumObjectsWithoutCollision(),1);
        a->setIgnoreCollisionCheck(b,false); ASSERT_TRUE(value->validatePreparedRestore(*prepared));
    }

}
