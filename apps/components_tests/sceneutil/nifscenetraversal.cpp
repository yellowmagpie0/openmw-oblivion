#include <components/sceneutil/nifscenetraversal.hpp>
#include <components/sceneutil/nodecallback.hpp>
#include <components/sceneutil/skeleton.hpp>
#include <components/nifosg/controller.hpp>
#include <components/nifosg/matrixtransform.hpp>

#include <gtest/gtest.h>

#include <osg/FrameStamp>
#include <osg/Geometry>
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

    TEST_F(NifSceneTraversalTest, ObjectLocalTraversalInterleavesCollisionTargetsWithUpdatedParentCaches)
    {
        osg::ref_ptr<osg::MatrixTransform> scaffold = new osg::MatrixTransform(osg::Matrixd::translate(-300, 0, 0));
        scaffold->addChild(mRoot);
        auto placement = Nif::NiTransform::getIdentity(); placement.mTranslation = {100, 0, 0};
        Binding binding(*mRoot, placement);
        EXPECT_EQ(binding.cachedWorld(*mRoot).mTranslation.x(), 110);
        EXPECT_NEAR(mLeaf->getWorldMatrices()[0].getTrans().x(), 117, 1e-9);
        std::vector<std::string> events;
        mRoot->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor& visitor) {
            EXPECT_FALSE(Binding::isReferenceRoot(&visitor, mRoot));
            ASSERT_TRUE(Binding::nativeRootParentWorld(&visitor, mRoot));
            EXPECT_EQ(Binding::nativeRootParentWorld(&visitor, mRoot)->mTranslation.x(), 200);
            EXPECT_FALSE(Binding::nativeRootParentWorld(&visitor, mChild));
            EXPECT_EQ(binding.cachedWorld(*mRoot).mTranslation.x(), 110);
            EXPECT_NEAR(mRoot->getWorldMatrices()[0].getTrans().x(), 110, 1e-9);
            events.push_back("root controller");
        }, true));
        mChild->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) {
            EXPECT_EQ(binding.cachedWorld(*mRoot).mTranslation.x(), 250);
            EXPECT_EQ(binding.cachedWorld(*mChild).mTranslation.x(), 115);
            events.push_back("child controller");
        }));
        Binding::Callbacks callbacks;
        std::optional<Nif::NiTransform> physicalTarget;
        callbacks.mCollisionUpdate = [&](const Node& node, const auto& animated) {
            events.push_back(&node == mRoot ? "root collision" : &node == mChild ? "child collision" : "leaf collision");
            EXPECT_EQ(animated.mTranslation.x(), &node == mRoot ? 210 : &node == mChild ? 255 : 257);
            physicalTarget.reset();
            if (&node == mRoot)
            {
                physicalTarget = animated;
                physicalTarget->mTranslation.x() += 40;
            }
        };
        callbacks.mCollisionFlags = [&](const Node&) -> std::optional<std::uint16_t> {
            return physicalTarget ? std::optional<std::uint16_t>(1) : std::nullopt;
        };
        callbacks.mPhysicalWorld = [&](const Node&) {
            return physicalTarget.value();
        };
        placement.mTranslation.x() = 200;
        binding.updateObjectLocal(mStamp, 1, placement, callbacks);
        EXPECT_EQ(events, (std::vector<std::string>{"root controller", "root collision", "child controller",
            "child collision", "leaf collision"}));
        EXPECT_EQ(mRoot->getControllerTransform().mTranslation.x(), 10);
        EXPECT_EQ(binding.cachedWorld(*mLeaf).mTranslation.x(), 257);
        EXPECT_NEAR(mLeaf->getWorldMatrices()[0].getTrans().x(), 257, 1e-9);
    }

    TEST_F(NifSceneTraversalTest, ObjectLocalRootWritesLocalRelativeToNativeParentRatherThanDrawingAncestors)
    {
        osg::ref_ptr<osg::MatrixTransform> scaffold = new osg::MatrixTransform(osg::Matrixd::translate(-300, 0, 0));
        scaffold->addChild(mRoot);
        auto placement = Nif::NiTransform::getIdentity(); placement.mTranslation = {100, 0, 0}; placement.mScale = 2;
        Binding binding(*mRoot, placement);
        Binding::Callbacks callbacks;
        callbacks.mCollisionFlags = [&](const Node& node) -> std::optional<std::uint16_t> {
            return &node == mRoot ? std::optional<std::uint16_t>(9) : std::nullopt;
        };
        callbacks.mPhysicalWorld = [](const Node&) {
            auto world = Nif::NiTransform::getIdentity(); world.mTranslation = {150, 0, 0}; return world;
        };
        binding.updateObjectLocalWithoutControllers(mStamp, 1, placement, callbacks);
        EXPECT_EQ(mRoot->getControllerTransform().mTranslation.x(), 25);
        EXPECT_EQ(mRoot->mScale, 1);
        EXPECT_EQ(binding.cachedWorld(*mRoot).mScale, 2);
        EXPECT_EQ(binding.cachedWorld(*mLeaf).mTranslation.x(), 164);
        EXPECT_NEAR(mLeaf->getWorldMatrices()[0].getTrans().x(), 164, 1e-9);
    }

    TEST_F(NifSceneTraversalTest, ObjectLocalRootLookAtUsesExplicitNativeParentAndPreviousOwnCache)
    {
        osg::ref_ptr<osg::MatrixTransform> scaffold = new osg::MatrixTransform(osg::Matrixd::translate(-300, 0, 0));
        scaffold->addChild(mRoot);
        mRoot->setTranslation({0, 0, 0}); mChild->setTranslation({0, -10, 0});
        auto placement = Nif::NiTransform::getIdentity();
        placement.mTranslation = {100, 0, 0};
        placement.mRotation = Nif::NiTransform::getIdentity().mRotation;
        placement.mRotation.mValues[0][0] = 0; placement.mRotation.mValues[0][1] = -1;
        placement.mRotation.mValues[1][0] = 1; placement.mRotation.mValues[1][1] = 0;
        Binding binding(*mRoot, placement);
        Nif::NiLookAtController source; source.mLookAtFlags = Nif::NiLookAtController::Flag_Flip;
        osg::ref_ptr<NifOsg::LookAtController> look = new NifOsg::LookAtController(source);
        look->setTarget(mChild); mRoot->setUpdateCallback(look);
        binding.updateObjectLocal(mStamp, 1, placement);
        EXPECT_NEAR(mRoot->getMatrix()(0, 0), 0, 1e-6);
        EXPECT_NEAR(mRoot->getMatrix()(0, 1), -1, 1e-6);
        const auto world = binding.cachedWorld(*mRoot);
        EXPECT_NEAR(world.mRotation.mValues[0][0], 1, 1e-6);
        EXPECT_NEAR(world.mRotation.mValues[1][1], 1, 1e-6);
        EXPECT_EQ(world.mTranslation, osg::Vec3f(100, 0, 0));
    }

    TEST_F(NifSceneTraversalTest, ObjectLocalPlacementAndEntryGuardsRejectBeforeControllersOrProjections)
    {
        auto placement = Nif::NiTransform::getIdentity(); placement.mTranslation = {100, 0, 0};
        Binding binding(*mRoot, placement);
        unsigned calls = 0;
        mRoot->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) { ++calls; }));
        const auto projection = mRoot->getTraversalMatrix();
        EXPECT_THROW(binding.update(mStamp, 1), std::invalid_argument);
        EXPECT_THROW(binding.updateWithoutControllers(mStamp, 1), std::invalid_argument);
        EXPECT_THROW(binding.refreshReferenceRootCache(), std::invalid_argument);
        placement.mTranslation.x() = std::numeric_limits<float>::quiet_NaN();
        EXPECT_THROW(binding.updateObjectLocal(mStamp, 1, placement), std::invalid_argument);
        placement = Nif::NiTransform::getIdentity(); placement.mScale = 0;
        EXPECT_THROW(binding.updateObjectLocalWithoutControllers(mStamp, 1, placement), std::invalid_argument);
        EXPECT_EQ(calls, 0u); EXPECT_EQ(projection, mRoot->getTraversalMatrix());
        EXPECT_EQ(binding.cachedWorld(*mRoot).mTranslation.x(), 110);
        osgUtil::UpdateVisitor ordinary;
        EXPECT_FALSE(Binding::nativeRootParentWorld(&ordinary, mRoot));
        placement = Nif::NiTransform::getIdentity();
        binding.updateObjectLocal(mStamp, 1, placement);
        EXPECT_EQ(calls, 1u); EXPECT_EQ(binding.cachedWorld(*mRoot).mTranslation.x(), 10);
        auto other = makeNode(); Binding reference(*other);
        EXPECT_THROW(reference.updateObjectLocal(mStamp, 1, placement), std::invalid_argument);
    }

    TEST_F(NifSceneTraversalTest, ObjectLocalSelectedProjectionGatesCollisionUpdateAndSettlesFirstSynchronization)
    {
        auto placement = Nif::NiTransform::getIdentity(); placement.mTranslation = {100, 0, 0};
        Binding binding(*mRoot, placement);
        unsigned own = 0, dispatch = 0, sync = 0, physical = 0;
        std::uint16_t nodeFlags = 0, collisionFlags = 0x49;
        mRoot->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) { ++own; }));
        Binding::Callbacks callbacks;
        callbacks.mNodeFlags = [&](const Node& node) { return &node == mRoot ? nodeFlags : std::uint16_t(0); };
        callbacks.mCollisionUpdate = [&](const Node&, const auto& animated) {
            ++dispatch; EXPECT_EQ(animated.mTranslation.x(), 110);
        };
        callbacks.mCollisionFlags = [&](const Node&) { return std::optional(collisionFlags); };
        callbacks.mFirstSynchronization = [&](const Node&, const auto& animated, auto flags) {
            ++sync; EXPECT_EQ(animated.mTranslation.x(), 110); EXPECT_EQ(flags, 0x49);
            collisionFlags &= ~0x40;
        };
        callbacks.mPhysicalWorld = [&](const Node&) {
            ++physical; auto world = Nif::NiTransform::getIdentity(); world.mTranslation = {260, 0, 0}; return world;
        };
        binding.updateObjectLocal(mStamp, 1, placement, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(own, 1u); EXPECT_EQ(dispatch, 0u); EXPECT_EQ(sync, 0u); EXPECT_EQ(physical, 0u);
        nodeFlags = 4;
        binding.updateObjectLocal(mStamp, 2, placement, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(own, 2u); EXPECT_EQ(dispatch, 1u); EXPECT_EQ(sync, 1u); EXPECT_EQ(physical, 0u);
        EXPECT_EQ(collisionFlags, 9);
        binding.updateObjectLocal(mStamp, 3, placement, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(own, 3u); EXPECT_EQ(dispatch, 2u); EXPECT_EQ(sync, 1u); EXPECT_EQ(physical, 1u);
        EXPECT_EQ(mRoot->getControllerTransform().mTranslation.x(), 160);
        EXPECT_EQ(binding.cachedWorld(*mRoot).mTranslation.x(), 260);
        EXPECT_EQ(binding.cachedWorld(*mChild).mTranslation.x(), 115);
        EXPECT_NEAR(mChild->getWorldMatrices()[0].getTrans().x(), 115, 1e-9);
    }

    TEST_F(NifSceneTraversalTest, CollisionUpdateRejectsReentryAndRetiredHierarchyBeforePhysicalReads)
    {
        auto placement = Nif::NiTransform::getIdentity(); placement.mTranslation = {100, 0, 0};
        Binding binding(*mRoot, placement);
        unsigned flags = 0;
        Binding::Callbacks callbacks;
        callbacks.mCollisionUpdate = [&](const Node&, const auto&) {
            EXPECT_THROW(binding.updateObjectLocal(mStamp, 1, placement), std::invalid_argument);
            mRoot->removeChild(mChild);
        };
        callbacks.mCollisionFlags = [&](const Node&) -> std::optional<std::uint16_t> { ++flags; return {}; };
        EXPECT_THROW(binding.updateObjectLocal(mStamp, 1, placement, callbacks), std::invalid_argument);
        EXPECT_EQ(flags, 0u);
        EXPECT_EQ(binding.cachedWorld(*mRoot).mTranslation.x(), 110);
        mRoot->addChild(mChild);
        EXPECT_TRUE(binding.isValid());
        callbacks.mCollisionUpdate = {};
        binding.updateObjectLocal(mStamp, 1, placement, callbacks);
        EXPECT_EQ(flags, 3u);
    }

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

    TEST_F(NifSceneTraversalTest, ControllerlessFullProjectsNativeGenerationCachesAndResumesCallbacks)
    {
        osg::ref_ptr<osg::Group> scaffold = new osg::Group;
        mRoot->removeChild(mChild); mRoot->addChild(scaffold); scaffold->addChild(mChild);
        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        osg::ref_ptr<osg::Geometry> drawable = new osg::Geometry;
        mChild->addChild(geode); geode->addDrawable(drawable);
        Binding binding(*mRoot);
        unsigned own = 0, property = 0, drawing = 0;
        for (auto* node : {mRoot.get(), mChild.get(), mLeaf.get()})
        {
            node->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) { ++own; }, true));
            node->getOrCreateStateSet()->setUpdateCallback(new PropertyCallback([&](auto&) { ++property; }));
        }
        for (auto* node : {scaffold.get(), static_cast<osg::Group*>(geode.get())})
            node->getOrCreateStateSet()->setUpdateCallback(new PropertyCallback([&](auto&) { ++property; }));
        drawable->getOrCreateStateSet()->setUpdateCallback(new PropertyCallback([&](auto&) { ++property; }));
        struct Update : osg::DrawableUpdateCallback
        {
            unsigned& mCalls;
            explicit Update(unsigned& calls) : mCalls(calls) {}
            void update(osg::NodeVisitor*, osg::Drawable*) override { ++mCalls; }
        };
        drawable->setUpdateCallback(new Update(drawing));

        // Original reference-early-generation-trace-01 pre_scale world13,
        // before final reference scale/registration. Change locals after the
        // constructor so this checks an actual cache refresh, not initial caches.
        mRoot->setTranslation({10, 20, 30});
        auto rotation = Nif::NiTransform::getIdentity().mRotation;
        rotation.mValues[0][0] = 0; rotation.mValues[0][1] = 1;
        rotation.mValues[1][0] = -1; rotation.mValues[1][1] = 0;
        mChild->setRotation(rotation); mChild->setTranslation({3, 4, 5});
        std::vector<Node*> projections;
        Binding::Callbacks callbacks;
        // Full ignores selective flags, including a policy which admits no children.
        callbacks.mNodeFlags = [](const Node&) -> std::uint16_t { ADD_FAILURE(); return 0; };
        callbacks.mProjected = [&](Node& node, const auto&, auto) { projections.push_back(&node); };
        binding.updateWithoutControllers(osg::FrameStamp{}, 0, callbacks);
        EXPECT_EQ(own, 0u); EXPECT_EQ(property, 0u); EXPECT_EQ(drawing, 0u);
        EXPECT_EQ(projections, (std::vector<Node*>{mRoot, mChild, mLeaf}));
        const std::array<std::uint32_t, 13> expected{
            0,1065353216,0,3212836864,0,0,0,0,1065353216,1095761920,1103101952,1108082688,1065353216};
        const auto world = binding.cachedWorld(*mChild);
        for (unsigned i = 0; i < 9; ++i)
            EXPECT_EQ(std::bit_cast<std::uint32_t>(world.mRotation.mValues[i / 3][i % 3]), expected[i]);
        for (unsigned i = 0; i < 3; ++i)
            EXPECT_EQ(std::bit_cast<std::uint32_t>(world.mTranslation[i]), expected[9 + i]);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(world.mScale), expected[12]);
        EXPECT_EQ(mChild->getWorldMatrices().front(), osg::Matrixd(world.toMatrix()));
        EXPECT_EQ(mChild->getControllerTransform().mTranslation, osg::Vec3f(3, 4, 5));
        binding.update(mStamp, 1, callbacks);
        EXPECT_EQ(own, 3u); EXPECT_EQ(property, 6u); EXPECT_EQ(drawing, 1u);
    }

    TEST_F(NifSceneTraversalTest, ControllerlessFirstSynchronizationRetriesWithoutControllerSideEffects)
    {
        Binding binding(*mRoot);
        unsigned own = 0, force = 0, projected = 0, reads = 0, references = 0;
        mRoot->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) { ++own; }));
        mChild->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) { ++own; }));
        mRoot->setTranslation({20, 0, 0});
        std::uint16_t flags = 0x49;
        Binding::Callbacks callbacks;
        callbacks.mCollisionFlags = [&](const Node& node) -> std::optional<std::uint16_t> {
            return &node == mRoot ? std::optional(flags) : std::nullopt;
        };
        callbacks.mPhysicalWorld = [&](const Node&) { ++reads; return Nif::NiTransform::getIdentity(); };
        callbacks.mFirstSynchronization = [&](const Node&, const auto& world, auto) {
            ++force; EXPECT_EQ(world.mTranslation, osg::Vec3f(20, 0, 0));
            EXPECT_EQ(binding.cachedWorld(*mChild).mTranslation, osg::Vec3f(15, 0, 0));
            throw std::runtime_error("retry initial physical capture");
        };
        callbacks.mProjected = [&](Node&, const auto&, auto) { ++projected; };
        callbacks.mReferenceUpdate = [&](Node&, const auto&, auto) { ++references; };
        EXPECT_THROW(binding.updateWithoutControllers(osg::FrameStamp{}, 0, callbacks), std::runtime_error);
        EXPECT_EQ(flags, 0x49); EXPECT_EQ(force, 1u); EXPECT_EQ(projected, 0u); EXPECT_EQ(references, 0u);
        EXPECT_EQ(own, 0u); EXPECT_EQ(reads, 0u); EXPECT_TRUE(binding.isValid());
        callbacks.mFirstSynchronization = [&](const Node&, const auto&, auto) { ++force; flags &= ~0x40; };
        binding.updateWithoutControllers(osg::FrameStamp{}, 0, callbacks);
        EXPECT_EQ(flags, 9); EXPECT_EQ(force, 2u); EXPECT_EQ(projected, 3u); EXPECT_EQ(references, 1u);
        EXPECT_EQ(own, 0u); EXPECT_EQ(reads, 0u);
        EXPECT_EQ(binding.cachedWorld(*mChild).mTranslation, osg::Vec3f(25, 0, 0));
        EXPECT_EQ(mRoot->getControllerTransform().mTranslation, osg::Vec3f(20, 0, 0));
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

