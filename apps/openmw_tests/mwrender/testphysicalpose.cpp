#include <apps/openmw/mwrender/animation.hpp>
#include <apps/openmw/mwrender/oblivionmeleeprojection.hpp>
#include <components/esm4/runtimestate.hpp>
#include <components/misc/osguservalues.hpp>
#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/nifosg/matrixtransform.hpp>
#include <components/nif/niftypes.hpp>
#include <components/sceneutil/skeleton.hpp>
#include <components/sceneutil/postphysicsupdatevisitor.hpp>
#include <components/sceneutil/riggeometry.hpp>
#include <osgUtil/CullVisitor>
#include <components/sceneutil/keyframe.hpp>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>

#include <gtest/gtest.h>
#include <osg/Geode>
#include <osg/MatrixTransform>
#include <osgUtil/UpdateVisitor>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace
{
    class PoseWriter : public osg::NodeCallback
    {
    public:
        unsigned mWrites = 0;
        bool mWriteTranslation = true;
        bool mWriteRotation = true;
        bool mInvalidTranslation = false;
        bool mThrowAfterWrite = false;
        osg::Vec3f mTranslation{1, 2, 3};
        bool mReadParent = false;
        double mObservedParentX = 0;
        double mObservedSimulationTime = 0;
        unsigned mObservedTraversal = 0;
        unsigned mObservedPathLength = 0;
        osg::Vec3f mObservedLocalBefore;
        void operator()(osg::Node* node, osg::NodeVisitor* visitor) override
        {
            ++mWrites;
            if (visitor->getFrameStamp())
                mObservedSimulationTime = visitor->getFrameStamp()->getSimulationTime();
            mObservedTraversal = visitor->getTraversalNumber();
            mObservedPathLength = visitor->getNodePath().size();
            if (mReadParent)
                mObservedParentX = dynamic_cast<osg::MatrixTransform*>(node->getParent(0))->getMatrix().getTrans().x();
            auto& transform = *dynamic_cast<NifOsg::MatrixTransform*>(node);
            mObservedLocalBefore = transform.getControllerTransform().mTranslation;
            if (mWriteTranslation)
                transform.setTranslation(mInvalidTranslation
                    ? osg::Vec3f(std::numeric_limits<float>::quiet_NaN(), 0, 0) : mTranslation);
            if (mWriteRotation)
                transform.setRotation(osg::Quat(.4, osg::Vec3f(0, 0, 1)));
            if (mThrowAfterWrite)
                throw std::runtime_error("pose callback failed");
            traverse(node, visitor);
        }
    };

    class ResidentKeyframe : public SceneUtil::KeyframeController, public osg::NodeCallback
    {
    public:
        unsigned mWrites = 0;
        osg::Callback* getAsCallback() override { return this; }
        void operator()(osg::Node* node, osg::NodeVisitor* visitor) override
        {
            ++mWrites;
            auto& transform = *dynamic_cast<NifOsg::MatrixTransform*>(node);
            transform.setRotation(transform.mRotationScale);
            traverse(node, visitor);
        }
    };

    class SceneCounter : public osg::NodeCallback
    {
    public:
        unsigned mVisits = 0;
        void operator()(osg::Node* node, osg::NodeVisitor* visitor) override
        {
            ++mVisits;
            traverse(node, visitor);
        }
    };

    class NativeTrack : public SceneUtil::KeyframeController, public osg::NodeCallback
    {
    public:
        struct Observation { unsigned mSamples = 0; float mTime = -1; };
        KfTransform mPose;
        osg::Vec3f mVelocity;
        bool mThrow = false;
        std::shared_ptr<Observation> mObserved = std::make_shared<Observation>();
        NativeTrack() = default;
        NativeTrack(const NativeTrack& other, const osg::CopyOp& copy)
            : osg::Object(other, copy), SceneUtil::KeyframeController(other, copy), osg::NodeCallback(other, copy)
            , mPose(other.mPose), mVelocity(other.mVelocity), mThrow(other.mThrow), mObserved(other.mObserved) {}
        META_Object(Testing, NativeTrack)
        osg::Callback* getAsCallback() override { return this; }
        KfTransform getCurrentTransformation(osg::NodeVisitor* visitor) override
        {
            if (mThrow) throw std::runtime_error("native track sample failed");
            ++mObserved->mSamples;
            const float time = hasInput() ? getInputValue(visitor) : 0;
            mObserved->mTime = time;
            auto result = mPose;
            if (result.mTranslation) *result.mTranslation += mVelocity * time;
            return result;
        }
        void operator()(osg::Node* node, osg::NodeVisitor* visitor) override
        {
            auto* bone = dynamic_cast<NifOsg::MatrixTransform*>(node);
            const auto pose = getCurrentTransformation(visitor);
            if (pose.mTranslation) bone->setTranslation(*pose.mTranslation);
            if (pose.mRotation) bone->setRotation(*pose.mRotation);
            if (pose.mScale) bone->setScale(*pose.mScale);
            traverse(node, visitor);
        }
    };

    class TestAnimation : public MWRender::Animation
    {
    public:
        osg::ref_ptr<NifOsg::MatrixTransform> mBone;
        osg::ref_ptr<PoseWriter> mWriter = new PoseWriter;
        MWRender::ActiveControllersVector mBeforeControllers;
        bool mThrowAfterControllers = false;
        unsigned mControllerBuilds = 0;
        TestAnimation(osg::ref_ptr<osg::Group> parent, osg::ref_ptr<SceneUtil::Skeleton> root,
            osg::ref_ptr<NifOsg::MatrixTransform> bone)
            : Animation({}, parent, nullptr), mBone(std::move(bone))
        {
            mObjectRoot = root;
            mSkeleton = root;
            parent->addChild(root);
            resetActiveGroups();
        }
        void rebuildControllers() { resetActiveGroups(); }
        void usePassivePlayback() { mPassivePlayback = true; }
        void bindRoots(std::span<const SceneUtil::ControllerSequenceMetadata> sequences)
        { bindNativeReactionRoot(sequences); }
        void replaceBoundRoot(osg::Group* root) { mObjectRoot = root; }
        osg::ref_ptr<SceneUtil::KeyframeHolder> addNativeSource(
            std::string group, osg::ref_ptr<NativeTrack> track, std::uint8_t priority, float stop = 10)
        {
            osg::ref_ptr<SceneUtil::KeyframeHolder> holder = new SceneUtil::KeyframeHolder;
            holder->mTextKeys.emplace(0.f, group + ": start");
            holder->mTextKeys.emplace(stop, group + ": stop");
            SceneUtil::ControllerSequenceMetadata sequence;
            sequence.mGroup = group;
            sequence.mTimelineStop = sequence.mStopTime = stop;
            sequence.mTransformPriorities.emplace_back(mBone->getName(), priority);
            holder->mControllerSequences.push_back(std::move(sequence));
            holder->mKeyframeControllers.emplace(mBone->getName(), track);
            auto source = std::make_shared<AnimSource>();
            source->mKeyframes = holder;
            source->mControllerMap[0].emplace(Misc::StringUtils::lowerCase(mBone->getName()), track);
            mAnimSources.push_back(source);
            mSupportedAnimations.insert(std::move(group));
            return holder;
        }
    private:
        void addControllers() override
        {
            ++mControllerBuilds;
            for (const auto& [node, callback] : mBeforeControllers)
            {
                node->addUpdateCallback(callback);
                mActiveControllers.emplace_back(node, callback);
            }
            mBone->addUpdateCallback(mWriter);
            mActiveControllers.emplace_back(mBone, mWriter);
            if (mThrowAfterControllers)
                throw std::runtime_error("controller rebuild failed");
        }
    };

    struct PhysicalPoseAnimationTest : ::testing::Test
    {
        osg::ref_ptr<osg::Group> mParent = new osg::Group;
        osg::ref_ptr<SceneUtil::Skeleton> mRoot = new SceneUtil::Skeleton;
        osg::ref_ptr<NifOsg::MatrixTransform> mBone = new NifOsg::MatrixTransform;
        NifBullet::ActorRagdollDefinition mGraph;
        std::unique_ptr<TestAnimation> mAnimation;
        osg::ref_ptr<osg::FrameStamp> mFrameStamp = new osg::FrameStamp;
        osgUtil::UpdateVisitor mVisitor;
        PhysicalPoseAnimationTest()
        {
            mRoot->setUserValue(Misc::OsgUserValues::sFileHash, std::string("physical-pose-test"));
            mBone->setName("Pelvis");
            mBone->setRotation(osg::Quat());
            mBone->setScale(1);
            mBone->setTranslation(osg::Vec3f());
            mBone->setUserValue("recordIndex", 8u);
            mRoot->addChild(mBone);
            mGraph.mSourceHash = "physical-pose-test";
            NifBullet::RagdollBodyDefinition body{};
            body.mRecord = 12;
            body.mNodeRecord = 8;
            body.mBone = "Pelvis";
            mGraph.mBodies.push_back(body);
            mAnimation = std::make_unique<TestAnimation>(mParent, mRoot, mBone);
            mVisitor.setFrameStamp(mFrameStamp);
        }
        void traverse() { mRoot->accept(mVisitor); }
    };


    osg::ref_ptr<NativeTrack> nativeTrack(osg::Vec3f position, float scale = 1, bool rotated = false)
    {
        osg::ref_ptr<NativeTrack> result = new NativeTrack;
        result->mPose.mTranslation = position;
        const float q = .7071067811865476f;
        result->mPose.mRotation = rotated ? osg::Quat(0, 0, q, q) : osg::Quat();
        result->mPose.mScale = scale;
        return result;
    }

    TEST_F(PhysicalPoseAnimationTest, ReactionWorldBoundUsesParentPlacementOnceAndRejectsAmbiguousPaths)
    {
        osg::ref_ptr<osg::MatrixTransform> actor = new osg::MatrixTransform;
        actor->setMatrix(osg::Matrixf::scale(2, 2, 2) * osg::Matrixf::translate(100, 200, 300));
        mParent->addChild(actor);
        osg::ref_ptr<osg::MatrixTransform> model = new osg::MatrixTransform;
        model->setMatrix(osg::Matrixf::translate(10, 0, 0));
        actor->addChild(model);
        osg::ref_ptr<osg::Geode> geometry = new osg::Geode;
        geometry->setInitialBound(osg::BoundingSphere(osg::Vec3f(1, 2, 3), 2));
        model->addChild(geometry);
        mAnimation->replaceBoundRoot(model);
        const auto bound = mAnimation->nativeReactionWorldBound(*mParent);
        ASSERT_TRUE(bound);
        EXPECT_EQ(*bound, (std::array<float, 4>{122, 204, 306, 4}));
        osg::ref_ptr<osg::Group> unrelated = new osg::Group;
        EXPECT_FALSE(mAnimation->nativeReactionWorldBound(*unrelated));
        osg::ref_ptr<osg::Group> secondPlacement = new osg::Group;
        mParent->addChild(secondPlacement);
        secondPlacement->addChild(model);
        EXPECT_THROW(mAnimation->nativeReactionWorldBound(*mParent), std::invalid_argument);
    }

    TEST_F(PhysicalPoseAnimationTest, CapturedNativePoseRemainsConstantAndBlendsIntoDestination)
    {
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        auto destination = nativeTrack({100, 200, 300}, 3);
        mAnimation->addNativeSource("native-reaction", destination, 9);
        mBone->setTranslation({11, 22, 33}); mBone->setScale(2);
        const auto captured = mAnimation->captureNativeSequencePose("native-reaction", 0);
        ASSERT_TRUE(captured); ASSERT_EQ(captured->mTracks.size(), 1u);
        EXPECT_EQ(captured->mTracks[0].mTranslation, (std::array<float, 3>{11, 22, 33}));
        EXPECT_EQ(captured->mTracks[0].mRotation, (std::array<float, 4>{1, 0, 0, 0}));
        EXPECT_EQ(captured->mTracks[0].mScale, 2); EXPECT_EQ(captured->mTracks[0].mPriority, 9);
        const auto snapshot = std::make_shared<const ESM4::RuntimeMeleePoseSnapshot>(*captured);
        mBone->setTranslation({-111, -222, -333});
        std::array<MWRender::Animation::NativeSequenceLayer, 2> layers{{
            {"native-reaction", MWRender::BlendMask_All, 0, 1, 1, false, snapshot},
            {"native-reaction", MWRender::BlendMask_All, 0, 1, 0}}};
        unsigned builds = 0;
        for (unsigned i = 0; i <= 4; ++i)
        {
            layers[0].mEase = 1 - i * .25f; layers[1].mEase = i * .25f;
            ASSERT_TRUE(mAnimation->setNativeSequenceLayers(layers));
            if (i == 0) builds = mAnimation->mControllerBuilds;
            EXPECT_EQ(mAnimation->mControllerBuilds, builds);
            mFrameStamp->setFrameNumber(i); traverse();
            const auto local = mBone->getControllerTransform();
            if (i == 0) EXPECT_EQ(destination->mObserved->mSamples, 0u);
            // Full original6C5C70/6CA950/6CC3A0 one-block observations.
            EXPECT_EQ(local.mTranslation.x(), 11 + i * 22.25f);
            EXPECT_EQ(local.mTranslation.y(), 22 + i * 44.5f);
            EXPECT_EQ(local.mTranslation.z(), 33 + i * 66.75f);
            EXPECT_EQ(local.mScale, 2 + i * .25f);
            EXPECT_EQ(destination->mObserved->mTime, i == 0 ? -1 : 0);
        }
        auto replacement = std::make_shared<ESM4::RuntimeMeleePoseSnapshot>(*captured);
        replacement->mTracks[0].mTranslation = {7, 8, 9};
        layers[0].mPoseSnapshot = replacement; layers[0].mEase = 1; layers[1].mEase = 0;
        ASSERT_TRUE(mAnimation->setNativeSequenceLayers(layers));
        EXPECT_GT(mAnimation->mControllerBuilds, builds);
        mFrameStamp->setFrameNumber(5); traverse();
        EXPECT_EQ(mBone->getControllerTransform().mTranslation.x(), 7);
        auto invalid = layers; invalid[0].mFollowAnimationTime = true;
        EXPECT_THROW(mAnimation->setNativeSequenceLayers(invalid), std::invalid_argument);
        EXPECT_FALSE(mAnimation->captureNativeSequencePose("missing", 0));
    }

    TEST_F(PhysicalPoseAnimationTest, CapturedPoseSkipsAuthoredTracksWithoutBoundControllers)
    {
        mRoot->setName("uncontrolled-root");
        auto destination = nativeTrack({100, 200, 300}, 3);
        auto holder = mAnimation->addNativeSource("native-reaction", destination, 9);
        auto& priorities = holder->mControllerSequences.front().mTransformPriorities;
        priorities.insert(priorities.begin(), {"missing-before", 99});
        priorities.emplace_back("uncontrolled-root", 77);
        priorities.emplace_back("missing-after", 88);
        holder->mKeyframeControllers.emplace("missing-before", nativeTrack({7, 8, 9}));
        mBone->setTranslation({11, 22, 33}); mBone->setScale(2);
        const auto captured = mAnimation->captureNativeSequencePose("native-reaction", 0);
        ASSERT_TRUE(captured);
        ASSERT_EQ(captured->mTracks.size(), 1u);
        EXPECT_EQ(captured->mTracks.front().mBone, "pelvis");
        EXPECT_EQ(captured->mTracks.front().mPriority, 9);
        EXPECT_EQ(captured->mTracks.front().mTranslation, (std::array<float, 3>{11, 22, 33}));
        EXPECT_EQ(captured->mTracks.front().mScale, 2);
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        const std::array<MWRender::Animation::NativeSequenceLayer, 2> layers{{
            {"native-reaction", MWRender::BlendMask_All, 0, 1, .5f, false,
                std::make_shared<const ESM4::RuntimeMeleePoseSnapshot>(*captured)},
            {"native-reaction", MWRender::BlendMask_All, 0, 1, .5f}}};
        ASSERT_TRUE(mAnimation->setNativeSequenceLayers(layers));
        traverse();
        EXPECT_EQ(mBone->getControllerTransform().mTranslation.x(), 55.5f);
        EXPECT_EQ(mBone->getControllerTransform().mScale, 2.5f);
    }

    TEST_F(PhysicalPoseAnimationTest, CapturedNativeRotationPreservesQuarterTurnAtNegativeAndZeroScale)
    {
        mAnimation->addNativeSource("native-reaction", nativeTrack({100, 200, 300}), 9);
        Nif::Matrix3 rotation;
        const std::array<float, 9> values{0, -1, 0, 1, 0, 0, 0, 0, 1};
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned column = 0; column < 3; ++column)
                rotation.mValues[row][column] = values[row * 3 + column];
        for (float scale : {-2.f, -0.f, 0.f, 2.f})
        {
            mBone->setRotation(rotation); mBone->setScale(scale); mBone->setTranslation({11, 22, 33});
            const auto captured = mAnimation->captureNativeSequencePose("native-reaction", 0);
            ASSERT_TRUE(captured); ASSERT_EQ(captured->mTracks.size(), 1u);
            const auto& pose = captured->mTracks.front();
            EXPECT_EQ(pose.mTranslation, (std::array<float, 3>{11, 22, 33}));
            EXPECT_EQ(std::bit_cast<std::uint32_t>(pose.mScale), std::bit_cast<std::uint32_t>(scale));
            // Original7150F0 quarter-turn-z fixture, no quaternion normalization.
            EXPECT_EQ(std::bit_cast<std::uint32_t>(pose.mRotation[0]), 1060439283u);
            EXPECT_EQ(pose.mRotation[1], 0); EXPECT_EQ(pose.mRotation[2], 0);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(pose.mRotation[3]), 1060439283u);
        }
    }

    TEST_F(PhysicalPoseAnimationTest, NativeSequenceLayersUseAuthoredPriorityAndCallerFramesWithoutRebuilding)
    {
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        auto first = nativeTrack({1, 2, 3});
        auto second = nativeTrack({11, 12, 13}, 2, true);
        first->mVelocity = {2, 0, 0}; second->mVelocity = {0, 2, 0};
        mAnimation->addNativeSource("native-idle", first, 20);
        mAnimation->addNativeSource("native-reaction", second, 75);
        std::array<MWRender::Animation::NativeSequenceLayer, 2> layers{{
            {"native-idle", MWRender::BlendMask_All, 0, 1, 1},
            {"native-reaction", MWRender::BlendMask_All, 0, 1, .25f}}};
        ASSERT_TRUE(mAnimation->setNativeSequenceLayers(layers));
        const auto builds = mAnimation->mControllerBuilds;
        EXPECT_EQ(first->mObserved->mSamples, 0u);
        EXPECT_FALSE(first->getSource()); EXPECT_FALSE(second->getSource());
        traverse();
        const auto original = mBone->getControllerTransform();
        // Independent two-track original fixture, threshold0.
        EXPECT_EQ(std::bit_cast<std::uint32_t>(original.mTranslation.x()), 1071805992u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(original.mTranslation.y()), 1076968212u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(original.mTranslation.z()), 1081162516u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(original.mScale), 1065998494u);
        EXPECT_NEAR(mBone->mRotationScale.toOsgMatrix()(0, 1), .11094956, 1e-6);
        layers[0].mTime = .5f; layers[1].mTime = 1; layers[1].mEase = .5f;
        ASSERT_TRUE(mAnimation->setNativeSequenceLayers(layers));
        EXPECT_EQ(mAnimation->mControllerBuilds, builds);
        mFrameStamp->setFrameNumber(1); traverse();
        EXPECT_EQ(first->mObserved->mTime, .5f); EXPECT_EQ(second->mObserved->mTime, 1);
        EXPECT_FLOAT_EQ(mBone->getControllerTransform().mTranslation.x(), 5);
        EXPECT_FLOAT_EQ(mBone->getControllerTransform().mTranslation.y(), 6);
        const auto samples = first->mObserved->mSamples;
        ASSERT_TRUE(mAnimation->setNativeSequenceLayers({}));
        mAnimation->mWriter->mWriteTranslation = true;
        mFrameStamp->setFrameNumber(2); traverse();
        EXPECT_EQ(first->mObserved->mSamples, samples);
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(1, 2, 3));
    }

    TEST_F(PhysicalPoseAnimationTest, Slot0SourceSelectionAndSuppressionPreserveParallelSlots)
    {
        // Source selection does not require gameplay notifications. This
        // renderer fixture has no Lua manager for animation-end callbacks.
        mAnimation->usePassivePlayback();
        const auto add = [&](const std::string& group, const std::string& name, int priority, int mask) {
            auto holder = mAnimation->addNativeSource(group, nativeTrack({0, 0, 0}), 20);
            holder->mControllerSequences.front().mSequenceName = name;
            mAnimation->play(group, MWRender::AnimPriority{priority}, mask, false, 1, "start", "stop", 0, 0, false);
        };
        add("handtohandidle", "Idle", 1, MWRender::BlendMask_All);
        add("handtohandwalkforward", "Forward", 2, MWRender::BlendMask_All);
        add("handtohandattackleft", "AttackLeft", 10, MWRender::BlendMask_All);
        add("specialidle", "SpecialIdle", 20, MWRender::BlendMask_All);
        mAnimation->runAnimation(.25f);
        const auto source = mAnimation->nativeLocomotionSource();
        ASSERT_TRUE(source);
        EXPECT_EQ(source->mGroup, "handtohandwalkforward");
        EXPECT_EQ(source->mTime, .25f);
        EXPECT_FALSE(source->mFollowAnimationTime);
        const auto background = mAnimation->nativePlayingSequenceLayers({}, true);
        ASSERT_EQ(background.size(), 2u);
        EXPECT_EQ(background[0].mGroup, "handtohandattackleft");
        EXPECT_EQ(background[1].mGroup, "specialidle");
        EXPECT_EQ(mAnimation->nativePlayingSequenceLayers({}).size(), 4u);
        mAnimation->disable("handtohandwalkforward");
        ASSERT_TRUE(mAnimation->nativeLocomotionSource());
        EXPECT_EQ(mAnimation->nativeLocomotionSource()->mGroup, "handtohandidle");
        mAnimation->disable("handtohandidle");
        EXPECT_FALSE(mAnimation->nativeLocomotionSource());
    }

    TEST_F(PhysicalPoseAnimationTest, NativeBackgroundLayersFollowSharedClockWithoutAdvancingOwnedSamples)
    {
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        auto idle = nativeTrack({0, 0, 0}); idle->mVelocity = {1, 0, 0};
        auto reaction = nativeTrack({10, 0, 0});
        mAnimation->addNativeSource("native-idle", idle, 20);
        mAnimation->addNativeSource("native-reaction", reaction, 75);
        mAnimation->play("native-idle", MWRender::AnimPriority{1}, MWRender::BlendMask_All,
            false, 1, "start", "stop", 0, 0, false);
        const std::array<std::string_view, 1> excluded{{"native-reaction"}};
        auto layers = mAnimation->nativePlayingSequenceLayers(excluded);
        ASSERT_EQ(layers.size(), 1u); EXPECT_TRUE(layers[0].mFollowAnimationTime);
        layers.push_back({"native-reaction", MWRender::BlendMask_All, 1, 1, .5f});
        ASSERT_TRUE(mAnimation->setNativeSequenceLayers(layers));
        const auto builds = mAnimation->mControllerBuilds;
        mAnimation->runAnimation(.25f); traverse();
        EXPECT_EQ(idle->mObserved->mTime, .25f);
        EXPECT_EQ(reaction->mObserved->mTime, 1);
        mAnimation->runAnimation(.25f); mFrameStamp->setFrameNumber(1); traverse();
        EXPECT_EQ(idle->mObserved->mTime, .5f);
        EXPECT_EQ(reaction->mObserved->mTime, 1);
        EXPECT_EQ(mAnimation->mControllerBuilds, builds);
        layers[0].mFollowAnimationTime = false; layers[0].mTime = .125f;
        ASSERT_TRUE(mAnimation->setNativeSequenceLayers(layers));
        mAnimation->runAnimation(.25f); mFrameStamp->setFrameNumber(2); traverse();
        EXPECT_EQ(idle->mObserved->mTime, .125f);
        EXPECT_EQ(mAnimation->mControllerBuilds, builds);
        auto invalid = layers; invalid[1].mFollowAnimationTime = true;
        EXPECT_FALSE(mAnimation->setNativeSequenceLayers(invalid));
        mFrameStamp->setFrameNumber(3); traverse();
        EXPECT_EQ(idle->mObserved->mTime, .125f);
        EXPECT_EQ(reaction->mObserved->mTime, 1);
    }

    TEST_F(PhysicalPoseAnimationTest, NativeSequenceLayerRejectionPreservesExistingProjectionAndWinningSources)
    {
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        auto first = nativeTrack({1, 2, 3});
        mAnimation->addNativeSource("native-idle", first, 20);
        std::array<MWRender::Animation::NativeSequenceLayer, 1> layers{{{"native-idle"}}};
        ASSERT_TRUE(mAnimation->setNativeSequenceLayers(layers));
        traverse();
        const auto builds = mAnimation->mControllerBuilds;
        const auto callbacks = mBone->getUpdateCallback();
        auto invalid = layers; invalid[0].mEase = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(mAnimation->setNativeSequenceLayers(invalid), std::invalid_argument);
        invalid = layers; invalid[0].mTime = 11;
        EXPECT_THROW(mAnimation->setNativeSequenceLayers(invalid), std::invalid_argument);
        invalid = layers; invalid[0].mBlendMask = 0;
        EXPECT_THROW(mAnimation->setNativeSequenceLayers(invalid), std::invalid_argument);
        invalid = layers; invalid[0].mGroup = "absent";
        EXPECT_FALSE(mAnimation->setNativeSequenceLayers(invalid));
        auto overrideTrack = nativeTrack({99, 99, 99});
        auto overrideSource = mAnimation->addNativeSource("native-idle", overrideTrack, 75);
        overrideSource->mControllerSequences.push_back(overrideSource->mControllerSequences.front());
        EXPECT_FALSE(mAnimation->setNativeSequenceLayers(layers));
        EXPECT_EQ(mAnimation->mControllerBuilds, builds);
        EXPECT_EQ(mBone->getUpdateCallback(), callbacks);
        mFrameStamp->setFrameNumber(1); traverse();
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(1, 2, 3));
        EXPECT_EQ(first->mObserved->mSamples, 2u);
        EXPECT_EQ(overrideTrack->mObserved->mSamples, 0u);
        mAnimation->removeFromScene();
        mFrameStamp->setFrameNumber(2); traverse();
        EXPECT_EQ(first->mObserved->mSamples, 2u);
    }

    TEST_F(PhysicalPoseAnimationTest, NativeSequenceFramesSampleAnimatedPhysicalTargetsAndReleaseWithoutReplay)
    {
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        auto first = nativeTrack({1, 2, 3}); auto second = nativeTrack({11, 12, 13});
        mAnimation->addNativeSource("native-idle", first, 20);
        mAnimation->addNativeSource("native-reaction", second, 75);
        std::array<MWRender::Animation::NativeSequenceLayer, 2> layers{{
            {"native-idle", MWRender::BlendMask_All, 0, 1, 1},
            {"native-reaction", MWRender::BlendMask_All, 0, 1, .25f}}};
        ASSERT_TRUE(mAnimation->setNativeSequenceLayers(layers));
        traverse();
        const auto placement = Nif::NiTransform::getIdentity();
        auto prepared = mAnimation->prepareNativePhysicalPose(mGraph, placement,
            MWRender::PhysicalPoseAnimation::AnimatedTargets);
        ASSERT_TRUE(prepared); ASSERT_TRUE(mAnimation->commitPhysicalPose(*prepared));
        mAnimation->applyPhysicalPose(std::vector<NifBullet::RagdollBoneWorldPose>{
            {8, osg::Matrixf::translate(99, 0, 0)}}, placement);
        const auto physical = mBone->getMatrix();
        layers[1].mEase = .5f;
        ASSERT_TRUE(mAnimation->setNativeSequenceLayers(layers));
        auto sampled = mAnimation->samplePhysicalAnimationTarget(placement, mVisitor);
        ASSERT_EQ(sampled.size(), 1u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(static_cast<float>(sampled[0].mPose.getTrans().x())), 1082829483u);
        EXPECT_EQ(mBone->getMatrix(), physical);
        ASSERT_TRUE(mAnimation->setNativeSequenceLayers({}));
        const auto count = first->mObserved->mSamples;
        mAnimation->endPhysicalPose();
        mAnimation->mWriter->mWriteTranslation = true;
        mFrameStamp->setFrameNumber(1); traverse();
        EXPECT_EQ(first->mObserved->mSamples, count);
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(1, 2, 3));
    }

    TEST_F(PhysicalPoseAnimationTest, NativeSequenceControllerStagesSamplesAndCopiesIndependentFrames)
    {
        auto first = nativeTrack({1, 2, 3}); auto second = nativeTrack({11, 12, 13});
        auto a = std::make_shared<MWRender::NativeSequenceFrame>();
        auto b = std::make_shared<MWRender::NativeSequenceFrame>(); b->mEase = .5f;
        osg::ref_ptr<MWRender::NativeSequenceController> controller = new MWRender::NativeSequenceController(
            {{first, a, 20}, {second, b, 75}}, 0, false);
        second->mThrow = true;
        const auto before = mBone->getMatrix();
        EXPECT_THROW((*controller)(mBone, &mVisitor), std::runtime_error);
        EXPECT_EQ(mBone->getMatrix(), before);
        b->mEase = 0;
        ASSERT_NO_THROW((*controller)(mBone, &mVisitor));
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(1, 2, 3));
        second->mThrow = false; b->mEase = .5f;
        osg::ref_ptr<MWRender::NativeSequenceController> copied
            = osg::clone(controller.get(), osg::CopyOp::SHALLOW_COPY);
        b->mEase = 1;
        (*controller)(mBone, &mVisitor);
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(11, 12, 13));
        (*copied)(mBone, &mVisitor);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(mBone->getControllerTransform().mTranslation.x()), 1082829483u);
    }

    TEST_F(PhysicalPoseAnimationTest, PreparedOwnershipLeavesCallbacksLiveUntilCommitWithoutControllerRebuild)
    {
        traverse();
        const auto matrix = mBone->getMatrix();
        const auto builds = mAnimation->mControllerBuilds;
        osg::ref_ptr<osg::Callback> callback = mBone->getUpdateCallback();
        {
            const auto discarded = mAnimation->preparePhysicalPose(mGraph, Nif::NiTransform::getIdentity(),
                MWRender::PhysicalPoseAnimation::AnimatedTargets);
            EXPECT_TRUE(discarded->isValid());
            EXPECT_EQ(discarded->initialBones().size(), 1u);
            EXPECT_EQ(mBone->getUpdateCallback(), callback);
            EXPECT_FALSE(mAnimation->hasPhysicalPose());
        }
        EXPECT_EQ(mBone->getUpdateCallback(), callback);
        auto prepared = mAnimation->preparePhysicalPose(mGraph, Nif::NiTransform::getIdentity(),
            MWRender::PhysicalPoseAnimation::AnimatedTargets);
        auto competing = mAnimation->preparePhysicalPose(mGraph, Nif::NiTransform::getIdentity());
        // Publishing the already selected controllers must not invoke a
        // potentially throwing subclass's controller-selection hook again.
        mAnimation->mThrowAfterControllers = true;
        ASSERT_TRUE(mAnimation->commitPhysicalPose(*prepared));
        EXPECT_EQ(mAnimation->mControllerBuilds, builds);
        EXPECT_EQ(mBone->getUpdateCallback(), nullptr);
        EXPECT_FALSE(prepared->isValid());
        EXPECT_FALSE(competing->isValid());
        EXPECT_FALSE(mAnimation->commitPhysicalPose(*prepared));
        EXPECT_THROW(prepared->initialBones(), std::invalid_argument);
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
        EXPECT_EQ(std::memcmp(matrix.ptr(), mBone->getMatrix().ptr(), sizeof(double) * 16), 0);
        EXPECT_EQ(mAnimation->samplePhysicalAnimationTarget(Nif::NiTransform::getIdentity(), mVisitor).size(), 1u);
        EXPECT_EQ(mAnimation->mWriter->mWrites, 2u);
        mAnimation->mThrowAfterControllers = false;
        mAnimation->endPhysicalPose();
        EXPECT_FALSE(competing->isValid()); // Cannot revive after release.
        EXPECT_FALSE(mAnimation->commitPhysicalPose(*competing));
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 3u);
    }

    TEST_F(PhysicalPoseAnimationTest, PreparedOwnershipRejectsChangedPoseCallbackTopologyAndExpiredOwner)
    {
        const auto placement = osg::Matrixf::identity();
        auto changedPose = mAnimation->preparePhysicalPose(mGraph, placement);
        mBone->setTranslation({1, 0, 0});
        EXPECT_FALSE(changedPose->isValid());
        EXPECT_FALSE(mAnimation->commitPhysicalPose(*changedPose));
        EXPECT_EQ(mBone->getUpdateCallback(), mAnimation->mWriter);
        auto changedCallback = mAnimation->preparePhysicalPose(mGraph, placement);
        osg::ref_ptr<SceneCounter> counter = new SceneCounter;
        mRoot->addUpdateCallback(counter);
        EXPECT_FALSE(changedCallback->isValid());
        EXPECT_FALSE(mAnimation->commitPhysicalPose(*changedCallback));
        auto changedChildren = mAnimation->preparePhysicalPose(mGraph, placement);
        mRoot->addChild(new osg::Group);
        EXPECT_FALSE(changedChildren->isValid());
        EXPECT_FALSE(mAnimation->commitPhysicalPose(*changedChildren));
        auto foreign = mAnimation->preparePhysicalPose(mGraph, placement);
        osg::ref_ptr<SceneUtil::Skeleton> otherRoot = new SceneUtil::Skeleton;
        osg::ref_ptr<NifOsg::MatrixTransform> otherBone = new NifOsg::MatrixTransform;
        otherRoot->addChild(otherBone);
        TestAnimation other(new osg::Group, otherRoot, otherBone);
        ASSERT_TRUE(foreign->isValid());
        EXPECT_FALSE(other.commitPhysicalPose(*foreign));
        EXPECT_TRUE(foreign->isValid());
        EXPECT_FALSE(mAnimation->hasPhysicalPose());
        // The borrowed animation pointer must never be followed after death.
        auto expired = mAnimation->preparePhysicalPose(mGraph, placement);
        mAnimation.reset();
        EXPECT_FALSE(expired->isValid());
        EXPECT_THROW(expired->initialBones(), std::invalid_argument);
    }


    TEST_F(PhysicalPoseAnimationTest, NativeFramePublicationUsesSampledLocalsAndRestoresRejectedProjections)
    {
        auto placement = Nif::NiTransform::getIdentity();
        EXPECT_THROW(mAnimation->publishNativePhysicalFrame({}, placement), std::logic_error);
        auto prepared = mAnimation->prepareNativePhysicalPose(mGraph, placement,
            MWRender::PhysicalPoseAnimation::AnimatedTargets);
        ASSERT_TRUE(mAnimation->commitPhysicalPose(*prepared));
        EXPECT_EQ(mAnimation->physicalBodyNodeOrder(), (std::vector<std::uint32_t>{8}));
        auto sampled = mAnimation->samplePhysicalAnimationTarget(placement, mVisitor);
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
        std::array<NifBullet::RagdollNativeBlendPublication, 1> publications{{
            {12, 9, osg::Matrixf::translate(10, 20, 30)}}};
        auto result = mAnimation->publishNativePhysicalFrame(publications, placement);
        EXPECT_EQ(result[0].mPose.getTrans(), osg::Vec3f(10, 20, 30));
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(10, 20, 30));
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
        // A missing KF translation channel inherits the preceding animated
        // local, never the last physically written local or drawing projection.
        mAnimation->mWriter->mWriteTranslation = false;
        sampled = mAnimation->samplePhysicalAnimationTarget(placement, mVisitor);
        EXPECT_EQ(sampled[0].mPose.getTrans(), osg::Vec3f(1, 2, 3));
        result = mAnimation->publishNativePhysicalFrame({}, placement);
        EXPECT_EQ(result[0].mPose.getTrans(), osg::Vec3f(1, 2, 3));
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(1, 2, 3));
        publications[0] = {12, 1, osg::Matrixf::translate(50, 60, 70)};
        result = mAnimation->publishNativePhysicalFrame(publications, placement);
        EXPECT_EQ(result[0].mPose.getTrans(), osg::Vec3f(50, 60, 70));
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(1, 2, 3));
        const auto matrix = mBone->getMatrix();
        const auto projection = mBone->getTraversalMatrix();
        publications[0] = {12, 9, osg::Matrixf::scale(2, 2, 2)};
        EXPECT_THROW(mAnimation->publishNativePhysicalFrame(publications, placement), std::invalid_argument);
        EXPECT_EQ(mBone->getMatrix(), matrix);
        EXPECT_EQ(mBone->getTraversalMatrix(), projection);
        EXPECT_EQ(mAnimation->mWriter->mWrites, 2u);
        mAnimation->samplePhysicalAnimationTarget(placement, mVisitor);
        result = mAnimation->publishNativePhysicalFrame({}, placement);
        EXPECT_EQ(result[0].mPose.getTrans(), osg::Vec3f(1, 2, 3));
        EXPECT_EQ(mAnimation->mWriter->mWrites, 3u);
        mAnimation->endPhysicalPose();
        EXPECT_FALSE(mBone->getTraversalMatrix().getMatrix());
    }

    TEST_F(PhysicalPoseAnimationTest, NativePreparationAndAnimatedSamplingRetainOriginalCompositionBits)
    {
        const std::array<std::uint32_t, 9> rotation{0x3f3b0df5u,0x3f1d8dc8u,0x3e974e6du,
            0xbf2d20bcu,0x3f3636ceu,0x3e4259e6u,0xbdbf8f0cu,0xbead547au,0x3f6fb0edu};
        const std::array<std::uint32_t, 3> position{0x41b21a51u,0x41bc4696u,0x42105fb0u};
        auto placement = Nif::NiTransform::getIdentity();
        for (unsigned i = 0; i < 9; ++i)
            placement.mRotation.mValues[i / 3][i % 3] = std::bit_cast<float>(rotation[i]);
        placement.mTranslation = {10,20,30}; placement.mScale = 2;
        mBone->setTranslation({3,4,5});
        mAnimation->mWriter->mTranslation = {3,4,5};
        mAnimation->mWriter->mWriteRotation = false;
        auto prepared = mAnimation->prepareNativePhysicalPose(mGraph, placement,
            MWRender::PhysicalPoseAnimation::AnimatedTargets);
        const auto assertNative = [&](const auto& poses) {
            ASSERT_EQ(poses.size(), 1u);
            for (unsigned i = 0; i < 9; ++i)
                EXPECT_EQ(std::bit_cast<std::uint32_t>(poses[0].mPose(i % 3,i / 3)), rotation[i]);
            for (unsigned i = 0; i < 3; ++i)
                EXPECT_EQ(std::bit_cast<std::uint32_t>(poses[0].mPose(3,i)), position[i]);
        };
        assertNative(prepared->initialBones());
        ASSERT_TRUE(mAnimation->commitPhysicalPose(*prepared));
        const auto before = mBone->getMatrix();
        assertNative(mAnimation->samplePhysicalAnimationTarget(placement, mVisitor));
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
        EXPECT_EQ(mBone->getMatrix(), before);
        assertNative(mAnimation->capturePhysicalPose(placement));
        mAnimation->endPhysicalPose();
    }

    TEST_F(PhysicalPoseAnimationTest, NativePlacementCarriesSeparateUniformScaleThroughOwnership)
    {
        auto placement = Nif::NiTransform::getIdentity();
        placement.mScale = 2.f;
        placement.mTranslation = {100, 20, 30};
        mBone->setTranslation({1, 2, 3});
        const auto initial = mAnimation->beginPhysicalPose(mGraph, placement);
        ASSERT_EQ(initial.size(), 1u);
        EXPECT_EQ(initial[0].mPose.getTrans(), osg::Vec3f(102, 24, 36));
        EXPECT_NEAR(initial[0].mPose(0, 0), 1.f, 1e-6);
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{{8, osg::Matrixf::translate(120, 30, 40)}};
        mAnimation->applyPhysicalPose(physical, placement);
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(10, 5, 5));
        EXPECT_EQ(mAnimation->capturePhysicalPose(placement)[0].mPose.getTrans(), osg::Vec3f(120, 30, 40));
        mAnimation->endPhysicalPose();
        EXPECT_FALSE(mAnimation->hasPhysicalPose());
    }

    TEST_F(PhysicalPoseAnimationTest, NativePlacementSamplingRestoresPhysicsAndReturnsScaledAnimationTarget)
    {
        auto placement = Nif::NiTransform::getIdentity();
        placement.mScale = .5f;
        placement.mTranslation = {100, 20, 30};
        mAnimation->beginPhysicalPose(mGraph, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{{8, osg::Matrixf::translate(120, 30, 40)}};
        mAnimation->applyPhysicalPose(physical, placement);
        const auto before = mBone->getMatrix();
        const auto writes = mAnimation->mWriter->mWrites;
        const auto target = mAnimation->samplePhysicalAnimationTarget(placement, mVisitor);
        ASSERT_EQ(target.size(), 1u);
        EXPECT_EQ(target[0].mPose.getTrans(), osg::Vec3f(100.5f, 21, 31.5f));
        EXPECT_EQ(mAnimation->mWriter->mWrites, writes + 1);
        EXPECT_EQ(std::memcmp(before.ptr(), mBone->getMatrix().ptr(), 16 * sizeof(double)), 0);
        EXPECT_EQ(mAnimation->capturePhysicalPose(placement)[0].mPose.getTrans(), osg::Vec3f(120, 30, 40));
        mAnimation->endPhysicalPose();
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(1, 2, 3));
    }

    TEST_F(PhysicalPoseAnimationTest, NativePlacementRejectsInvalidScaleBeforeControllerDetachOrCallback)
    {
        auto placement = Nif::NiTransform::getIdentity();
        for (float scale : {0.f, -1.f, std::numeric_limits<float>::infinity(),
                 std::numeric_limits<float>::quiet_NaN()})
        {
            placement.mScale = scale;
            EXPECT_THROW(mAnimation->beginPhysicalPose(mGraph, placement), std::invalid_argument);
            EXPECT_FALSE(mAnimation->hasPhysicalPose());
        }
        placement.mScale = 2.f;
        mAnimation->beginPhysicalPose(mGraph, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        const auto before = mBone->getMatrix();
        const auto writes = mAnimation->mWriter->mWrites;
        placement.mScale = -1.f;
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(placement, mVisitor), std::invalid_argument);
        EXPECT_EQ(mAnimation->mWriter->mWrites, writes);
        EXPECT_EQ(std::memcmp(before.ptr(), mBone->getMatrix().ptr(), 16 * sizeof(double)), 0);
        EXPECT_THROW(mAnimation->capturePhysicalPose(placement), std::invalid_argument);
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{{8, osg::Matrixf::identity()}};
        EXPECT_THROW(mAnimation->applyPhysicalPose(physical, placement), std::invalid_argument);
        mAnimation->endPhysicalPose();
    }

    TEST_F(PhysicalPoseAnimationTest, NativeReactionRootUsesLastExactNamedRecordIncludingHiddenNonBoneNodes)
    {
        osg::ref_ptr<osg::Group> duplicate = new osg::Group;
        duplicate->setName("Pelvis");
        duplicate->setUserValue("recordIndex", 21u);
        duplicate->setNodeMask(0);
        mRoot->addChild(duplicate);
        osg::ref_ptr<osg::Group> otherCase = new osg::Group;
        otherCase->setName("pelvis");
        otherCase->setUserValue("recordIndex", 22u);
        mRoot->addChild(otherCase);
        osg::ref_ptr<osg::Group> otherSpace = new osg::Group;
        otherSpace->setName("Pelvis ");
        otherSpace->setUserValue("recordIndex", 23u);
        mRoot->addChild(otherSpace);
        osg::ref_ptr<osg::Group> wrapper = new osg::Group;
        wrapper->setName("Pelvis"); // Renderer wrapper has no native record identity.
        mRoot->addChild(wrapper);
        SceneUtil::ControllerSequenceMetadata sequence;
        sequence.mAccumRootName = "Pelvis";
        mAnimation->bindRoots(std::span(&sequence, 1));
        ASSERT_TRUE(mAnimation->getNativeReactionRootRecord());
        EXPECT_EQ(*mAnimation->getNativeReactionRootRecord(), 21u);
        EXPECT_EQ(mBone->getName(), "Pelvis");
        EXPECT_EQ(duplicate->getNodeMask(), 0u);
    }

    TEST_F(PhysicalPoseAnimationTest, NativeReactionRootMissingFirstSequenceDoesNotUseLaterSequence)
    {
        std::array<SceneUtil::ControllerSequenceMetadata, 2> sequences;
        sequences[0].mAccumRootName = "Missing";
        sequences[1].mAccumRootName = "Pelvis";
        mAnimation->bindRoots(sequences);
        EXPECT_FALSE(mAnimation->getNativeReactionRootRecord());
        mAnimation->bindRoots(std::span(&sequences[1], 1));
        EXPECT_FALSE(mAnimation->getNativeReactionRootRecord());
    }

    TEST_F(PhysicalPoseAnimationTest, NativeReactionRootEmptySequenceSetDoesNotConsumeFirstSlot)
    {
        mAnimation->bindRoots({});
        EXPECT_FALSE(mAnimation->getNativeReactionRootRecord());
        osg::ref_ptr<osg::Group> attachedPart = new osg::Group;
        attachedPart->setName("Pelvis");
        attachedPart->setUserValue("recordIndex", 39u);
        mRoot->addChild(attachedPart); // Added after native model palette construction.
        SceneUtil::ControllerSequenceMetadata sequence;
        sequence.mAccumRootName = "Pelvis";
        mAnimation->bindRoots(std::span(&sequence, 1));
        EXPECT_EQ(mAnimation->getNativeReactionRootRecord(), 8u);
    }

    TEST_F(PhysicalPoseAnimationTest, NativeReactionRootAbsentAuthoredNameUsesModelRecordName)
    {
        mRoot->setName("RendererWrapper");
        osg::ref_ptr<osg::Group> model = new osg::Group;
        model->setName("ModelRoot");
        model->setUserValue("recordIndex", 0u);
        mRoot->removeChild(mBone);
        model->addChild(mBone);
        mRoot->addChild(model);
        SceneUtil::ControllerSequenceMetadata sequence;
        mAnimation->bindRoots(std::span(&sequence, 1));
        EXPECT_EQ(mAnimation->getNativeReactionRootRecord(), 0u);
    }

    TEST_F(PhysicalPoseAnimationTest, NativeReactionRootRetainsSelectionAndSceneRemovalReleasesIt)
    {
        SceneUtil::ControllerSequenceMetadata sequence;
        sequence.mAccumRootName = "Pelvis";
        mAnimation->bindRoots(std::span(&sequence, 1));
        EXPECT_EQ(mAnimation->getNativeReactionRootRecord(), 8u);
        mBone->setName("Changed");
        osg::ref_ptr<osg::Group> replacement = new osg::Group;
        replacement->setName("Pelvis");
        replacement->setUserValue("recordIndex", 29u);
        mRoot->addChild(replacement);
        mAnimation->bindRoots(std::span(&sequence, 1));
        EXPECT_EQ(mAnimation->getNativeReactionRootRecord(), 8u);
        mAnimation->removeFromScene();
        EXPECT_FALSE(mAnimation->getNativeReactionRootRecord());
    }

    TEST_F(PhysicalPoseAnimationTest, PhysicalOwnershipDetachesResidentAncestorKeyframesAndRestoresThem)
    {
        osg::ref_ptr<NifOsg::MatrixTransform> ancestor = new NifOsg::MatrixTransform;
        ancestor->setScale(1);
        ancestor->setRotation(osg::Quat());
        ancestor->setTranslation({0, 0, 0});
        ancestor->setUserValue("recordIndex", 17u);
        mRoot->removeChild(mBone);
        ancestor->addChild(mBone);
        mRoot->addChild(ancestor);
        osg::ref_ptr<ResidentKeyframe> resident = new ResidentKeyframe;
        osg::ref_ptr<SceneCounter> counter = new SceneCounter;
        ancestor->addUpdateCallback(resident);
        ancestor->addUpdateCallback(counter);
        const auto placement = osg::Matrixf::identity();
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{
            {8, osg::Matrixf::translate(20, 5, 6)}};
        for (auto mode : {MWRender::PhysicalPoseAnimation::Frozen, MWRender::PhysicalPoseAnimation::AnimatedTargets})
        {
            ancestor->setMatrix(osg::Matrixd::rotate(.3, osg::Vec3d(0, 0, 1)));
            const auto before = ancestor->getMatrix();
            const auto writes = resident->mWrites;
            mRoot->setNodeMask(0);
            mAnimation->beginPhysicalPose(mGraph, placement, mode);
            mRoot->setNodeMask(~0u);
            mAnimation->applyPhysicalPose(physical, placement);
            if (mode == MWRender::PhysicalPoseAnimation::AnimatedTargets)
            {
                EXPECT_EQ(mAnimation->samplePhysicalAnimationTarget(placement, mVisitor)[0].mPose.getTrans(),
                    osg::Vec3f(1, 2, 3));
                EXPECT_EQ(resident->mWrites, writes + 1);
            }
            const auto afterSample = resident->mWrites;
            traverse();
            EXPECT_EQ(resident->mWrites, afterSample);
            EXPECT_EQ(std::memcmp(before.ptr(), ancestor->getMatrix().ptr(), sizeof(double) * 16), 0);
            const auto captured = mAnimation->capturePhysicalPose(placement);
            for (unsigned r = 0; r < 4; ++r)
                for (unsigned c = 0; c < 4; ++c)
                    EXPECT_NEAR(captured[0].mPose(r, c), physical[0].mPose(r, c), 1e-4);
            mAnimation->endPhysicalPose();
            traverse();
            EXPECT_EQ(resident->mWrites, afterSample + 1);
        }
        EXPECT_EQ(counter->mVisits, 4u);
    }

    TEST_F(PhysicalPoseAnimationTest, AnimatedControllerRebuildPreservesPhysicsAndDetachesOnFailure)
    {
        traverse();
        mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity(), MWRender::PhysicalPoseAnimation::AnimatedTargets);
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{
            {8, osg::Matrixf::translate(20, 5, 6)}};
        mAnimation->applyPhysicalPose(physical, osg::Matrixf::identity());
        const auto matrix = mBone->getMatrix();
        mAnimation->rebuildControllers();
        EXPECT_EQ(std::memcmp(matrix.ptr(), mBone->getMatrix().ptr(), sizeof(double) * 16), 0);
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
        mAnimation->mThrowAfterControllers = true;
        EXPECT_THROW(mAnimation->rebuildControllers(), std::runtime_error);
        EXPECT_EQ(std::memcmp(matrix.ptr(), mBone->getMatrix().ptr(), sizeof(double) * 16), 0);
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
        mAnimation->mThrowAfterControllers = false;
        mAnimation->rebuildControllers();
        EXPECT_EQ(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), mVisitor)[0].mPose.getTrans(),
            osg::Vec3f(1, 2, 3));
    }

    TEST_F(PhysicalPoseAnimationTest, AnimatedSamplingHonorsAncestorMasksAndNodeMaskOverride)
    {
        traverse();
        mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity(), MWRender::PhysicalPoseAnimation::AnimatedTargets);
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{
            {8, osg::Matrixf::translate(20, 5, 6)}};
        mAnimation->applyPhysicalPose(physical, osg::Matrixf::identity());
        mAnimation->mWriter->mTranslation = {7, 8, 9};
        mRoot->setNodeMask(2);
        mVisitor.setTraversalMask(1);
        EXPECT_EQ(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), mVisitor)[0].mPose.getTrans(),
            osg::Vec3f(1, 2, 3));
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
        mVisitor.setNodeMaskOverride(1);
        EXPECT_EQ(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), mVisitor)[0].mPose.getTrans(),
            osg::Vec3f(7, 8, 9));
        EXPECT_EQ(mAnimation->mWriter->mWrites, 2u);
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(20, 5, 6));
    }

    TEST_F(PhysicalPoseAnimationTest, AnimatedSamplingCapturesTargetsAndPreservesPhysicalRendererAndSkinning)
    {
        traverse();
        const auto placement = osg::Matrixf::translate(100, 0, 0);
        mAnimation->beginPhysicalPose(mGraph, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{{8, osg::Matrixf::translate(120, 5, 6)}};
        mAnimation->applyPhysicalPose(physical, placement);
        const auto before = mBone->getMatrix();
        const auto cache = mBone->mRotationScale;
        mFrameStamp->setSimulationTime(.125);
        mVisitor.setTraversalNumber(91);
        const auto sampled = mAnimation->samplePhysicalAnimationTarget(placement, mVisitor);
        ASSERT_EQ(sampled.size(), 1);
        EXPECT_EQ(sampled[0].mPose.getTrans(), osg::Vec3f(101, 2, 3));
        EXPECT_EQ(mAnimation->mWriter->mWrites, 2);
        EXPECT_EQ(mAnimation->mWriter->mObservedSimulationTime, .125);
        EXPECT_EQ(mAnimation->mWriter->mObservedTraversal, 91u);
        EXPECT_EQ(mAnimation->mWriter->mObservedPathLength, 2u);
        EXPECT_EQ(std::memcmp(mBone->getMatrix().ptr(), before.ptr(), 16 * sizeof(double)), 0);
        EXPECT_EQ(std::memcmp(mBone->mRotationScale.mValues, cache.mValues, 9 * sizeof(float)), 0);
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 2);
        auto* bone = mRoot->getBone("Pelvis");
        ASSERT_NE(bone, nullptr);
        mRoot->updateBoneMatrices(0);
        EXPECT_EQ(bone->mMatrixInSkeletonSpace.getTrans(), osg::Vec3f(20, 5, 6));
        EXPECT_EQ(mAnimation->capturePhysicalPose(placement)[0].mPose.getTrans(), osg::Vec3f(120, 5, 6));
    }

    TEST_F(PhysicalPoseAnimationTest, AnimatedSamplingPreservesUndefinedChannelsFromAnimatedSource)
    {
        traverse();
        const auto placement = osg::Matrixf::translate(100, 0, 0);
        const auto animated = mBone->getMatrix();
        mAnimation->beginPhysicalPose(mGraph, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        mAnimation->applyPhysicalPose(std::vector<NifBullet::RagdollBoneWorldPose>{{8,
            osg::Matrixf::translate(120, 5, 6)}}, placement);
        mAnimation->mWriter->mWriteTranslation = false;
        mAnimation->mWriter->mWriteRotation = false;
        const auto target = mAnimation->samplePhysicalAnimationTarget(placement, mVisitor);
        EXPECT_EQ(target[0].mPose.getTrans(), osg::Vec3f(101, 2, 3));
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned column = 0; column < 3; ++column)
                EXPECT_FLOAT_EQ(target[0].mPose(row, column), float(animated(row, column)));
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(20, 5, 6));
    }

    TEST_F(PhysicalPoseAnimationTest, AnimatedSamplingDoesNotTraverseSceneChildrenAndRollsBackInvalidCallbacks)
    {
        traverse();
        osg::ref_ptr<osg::Group> child = new osg::Group;
        osg::ref_ptr<SceneCounter> counter = new SceneCounter;
        mBone->addChild(child);
        child->setUpdateCallback(counter);
        const auto placement = osg::Matrixf::translate(100, 0, 0);
        mAnimation->beginPhysicalPose(mGraph, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        mAnimation->applyPhysicalPose(std::vector<NifBullet::RagdollBoneWorldPose>{{8,
            osg::Matrixf::translate(120, 5, 6)}}, placement);
        const auto before = mBone->getMatrix();
        mAnimation->mWriter->mInvalidTranslation = true;
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(placement, mVisitor), std::invalid_argument);
        EXPECT_EQ(mBone->getMatrix(), before);
        mAnimation->mWriter->mInvalidTranslation = false;
        mAnimation->mWriter->mThrowAfterWrite = true;
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(placement, mVisitor), std::runtime_error);
        EXPECT_EQ(mBone->getMatrix(), before);
        mAnimation->mWriter->mThrowAfterWrite = false;
        EXPECT_EQ(mAnimation->samplePhysicalAnimationTarget(placement, mVisitor)[0].mPose.getTrans(),
            osg::Vec3f(101, 2, 3));
        EXPECT_EQ(mBone->getMatrix(), before);
        EXPECT_EQ(counter->mVisits, 0u);
        traverse();
        EXPECT_EQ(counter->mVisits, 1u);
    }

    TEST_F(PhysicalPoseAnimationTest, EndingAnimatedPhysicalModeRestoresLatestSampleAndReattachesControllers)
    {
        traverse();
        const auto placement = osg::Matrixf::translate(100, 0, 0);
        mAnimation->beginPhysicalPose(mGraph, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        mAnimation->applyPhysicalPose(std::vector<NifBullet::RagdollBoneWorldPose>{{8,
            osg::Matrixf::translate(120, 5, 6)}}, placement);
        mAnimation->mWriter->mTranslation = {2, 4, 6};
        mAnimation->samplePhysicalAnimationTarget(placement, mVisitor);
        mAnimation->endPhysicalPose();
        EXPECT_FALSE(mAnimation->hasPhysicalPose());
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(2, 4, 6));
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 3);
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(2, 4, 6));
    }

    TEST_F(PhysicalPoseAnimationTest, SamplingRejectsUnboundFrozenAndWrongVisitorAndInvalidMode)
    {
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), mVisitor), std::logic_error);
        EXPECT_THROW(mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity(),
            static_cast<MWRender::PhysicalPoseAnimation>(99)), std::invalid_argument);
        EXPECT_FALSE(mAnimation->hasPhysicalPose());
        mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity());
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), mVisitor), std::logic_error);
        mAnimation->endPhysicalPose();
        mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity(), MWRender::PhysicalPoseAnimation::AnimatedTargets);
        osg::NodeVisitor invalid;
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), invalid), std::invalid_argument);
        osgUtil::UpdateVisitor noStamp;
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), noStamp), std::invalid_argument);
        mFrameStamp->setSimulationTime(std::numeric_limits<double>::quiet_NaN());
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::identity(), mVisitor), std::invalid_argument);
        mFrameStamp->setSimulationTime(0);
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(osg::Matrixf::scale(2, 2, 2), mVisitor), std::invalid_argument);
        EXPECT_EQ(mAnimation->mWriter->mWrites, 0);
    }

    TEST_F(PhysicalPoseAnimationTest, AnimatedSamplingExecutesParentBeforeChildDespiteRegistrationOrder)
    {
        osg::ref_ptr<NifOsg::MatrixTransform> child = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        child->setName("Child");
        child->setUserValue("recordIndex", 18u);
        mBone->addChild(child);
        osg::ref_ptr<PoseWriter> reader = new PoseWriter;
        reader->mReadParent = true;
        mAnimation->mBeforeControllers.emplace_back(child, reader);
        mAnimation->rebuildControllers();
        traverse();
        const auto placement = osg::Matrixf::translate(100, 0, 0);
        mAnimation->beginPhysicalPose(mGraph, placement, MWRender::PhysicalPoseAnimation::AnimatedTargets);
        mAnimation->applyPhysicalPose(std::vector<NifBullet::RagdollBoneWorldPose>{{8,
            osg::Matrixf::translate(120, 5, 6)}}, placement);
        mAnimation->mWriter->mTranslation = {3, 4, 5};
        mAnimation->samplePhysicalAnimationTarget(placement, mVisitor);
        EXPECT_EQ(reader->mObservedParentX, 3);
        EXPECT_EQ(reader->mWrites, 2u);
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3d(20, 5, 6));
        traverse();
        EXPECT_EQ(reader->mWrites, 2u);
    }

    TEST_F(PhysicalPoseAnimationTest, PhysicalSnapshotOwnsBonesAcrossAnimationAndSceneTraversal)
    {
        traverse();
        ASSERT_EQ(mAnimation->mWriter->mWrites, 1);
        const auto placement = osg::Matrixf::translate(100, 0, 0);
        const auto initial = mAnimation->beginPhysicalPose(mGraph, placement);
        ASSERT_EQ(initial.size(), 1);
        EXPECT_EQ(initial[0].mPose.getTrans(), osg::Vec3f(101, 2, 3));
        EXPECT_TRUE(mAnimation->hasPhysicalPose());
        const std::vector<NifBullet::RagdollBoneWorldPose> physical{{8, osg::Matrixf::translate(120, 5, 6)}};
        mAnimation->applyPhysicalPose(physical, placement);
        mAnimation->rebuildControllers();
        EXPECT_EQ(mAnimation->runAnimation(.25f), osg::Vec3f());
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1);
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3f(20, 5, 6));
        EXPECT_EQ(mAnimation->capturePhysicalPose(placement)[0].mPose.getTrans(), physical[0].mPose.getTrans());
        mAnimation->endPhysicalPose();
        EXPECT_FALSE(mAnimation->hasPhysicalPose());
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 2);
        EXPECT_EQ(mBone->getMatrix().getTrans(), osg::Vec3f(1, 2, 3));
        mAnimation->endPhysicalPose();
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 3);
    }

    TEST_F(PhysicalPoseAnimationTest, FailedAdmissionLeavesAnimationControllerAttached)
    {
        auto invalid = mGraph;
        invalid.mSourceHash = "different-asset";
        EXPECT_THROW(mAnimation->beginPhysicalPose(invalid, osg::Matrixf::identity()), std::invalid_argument);
        EXPECT_FALSE(mAnimation->hasPhysicalPose());
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1);
        EXPECT_THROW(mAnimation->capturePhysicalPose(osg::Matrixf::identity()), std::logic_error);
        const std::vector<NifBullet::RagdollBoneWorldPose> empty;
        EXPECT_THROW(mAnimation->applyPhysicalPose(empty, osg::Matrixf::identity()), std::logic_error);
    }

    TEST_F(PhysicalPoseAnimationTest, InvalidSnapshotAndDuplicateAdmissionPreservePhysicalOwnership)
    {
        mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity());
        EXPECT_THROW(mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity()), std::logic_error);
        const std::vector<NifBullet::RagdollBoneWorldPose> empty;
        EXPECT_THROW(mAnimation->applyPhysicalPose(empty, osg::Matrixf::identity()), std::invalid_argument);
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 0);
        EXPECT_TRUE(mAnimation->hasPhysicalPose());
        mAnimation->endPhysicalPose();
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1);
    }

    TEST_F(PhysicalPoseAnimationTest, SceneRemovalReleasesBindingWithoutReattachingControllers)
    {
        mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity());
        mAnimation->removeFromScene();
        EXPECT_FALSE(mAnimation->hasPhysicalPose());
        EXPECT_EQ(mParent->getNumChildren(), 0);
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 0);
    }
}

