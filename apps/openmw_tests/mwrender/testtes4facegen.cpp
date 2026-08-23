#include <gtest/gtest.h>

#include "../../openmw/mwrender/tes4facegen.hpp"

#include <osg/Group>
#include <osg/StateSet>

#include <components/esm4/facegen.hpp>

namespace MWRender
{
    TEST(Tes4FaceGen, IsolatesActorGeometryFromSharedTemplate)
    {
        osg::ref_ptr<osg::Vec3Array> templateVertices = new osg::Vec3Array;
        templateVertices->push_back(osg::Vec3f(1.f, 2.f, 3.f));
        osg::ref_ptr<osg::Geometry> templateGeometry = new osg::Geometry;
        templateGeometry->setVertexArray(templateVertices);
        templateGeometry->setStateSet(new osg::StateSet);
        osg::ref_ptr<osg::Group> templateRoot = new osg::Group;
        templateRoot->addChild(templateGeometry);

        osg::ref_ptr<osg::Group> first = new osg::Group(*templateRoot, osg::CopyOp::SHALLOW_COPY);
        osg::ref_ptr<osg::Group> second = new osg::Group(*templateRoot, osg::CopyOp::SHALLOW_COPY);
        ASSERT_EQ(first->getChild(0), templateGeometry);
        ASSERT_EQ(second->getChild(0), templateGeometry);

        EXPECT_EQ(isolateTes4ActorGeometry(*first), 1u);
        auto* isolated = dynamic_cast<osg::Geometry*>(first->getChild(0));
        ASSERT_NE(isolated, nullptr);
        EXPECT_NE(isolated, templateGeometry);
        EXPECT_NE(isolated->getVertexArray(), templateGeometry->getVertexArray());
        EXPECT_NE(isolated->getStateSet(), templateGeometry->getStateSet());

        auto* isolatedVertices = dynamic_cast<osg::Vec3Array*>(isolated->getVertexArray());
        ASSERT_NE(isolatedVertices, nullptr);
        (*isolatedVertices)[0].x() = 99.f;
        EXPECT_FLOAT_EQ((*templateVertices)[0].x(), 1.f);
        EXPECT_EQ(second->getChild(0), templateGeometry);
    }

    TEST(Tes4FaceGen, AppliesEgmBasePrefixWhenStatMorphVerticesFollow)
    {
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        vertices->push_back(osg::Vec3f(1.f, 2.f, 3.f));
        vertices->push_back(osg::Vec3f(4.f, 5.f, 6.f));
        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        geometry->setVertexArray(vertices);

        ESM4::FaceGenMorph mode;
        mode.mScale = 0.5f;
        mode.mVertices = { { 2, 4, 6 }, { -2, -4, -6 }, { 100, 100, 100 }, { 100, 100, 100 },
            { 100, 100, 100 }, { 100, 100, 100 } };
        ESM4::FaceGenEgm egm;
        egm.mVertexCount = 6;
        egm.mSymmetricMorphs.push_back(mode);

        ASSERT_TRUE(applyTes4FaceGenEgm(*geometry, egm, { 2.f }, {}));
        const auto* result = dynamic_cast<const osg::Vec3Array*>(geometry->getVertexArray());
        ASSERT_NE(result, nullptr);
        EXPECT_EQ((*result)[0], osg::Vec3f(3.f, 6.f, 9.f));
        EXPECT_EQ((*result)[1], osg::Vec3f(2.f, 1.f, 0.f));
    }

    TEST(Tes4FaceGen, CancelsBipedHeadBindOrientation)
    {
        const osg::Quat bindRotation(osg::DegreesToRadians(90.f), osg::Vec3f(0.f, 1.f, 0.f));
        const osg::Matrixf bind
            = osg::Matrixf::rotate(bindRotation) * osg::Matrixf::translate(4.f, 5.f, 6.f);
        const osg::Matrixf corrected
            = osg::Matrixf::rotate(getTes4HeadPartCorrection(bind)) * bind;

        const osg::Vec3f horizontal = osg::Vec3f(2.f, 0.f, 0.f) * corrected;
        EXPECT_NEAR(horizontal.x(), 6.f, 1e-5f);
        EXPECT_NEAR(horizontal.y(), 5.f, 1e-5f);
        EXPECT_NEAR(horizontal.z(), 6.f, 1e-5f);
    }

    TEST(Tes4FaceGen, RejectsEgmShorterThanGeometry)
    {
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        vertices->push_back(osg::Vec3f(1.f, 2.f, 3.f));
        vertices->push_back(osg::Vec3f(4.f, 5.f, 6.f));
        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        geometry->setVertexArray(vertices);
        ESM4::FaceGenEgm egm;
        egm.mVertexCount = 1;

        EXPECT_FALSE(applyTes4FaceGenEgm(*geometry, egm, {}, {}));
        EXPECT_EQ(geometry->getVertexArray(), vertices);
    }

    TEST(Tes4FaceGen, SelectsExactSpeechChannels)
    {
        EXPECT_FLOAT_EQ(getTes4FaceMorphWeight("i", "i", "r", 0.25f, 0.8f, 0.f), 0.6f);
        EXPECT_FLOAT_EQ(getTes4FaceMorphWeight("r", "i", "r", 0.25f, 0.8f, 0.f), 0.2f);
        EXPECT_FLOAT_EQ(getTes4FaceMorphWeight("dispanger1", "i", "r", 0.25f, 0.8f, 0.f), 0.f);
        EXPECT_FLOAT_EQ(getTes4FaceMorphWeight("surprise", "i", "r", 0.25f, 0.8f, 0.f), 0.f);
        EXPECT_FLOAT_EQ(getTes4FaceMorphWeight("browinright", "i", "r", 0.25f, 0.8f, 0.f), 0.f);
    }

    TEST(Tes4FaceGen, KeepsBlinkIndependentFromSpeech)
    {
        EXPECT_FLOAT_EQ(getTes4FaceMorphWeight("blink", "aah", "bigaah", 0.5f, 1.f, 0.75f), 0.75f);
        EXPECT_FLOAT_EQ(
            getTes4FaceMorphWeight("eyes closed left", "aah", "bigaah", 0.5f, 1.f, 0.5f), 0.5f);
        EXPECT_FLOAT_EQ(getTes4FaceMorphWeight("happy", "aah", "bigaah", 0.5f, 0.f, 0.5f), 0.f);
    }
}