namespace
{
    TEST(NifSceneFirstSynchronization, PendingDispatchPublishesAnimatedWorldBeforeForceAndChildren)
    {
        // Exact before/after flag pairs from original first-sync corpus02.
        for (std::uint16_t initial : {0x40, 0x41, 0x48, 0x49, 0x4c, 0x4d})
        {
            SCOPED_TRACE(initial);
            auto root = makeNode(10), child = makeNode(5);
            root->addChild(child);
            Binding binding(*root);
            std::uint16_t flags = initial;
            unsigned forced = 0, physicalReads = 0;
            std::vector<std::string> events;
            root->getOrCreateStateSet()->setUpdateCallback(new PropertyCallback(
                [&](osg::NodeVisitor&) { events.push_back("property"); }));
            root->setUpdateCallback(new Callback([&](Node& node, osg::NodeVisitor&) {
                events.push_back("own"); node.setTranslation({20, 0, 0});
            }, true));
            child->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) { events.push_back("child"); }));
            Binding::Callbacks callbacks;
            callbacks.mCollisionFlags = [&](const Node& node) -> std::optional<std::uint16_t> {
                return &node == root ? std::optional(flags) : std::nullopt;
            };
            callbacks.mPhysicalWorld = [&](const Node&) {
                ++physicalReads;
                auto world = Nif::NiTransform::getIdentity(); world.mTranslation = {100, 0, 0}; return world;
            };
            callbacks.mFirstSynchronization = [&](const Node& node, const auto& world, std::uint16_t before) {
                events.push_back("force"); ++forced;
                EXPECT_EQ(&node, root.get()); EXPECT_EQ(before, initial);
                EXPECT_EQ(world.mTranslation, osg::Vec3f(20, 0, 0));
                EXPECT_EQ(binding.cachedWorld(node).mTranslation, world.mTranslation);
                EXPECT_EQ(node.getWorldMatrices().front().getTrans(), osg::Vec3d(20, 0, 0));
                // Native child cache still belongs to its preceding update.
                EXPECT_EQ(child->getWorldMatrices().front().getTrans(), osg::Vec3d(15, 0, 0));
                flags &= ~0x40;
            };
            callbacks.mProjected = [&](Node& node, const auto& frame, auto settled) {
                if (&node != root) return;
                events.push_back("projected");
                EXPECT_EQ(settled, std::optional<std::uint16_t>(initial & ~0x40));
                if (forced == 1 && physicalReads == 0)
                {
                    EXPECT_EQ(frame.mWorldChangeMask, 0);
                }
            };
            osg::FrameStamp stamp;
            binding.update(stamp, 1, callbacks);
            EXPECT_EQ(events, (std::vector<std::string>{"property", "own", "force", "projected", "child"}));
            EXPECT_EQ(forced, 1u); EXPECT_EQ(physicalReads, 0u);
            EXPECT_EQ(binding.cachedWorld(*child).mTranslation, osg::Vec3f(25, 0, 0));
            binding.update(stamp, 2, callbacks);
            EXPECT_EQ(forced, 1u); EXPECT_EQ(physicalReads, unsigned(initial & 1));
        }
    }

    TEST_F(NifSceneTraversalTest, FailedFirstSynchronizationLeavesPendingAndDoesNotVisitChildren)
    {
        Binding binding(*mRoot);
        std::uint16_t flags = 0x41;
        unsigned children = 0, projected = 0, forced = 0;
        mRoot->setUpdateCallback(new Callback([](Node& node, osg::NodeVisitor&) { node.setTranslation({20, 0, 0}); }));
        mChild->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) { ++children; }));
        Binding::Callbacks callbacks;
        callbacks.mCollisionFlags = [&](const Node& node) -> std::optional<std::uint16_t> {
            return &node == mRoot ? std::optional(flags) : std::nullopt;
        };
        callbacks.mProjected = [&](Node&, const auto&, auto) { ++projected; };
        callbacks.mFirstSynchronization = [&](const Node&, const auto&, auto) { ++forced; throw std::runtime_error("force"); };
        EXPECT_THROW(binding.update(mStamp, 1, callbacks), std::runtime_error);
        EXPECT_EQ(flags, 0x41); EXPECT_EQ(children, 0u); EXPECT_EQ(projected, 0u);
        EXPECT_TRUE(binding.isValid());
        EXPECT_EQ(binding.cachedWorld(*mRoot).mTranslation, osg::Vec3f(20, 0, 0));
        EXPECT_EQ(mChild->getWorldMatrices().front().getTrans(), osg::Vec3d(15, 0, 0));
        callbacks.mFirstSynchronization = [&](const Node&, const auto&, auto) { ++forced; flags &= ~0x40; };
        EXPECT_NO_THROW(binding.update(mStamp, 2, callbacks));
        EXPECT_EQ(forced, 2u); EXPECT_EQ(children, 1u); EXPECT_EQ(flags, 1);
    }

    TEST(NifSceneFirstSynchronization, ReinspectsExactFlagClearBeforePublicationCallback)
    {
        for (const auto invalid : {std::optional<std::uint16_t>{}, std::optional<std::uint16_t>{0x41},
                std::optional<std::uint16_t>{9}})
        {
            auto root = makeNode(10);
            Binding binding(*root);
            Binding::Callbacks callbacks;
            std::optional<std::uint16_t> flags = 0x41;
            unsigned projected = 0;
            callbacks.mCollisionFlags = [&](const Node& node) { return &node == root ? flags : std::nullopt; };
            callbacks.mFirstSynchronization = [&](const Node&, const auto&, auto) { flags = invalid; };
            callbacks.mProjected = [&](Node&, const auto&, auto) { ++projected; };
            EXPECT_THROW(binding.update(osg::FrameStamp{}, 1, callbacks), std::invalid_argument);
            EXPECT_EQ(projected, 0u);
        }
    }

    TEST_F(NifSceneTraversalTest, PendingBit8DoesNotRewriteProceduralControllerRotationCache)
    {
        osg::Matrixd raw;
        raw(0, 0) = raw(1, 1) = 0; raw(0, 1) = 1; raw(1, 0) = -1; raw(3, 0) = 20;
        mRoot->setMatrix(raw); // Deliberately leaves the separate NIF cache as identity.
        const auto before = mRoot->mRotationScale;
        Binding binding(*mRoot);
        std::uint16_t flags = 0x49;
        Binding::Callbacks callbacks;
        callbacks.mCollisionFlags = [&](const Node& node) -> std::optional<std::uint16_t> {
            return &node == mRoot ? std::optional(flags) : std::nullopt;
        };
        callbacks.mFirstSynchronization = [&](const Node&, const auto&, auto) { flags &= ~0x40; };
        binding.update(mStamp, 1, callbacks);
        EXPECT_EQ(flags, 9);
        EXPECT_EQ(mRoot->getMatrix(), raw);
        for (unsigned i = 0; i < 9; ++i)
            EXPECT_EQ(std::bit_cast<unsigned>(mRoot->mRotationScale.mValues[i / 3][i % 3]),
                std::bit_cast<unsigned>(before.mValues[i / 3][i % 3]));
    }
}