namespace
{
    TEST_F(PhysicalPoseAnimationTest, EndPhysicalPoseReleasesOwnershipWhenHierarchyChanged)
    {
        mAnimation->beginPhysicalPose(mGraph, osg::Matrixf::identity(), MWRender::PhysicalPoseAnimation::AnimatedTargets);
        mRoot->removeChild(mBone);
        EXPECT_THROW(mAnimation->endPhysicalPose(), std::invalid_argument);
        EXPECT_FALSE(mAnimation->hasPhysicalPose());
        mRoot->addChild(mBone);
        EXPECT_NO_THROW(mAnimation->endPhysicalPose());
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
    }
}

namespace
{
    class NativeRootProperty : public osg::StateSet::Callback
    {
    public:
        unsigned mVisits = 0;
        std::size_t mPathLength = 0;
        void operator()(osg::StateSet*, osg::NodeVisitor* visitor) override
        { ++mVisits; mPathLength = visitor->getNodePath().size(); }
    };

    TEST_F(PhysicalPoseAnimationTest, NativeScenePreparationCommitsLiveCallbacksAndRetiresWithoutRebuilding)
    {
        osg::ref_ptr<SceneCounter> rootCallback = new SceneCounter;
        osg::ref_ptr<NativeRootProperty> rootProperty = new NativeRootProperty, boneProperty = new NativeRootProperty;
        mRoot->setUpdateCallback(rootCallback);
        mRoot->getOrCreateStateSet()->setUpdateCallback(rootProperty);
        mBone->getOrCreateStateSet()->setUpdateCallback(boneProperty);
        auto placement = Nif::NiTransform::getIdentity(); placement.mTranslation = {100, 0, 0};
        unsigned frames = 0, nodes = 0;
        mFrameStamp->setSimulationTime(4.5); mFrameStamp->setReferenceTime(10);
        mVisitor.setTraversalNumber(17);
        auto source = [&](const MWWorld::Ptr& actor, const osg::FrameStamp& stamp, unsigned traversal) {
            ++frames; EXPECT_TRUE(actor.isEmpty()); EXPECT_EQ(stamp.getSimulationTime(), 4.5); EXPECT_EQ(traversal, 17u);
            MWRender::Animation::NativeSceneFrame frame; frame.mPlacement = placement;
            frame.mCollision = [&](std::uint32_t record, const Nif::NiTransform& animated) {
                ++nodes; EXPECT_EQ(record, 8u); EXPECT_EQ(animated.mTranslation, osg::Vec3f(101, 2, 3));
                auto target = Nif::NiTransform::getIdentity(); target.mTranslation = {140, 0, 0};
                return SceneUtil::NifSceneTraversal::CollisionResult{9, target};
            };
            return frame;
        };
        const auto builds = mAnimation->mControllerBuilds;
        auto prepared = mAnimation->prepareNativeScene(mGraph, placement, source);
        auto competing = mAnimation->prepareNativeScene(mGraph, placement, source);
        auto generic = mAnimation->preparePhysicalPose(mGraph, placement);
        ASSERT_TRUE(prepared->isValid()); EXPECT_EQ(prepared->initialBones().size(), 1u);
        EXPECT_EQ(mRoot->getUpdateCallback(), rootCallback); EXPECT_EQ(mBone->getUpdateCallback(), mAnimation->mWriter);
        EXPECT_FALSE(mBone->getTraversalMatrix().getMatrix()); EXPECT_EQ(frames, 0u);
        ASSERT_TRUE(mAnimation->commitNativeScene(*prepared));
        EXPECT_FALSE(prepared->isValid()); EXPECT_FALSE(competing->isValid()); EXPECT_FALSE(generic->isValid());
        EXPECT_TRUE(mAnimation->hasNativeScene()); EXPECT_TRUE(mAnimation->hasPhysicalPose());
        EXPECT_EQ(mAnimation->mControllerBuilds, builds);
        traverse();
        EXPECT_EQ(frames, 1u); EXPECT_EQ(nodes, 1u); EXPECT_EQ(rootProperty->mVisits, 1u);
        EXPECT_EQ(boneProperty->mVisits, 1u); EXPECT_EQ(rootCallback->mVisits, 1u);
        EXPECT_EQ(mAnimation->mWriter->mWrites, 1u); EXPECT_EQ(mAnimation->mWriter->mObservedSimulationTime, 4.5);
        EXPECT_EQ(mAnimation->mWriter->mObservedTraversal, 17u);
        EXPECT_EQ(mBone->getControllerTransform().mTranslation, osg::Vec3f(40, 0, 0));
        EXPECT_NEAR(mBone->getWorldMatrices()[0].getTrans().x(), 140, 1e-9);
        mAnimation->endPhysicalPose();
        EXPECT_FALSE(mAnimation->hasPhysicalPose()); EXPECT_FALSE(mBone->getTraversalMatrix().getMatrix());
        EXPECT_EQ(mRoot->getUpdateCallback(), rootCallback); EXPECT_EQ(mBone->getUpdateCallback(), mAnimation->mWriter);
        EXPECT_EQ(mAnimation->mControllerBuilds, builds);
        traverse();
        EXPECT_EQ(frames, 1u); EXPECT_EQ(mAnimation->mWriter->mWrites, 2u);
    }

