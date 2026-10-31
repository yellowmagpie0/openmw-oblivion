#include <components/testing/util.hpp>
#include <osg/Group>
#include <osg/StateSet>
#include <osg/Uniform>
#include <osgDB/Registry>
#include <osgDB/ReaderWriter>
#include <sstream>
#include <components/files/configurationmanager.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/shadow.hpp>
#include <components/toutf8/toutf8.hpp>
#include <components/vfs/manager.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>

namespace
{
    using namespace testing;

    TEST(ResourceResourceSystem, scenemanager_getinstance_should_be_thread_safe)
    {
        const VFS::Manager vfsManager;
        const ToUTF8::Utf8Encoder encoder(ToUTF8::WINDOWS_1252);
        Resource::ResourceSystem resourceSystem(&vfsManager, 1.0, &encoder.getStatelessEncoder());
        Resource::SceneManager* sceneManager = resourceSystem.getSceneManager();
        const Files::ConfigurationManager configurationManager;
        sceneManager->setShaderPath(configurationManager.getLocalPath() / "resources/shaders");

        auto defines = Shader::getDefaultDefines();

        auto shadowDefines = SceneUtil::ShadowManager::getShadowsDisabledDefines();

        osg::ref_ptr<SceneUtil::LightManager> lightManager
            = new SceneUtil::LightManager(SceneUtil::LightSettings{}, &resourceSystem);
        auto lightDefines = lightManager->getLightDefines();

        for (const auto& define : shadowDefines)
            defines[define.first] = define.second;
        for (const auto& define : lightDefines)
            defines[define.first] = define.second;

        sceneManager->getShaderManager().setGlobalDefines(defines);

        constexpr VFS::Path::NormalizedView noSuchPath("meshes/whatever.nif");
        std::vector<std::thread> threads;

        for (int i = 0; i < 50; ++i)
        {
            threads.emplace_back([=]() { sceneManager->getInstance(noSuchPath); });
        }
        for (std::thread& thread : threads)
            thread.join();
    }
}

namespace
{
    void configureTemplateTestScene(Resource::ResourceSystem& resources)
    {
        const Files::ConfigurationManager config;
        auto* scenes = resources.getSceneManager();
        scenes->setShaderPath(config.getLocalPath() / "resources/shaders");
        auto defines = Shader::getDefaultDefines();
        for (const auto& [name, value] : SceneUtil::ShadowManager::getShadowsDisabledDefines())
            defines[name] = value;
        osg::ref_ptr<SceneUtil::LightManager> lights
            = new SceneUtil::LightManager(SceneUtil::LightSettings{}, &resources);
        for (const auto& [name, value] : lights->getLightDefines())
            defines[name] = value;
        scenes->getShaderManager().setGlobalDefines(defines);
    }

    TEST(ResourceResourceSystem, StrictTemplatesRejectMissingAndCachedFallbackWhileDefaultBehaviorRemains)
    {
        VFS::Manager vfs;
        Resource::ResourceSystem resources(&vfs, 0., nullptr);
        configureTemplateTestScene(resources);
        auto* scenes = resources.getSceneManager();
        constexpr VFS::Path::NormalizedView missing("missing-model.nif");
        EXPECT_THROW(scenes->getTemplate(missing, false, true), std::runtime_error);
        EXPECT_FALSE(scenes->checkLoaded(missing, 0.));
        auto fallback = scenes->getTemplate(missing, false);
        ASSERT_TRUE(fallback);
        EXPECT_TRUE(scenes->checkLoaded(missing, 0.));
        EXPECT_THROW(scenes->getTemplate(missing, false, true), std::runtime_error);
        EXPECT_EQ(scenes->getTemplate(missing, false).get(), fallback.get());
        EXPECT_TRUE(scenes->getInstance(missing));
    }

