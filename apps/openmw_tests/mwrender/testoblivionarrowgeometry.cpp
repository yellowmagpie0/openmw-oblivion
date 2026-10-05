#include <apps/openmw/mwrender/animation.hpp>
#include <apps/openmw/mwrender/util.hpp>
#include <components/nif/niftypes.hpp>
#include <components/nifosg/matrixtransform.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/vfs/manager.hpp>

#include <gtest/gtest.h>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <limits>

namespace
{
    TEST(OblivionArrowGeometryTest, ClonesOnlyNamedArrowAndPreservesAuthoredTransformAndTemplate)
    {
        VFS::Manager vfs;
        Resource::ResourceSystem resources(&vfs, 0., nullptr);
        osg::ref_ptr<osg::Group> model = new osg::Group;
        model->setName("Arrow");
        osg::ref_ptr<osg::Group> quiver = new osg::Group;
        quiver->setName("ArrowQuiver");
        model->addChild(quiver);
        osg::ref_ptr<NifOsg::MatrixTransform> arrow = new NifOsg::MatrixTransform(Nif::NiTransform::getIdentity());
        arrow->setName("Arrow:0");
        arrow->setTranslation({1, 2, 3});
        arrow->setRotation(osg::Quat(.25, osg::Vec3f(0, 0, 1)));
        arrow->setScale(2);
        model->addChild(arrow);
        const auto original = arrow->getMatrix();
        auto first = MWRender::cloneOblivionArrowGeometry(*model, *resources.getSceneManager());
        auto second = MWRender::cloneOblivionArrowGeometry(*model, *resources.getSceneManager());
        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        EXPECT_NE(first.get(), arrow.get());
        EXPECT_NE(first.get(), second.get());
        EXPECT_EQ(first->getName(), "Arrow:0");
        auto* transform = dynamic_cast<NifOsg::MatrixTransform*>(first.get());
        ASSERT_NE(transform, nullptr);
        EXPECT_EQ(transform->getMatrix(), original);
        EXPECT_FLOAT_EQ(transform->mScale, 2);
        EXPECT_EQ(first->getNumParents(), 0u);
        EXPECT_EQ(model->getNumChildren(), 2u);
        EXPECT_EQ(arrow->getNumParents(), 1u);
        transform->setTranslation({9, 8, 7});
        EXPECT_EQ(arrow->getMatrix(), original);
        EXPECT_EQ(dynamic_cast<NifOsg::MatrixTransform*>(second.get())->getMatrix(), original);
        osg::ref_ptr<osg::Group> bone = new osg::Group;
        bone->setName("ArrowBone");
        {
            MWRender::PartHolder holder(first);
            ASSERT_TRUE(bone->addChild(first));
            EXPECT_EQ(bone->getNumChildren(), 1u);
            EXPECT_EQ(first->getNumParents(), 1u);
        }
        EXPECT_EQ(bone->getNumChildren(), 0u);
        EXPECT_EQ(first->getNumParents(), 0u);
        EXPECT_EQ(model->getNumChildren(), 2u);
    }

    TEST(OblivionArrowGeometryTest, RequiresExactArrowNameAndDoesNotChangeHiddenSource)
    {
        VFS::Manager vfs;
        Resource::ResourceSystem resources(&vfs, 0., nullptr);
        osg::ref_ptr<osg::Group> model = new osg::Group;
        osg::ref_ptr<osg::Group> spare = new osg::Group;
        spare->setName("Arrow1:0"); model->addChild(spare);
        EXPECT_FALSE(MWRender::cloneOblivionArrowGeometry(*model, *resources.getSceneManager()));
        spare->setName("arrow:0");
        EXPECT_FALSE(MWRender::cloneOblivionArrowGeometry(*model, *resources.getSceneManager()));
        spare->setName("Arrow:0"); spare->setNodeMask(0);
        auto selected = MWRender::cloneOblivionArrowGeometry(*model, *resources.getSceneManager());
        ASSERT_TRUE(selected);
        EXPECT_NE(selected.get(), spare.get());
        EXPECT_EQ(selected->getName(), "Arrow:0");
        EXPECT_EQ(spare->getNodeMask(), 0u);
        EXPECT_EQ(spare->getNumParents(), 1u);
    }
}

namespace
{
    TEST(OblivionAttachedWeaponPoseTest, EmptyAndMovedFromCapturesHaveNoBorrowedSceneNodes)
    {
        MWRender::OblivionAttachedWeaponPose empty;
        EXPECT_EQ(empty.getPlacementNode(), nullptr);
        EXPECT_EQ(empty.getAttachedNode(), nullptr);
        EXPECT_FALSE(empty.matchesCurrentScene());
        osg::ref_ptr<osg::Group> node = new osg::Group;
        auto captured = MWRender::OblivionAttachedWeaponPose::capture(*node, *node);
        ASSERT_TRUE(captured);
        auto moved = std::move(*captured);
        EXPECT_TRUE(moved.matchesCurrentScene());
        EXPECT_EQ(captured->getPlacementNode(), nullptr);
        EXPECT_EQ(captured->getAttachedNode(), nullptr);
        EXPECT_FALSE(captured->matchesCurrentScene());
    }