    TEST_F(PhysicalPoseAnimationTest, NativeSceneKeepsPreviousPhysicalLocalsAcrossPartialCallbacksAndSelectionRebuild)
    {
        auto placement = Nif::NiTransform::getIdentity(); placement.mTranslation = {100, 0, 0};
        bool target = true;
        auto prepared = mAnimation->prepareNativeScene(mGraph, placement, [&](const auto&, const auto&, unsigned) {
            MWRender::Animation::NativeSceneFrame frame; frame.mPlacement = placement;
            frame.mCollision = [&](auto, const auto&) {
                auto desired = Nif::NiTransform::getIdentity(); desired.mTranslation = {140, 0, 0};
                return SceneUtil::NifSceneTraversal::CollisionResult{9, target ? std::optional(desired) : std::nullopt};
            };
            return frame;
        });
        ASSERT_TRUE(mAnimation->commitNativeScene(*prepared));
        traverse();
        EXPECT_EQ(mBone->getControllerTransform().mTranslation.x(), 40);
        target = false; mAnimation->mWriter->mWriteTranslation = false; mAnimation->mWriter->mWriteRotation = false;
        mAnimation->rebuildControllers();
        traverse();
        EXPECT_EQ(mAnimation->mWriter->mObservedLocalBefore.x(), 40);
        EXPECT_EQ(mBone->getControllerTransform().mTranslation.x(), 40);
        EXPECT_NEAR(mBone->getWorldMatrices()[0].getTrans().x(), 140, 1e-9);
        EXPECT_EQ(mAnimation->mWriter->mWrites, 2u);
        EXPECT_THROW(mAnimation->samplePhysicalAnimationTarget(placement, mVisitor), std::logic_error);
        mAnimation->endNativeScene();
    }

