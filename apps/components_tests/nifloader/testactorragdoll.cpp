#include "../nif/node.hpp"

#include <components/nif/physics.hpp>
#include <components/nif/controller.hpp>
#include <components/nifbullet/actorragdoll.hpp>
#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/nifbullet/ragdollcollisionfilter.hpp>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <span>
#include <cstring>
#include <limits>
#include <sstream>

namespace
{
    struct ActorRagdollTest : ::testing::Test
    {
        Nif::NIFFile mFile{ VFS::Path::Normalized("synthetic-ragdoll.nif") };
        template<class T> T& add()
        {
            auto object = std::make_unique<T>();
            object->mRecordIndex = mFile.mRecords.size();
            auto& result = *object;
            mFile.mRecords.push_back(std::move(object));
            return result;
        }
        ActorRagdollTest()
        {
            mFile.mVersion = Nif::NIFFile::VER_OB;
            mFile.mUserVersion = 10;
            mFile.mHash = "synthetic-content-identity";
            auto& root = add<Nif::NiNode>();
            Nif::Testing::init(root);
            root.mName = "Bip01";
            root.mTransform.mTranslation = {10, 20, 30};
            mFile.mRoots = {&root};
            auto& first = node("Bip01 Pelvis", {1, 2, 3});
            auto& second = node("Bip01 Spine", {4, 5, 6});
            root.mChildren = {Nif::NiAVObjectPtr(&first), Nif::NiAVObjectPtr(&second)};
            auto& sphere = add<Nif::bhkSphereShape>();
            sphere.mRecordType = Nif::RC_bhkSphereShape;
            sphere.mRadius = 0.5f;
            auto& capsule = add<Nif::bhkCapsuleShape>();
            capsule.mRecordType = Nif::RC_bhkCapsuleShape;
            capsule.mPoint1 = {0, 0, -1};
            capsule.mPoint2 = {0, 0, 1};
            capsule.mRadius1 = 0.25f;
            capsule.mRadius2 = 0.3f;
            auto& bodyA = body(first, sphere, 40);
            auto& bodyB = body(second, capsule, 20);
            auto& joint = add<Nif::bhkLimitedHingeConstraint>();
            joint.mInfo.mEntityA = Nif::bhkEntityPtr(&bodyA);
            joint.mInfo.mEntityB = Nif::bhkEntityPtr(&bodyB);
            joint.mConstraint.mDataA.mPivot = {0, 0, 1, 0};
            joint.mConstraint.mDataB.mPivot = {0, 0, -1, 0};
            joint.mConstraint.mDataA.mAxis = joint.mConstraint.mDataB.mAxis = {1, 0, 0, 0};
            joint.mConstraint.mDataA.mPerpAxis1 = {0, 1, 0, 0};
            joint.mConstraint.mDataB.mPerpAxis2 = {0, 0, 1, 0};
            joint.mConstraint.mDataB.mPerpAxis1 = {std::numeric_limits<float>::quiet_NaN(), 0, 0, 0};
            joint.mConstraint.mMinAngle = -0.2f;
            joint.mConstraint.mMaxAngle = 1.2f;
            joint.mConstraint.mMaxFriction = 0.1f;
            bodyA.mConstraints = {Nif::RecordPtrT<Nif::bhkSerializable>(&joint)};
            bodyB.mConstraints = {};
        }
        Nif::bhkBlendCollisionObject& blendCollision()
        {
            for (auto& record : mFile.mRecords)
                if (auto* collision = dynamic_cast<Nif::bhkCollisionObject*>(record.get()))
                {
                    auto blend = std::make_unique<Nif::bhkBlendCollisionObject>();
                    blend->mRecordIndex = collision->mRecordIndex;
                    blend->mRecordType = Nif::RC_bhkBlendCollisionObject;
                    blend->mTarget = collision->mTarget;
                    blend->mBody = collision->mBody;
                    blend->mFlags = 0x123;
                    blend->mHeirGain = .125f;
                    blend->mVelGain = .75f;
                    auto& result = *blend;
                    blend->mTarget->mCollision = Nif::NiCollisionObjectPtr(blend.get());
                    record = std::move(blend);
                    return result;
                }
            throw std::runtime_error("missing fixture collision");
        }
        Nif::NiNode& node(std::string name, osg::Vec3f position)
        {
            auto& result = add<Nif::NiNode>();
            Nif::Testing::init(result);
            result.mName = std::move(name);
            result.mTransform.mTranslation = position;
            return result;
        }
        Nif::bhkRigidBody& body(Nif::NiNode& node, Nif::bhkShape& shape, float mass)
        {
            auto& result = add<Nif::bhkRigidBody>();
            result.mRecordType = Nif::RC_bhkRigidBody;
            result.mShape = Nif::bhkShapePtr(&shape);
            result.mInfo = {};
            result.mInfo.mMass = mass;
            result.mInfo.mRotation = osg::Quat();
            result.mInfo.mLinearDamping = 2;
            result.mInfo.mAngularDamping = 1;
            result.mInfo.mFriction = 0.6f;
            result.mInfo.mRestitution = 0.2f;
            result.mInfo.mMaxLinearVelocity = 100;
            result.mInfo.mMaxAngularVelocity = 50;
            auto& collision = add<Nif::bhkCollisionObject>();
            collision.mFlags = 1;
            collision.mBody = Nif::bhkWorldObjectPtr(&result);
            collision.mTarget = Nif::NiAVObjectPtr(&node);
            node.mCollision = Nif::NiCollisionObjectPtr(&collision);
            return result;
        }
    };

    TEST_F(ActorRagdollTest, OwnsBodyGraphAndBindPoseWithoutChangingNativeUnits)
    {
        auto graph = NifBullet::loadActorRagdollDefinition(mFile);
        ASSERT_EQ(graph.mBodies.size(), 2);
        ASSERT_EQ(graph.mJoints.size(), 1);
        EXPECT_EQ(graph.mSourceHash, "synthetic-content-identity");
        EXPECT_EQ(graph.mBodies[0].mBone, "Bip01 Pelvis");
        EXPECT_EQ(graph.mBodies[0].mBoneBind.getTrans(), osg::Vec3f(11, 22, 33));
        EXPECT_FLOAT_EQ(graph.mBodies[0].mMass, 40);
        EXPECT_FLOAT_EQ(graph.mBodies[0].mLinearDamping, 2);
        EXPECT_FLOAT_EQ(std::get<NifBullet::RagdollSphere>(graph.mBodies[0].mShape).mRadius, 0.5f);
        EXPECT_EQ(std::get<NifBullet::RagdollCapsule>(graph.mBodies[1].mShape).mPoint2, osg::Vec3f(0, 0, 1));
        EXPECT_EQ(graph.mJoints[0].mBodyA, 0);
        EXPECT_EQ(graph.mJoints[0].mBodyB, 1);
        EXPECT_FLOAT_EQ(std::get<NifBullet::RagdollHingeJoint>(graph.mJoints[0].mJoint).mMin, -0.2f);
        // Description ownership survives destruction of every parsed record.
        mFile.mRecords.clear();
        EXPECT_EQ(graph.mBodies[1].mBone, "Bip01 Spine");
        EXPECT_EQ(graph.mBodies[1].mBoneBind.getTrans(), osg::Vec3f(14, 25, 36));
    }