    TEST(OblivionAttachedWeaponPoseTest, CapturesActualAttachmentIncludingAuthoredTransformAndActorPlacement)
    {
        osg::ref_ptr<osg::MatrixTransform> actor = new osg::MatrixTransform;
        actor->setMatrix(osg::Matrix::rotate(.5, osg::Vec3(0, 0, 1)) * osg::Matrix::translate(100, 20, 30));
        osg::ref_ptr<osg::MatrixTransform> hand = new osg::MatrixTransform;
        hand->setMatrix(osg::Matrix::rotate(.25, osg::Vec3(1, 0, 0)) * osg::Matrix::translate(3, -4, 5));
        osg::ref_ptr<osg::MatrixTransform> model = new osg::MatrixTransform;
        model->setName("Bow");
        model->setMatrix(osg::Matrix::translate(1, 2, 3));
        actor->addChild(hand);
        hand->addChild(model);
        const auto pose = MWRender::OblivionAttachedWeaponPose::capture(*actor, *model);
        ASSERT_TRUE(pose);
        EXPECT_EQ(pose->getPlacementNode(), actor.get());
        EXPECT_EQ(pose->getAttachedNode(), model.get());
        const auto expected = model->getMatrix() * hand->getMatrix() * actor->getMatrix();
        // Independent multiplication association can differ by one double ULP.
        for (unsigned row = 0; row != 4; ++row)
            for (unsigned column = 0; column != 4; ++column)
                EXPECT_NEAR(pose->getWorldMatrix()(row, column), expected(row, column), 1e-12);
        EXPECT_TRUE(pose->matchesCurrentScene());
        EXPECT_EQ(hand->getNumChildren(), 1u);
        EXPECT_EQ(model->getNumParents(), 1u);
    }

    TEST(OblivionAttachedWeaponPoseTest, DetectsChangesAtEveryLevelWithoutChangingTheSnapshot)
    {
        osg::ref_ptr<osg::MatrixTransform> actor = new osg::MatrixTransform;
        osg::ref_ptr<osg::MatrixTransform> hand = new osg::MatrixTransform;
        osg::ref_ptr<osg::MatrixTransform> model = new osg::MatrixTransform;
        actor->addChild(hand); hand->addChild(model);
        const auto pose = MWRender::OblivionAttachedWeaponPose::capture(*actor, *model);
        ASSERT_TRUE(pose);
        const auto matrix = pose->getWorldMatrix();
        for (auto* node : { actor.get(), hand.get(), model.get() })
        {
            node->setMatrix(osg::Matrix::translate(1, 2, 3));
            EXPECT_FALSE(pose->matchesCurrentScene());
            EXPECT_EQ(pose->getWorldMatrix(), matrix);
            node->setMatrix(osg::Matrix::identity());
            EXPECT_TRUE(pose->matchesCurrentScene());
            node->setNodeMask(0);
            EXPECT_FALSE(pose->matchesCurrentScene());
            node->setNodeMask(~0u);
            EXPECT_TRUE(pose->matchesCurrentScene());
            node->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
            EXPECT_FALSE(pose->matchesCurrentScene());
            node->setReferenceFrame(osg::Transform::RELATIVE_RF);
        }
    }

    TEST(OblivionAttachedWeaponPoseTest, RejectsDetachedReparentedAndAmbiguousAttachments)
    {
        osg::ref_ptr<osg::Group> actor = new osg::Group;
        osg::ref_ptr<osg::Group> hand = new osg::Group;
        osg::ref_ptr<osg::Group> other = new osg::Group;
        osg::ref_ptr<osg::Group> model = new osg::Group;
        actor->addChild(hand); actor->addChild(other); hand->addChild(model);
        const auto pose = MWRender::OblivionAttachedWeaponPose::capture(*actor, *model);
        ASSERT_TRUE(pose);
        other->addChild(model);
        EXPECT_FALSE(pose->matchesCurrentScene());
        EXPECT_FALSE(MWRender::OblivionAttachedWeaponPose::capture(*actor, *model));
        hand->removeChild(model);
        EXPECT_FALSE(pose->matchesCurrentScene());
        ASSERT_TRUE(MWRender::OblivionAttachedWeaponPose::capture(*actor, *model));
        other->removeChild(model);
        EXPECT_FALSE(pose->matchesCurrentScene());
        EXPECT_FALSE(MWRender::OblivionAttachedWeaponPose::capture(*actor, *model));
        EXPECT_FALSE(MWRender::OblivionAttachedWeaponPose::capture(*other, *hand));
    }

    TEST(OblivionAttachedWeaponPoseTest, OwnsPathAfterExternalHandlesAreReleasedAndRejectsNonfiniteTransforms)
    {
        osg::ref_ptr<osg::MatrixTransform> actor = new osg::MatrixTransform;
        osg::ref_ptr<osg::MatrixTransform> model = new osg::MatrixTransform;
        actor->addChild(model);
        auto pose = MWRender::OblivionAttachedWeaponPose::capture(*actor, *model);
        ASSERT_TRUE(pose);
        actor = nullptr;
        EXPECT_TRUE(pose->matchesCurrentScene());
        auto invalid = osg::Matrix::identity();
        invalid(0, 0) = std::numeric_limits<double>::infinity();
        model->setMatrix(invalid);
        EXPECT_FALSE(pose->matchesCurrentScene());
        EXPECT_FALSE(MWRender::OblivionAttachedWeaponPose::capture(
            *const_cast<osg::Node*>(pose->getPlacementNode()), *model));
        model = nullptr;
        EXPECT_FALSE(pose->matchesCurrentScene());
    }

    TEST(OblivionAttachedWeaponPoseTest, RejectsCyclesRatherThanWalkingAnUnboundedParentChain)
    {
        osg::ref_ptr<osg::Group> actor = new osg::Group;
        osg::ref_ptr<osg::Group> first = new osg::Group;
        osg::ref_ptr<osg::Group> second = new osg::Group;
        first->addChild(second); second->addChild(first);
        EXPECT_FALSE(MWRender::OblivionAttachedWeaponPose::capture(*actor, *first));
        second->removeChild(first); first->removeChild(second);
    }
}