    TEST_F(PhysicalPoseAnimationTest, NativeSceneForestUsesAssetScopedRecordsAndOnePlacementForIndependentRoots)
    {
        osg::ref_ptr<NifOsg::MatrixTransform> other = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        other->setName("Other"); other->setTranslation({2, 0, 0}); other->setUserValue("recordIndex", 9u);
        auto otherBody = mGraph.mBodies.front();
        otherBody.mRecord = 13; otherBody.mNodeRecord = 9; otherBody.mBone = "Other";
        mGraph.mBodies.push_back(otherBody);
        osg::ref_ptr<osg::Group> foreignAsset = new osg::Group;
        foreignAsset->setUserValue(Misc::OsgUserValues::sFileHash, std::string("foreign-part"));
        osg::ref_ptr<NifOsg::MatrixTransform> foreign = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        foreign->setName("Pelvis"); foreign->setTranslation({7, 0, 0}); foreign->setUserValue("recordIndex", 8u);
        foreignAsset->addChild(foreign); mRoot->addChild(other); mRoot->addChild(foreignAsset);
        auto placement = Nif::NiTransform::getIdentity(); placement.mTranslation = {100, 0, 0};
        std::vector<std::uint32_t> records;
        auto prepared = mAnimation->prepareNativeScene(mGraph, placement, [&](const auto&, const auto&, unsigned) {
            MWRender::Animation::NativeSceneFrame frame; frame.mPlacement = placement;
            frame.mCollision = [&](std::uint32_t record, const auto&) {
                records.push_back(record); return SceneUtil::NifSceneTraversal::CollisionResult{};
            };
            return frame;
        });
        ASSERT_TRUE(mAnimation->commitNativeScene(*prepared)); traverse();
        EXPECT_EQ(records, (std::vector<std::uint32_t>{8, 9}));
        EXPECT_NEAR(other->getWorldMatrices()[0].getTrans().x(), 102, 1e-9);
        EXPECT_NEAR(foreign->getWorldMatrices()[0].getTrans().x(), 107, 1e-9);
        mAnimation->endNativeScene();
        EXPECT_FALSE(other->getTraversalMatrix().getMatrix()); EXPECT_FALSE(foreign->getTraversalMatrix().getMatrix());
    }