    TEST_F(ActorRagdollTest, OwnsAuthoredCollisionFlagsIndependentlyOfHavokFiltersAndReferenceRelocation)
    {
        auto& node = static_cast<Nif::NiNode&>(*mFile.mRecords[1]);
        auto& collision = static_cast<Nif::bhkCollisionObject&>(*node.mCollision.getPtr());
        auto& body = static_cast<Nif::bhkRigidBody&>(*collision.mBody.getPtr());
        body.mHavokFilter.mFlags = 0xab;
        body.mInfo.mHavokFilter.mFlags = 0xcd;
        for (std::uint16_t flags : {0, 1, 8, 0x123, 0xffff})
        {
            SCOPED_TRACE(flags);
            collision.mFlags = flags;
            const auto carried = NifBullet::loadActorRagdollDefinition(mFile);
            const auto placed = NifBullet::loadReferenceRagdollDefinition(mFile);
            EXPECT_EQ(carried.mBodies[0].mCollisionFlags, flags);
            EXPECT_EQ(placed.mBodies[0].mCollisionFlags, flags);
            EXPECT_EQ(placed.mBodies[0].mNodeRecord, 0u);
            EXPECT_EQ(placed.mBodies[0].mWorldObjectFilter.mFlags, 0xabu);
            EXPECT_EQ(placed.mBodies[0].mInfoFilter.mFlags, 0xcdu);
            const auto scaled = NifBullet::ragdollDefinitionWithNativeScaledProperties(placed, 2.f);
            EXPECT_EQ(scaled.mBodies[0].mCollisionFlags, flags);
            EXPECT_EQ(collision.mFlags, flags);
        }
    }
    TEST_F(ActorRagdollTest, ReferencePlacementReplacesRootAndRelocatesOnlySelectedCollision)
    {
        auto& root = static_cast<Nif::NiNode&>(*mFile.mRecords[0]);
        root.mTransform.mRotation.mValues[0][0] = 0;
        root.mTransform.mRotation.mValues[0][1] = -1;
        root.mTransform.mRotation.mValues[1][0] = 1;
        root.mTransform.mRotation.mValues[1][1] = 0;
        root.mTransform.mScale = 2;
        const auto authored = NifBullet::loadActorRagdollDefinition(mFile);
        const auto placed = NifBullet::loadReferenceRagdollDefinition(mFile);
        EXPECT_FALSE(authored.mReferenceRoot);
        ASSERT_EQ(placed.mReferenceRoot, 0u);
        ASSERT_EQ(placed.mBodies.size(), authored.mBodies.size());
        EXPECT_EQ(placed.mSourceHash, authored.mSourceHash);
        EXPECT_EQ(placed.mBodies[0].mNodeRecord, 0u);
        EXPECT_EQ(placed.mBodies[0].mBone, root.mName);
        EXPECT_TRUE(placed.mBodies[0].mUsesRigidBodyTransform);
        EXPECT_EQ(placed.mBodies[0].mBoneBind, osg::Matrixf::identity());
        EXPECT_EQ(placed.mBodies[1].mBoneBind.getTrans(), osg::Vec3f(4, 5, 6));
        EXPECT_NE(authored.mBodies[0].mBoneBind, placed.mBodies[0].mBoneBind);
        EXPECT_FLOAT_EQ(placed.mBodies[0].mMass, authored.mBodies[0].mMass);
        EXPECT_EQ(placed.mBodies[0].mInertia, authored.mBodies[0].mInertia);
        EXPECT_EQ(placed.mBodies[0].mTranslation,
            NifBullet::ragdollWorldToNativePosition({1, 2, 3}));
        EXPECT_EQ(placed.mBodies[0].mRotation, authored.mBodies[0].mRotation);
        EXPECT_EQ(std::get<NifBullet::RagdollHingeJoint>(placed.mJoints[0].mJoint).mA.mPivot,
            std::get<NifBullet::RagdollHingeJoint>(authored.mJoints[0].mJoint).mA.mPivot);
        EXPECT_EQ(root.mTransform.mTranslation, osg::Vec3f(10, 20, 30));
        EXPECT_FLOAT_EQ(root.mTransform.mScale, 2);
        EXPECT_EQ(NifBullet::loadActorRagdollDefinition(mFile).mBodies[0].mBoneBind,
            authored.mBodies[0].mBoneBind);
    }

    TEST_F(ActorRagdollTest, ReferenceRelocationUsesSecondLiteralChildInsteadOfDeepFirstBody)
    {
        auto& root = static_cast<Nif::NiNode&>(*mFile.mRecords[0]);
        const auto first = root.mChildren[0], second = root.mChildren[1];
        auto& helper = node("Nonphysical helper", {});
        helper.mChildren = {first};
        root.mChildren = {Nif::NiAVObjectPtr(&helper), second};
        const auto placed = NifBullet::loadReferenceRagdollDefinition(mFile);
        ASSERT_EQ(placed.mBodies.size(), 2u);
        EXPECT_EQ(placed.mBodies[0].mNodeRecord, first->mRecordIndex);
        EXPECT_EQ(placed.mBodies[1].mNodeRecord, root.mRecordIndex);
        EXPECT_TRUE(placed.mBodies[1].mUsesRigidBodyTransform);
        EXPECT_EQ(helper.mChildren[0].getPtr(), first.getPtr());
        EXPECT_FALSE(second->mCollision.empty()); // Parsed source stays authored.
        EXPECT_TRUE(root.mCollision.empty());
    }

    TEST_F(ActorRagdollTest, ReferenceRelocationDoesNotSkipNullFirstSlotOrSearchDescendants)
    {
        auto& root = static_cast<Nif::NiNode&>(*mFile.mRecords[0]);
        const auto children = root.mChildren;
        auto& helper = node("Nonphysical helper", {});
        helper.mChildren = children;
        for (bool nullFirst : {true, false})
        {
            SCOPED_TRACE(nullFirst);
            root.mChildren = {Nif::NiAVObjectPtr(&helper)};
            if (nullFirst) root.mChildren.insert(root.mChildren.begin(), Nif::NiAVObjectPtr(nullptr));
            const auto placed = NifBullet::loadReferenceRagdollDefinition(mFile);
            EXPECT_EQ(placed.mBodies[0].mNodeRecord, children[0]->mRecordIndex);
            EXPECT_EQ(placed.mBodies[1].mNodeRecord, children[1]->mRecordIndex);
            EXPECT_FALSE(placed.mBodies[0].mUsesRigidBodyTransform);
            EXPECT_FALSE(placed.mBodies[1].mUsesRigidBodyTransform);
        }
    }

    TEST_F(ActorRagdollTest, ReferenceRelocationPreservesFixedAndKeyframedChildTargets)
    {
        const auto authored = NifBullet::loadActorRagdollDefinition(mFile);
        auto& body = static_cast<Nif::bhkRigidBody&>(*mFile.mRecords[authored.mBodies[0].mRecord]);
        for (auto motion : {Nif::HkMotionType::Motion_Keyframed, Nif::HkMotionType::Motion_Fixed})
        {
            body.mInfo.mMotionType = motion;
            const auto placed = NifBullet::loadReferenceRagdollDefinition(mFile);
            EXPECT_EQ(placed.mBodies[0].mNodeRecord, authored.mBodies[0].mNodeRecord);
            EXPECT_FALSE(placed.mBodies[0].mUsesRigidBodyTransform);
            EXPECT_EQ(placed.mBodies[0].mTranslation, authored.mBodies[0].mTranslation);
        }
    }

    TEST_F(ActorRagdollTest, ReferenceRelocationAllowsAnUnnamedModelRoot)
    {
        auto& root = static_cast<Nif::NiNode&>(*mFile.mRecords[0]);
        root.mName.clear();
        const auto placed = NifBullet::loadReferenceRagdollDefinition(mFile);
        EXPECT_EQ(placed.mBodies[0].mNodeRecord, 0u);
        EXPECT_TRUE(placed.mBodies[0].mBone.empty());
        EXPECT_EQ(placed.mBodies[1].mBone, "Bip01 Spine");
    }

    TEST_F(ActorRagdollTest, ReferencePlacementRejectsMissingMultipleOrNonSceneRoots)
    {
        mFile.mRoots.push_back(mFile.mRoots.front());
        EXPECT_THROW(NifBullet::loadReferenceRagdollDefinition(mFile), std::runtime_error);
        mFile.mRoots.clear();
        EXPECT_THROW(NifBullet::loadReferenceRagdollDefinition(mFile), std::runtime_error);
        mFile.mRoots.push_back(mFile.mRecords.back().get());
        EXPECT_THROW(NifBullet::loadReferenceRagdollDefinition(mFile), std::runtime_error);
    }

    TEST_F(ActorRagdollTest, RetainsRigidBodyTransformRecordIdentityForPoseAdmission)
    {
        // Locate by the already owned graph rather than depending on fixtures'
        // record ordering beyond its explicit identity.
        const auto plain = NifBullet::loadActorRagdollDefinition(mFile);
        auto& body = static_cast<Nif::bhkRigidBody&>(*mFile.mRecords[plain.mBodies[0].mRecord]);
        EXPECT_FALSE(plain.mBodies[0].mUsesRigidBodyTransform);
        body.mRecordType = Nif::RC_bhkRigidBodyT;
        EXPECT_TRUE(NifBullet::loadActorRagdollDefinition(mFile).mBodies[0].mUsesRigidBodyTransform);
    }

    TEST_F(ActorRagdollTest, NativePropertyScaleUsesLinearMassAndStoredCubicInertia)
    {
        auto original = NifBullet::loadActorRagdollDefinition(mFile).mBodies.front();
        original.mCenter = {1, -2, 3};
        original.mInertia = {1, .25f, 0, .25f, 2, 0, 0, 0, 3};
        const auto scaled = NifBullet::ragdollBodyWithNativeScaledProperties(original, 2);
        EXPECT_EQ(scaled.mMass, 80);
        EXPECT_EQ(scaled.mCenter, osg::Vec3f(2, -4, 6));
        EXPECT_EQ(scaled.mInertia, (std::array<float, 9>{8, 2, 0, 2, 16, 0, 0, 0, 24}));
        EXPECT_EQ(std::get<NifBullet::RagdollSphere>(scaled.mShape).mRadius, 1);
        EXPECT_EQ(original.mMass, 40);
        EXPECT_EQ(original.mCenter, osg::Vec3f(1, -2, 3));
        EXPECT_EQ(scaled.mBoneBind, original.mBoneBind);
        EXPECT_EQ(scaled.mTranslation, original.mTranslation);
        EXPECT_EQ(scaled.mRotation, original.mRotation);
        EXPECT_EQ(scaled.mRecord, original.mRecord);
        EXPECT_EQ(scaled.mNodeRecord, original.mNodeRecord);
        EXPECT_EQ(scaled.mLinearDamping, original.mLinearDamping);
        EXPECT_EQ(scaled.mMaxLinearVelocity, original.mMaxLinearVelocity);
    }

