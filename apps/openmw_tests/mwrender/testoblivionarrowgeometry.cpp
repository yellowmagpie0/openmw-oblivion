#include <apps/openmw/mwrender/animation.hpp>
#include <apps/openmw/mwrender/util.hpp>
#include <components/nif/niftypes.hpp>
#include <components/nifosg/matrixtransform.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/vfs/manager.hpp>

#include <gtest/gtest.h>
#include <osg/Group>

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
