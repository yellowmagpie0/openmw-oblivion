#include "../nif/node.hpp"

#include <components/nif/physics.hpp>
#include <components/nifbullet/actorragdoll.hpp>
#include <components/nifbullet/ragdollcollisionfilter.hpp>

#include <gtest/gtest.h>

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