namespace
{
    TEST(NifSceneRegistrationCache, FinalScaleAndSelectedRootRefreshPreserveOriginalMixedCaches)
    {
        // Actual Generate/update/registration corpus: root authored scale1.75,
        // reference scale2. Registration refreshes a selected root but leaves
        // child world caches at their earlier scale until their own update.
        for (const bool selected : {false, true})
        {
            SCOPED_TRACE(selected);
            auto root = makeNode(), child = makeNode();
            root->setTranslation({10, 20, 30}); root->setScale(1.75f);
            auto rotation = Nif::NiTransform::getIdentity().mRotation;
            rotation.mValues[0][0] = rotation.mValues[1][1] = 0;
            rotation.mValues[0][1] = 1; rotation.mValues[1][0] = -1;
            child->setRotation(rotation); child->setTranslation({3, 4, 5}); root->addChild(child);
            Binding binding(*root);
            unsigned own = 0, properties = 0, children = 0;
            root->setUpdateCallback(new Callback([&](Node& node, osg::NodeVisitor&) {
                ++own;
                EXPECT_FLOAT_EQ(node.mScale, 2);
                EXPECT_FLOAT_EQ(binding.cachedWorld(node).mScale, selected ? 2 : 1.75f);
            }));
            root->getOrCreateStateSet()->setUpdateCallback(new PropertyCallback(
                [&](osg::NodeVisitor&) { ++properties; }));
            child->setUpdateCallback(new Callback([&](Node& node, osg::NodeVisitor&) {
                ++children;
                EXPECT_FLOAT_EQ(binding.cachedWorld(node).mScale, 1.75f);
                EXPECT_FLOAT_EQ(binding.cachedWorld(*root).mScale, 2);
                EXPECT_EQ(node.getWorldMatrices().front().getTrans(), osg::Vec3d(15.25, 27, 38.75));
            }));
            root->setScale(2);
            EXPECT_FLOAT_EQ(binding.cachedWorld(*root).mScale, 1.75f);
            if (selected) binding.refreshReferenceRootCache();
            EXPECT_EQ(own, 0u); EXPECT_EQ(properties, 0u); EXPECT_EQ(children, 0u);
            EXPECT_FLOAT_EQ(binding.cachedWorld(*root).mScale, selected ? 2 : 1.75f);
            EXPECT_FLOAT_EQ(binding.cachedWorld(*child).mScale, 1.75f);
            EXPECT_EQ(binding.cachedWorld(*child).mTranslation, osg::Vec3f(15.25, 27, 38.75));
            EXPECT_EQ(child->getWorldMatrices().front().getTrans(), osg::Vec3d(15.25, 27, 38.75));
            binding.update(osg::FrameStamp{}, 1);
            EXPECT_EQ(own, 1u); EXPECT_EQ(properties, 1u); EXPECT_EQ(children, 1u);
            EXPECT_FLOAT_EQ(binding.cachedWorld(*child).mScale, 2);
            EXPECT_EQ(binding.cachedWorld(*child).mTranslation, osg::Vec3f(16, 28, 40));
            EXPECT_EQ(root->getMatrix().getTrans(), osg::Vec3d(10, 20, 30));
            EXPECT_FLOAT_EQ(root->mScale, 2);
        }
    }

