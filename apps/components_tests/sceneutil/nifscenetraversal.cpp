#include <components/sceneutil/nifscenetraversal.hpp>
#include <components/sceneutil/nodecallback.hpp>
#include <components/sceneutil/skeleton.hpp>
#include <components/nifosg/controller.hpp>
#include <components/nifosg/matrixtransform.hpp>

#include <gtest/gtest.h>

#include <osg/FrameStamp>
#include <osg/StateSet>
#include <osgUtil/UpdateVisitor>

#include <stdexcept>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace
{
    using Node = NifOsg::MatrixTransform;
    using Binding = SceneUtil::NifSceneTraversal;

    osg::ref_ptr<Node> makeNode(float x = 0)
    {
        osg::ref_ptr<Node> node = new Node(Nif::NiTransform::getIdentity());
        node->setTranslation({x, 0, 0});
        return node;
    }

    struct Callback : SceneUtil::NodeCallback<Callback, Node*>
    {
        std::function<void(Node&, osg::NodeVisitor&)> mBody;
        bool mAfterTraverse;
        Callback(std::function<void(Node&, osg::NodeVisitor&)> body, bool after = false)
            : mBody(std::move(body)), mAfterTraverse(after) {}
        void operator()(Node* node, osg::NodeVisitor* visitor)
        {
            if (mAfterTraverse) traverse(node, visitor);
            mBody(*node, *visitor);
            if (!mAfterTraverse) traverse(node, visitor);
        }
    };

    struct PropertyCallback : osg::StateSet::Callback
    {
        std::function<void(osg::NodeVisitor&)> mBody;
        explicit PropertyCallback(std::function<void(osg::NodeVisitor&)> body) : mBody(std::move(body)) {}
        void operator()(osg::StateSet*, osg::NodeVisitor* visitor) override
        {
            mBody(*visitor);
        }
    };

    struct NifSceneTraversalTest : ::testing::Test
    {
        osg::ref_ptr<Node> mRoot = makeNode(10), mChild = makeNode(5), mLeaf = makeNode(2);
        osg::FrameStamp mStamp;
        NifSceneTraversalTest()
        {
            mRoot->addChild(mChild);
            mChild->addChild(mLeaf);
            mStamp.setFrameNumber(1);
            mStamp.setSimulationTime(1. / 60.);
        }
    };

    TEST_F(NifSceneTraversalTest, PropertyCallbacksPrecedeOwnPostTraverseWorkAndProjection)
    {
        Binding binding(*mRoot);
        std::vector<std::string> events;
        mRoot->getOrCreateStateSet()->setUpdateCallback(new PropertyCallback([&](osg::NodeVisitor& visitor) {
            EXPECT_EQ(visitor.getTraversalNumber(), 42u);
            EXPECT_EQ(visitor.getFrameStamp()->getSimulationTime(), mStamp.getSimulationTime());
            events.push_back("root property");
        }));
        mRoot->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) {
            events.push_back("root node");
        }, true));
        mChild->getOrCreateStateSet()->setUpdateCallback(new PropertyCallback([&](osg::NodeVisitor&) {
            events.push_back("child property");
        }));
        mChild->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) {
            events.push_back("child node");
        }, true));
        Binding::Callbacks callbacks;
        callbacks.mProjected = [&](Node& node, const auto&, auto) {
            events.push_back(&node == mRoot ? "root projection" : &node == mChild ? "child projection" : "leaf projection");
        };
        binding.update(mStamp, 42, callbacks);
        EXPECT_EQ(events, (std::vector<std::string>{"root property", "root node", "root projection",
            "child property", "child node", "child projection", "leaf projection"}));
    }

    TEST_F(NifSceneTraversalTest, PhysicalProjectionInvalidatesExistingPaletteInTheSameTraversal)
    {
        osg::ref_ptr<SceneUtil::Skeleton> skeleton = new SceneUtil::Skeleton;
        skeleton->addChild(mRoot);
        mChild->setName("bone");
        auto* bone = skeleton->getBone("bone");
        ASSERT_NE(bone, nullptr);
        skeleton->updateBoneMatrices(42);
        EXPECT_FLOAT_EQ(bone->mMatrixInSkeletonSpace.getTrans().x(), 15);
        Binding binding(*mRoot);
        mRoot->setUpdateCallback(new Callback([](Node& node, osg::NodeVisitor&) { node.setTranslation({20, 0, 0}); }));
        Binding::Callbacks callbacks;
        callbacks.mCollisionFlags = [&](const Node& node) -> std::optional<std::uint16_t> {
            if (&node == mChild) return 1;
            return {};
        };
        callbacks.mPhysicalWorld = [](const Node&) {
            auto pose = Nif::NiTransform::getIdentity();
            pose.mTranslation = {50, 0, 0};
            return pose;
        };
        callbacks.mProjected = [&](Node& node, const auto&, auto) {
            if (&node != mChild) return;
            EXPECT_EQ(skeleton->getBone("bone"), bone);
            skeleton->updateBoneMatrices(42);
            EXPECT_FLOAT_EQ(bone->mMatrixInSkeletonSpace.getTrans().x(), 50);
        };
        binding.update(mStamp, 42, callbacks);
        skeleton->updateBoneMatrices(42);
        EXPECT_FLOAT_EQ(bone->mMatrixInSkeletonSpace.getTrans().x(), 50);
        EXPECT_FLOAT_EQ(mChild->getMatrix().getTrans().x(), 5);
    }

    TEST_F(NifSceneTraversalTest, SelfDetachingControllerRemainsAliveUntilHierarchyRejection)
    {
        Binding binding(*mRoot);
        osg::observer_ptr<Node> child = mChild;
        mChild->setUpdateCallback(new Callback([&](Node& node, osg::NodeVisitor&) {
            mRoot->removeChild(&node);
            EXPECT_TRUE(child.valid());
        }));
        mChild = nullptr;
        EXPECT_THROW(binding.update(mStamp, 42), std::invalid_argument);
        EXPECT_FALSE(child.valid());
        EXPECT_FALSE(binding.isValid());
    }

    TEST_F(NifSceneTraversalTest, DetachingReferenceKeepsBorrowedPlacementAncestorsAliveUntilRejection)
    {
        osg::ref_ptr<SceneUtil::Skeleton> skeleton = new SceneUtil::Skeleton;
        skeleton->addChild(mRoot);
        osg::observer_ptr<SceneUtil::Skeleton> ancestor = skeleton;
        Binding binding(*mRoot);
        mRoot->setUpdateCallback(new Callback([&](Node& node, osg::NodeVisitor&) {
            skeleton->removeChild(&node);
            skeleton = nullptr;
            EXPECT_TRUE(ancestor.valid());
        }));
        EXPECT_THROW(binding.update(mStamp, 42), std::invalid_argument);
        EXPECT_FALSE(ancestor.valid());
        mRoot->setUpdateCallback(nullptr);
        EXPECT_NO_THROW(binding.update(mStamp, 42));
    }

    TEST_F(NifSceneTraversalTest, PhysicalProviderCannotRetireChildrenDuringProjection)
    {
        Binding binding(*mRoot);
        osg::observer_ptr<Node> child = mChild;
        Binding::Callbacks callbacks;
        callbacks.mCollisionFlags = [&](const Node& node) -> std::optional<std::uint16_t> {
            if (&node == mRoot) return 1;
            return {};
        };
        callbacks.mPhysicalWorld = [&](const Node&) {
            mRoot->removeChild(mChild);
            mChild = nullptr;
            return Nif::NiTransform::getIdentity();
        };
        EXPECT_THROW(binding.update(mStamp, 42, callbacks), std::invalid_argument);
        EXPECT_FALSE(child.valid());
        EXPECT_FALSE(binding.isValid());
    }

    TEST_F(NifSceneTraversalTest, OwnControllersSeePreviousWorldAfterParentUpdatesAndBeforeChildComposition)
    {
        Binding binding(*mRoot);
        std::vector<std::string> events;
        mRoot->setUpdateCallback(new Callback([&](Node& node, osg::NodeVisitor&) {
            events.push_back("root controller");
            node.setTranslation({20, 0, 0});
        }));
        mChild->setUpdateCallback(new Callback([&](Node& node, osg::NodeVisitor& visitor) {
            events.push_back("child controller");
            EXPECT_EQ(binding.cachedWorld(*mRoot).mTranslation.x(), 20);
            EXPECT_EQ(binding.cachedWorld(node).mTranslation.x(), 15);
            EXPECT_NEAR(node.getWorldMatrices()[0].getTrans().x(), 15, 1e-10);
            EXPECT_NEAR(osg::computeLocalToWorld(visitor.getNodePath()).getTrans().x(), 15, 1e-10);
        }));
        mLeaf->setUpdateCallback(new Callback([&](Node& node, osg::NodeVisitor&) {
            events.push_back("leaf controller");
            EXPECT_EQ(binding.cachedWorld(*mChild).mTranslation.x(), 25);
            EXPECT_EQ(binding.cachedWorld(node).mTranslation.x(), 17);
            EXPECT_NEAR(node.getWorldMatrices()[0].getTrans().x(), 17, 1e-10);
        }));
        Binding::Callbacks callbacks;
        callbacks.mProjected = [&](Node& node, const auto&, auto) {
            events.push_back(&node == mRoot ? "root projected" : &node == mChild ? "child projected" : "leaf projected");
        };
        binding.update(mStamp, 7, callbacks);
        EXPECT_EQ(events, (std::vector<std::string>{"root controller", "root projected", "child controller",
            "child projected", "leaf controller", "leaf projected"}));
        EXPECT_EQ(binding.cachedWorld(*mLeaf).mTranslation.x(), 27);
        EXPECT_NEAR(mLeaf->getWorldMatrices()[0].getTrans().x(), 27, 1e-10);
        EXPECT_EQ(mChild->getMatrix().getTrans().x(), 5);
    }

    TEST_F(NifSceneTraversalTest, ActiveAndInactiveFlagsRetainSeparateLocalsAndRebaseUnvisitedChildren)
    {
        for (std::uint16_t flags : {0, 1, 8, 9})
        {
            SCOPED_TRACE(flags);
            auto root = makeNode(10), child = makeNode(5), leaf = makeNode(2);
            root->addChild(child); child->addChild(leaf);
            Binding binding(*root);
            root->setUpdateCallback(new Callback([](Node& node, osg::NodeVisitor&) { node.setTranslation({20, 0, 0}); }));
            unsigned physicalReads = 0;
            Binding::Callbacks callbacks;
            callbacks.mCollisionFlags = [&](const Node& node) -> std::optional<std::uint16_t> {
                if (&node == child) return flags;
                return {};
            };
            callbacks.mPhysicalWorld = [&](const Node&) {
                ++physicalReads;
                EXPECT_TRUE(flags & 1);
                auto pose = Nif::NiTransform::getIdentity(); pose.mTranslation.x() = 50;
                return pose;
            };
            callbacks.mProjected = [&](Node& node, const auto& frame, auto) {
                if (&node != child) return;
                EXPECT_EQ(frame.mWorldChangeMask, flags & 1 ? 1 : 0);
                EXPECT_EQ(binding.cachedWorld(*leaf).mTranslation.x(), 17);
                EXPECT_NEAR(leaf->getWorldMatrices()[0].getTrans().x(), 17, 1e-10);
            };
            binding.update(mStamp, 1, callbacks);
            EXPECT_EQ(physicalReads, unsigned(flags & 1));
            EXPECT_EQ(binding.cachedWorld(*child).mTranslation.x(), flags & 1 ? 50 : 25);
            EXPECT_EQ(child->getMatrix().getTrans().x(), flags == 9 ? 30 : 5);
            EXPECT_EQ(binding.cachedWorld(*leaf).mTranslation.x(), flags & 1 ? 52 : 27);
        }
    }

    TEST_F(NifSceneTraversalTest, SubthresholdLocalWriteIsRecomposedOnTheNextPass)
    {
        mRoot->setScale(2); mChild->setTranslation({2, 0, 0}); mChild->setScale(.25f);
        Binding binding(*mRoot);
        Binding::Callbacks callbacks;
        callbacks.mCollisionFlags = [&](const Node& node) -> std::optional<std::uint16_t> {
            return &node == mChild ? std::optional<std::uint16_t>(9) : std::nullopt;
        };
        callbacks.mPhysicalWorld = [](const Node&) {
            auto desired = Nif::NiTransform::getIdentity(); desired.mTranslation.x() = 14.005f;
            return desired;
        };
        callbacks.mProjected = [&](Node& node, const auto& frame, auto) {
            if (&node == mChild)
            {
                EXPECT_EQ(frame.mWorldChangeMask, 0);
            }
        };
        binding.update(mStamp, 1, callbacks);
        EXPECT_EQ(binding.cachedWorld(*mChild).mTranslation.x(), 14);
        EXPECT_GT(mChild->getMatrix().getTrans().x(), 2);
        EXPECT_EQ(mChild->mScale, .25f);
        binding.update(mStamp, 1, callbacks); // Explicit native passes need not have distinct frame IDs.
        EXPECT_EQ(binding.cachedWorld(*mChild).mTranslation.x(), 14.005f);
        EXPECT_EQ(binding.cachedWorld(*mChild).mScale, .5f);
    }

    TEST_F(NifSceneTraversalTest, RealRollFinishesPostTraverseWorkBeforeProjectionAndChildren)
    {
        Nif::NiFloatInterpolator interpolator;
        interpolator.mRecordType = Nif::RC_NiFloatInterpolator;
        interpolator.mData = nullptr; interpolator.mDefaultValue = .5f;
        Nif::NiRollController source;
        source.mData = nullptr; source.mInterpolator = &interpolator;
        class Time : public SceneUtil::ControllerSource
        {
            float getValue(osg::NodeVisitor*) override { return 1; }
        };
        osg::ref_ptr<NifOsg::RollController> roll = new NifOsg::RollController(&source);
        roll->setSource(std::make_shared<Time>());
        mRoot->setUpdateCallback(roll);
        Binding binding(*mRoot);
        unsigned visits = 0;
        mChild->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) {
            ++visits;
            EXPECT_NEAR(binding.cachedWorld(*mRoot).mRotation.mValues[0][0], std::cos(.5), 1e-6);
        }));
        binding.update(mStamp, 5);
        EXPECT_EQ(visits, 1);
        EXPECT_NEAR(mRoot->getMatrix()(0, 0), std::cos(.5), 1e-6);
    }

    TEST_F(NifSceneTraversalTest, RealLookAtReadsPreviousOwnCacheAndCurrentParentCache)
    {
        mRoot->setTranslation({0, 0, 0});
        Nif::NiLookAtController source; source.mLookAtFlags = Nif::NiLookAtController::Flag_Flip;
        osg::ref_ptr<NifOsg::LookAtController> look = new NifOsg::LookAtController(source);
        look->setTarget(mRoot);
        mChild->setUpdateCallback(look);
        mRoot->setUpdateCallback(new Callback([](Node& node, osg::NodeVisitor&) { node.setTranslation({100, 0, 0}); }));
        Binding binding(*mRoot);
        binding.update(mStamp, 1);
        // Existing look-at uses old own position5 and updated target100. A
        // premature recomposition to105 would reverse its computed direction.
        EXPECT_NEAR(mChild->getMatrix()(0, 0), 1, 1e-6);
        EXPECT_EQ(binding.cachedWorld(*mChild).mTranslation.x(), 105);
    }

    TEST_F(NifSceneTraversalTest, ReferenceRootLookAtIgnoresSyntheticPlacementAsANativeParent)
    {
        osg::ref_ptr<osg::MatrixTransform> placement = new osg::MatrixTransform(
            osg::Matrixd::rotate(osg::PI_2, osg::Vec3d(0, 0, 1)) * osg::Matrixd::translate(100, 0, 0));
        placement->addChild(mRoot);
        mRoot->setTranslation({100, 0, 0}); mRoot->setRotation(osg::Quat(osg::PI_2, osg::Vec3d(0, 0, 1)));
        mChild->setTranslation({0, -10, 0});
        Nif::NiLookAtController source; source.mLookAtFlags = Nif::NiLookAtController::Flag_Flip;
        osg::ref_ptr<NifOsg::LookAtController> look = new NifOsg::LookAtController(source);
        look->setTarget(mChild); mRoot->setUpdateCallback(look);
        Binding binding(*mRoot);
        binding.update(mStamp, 1);
        EXPECT_NEAR(mRoot->getMatrix()(0, 0), 1, 1e-6);
        EXPECT_NEAR(binding.cachedWorld(*mRoot).mRotation.mValues[0][0], 1, 1e-6);
        EXPECT_NEAR(mRoot->getWorldMatrices()[0].getTrans().x(), 100, 1e-10);
        osgUtil::UpdateVisitor ordinary;
        EXPECT_FALSE(Binding::isReferenceRoot(&ordinary, mRoot));
    }

    TEST_F(NifSceneTraversalTest, PlacementChangesBetweenPassesKeepCachedGlobalUntilOwnControllersRun)
    {
        osg::ref_ptr<osg::MatrixTransform> placement = new osg::MatrixTransform(osg::Matrixd::translate(100, 0, 0));
        placement->addChild(mRoot);
        Binding binding(*mRoot);
        placement->setMatrix(osg::Matrixd::translate(-200, 0, 0));
        mRoot->setUpdateCallback(new Callback([&](Node& node, osg::NodeVisitor&) {
            EXPECT_NEAR(node.getWorldMatrices()[0].getTrans().x(), 10, 1e-10);
            EXPECT_EQ(binding.cachedWorld(node).mTranslation.x(), 10);
        }));
        binding.update(mStamp, 1);
        EXPECT_NEAR(mLeaf->getWorldMatrices()[0].getTrans().x(), 17, 1e-10);
    }

    TEST_F(NifSceneTraversalTest, ConstructorRejectsInvalidLateNodesAndExistingProjectionWithoutPrefixWrites)
    {
        mLeaf->setScale(0);
        EXPECT_THROW((Binding(*mRoot)), std::invalid_argument);
        EXPECT_EQ(mRoot->getTraversalMatrix().getMatrix(), nullptr);
        EXPECT_EQ(mChild->getTraversalMatrix().getMatrix(), nullptr);
        mLeaf->setScale(1);
        mLeaf->setTraversalMatrix(osg::Matrixd::translate(99, 0, 0));
        const auto saved = mLeaf->getTraversalMatrix();
        EXPECT_THROW((Binding(*mRoot)), std::invalid_argument);
        EXPECT_EQ(mRoot->getTraversalMatrix().getMatrix(), nullptr);
        EXPECT_EQ(mLeaf->getTraversalMatrix(), saved);
    }

    TEST_F(NifSceneTraversalTest, ControllerExceptionDoesNotAdvanceChildrenAndAllowsAnExplicitRetry)
    {
        Binding binding(*mRoot);
        unsigned children = 0;
        mChild->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) { ++children; }));
        mRoot->setUpdateCallback(new Callback([](Node&, osg::NodeVisitor&) {
            throw std::runtime_error("controller failure");
        }, true));
        EXPECT_THROW(binding.update(mStamp, 1), std::runtime_error);
        EXPECT_EQ(children, 0);
        EXPECT_TRUE(binding.isValid());
        mRoot->setUpdateCallback(nullptr);
        binding.update(mStamp, 2);
        EXPECT_EQ(children, 1);
    }

    TEST_F(NifSceneTraversalTest, RejectsDetachedNodesAndExternalProjectionChangesBeforeControllers)
    {
        Binding binding(*mRoot);
        unsigned calls = 0;
        mRoot->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) { ++calls; }));
        mChild->clearTraversalMatrix();
        EXPECT_FALSE(binding.isValid());
        EXPECT_THROW(binding.update(mStamp, 1), std::invalid_argument);
        EXPECT_EQ(calls, 0);
        mRoot->removeChild(mChild);
        EXPECT_FALSE(binding.isValid());
    }

    TEST_F(NifSceneTraversalTest, RejectsReentrantUpdatesAndUnsettledFlagsWithoutCallingPhysics)
    {
        Binding binding(*mRoot);
        Binding::Callbacks callbacks;
        callbacks.mProjected = [&](Node& node, const auto&, auto) {
            if (&node == mRoot)
            {
                EXPECT_THROW(binding.update(mStamp, 2), std::invalid_argument);
            }
        };
        binding.update(mStamp, 1, callbacks);
        callbacks.mCollisionFlags = [](const Node&) -> std::optional<std::uint16_t> { return 0x41; };
        callbacks.mPhysicalWorld = [](const Node&) -> Nif::NiTransform {
            ADD_FAILURE() << "pending flags must not read physical pose";
            return Nif::NiTransform::getIdentity();
        };
        EXPECT_THROW(binding.update(mStamp, 3, callbacks), std::invalid_argument);
    }

    TEST_F(NifSceneTraversalTest, SeparateNifRotationSurvivesNonunitDrawingScaleWithoutAnInverseRoundtrip)
    {
        // Original node-controller-order-oracle-01 frame4 root local. Dividing
        // its rounded drawing matrix by1.25 loses one bit of rotation lane4.
        const std::array<std::uint32_t, 9> rotation{0x3f3b0df4u, 0xbe974e6du, 0x3f1d8dc8u,
            0x3e677372u, 0x3f7490efu, 0x3e42f2ceu, 0xbf24eb73u, 0u, 0x3f43ccb3u};
        Nif::Matrix3 native;
        for (unsigned i = 0; i < 9; ++i) native.mValues[i / 3][i % 3] = std::bit_cast<float>(rotation[i]);
        mRoot->setScale(1.25f);
        mRoot->setRotation(native);
        Binding binding(*mRoot);
        for (unsigned i = 0; i < 9; ++i)
            EXPECT_EQ(std::bit_cast<std::uint32_t>(binding.cachedWorld(*mRoot).mRotation.mValues[i / 3][i % 3]),
                rotation[i]);
    }

    TEST_F(NifSceneTraversalTest, RealKeyframeMissingChannelsKeepAuthorRotationAndScaleUnderWorldOnlyPhysics)
    {
        constexpr float unset = -std::numeric_limits<float>::max();
        Nif::NiTransformInterpolator interpolator;
        interpolator.mRecordType = Nif::RC_NiTransformInterpolator; interpolator.mData = nullptr;
        interpolator.mDefaultValue = {osg::Vec3f(7, 8, 9), osg::Quat(unset, unset, unset, unset), unset};
        class Time : public SceneUtil::ControllerSource
        {
            float getValue(osg::NodeVisitor*) override { return .5f; }
        };
        osg::ref_ptr<NifOsg::KeyframeController> controller
            = new NifOsg::KeyframeController({{0.f, 1.f, 0.f, 1.f, &interpolator}});
        controller->setSource(std::make_shared<Time>());
        mChild->setRotation(osg::Quat(.3, osg::Vec3d(0, 0, 1))); mChild->setScale(3);
        mChild->setUpdateCallback(controller);
        const auto rotation = mChild->mRotationScale;
        Binding binding(*mRoot);
        Binding::Callbacks callbacks;
        callbacks.mCollisionFlags = [&](const Node& node) -> std::optional<std::uint16_t> {
            return &node == mChild ? std::optional<std::uint16_t>(1) : std::nullopt;
        };
        callbacks.mPhysicalWorld = [&](const Node& node) {
            EXPECT_EQ(node.getMatrix().getTrans(), osg::Vec3d(7, 8, 9));
            auto pose = Nif::NiTransform::getIdentity(); pose.mTranslation = {100, 20, 30}; return pose;
        };
        binding.update(mStamp, 1, callbacks);
        EXPECT_EQ(mChild->getMatrix().getTrans(), osg::Vec3d(7, 8, 9));
        EXPECT_EQ(mChild->mScale, 3);
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned column = 0; column < 3; ++column)
                EXPECT_EQ(mChild->mRotationScale.mValues[row][column], rotation.mValues[row][column]);
        EXPECT_EQ(binding.cachedWorld(*mChild).mTranslation, osg::Vec3f(100, 20, 30));
    }
}