    TEST_F(ActorRagdollTest, NativeCapsuleScaleRetainsDistinctEndpointRadii)
    {
        const auto original = NifBullet::loadActorRagdollDefinition(mFile).mBodies[1];
        const auto scaled = NifBullet::ragdollBodyWithNativeScaledProperties(original, .5f);
        const auto& capsule = std::get<NifBullet::RagdollCapsule>(scaled.mShape);
        EXPECT_EQ(capsule.mPoint1, osg::Vec3f(0, 0, -.5f));
        EXPECT_EQ(capsule.mPoint2, osg::Vec3f(0, 0, .5f));
        EXPECT_EQ(capsule.mRadius1, .125f);
        EXPECT_EQ(capsule.mRadius2, .15f);
        EXPECT_EQ(std::get<NifBullet::RagdollCapsule>(original.mShape).mRadius2, .3f);
    }

    TEST_F(ActorRagdollTest, NativeHullScalePreservesCollisionRadiusAndOwnsVertices)
    {
        auto original = NifBullet::loadActorRagdollDefinition(mFile).mBodies.front();
        original.mShape = NifBullet::RagdollHull{{{1, 0, 0}, {0, 2, 0}, {0, 0, 3}, {-1, -1, -1}}, .3f};
        auto scaled = NifBullet::ragdollBodyWithNativeScaledProperties(original, 2);
        auto& hull = std::get<NifBullet::RagdollHull>(scaled.mShape);
        EXPECT_EQ(hull.mRadius, .3f);
        EXPECT_EQ(hull.mVertices[1], osg::Vec3f(0, 4, 0));
        hull.mVertices[0] = {};
        EXPECT_EQ(std::get<NifBullet::RagdollHull>(original.mShape).mVertices[0], osg::Vec3f(1, 0, 0));
    }

    TEST_F(ActorRagdollTest, NativePropertyScaleRejectsInvalidAndUnrepresentableResultsAtomically)
    {
        auto original = NifBullet::loadActorRagdollDefinition(mFile).mBodies.front();
        for (float scale : {0.f, -1.f, std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::max()})
            EXPECT_THROW(NifBullet::ragdollBodyWithNativeScaledProperties(original, scale), std::runtime_error);
        original.mCenter[0] = std::numeric_limits<float>::max();
        EXPECT_THROW(NifBullet::ragdollBodyWithNativeScaledProperties(original, 2), std::runtime_error);
        EXPECT_EQ(original.mMass, 40);
        original.mCenter[0] = 0;
        original.mInertia[4] = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(NifBullet::ragdollBodyWithNativeScaledProperties(original, 2), std::runtime_error);
    }

    TEST_F(ActorRagdollTest, NativeGraphScaleChangesBodyTTranslationWithoutScalingOrdinaryTransforms)
    {
        auto source = NifBullet::loadActorRagdollDefinition(mFile);
        source.mBodies[0].mUsesRigidBodyTransform = true;
        source.mBodies[0].mTranslation = {1, -2, 3};
        source.mBodies[1].mTranslation = {4, -5, 6};
        const auto result = NifBullet::ragdollDefinitionWithNativeScaledProperties(source, 2);
        EXPECT_EQ(result.mBodies[0].mTranslation, osg::Vec3f(2, -4, 6));
        EXPECT_EQ(result.mBodies[1].mTranslation, source.mBodies[1].mTranslation);
        EXPECT_EQ(result.mBodies[0].mBoneBind, source.mBodies[0].mBoneBind);
        EXPECT_EQ(result.mBodies[0].mRotation, source.mBodies[0].mRotation);
        EXPECT_EQ(result.mSourceHash, source.mSourceHash);
        EXPECT_EQ(result.mBodies[1].mMass, 40);
        EXPECT_EQ(source.mBodies[0].mTranslation, osg::Vec3f(1, -2, 3));
    }

    TEST_F(ActorRagdollTest, NativeGraphScaleChangesConeAndMalleableHingePivotsOnly)
    {
        auto source = NifBullet::loadActorRagdollDefinition(mFile);
        source.mJoints[0].mMalleable = true;
        source.mJoints[0].mTau = .25f;
        source.mJoints[0].mDamping = .75f;
        NifBullet::RagdollConeJoint cone{{{1, -2, 3}, {1, 0, 0}, {0, 1, 0}},
            {{-4, 5, -6}, {0, 1, 0}, {1, 0, 0}}, .5f, -.3f, .7f, -.2f, .8f, .1f};
        source.mJoints.push_back({77, 0, 1, false, 1, 1, cone});
        const auto result = NifBullet::ragdollDefinitionWithNativeScaledProperties(source, 2);
        const auto& hinge = std::get<NifBullet::RagdollHingeJoint>(result.mJoints[0].mJoint);
        const auto& originalHinge = std::get<NifBullet::RagdollHingeJoint>(source.mJoints[0].mJoint);
        EXPECT_EQ(hinge.mA.mPivot, osg::Vec3f(0, 0, 2));
        EXPECT_EQ(hinge.mB.mPivot, osg::Vec3f(0, 0, -2));
        EXPECT_EQ(hinge.mA.mAxis, originalHinge.mA.mAxis);
        EXPECT_EQ(hinge.mB.mPlane, originalHinge.mB.mPlane);
        EXPECT_EQ(hinge.mMin, originalHinge.mMin);
        EXPECT_EQ(hinge.mFriction, originalHinge.mFriction);
        EXPECT_EQ(result.mJoints[0].mTau, .25f);
        EXPECT_EQ(result.mJoints[0].mDamping, .75f);
        EXPECT_TRUE(result.mJoints[0].mMalleable);
        const auto& scaledCone = std::get<NifBullet::RagdollConeJoint>(result.mJoints[1].mJoint);
        EXPECT_EQ(scaledCone.mA.mPivot, osg::Vec3f(2, -4, 6));
        EXPECT_EQ(scaledCone.mB.mPivot, osg::Vec3f(-8, 10, -12));
        EXPECT_EQ(scaledCone.mB.mAxis, cone.mB.mAxis);
        EXPECT_EQ(scaledCone.mA.mPlane, cone.mA.mPlane);
        EXPECT_EQ(scaledCone.mTwistMax, cone.mTwistMax);
        EXPECT_EQ(result.mJoints[1].mRecord, 77);
        EXPECT_EQ(result.mJoints[1].mBodyA, 0);
        EXPECT_EQ(result.mJoints[1].mBodyB, 1);
    }

    TEST_F(ActorRagdollTest, NativeGraphScaleCopiesAuthoredSourceAndRejectsLateInvalidPivotsAtomically)
    {
        auto source = NifBullet::loadActorRagdollDefinition(mFile);
        const auto first = NifBullet::ragdollDefinitionWithNativeScaledProperties(source, 2);
        const auto second = NifBullet::ragdollDefinitionWithNativeScaledProperties(source, 3);
        EXPECT_EQ(first.mBodies[0].mMass, 80);
        EXPECT_EQ(second.mBodies[0].mMass, 120);
        EXPECT_EQ(source.mBodies[0].mMass, 40);
        auto& hinge = std::get<NifBullet::RagdollHingeJoint>(source.mJoints[0].mJoint);
        hinge.mB.mPivot[0] = std::numeric_limits<float>::max();
        EXPECT_THROW(NifBullet::ragdollDefinitionWithNativeScaledProperties(source, 2), std::runtime_error);
        EXPECT_EQ(source.mBodies[0].mMass, 40);
        EXPECT_EQ(hinge.mA.mPivot, osg::Vec3f(0, 0, 1));
        hinge.mB.mPivot[0] = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(NifBullet::ragdollDefinitionWithNativeScaledProperties(source, 2), std::runtime_error);
    }

    TEST_F(ActorRagdollTest, RejectsUnsupportedFormat)
    {
        mFile.mVersion = Nif::NIFFile::VER_MW;
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    }

    TEST_F(ActorRagdollTest, RejectsCyclicAndSharedHierarchy)
    {
        auto& root = static_cast<Nif::NiNode&>(*mFile.mRecords[0]);
        root.mChildren.push_back(Nif::NiAVObjectPtr(&root));
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
        root.mChildren.back() = root.mChildren.front();
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    }

    TEST_F(ActorRagdollTest, RejectsReferenceOutsideFile)
    {
        Nif::NiNode external;
        Nif::Testing::init(external);
        static_cast<Nif::NiNode&>(*mFile.mRecords[0]).mChildren.push_back(Nif::NiAVObjectPtr(&external));
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    }

    TEST_F(ActorRagdollTest, RejectsAmbiguousBoneNames)
    {
        static_cast<Nif::NiNode&>(*mFile.mRecords[2]).mName = "Bip01 Pelvis";
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    }