    TEST_F(NifSceneTraversalTest, RegistrationRootRefreshRetainsReentrancyAndBindingGuards)
    {
        Binding binding(*mRoot);
        unsigned calls = 0;
        mRoot->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) {
            ++calls;
            EXPECT_THROW(binding.refreshReferenceRootCache(), std::invalid_argument);
        }));
        binding.refreshReferenceRootCache();
        EXPECT_EQ(calls, 0u);
        binding.update(mStamp, 1);
        EXPECT_EQ(calls, 1u);
        const auto projected = mRoot->getTraversalMatrix();
        mChild->clearTraversalMatrix();
        EXPECT_THROW(binding.refreshReferenceRootCache(), std::invalid_argument);
        EXPECT_EQ(calls, 1u);
        EXPECT_EQ(mRoot->getTraversalMatrix(), projected);
    }
}

namespace
{
    TEST_F(NifSceneTraversalTest, ReferenceUpdateMigratesBeforeChildrenAndRetainsNativeWorldCaches)
    {
        osg::ref_ptr<SceneUtil::Skeleton> previous = new SceneUtil::Skeleton;
        previous->addChild(mRoot);
        osg::observer_ptr<SceneUtil::Skeleton> old = previous;
        osg::ref_ptr<osg::MatrixTransform> placement = new osg::MatrixTransform(
            osg::Matrixd::scale(2, 2, 2) * osg::Matrixd::translate(-300, 40, 50));
        osg::ref_ptr<SceneUtil::Skeleton> next = new SceneUtil::Skeleton;
        placement->addChild(next);
        Binding binding(*mRoot);
        unsigned updates = 0, children = 0;
        std::vector<std::string> events;
        mRoot->setUpdateCallback(new Callback([&](Node& node, osg::NodeVisitor&) {
            events.push_back("controller"); node.setTranslation({20, 0, 0});
        }));
        mChild->setUpdateCallback(new Callback([&](Node& node, osg::NodeVisitor& visitor) {
            ++children;
            events.push_back("child");
            EXPECT_FALSE(old.valid());
            EXPECT_EQ(visitor.getNodePath(), (osg::NodePath{placement, next, mRoot, mChild}));
            EXPECT_FLOAT_EQ(binding.cachedWorld(*mRoot).mTranslation.x(), 20);
            EXPECT_FLOAT_EQ(binding.cachedWorld(node).mTranslation.x(), 15);
            EXPECT_NEAR(node.getWorldMatrices().front().getTrans().x(), 15, 1e-10);
            EXPECT_FLOAT_EQ(node.getControllerTransform().mTranslation.x(), 5);
        }));
        Binding::Callbacks callbacks;
        callbacks.mProjected = [&](Node& node, const auto&, auto) {
            if (&node == mRoot) events.push_back("projected");
        };
        callbacks.mReferenceUpdate = [&](Node& node, const auto& frame, auto flags) {
            ++updates;
            events.push_back("reference");
            EXPECT_EQ(&node, mRoot.get());
            EXPECT_FALSE(flags);
            EXPECT_FLOAT_EQ(frame.mWorld.mTranslation.x(), 20);
            EXPECT_THROW(binding.update(mStamp, 42), std::invalid_argument);
            previous->removeChild(&node);
            previous = nullptr;
            EXPECT_TRUE(old.valid());
            next->addChild(&node);
        };
        binding.update(mStamp, 42, callbacks);
        EXPECT_EQ(updates, 1u); EXPECT_EQ(children, 1u);
        EXPECT_EQ(events, (std::vector<std::string>{"controller", "projected", "reference", "child"}));
        EXPECT_FLOAT_EQ(binding.cachedWorld(*mLeaf).mTranslation.x(), 27);
        EXPECT_NEAR(mLeaf->getWorldMatrices().front().getTrans().x(), 27, 1e-10);
        EXPECT_TRUE(binding.isValid());
    }

