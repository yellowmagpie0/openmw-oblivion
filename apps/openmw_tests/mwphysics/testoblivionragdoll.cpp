#include <apps/openmw/mwphysics/oblivionragdoll.hpp>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <limits>

namespace
{
    struct NativeRagdollSnapshotTest : ::testing::Test
    {
        const ESM::FormKey mBase = ESM::FormKey::content("actors.esm", 100);
        const std::string mModel = "characters/_male/skeleton.nif";
        NifBullet::ActorRagdollDefinition mGraph;
        std::vector<NifBullet::RagdollBodyState> mBodies;

        NativeRagdollSnapshotTest()
        {
            mGraph.mSourceHash = std::string("\0\x80\xff", 3) + std::string(13, 'a');
            for (unsigned i = 0; i < 2; ++i)
            {
                NifBullet::RagdollBodyDefinition body{};
                body.mRecord = 24 - i * 12;
                body.mNodeRecord = 20 - i * 12;
                body.mBone = i ? "pelvis" : "spine";
                body.mMass = 2;
                body.mCenter = {.1f, .2f, .3f};
                body.mInertia = {2, 0, 0, 0, 3, 0, 0, 0, 4};
                body.mShape = NifBullet::RagdollSphere{.5f};
                body.mLinearDamping = .1f;
                body.mAngularDamping = .2f;
                mGraph.mBodies.push_back(body);
                mBodies.push_back({body.mRecord,
                    btTransform(btQuaternion(btVector3(0, 0, 1), .2), btVector3(-0., 10 * i, 20)),
                    btVector3(1.25, -2.5, -0.), btVector3(.1, .2, .3)});
            }
        }

        ESM4::RuntimeState save(const ESM4::RuntimeActorRagdoll& pose)
        {
            ESM4::RuntimeState state;
            state.mPlayer.mReference = ESM::FormKey::dynamic("player", 1);
            state.mPlayer.mCell = ESM::FormKey::content("actors.esm", 1);
            state.mPlayer.mRace = ESM::FormKey::content("actors.esm", 2);
            state.mPlayer.mClass = ESM::FormKey::content("actors.esm", 3);
            ESM4::RuntimeReferenceState ref;
            ref.mKey = ESM::FormKey::content("actors.esm", 10);
            ref.mBase = mBase;
            ref.mCell = state.mPlayer.mCell;
            state.mReferences.push_back(ref);
            ESM4::RuntimeActorValues values;
            values.mActor = ref.mKey;
            values.mBase = mBase;
            state.mNativeActorValues.push_back(values);
            state.mNativeActorLife.push_back({ref.mKey, mBase, ESM4::ActorLifePhase::Dead, 0, {}});
            state.mNativeActorRagdolls.emplace(ref.mKey, pose);
            return state;
        }

        void checkPhysicalSnapshotContinuation(bool bodyT)