    TEST(ResourceResourceSystem, StrictTemplatesLoadActualSceneAndRejectMalformedAuthoredBytes)
    {
        osg::ref_ptr<osg::Group> model = new osg::Group;
        model->setName("Independent authored model");
        std::ostringstream bytes;
        auto* writer = osgDB::Registry::instance()->getReaderWriterForExtension("osgt");
        ASSERT_NE(writer, nullptr);
        ASSERT_TRUE(writer->writeNode(*model, bytes).success());
        TestingOpenMW::VFSTestFile valid(bytes.str()), invalid("not an authored NIF");
        auto vfs = TestingOpenMW::createTestVFS({
            {VFS::Path::NormalizedView("model.osgt"), &valid},
            {VFS::Path::NormalizedView("broken.nif"), &invalid}});
        Resource::ResourceSystem resources(vfs.get(), 0., nullptr);
        configureTemplateTestScene(resources);
        auto* scenes = resources.getSceneManager();
        constexpr VFS::Path::NormalizedView path("model.osgt"), bad("broken.nif");
        auto loaded = scenes->getTemplate(path, false, true);
        ASSERT_TRUE(loaded);
        EXPECT_EQ(loaded->getName(), model->getName());
        EXPECT_EQ(scenes->getTemplate(path, false, true).get(), loaded.get());
        EXPECT_EQ(scenes->getTemplate(path, false).get(), loaded.get());
        EXPECT_NE(scenes->getInstance(path).get(), loaded.get());
        EXPECT_THROW(scenes->getTemplate(bad, false, true), std::runtime_error);
        EXPECT_FALSE(scenes->checkLoaded(bad, 0.));
        auto fallback = scenes->getTemplate(bad, false);
        ASSERT_TRUE(fallback);
        EXPECT_THROW(scenes->getTemplate(bad, false, true), std::runtime_error);
    }
}

namespace
{
    TEST(ResourceResourceSystem, EmbeddedLightsHaveModelLocalDefaultsAndKeepAuthoredValues)
    {
        const char* names[] = {"nifEmbeddedLightAmbient", "nifEmbeddedLightDiffuse",
            "nifEmbeddedLightSpecular", "nifEmbeddedLightDirection"};
        const osg::Vec4f light(.25f, .5f, .75f, 1.f);
        osg::ref_ptr<osg::Group> lit = new osg::Group, plain = new osg::Group;
        for (const auto* name : names)
            lit->getOrCreateStateSet()->addUniform(new osg::Uniform(name, light));
        auto* writer = osgDB::Registry::instance()->getReaderWriterForExtension("osgt");
        ASSERT_NE(writer, nullptr);
        std::ostringstream litBytes, plainBytes;
        ASSERT_TRUE(writer->writeNode(*lit, litBytes).success());
        ASSERT_TRUE(writer->writeNode(*plain, plainBytes).success());
        TestingOpenMW::VFSTestFile litFile(litBytes.str()), plainFile(plainBytes.str());
        auto vfs = TestingOpenMW::createTestVFS({
            {VFS::Path::NormalizedView("lit.osgt"), &litFile},
            {VFS::Path::NormalizedView("plain.osgt"), &plainFile}});
        Resource::ResourceSystem resources(vfs.get(), 0., nullptr);
        configureTemplateTestScene(resources);
        auto* scenes = resources.getSceneManager();
        auto first = scenes->getTemplate(VFS::Path::NormalizedView("lit.osgt"), false, true);
        auto next = scenes->getTemplate(VFS::Path::NormalizedView("plain.osgt"), false, true);
        for (const auto* name : names)
        {
            SCOPED_TRACE(name);
            ASSERT_NE(first->getStateSet(), nullptr);
            ASSERT_NE(first->getStateSet()->getUniform(name), nullptr);
            osg::Vec4f actual;
            ASSERT_TRUE(first->getStateSet()->getUniform(name)->get(actual));
            EXPECT_EQ(actual, light); // Authored lighting still overrides the baseline.
            ASSERT_NE(next->getStateSet(), nullptr);
            ASSERT_NE(next->getStateSet()->getUniform(name), nullptr);
            ASSERT_TRUE(next->getStateSet()->getUniform(name)->get(actual));
            EXPECT_EQ(actual, osg::Vec4f()); // Do not reuse the previous model's GLSL values.
        }
    }
}