    TEST_F(NifSceneTraversalTest, ReferenceUpdateReanchorsAndInvalidatesNewSkeletonBeforeChildControllers)
    {
        osg::ref_ptr<osg::MatrixTransform> placement = new osg::MatrixTransform;
        osg::ref_ptr<SceneUtil::Skeleton> skeleton = new SceneUtil::Skeleton;
        placement->addChild(skeleton); skeleton->addChild(mRoot);
        mChild->setName("child");
        auto* bone = skeleton->getBone("child"); ASSERT_NE(bone, nullptr);
        Binding binding(*mRoot);
        Binding::Callbacks callbacks;
        callbacks.mReferenceUpdate = [&](Node&, const auto&, auto) {
            placement->setMatrix(osg::Matrixd::translate(100, 0, 0));
            // Cache the temporary rendering pose in this same traversal. The
            // reanchor must invalidate it before a child requests skinning.
            skeleton->invalidateBoneMatrices();
            skeleton->updateBoneMatrices(42);
            EXPECT_FLOAT_EQ(bone->mMatrixInSkeletonSpace.getTrans().x(), 15);
        };
        mChild->setUpdateCallback(new Callback([&](Node& node, osg::NodeVisitor&) {
            skeleton->updateBoneMatrices(42);
            EXPECT_FLOAT_EQ(bone->mMatrixInSkeletonSpace.getTrans().x(), -85);
            EXPECT_NEAR(node.getWorldMatrices().front().getTrans().x(), 15, 1e-10);
        }));
        binding.update(mStamp, 42, callbacks);
    }

    TEST_F(NifSceneTraversalTest, PlacementMutationOutsideReferenceUpdateRejectsBeforeMoreChildren)
    {
        osg::ref_ptr<osg::MatrixTransform> placement = new osg::MatrixTransform;
        placement->addChild(mRoot);
        Binding binding(*mRoot);
        for (unsigned source = 0; source < 3; ++source)
        {
            SCOPED_TRACE(source);
            placement->setMatrix(osg::Matrixd::identity());
            unsigned references = 0, leaves = 0;
            auto move = [&] { placement->setMatrix(osg::Matrixd::translate(100, 0, 0)); };
            mRoot->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) { if (source == 0) move(); }));
            mChild->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) { if (source == 2) move(); }));
            mLeaf->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) { ++leaves; }));
            Binding::Callbacks callbacks;
            callbacks.mProjected = [&](Node& node, const auto&, auto) { if (source == 1 && &node == mRoot) move(); };
            callbacks.mReferenceUpdate = [&](Node&, const auto&, auto) { ++references; };
            EXPECT_THROW(binding.update(mStamp, 42, callbacks), std::invalid_argument);
            EXPECT_EQ(references, source == 2 ? 1u : 0u);
            EXPECT_EQ(leaves, 0u);
        }
    }

    TEST_F(NifSceneTraversalTest, ReferenceUpdateRejectsInvalidPlacementAndHierarchyAndAllowsRetry)
    {
        osg::ref_ptr<osg::MatrixTransform> placement = new osg::MatrixTransform;
        placement->addChild(mRoot);
        Binding binding(*mRoot);
        unsigned children = 0;
        mChild->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) { ++children; }));
        Binding::Callbacks callbacks;
        callbacks.mReferenceUpdate = [&](Node&, const auto&, auto) {
            placement->setMatrix(osg::Matrixd::scale(0, 1, 1));
        };
        EXPECT_THROW(binding.update(mStamp, 42, callbacks), std::invalid_argument);
        EXPECT_EQ(children, 0u);
        placement->setMatrix(osg::Matrixd::translate(100, 0, 0));
        callbacks.mReferenceUpdate = [&](Node&, const auto&, auto) { throw std::runtime_error("reference write"); };
        EXPECT_THROW(binding.update(mStamp, 42, callbacks), std::runtime_error);
        EXPECT_EQ(children, 0u);
        callbacks.mReferenceUpdate = {};
        EXPECT_NO_THROW(binding.update(mStamp, 42, callbacks));
        EXPECT_EQ(children, 1u);
        EXPECT_NEAR(mLeaf->getWorldMatrices().front().getTrans().x(), 17, 1e-10);
        callbacks.mReferenceUpdate = [&](Node&, const auto&, auto) { mRoot->removeChild(mChild); };
        EXPECT_THROW(binding.update(mStamp, 42, callbacks), std::invalid_argument);
        EXPECT_EQ(children, 1u);
    }
}