    TEST_F(PhysicalPoseAnimationTest, NativeScenePreparationRejectsStaleIdentityAndCallbackChangesBeforePublication)
    {
        unsigned frames = 0;
        auto source = [&](const auto&, const auto&, unsigned) { ++frames; return MWRender::Animation::NativeSceneFrame{}; };
        auto prepared = mAnimation->prepareNativeScene(mGraph, Nif::NiTransform::getIdentity(), source);
        mBone->setUserValue("recordIndex", 99u);
        EXPECT_FALSE(prepared->isValid()); EXPECT_FALSE(mAnimation->commitNativeScene(*prepared));
        mBone->setUserValue("recordIndex", 8u);
        auto second = mAnimation->prepareNativeScene(mGraph, Nif::NiTransform::getIdentity(), source);
        osg::ref_ptr<SceneCounter> tail = new SceneCounter; mBone->addUpdateCallback(tail);
        EXPECT_FALSE(second->isValid()); EXPECT_FALSE(mAnimation->commitNativeScene(*second));
        EXPECT_EQ(frames, 0u); EXPECT_FALSE(mBone->getTraversalMatrix().getMatrix());
        EXPECT_EQ(mBone->getUpdateCallback(), mAnimation->mWriter);
    }

    TEST_F(PhysicalPoseAnimationTest, NativeSceneRejectsReentrantReleaseAndInvalidPlacementBeforeBoneCallbacks)
    {
        bool reenter = true, invalid = false;
        unsigned frames = 0;
        auto prepared = mAnimation->prepareNativeScene(mGraph, Nif::NiTransform::getIdentity(), [&](const auto&, const auto&, unsigned) {
            ++frames;
            if (reenter) mAnimation->endNativeScene();
            MWRender::Animation::NativeSceneFrame frame;
            if (invalid) frame.mPlacement.mScale = 0;
            return frame;
        });
        ASSERT_TRUE(mAnimation->commitNativeScene(*prepared));
        EXPECT_THROW(traverse(), std::logic_error); EXPECT_TRUE(mAnimation->hasNativeScene());
        EXPECT_EQ(mAnimation->mWriter->mWrites, 0u);
        reenter = false; invalid = true;
        EXPECT_THROW(traverse(), std::invalid_argument); EXPECT_EQ(mAnimation->mWriter->mWrites, 0u);
        invalid = false; traverse();
        EXPECT_EQ(frames, 3u); EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
        mAnimation->endNativeScene();
    }

    TEST_F(PhysicalPoseAnimationTest, NativeSceneReleasePreservesForeignCallbacksAndProjectionStorage)
    {
        auto prepared = mAnimation->prepareNativeScene(mGraph, Nif::NiTransform::getIdentity(),
            [](const auto&, const auto&, unsigned) { return MWRender::Animation::NativeSceneFrame{}; });
        ASSERT_TRUE(mAnimation->commitNativeScene(*prepared)); traverse();
        const NifOsg::MatrixTransform::TraversalMatrix foreign(osg::Matrixd(mBone->getMatrix()));
        mBone->setTraversalMatrix(foreign);
        osg::ref_ptr<SceneCounter> replacement = new SceneCounter; mRoot->setUpdateCallback(replacement);
        mAnimation->endNativeScene();
        EXPECT_EQ(mRoot->getUpdateCallback(), replacement);
        EXPECT_TRUE(mBone->getTraversalMatrix().sharesStorageWith(foreign));
    }

    TEST_F(PhysicalPoseAnimationTest, NativeScenePreparedAndPublishedHandlesRemainSafeAfterAnimationDestruction)
    {
        auto prepared = mAnimation->prepareNativeScene(mGraph, Nif::NiTransform::getIdentity(),
            [](const auto&, const auto&, unsigned) { return MWRender::Animation::NativeSceneFrame{}; });
        auto stale = mAnimation->prepareNativeScene(mGraph, Nif::NiTransform::getIdentity(),
            [](const auto&, const auto&, unsigned) { return MWRender::Animation::NativeSceneFrame{}; });
        auto writer = mAnimation->mWriter;
        ASSERT_TRUE(mAnimation->commitNativeScene(*prepared)); traverse();
        mAnimation.reset();
        EXPECT_FALSE(prepared->isValid()); EXPECT_FALSE(stale->isValid());
        EXPECT_THROW(stale->initialBones(), std::invalid_argument);
        EXPECT_FALSE(mBone->getTraversalMatrix().getMatrix());
        traverse(); EXPECT_EQ(writer->mWrites, 2u);
    }
}

namespace
{
    TEST_F(PhysicalPoseAnimationTest, NativeScenePrePhysicsTraversalsDriveOwnedBodyDeferralSyncAndPhysicalContinuation)
    {
        btDefaultCollisionConfiguration configuration;
        btCollisionDispatcher dispatcher(&configuration);
        btDbvtBroadphase broadphase;
        btSequentialImpulseConstraintSolver solver;
        btDiscreteDynamicsWorld world(&dispatcher, &broadphase, &solver, &configuration);
        world.setGravity(btVector3(0, 0, 0));
        auto& definition = mGraph.mBodies[0];
        definition.mMass = 2; definition.mInertia = {1,0,0,0,1,0,0,0,1};
        definition.mLinearDamping = definition.mAngularDamping = 2;
        definition.mFriction = .6f; definition.mRestitution = .2f;
        definition.mShape = NifBullet::RagdollSphere{.5f};
        definition.mBlend = NifBullet::RagdollBlendDefinition{30, 0x49, 0, 0};
        auto placement = Nif::NiTransform::getIdentity(); placement.mTranslation = {100, 0, 0};
        NifBullet::ActorRagdollPhysics* actor = nullptr;
        unsigned selector = 2, frames = 0, collisions = 0;
        auto prepared = mAnimation->prepareNativeScene(mGraph, placement, [&](const auto&, const auto&, unsigned) {
            ++frames;
            MWRender::Animation::NativeSceneFrame frame; frame.mPlacement = placement;
            frame.mCollision = [&](std::uint32_t record, const Nif::NiTransform& animated) {
                ++collisions;
                EXPECT_EQ(record, 8u);
                auto matrix = animated.mRotation.toOsgMatrix(); matrix.setTrans(animated.mTranslation);
                const auto publication = actor->updateNativeBlendSceneNode(record, matrix, 1.f / 60, selector, 0);
                SceneUtil::NifSceneTraversal::CollisionResult result{
                    actor->captureNativeBlendStates()[0].mCollisionFlags, std::nullopt};
                if (publication && publication->mSceneTarget)
                {
                    auto target = Nif::NiTransform::getIdentity(); target.mTranslation = publication->mSceneTarget->getTrans();
                    for (unsigned r = 0; r < 3; ++r) for (unsigned c = 0; c < 3; ++c)
                        target.mRotation.mValues[r][c] = (*publication->mSceneTarget)(c, r);
                    result.mPhysicalTarget = target;
                }
                return result;
            };
            return frame;
        });
        const auto poses = NifBullet::ragdollBodyWorldPoses(mGraph, prepared->initialBones(), Nif::NativeTransformPrecision::Single);
        NifBullet::ActorRagdollPhysics owned(mGraph, world, NifBullet::RagdollNativeLengthScale, poses, 1, -1);
        actor = &owned;
        auto states = owned.captureNativeBlendStates(); states[0].mRequestedMotion = 1; owned.restoreNativeBlendStates(states);
        auto packed = owned.captureNativePackedVelocities();
        packed[0].mVelocities.mLinear = {1,2,3,4}; packed[0].mVelocities.mAngular = {5,6,7,8};
        owned.restoreNativePackedVelocities(packed);
        auto* rigid = btRigidBody::upcast(owned.collisionObjects()[0]); rigid->setActivationState(ISLAND_SLEEPING);
        const auto before = owned.capture()[0];
        ASSERT_TRUE(mAnimation->commitNativeScene(*prepared));
        unsigned renderFrame = 0;
        auto update = [&] {
            mFrameStamp->setFrameNumber(++renderFrame);
            EXPECT_TRUE(mAnimation->updateNativeSceneBeforePhysics(*mFrameStamp, renderFrame));
            SceneUtil::PostPhysicsUpdateVisitor post;
            post.setFrameStamp(mFrameStamp); post.setTraversalNumber(renderFrame);
            mRoot->accept(post);
        };
        for (const unsigned phase : {2u, 3u})
        {
            selector = phase; update();
            EXPECT_EQ(owned.captureNativeBlendStates()[0].mCollisionFlags, 0x49);
            EXPECT_EQ(owned.capture()[0].mPose, before.mPose); EXPECT_FALSE(rigid->isActive());
            EXPECT_NEAR(mBone->getWorldMatrices()[0].getTrans().x(), 101, 1e-5);
        }
        selector = 0; update();
        EXPECT_EQ(owned.captureNativeBlendStates()[0].mCollisionFlags, 9);
        EXPECT_NEAR(owned.capture()[0].mPose.getOrigin().x(), 101, 1e-4);
        EXPECT_EQ(owned.captureNativePackedVelocities()[0].mVelocities, packed[0].mVelocities);
        EXPECT_EQ(owned.captureNativeBlendStates()[0].mRequestedMotion, 1u);
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        auto moved = rigid->getWorldTransform(); moved.setOrigin(btVector3(200, 0, 0)); rigid->setCenterOfMassTransform(moved);
        update(); update();
        EXPECT_EQ(frames, 5u); EXPECT_EQ(collisions, 5u); EXPECT_EQ(mAnimation->mWriter->mWrites, 5u);
        EXPECT_NEAR(mBone->getWorldMatrices()[0].getTrans().x(), 200, 1e-3);
        EXPECT_NEAR(mAnimation->mWriter->mObservedLocalBefore.x(), 100, 1e-3);
        EXPECT_EQ(owned.captureNativePackedVelocities()[0].mVelocities, packed[0].mVelocities);
        mAnimation->endNativeScene();
    }
}

namespace
{
    TEST_F(PhysicalPoseAnimationTest, NativePrePhysicsRunsPropertiesOnceAndLeavesOtherCallbacksForViewer)
    {
        unsigned frames = 0;
        osg::ref_ptr<NativeRootProperty> rootProperty = new NativeRootProperty, boneProperty = new NativeRootProperty;
        mRoot->getOrCreateStateSet()->setUpdateCallback(rootProperty);
        mBone->getOrCreateStateSet()->setUpdateCallback(boneProperty);
        osg::ref_ptr<osg::Group> other = new osg::Group;
        osg::ref_ptr<SceneCounter> late = new SceneCounter;
        other->setUpdateCallback(late); mParent->addChild(other);
        auto prepared = mAnimation->prepareNativeScene(mGraph, Nif::NiTransform::getIdentity(),
            [&](const auto&, const auto&, unsigned) { ++frames; return MWRender::Animation::NativeSceneFrame{}; });
        ASSERT_TRUE(mAnimation->commitNativeScene(*prepared));
        SceneUtil::PostPhysicsUpdateVisitor post;
        post.setFrameStamp(mFrameStamp); post.setTraversalNumber(17);
        for (unsigned frame : {1u, 2u})
        {
            mFrameStamp->setFrameNumber(frame);
            ASSERT_TRUE(mAnimation->updateNativeSceneBeforePhysics(*mFrameStamp, 17));
            EXPECT_FALSE(mAnimation->updateNativeSceneBeforePhysics(*mFrameStamp, 17));
            EXPECT_EQ(frames, frame); EXPECT_EQ(mAnimation->mWriter->mWrites, frame);
            EXPECT_EQ(rootProperty->mVisits, frame); EXPECT_EQ(boneProperty->mVisits, frame);
            EXPECT_EQ(rootProperty->mPathLength, 2u);
            EXPECT_EQ(boneProperty->mPathLength, 3u);
            EXPECT_EQ(late->mVisits, frame - 1);
            mParent->accept(post);
            EXPECT_EQ(late->mVisits, frame); EXPECT_EQ(frames, frame);
            EXPECT_EQ(rootProperty->mVisits, frame); EXPECT_EQ(boneProperty->mVisits, frame);
            EXPECT_EQ(mAnimation->mWriter->mWrites, frame);
        }
    }

    TEST_F(PhysicalPoseAnimationTest, NativePostPhysicsReanchorsDrawingWithoutChangingPhysicalCacheOrLocals)
    {
        osg::ref_ptr<osg::MatrixTransform> outer = new osg::MatrixTransform;
        outer->addChild(mParent);
        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        osg::ref_ptr<SceneUtil::RigGeometry> rig = new SceneUtil::RigGeometry;
        osg::ref_ptr<osg::Geometry> source = new osg::Geometry;
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        vertices->push_back({0, 0, 0}); source->setVertexArray(vertices);
        rig->setSourceGeometry(source);
        rig->setBoneInfo({{"Pelvis", osg::BoundingSpheref(osg::Vec3f(), 1), osg::Matrixf::identity()}});
        rig->setTransform(osg::Matrixf::identity()); rig->setRootBone("Pelvis");
        rig->setInfluences({{{0, 1.f}}});
        geode->addDrawable(rig); mBone->addChild(geode);
        auto placement = Nif::NiTransform::getIdentity(); placement.mTranslation = {100, 0, 0};
        unsigned frames = 0;
        auto prepared = mAnimation->prepareNativeScene(mGraph, placement, [&](const auto&, const auto&, unsigned) {
            ++frames;
            MWRender::Animation::NativeSceneFrame frame; frame.mPlacement = placement;
            frame.mCollision = [](auto, const auto&) {
                auto physical = Nif::NiTransform::getIdentity(); physical.mTranslation = {140, 0, 0};
                return SceneUtil::NifSceneTraversal::CollisionResult{9, physical};
            };
            return frame;
        });
        ASSERT_TRUE(mAnimation->commitNativeScene(*prepared));
        ASSERT_TRUE(mAnimation->updateNativeSceneBeforePhysics(*mFrameStamp, 17));
        EXPECT_NEAR(mBone->getWorldMatrices()[0].getTrans().x(), 140, 1e-9);
        const auto local = mBone->getControllerTransform();
        outer->setMatrix(osg::Matrixd::translate(500, 0, 0));
        EXPECT_NEAR(mBone->getWorldMatrices()[0].getTrans().x(), 640, 1e-9);
        SceneUtil::PostPhysicsUpdateVisitor post; post.setFrameStamp(mFrameStamp); post.setTraversalNumber(17);
        outer->accept(post);
        EXPECT_NEAR(mBone->getWorldMatrices()[0].getTrans().x(), 140, 1e-9);
        EXPECT_EQ(mBone->getControllerTransform().mTranslation, local.mTranslation);
        for (unsigned r = 0; r < 3; ++r) for (unsigned c = 0; c < 3; ++c)
            EXPECT_EQ(mBone->getControllerTransform().mRotation.mValues[r][c], local.mRotation.mValues[r][c]);
        EXPECT_EQ(frames, 1u); EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
        struct Capture : osgUtil::CullVisitor
        {
            osg::Vec3f mPosition;
            unsigned mCalls = 0;
            void apply(osg::Geometry& geometry) override
            {
                ++mCalls;
                mPosition = static_cast<const osg::Vec3Array*>(geometry.getVertexArray())->front();
            }
        } capture;
        capture.setTraversalNumber(17);
        rig->accept(capture);
        EXPECT_EQ(capture.mCalls, 1u);
        EXPECT_NEAR(capture.mPosition.x(), 0, 1e-5);
        EXPECT_NEAR(rig->getBoundingBox().center().x(), 0, 1e-5);
    }

    TEST_F(PhysicalPoseAnimationTest, NativeSceneAdmittedAfterPrePhysicsWaitsUntilNextOwnedPass)
    {
        unsigned frames = 0;
        osg::ref_ptr<NativeRootProperty> property = new NativeRootProperty;
        mRoot->getOrCreateStateSet()->setUpdateCallback(property);
        auto prepared = mAnimation->prepareNativeScene(mGraph, Nif::NiTransform::getIdentity(),
            [&](const auto&, const auto&, unsigned) { ++frames; return MWRender::Animation::NativeSceneFrame{}; });
        ASSERT_TRUE(mAnimation->commitNativeScene(*prepared));
        SceneUtil::PostPhysicsUpdateVisitor post; post.setFrameStamp(mFrameStamp);
        mRoot->accept(post);
        EXPECT_EQ(frames, 0u); EXPECT_EQ(property->mVisits, 0u); EXPECT_EQ(mAnimation->mWriter->mWrites, 0u);
        mFrameStamp->setFrameNumber(1);
        ASSERT_TRUE(mAnimation->updateNativeSceneBeforePhysics(*mFrameStamp, 1));
        mRoot->accept(post);
        EXPECT_EQ(frames, 1u); EXPECT_EQ(property->mVisits, 1u); EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
    }