    TEST_F(ActorRagdollTest, RejectsCollisionTargetDisagreementAndOrphanBody)
    {
        auto& collision = static_cast<Nif::bhkCollisionObject&>(*mFile.mRecords[6]);
        collision.mTarget = Nif::NiAVObjectPtr(static_cast<Nif::NiNode*>(mFile.mRecords[2].get()));
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
        collision.mTarget = Nif::NiAVObjectPtr(static_cast<Nif::NiNode*>(mFile.mRecords[1].get()));
        static_cast<Nif::NiNode&>(*mFile.mRecords[0]).mChildren.pop_back();
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    }

    TEST_F(ActorRagdollTest, RejectsNonfiniteBodyAndInvalidMass)
    {
        auto& body = static_cast<Nif::bhkRigidBody&>(*mFile.mRecords[5]);
        body.mInfo.mTranslation.x() = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
        body.mInfo.mTranslation.x() = 0;
        body.mInfo.mMass = 0;
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
        body.mInfo.mMass = -1;
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    }

    TEST_F(ActorRagdollTest, RejectsUnadmittedShapeAndInvalidRadius)
    {
        auto& body = static_cast<Nif::bhkRigidBody&>(*mFile.mRecords[5]);
        static_cast<Nif::bhkSphereShape&>(*mFile.mRecords[3]).mRadius = -0.5f;
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
        auto& box = add<Nif::bhkBoxShape>();
        box.mRecordType = Nif::RC_bhkBoxShape;
        body.mShape = Nif::bhkShapePtr(&box);
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    }

    TEST_F(ActorRagdollTest, DeduplicatesJointReferencesFromBothEndpoints)
    {
        auto& body = static_cast<Nif::bhkRigidBody&>(*mFile.mRecords[7]);
        body.mConstraints = {Nif::RecordPtrT<Nif::bhkSerializable>(
            static_cast<Nif::bhkLimitedHingeConstraint*>(mFile.mRecords[9].get()))};
        EXPECT_EQ(NifBullet::loadActorRagdollDefinition(mFile).mJoints.size(), 1);
    }

    TEST_F(ActorRagdollTest, RejectsSelfJointAndDegenerateFrame)
    {
        auto& joint = static_cast<Nif::bhkLimitedHingeConstraint&>(*mFile.mRecords[9]);
        joint.mInfo.mEntityB = joint.mInfo.mEntityA;
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
        joint.mInfo.mEntityB = Nif::bhkEntityPtr(static_cast<Nif::bhkRigidBody*>(mFile.mRecords[7].get()));
        joint.mConstraint.mDataA.mPerpAxis1 = joint.mConstraint.mDataA.mAxis;
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    }

    TEST_F(ActorRagdollTest, RejectsUnreferencedJointAndReversedLimits)
    {
        auto& body = static_cast<Nif::bhkRigidBody&>(*mFile.mRecords[5]);
        body.mConstraints.clear();
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
        body.mConstraints = {Nif::RecordPtrT<Nif::bhkSerializable>(
            static_cast<Nif::bhkLimitedHingeConstraint*>(mFile.mRecords[9].get()))};
        static_cast<Nif::bhkLimitedHingeConstraint&>(*mFile.mRecords[9]).mConstraint.mMaxAngle = -1;
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    }

    TEST_F(ActorRagdollTest, OwnsMalleableHingeAndIgnoresFieldsAbsentFromOblivion)
    {
        auto& original = static_cast<Nif::bhkLimitedHingeConstraint&>(*mFile.mRecords[9]);
        auto& wrapper = add<Nif::bhkMalleableConstraint>();
        wrapper.mInfo.mEntityA = original.mInfo.mEntityA;
        wrapper.mInfo.mEntityB = original.mInfo.mEntityB;
        wrapper.mConstraint.mInfo.mEntityA = original.mInfo.mEntityA;
        wrapper.mConstraint.mInfo.mEntityB = original.mInfo.mEntityB;
        wrapper.mConstraint.mType = Nif::HkConstraintType::LimitedHinge;
        wrapper.mConstraint.mLimitedHingeInfo = original.mConstraint;
        wrapper.mConstraint.mTau = 0.8f;
        wrapper.mConstraint.mDamping = 0.7f;
        wrapper.mConstraint.mStrength = std::numeric_limits<float>::quiet_NaN();
        auto& body = static_cast<Nif::bhkRigidBody&>(*mFile.mRecords[5]);
        body.mConstraints.push_back(Nif::RecordPtrT<Nif::bhkSerializable>(&wrapper));
        const auto graph = NifBullet::loadActorRagdollDefinition(mFile);
        ASSERT_EQ(graph.mJoints.size(), 2);
        EXPECT_TRUE(graph.mJoints[1].mMalleable);
        EXPECT_FLOAT_EQ(graph.mJoints[1].mTau, 0.8f);
        EXPECT_FLOAT_EQ(graph.mJoints[1].mDamping, 0.7f);
        EXPECT_EQ(std::get<NifBullet::RagdollHingeJoint>(graph.mJoints[1].mJoint).mB.mPlane, osg::Vec3f(0, 1, 0));
        wrapper.mConstraint.mInfo.mEntityA = Nif::bhkEntityPtr(nullptr);
        wrapper.mConstraint.mInfo.mEntityB = Nif::bhkEntityPtr(nullptr);
        EXPECT_EQ(NifBullet::loadActorRagdollDefinition(mFile).mJoints.size(), 2);
        wrapper.mConstraint.mInfo.mEntityB = original.mInfo.mEntityA;
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    }