namespace
{
    TEST_F(NifSceneTraversalTest, ReferenceUpdateSeesSettledFlagsAndProjectedPhysicsBeforeChildren)
    {
        osg::ref_ptr<osg::MatrixTransform> placement = new osg::MatrixTransform;
        placement->addChild(mRoot);
        Binding binding(*mRoot);
        std::uint16_t flags = 0x49;
        unsigned forces = 0, reads = 0, references = 0, children = 0;
        mRoot->setTranslation({30, 0, 0});
        Binding::Callbacks callbacks;
        callbacks.mCollisionFlags = [&](const Node& node) -> std::optional<std::uint16_t> {
            return &node == mRoot ? std::optional(flags) : std::nullopt;
        };
        callbacks.mFirstSynchronization = [&](const Node&, const auto& world, auto pending) {
            ++forces;
            EXPECT_EQ(pending, 0x49);
            EXPECT_FLOAT_EQ(world.mTranslation.x(), 30);
            EXPECT_EQ(references, 0u); EXPECT_EQ(children, 0u);
            flags &= ~0x40u;
        };
        callbacks.mPhysicalWorld = [&](const Node&) {
            ++reads;
            auto world = Nif::NiTransform::getIdentity(); world.mTranslation.x() = 40;
            return world;
        };
        callbacks.mReferenceUpdate = [&](Node& node, const auto& frame, auto settled) {
            ++references;
            ASSERT_TRUE(settled); EXPECT_EQ(*settled, 9);
            EXPECT_EQ(frame.mWorldChangeMask, references == 1 ? 0 : 1);
            EXPECT_EQ(forces, 1u); EXPECT_EQ(reads, references - 1);
            EXPECT_EQ(children, references - 1);
            const float x = references == 1 ? 30 : 40;
            EXPECT_FLOAT_EQ(frame.mWorld.mTranslation.x(), x);
            EXPECT_FLOAT_EQ(binding.cachedWorld(node).mTranslation.x(), x);
            placement->setMatrix(osg::Matrixd::translate(x, 0, 0));
        };
        mChild->setUpdateCallback(new Callback([&](Node& node, osg::NodeVisitor&) {
            ++children;
            EXPECT_EQ(references, children);
            EXPECT_FLOAT_EQ(binding.cachedWorld(node).mTranslation.x(), children == 1 ? 15 : 35);
            EXPECT_NEAR(node.getWorldMatrices().front().getTrans().x(), children == 1 ? 15 : 35, 1e-10);
        }));
        binding.update(mStamp, 42, callbacks);
        binding.update(mStamp, 43, callbacks);
        EXPECT_EQ(references, 2u); EXPECT_EQ(children, 2u);
        EXPECT_FLOAT_EQ(mRoot->getControllerTransform().mTranslation.x(), 40);
        EXPECT_NEAR(mLeaf->getWorldMatrices().front().getTrans().x(), 47, 1e-10);
    }
}