    TEST_F(PhysicalPoseAnimationTest, NativePrePhysicsFailureDoesNotConsumeTheFrame)
    {
        bool fail = true;
        unsigned frames = 0;
        auto prepared = mAnimation->prepareNativeScene(mGraph, Nif::NiTransform::getIdentity(),
            [&](const auto&, const auto&, unsigned) {
                ++frames;
                if (fail) throw std::runtime_error("frame preparation failed");
                return MWRender::Animation::NativeSceneFrame{};
            });
        ASSERT_TRUE(mAnimation->commitNativeScene(*prepared));
        EXPECT_THROW(mAnimation->updateNativeSceneBeforePhysics(*mFrameStamp, 0), std::runtime_error);
        EXPECT_EQ(mAnimation->mWriter->mWrites, 0u);
        fail = false;
        EXPECT_TRUE(mAnimation->updateNativeSceneBeforePhysics(*mFrameStamp, 0));
        SceneUtil::PostPhysicsUpdateVisitor post; post.setFrameStamp(mFrameStamp); mRoot->accept(post);
        EXPECT_EQ(frames, 2u); EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
    }

    TEST_F(PhysicalPoseAnimationTest, NativePostPhysicsRejectsChangedFrameStampBeforeProperties)
    {
        osg::ref_ptr<NativeRootProperty> property = new NativeRootProperty;
        mRoot->getOrCreateStateSet()->setUpdateCallback(property);
        auto prepared = mAnimation->prepareNativeScene(mGraph, Nif::NiTransform::getIdentity(),
            [](const auto&, const auto&, unsigned) { return MWRender::Animation::NativeSceneFrame{}; });
        ASSERT_TRUE(mAnimation->commitNativeScene(*prepared));
        ASSERT_TRUE(mAnimation->updateNativeSceneBeforePhysics(*mFrameStamp, 0));
        const auto simulation = mFrameStamp->getSimulationTime();
        mFrameStamp->setSimulationTime(simulation + 1);
        SceneUtil::PostPhysicsUpdateVisitor post; post.setFrameStamp(mFrameStamp);
        EXPECT_THROW(mRoot->accept(post), std::invalid_argument);
        EXPECT_EQ(property->mVisits, 1u); EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
        mFrameStamp->setSimulationTime(simulation);
        EXPECT_NO_THROW(mRoot->accept(post));
        EXPECT_EQ(property->mVisits, 1u); EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
    }
}

namespace
{
    TEST_F(PhysicalPoseAnimationTest, NativePrePhysicsRootPropertiesCannotReleaseOrRebindOwner)
    {
        struct Property : osg::StateSet::Callback
        {
            MWRender::Animation& mOwner;
            unsigned mVisits = 0;
            explicit Property(MWRender::Animation& owner) : mOwner(owner) {}
            void operator()(osg::StateSet*, osg::NodeVisitor*) override
            {
                ++mVisits;
                EXPECT_THROW(mOwner.updatePtr(mOwner.getPtr()), std::logic_error);
                EXPECT_THROW(mOwner.endNativeScene(), std::logic_error);
                EXPECT_THROW(mOwner.removeFromScene(), std::logic_error);
            }
        };
        osg::ref_ptr<Property> property = new Property(*mAnimation);
        mRoot->getOrCreateStateSet()->setUpdateCallback(property);
        unsigned frames = 0;
        auto prepared = mAnimation->prepareNativeScene(mGraph, Nif::NiTransform::getIdentity(),
            [&](const auto&, const auto&, unsigned) { ++frames; return MWRender::Animation::NativeSceneFrame{}; });
        ASSERT_TRUE(mAnimation->commitNativeScene(*prepared));
        ASSERT_TRUE(mAnimation->updateNativeSceneBeforePhysics(*mFrameStamp, 0));
        EXPECT_TRUE(mAnimation->hasNativeScene()); EXPECT_EQ(frames, 1u);
        SceneUtil::PostPhysicsUpdateVisitor post; post.setFrameStamp(mFrameStamp); mRoot->accept(post);
        EXPECT_EQ(property->mVisits, 1u); EXPECT_EQ(mAnimation->mWriter->mWrites, 1u);
    }
}

namespace
{
    TEST_F(PhysicalPoseAnimationTest, OwnedFirstPersonQuarterTurnPoseAppliesBeforeIncomingEaseBegins)
    {
        mAnimation->addNativeSource("native-reaction", nativeTrack({-100, -200, -300}, 1.5f), 9);
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        const float q = std::bit_cast<float>(0x3f3504f3u);
        auto snapshot = std::make_shared<const ESM4::RuntimeMeleePoseSnapshot>(ESM4::RuntimeMeleePoseSnapshot{0,
            {{"Pelvis", {-4, 5, 6}, {q, 0, 0, q}, .5f, 9}}});
        const std::array<MWRender::Animation::NativeSequenceLayer, 2> layers{{
            {"native-reaction", MWRender::BlendMask_All, 0, 1, 1, false, snapshot},
            {"native-reaction", MWRender::BlendMask_All, 0, 1, 0}}};
        ASSERT_TRUE(mAnimation->setNativeSequenceLayers(layers));
        traverse();
        EXPECT_EQ(mBone->getControllerTransform().mTranslation, osg::Vec3f(-4, 5, 6));
        EXPECT_EQ(mBone->getControllerTransform().mScale, .5f);
        // Original6CC3A0 retains two slots at ease0, renormalizing F3 ->F4.
        // The quaternion is converted to the engine matrix at this boundary.
        const float normalized = std::bit_cast<float>(0x3f3504f4u);
        const osg::Matrixf expected = osg::Matrixf::rotate(osg::Quat(0, 0, normalized, normalized))
            * osg::Matrixf::scale(.5f, .5f, .5f);
        const auto actual = mBone->getMatrix();
        for (unsigned r = 0; r < 3; ++r)
            for (unsigned c = 0; c < 3; ++c)
                EXPECT_NEAR(actual(r, c), expected(r, c), 1e-7f);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(snapshot->mTracks.front().mRotation[0]), 0x3f3504f3u);
    }
}

namespace
{
    struct PlaybackPeerView
    {
        osg::ref_ptr<osg::Group> mParent = new osg::Group;
        osg::ref_ptr<SceneUtil::Skeleton> mRoot = new SceneUtil::Skeleton;
        osg::ref_ptr<NifOsg::MatrixTransform> mBone = new NifOsg::MatrixTransform;
        osg::ref_ptr<TestAnimation> mAnimation;
        PlaybackPeerView()
        {
            mRoot->setUserValue(Misc::OsgUserValues::sFileHash, std::string("physical-pose-test"));
            mBone->setName("Pelvis"); mBone->setUserValue("recordIndex", 8u);
            mBone->setRotation(osg::Quat()); mBone->setScale(1);
            mRoot->addChild(mBone);
            mAnimation = new TestAnimation(mParent, mRoot, mBone);
            mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        }
    };

    struct PlaybackKeys : MWRender::Animation::TextKeyListener
    {
        unsigned mCalls = 0;
        void handleTextKey(std::string_view, SceneUtil::TextKeyMap::ConstIterator,
            const SceneUtil::TextKeyMap&) override { ++mCalls; }
    };

    ESM4::RuntimePassiveMeleeSequence passiveMelee(unsigned protocol, float time)
    {
        if (protocol < 2)
        {
            ESM4::RuntimeMeleeStrike strike{12, protocol ? ESM4::MeleeStrikeKind::StandingPower
                : ESM4::MeleeStrikeKind::Left, {}, "resident", 1, time, true};
            strike.mSequenceTiming = ESM4::MeleeSequenceTiming{};
            if (protocol) strike.mPowerPhase = ESM4::PowerMeleePhase::End;
            else strike.mOrdinaryPhase = ESM4::OrdinaryMeleePhase::End;
            return {strike};
        }
        ESM4::RuntimeMeleeReaction reaction{12, protocol == 2 ? ESM4::MeleeReactionKind::BlockHit
            : ESM4::MeleeReactionKind::Recoil, "resident", time};
        return {reaction};
    }

    TEST_F(PhysicalPoseAnimationTest, PassiveMeleeLayersKeepPerViewPrioritiesAndDoNotPlayContactTracks)
    {
        PlaybackPeerView peer;
        mAnimation->setPlaybackPeer(peer.mAnimation);
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        auto bodyTrack = nativeTrack({3, 0, 0}), firstTrack = nativeTrack({11, 0, 0});
        bodyTrack->mVelocity = firstTrack->mVelocity = {4, 0, 0};
        mAnimation->addNativeSource("resident", bodyTrack, 20);
        peer.mAnimation->addNativeSource("resident", firstTrack, 80);
        mAnimation->addNativeSource("stagger", nativeTrack({20, 0, 0}), 50);
        peer.mAnimation->addNativeSource("stagger", nativeTrack({30, 0, 0}), 50);
        PlaybackKeys bodyKeys, firstKeys;
        mAnimation->setTextKeyListener(&bodyKeys); peer.mAnimation->setTextKeyListener(&firstKeys);
        for (unsigned protocol = 0; protocol < 4; ++protocol)
        {
            SCOPED_TRACE(protocol);
            auto body = passiveMelee(protocol, .25f), first = passiveMelee(protocol, .75f);
            std::shared_ptr<const ESM4::RuntimeMeleePoseSnapshot> bodyPose, firstPose;
            auto bodyLayers = MWRender::oblivionPassiveMeleeLayers(*mAnimation, &body, 1, bodyPose);
            auto firstLayers = MWRender::oblivionPassiveMeleeLayers(*peer.mAnimation, &first, 11, firstPose);
            ASSERT_EQ(bodyLayers.size(), 1u); ASSERT_EQ(firstLayers.size(), 1u);
            EXPECT_EQ(bodyLayers[0].mTime, .25f); EXPECT_EQ(firstLayers[0].mTime, .75f);
            EXPECT_FALSE(bodyLayers[0].mFollowAnimationTime); EXPECT_FALSE(firstLayers[0].mFollowAnimationTime);
            bodyLayers.push_back({"stagger"}); firstLayers.push_back({"stagger"});
            ASSERT_TRUE(mAnimation->setNativeSequenceLayersForViews(bodyLayers, firstLayers));
            mRoot->accept(mVisitor); peer.mRoot->accept(mVisitor);
            EXPECT_EQ(mBone->getControllerTransform().mTranslation.x(), 20);
            EXPECT_EQ(peer.mBone->getControllerTransform().mTranslation.x(), 14);
            // Slot3 remains visible when its higher-priority slot0 is removed.
            bodyLayers.pop_back(); firstLayers.pop_back();
            ASSERT_TRUE(mAnimation->setNativeSequenceLayersForViews(bodyLayers, firstLayers));
            mRoot->accept(mVisitor); peer.mRoot->accept(mVisitor);
            EXPECT_EQ(mBone->getControllerTransform().mTranslation.x(), 4);
            EXPECT_EQ(peer.mBone->getControllerTransform().mTranslation.x(), 14);
            mAnimation->runAnimation(.25f); peer.mAnimation->runAnimation(.25f);
            EXPECT_FALSE(mAnimation->getInfo("resident")); EXPECT_FALSE(peer.mAnimation->getInfo("resident"));
            EXPECT_EQ(bodyKeys.mCalls, 0u); EXPECT_EQ(firstKeys.mCalls, 0u);
        }
        mAnimation->setTextKeyListener(nullptr); peer.mAnimation->setTextKeyListener(nullptr);
    }

    TEST_F(PhysicalPoseAnimationTest, PassiveMeleeCapturedLayersReuseBindingsAndReconstructSavedPoses)
    {
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        mAnimation->addNativeSource("resident", nativeTrack({100, 200, 300}, 3), 9);
        for (unsigned protocol = 0; protocol < 4; ++protocol)
        {
            SCOPED_TRACE(protocol);
            auto passive = passiveMelee(protocol, 0);
            std::visit([](auto& sequence) {
                ESM4::MeleeSequenceTiming timing{.mEasing = true, .mOffset = 0.f,
                    .mEaseStart = 1.f, .mEaseEnd = 2.f, .mLastInput = 0.f};
                sequence.mSequenceTiming = timing;
                sequence.mPoseSnapshot = ESM4::RuntimeMeleePoseSnapshot{0,
                    {{"Pelvis", {11, 22, 33}, {1, 0, 0, 0}, 2, 9}}};
            }, passive.mSequence);
            std::shared_ptr<const ESM4::RuntimeMeleePoseSnapshot> pose;
            std::shared_ptr<const ESM4::RuntimeMeleePoseSnapshot> firstBinding;
            osg::ref_ptr<osg::Callback> controller;
            for (unsigned i = 0; i <= 4; ++i)
            {
                const float clock = 1 + i * .25f;
                auto layers = MWRender::oblivionPassiveMeleeLayers(*mAnimation, &passive, clock, pose);
                ASSERT_EQ(layers.size(), 2u); ASSERT_TRUE(pose);
                if (i == 0) firstBinding = pose;
                EXPECT_EQ(pose, firstBinding);
                ASSERT_TRUE(mAnimation->setNativeSequenceLayers(layers));
                if (i == 0) controller = mBone->getUpdateCallback();
                EXPECT_EQ(mBone->getUpdateCallback(), controller);
                mFrameStamp->setFrameNumber(i); traverse();
                const auto local = mBone->getControllerTransform();
                // Same original one-block interpolation goldens as the direct
                // captured-pose test, now reached through the persisted union.
                EXPECT_EQ(local.mTranslation.x(), 11 + i * 22.25f);
                EXPECT_EQ(local.mTranslation.y(), 22 + i * 44.5f);
                EXPECT_EQ(local.mTranslation.z(), 33 + i * 66.75f);
                EXPECT_EQ(local.mScale, 2 + i * .25f);
                PlaybackPeerView fresh;
                fresh.mAnimation->addNativeSource("resident", nativeTrack({100, 200, 300}, 3), 9);
                std::shared_ptr<const ESM4::RuntimeMeleePoseSnapshot> rebound;
                const auto restored = passive;
                const auto rebuilt = MWRender::oblivionPassiveMeleeLayers(*fresh.mAnimation, &restored, clock, rebound);
                ASSERT_TRUE(fresh.mAnimation->setNativeSequenceLayers(rebuilt));
                fresh.mRoot->accept(mVisitor);
                EXPECT_EQ(fresh.mBone->getMatrix(), mBone->getMatrix());
            }
            EXPECT_TRUE(MWRender::oblivionPassiveMeleeLayers(*mAnimation, nullptr, 2, pose).empty());
            EXPECT_FALSE(pose);
        }
    }

    TEST_F(PhysicalPoseAnimationTest, PassiveMeleeLayersRejectUnanchoredAndMissingSourcesAndReleaseInactiveBindings)
    {
        mAnimation->addNativeSource("resident", nativeTrack({3, 0, 0}), 9);
        auto passive = passiveMelee(2, .25f);
        auto& reaction = std::get<ESM4::RuntimeMeleeReaction>(passive.mSequence);
        reaction.mSequenceTiming = {.mOffset = 0.f, .mEaseStart = 0.f, .mLastInput = 0.f};
        reaction.mRetirement = ESM4::RuntimeReactionRetirement{1, {}};
        std::shared_ptr<const ESM4::RuntimeMeleePoseSnapshot> pose;
        EXPECT_THROW(MWRender::oblivionPassiveMeleeLayers(*mAnimation, &passive, 1, pose), std::invalid_argument);
        reaction.mRetirement->mStartClock = 1;
        auto layers = MWRender::oblivionPassiveMeleeLayers(*mAnimation, &passive, 1.25f, pose);
        ASSERT_EQ(layers.size(), 1u); EXPECT_EQ(layers[0].mEase, .75f);
        reaction.mAnimationGroup = "missing";
        EXPECT_THROW(MWRender::oblivionPassiveMeleeLayers(*mAnimation, &passive, 1.25f, pose), std::invalid_argument);
        reaction.mInactive = true;
        pose = std::make_shared<const ESM4::RuntimeMeleePoseSnapshot>();
        EXPECT_TRUE(MWRender::oblivionPassiveMeleeLayers(*mAnimation, &passive, 2, pose).empty()); EXPECT_FALSE(pose);
        EXPECT_THROW(MWRender::oblivionPassiveMeleeLayers(*mAnimation, &passive, -1, pose), std::invalid_argument);
        auto malformed = passiveMelee(0, 0);
        std::get<ESM4::RuntimeMeleeStrike>(malformed.mSequence).mSequenceTiming.reset();
        EXPECT_THROW(MWRender::oblivionPassiveMeleeLayers(*mAnimation, &malformed, 0, pose), std::runtime_error);
        auto fresh = passiveMelee(0, 0);
        auto& strike = std::get<ESM4::RuntimeMeleeStrike>(fresh.mSequence);
        strike.mSequenceTiming = ESM4::MeleeSequenceTiming{.mEasing = true, .mEaseEnd = 1};
        strike.mPoseSnapshot = ESM4::RuntimeMeleePoseSnapshot{};
        EXPECT_THROW(MWRender::oblivionPassiveMeleeLayers(*mAnimation, &fresh, 0, pose), std::invalid_argument);
    }