            {
            if (bodyT)
            {
                mGraph.mBodies[0].mUsesRigidBodyTransform = true;
                mGraph.mBodies[0].mTranslation = {2, 3, 4};
                mGraph.mBodies[0].mRotation = osg::Quat(0, 0, 1, 0);
            }
            struct World
            {
                btDefaultCollisionConfiguration mConfiguration;
                btCollisionDispatcher mDispatcher{&mConfiguration};
                btDbvtBroadphase mBroadphase;
                btSequentialImpulseConstraintSolver mSolver;
                btDiscreteDynamicsWorld mWorld{&mDispatcher, &mBroadphase, &mSolver, &mConfiguration};
                World() { mWorld.setGravity(btVector3(0, 0, 0)); }
            } first, second;
            std::vector<btTransform> poses;
            for (const auto& body : mBodies)
                poses.push_back(body.mPose);
            NifBullet::ActorRagdollPhysics uninterrupted(mGraph, first.mWorld,
                NifBullet::RagdollNativeLengthScale, poses, 1, -1);
            uninterrupted.restore(mBodies);
            constexpr float dt = 1.f / 120.f;
            const auto step = [&](auto& body, auto& world) {
                body.applyNativeDamping(dt);
                world.stepSimulation(dt, 0);
            };
            for (unsigned i = 0; i < 30; ++i)
                step(uninterrupted, first.mWorld);
            const auto snapshot = MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, uninterrupted.capture());
            const auto bytes = save(snapshot).serializeBinary();
            const auto decoded = ESM4::RuntimeState::deserializeBinary(bytes);
            const auto loaded = MWPhysics::restoreNativeActorRagdoll(decoded.mNativeActorRagdolls.begin()->second,
                mBase, mModel, mGraph);
            poses.clear();
            for (const auto& body : loaded)
                poses.push_back(body.mPose);
            NifBullet::ActorRagdollPhysics resumed(mGraph, second.mWorld,
                NifBullet::RagdollNativeLengthScale, poses, 1, -1);
            resumed.restore(loaded);
            for (unsigned i = 0; i < 60; ++i)
            {
                step(uninterrupted, first.mWorld);
                step(resumed, second.mWorld);
            }
            const auto expected = uninterrupted.capture(), actual = resumed.capture();
            ASSERT_EQ(actual.size(), expected.size());
            // Snapshot floats round the double-precision Bullet bridge at the save
            // boundary. Tolerances are fixed before running this continuation.
            for (unsigned i = 0; i < actual.size(); ++i)
                for (unsigned row = 0; row < 3; ++row)
                {
                    EXPECT_NEAR(actual[i].mPose.getOrigin()[row], expected[i].mPose.getOrigin()[row], 1e-4);
                    EXPECT_NEAR(actual[i].mLinearVelocity[row], expected[i].mLinearVelocity[row], 1e-6);
                    EXPECT_NEAR(actual[i].mAngularVelocity[row], expected[i].mAngularVelocity[row], 1e-6);
                    for (unsigned col = 0; col < 3; ++col)
                        EXPECT_NEAR(actual[i].mPose.getBasis()[row][col], expected[i].mPose.getBasis()[row][col], 1e-5);
                }
            EXPECT_EQ(first.mWorld.getNumCollisionObjects(), 2);
            EXPECT_EQ(second.mWorld.getNumCollisionObjects(), 2);
        }
    };

    TEST_F(NativeRagdollSnapshotTest, BodyTPhysicalProjectionRoundtripsWithoutApplyingLocalOffset)
    {
        auto& body = mGraph.mBodies[0];
        body.mUsesRigidBodyTransform = true;
        body.mTranslation = {2, 3, 4};
        body.mRotation = osg::Quat(0, 0, 1, 0);
        const auto snapshot = MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, mBodies);
        ASSERT_EQ(snapshot.mBodies.size(), 2);
        EXPECT_EQ(snapshot.mBodies[1].mRecord, 24);
        EXPECT_EQ(snapshot.mBodies[1].mPosition, (std::array<float, 3>{-0.f, 0.f, 20.f}));
        EXPECT_EQ(std::bit_cast<std::uint32_t>(snapshot.mBodies[1].mPosition[0]), 0x80000000u);
        const auto state = save(snapshot);
        const auto decoded = ESM4::RuntimeState::deserializeBinary(state.serializeBinary());
        EXPECT_EQ(decoded.canonicalJson(), state.canonicalJson());
        EXPECT_EQ(decoded.mNativeActorRagdolls.begin()->second, snapshot);
        auto roundtrip = snapshot;
        for (unsigned i = 0; i < 100; ++i)
        {
            const auto restored = MWPhysics::restoreNativeActorRagdoll(roundtrip, mBase, mModel, mGraph);
            EXPECT_EQ(restored[0].mPose.getOrigin(), mBodies[0].mPose.getOrigin());
            roundtrip = MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, restored);
            EXPECT_EQ(roundtrip, snapshot);
        }
        EXPECT_EQ(body.mTranslation, osg::Vec3f(2, 3, 4));
        EXPECT_EQ(body.mRotation, osg::Quat(0, 0, 1, 0));
    }

    TEST_F(NativeRagdollSnapshotTest, BodyTProjectionRetainsIdentityAndPhysicalInputRejection)
    {
        mGraph.mBodies[0].mUsesRigidBodyTransform = true;
        mGraph.mBodies[0].mTranslation = {2, 3, 4};
        mGraph.mBodies[0].mRotation = osg::Quat(0, 0, 1, 0);
        const auto snapshot = MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, mBodies);
        auto changed = mGraph;
        changed.mBodies[0].mRecord = changed.mBodies[1].mRecord;
        EXPECT_THROW(MWPhysics::captureNativeActorRagdoll(mBase, mModel, changed, mBodies), std::invalid_argument);
        changed = mGraph;
        changed.mBodies[0].mNodeRecord = changed.mBodies[1].mNodeRecord;
        EXPECT_THROW(MWPhysics::restoreNativeActorRagdoll(snapshot, mBase, mModel, changed), std::invalid_argument);
        changed = mGraph;
        changed.mSourceHash[0] ^= 1;
        EXPECT_THROW(MWPhysics::restoreNativeActorRagdoll(snapshot, mBase, mModel, changed), std::invalid_argument);
        auto invalid = mBodies;
        invalid[0].mPose.getOrigin().setX(std::numeric_limits<btScalar>::infinity());
        EXPECT_ANY_THROW(MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, invalid));
        EXPECT_EQ(MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, mBodies), snapshot);
    }

    TEST_F(NativeRagdollSnapshotTest, PreservesWorldCoordinatesAssetBytesAndGraphOrder)
    {
        const auto snapshot = MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, mBodies);
        EXPECT_EQ(snapshot.mAssetHash, "0080ff61616161616161616161616161");
        ASSERT_EQ(snapshot.mBodies.size(), 2);
        EXPECT_EQ(snapshot.mBodies[0].mRecord, 12);
        EXPECT_EQ(snapshot.mBodies[0].mNodeRecord, 8);
        EXPECT_EQ(snapshot.mBodies[1].mRecord, 24);
        EXPECT_EQ(snapshot.mBodies[1].mNodeRecord, 20);
        EXPECT_EQ(snapshot.mBodies[0].mPosition, (std::array<float, 3>{-0., 10, 20}));
        EXPECT_EQ(std::bit_cast<std::uint32_t>(snapshot.mBodies[0].mPosition[0]), 0x80000000u);
        const auto restored = MWPhysics::restoreNativeActorRagdoll(snapshot, mBase, mModel, mGraph);
        ASSERT_EQ(restored.size(), mBodies.size());
        for (unsigned i = 0; i < restored.size(); ++i)
        {
            EXPECT_EQ(restored[i].mRecord, mGraph.mBodies[i].mRecord);
            EXPECT_EQ(restored[i].mPose.getOrigin(), mBodies[i].mPose.getOrigin());
            EXPECT_EQ(restored[i].mLinearVelocity, mBodies[i].mLinearVelocity);
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned col = 0; col < 3; ++col)
                    EXPECT_EQ(restored[i].mPose.getBasis()[row][col], float(mBodies[i].mPose.getBasis()[row][col]));
        }
        auto permuted = mBodies;
        std::reverse(permuted.begin(), permuted.end());
        EXPECT_EQ(MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, permuted), snapshot);
    }

    TEST_F(NativeRagdollSnapshotTest, RejectsChangedWinningAssetsAndBodyBindingsBeforeRestore)
    {
        const auto snapshot = MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, mBodies);
        EXPECT_THROW(MWPhysics::restoreNativeActorRagdoll(snapshot, ESM::FormKey{}, mModel, mGraph), std::invalid_argument);
        EXPECT_THROW(MWPhysics::restoreNativeActorRagdoll(snapshot, mBase, "other.nif", mGraph), std::invalid_argument);
        for (unsigned field = 0; field < 6; ++field)
        {
            auto changed = mGraph;
            switch (field)
            {
                case 0: changed.mSourceHash[0] = 1; break;
                case 1: changed.mSourceHash.clear(); break;
                case 2: changed.mBodies[0].mRecord = 28; break;
                case 3: changed.mBodies[0].mNodeRecord = 21; break;
                case 4: changed.mBodies.pop_back(); break;
                case 5: changed.mBodies[0].mNodeRecord = changed.mBodies[1].mNodeRecord; break;
            }
            EXPECT_THROW(MWPhysics::restoreNativeActorRagdoll(snapshot, mBase, mModel, changed), std::invalid_argument);
        }
        auto invalid = snapshot;
        invalid.mBodies[1].mRotation[0] = 2;
        EXPECT_THROW(MWPhysics::restoreNativeActorRagdoll(invalid, mBase, mModel, mGraph), std::runtime_error);
        EXPECT_EQ(MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, mBodies), snapshot);
    }

    TEST_F(NativeRagdollSnapshotTest, RejectsMissingDuplicateUnknownAndNonfinitePhysicalCaptures)
    {
        const auto snapshot = MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, mBodies);
        for (unsigned field = 0; field < 8; ++field)
        {
            auto invalid = mBodies;
            switch (field)
            {
                case 0: invalid.pop_back(); break;
                case 1: invalid[0].mRecord = 100; break;
                case 2: invalid[0].mRecord = invalid[1].mRecord; break;
                case 3: invalid[0].mPose.getBasis()[0][0] = 2; break;
                case 4: invalid[0].mPose.getOrigin().setX(std::numeric_limits<btScalar>::quiet_NaN()); break;
                case 5: invalid[0].mLinearVelocity.setY(std::numeric_limits<btScalar>::infinity()); break;
                case 6: invalid[0].mAngularVelocity.setZ(std::numeric_limits<btScalar>::quiet_NaN()); break;
                case 7: invalid[0].mPose.getOrigin().setX(std::numeric_limits<btScalar>::max()); break;
            }
            EXPECT_ANY_THROW(MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, invalid));
        }
        EXPECT_EQ(MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, mBodies), snapshot);
    }

    TEST_F(NativeRagdollSnapshotTest, BinarySnapshotResumesPhysicalMotionInAnIndependentWorld)
    {
        checkPhysicalSnapshotContinuation(false);
    }
    TEST_F(NativeRagdollSnapshotTest, BodyTBinarySnapshotResumesPhysicalMotionInAnIndependentWorld)
    {
        checkPhysicalSnapshotContinuation(true);
    }
}


