#include <gtest/gtest.h>

#include "../../openmw/mwrender/tes4facegen.hpp"
#include "../../openmw/mwrender/tes4hair.hpp"
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/testing/util.hpp>
#include <components/vfs/manager.hpp>
#include <osg/Material>
#include <fstream>
#include <array>

#include <osg/Group>
#include <osg/StateSet>
#include <osg/Texture2D>

#include <components/esm4/facegen.hpp>
#include <components/esm4/loadrace.hpp>

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

    TEST(Tes4FaceGen, HairTintRefreshesCachedVertexColorModeWithoutChangingSharedTemplate)
    {
        VFS::Manager vfs;
        Resource::ResourceSystem resources(&vfs, 0, nullptr);
        auto& scenes = *resources.getSceneManager();
        const auto directory = TestingOpenMW::outputFilePathWithSubDir("tes4-hair");
        std::filesystem::create_directories(directory / "compatibility");
        for (const auto extension : {".vert", ".frag"})
        {
            std::ofstream file(directory / "compatibility" / (std::string("objects") + extension));
            file << "#version 120\nvoid main(){"
                 << (std::string_view(extension) == ".vert" ? "gl_Position" : "gl_FragColor") << "=vec4(1);}";
        }
        scenes.setShaderPath(directory);
        osg::ref_ptr<osg::Group> model = new osg::Group;
        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        const osg::Vec3f vertices[] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
        const osg::Vec4f colors[] = {{0, 1, 1, 1}};
        geometry->setVertexArray(new osg::Vec3Array(3, vertices));
        geometry->setColorArray(new osg::Vec4Array(1, colors), osg::Array::BIND_OVERALL);
        geometry->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, 3));
        osg::ref_ptr<osg::Material> authored = new osg::Material;
        authored->setColorMode(osg::Material::AMBIENT_AND_DIFFUSE);
        geometry->getOrCreateStateSet()->setAttribute(authored);
        model->addChild(geometry);
        scenes.recreateShaders(model);
        int mode = -1;
        ASSERT_NE(geometry->getStateSet()->getUniform("colorMode"), nullptr);
        ASSERT_TRUE(geometry->getStateSet()->getUniform("colorMode")->get(mode));
        ASSERT_EQ(mode, 2); // The cached template uses the authored cyan vertices.
        osg::ref_ptr<osg::Group> actor = new osg::Group(*model, osg::CopyOp::SHALLOW_COPY);
        ASSERT_EQ(isolateTes4ActorGeometry(*actor), 1u);
        const osg::Vec4f gray(126.f / 255, 126.f / 255, 126.f / 255, 1);
        applyTes4HairTint(*actor, gray, scenes);
        auto* hair = dynamic_cast<osg::Geometry*>(actor->getChild(0));
        ASSERT_NE(hair, nullptr);
        ASSERT_NE(hair, geometry);
        ASSERT_NE(hair->getStateSet()->getUniform("colorMode"), nullptr);
        ASSERT_TRUE(hair->getStateSet()->getUniform("colorMode")->get(mode));
        EXPECT_EQ(mode, 0); // The actor material, not authoring vertices, now supplies tint.
        const auto* material = dynamic_cast<const osg::Material*>(actor->getStateSet()->getAttribute(osg::StateAttribute::MATERIAL));
        ASSERT_NE(material, nullptr);
        EXPECT_EQ(material->getDiffuse(osg::Material::FRONT), gray);
        ASSERT_TRUE(geometry->getStateSet()->getUniform("colorMode")->get(mode));
        EXPECT_EQ(mode, 2); // A second actor's template must remain unchanged.
        EXPECT_EQ(authored->getColorMode(), osg::Material::AMBIENT_AND_DIFFUSE);
    }

    TEST(Tes4FaceGen, ColorsOnlyEmbeddedBodySkinAndPreservesAuthoredStateAndOtherActors)
    {
        ESM4::Race race;
        race.mBodyPartsMale.resize(ESM4::Race::NumBodyParts);
        race.mBodyPartsFemale.resize(ESM4::Race::NumBodyParts);
        race.mBodyPartsMale[ESM4::Race::UpperBody].texture = "textures/characters/darkelf/male/upperbodymale.dds";
        osg::ref_ptr<osg::Group> model = new osg::Group;
        const std::array<const char*, 6> paths = {
            "Textures\\Characters\\Imperial\\Male\\UpperBodyMale.dds",
            "textures/characters/imperial/male/legmale.dds",
            "textures/characters/imperial/male/handmale.dds",
            "textures/characters/imperial/male/footmale.dds",
            "textures/clothes/upperbodymale.dds", // A similarly named cloth is not skin.
            "textures/clothes/lowerclass/jailshirtm.dds"};
        osg::ref_ptr<osg::Texture2D> normal = new osg::Texture2D;
        osg::ref_ptr<osg::Material> material = new osg::Material;
        material->setShininess(osg::Material::FRONT_AND_BACK, 25);
        material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4f(1, 1, 1, 1));
        for (const auto path : paths)
        {
            osg::ref_ptr<osg::Geometry> shape = new osg::Geometry;
            osg::ref_ptr<osg::Image> image = new osg::Image;
            image->setFileName(path);
            osg::ref_ptr<osg::Texture2D> diffuse = new osg::Texture2D(image);
            diffuse->setWrap(osg::Texture::WRAP_S, osg::Texture::MIRROR);
            diffuse->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
            auto* state = shape->getOrCreateStateSet();
            state->setTextureAttribute(0, diffuse, osg::StateAttribute::PROTECTED);
            state->setTextureAttribute(1, normal);
            state->setAttribute(material);
            model->addChild(shape);
        }
        osg::ref_ptr<osg::Group> actor = new osg::Group(*model, osg::CopyOp::SHALLOW_COPY);
        ASSERT_EQ(isolateTes4ActorGeometry(*actor), paths.size());
        std::vector<std::size_t> selected;
        osg::ref_ptr<osg::Image> colored = new osg::Image;
        EXPECT_EQ(replaceTes4ClothingSkin(*actor, race, false, [&](std::size_t index) {
            selected.push_back(index);
            return colored;
        }), 4u);
        EXPECT_EQ(selected, (std::vector<std::size_t>{0, 1, 2, 3}));
        for (std::size_t i = 0; i < paths.size(); ++i)
        {
            const auto* state = actor->getChild(i)->getStateSet();
            const auto* diffuse = dynamic_cast<const osg::Texture2D*>(state->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
            const auto* original = dynamic_cast<const osg::Texture2D*>(model->getChild(i)->getStateSet()->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
            ASSERT_NE(diffuse, nullptr);
            ASSERT_NE(original, nullptr);
            EXPECT_NE(original->getImage(), colored);
            EXPECT_EQ(diffuse->getImage(), i < 4 ? colored.get() : original->getImage());
            EXPECT_EQ(diffuse->getWrap(osg::Texture::WRAP_S), osg::Texture::MIRROR);
            EXPECT_EQ(diffuse->getWrap(osg::Texture::WRAP_T), osg::Texture::REPEAT);
            EXPECT_EQ(state->getTextureAttributePair(0, osg::StateAttribute::TEXTURE)->second, osg::StateAttribute::PROTECTED);
            EXPECT_EQ(state->getTextureAttribute(1, osg::StateAttribute::TEXTURE), normal);
            EXPECT_EQ(state->getAttribute(osg::StateAttribute::MATERIAL), material);
        }
    }

    TEST(Tes4FaceGen, RecognizesFemaleAndCustomRaceSkinWithoutTouchingUnresolvedTextures)
    {
        ESM4::Race race;
        race.mBodyPartsMale.resize(ESM4::Race::NumBodyParts);
        race.mBodyPartsFemale.resize(ESM4::Race::NumBodyParts);
        race.mBodyPartsFemale[ESM4::Race::UpperBody].texture = "custom/skin.dds";
        for (const auto path : {"textures/characters/imperial/female/upperbodyfemale.dds", "textures/custom/skin.dds"})
        {
            osg::ref_ptr<osg::Group> node = new osg::Group;
            osg::ref_ptr<osg::Image> image = new osg::Image;
            image->setFileName(path);
            osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D(image);
            node->getOrCreateStateSet()->setTextureAttribute(0, texture);
            const auto* original = node->getStateSet();
            std::size_t calls = 0;
            EXPECT_EQ(replaceTes4ClothingSkin(*node, race, true, [&](std::size_t index) -> osg::ref_ptr<osg::Image> {
                ++calls;
                EXPECT_EQ(index, ESM4::Race::UpperBody);
                return {}; // Missing skin leaves the authored state intact.
            }), 0u);
            EXPECT_EQ(calls, 1u);
            EXPECT_EQ(node->getStateSet(), original);
            EXPECT_EQ(texture->getImage(), image);
        }
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

    TEST(Tes4FaceGen, MapsEgtTopToBottomRowsToImageOrigin)
    {
        osg::ref_ptr<osg::Image> base = new osg::Image;
        base->allocateImage(1, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE);
        base->setColor(osg::Vec4f(0.f, 0.f, 0.f, 1.f), 0, 0);
        base->setColor(osg::Vec4f(0.f, 0.f, 0.f, 1.f), 0, 1);

        ESM4::FaceGenTextureMode mode;
        mode.mScale = 1.f;
        mode.mRed = { 10, 20 };
        mode.mGreen = { 0, 0 };
        mode.mBlue = { 0, 0 };
        ESM4::FaceGenEgt egt;
        egt.mWidth = 1;
        egt.mHeight = 2;
        egt.mSymmetricTextures.push_back(mode);

        base->setOrigin(osg::Image::BOTTOM_LEFT);
        osg::ref_ptr<osg::Image> bottomLeft = applyTes4FaceGenEgt(*base, egt, {}, { 1.f });
        ASSERT_NE(bottomLeft, nullptr);
        EXPECT_EQ(bottomLeft->getOrigin(), osg::Image::BOTTOM_LEFT);
        EXPECT_NEAR(bottomLeft->getColor(0, 0).r(), 20.f / 255.f, 1.f / 255.f);
        EXPECT_NEAR(bottomLeft->getColor(0, 1).r(), 10.f / 255.f, 1.f / 255.f);

        base->setOrigin(osg::Image::TOP_LEFT);
        osg::ref_ptr<osg::Image> topLeft = applyTes4FaceGenEgt(*base, egt, {}, { 1.f });
        ASSERT_NE(topLeft, nullptr);
        EXPECT_EQ(topLeft->getOrigin(), osg::Image::TOP_LEFT);
        EXPECT_NEAR(topLeft->getColor(0, 0).r(), 10.f / 255.f, 1.f / 255.f);
        EXPECT_NEAR(topLeft->getColor(0, 1).r(), 20.f / 255.f, 1.f / 255.f);
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

    TEST(Tes4FaceGen, KeepsInnerMouthInBipedHeadCoordinates)
    {
        EXPECT_TRUE(shouldCorrectTes4HeadPartOrientation(ESM4::Race::Head));
        EXPECT_TRUE(shouldCorrectTes4HeadPartOrientation(ESM4::Race::EarMale));
        EXPECT_TRUE(shouldCorrectTes4HeadPartOrientation(ESM4::Race::EarFemale));
        EXPECT_FALSE(shouldCorrectTes4HeadPartOrientation(ESM4::Race::Mouth));
        EXPECT_FALSE(shouldCorrectTes4HeadPartOrientation(ESM4::Race::TeethLower));
        EXPECT_FALSE(shouldCorrectTes4HeadPartOrientation(ESM4::Race::TeethUpper));
        EXPECT_FALSE(shouldCorrectTes4HeadPartOrientation(ESM4::Race::Tongue));
        EXPECT_TRUE(shouldCorrectTes4HeadPartOrientation(ESM4::Race::EyeLeft));
        EXPECT_TRUE(shouldCorrectTes4HeadPartOrientation(ESM4::Race::EyeRight));
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