    TEST_F(PhysicalPoseAnimationTest, RetainedPlaybackGroupKeepsClockWhileOtherEqualPriorityStatesAreReplaced)
    {
        mAnimation->usePassivePlayback();
        for (const auto* group : {"held-block", "old-slot3", "counter"})
            mAnimation->addNativeSource(group, nativeTrack({1, 2, 3}), 9, 2);
        const MWRender::AnimPriority priority{1};
        const auto scope = MWRender::Animation::PlaybackScope::ThisView;
        mAnimation->play("held-block", priority, MWRender::BlendMask_All, false, 1,
            "start", "stop", 0, std::numeric_limits<std::uint32_t>::max(), true, scope);
        mAnimation->runAnimation(.5f);
        const auto* heldSource = mAnimation->getControllerSequenceMetadata("held-block");
        mAnimation->play("old-slot3", priority, MWRender::BlendMask_All, false, 0,
            "start", "stop", 0, 0, false, scope, "held-block");
        mAnimation->play("counter", priority, MWRender::BlendMask_All, false, 0,
            "start", "stop", 0, 0, false, scope, "held-block");
        EXPECT_FALSE(mAnimation->getInfo("old-slot3"));
        EXPECT_TRUE(mAnimation->getInfo("counter"));
        EXPECT_TRUE(mAnimation->isPlaying("held-block"));
        EXPECT_EQ(mAnimation->getControllerSequenceMetadata("held-block"), heldSource);
        EXPECT_FLOAT_EQ(mAnimation->getCurrentTime("held-block"), .5f);
        for (const float step : {.025f, .025f, .05f, .025f})
            mAnimation->runAnimation(step);
        EXPECT_FLOAT_EQ(mAnimation->getCurrentTime("held-block"), .625f);
        const auto layers = mAnimation->nativePlayingSequenceLayers({});
        ASSERT_EQ(layers.size(), 2u);
        const auto held = std::find_if(layers.begin(), layers.end(), [](const auto& layer) {
            return layer.mGroup == "held-block";
        });
        ASSERT_NE(held, layers.end());
        EXPECT_FLOAT_EQ(held->mTime, .625f);
        EXPECT_TRUE(held->mFollowAnimationTime);
        mAnimation->disable("counter", scope);
        EXPECT_TRUE(mAnimation->isPlaying("held-block"));
        // The ordinary replacement contract still removes equal priorities
        // when the caller has no separately resident native slot to retain.
        mAnimation->play("old-slot3", priority, MWRender::BlendMask_All, false, 0,
            "start", "stop", 0, 0, false, scope);
        EXPECT_FALSE(mAnimation->getInfo("held-block"));
    }

    TEST_F(PhysicalPoseAnimationTest, RetainedPlaybackGroupRespectsIndependentPeerClockAndViewScope)
    {
        PlaybackPeerView peer;
        mAnimation->usePassivePlayback();
        mAnimation->setPlaybackPeer(peer.mAnimation);
        for (auto* view : {mAnimation.get(), peer.mAnimation.get()})
        {
            view->addNativeSource("held-block", nativeTrack({1, 2, 3}), 9, 2);
            view->addNativeSource("counter", nativeTrack({1, 2, 3}), 9, 2);
        }
        const MWRender::AnimPriority priority{1};
        const auto scope = MWRender::Animation::PlaybackScope::ThisView;
        mAnimation->play("held-block", priority, MWRender::BlendMask_All, false, 1,
            "start", "stop", .25f, 1, true, scope);
        peer.mAnimation->play("held-block", priority, MWRender::BlendMask_All, false, 1,
            "start", "stop", .125f, 1, true, scope);
        mAnimation->play("counter", priority, MWRender::BlendMask_All, false, 0,
            "start", "stop", 0, 0, false, scope, "held-block");
        EXPECT_TRUE(mAnimation->getInfo("counter"));
        EXPECT_FALSE(peer.mAnimation->getInfo("counter"));
        EXPECT_FLOAT_EQ(mAnimation->getCurrentTime("held-block"), .5f);
        EXPECT_FLOAT_EQ(peer.mAnimation->getCurrentTime("held-block"), .25f);
        // Mirrored ordinary playback also passes retention to each view; it
        // must not restart either resident sequence at the other's clock.
        mAnimation->play("counter", priority, MWRender::BlendMask_All, false, 0,
            "start", "stop", 0, 0, false, MWRender::Animation::PlaybackScope::AllViews, "held-block");
        EXPECT_TRUE(peer.mAnimation->getInfo("counter"));
        EXPECT_FLOAT_EQ(mAnimation->getCurrentTime("held-block"), .5f);
        EXPECT_FLOAT_EQ(peer.mAnimation->getCurrentTime("held-block"), .25f);
    }

    TEST_F(PhysicalPoseAnimationTest, PlaybackPeerKeepsOwnResourcesAndSuppressesDuplicateKeyAndEndEvents)
    {
        PlaybackPeerView peer;
        mAnimation->setPlaybackPeer(peer.mAnimation);
        EXPECT_THROW(peer.mAnimation->setPlaybackPeer(mAnimation.get()), std::invalid_argument);
        auto bodyTrack = nativeTrack({1, 2, 3}), firstTrack = nativeTrack({11, 12, 13});
        bodyTrack->mVelocity = {1, 0, 0}; firstTrack->mVelocity = {0, 4, 0};
        auto body = mAnimation->addNativeSource("walk", bodyTrack, 9);
        auto first = peer.mAnimation->addNativeSource("walk", firstTrack, 9);
        body->mTextKeys.emplace(.1f, "foot: contact"); first->mTextKeys.emplace(.1f, "foot: contact");
        PlaybackKeys bodyKeys, firstKeys;
        mAnimation->setTextKeyListener(&bodyKeys); peer.mAnimation->setTextKeyListener(&firstKeys);
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        mAnimation->play("walk", MWRender::AnimPriority{1}, MWRender::BlendMask_All,
            false, 1, "start", "stop", 0, 0);
        EXPECT_EQ(bodyKeys.mCalls, 1u); EXPECT_EQ(firstKeys.mCalls, 0u);
        ASSERT_TRUE(mAnimation->setNativeSequenceLayers(mAnimation->nativePlayingSequenceLayers({})));
        ASSERT_TRUE(peer.mAnimation->setNativeSequenceLayers(peer.mAnimation->nativePlayingSequenceLayers({})));
        mAnimation->runAnimation(.25f);
        mRoot->accept(mVisitor); peer.mRoot->accept(mVisitor);
        EXPECT_EQ(mAnimation->getCurrentTime("walk"), .25f);
        EXPECT_EQ(peer.mAnimation->getCurrentTime("walk"), .25f);
        EXPECT_EQ(bodyKeys.mCalls, 2u); EXPECT_EQ(firstKeys.mCalls, 0u);
        EXPECT_EQ(mBone->getControllerTransform().mTranslation, osg::Vec3f(1.25f, 2, 3));
        EXPECT_EQ(peer.mBone->getControllerTransform().mTranslation, osg::Vec3f(11, 13, 13));
        // No Lua manager is installed in this fixture. A passive disable must
        // not dispatch an animation-end event into the gameplay environment.
        peer.mAnimation->disable("walk");
        EXPECT_TRUE(mAnimation->isPlaying("walk"));
        EXPECT_FALSE(peer.mAnimation->isPlaying("walk"));
        EXPECT_EQ(firstKeys.mCalls, 0u);
        mAnimation->setTextKeyListener(nullptr); peer.mAnimation->setTextKeyListener(nullptr);
    }

    TEST_F(PhysicalPoseAnimationTest, ViewScopedPlaybackAndNativeLayersDoNotOverwritePeerOwnership)
    {
        PlaybackPeerView peer;
        mAnimation->setPlaybackPeer(peer.mAnimation);
        mAnimation->addNativeSource("reaction", nativeTrack({1, 2, 3}), 9);
        peer.mAnimation->addNativeSource("reaction", nativeTrack({11, 12, 13}), 9);
        peer.mAnimation->play("reaction", MWRender::AnimPriority{1}, MWRender::BlendMask_All,
            false, 0, "start", "stop", 0, 0);
        mAnimation->play("reaction", MWRender::AnimPriority{1}, MWRender::BlendMask_All,
            false, 0, "start", "stop", 0, 0, false, MWRender::Animation::PlaybackScope::ThisView);
        ASSERT_TRUE(peer.mAnimation->setAnimationFrameTime("reaction", .125f));
        ASSERT_TRUE(mAnimation->setAnimationFrameTime("reaction", .25f,
            MWRender::Animation::PlaybackScope::ThisView));
        const std::array<MWRender::Animation::NativeSequenceLayer, 1> firstLayers{{
            {"reaction", MWRender::BlendMask_All, .125f, 1, 1}}};
        ASSERT_TRUE(peer.mAnimation->setNativeSequenceLayers(firstLayers));
        const auto builds = peer.mAnimation->mControllerBuilds;
        ASSERT_TRUE(mAnimation->setNativeSequenceLayers({}));
        mAnimation->runAnimation(0); peer.mRoot->accept(mVisitor);
        EXPECT_EQ(mAnimation->getCurrentTime("reaction"), .25f);
        EXPECT_EQ(peer.mAnimation->getCurrentTime("reaction"), .125f);
        EXPECT_EQ(peer.mAnimation->mControllerBuilds, builds);
        EXPECT_EQ(peer.mBone->getControllerTransform().mTranslation, osg::Vec3f(11, 12, 13));
        EXPECT_EQ(mAnimation->getObjectRoot(), mRoot.get());
        EXPECT_EQ(peer.mAnimation->getObjectRoot(), peer.mRoot.get());
    }

    TEST_F(PhysicalPoseAnimationTest, HiddenPeerContinuesUpdateTraversalWithoutRootOrControllerReplacement)
    {
        PlaybackPeerView peer;
        mAnimation->setPlaybackPeer(peer.mAnimation);
        const auto* bodyRoot = mAnimation->getObjectRoot();
        const auto* firstRoot = peer.mAnimation->getObjectRoot();
        const auto builds = peer.mAnimation->mControllerBuilds;
        // A cull-only visibility switch leaves every normal update reachable.
        for (unsigned frame = 1; frame <= 8; ++frame)
        {
            const bool firstVisible = frame % 2 == 0;
            mAnimation->setPresentationVisible(!firstVisible);
            peer.mAnimation->setPresentationVisible(firstVisible);
            EXPECT_NE(mParent->getNodeMask(), 0u); EXPECT_NE(peer.mParent->getNodeMask(), 0u);
            struct CountNodes : osg::NodeVisitor
            {
                unsigned mCount = 0;
                CountNodes() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
                void apply(osg::Node& node) override { ++mCount; traverse(node); }
            } bodyCull, firstCull;
            mParent->getCullCallback()->run(mParent, &bodyCull);
            peer.mParent->getCullCallback()->run(peer.mParent, &firstCull);
            EXPECT_EQ(bodyCull.mCount > 0, !firstVisible);
            EXPECT_EQ(firstCull.mCount > 0, firstVisible);
            mFrameStamp->setFrameNumber(frame); mVisitor.setTraversalNumber(frame);
            mParent->accept(mVisitor); peer.mParent->accept(mVisitor);
            EXPECT_EQ(mAnimation->mWriter->mWrites, frame);
            EXPECT_EQ(peer.mAnimation->mWriter->mWrites, frame);
            EXPECT_EQ(mAnimation->getObjectRoot(), bodyRoot);
            EXPECT_EQ(peer.mAnimation->getObjectRoot(), firstRoot);
            EXPECT_EQ(peer.mAnimation->mControllerBuilds, builds);
        }
    }
}

namespace
{
    TEST_F(PhysicalPoseAnimationTest, OrdinaryFrameCommandsUsePeerBoundsWhileNativeTargetsStayViewScoped)
    {
        PlaybackPeerView peer;
        mAnimation->setPlaybackPeer(peer.mAnimation);
        mAnimation->addNativeSource("strike", nativeTrack({1, 2, 3}), 9);
        peer.mAnimation->addNativeSource("strike", nativeTrack({11, 12, 13}), 9, 1);
        mAnimation->play("strike", MWRender::AnimPriority{1}, MWRender::BlendMask_All,
            false, 0, "start", "stop", 0, 0);
        ASSERT_TRUE(mAnimation->setAnimationFrameTime("strike", 1.5f));
        mAnimation->runAnimation(0);
        EXPECT_EQ(mAnimation->getCurrentTime("strike"), 1.5f);
        EXPECT_EQ(peer.mAnimation->getCurrentTime("strike"), 1.f);
        EXPECT_FALSE(peer.mAnimation->isPlaying("strike"));
        ASSERT_TRUE(mAnimation->setAnimationFrameTime("strike", 2,
            MWRender::Animation::PlaybackScope::ThisView));
        mAnimation->runAnimation(0);
        EXPECT_EQ(mAnimation->getCurrentTime("strike"), 2);
        EXPECT_EQ(peer.mAnimation->getCurrentTime("strike"), 1);
        EXPECT_THROW(mAnimation->setAnimationFrameTime("strike", -1), std::invalid_argument);
        EXPECT_EQ(peer.mAnimation->getCurrentTime("strike"), 1);
    }
}

namespace
{
    TEST_F(PhysicalPoseAnimationTest, MissingPeerResourceDoesNotEraseItsIndependentSamePriorityState)
    {
        PlaybackPeerView peer;
        mAnimation->setPlaybackPeer(peer.mAnimation);
        mAnimation->addNativeSource("walk", nativeTrack({1, 2, 3}), 9);
        peer.mAnimation->addNativeSource("hands-idle", nativeTrack({11, 12, 13}), 9);
        peer.mAnimation->play("hands-idle", MWRender::AnimPriority{1}, MWRender::BlendMask_All,
            false, 1, "start", "stop", 0, 0);
        peer.mAnimation->runAnimation(.25f);
        const auto builds = peer.mAnimation->mControllerBuilds;
        mAnimation->play("walk", MWRender::AnimPriority{1}, MWRender::BlendMask_All,
            false, 1, "start", "stop", 0, 0);
        EXPECT_TRUE(mAnimation->isPlaying("walk"));
        EXPECT_TRUE(peer.mAnimation->isPlaying("hands-idle"));
        EXPECT_FALSE(peer.mAnimation->isPlaying("walk"));
        EXPECT_EQ(peer.mAnimation->getCurrentTime("hands-idle"), .25f);
        EXPECT_EQ(peer.mAnimation->mControllerBuilds, builds);
    }

    TEST_F(PhysicalPoseAnimationTest, PairedNativePlansRejectEitherInvalidViewBeforeChangingPublishedFrames)
    {
        PlaybackPeerView peer;
        mAnimation->setPlaybackPeer(peer.mAnimation);
        auto bodyTrack = nativeTrack({1, 2, 3}), firstTrack = nativeTrack({11, 12, 13});
        bodyTrack->mVelocity = firstTrack->mVelocity = {1, 0, 0};
        mAnimation->addNativeSource("reaction", bodyTrack, 9);
        peer.mAnimation->addNativeSource("reaction", firstTrack, 9);
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        using Layer = MWRender::Animation::NativeSequenceLayer;
        std::array<Layer, 1> body{{{"reaction", MWRender::BlendMask_All, .125f, 1, 1}}};
        std::array<Layer, 1> first{{{"reaction", MWRender::BlendMask_All, .25f, 1, 1}}};
        ASSERT_TRUE(mAnimation->setNativeSequenceLayersForViews(body, first));
        const auto bodyBuilds = mAnimation->mControllerBuilds;
        const auto firstBuilds = peer.mAnimation->mControllerBuilds;
        const auto checkPublished = [&](float bodyTime, float firstTime) {
            mAnimation->runAnimation(0);
            mRoot->accept(mVisitor); peer.mRoot->accept(mVisitor);
            EXPECT_EQ(mBone->getControllerTransform().mTranslation, osg::Vec3f(1 + bodyTime, 2, 3));
            EXPECT_EQ(peer.mBone->getControllerTransform().mTranslation, osg::Vec3f(11 + firstTime, 12, 13));
            EXPECT_EQ(mAnimation->mControllerBuilds, bodyBuilds);
            EXPECT_EQ(peer.mAnimation->mControllerBuilds, firstBuilds);
        };
        checkPublished(.125f, .25f);
        body[0].mTime = .5f;
        first[0].mTime = 11;
        EXPECT_THROW(mAnimation->setNativeSequenceLayersForViews(body, first), std::invalid_argument);
        checkPublished(.125f, .25f);
        first[0].mTime = .75f;
        first[0].mGroup = "missing";
        EXPECT_FALSE(mAnimation->setNativeSequenceLayersForViews(body, first));
        checkPublished(.125f, .25f);
        first[0].mGroup = "reaction";
        first[0].mPoseSnapshot = std::make_shared<const ESM4::RuntimeMeleePoseSnapshot>(
            ESM4::RuntimeMeleePoseSnapshot{0, {{"unbound bone", {0, 0, 0}, {1, 0, 0, 0}, 1, 9}}});
        EXPECT_FALSE(mAnimation->setNativeSequenceLayersForViews(body, first));
        checkPublished(.125f, .25f);
        first[0].mPoseSnapshot.reset();
        body[0].mTime = -1;
        EXPECT_THROW(mAnimation->setNativeSequenceLayersForViews(body, first), std::invalid_argument);
        checkPublished(.125f, .25f);
        body[0].mTime = .5f;
        ASSERT_TRUE(mAnimation->setNativeSequenceLayersForViews(body, first));
        checkPublished(.5f, .75f);
    }