namespace
{
    TEST_F(NativeRagdollSnapshotTest, PackedNativeVelocitySnapshotRestartsAndContinuesExactPhases)
    {
        struct World
        {
            btDefaultCollisionConfiguration mConfiguration;
            btCollisionDispatcher mDispatcher{&mConfiguration};
            btDbvtBroadphase mBroadphase;
            btSequentialImpulseConstraintSolver mSolver;
            btDiscreteDynamicsWorld mWorld{&mDispatcher, &mBroadphase, &mSolver, &mConfiguration};
        } first, second;
        for (auto& body : mGraph.mBodies) body.mMaxAngularVelocity = 31.4159f;
        std::vector<btTransform> poses; for (const auto& body : mBodies) poses.push_back(body.mPose);
        NifBullet::ActorRagdollPhysics uninterrupted(mGraph, first.mWorld, NifBullet::RagdollNativeLengthScale, poses, 1, -1);
        auto initial = uninterrupted.captureNativePackedVelocities();
        initial[0].mVelocities = {{1000, -2000, 3000, 8}, {1000, -2000, 3000, 8}};
        initial[1].mVelocities = {{1, 2, 3, -0.f}, {4, 5, 6, -8}};
        uninterrupted.restoreNativePackedVelocities(initial);
        const auto snapshot = MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph,
            uninterrupted.capture(), uninterrupted.captureNativePackedVelocities());
        const auto wire = save(snapshot).serializeBinary();
        const auto decoded = ESM4::RuntimeState::deserializeBinary(wire);
        const auto& loaded = decoded.mNativeActorRagdolls.begin()->second;
        const auto spatial = MWPhysics::restoreNativeActorRagdoll(loaded, mBase, mModel, mGraph);
        const auto packed = MWPhysics::restoreNativeActorRagdollPackedVelocities(loaded, mBase, mModel, mGraph);
        ASSERT_TRUE(packed); ASSERT_EQ(packed->size(), initial.size());
        // The asset order is24/12; the wire is12/24. Restore must resolve IDs.
        EXPECT_EQ((*packed)[0].mRecord, 24u); EXPECT_EQ((*packed)[1].mRecord, 12u);
        NifBullet::ActorRagdollPhysics resumed(mGraph, second.mWorld, NifBullet::RagdollNativeLengthScale, poses, 1, -1);
        resumed.restore(spatial, *packed);
        const std::array<std::array<float, 4>, 2> deltas{{{0, 0, -1.1772000789642334f, 8}, {0, 0, 0, 0}}};
        for (unsigned step = 0; step < 60; ++step)
        {
            const auto expected = uninterrupted.captureNativePackedVelocities();
            const auto actual = resumed.captureNativePackedVelocities();
            for (std::size_t body = 0; body < expected.size(); ++body)
                for (unsigned axis = 0; axis < 4; ++axis)
                {
                    EXPECT_EQ(std::bit_cast<std::uint32_t>(actual[body].mVelocities.mLinear[axis]), std::bit_cast<std::uint32_t>(expected[body].mVelocities.mLinear[axis]));
                    EXPECT_EQ(std::bit_cast<std::uint32_t>(actual[body].mVelocities.mAngular[axis]), std::bit_cast<std::uint32_t>(expected[body].mVelocities.mAngular[axis]));
                }
            uninterrupted.applyNativePackedVelocityStep(.016f, deltas);
            resumed.applyNativePackedVelocityStep(.016f, deltas);
        }
        auto legacy = loaded; for (auto& body : legacy.mBodies) body.mNativePackedVelocity.reset();
        EXPECT_FALSE(MWPhysics::restoreNativeActorRagdollPackedVelocities(legacy, mBase, mModel, mGraph));
        auto wrong = initial; wrong[1].mRecord = 999;
        EXPECT_THROW(MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, uninterrupted.capture(), wrong), std::invalid_argument);
        EXPECT_THROW(MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, uninterrupted.capture(), std::span<const NifBullet::RagdollNativePackedVelocityState>(initial.data(), 1)), std::invalid_argument);
    }
}