namespace
{
    TEST_F(NifSceneTraversalTest, SelectedAlwaysRunsOwnControllersButSeparatelyGatesPropertiesAndProjection)
    {
        Binding binding(*mRoot);
        unsigned own = 0, child = 0, properties = 0, projected = 0;
        mRoot->setUpdateCallback(new Callback([&](Node& node, osg::NodeVisitor& visitor) {
            ++own;
            EXPECT_EQ(Binding::updateMode(&visitor), Binding::Mode::Selected);
            node.setTranslation({20, 0, 0});
        }));
        mRoot->getOrCreateStateSet()->setUpdateCallback(new PropertyCallback([&](auto&) { ++properties; }));
        mChild->setUpdateCallback(new Callback([&](Node&, auto&) { ++child; }));
        Binding::Callbacks callbacks;
        std::uint16_t rootFlags = 0, childFlags = 0;
        callbacks.mNodeFlags = [&](const Node& node) { return &node == mRoot ? rootFlags : childFlags; };
        callbacks.mProjected = [&](Node&, const auto&, auto) { ++projected; };
        binding.update(mStamp, 42, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(own, 1u); EXPECT_EQ(child, 0u); EXPECT_EQ(properties, 0u); EXPECT_EQ(projected, 0u);
        EXPECT_FLOAT_EQ(binding.cachedWorld(*mRoot).mTranslation.x(), 10);
        EXPECT_FLOAT_EQ(mRoot->getControllerTransform().mTranslation.x(), 20);
        childFlags = 2;
        binding.update(mStamp, 43, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(own, 2u); EXPECT_EQ(child, 1u); EXPECT_EQ(properties, 0u); EXPECT_EQ(projected, 0u);
        EXPECT_FLOAT_EQ(binding.cachedWorld(*mChild).mTranslation.x(), 15);
        rootFlags = 12; childFlags = 6;
        binding.update(mStamp, 44, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(own, 3u); EXPECT_EQ(child, 2u); EXPECT_EQ(properties, 1u); EXPECT_EQ(projected, 3u);
        EXPECT_FLOAT_EQ(binding.cachedWorld(*mLeaf).mTranslation.x(), 27);
    }

    TEST_F(NifSceneTraversalTest, SelectedRigidChildModePropagatesAndRestoresAcrossSiblings)
    {
        auto sibling = makeNode(); mRoot->addChild(sibling);
        Binding binding(*mRoot);
        Binding::Callbacks callbacks;
        callbacks.mNodeFlags = [&](const Node& node) -> std::uint16_t { return &node == mChild ? 0x12 : 2; };
        std::vector<Binding::Mode> modes;
        for (auto* node : {mRoot.get(), mChild.get(), mLeaf.get(), sibling.get()})
            node->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor& visitor) {
                ASSERT_TRUE(Binding::updateMode(&visitor)); modes.push_back(*Binding::updateMode(&visitor));
            }));
        binding.update(mStamp, 42, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(modes, (std::vector{Binding::Mode::Selected, Binding::Mode::Rigid,
            Binding::Mode::Rigid, Binding::Mode::Selected}));
        modes.clear();
        binding.update(mStamp, 43, callbacks, Binding::Mode::Rigid);
        EXPECT_EQ(modes, (std::vector(4, Binding::Mode::Rigid)));
        osgUtil::UpdateVisitor ordinary;
        EXPECT_FALSE(Binding::updateMode(&ordinary));
        EXPECT_FALSE(Binding::updateMode(nullptr));
    }

    TEST_F(NifSceneTraversalTest, SelectedReinspectsFlagsAndNodeControllerListAfterPropertyWork)
    {
        Binding binding(*mRoot);
        std::uint16_t rootFlags = 8, childFlags = 0;
        std::vector<std::string> events;
        mRoot->setUpdateCallback(new Callback([](Node&, auto&) { FAIL() << "replaced controller ran"; }));
        mRoot->getOrCreateStateSet()->setUpdateCallback(new PropertyCallback([&](auto&) {
            events.push_back("property"); rootFlags = 4;
            mRoot->setUpdateCallback(new Callback([&](Node& node, auto&) {
                events.push_back("root"); childFlags = 6; node.setTranslation({20, 0, 0});
            }));
        }));
        mChild->setUpdateCallback(new Callback([&](Node&, auto&) {
            events.push_back("child");
            EXPECT_FLOAT_EQ(binding.cachedWorld(*mRoot).mTranslation.x(), 20);
            EXPECT_FLOAT_EQ(binding.cachedWorld(*mChild).mTranslation.x(), 15);
        }));
        mLeaf->setUpdateCallback(new Callback([&](Node&, auto&) { events.push_back("leaf"); }));
        Binding::Callbacks callbacks;
        callbacks.mNodeFlags = [&](const Node& node) -> std::uint16_t {
            return &node == mRoot ? rootFlags : &node == mChild ? childFlags : 2;
        };
        callbacks.mProjected = [&](Node& node, const auto&, auto) {
            events.push_back(&node == mRoot ? "project root" : "project child");
        };
        callbacks.mReferenceUpdate = [&](Node&, const auto&, auto) { events.push_back("reference"); };
        binding.update(mStamp, 42, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(events, (std::vector<std::string>{"property", "root", "project root", "reference",
            "child", "project child", "leaf"}));
        EXPECT_FLOAT_EQ(binding.cachedWorld(*mChild).mTranslation.x(), 25);
        EXPECT_FLOAT_EQ(binding.cachedWorld(*mLeaf).mTranslation.x(), 17);
        EXPECT_NEAR(mLeaf->getWorldMatrices().front().getTrans().x(), 17, 1e-10);
    }

    TEST_F(NifSceneTraversalTest, SelectedScaffoldingAndDrawablesInheritNativePropertyPolicy)
    {
        osg::ref_ptr<osg::Group> wrapper = new osg::Group;
        mRoot->removeChild(mChild); mRoot->addChild(wrapper); wrapper->addChild(mChild);
        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        osg::ref_ptr<osg::Geometry> drawable = new osg::Geometry;
        mChild->addChild(geode); geode->addDrawable(drawable);
        Binding binding(*mRoot);
        unsigned rootProperties = 0, childProperties = 0, geometryProperties = 0, drawableCalls = 0;
        wrapper->getOrCreateStateSet()->setUpdateCallback(new PropertyCallback([&](auto&) { ++rootProperties; }));
        geode->getOrCreateStateSet()->setUpdateCallback(new PropertyCallback([&](auto&) { ++childProperties; }));
        drawable->getOrCreateStateSet()->setUpdateCallback(new PropertyCallback([&](auto&) { ++geometryProperties; }));
        struct Update : osg::DrawableUpdateCallback
        {
            unsigned& mCalls;
            explicit Update(unsigned& calls) : mCalls(calls) {}
            void update(osg::NodeVisitor*, osg::Drawable*) override { ++mCalls; }
        };
        drawable->setUpdateCallback(new Update(drawableCalls));
        std::uint16_t childFlags = 2;
        Binding::Callbacks callbacks;
        callbacks.mNodeFlags = [&](const Node& node) -> std::uint16_t { return &node == mChild ? childFlags : 2; };
        binding.update(mStamp, 42, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(rootProperties, 0u); EXPECT_EQ(childProperties, 0u); EXPECT_EQ(geometryProperties, 0u);
        EXPECT_EQ(drawableCalls, 1u);
        childFlags = 10;
        binding.update(mStamp, 43, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(rootProperties, 0u); EXPECT_EQ(childProperties, 1u); EXPECT_EQ(geometryProperties, 1u);
        EXPECT_EQ(drawableCalls, 2u);
    }

    TEST_F(NifSceneTraversalTest, SelectedMissingOrMutatingPolicyRejectsWithoutAdvancingControllers)
    {
        Binding binding(*mRoot);
        const auto projected = mRoot->getTraversalMatrix();
        unsigned own = 0;
        mRoot->setUpdateCallback(new Callback([&](Node&, auto&) { ++own; }));
        EXPECT_THROW(binding.update(mStamp, 42, {}, Binding::Mode::Selected), std::invalid_argument);
        EXPECT_THROW(binding.update(mStamp, 42, {}, static_cast<Binding::Mode>(255)), std::invalid_argument);
        EXPECT_EQ(projected, mRoot->getTraversalMatrix());
        EXPECT_EQ(own, 0u);
        Binding::Callbacks callbacks;
        callbacks.mNodeFlags = [&](const Node&) -> std::uint16_t { mRoot->removeChild(mChild); return 14; };
        EXPECT_THROW(binding.update(mStamp, 42, callbacks, Binding::Mode::Selected), std::invalid_argument);
        EXPECT_EQ(own, 0u);
    }
}

namespace
{
    TEST_F(NifSceneTraversalTest, SelectedProjectionGatePreservesPendingPhysicsUntilAnEnabledPass)
    {
        Binding binding(*mRoot);
        std::uint16_t updateFlags = 0, collisionFlags = 0x4d;
        unsigned own = 0, flagsRead = 0, forced = 0, physicalReads = 0, references = 0;
        mRoot->setUpdateCallback(new Callback([&](Node& node, auto&) { ++own; node.setTranslation({20, 0, 0}); }));
        Binding::Callbacks callbacks;
        callbacks.mNodeFlags = [&](const Node& node) -> std::uint16_t { return &node == mRoot ? updateFlags : 0; };
        callbacks.mCollisionFlags = [&](const Node& node) -> std::optional<std::uint16_t> {
            EXPECT_EQ(&node, mRoot.get()); ++flagsRead; return collisionFlags;
        };
        callbacks.mPhysicalWorld = [&](const Node&) {
            ++physicalReads;
            auto world = Nif::NiTransform::getIdentity(); world.mTranslation.x() = 30;
            return world;
        };
        callbacks.mReferenceUpdate = [&](Node&, const auto& frame, auto settled) {
            ++references;
            ASSERT_TRUE(settled); EXPECT_EQ(*settled, 0xd);
            EXPECT_EQ(frame.mWorldChangeMask, references == 1 ? 0 : 1);
        };
        binding.update(mStamp, 42, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(own, 1u); EXPECT_EQ(flagsRead, 0u); EXPECT_EQ(collisionFlags, 0x4d);
        EXPECT_EQ(physicalReads, 0u); EXPECT_EQ(references, 0u);
        EXPECT_FLOAT_EQ(binding.cachedWorld(*mRoot).mTranslation.x(), 10);
        updateFlags = 4;
        EXPECT_THROW(binding.update(mStamp, 43, callbacks, Binding::Mode::Selected), std::invalid_argument);
        EXPECT_EQ(collisionFlags, 0x4d);
        EXPECT_FLOAT_EQ(binding.cachedWorld(*mRoot).mTranslation.x(), 10);
        callbacks.mFirstSynchronization = [&](const Node&, const auto& world, auto pending) {
            ++forced; EXPECT_EQ(pending, 0x4d); EXPECT_FLOAT_EQ(world.mTranslation.x(), 20);
            EXPECT_EQ(references, 0u); collisionFlags &= ~0x40u;
        };
        binding.update(mStamp, 44, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(forced, 1u); EXPECT_EQ(references, 1u); EXPECT_EQ(physicalReads, 0u);
        binding.update(mStamp, 45, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(own, 4u); EXPECT_EQ(forced, 1u); EXPECT_EQ(references, 2u); EXPECT_EQ(physicalReads, 1u);
        EXPECT_FLOAT_EQ(binding.cachedWorld(*mRoot).mTranslation.x(), 30);
        EXPECT_FLOAT_EQ(mRoot->getControllerTransform().mTranslation.x(), 30);
        EXPECT_FLOAT_EQ(binding.cachedWorld(*mChild).mTranslation.x(), 15);
        EXPECT_NEAR(mChild->getWorldMatrices().front().getTrans().x(), 15, 1e-10);
    }
    TEST(NifSelectiveUpdatePolicy, NativeComputedFlagsDriveTraversalAndRemainPrivate)
    {
        using Policy = Binding::SelectiveUpdatePolicy;
        std::array<osg::ref_ptr<Node>, 5> nodes;
        for (auto& node : nodes) node = makeNode();
        nodes[0]->addChild(nodes[1]); nodes[0]->addChild(nodes[2]);
        nodes[1]->addChild(nodes[3]); nodes[1]->addChild(nodes[4]);
        // Literal original selected-flags-oracle01 case288, including inactive
        // transform controllers, a later transform controller and skinned leaves.
        const std::array<Policy, 5> inputs{{
            {Policy::Kind::Node, 53385, false, false, false, false},
            {Policy::Kind::Node, 8899, true, true, true, false},
            {Policy::Kind::Geometry, 22211, true, true, true, true},
            {Policy::Kind::Geometry, 7951, true, true, false, false},
            {Policy::Kind::Geometry, 40850, true, false, false, true},
        }};
        const std::array<unsigned, 5> expected{53379, 8911, 22239, 7959, 40854};
        for (unsigned i = 0; i < nodes.size(); ++i) nodes[i]->setUserValue("nativeNifFlags", unsigned(inputs[i].mFlags));
        unsigned own = 0, property = 0;
        for (unsigned i = 0; i < nodes.size(); ++i)
        {
            if (inputs[i].mHasController)
                nodes[i]->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) { ++own; }));
            if (inputs[i].mHasPropertyController)
                nodes[i]->getOrCreateStateSet()->setUpdateCallback(new PropertyCallback([&](osg::NodeVisitor&) { ++property; }));
        }
        const auto index = [&](const Node& node) {
            for (unsigned i = 0; i < nodes.size(); ++i) if (nodes[i].get() == &node) return i;
            throw std::logic_error("foreign test node");
        };
        Binding binding(*nodes[0]);
        EXPECT_THROW(binding.selectiveUpdateFlags(*nodes[0]), std::invalid_argument);
        const auto before = nodes[0]->getTraversalMatrix();
        const auto result = binding.recomputeSelectiveUpdateFlags([&](const Node& node) { return inputs[index(node)]; }, false);
        EXPECT_TRUE(result.mNeedsUpdate); EXPECT_FALSE(result.mRigid);
        EXPECT_EQ(own, 0u); EXPECT_EQ(property, 0u);
        EXPECT_EQ(nodes[0]->getTraversalMatrix(), before);
        for (unsigned i = 0; i < nodes.size(); ++i)
        {
            EXPECT_EQ(binding.selectiveUpdateFlags(*nodes[i]), expected[i]);
            unsigned authored = 0; ASSERT_TRUE(nodes[i]->getUserValue("nativeNifFlags", authored));
            EXPECT_EQ(authored, inputs[i].mFlags);
        }
        std::vector<unsigned> projected;
        Binding::Callbacks callbacks;
        callbacks.mProjected = [&](Node& node, const auto&, auto) { projected.push_back(index(node)); };
        binding.update(osg::FrameStamp{}, 1, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(projected, (std::vector<unsigned>{1, 3, 4, 2}));
        EXPECT_EQ(own, 4u); EXPECT_EQ(property, 2u);
        auto foreign = makeNode();
        EXPECT_THROW(binding.selectiveUpdateFlags(*foreign), std::invalid_argument);
    }

    TEST_F(NifSceneTraversalTest, PolicyPreparationIsAtomicAndRejectsMutationAndReentrancy)
    {
        using Policy = Binding::SelectiveUpdatePolicy;
        osg::ref_ptr<osg::MatrixTransform> parent = new osg::MatrixTransform;
        parent->addChild(mRoot);
        Binding binding(*mRoot);
        const auto plain = [](const Node&) { return Policy{}; };
        binding.recomputeSelectiveUpdateFlags(plain, true);
        const auto unchanged = [&] {
            EXPECT_EQ(binding.selectiveUpdateFlags(*mRoot), 22);
            EXPECT_EQ(binding.selectiveUpdateFlags(*mChild), 22);
            EXPECT_EQ(binding.selectiveUpdateFlags(*mLeaf), 22);
            EXPECT_FLOAT_EQ(binding.cachedWorld(*mLeaf).mTranslation.x(), 17);
        };
        for (unsigned invalid = 0; invalid < 5; ++invalid)
        {
            SCOPED_TRACE(invalid);
            const auto resolve = [&](const Node& node) {
                Policy value;
                if (invalid == 0) value.mKind = static_cast<Policy::Kind>(99);
                if (invalid == 1) value.mKind = Policy::Kind::Geometry;
                if (invalid == 2) value.mHasSkin = true;
                if (invalid == 3) value.mHasTransformController = true;
                if (invalid == 4 && &node == mLeaf) throw std::invalid_argument("late policy failure");
                return value;
            };
            EXPECT_THROW(binding.recomputeSelectiveUpdateFlags(resolve, false), std::invalid_argument);
            unchanged();
        }
        EXPECT_THROW(binding.recomputeSelectiveUpdateFlags({}, false), std::invalid_argument);
        EXPECT_THROW(binding.recomputeSelectiveUpdateFlags([&](const Node&) {
            parent->setMatrix(osg::Matrixd::translate(1, 0, 0)); return Policy{};
        }, false), std::invalid_argument);
        parent->setMatrix(osg::Matrixd::identity()); unchanged();
        EXPECT_THROW(binding.recomputeSelectiveUpdateFlags([&](const Node&) {
            mRoot->removeChild(mChild); return Policy{};
        }, false), std::invalid_argument);
        mRoot->addChild(mChild); unchanged();
        unsigned nested = 0;
        binding.recomputeSelectiveUpdateFlags([&](const Node&) {
            EXPECT_THROW(binding.recomputeSelectiveUpdateFlags(plain, false), std::invalid_argument);
            EXPECT_THROW(binding.update(mStamp, 0), std::invalid_argument);
            ++nested; return Policy{};
        }, true);
        EXPECT_EQ(nested, 3u); unchanged();
    }

    TEST_F(NifSceneTraversalTest, RecomputedFlagsAndExplicitOverridesSelectCurrentChildren)
    {
        using Policy = Binding::SelectiveUpdatePolicy;
        Binding binding(*mRoot);
        unsigned own = 0, projected = 0;
        for (Node* node : {mRoot.get(), mChild.get(), mLeaf.get()})
            node->setUpdateCallback(new Callback([&](Node&, osg::NodeVisitor&) { ++own; }));
        // The supplied policy describes native lists; arbitrary OSG scaffolding
        // callbacks do not implicitly become native transform controllers.
        const auto policy = [](const Node&) { return Policy{}; };
        auto result = binding.recomputeSelectiveUpdateFlags(policy, false);
        EXPECT_FALSE(result.mNeedsUpdate); EXPECT_TRUE(result.mRigid);
        Binding::Callbacks callbacks;
        callbacks.mProjected = [&](Node&, const auto&, auto) { ++projected; };
        binding.update(mStamp, 0, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(own, 1u); EXPECT_EQ(projected, 0u);
        result = binding.recomputeSelectiveUpdateFlags(policy, true);
        EXPECT_TRUE(result.mNeedsUpdate); EXPECT_TRUE(result.mRigid);
        binding.update(mStamp, 1, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(own, 4u); EXPECT_EQ(projected, 3u);
        callbacks.mNodeFlags = [](const Node&) { return 0; };
        binding.update(mStamp, 2, callbacks, Binding::Mode::Selected);
        EXPECT_EQ(own, 5u); EXPECT_EQ(projected, 3u);
        EXPECT_EQ(binding.selectiveUpdateFlags(*mRoot), 22);
    }

}