    TEST_F(PhysicalPoseAnimationTest, HeldCapturedLayersReuseBindingsAndRebuildSavedViews)
    {
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        auto source = mAnimation->addNativeSource("held", nativeTrack({100, 200, 300}, 3), 9);
        source->mControllerSequences[0].mCycleType = 0;
        ESM4::RuntimeHeldBlockSequence held{"held", 0,
            {.mEasing = true, .mOffset = 0.f, .mEaseStart = 1.f, .mEaseEnd = 2.f, .mLastInput = 0.f}};
        held.mPoseSnapshot = ESM4::RuntimeMeleePoseSnapshot{0,
            {{"Pelvis", {11, 22, 33}, {1, 0, 0, 0}, 2, 9}}};
        MWRender::OblivionHeldPoseBindings bindings;
        std::shared_ptr<const ESM4::RuntimeMeleePoseSnapshot> original;
        osg::ref_ptr<osg::Callback> controller;
        for (unsigned i = 0; i <= 4; ++i)
        {
            const float clock = 1 + i * .25f;
            auto layers = MWRender::oblivionHeldBlockLayers(*mAnimation, {&held, 1}, clock, bindings);
            ASSERT_EQ(layers.size(), 2u); ASSERT_EQ(bindings.size(), 1u); ASSERT_TRUE(bindings[0]);
            if (i == 0) original = bindings[0];
            EXPECT_EQ(bindings[0], original);
            EXPECT_FALSE(layers[0].mExposeSampleTime); EXPECT_TRUE(layers[1].mExposeSampleTime);
            ASSERT_TRUE(mAnimation->setNativeSequenceLayers(layers));
            if (i == 0) controller = mBone->getUpdateCallback();
            EXPECT_EQ(controller, mBone->getUpdateCallback());
            mFrameStamp->setFrameNumber(i); traverse();
            const auto transform = mBone->getControllerTransform();
            EXPECT_EQ(transform.mTranslation.x(), 11 + i * 22.25f);
            EXPECT_EQ(transform.mTranslation.y(), 22 + i * 44.5f);
            EXPECT_EQ(transform.mScale, 2 + i * .25f);
            PlaybackPeerView fresh;
            auto rebuiltSource = fresh.mAnimation->addNativeSource("held", nativeTrack({100, 200, 300}, 3), 9);
            rebuiltSource->mControllerSequences[0].mCycleType = 0;
            MWRender::OblivionHeldPoseBindings rebound;
            const auto rebuilt = MWRender::oblivionHeldBlockLayers(*fresh.mAnimation, {&held, 1}, clock, rebound);
            ASSERT_TRUE(fresh.mAnimation->setNativeSequenceLayers(rebuilt));
            fresh.mRoot->accept(mVisitor);
            EXPECT_EQ(fresh.mBone->getMatrix(), mBone->getMatrix());
            EXPECT_FALSE(fresh.mAnimation->getInfo("held"));
        }
    }

    TEST_F(PhysicalPoseAnimationTest, HeldPeerRetirementUsesOneNewEaseForCapturedSourceAndDestination)
    {
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        auto source = mAnimation->addNativeSource("held", nativeTrack({100, 0, 0}), 9);
        source->mControllerSequences[0].mCycleType = 0;
        ESM4::RuntimeHeldBlockSequence held{"held", 0,
            {.mOffset = 0.f, .mEaseStart = 1.f, .mEaseEnd = 2.f, .mLastInput = 0.f},
            ESM4::RuntimeReactionRetirement{1, 1}};
        held.mPoseSnapshot = ESM4::RuntimeMeleePoseSnapshot{0,
            {{"Pelvis", {11, 0, 0}, {1, 0, 0, 0}, 1, 9}}};
        MWRender::OblivionHeldPoseBindings bindings;
        for (unsigned i = 0; i < 4; ++i)
        {
            auto layers = MWRender::oblivionHeldBlockLayers(*mAnimation, {&held, 1}, 1 + i * .25f, bindings);
            ASSERT_EQ(layers.size(), 2u);
            EXPECT_EQ(layers[0].mEase, 1 - i * .25f);
            EXPECT_EQ(layers[1].mEase, 1 - i * .25f);
            EXPECT_EQ(layers[1].mTime, 0);
            ASSERT_TRUE(mAnimation->setNativeSequenceLayers(layers));
            mFrameStamp->setFrameNumber(i); traverse();
            // Equal native priorities normalize the two shared NI3 layers.
            EXPECT_EQ(mBone->getControllerTransform().mTranslation.x(), 55.5f);
        }
        held.mPoseSnapshot.reset(); held.mInactive = true;
        EXPECT_TRUE(MWRender::oblivionHeldBlockLayers(*mAnimation, {&held, 1}, 2, bindings).empty());
        ASSERT_EQ(bindings.size(), 1u); EXPECT_FALSE(bindings[0]);
    }

    TEST_F(PhysicalPoseAnimationTest, HeldLoopSamplesExposeIndependentWrappedTimesWithoutReplayingEventTracks)
    {
        PlaybackPeerView peer;
        mAnimation->setPlaybackPeer(peer.mAnimation);
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        auto bodyTrack = nativeTrack({3, 0, 0}), firstTrack = nativeTrack({11, 0, 0});
        bodyTrack->mVelocity = firstTrack->mVelocity = {4, 0, 0};
        auto bodySource = mAnimation->addNativeSource("held", bodyTrack, 9, 2);
        auto firstSource = peer.mAnimation->addNativeSource("held", firstTrack, 9, 2);
        bodySource->mControllerSequences[0].mCycleType = firstSource->mControllerSequences[0].mCycleType = 0;
        mAnimation->play("held", 1, MWRender::BlendMask_All, false, 0, "start", "stop", 0, 0, false,
            MWRender::Animation::PlaybackScope::ThisView);
        peer.mAnimation->play("held", 1, MWRender::BlendMask_All, false, 0, "start", "stop", 0, 0, false,
            MWRender::Animation::PlaybackScope::ThisView);
        const std::array<std::string_view, 1> excluded{"HELD"};
        EXPECT_TRUE(mAnimation->nativePlayingSequenceLayers(excluded).empty());
        EXPECT_TRUE(peer.mAnimation->nativePlayingSequenceLayers(excluded).empty());
        PlaybackKeys bodyKeys, firstKeys;
        mAnimation->setTextKeyListener(&bodyKeys); peer.mAnimation->setTextKeyListener(&firstKeys);
        ESM4::RuntimeHeldBlockSequence body{"held"}, first{"held"};
        MWRender::OblivionHeldPoseBindings bodyBindings, firstBindings;
        for (const float time : {.25f, 1.5f, 2.f, 0.f, .125f})
        {
            body.mAnimationTime = time; first.mAnimationTime = 2 - time;
            auto bodyLayers = MWRender::oblivionHeldBlockLayers(*mAnimation, {&body, 1}, 1, bodyBindings);
            auto firstLayers = MWRender::oblivionHeldBlockLayers(*peer.mAnimation, {&first, 1}, 11, firstBindings);
            ASSERT_TRUE(mAnimation->setNativeSequenceLayersForViews(bodyLayers, firstLayers));
            mAnimation->runAnimation(.25f); peer.mAnimation->runAnimation(.25f);
            mRoot->accept(mVisitor); peer.mRoot->accept(mVisitor);
            EXPECT_EQ(mAnimation->getCurrentTime("HELD"), time);
            EXPECT_EQ(peer.mAnimation->getCurrentTime("held"), 2 - time);
            EXPECT_EQ(mBone->getControllerTransform().mTranslation.x(), 3 + 4 * time);
            EXPECT_EQ(peer.mBone->getControllerTransform().mTranslation.x(), 11 + 4 * (2 - time));
            EXPECT_EQ(bodyKeys.mCalls, 0u); EXPECT_EQ(firstKeys.mCalls, 0u);
            bodyLayers[0].mExposeSampleTime = false;
            ASSERT_TRUE(mAnimation->setNativeSequenceLayers(bodyLayers));
            EXPECT_EQ(mAnimation->getCurrentTime("held"), 0);
        }
        mAnimation->setTextKeyListener(nullptr); peer.mAnimation->setTextKeyListener(nullptr);
    }

    TEST_F(PhysicalPoseAnimationTest, HeldProjectionRejectsBadFramesWithoutReplacingBindingsOrPublishedTime)
    {
        auto source = mAnimation->addNativeSource("held", nativeTrack({3, 0, 0}), 9);
        source->mControllerSequences[0].mCycleType = 0;
        ESM4::RuntimeHeldBlockSequence held{"held", .25f};
        MWRender::OblivionHeldPoseBindings bindings{std::make_shared<const ESM4::RuntimeMeleePoseSnapshot>()};
        const auto old = bindings;
        held.mRetirement = ESM4::RuntimeReactionRetirement{1, {}};
        EXPECT_THROW(MWRender::oblivionHeldBlockLayers(*mAnimation, {&held, 1}, 1, bindings), std::invalid_argument);
        EXPECT_EQ(bindings, old);
        held.mRetirement.reset(); held.mAnimationGroup = "missing";
        EXPECT_THROW(MWRender::oblivionHeldBlockLayers(*mAnimation, {&held, 1}, 1, bindings), std::invalid_argument);
        EXPECT_EQ(bindings, old);
        held.mAnimationGroup = "held";
        auto layers = MWRender::oblivionHeldBlockLayers(*mAnimation, {&held, 1}, 1, bindings);
        ASSERT_TRUE(mAnimation->setNativeSequenceLayers(layers));
        auto bad = layers; bad.push_back(bad.front());
        EXPECT_THROW(mAnimation->setNativeSequenceLayers(bad), std::invalid_argument);
        EXPECT_EQ(mAnimation->getCurrentTime("held"), .25f);
        bad = layers; bad[0].mFollowAnimationTime = true;
        EXPECT_THROW(mAnimation->setNativeSequenceLayers(bad), std::invalid_argument);
        EXPECT_EQ(mAnimation->getCurrentTime("held"), .25f);
        source->mControllerSequences[0].mCycleType = 2;
        EXPECT_THROW(MWRender::oblivionHeldBlockLayers(*mAnimation, {&held, 1}, 1, bindings), std::invalid_argument);
        EXPECT_THROW(MWRender::oblivionHeldBlockLayers(*mAnimation, {&held, 1}, -1, bindings), std::invalid_argument);
        held.mInactive = true;
        EXPECT_TRUE(MWRender::oblivionHeldBlockLayers(*mAnimation, {&held, 1}, 1, bindings).empty());
        EXPECT_FALSE(bindings[0]);
    }

}

namespace
{
    TEST_F(PhysicalPoseAnimationTest, PreparedNativeFramesRetainClocksAndPoseLayersWithoutOrdinaryRebuilds)
    {
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        auto track = nativeTrack({1, 0, 0}); track->mVelocity = {4, 0, 0};
        mAnimation->addNativeSource("prepared-idle", track, 20);
        const auto builds = mAnimation->mControllerBuilds;
        MWRender::Animation::NativeSequenceLayer layer{"prepared-idle", MWRender::BlendMask_All, .25f};
        layer.mExposeSampleTime = true;
        const auto before = mBone->getMatrix();
        auto prepared = mAnimation->prepareNativeSequenceLayers(std::span(&layer, 1)); ASSERT_TRUE(prepared);
        EXPECT_EQ(mBone->getMatrix(), before); EXPECT_EQ(track->mObserved->mSamples, 0);
        ASSERT_TRUE(prepared->commit()); EXPECT_EQ(mAnimation->mControllerBuilds, builds);
        traverse(); EXPECT_FLOAT_EQ(track->mObserved->mTime, .25f);
        EXPECT_FLOAT_EQ(mBone->getControllerTransform().mTranslation.x(), 2);
        const auto callback = osg::ref_ptr(mBone->getUpdateCallback());
        layer.mTime = .75f; prepared = mAnimation->prepareNativeSequenceLayers(std::span(&layer, 1)); ASSERT_TRUE(prepared);
        EXPECT_FLOAT_EQ(mAnimation->getCurrentTime("prepared-idle"), .25f);
        ASSERT_TRUE(prepared->commit()); EXPECT_EQ(mBone->getUpdateCallback(), callback);
        mFrameStamp->setFrameNumber(1); traverse();
        EXPECT_FLOAT_EQ(track->mObserved->mTime, .75f); EXPECT_FLOAT_EQ(mAnimation->getCurrentTime("prepared-idle"), .75f);
        EXPECT_FLOAT_EQ(mBone->getControllerTransform().mTranslation.x(), 4);
        const auto captured = mAnimation->captureNativeSequencePose("prepared-idle", 0); ASSERT_TRUE(captured);
        std::array<MWRender::Animation::NativeSequenceLayer, 2> layers{{
            {"prepared-idle", MWRender::BlendMask_All, 0, 1, .5f, false,
                std::make_shared<const ESM4::RuntimeMeleePoseSnapshot>(*captured)},
            {"prepared-idle", MWRender::BlendMask_All, 1, 1, .5f}}};
        prepared = mAnimation->prepareNativeSequenceLayers(layers); ASSERT_TRUE(prepared); ASSERT_TRUE(prepared->commit());
        mFrameStamp->setFrameNumber(2); traverse();
        EXPECT_FLOAT_EQ(track->mObserved->mTime, 1); EXPECT_FLOAT_EQ(mBone->getControllerTransform().mTranslation.x(), 4.5f);
        EXPECT_EQ(mAnimation->mControllerBuilds, builds);
        const auto samples = track->mObserved->mSamples;
        auto release = mAnimation->prepareNativeSequenceLayers({}); ASSERT_TRUE(release); ASSERT_TRUE(release->commit());
        mFrameStamp->setFrameNumber(3); traverse();
        EXPECT_EQ(track->mObserved->mSamples, samples); EXPECT_EQ(mAnimation->mControllerBuilds, builds);
    }
}

namespace
{
    TEST_F(PhysicalPoseAnimationTest, PreparedNativeProjectionPublishesInsideOwnedSceneFrameBeforeCollision)
    {
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        auto track = nativeTrack({1, 0, 0}); track->mVelocity = {4, 0, 0};
        mAnimation->addNativeSource("scene-prepared", track, 20);
        const auto builds = mAnimation->mControllerBuilds;
        auto placement = Nif::NiTransform::getIdentity(); placement.mTranslation = {100, 0, 0};
        unsigned frames = 0, collisions = 0;
        auto scene = mAnimation->prepareNativeScene(mGraph, placement, [&](const auto&, const auto&, unsigned) {
            ++frames;
            MWRender::Animation::NativeSequenceLayer layer{"scene-prepared", MWRender::BlendMask_All, frames * .25f};
            layer.mExposeSampleTime = true;
            auto projection = mAnimation->prepareNativeSequenceLayers(std::span(&layer, 1));
            if (!projection || !projection->commit()) throw std::runtime_error("owned scene projection failed");
            MWRender::Animation::NativeSceneFrame frame; frame.mPlacement = placement;
            frame.mCollision = [&](std::uint32_t record, const Nif::NiTransform& animated) {
                ++collisions; EXPECT_EQ(record, 8);
                EXPECT_FLOAT_EQ(animated.mTranslation.x(), 101 + frames);
                return SceneUtil::NifSceneTraversal::CollisionResult{};
            };
            return frame;
        });
        ASSERT_TRUE(mAnimation->commitNativeScene(*scene));
        const auto rootCallback = osg::ref_ptr(mRoot->getUpdateCallback());
        for (unsigned frame = 1; frame <= 2; ++frame)
        {
            mFrameStamp->setFrameNumber(frame); traverse();
            EXPECT_EQ(frames, frame); EXPECT_EQ(collisions, frame);
            EXPECT_EQ(mRoot->getUpdateCallback(), rootCallback);
            EXPECT_FLOAT_EQ(track->mObserved->mTime, frame * .25f);
            EXPECT_FLOAT_EQ(mBone->getControllerTransform().mTranslation.x(), 1 + frame);
            EXPECT_EQ(mAnimation->mControllerBuilds, builds);
        }
        MWRender::Animation::NativeSequenceLayer layer{"scene-prepared", MWRender::BlendMask_All, .75f};
        auto stale = mAnimation->prepareNativeSequenceLayers(std::span(&layer, 1)); ASSERT_TRUE(stale);
        mAnimation->endNativeScene(); EXPECT_FALSE(stale->isValid()); EXPECT_FALSE(stale->commit());
        EXPECT_FLOAT_EQ(mAnimation->getCurrentTime("scene-prepared"), .5f);
    }
}

namespace
{
    TEST_F(PhysicalPoseAnimationTest, PreparedProjectionPreservesCompatibleSharedAdditionalCallbacks)
    {
        mAnimation->mWriter->mWriteTranslation = mAnimation->mWriter->mWriteRotation = false;
        osg::ref_ptr<NifOsg::MatrixTransform> other = new NifOsg::MatrixTransform;
        other->setName("Other"); other->setTranslation({0, 0, 0}); other->setRotation(osg::Quat()); other->setScale(1);
        mRoot->addChild(other);
        osg::ref_ptr<SceneCounter> shared = new SceneCounter;
        mAnimation->mBeforeControllers = {{mBone, shared}, {other, shared}};
        mAnimation->rebuildControllers(); const auto builds = mAnimation->mControllerBuilds;
        const auto tail = osg::ref_ptr(shared->getNestedCallback());
        auto empty = mAnimation->prepareNativeSequenceLayers({}); ASSERT_TRUE(empty); ASSERT_TRUE(empty->commit());
        EXPECT_EQ(mBone->getUpdateCallback(), shared); EXPECT_EQ(other->getUpdateCallback(), shared);
        auto track = nativeTrack({7, 0, 0}); mAnimation->addNativeSource("shared-compatible", track, 20);
        MWRender::Animation::NativeSequenceLayer layer{"shared-compatible", MWRender::BlendMask_All, 0};
        auto prepared = mAnimation->prepareNativeSequenceLayers(std::span(&layer, 1)); ASSERT_TRUE(prepared);
        ASSERT_TRUE(prepared->commit()); EXPECT_EQ(mAnimation->mControllerBuilds, builds);
        EXPECT_EQ(mBone->getUpdateCallback()->getNestedCallback(), shared);
        EXPECT_EQ(other->getUpdateCallback(), shared); EXPECT_EQ(shared->getNestedCallback(), tail);
        traverse(); EXPECT_EQ(shared->mVisits, 2);
        EXPECT_FLOAT_EQ(mBone->getControllerTransform().mTranslation.x(), 7);
        EXPECT_FLOAT_EQ(other->getControllerTransform().mTranslation.x(), 0);
        auto release = mAnimation->prepareNativeSequenceLayers({}); ASSERT_TRUE(release); ASSERT_TRUE(release->commit());
        EXPECT_EQ(mBone->getUpdateCallback(), shared); EXPECT_EQ(other->getUpdateCallback(), shared);
        EXPECT_EQ(shared->getNestedCallback(), tail); EXPECT_EQ(mAnimation->mControllerBuilds, builds);
    }
}