namespace
{
    TEST_F(NativeRagdollSnapshotTest, NativeMotionSnapshotRestoresFreshKeyframedOwnerAndVelocityPhase)
    {
        struct World
        {
            btDefaultCollisionConfiguration mConfiguration;
            btCollisionDispatcher mDispatcher{&mConfiguration};
            btDbvtBroadphase mBroadphase;
            btSequentialImpulseConstraintSolver mSolver;
            btDiscreteDynamicsWorld mWorld{&mDispatcher, &mBroadphase, &mSolver, &mConfiguration};
        } first, second;
        for (auto& body : mGraph.mBodies) body.mMaxAngularVelocity = 31.4159f;
        std::vector<btTransform> poses; for (const auto& body : mBodies) poses.push_back(body.mPose);
        NifBullet::ActorRagdollPhysics uninterrupted(mGraph, first.mWorld, NifBullet::RagdollNativeLengthScale, poses, 1, -1);
        const std::array key{NifBullet::RagdollNativeMotionRequest{24, NifBullet::RagdollNativeMotion::Keyframed}};
        uninterrupted.setNativeMotionModes(key);
        auto initial = uninterrupted.captureNativePackedVelocities();
        initial[0].mVelocities = {{1, 2, 3, 8}, {4, 5, 6, -0.f}};
        initial[1].mVelocities = {{7, 8, 9, -8}, {10, 11, 12, 4}};
        uninterrupted.restoreNativePackedVelocities(initial);
        const auto modes = uninterrupted.captureNativeMotionModes();
        const auto snapshot = MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph,
            uninterrupted.capture(), initial, modes);
        const auto decoded = ESM4::RuntimeState::deserializeBinary(save(snapshot).serializeBinary());
        const auto& loaded = decoded.mNativeActorRagdolls.begin()->second;
        const auto spatial = MWPhysics::restoreNativeActorRagdoll(loaded, mBase, mModel, mGraph);
        const auto packed = MWPhysics::restoreNativeActorRagdollPackedVelocities(loaded, mBase, mModel, mGraph);
        const auto restoredModes = MWPhysics::restoreNativeActorRagdollMotionModes(loaded, mBase, mModel, mGraph);
        ASSERT_TRUE(packed); ASSERT_TRUE(restoredModes);
        EXPECT_EQ(*restoredModes, modes);
        NifBullet::ActorRagdollPhysics resumed(mGraph, second.mWorld, NifBullet::RagdollNativeLengthScale, poses, 1, -1);
        ASSERT_EQ(resumed.captureNativeMotionModes()[0].mMotion, NifBullet::RagdollNativeMotion::Dynamic);
        resumed.restore(spatial, *packed, *restoredModes);
        EXPECT_EQ(resumed.captureNativeMotionModes(), modes);
        EXPECT_EQ(btRigidBody::upcast(resumed.collisionObjects()[0])->getInvMass(), 0);
        EXPECT_EQ(btRigidBody::upcast(resumed.collisionObjects()[1])->getInvMass(), .5);
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const std::array<std::array<float, 4>, 2> deltas{{{nan, nan, nan, nan}, {0, 0, -1.1772000789642334f, 0}}};
        for (unsigned step = 0; step < 60; ++step)
        {
            const auto expected = uninterrupted.captureNativePackedVelocities();
            const auto actual = resumed.captureNativePackedVelocities();
            for (std::size_t body = 0; body < expected.size(); ++body)
                for (unsigned axis = 0; axis < 4; ++axis)
                {
                    EXPECT_EQ(std::bit_cast<std::uint32_t>(actual[body].mVelocities.mLinear[axis]), std::bit_cast<std::uint32_t>(expected[body].mVelocities.mLinear[axis]));
                    EXPECT_EQ(std::bit_cast<std::uint32_t>(actual[body].mVelocities.mAngular[axis]), std::bit_cast<std::uint32_t>(expected[body].mVelocities.mAngular[axis]));
                }
            EXPECT_EQ(actual[0].mVelocities.mLinear, initial[0].mVelocities.mLinear);
            uninterrupted.applyNativePackedVelocityStep(.016f, deltas);
            resumed.applyNativePackedVelocityStep(.016f, deltas);
        }
        auto legacy = loaded; for (auto& body : legacy.mBodies) body.mNativeMotion.reset();
        EXPECT_FALSE(MWPhysics::restoreNativeActorRagdollMotionModes(legacy, mBase, mModel, mGraph));
        auto wrong = modes; wrong[1].mRecord = 999;
        EXPECT_THROW(MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, uninterrupted.capture(), initial, wrong), std::invalid_argument);
        wrong = modes; wrong[1].mMotion = static_cast<NifBullet::RagdollNativeMotion>(255);
        EXPECT_THROW(MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, uninterrupted.capture(), initial, wrong), std::invalid_argument);
        EXPECT_THROW(MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, uninterrupted.capture(), initial,
            std::span<const NifBullet::RagdollNativeMotionRequest>(modes.data(), 1)), std::invalid_argument);
        EXPECT_THROW(MWPhysics::captureNativeActorRagdoll(mBase, mModel, mGraph, uninterrupted.capture(),
            std::span<const NifBullet::RagdollNativePackedVelocityState>{}, modes), std::invalid_argument);
    }
}