    TEST_F(ActorRagdollTest, OwnsConeLimitsAndHullVertices)
    {
        auto& original = static_cast<Nif::bhkLimitedHingeConstraint&>(*mFile.mRecords[9]);
        auto& cone = add<Nif::bhkRagdollConstraint>();
        cone.mInfo.mEntityA = original.mInfo.mEntityA;
        cone.mInfo.mEntityB = original.mInfo.mEntityB;
        cone.mConstraint.mDataA.mPivot = {1, 2, 3, 0};
        cone.mConstraint.mDataB.mPivot = {-1, -2, -3, 0};
        cone.mConstraint.mDataA.mTwist = cone.mConstraint.mDataB.mTwist = {1, 0, 0, 0};
        cone.mConstraint.mDataA.mPlane = cone.mConstraint.mDataB.mPlane = {0, 1, 0, 0};
        cone.mConstraint.mConeMaxAngle = 0.6f;
        cone.mConstraint.mPlaneMinAngle = -0.3f;
        cone.mConstraint.mPlaneMaxAngle = 0.4f;
        cone.mConstraint.mTwistMinAngle = -0.1f;
        cone.mConstraint.mTwistMaxAngle = 0.2f;
        cone.mConstraint.mMaxFriction = 0.5f;
        auto& body = static_cast<Nif::bhkRigidBody&>(*mFile.mRecords[5]);
        body.mConstraints.push_back(Nif::RecordPtrT<Nif::bhkSerializable>(&cone));
        auto& hull = add<Nif::bhkConvexVerticesShape>();
        hull.mRecordType = Nif::RC_bhkConvexVerticesShape;
        hull.mRadius = 0.1f;
        hull.mVertices = {{0, 0, 0, 0}, {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
        body.mShape = Nif::bhkShapePtr(&hull);
        auto graph = NifBullet::loadActorRagdollDefinition(mFile);
        ASSERT_EQ(graph.mJoints.size(), 2);
        const auto& joint = std::get<NifBullet::RagdollConeJoint>(graph.mJoints[1].mJoint);
        EXPECT_EQ(joint.mA.mPivot, osg::Vec3f(1, 2, 3));
        EXPECT_FLOAT_EQ(joint.mConeAngle, 0.6f);
        EXPECT_FLOAT_EQ(joint.mTwistMin, -0.1f);
        const auto& shape = std::get<NifBullet::RagdollHull>(graph.mBodies[0].mShape);
        ASSERT_EQ(shape.mVertices.size(), 4);
        mFile.mRecords.clear();
        EXPECT_EQ(shape.mVertices[3], osg::Vec3f(0, 0, 1));
    }

    TEST_F(ActorRagdollTest, AdmitsStockOblivionVersionAndRejectsLaterStreamLayout)
    {
        mFile.mVersion = 0x14000004;
        mFile.mBethVersion = 11;
        EXPECT_EQ(NifBullet::loadActorRagdollDefinition(mFile).mBodies.size(), 2);
        mFile.mBethVersion = 17;
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    }

    TEST_F(ActorRagdollTest, ResolvesSerializedBodyAndMalleableReferences)
    {
        auto& original = static_cast<Nif::bhkLimitedHingeConstraint&>(*mFile.mRecords[9]);
        auto& wrapper = add<Nif::bhkMalleableConstraint>();
        Nif::Reader reader(mFile, nullptr);
        const std::uint32_t indices[] = {5, 7, 0xffffffffu, 0xffffffffu, 9};
        auto bytes = std::make_unique<std::istringstream>(
            std::string(reinterpret_cast<const char*>(indices), sizeof(indices)));
        Nif::NIFStream stream(reader, std::move(bytes), nullptr);
        wrapper.mInfo.mEntityA.read(&stream);
        wrapper.mInfo.mEntityB.read(&stream);
        wrapper.mConstraint.mInfo.mEntityA.read(&stream);
        wrapper.mConstraint.mInfo.mEntityB.read(&stream);
        wrapper.post(reader);
        EXPECT_EQ(wrapper.mInfo.mEntityA.getPtr(), original.mInfo.mEntityA.getPtr());
        EXPECT_EQ(wrapper.mInfo.mEntityB.getPtr(), original.mInfo.mEntityB.getPtr());
        EXPECT_TRUE(wrapper.mConstraint.mInfo.mEntityA.empty());
        EXPECT_TRUE(wrapper.mConstraint.mInfo.mEntityB.empty());
        auto& body = static_cast<Nif::bhkRigidBody&>(*mFile.mRecords[5]);
        body.mConstraints.clear();
        body.mConstraints.emplace_back();
        body.mConstraints[0].read(&stream);
        // bhkEntity::post resolves the shape as well, so supply an index-phase
        // shape reference just as the reader does before its post pass.
        const std::uint32_t shapeIndex = 3;
        auto shapeBytes = std::make_unique<std::istringstream>(
            std::string(reinterpret_cast<const char*>(&shapeIndex), sizeof(shapeIndex)));
        Nif::NIFStream shapeStream(reader, std::move(shapeBytes), nullptr);
        body.mShape = Nif::bhkShapePtr();
        body.mShape.read(&shapeStream);
        body.post(reader);
        EXPECT_EQ(body.mConstraints[0].getPtr(), &original);
        EXPECT_EQ(body.mShape.getPtr(), mFile.mRecords[3].get());
    }

    TEST_F(ActorRagdollTest, ReadsHavokBodyRotationInXYZWOrder)
    {
        Nif::Reader reader(mFile, nullptr);
        std::string bytes(196, '\0');
        const std::array<float, 4> rotation = {1, 2, 3, 4};
        std::memcpy(bytes.data() + 36, rotation.data(), sizeof(rotation));
        Nif::NIFStream stream(reader, std::make_unique<std::istringstream>(bytes), nullptr);
        Nif::bhkRigidBodyCInfo info;
        info.read(&stream);
        EXPECT_EQ(info.mRotation, osg::Quat(1, 2, 3, 4));
    }

}

TEST_F(ActorRagdollTest, RetainsIndependentWorldAndBodyCollisionFilters)
{
    for (auto& record : mFile.mRecords)
        if (auto* body = dynamic_cast<Nif::bhkRigidBody*>(record.get()))
        {
            body->mHavokFilter = { 8, 0x42, 65535 };
            body->mInfo.mHavokFilter = { 29, 0x83, 123 };
        }
    const auto graph = NifBullet::loadActorRagdollDefinition(Nif::FileView(mFile));
    ASSERT_EQ(graph.mBodies.size(), 2u);
    for (const auto& body : graph.mBodies)
    {
        EXPECT_EQ(body.mWorldObjectFilter.mLayer, 8);
        EXPECT_EQ(body.mWorldObjectFilter.mFlags, 0x42);
        EXPECT_EQ(body.mWorldObjectFilter.mGroup, 65535);
        EXPECT_EQ(body.mInfoFilter.mLayer, 29);
        EXPECT_EQ(body.mInfoFilter.mFlags, 0x83);
        EXPECT_EQ(body.mInfoFilter.mGroup, 123);
    }
}

TEST(ActorRagdollCollisionFilter, RejectsUnverifiedLayersEvenOnWildcardOrDisabledPaths)
{
    for (unsigned layer = 32; layer < 64; ++layer)
    {
        EXPECT_THROW(NifBullet::InitialRagdollCollisionFilter.enabled(layer, 0), std::invalid_argument);
        EXPECT_THROW(NifBullet::InitialRagdollCollisionFilter.enabled(0x4000, layer), std::invalid_argument);
    }
}

TEST(ActorRagdollCollisionFilter, CallerMaskChangesAndOrderedBranchesAreRespected)
{
    auto filter = NifBullet::InitialRagdollCollisionFilter;
    EXPECT_TRUE(filter.enabled(8, 8)); // Zero system group is a wildcard.
    EXPECT_FALSE(filter.enabled(8 | 0x4000, 8));
    EXPECT_TRUE(filter.enabled(29, 8 | 0x4000)); // Native exception is ordered.
    EXPECT_FALSE(filter.enabled(8 | 0x4000, 29));
    const std::uint32_t first = 8 | (2u << 8) | (1u << 16);
    const std::uint32_t second = 8 | (6u << 8) | (1u << 16);
    ASSERT_TRUE(filter.enabled(first, second));
    filter.mBoneMasks[2] = 0;
    EXPECT_FALSE(filter.enabled(first, second));
    EXPECT_FALSE(filter.enabled(first | 0x8000, second | 0x8000));
    filter.mLayerMasks[8] = 0;
    EXPECT_FALSE(filter.enabled(first, second + (1u << 16)));
    EXPECT_TRUE(filter.enabled(first & 0xffff, second));
}

TEST_F(ActorRagdollTest, RetainsAuthoredBlendIdentityFlagsAndIndependentGains)
{
    auto& blend = blendCollision();
    const auto graph = NifBullet::loadActorRagdollDefinition(mFile);
    ASSERT_TRUE(graph.mBodies[0].mBlend);
    EXPECT_EQ(graph.mBodies[0].mBlend->mRecord, blend.mRecordIndex);
    EXPECT_EQ(graph.mBodies[0].mBlend->mFlags, 0x123);
    EXPECT_FLOAT_EQ(graph.mBodies[0].mBlend->mHierarchyGain, .125f);
    EXPECT_FLOAT_EQ(graph.mBodies[0].mBlend->mVelocityGain, .75f);
    EXPECT_FALSE(graph.mBodies[1].mBlend);
    blend.mHeirGain = 1;
    EXPECT_FLOAT_EQ(graph.mBodies[0].mBlend->mHierarchyGain, .125f);
}

TEST_F(ActorRagdollTest, RejectsNonfiniteAuthoredBlendGainsWithoutInventingFiniteBounds)
{
    auto& blend = blendCollision();
    blend.mHeirGain = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    blend.mHeirGain = -.5f;
    blend.mVelGain = std::numeric_limits<float>::infinity();
    EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    blend.mVelGain = -.25f;
    const auto graph = NifBullet::loadActorRagdollDefinition(mFile);
    EXPECT_FLOAT_EQ(graph.mBodies[0].mBlend->mHierarchyGain, -.5f);
    EXPECT_FLOAT_EQ(graph.mBodies[0].mBlend->mVelocityGain, -.25f);
}


namespace
{
    struct ActorRootBlendTest : ActorRagdollTest
    {
        std::array<Nif::NiNode*, 9> mNodes;
        ActorRootBlendTest()
        {
            mFile.mRoots.clear();
            mFile.mRecords.clear();
            for (unsigned i = 0; i < mNodes.size(); ++i)
                mNodes[i] = &node("root lookup " + std::to_string(i), {});
            children(0, {1});
            children(1, {2, 3});
            children(2, {4, 5});
            children(3, {6, 7});
            children(6, {8});
            mFile.mRoots = {mNodes[0]};
            auto& shape = add<Nif::bhkSphereShape>();
            shape.mRecordType = Nif::RC_bhkSphereShape;
            shape.mRadius = .5f;
            for (auto* n : mNodes)
                body(*n, shape, 2);
        }
        void children(unsigned parent, std::initializer_list<unsigned> ids)
        {
            auto& values = mNodes[parent]->mChildren;
            values.clear();
            for (auto i : ids)
                values.push_back(Nif::NiAVObjectPtr(mNodes[i]));
        }
        Nif::bhkBlendCollisionObject& blend(unsigned i)
        {
            auto* collision = static_cast<Nif::bhkCollisionObject*>(mNodes[i]->mCollision.getPtr());
            auto value = std::make_unique<Nif::bhkBlendCollisionObject>();
            value->mRecordIndex = collision->mRecordIndex;
            value->mRecordType = Nif::RC_bhkBlendCollisionObject;
            value->mTarget = collision->mTarget;
            value->mBody = collision->mBody;
            value->mFlags = 0x123;
            value->mHeirGain = .125f;
            value->mVelGain = .75f;
            auto& result = *value;
            mNodes[i]->mCollision = Nif::NiCollisionObjectPtr(value.get());
            mFile.mRecords[value->mRecordIndex] = std::move(value);
            return result;
        }
        auto lookup(std::optional<std::uint32_t> root = 0)
        {
            return NifBullet::loadActorRagdollRootBlend(mFile, root);
        }
        void expectNode(unsigned i)
        {
            const auto result = lookup();
            ASSERT_TRUE(result);
            EXPECT_EQ(result->mNodeRecord, mNodes[i]->mRecordIndex);
            EXPECT_EQ(result->mBodyRecord,
                static_cast<Nif::bhkCollisionObject*>(mNodes[i]->mCollision.getPtr())->mBody->mRecordIndex);
            EXPECT_EQ(result->mBlend.mRecord, mNodes[i]->mCollision->mRecordIndex);
        }
    };

    TEST_F(ActorRootBlendTest, PrioritizesRootFirstChildAndNativeSelectedBranch)
    {
        for (auto i : {0, 1, 2, 3, 6, 7})
            blend(i);
        expectNode(0);
        mNodes[0]->mCollision = Nif::NiCollisionObjectPtr(nullptr);
        expectNode(1);
        mNodes[1]->mCollision = Nif::NiCollisionObjectPtr(nullptr);
        expectNode(3); // Two nonnull slots select slot1, despite slot0's blend.
        mNodes[3]->mCollision = Nif::NiCollisionObjectPtr(nullptr);
        expectNode(6);
        mNodes[6]->mCollision = Nif::NiCollisionObjectPtr(nullptr);
        expectNode(7);
        mNodes[7]->mCollision = Nif::NiCollisionObjectPtr(nullptr);
        EXPECT_FALSE(lookup()); // No search of the unselected branch2.
    }

    TEST_F(ActorRootBlendTest, CountsNonnullChildrenWithoutCompactingSlots)
    {
        blend(2);
        blend(3);
        mNodes[1]->mChildren[1] = Nif::NiAVObjectPtr(nullptr);
        expectNode(2);
        mNodes[1]->mChildren[0] = Nif::NiAVObjectPtr(nullptr);
        mNodes[1]->mChildren[1] = Nif::NiAVObjectPtr(mNodes[3]);
        EXPECT_FALSE(lookup()); // Effective count1 selects the empty slot0.
        children(1, {2, 3});
        expectNode(3);
        children(1, {2});
        expectNode(2);
    }

    TEST_F(ActorRootBlendTest, StopsAfterSelectedNodesDirectChildren)
    {
        blend(6);
        blend(8);
        expectNode(6);
        mNodes[6]->mCollision = Nif::NiCollisionObjectPtr(nullptr);
        EXPECT_FALSE(lookup()); // Blend8 is a grandchild of the selected node.
        blend(2);
        EXPECT_FALSE(lookup()); // A DFS or first-body search would pick a blend.
    }

    TEST_F(ActorRootBlendTest, OwnsSelectedMetadataAndDoesNotFilterByGain)
    {
        auto& value = blend(3);
        value.mHeirGain = -.5f;
        value.mVelGain = -.25f;
        const auto result = lookup();
        ASSERT_TRUE(result);
        EXPECT_EQ(result->mSourceHash, mFile.mHash);
        EXPECT_EQ(result->mNodeRecord, 3);
        EXPECT_EQ(result->mBlend.mFlags, 0x123);
        EXPECT_FLOAT_EQ(result->mBlend.mHierarchyGain, -.5f);
        EXPECT_FLOAT_EQ(result->mBlend.mVelocityGain, -.25f);
        value.mHeirGain = 0;
        EXPECT_TRUE(lookup());
        mFile.mRoots.clear();
        mFile.mRecords.clear();
        EXPECT_FLOAT_EQ(result->mBlend.mHierarchyGain, -.5f);
        EXPECT_FLOAT_EQ(result->mBlend.mVelocityGain, -.25f);
    }

    TEST_F(ActorRootBlendTest, UsesExplicitRootAndAdmitsMissingBlend)
    {
        blend(3);
        expectNode(3);
        EXPECT_FALSE(lookup(std::nullopt));
        mNodes[3]->mCollision = Nif::NiCollisionObjectPtr(nullptr);
        EXPECT_FALSE(lookup());
        blend(8);
        const auto nested = lookup(mNodes[6]->mRecordIndex);
        ASSERT_TRUE(nested);
        EXPECT_EQ(nested->mNodeRecord, 8);
        mFile.mRoots = {mNodes[2]};
        EXPECT_FALSE(lookup()); // The caller's selected root remains node0.
    }

    TEST_F(ActorRootBlendTest, RejectsMalformedLookupPathAndForeignIdentity)
    {
        EXPECT_THROW(lookup(999), std::runtime_error);
        mNodes[0]->mChildren.clear();
        EXPECT_THROW(lookup(), std::runtime_error);
        children(0, {1});
        mNodes[0]->mChildren[0] = Nif::NiAVObjectPtr(nullptr);
        EXPECT_THROW(lookup(), std::runtime_error);
        children(0, {1});
        Nif::NiNode foreign;
        Nif::Testing::init(foreign);
        foreign.mRecordIndex = 3;
        mNodes[1]->mChildren[1] = Nif::NiAVObjectPtr(&foreign);
        EXPECT_THROW(lookup(), std::runtime_error);
        children(1, {2, 3});
        mNodes[3]->mChildren[0] = Nif::NiAVObjectPtr(nullptr);
        EXPECT_THROW(lookup(), std::runtime_error);
        children(3, {6, 7});
        auto& selected = blend(3);
        selected.mTarget = Nif::NiAVObjectPtr(mNodes[2]);
        EXPECT_THROW(lookup(), std::runtime_error);
        selected.mTarget = Nif::NiAVObjectPtr(mNodes[3]);
        selected.mHeirGain = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(lookup(), std::runtime_error);
        selected.mHeirGain = .125f;
        selected.mBody = Nif::bhkWorldObjectPtr(nullptr);
        EXPECT_THROW(lookup(), std::runtime_error);
        mFile.mVersion = Nif::NIFFile::VER_MW;
        EXPECT_THROW(lookup(), std::runtime_error);
    }
}

TEST_F(ActorRagdollTest, OwnsFirstAuthoredNodeBlendControllerAndTimingInputs)
{
    auto& blend = blendCollision();
    auto& prefix = add<Nif::NiTimeController>();
    auto& first = add<Nif::bhkBlendController>();
    auto& second = add<Nif::bhkBlendController>();
    first.mRecordType = second.mRecordType = Nif::RC_bhkBlendController;
    first.mFlags = 0xc5;
    first.mFrequency = .75f;
    first.mPhase = -.5f;
    first.mTimeStart = -0.f;
    first.mTimeStop = .25f;
    first.mTarget = Nif::NiObjectNETPtr(blend.mTarget.getPtr());
    prefix.mNext = Nif::NiTimeControllerPtr(&first);
    first.mNext = Nif::NiTimeControllerPtr(&second);
    second.mFlags = 0xffff;
    blend.mTarget->mController = Nif::NiTimeControllerPtr(&prefix);
    const auto id = first.mRecordIndex;
    const auto target = blend.mTarget->mRecordIndex;
    const auto graph = NifBullet::loadActorRagdollDefinition(mFile);
    ASSERT_TRUE(graph.mBodies[0].mBlendController);
    EXPECT_EQ(graph.mBodies[0].mBlendController->mRecord, id);
    EXPECT_EQ(graph.mBodies[0].mBlendController->mTargetRecord, target);
    EXPECT_EQ(graph.mBodies[0].mBlendController->mFlags, 0xc5);
    EXPECT_FLOAT_EQ(graph.mBodies[0].mBlendController->mFrequency, .75f);
    EXPECT_FLOAT_EQ(graph.mBodies[0].mBlendController->mPhase, -.5f);
    EXPECT_TRUE(std::signbit(graph.mBodies[0].mBlendController->mStartTime));
    EXPECT_FLOAT_EQ(graph.mBodies[0].mBlendController->mStopTime, .25f);
    EXPECT_FALSE(graph.mBodies[1].mBlendController);
    mFile.mRecords.clear();
    EXPECT_EQ(graph.mBodies[0].mBlendController->mRecord, id);
    EXPECT_EQ(graph.mBodies[0].mBlendController->mTargetRecord, target);
}

TEST_F(ActorRagdollTest, DistinguishesMissingControllerAndNullTargetWithoutInventingOne)
{
    auto& blend = blendCollision();
    auto graph = NifBullet::loadActorRagdollDefinition(mFile);
    EXPECT_FALSE(graph.mBodies[0].mBlendController);
    auto& controller = add<Nif::bhkBlendController>();
    controller.mRecordType = Nif::RC_bhkBlendController;
    controller.mFlags = 0;
    controller.mFrequency = 1;
    controller.mPhase = 0;
    controller.mTimeStart = -1;
    controller.mTimeStop = 1;
    controller.mTarget = Nif::NiObjectNETPtr(nullptr);
    controller.mNext = Nif::NiTimeControllerPtr(nullptr);
    blend.mTarget->mController = Nif::NiTimeControllerPtr(&controller);
    graph = NifBullet::loadActorRagdollDefinition(mFile);
    ASSERT_TRUE(graph.mBodies[0].mBlendController);
    EXPECT_FALSE(graph.mBodies[0].mBlendController->mTargetRecord);
}

TEST_F(ActorRagdollTest, RejectsForeignAndCyclicControllerLookupBeforeProducingGraph)
{
    auto& blend = blendCollision();
    Nif::NiTimeController foreign{};
    blend.mTarget->mController = Nif::NiTimeControllerPtr(&foreign);
    EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    auto& prefix = add<Nif::NiTimeController>();
    prefix.mNext = Nif::NiTimeControllerPtr(&prefix);
    blend.mTarget->mController = Nif::NiTimeControllerPtr(&prefix);
    EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
}

namespace
{
    std::string blendControllerBytes(std::uint32_t count, std::span<const std::array<float, 3>> keys)
    {
        std::string bytes;
        const auto append = [&](const auto& value) {
            bytes.append(reinterpret_cast<const char*>(&value), sizeof(value));
        };
        append(std::int32_t{-1}); append(std::uint16_t{0xc5});
        for (float value : {.75f, -.5f, 7.f, 8.f}) append(value);
        append(std::int32_t{-1}); append(count);
        for (const auto& key : keys) for (float value : key) append(value);
        return bytes;
    }
}

TEST_F(ActorRagdollTest, ReadsAuthoredBlendKeysAndPreservesTimeControllerFields)
{
    const std::array<std::array<float, 3>, 2> keys{{{0, 1, 1}, {.25f, 0, 0}}};
    Nif::Reader reader(mFile, nullptr);
    Nif::NIFStream stream(reader, std::make_unique<std::istringstream>(blendControllerBytes(2, keys)), nullptr);
    Nif::bhkBlendController controller;
    controller.read(&stream); controller.post(reader);
    ASSERT_EQ(controller.mKeys.size(), 2);
    EXPECT_FLOAT_EQ(controller.mKeys[0].mTime, 0);
    EXPECT_FLOAT_EQ(controller.mKeys[0].mHierarchyGain, 1);
    EXPECT_FLOAT_EQ(controller.mKeys[0].mVelocityGain, 1);
    EXPECT_FLOAT_EQ(controller.mKeys[1].mTime, .25f);
    EXPECT_FLOAT_EQ(controller.mKeys[1].mHierarchyGain, 0);
    EXPECT_FLOAT_EQ(controller.mKeys[1].mVelocityGain, 0);
    EXPECT_EQ(controller.mFlags, 0xc5);
    EXPECT_FLOAT_EQ(controller.mFrequency, .75f); EXPECT_FLOAT_EQ(controller.mPhase, -.5f);
    EXPECT_FLOAT_EQ(controller.mTimeStart, 7); EXPECT_FLOAT_EQ(controller.mTimeStop, 8);
    EXPECT_TRUE(controller.mNext.empty()); EXPECT_TRUE(controller.mTarget.empty());
}

TEST_F(ActorRagdollTest, KeepsBlendKeySerializedOrderDuplicatesAndSignedZeros)
{
    const std::array<std::array<float, 3>, 4> keys{{{1, -.5f, 2}, {-1, 100, 0},
        {0, -0.f, 0}, {0, 0, -0.f}}};
    Nif::Reader reader(mFile, nullptr);
    Nif::NIFStream stream(reader, std::make_unique<std::istringstream>(blendControllerBytes(4, keys)), nullptr);
    Nif::bhkBlendController controller; controller.read(&stream);
    ASSERT_EQ(controller.mKeys.size(), keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        EXPECT_EQ(std::bit_cast<std::uint32_t>(controller.mKeys[i].mTime), std::bit_cast<std::uint32_t>(keys[i][0]));
        EXPECT_EQ(std::bit_cast<std::uint32_t>(controller.mKeys[i].mHierarchyGain), std::bit_cast<std::uint32_t>(keys[i][1]));
        EXPECT_EQ(std::bit_cast<std::uint32_t>(controller.mKeys[i].mVelocityGain), std::bit_cast<std::uint32_t>(keys[i][2]));
    }
}

TEST_F(ActorRagdollTest, RejectsTruncatedAndOversizedBlendKeyPayloads)
{
    const std::array<std::array<float, 3>, 1> keys{{{0, 1, 1}}};
    for (std::uint32_t count : {2u, std::numeric_limits<std::uint32_t>::max()})
    {
        Nif::Reader reader(mFile, nullptr);
        Nif::NIFStream stream(reader, std::make_unique<std::istringstream>(blendControllerBytes(count, keys)), nullptr);
        Nif::bhkBlendController controller;
        controller.mKeys = {{2, 3, 4}};
        EXPECT_THROW(controller.read(&stream), std::runtime_error);
        ASSERT_EQ(controller.mKeys.size(), 1);
        EXPECT_FLOAT_EQ(controller.mKeys[0].mTime, 2);
    }
}

TEST_F(ActorRagdollTest, OwnsParsedBlendKeysAfterSourceRecordsDisappear)
{
    auto& blend = blendCollision(); auto& controller = add<Nif::bhkBlendController>();
    controller.mRecordType = Nif::RC_bhkBlendController;
    const std::array<std::array<float, 3>, 2> keys{{{0, -0.f, 1}, {.25f, .5f, 0}}};
    Nif::Reader reader(mFile, nullptr);
    Nif::NIFStream stream(reader, std::make_unique<std::istringstream>(blendControllerBytes(2, keys)), nullptr);
    controller.read(&stream); controller.post(reader);
    blend.mTarget->mController = Nif::NiTimeControllerPtr(&controller);
    const auto graph = NifBullet::loadActorRagdollDefinition(mFile);
    ASSERT_TRUE(graph.mBodies[0].mBlendController);
    const auto& owned = graph.mBodies[0].mBlendController->mKeys;
    ASSERT_EQ(owned.size(), 2);
    mFile.mRecords.clear();
    EXPECT_FLOAT_EQ(owned[1].mTime, .25f); EXPECT_FLOAT_EQ(owned[1].mHierarchyGain, .5f);
    EXPECT_FLOAT_EQ(owned[1].mVelocityGain, 0); EXPECT_TRUE(std::signbit(owned[0].mHierarchyGain));
}

TEST_F(ActorRagdollTest, ZeroBlendKeyCountClearsPreviouslyReadKeys)
{
    Nif::Reader reader(mFile, nullptr);
    Nif::NIFStream stream(reader, std::make_unique<std::istringstream>(blendControllerBytes(0, {})), nullptr);
    Nif::bhkBlendController controller; controller.mKeys = {{1, 2, 3}};
    controller.read(&stream);
    EXPECT_TRUE(controller.mKeys.empty());
    EXPECT_FLOAT_EQ(controller.mTimeStart, 7); EXPECT_FLOAT_EQ(controller.mTimeStop, 8);
}

TEST_F(ActorRagdollTest, KeepsUnverifiedLaterNonemptyBlendKeyLayoutsUnsupported)
{
    mFile.mVersion = Nif::NIFStream::generateVersion(20, 2, 0, 7);
    const std::array<std::array<float, 3>, 1> keys{{{0, 1, 1}}};
    Nif::Reader reader(mFile, nullptr);
    Nif::NIFStream stream(reader, std::make_unique<std::istringstream>(blendControllerBytes(1, keys)), nullptr);
    Nif::bhkBlendController controller;
    EXPECT_THROW(controller.read(&stream), std::runtime_error);
}

TEST_F(ActorRagdollTest, OwnsNestedWeaponListsTransformsBoxesAndFiltersAfterFileDestruction)
{
    auto original = NifBullet::loadActorRagdollDefinition(mFile);
    auto& source = static_cast<Nif::bhkRigidBody&>(*mFile.mRecords[original.mBodies[0].mRecord]);
    auto& box = add<Nif::bhkBoxShape>(); box.mRecordType = Nif::RC_bhkBoxShape;
    box.mExtents = {.5f, 1.f, 2.f}; box.mRadius = .125f;
    auto& transformed = add<Nif::bhkConvexTransformShape>(); transformed.mRecordType = Nif::RC_bhkConvexTransformShape;
    transformed.mShape = Nif::bhkShapePtr(&box); transformed.mRadius = .125f;
    transformed.mTransform = osg::Matrixf::rotate(1.5707963267948966, osg::Vec3f(0,0,1));
    transformed.mTransform.setTrans(osg::Vec3f(2,3,4));
    auto& list = add<Nif::bhkListShape>(); list.mRecordType = Nif::RC_bhkListShape;
    list.mSubshapes = {source.mShape, Nif::bhkShapePtr(&transformed), Nif::bhkShapePtr(&box)};
    list.mHavokFilters = {{1,2,3},{4,5,6},{7,8,9}};
    source.mShape = Nif::bhkShapePtr(&list);
    auto graph = NifBullet::loadActorRagdollDefinition(mFile);
    mFile.mRecords.clear();
    auto& owned = std::get<NifBullet::RagdollList>(graph.mBodies[0].mShape);
    ASSERT_EQ(owned.mChildren.size(), 3u); ASSERT_EQ(owned.mFilters.size(), 3u);
    EXPECT_EQ(owned.mFilters[1].mLayer, 4); EXPECT_EQ(owned.mFilters[1].mFlags, 5); EXPECT_EQ(owned.mFilters[1].mGroup, 6);
    const auto& transform = std::get<NifBullet::RagdollTransform>(owned.mChildren[1]);
    ASSERT_EQ(transform.mChildren.size(), 1u); EXPECT_EQ(transform.mTransform.getTrans(), osg::Vec3f(2,3,4));
    EXPECT_EQ(std::get<NifBullet::RagdollBox>(transform.mChildren[0]).mHalfExtents, osg::Vec3f(.5f,1,2));
    // Shared source shapes are independently owned values, preserving instances.
    std::get<NifBullet::RagdollBox>(owned.mChildren[2]).mHalfExtents.x() = 5;
    EXPECT_EQ(std::get<NifBullet::RagdollBox>(transform.mChildren[0]).mHalfExtents.x(), .5f);
}

TEST_F(ActorRagdollTest, CompoundWeaponShapesRejectCyclesForeignNullEmptyAndMismatchedFilters)
{
    const auto graph = NifBullet::loadActorRagdollDefinition(mFile);
    auto& body = static_cast<Nif::bhkRigidBody&>(*mFile.mRecords[graph.mBodies[0].mRecord]);
    const auto original = body.mShape;
    Nif::bhkSphereShape foreign; foreign.mRecordType = Nif::RC_bhkSphereShape; foreign.mRadius = 1;
    for (int change = 0; change < 5; ++change)
    {
        SCOPED_TRACE(change);
        auto& list = add<Nif::bhkListShape>(); list.mRecordType = Nif::RC_bhkListShape;
        list.mSubshapes = {original};
        if (change == 0) list.mSubshapes.push_back(Nif::bhkShapePtr(&list));
        if (change == 1) list.mSubshapes.push_back(Nif::bhkShapePtr(nullptr));
        if (change == 2) list.mSubshapes.clear();
        if (change == 3) list.mHavokFilters.resize(2);
        if (change == 4) list.mSubshapes = {Nif::bhkShapePtr(&foreign)};
        body.mShape = Nif::bhkShapePtr(&list);
        EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
        EXPECT_EQ(body.mShape.getPtr(), &list);
        body.mShape = original;
        EXPECT_NO_THROW(NifBullet::loadActorRagdollDefinition(mFile));
    }
}

TEST_F(ActorRagdollTest, CompoundWeaponShapesBoundHierarchyDepthAndExpandedInstances)
{
    const auto graph = NifBullet::loadActorRagdollDefinition(mFile);
    auto& body = static_cast<Nif::bhkRigidBody&>(*mFile.mRecords[graph.mBodies[0].mRecord]);
    const auto original = body.mShape;
    for (unsigned i = 0; i < 129; ++i)
    {
        auto& list = add<Nif::bhkListShape>(); list.mRecordType = Nif::RC_bhkListShape;
        list.mSubshapes = {body.mShape}; body.mShape = Nif::bhkShapePtr(&list);
    }
    EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    auto& wide = add<Nif::bhkListShape>(); wide.mRecordType = Nif::RC_bhkListShape;
    wide.mSubshapes.resize(4096, original); body.mShape = Nif::bhkShapePtr(&wide);
    EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
}

TEST_F(ActorRagdollTest, NativeScaleOwnsNestedBoxExtentsAndTranslationWithoutScalingRadiusOrRotation)
{
    auto body = NifBullet::loadActorRagdollDefinition(mFile).mBodies[0];
    auto matrix = osg::Matrixf::rotate(1.5707963267948966, osg::Vec3f(0,0,1)); matrix.setTrans(osg::Vec3f(2,3,4));
    body.mShape = NifBullet::RagdollList{{NifBullet::RagdollTransform{matrix,.125f,
        {NifBullet::RagdollBox{{.5f,1,2},.0625f}}}},{{1,2,3}}};
    const auto scaled = NifBullet::ragdollBodyWithNativeScaledProperties(body, .5f);
    const auto& list = std::get<NifBullet::RagdollList>(scaled.mShape);
    const auto& transform = std::get<NifBullet::RagdollTransform>(list.mChildren[0]);
    EXPECT_EQ(transform.mTransform.getTrans(), osg::Vec3f(1,1.5f,2));
    for (unsigned row = 0; row < 3; ++row) for (unsigned col = 0; col < 3; ++col)
        EXPECT_EQ(transform.mTransform(row,col), matrix(row,col));
    const auto& box = std::get<NifBullet::RagdollBox>(transform.mChildren[0]);
    EXPECT_EQ(box.mHalfExtents, osg::Vec3f(.25f,.5f,1)); EXPECT_EQ(box.mRadius, .0625f);
    EXPECT_EQ(transform.mRadius, .125f); EXPECT_EQ(list.mFilters[0].mGroup, 3);
    const auto& source = std::get<NifBullet::RagdollTransform>(std::get<NifBullet::RagdollList>(body.mShape).mChildren[0]);
    EXPECT_EQ(source.mTransform.getTrans(), osg::Vec3f(2,3,4));
}

TEST_F(ActorRagdollTest, CompoundWeaponShapesRejectMalformedBoxesTransformsAndScalingWithoutMutation)
{
    auto body = NifBullet::loadActorRagdollDefinition(mFile).mBodies[0];
    body.mShape = NifBullet::RagdollTransform{osg::Matrixf::identity(),.1f,
        {NifBullet::RagdollBox{{1,2,3},.125f}}};
    auto& transform = std::get<NifBullet::RagdollTransform>(body.mShape);
    transform.mTransform(3,0) = std::numeric_limits<float>::max();
    EXPECT_THROW(NifBullet::ragdollBodyWithNativeScaledProperties(body, 2), std::runtime_error);
    EXPECT_EQ(transform.mTransform(3,0), std::numeric_limits<float>::max());
    transform.mChildren.clear();
    EXPECT_THROW(NifBullet::ragdollBodyWithNativeScaledProperties(body, 1), std::runtime_error);
    body.mShape = NifBullet::RagdollBox{{-1,2,3},.125f};
    EXPECT_THROW(NifBullet::ragdollBodyWithNativeScaledProperties(body, 1), std::runtime_error);
}

namespace
{
    struct NativeCompoundScaleCase
    {
        bool mTransform;
        std::uint32_t mScale;
        std::array<std::uint32_t, 3> mInput, mOutput;
        std::uint32_t mRadius;
    };
#include "native_compound_shape_scale_expected.inc"
}

TEST_F(ActorRagdollTest, CompoundWeaponScalingMatchesOriginalStoredBits)
{
    auto body = NifBullet::loadActorRagdollDefinition(mFile).mBodies[0]; body.mMass = 1;
    for (const auto& row : NativeCompoundScaleCases)
    {
        SCOPED_TRACE(row.mScale);
        SCOPED_TRACE(row.mTransform);
        const auto value = [](std::uint32_t bits) { return std::bit_cast<float>(bits); };
        const osg::Vec3f input(value(row.mInput[0]), value(row.mInput[1]), value(row.mInput[2]));
        if (row.mTransform)
        {
            auto matrix = osg::Matrixf::identity(); matrix.setTrans(input);
            body.mShape = NifBullet::RagdollTransform{matrix,value(row.mRadius),{NifBullet::RagdollSphere{1}}};
        }
        else body.mShape = NifBullet::RagdollBox{input,value(row.mRadius)};
        const auto scaled = NifBullet::ragdollBodyWithNativeScaledProperties(body, value(row.mScale));
        const auto result = row.mTransform ? osg::Vec3f(std::get<NifBullet::RagdollTransform>(scaled.mShape).mTransform.getTrans())
            : std::get<NifBullet::RagdollBox>(scaled.mShape).mHalfExtents;
        for (unsigned axis = 0; axis < 3; ++axis) EXPECT_EQ(std::bit_cast<std::uint32_t>(result[axis]), row.mOutput[axis]);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(row.mTransform
            ? std::get<NifBullet::RagdollTransform>(scaled.mShape).mRadius
            : std::get<NifBullet::RagdollBox>(scaled.mShape).mRadius), row.mRadius);
    }
}

TEST_F(ActorRagdollTest, CompoundWeaponLoaderRejectsMalformedBoxAndTransformRecords)
{
    const auto graph = NifBullet::loadActorRagdollDefinition(mFile);
    auto& body = static_cast<Nif::bhkRigidBody&>(*mFile.mRecords[graph.mBodies[0].mRecord]);
    const auto original = body.mShape;
    auto& box = add<Nif::bhkBoxShape>(); box.mRecordType = Nif::RC_bhkBoxShape;
    box.mExtents = {-1,1,2}; box.mRadius = .1f; body.mShape = Nif::bhkShapePtr(&box);
    EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    box.mExtents.x() = 1; box.mRadius = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    auto& transform = add<Nif::bhkConvexTransformShape>(); transform.mRecordType = Nif::RC_bhkConvexTransformShape;
    transform.mShape = original; transform.mRadius = .1f; transform.mTransform = osg::Matrixf::identity();
    transform.mTransform(2,1) = std::numeric_limits<float>::infinity();
    body.mShape = Nif::bhkShapePtr(&transform);
    EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
    transform.mTransform = osg::Matrixf::identity(); transform.mShape = Nif::bhkShapePtr(nullptr);
    EXPECT_THROW(NifBullet::loadActorRagdollDefinition(mFile), std::runtime_error);
}
