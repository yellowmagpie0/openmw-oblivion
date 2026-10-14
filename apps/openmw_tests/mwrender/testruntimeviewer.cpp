#include <apps/openmw/mwrender/runtimeviewer.hpp>
#include <components/sceneutil/shadow.hpp>
#include <components/settings/values.hpp>
#include <components/shader/shadermanager.hpp>
#include <components/sceneutil/glextensions.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <SDL.h>

#include <gtest/gtest.h>
#include <osg/Group>
#include <osgViewer/GraphicsWindow>

namespace
{
    struct ResizeCallback : osg::GraphicsContext::ResizedCallback
    {
        int mCalls = 0;
        void resizedImplementation(osg::GraphicsContext*, int, int, int, int) override { ++mCalls; }
    };

    struct DrawCallback : osgUtil::RenderBin::DrawCallback
    {
        void drawImplementation(osgUtil::RenderBin*, osg::RenderInfo&, osgUtil::RenderLeaf*&) override {}
    };

    struct RuntimeViewerTest : testing::Test
    {
        osg::ref_ptr<MWRender::RuntimeViewer> mViewer = new MWRender::RuntimeViewer;
        osg::ref_ptr<osgViewer::GraphicsWindowEmbedded> mContext
            = new osgViewer::GraphicsWindowEmbedded(0, 0, 32, 32);
        osg::ref_ptr<osgUtil::RenderBin> mBin = osgUtil::RenderBin::getRenderBinPrototype("DepthSortedBin");
        osg::ref_ptr<osgUtil::RenderBin::DrawCallback> mPreviousDraw = mBin->getDrawCallback();
        osg::ref_ptr<osgUtil::RenderBin> mPreviousDistortion = osgUtil::RenderBin::getRenderBinPrototype("Distortion");
        osg::ref_ptr<osgUtil::RenderBin> mDistortion = mPreviousDistortion
            ? mPreviousDistortion.get() : new osgUtil::RenderBin;
        osg::ref_ptr<osgUtil::RenderBin::DrawCallback> mPreviousDistortionDraw = mDistortion->getDrawCallback();

        RuntimeViewerTest()
        {
            mViewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);
            mViewer->getCamera()->setGraphicsContext(mContext);
            if (!mPreviousDistortion)
                osgUtil::RenderBin::addRenderBinPrototype("Distortion", mDistortion);
        }

        ~RuntimeViewerTest() override
        {
            mBin->setDrawCallback(mPreviousDraw);
            mContext->setResizedCallback(nullptr);
            mDistortion->setDrawCallback(mPreviousDistortionDraw);
            if (!mPreviousDistortion)
                osgUtil::RenderBin::removeRenderBinPrototype(mDistortion);
        }
    };

    TEST_F(RuntimeViewerTest, SelectsActualSceneCameraAndCallbacksWithoutResettingProcessClock)
    {
        osg::ref_ptr<osg::Camera> originalCamera = mViewer->getCamera();
        osg::ref_ptr<osg::Group> originalScene = new osg::Group;
        osg::ref_ptr<ResizeCallback> originalResize = new ResizeCallback;
        osg::ref_ptr<DrawCallback> originalDraw = new DrawCallback;
        mViewer->setSceneData(originalScene);
        mContext->setResizedCallback(originalResize);
        mBin->setDrawCallback(originalDraw);
        mDistortion->setDrawCallback(originalDraw);
        const auto original = mViewer->captureBindings();

        osg::ref_ptr<osg::Camera> candidateCamera = new osg::Camera;
        candidateCamera->setGraphicsContext(mContext);
        candidateCamera->setViewport(0, 0, 32, 32);
        osg::ref_ptr<osg::Group> candidateScene = new osg::Group;
        osg::ref_ptr<ResizeCallback> candidateResize = new ResizeCallback;
        osg::ref_ptr<DrawCallback> candidateDraw = new DrawCallback;
        mViewer->setCamera(candidateCamera);
        mViewer->setStartTick(12345);
        mViewer->prepareSceneData(candidateScene);
        EXPECT_EQ(mViewer->getStartTick(), 12345u);
        mContext->setResizedCallback(candidateResize);
        mBin->setDrawCallback(candidateDraw);
        mDistortion->setDrawCallback(candidateDraw);
        const auto candidate = mViewer->captureBindings();
        auto* candidateRenderer = candidateCamera->getRenderer();
        mViewer->setStartTick(12345);

        for (int i = 0; i < 3; ++i)
        {
            ASSERT_TRUE(mViewer->selectBindings(original));
            EXPECT_EQ(mViewer->getSceneData(), originalScene.get());
            EXPECT_EQ(mViewer->getCamera(), originalCamera.get());
            EXPECT_TRUE(originalCamera->containsNode(originalScene));
            EXPECT_EQ(mBin->getDrawCallback(), originalDraw.get());
            EXPECT_EQ(mDistortion->getDrawCallback(), originalDraw.get());
            EXPECT_EQ(mViewer->getStartTick(), 12345u);
            mContext->resized(0, 0, 32, 32);
            ASSERT_TRUE(mViewer->selectBindings(candidate));
            EXPECT_EQ(mViewer->getSceneData(), candidateScene.get());
            EXPECT_EQ(mViewer->getCamera(), candidateCamera.get());
            EXPECT_TRUE(candidateCamera->containsNode(candidateScene));
            EXPECT_EQ(candidateCamera->getRenderer(), candidateRenderer);
            EXPECT_EQ(mBin->getDrawCallback(), candidateDraw.get());
            EXPECT_EQ(mDistortion->getDrawCallback(), candidateDraw.get());
            EXPECT_EQ(mViewer->getStartTick(), 12345u);
            mContext->resized(0, 0, 32, 32);
        }
        EXPECT_EQ(originalResize->mCalls, 3);
        EXPECT_EQ(candidateResize->mCalls, 3);
    }

    TEST_F(RuntimeViewerTest, RejectsForeignDefaultAndChangedContextWithoutChangingActiveBindings)
    {
        osg::ref_ptr<osg::Group> scene = new osg::Group;
        mViewer->setSceneData(scene);
        const auto bindings = mViewer->captureBindings();
        osg::ref_ptr<MWRender::RuntimeViewer> other = new MWRender::RuntimeViewer;
        EXPECT_FALSE(other->selectBindings(bindings));
        EXPECT_FALSE(mViewer->selectBindings(MWRender::RuntimeViewer::Bindings{}));
        auto* camera = mViewer->getCamera();
        osg::ref_ptr<osgViewer::GraphicsWindowEmbedded> changed
            = new osgViewer::GraphicsWindowEmbedded(0, 0, 64, 64);
        camera->setGraphicsContext(changed);
        EXPECT_FALSE(mViewer->selectBindings(bindings));
        EXPECT_EQ(mViewer->getCamera(), camera);
        EXPECT_EQ(mViewer->getSceneData(), scene.get());
        EXPECT_EQ(camera->getGraphicsContext(), changed.get());
    }

    TEST_F(RuntimeViewerTest, RejectsUnpreparedScene)
    {
        EXPECT_THROW(mViewer->captureBindings(), std::logic_error);
    }

    TEST(ShadowRuntimeTest, DetachedManagersPreserveSelectionAndDisposalClearsOnlySelectedOwner)
    {
        ASSERT_EQ(SDL_InitSubSystem(SDL_INIT_VIDEO), 0) << SDL_GetError();
        struct QuitVideo { ~QuitVideo() { SDL_QuitSubSystem(SDL_INIT_VIDEO); } } quitVideo;
        std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
            SDL_CreateWindow("Shadow retention", 0, 0, 32, 32, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN),
            SDL_DestroyWindow);
        ASSERT_TRUE(window) << SDL_GetError();
        osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
        traits->width = traits->height = 32;
        traits->inheritedWindowData = new SDLUtil::GraphicsWindowSDL2::WindowData(window.get());
        osg::ref_ptr<SDLUtil::GraphicsWindowSDL2> context = new SDLUtil::GraphicsWindowSDL2(traits, SDLUtil::Disabled);
        ASSERT_TRUE(context->valid() && context->realize() && context->makeCurrent());
        osg::ref_ptr<SceneUtil::GetGLExtensionsOperation> extensions = new SceneUtil::GetGLExtensionsOperation;
        (*extensions)(context);
        struct RestoreSelection
        {
            const SceneUtil::ShadowManager* mOriginal = nullptr;
            RestoreSelection()
            {
                try { mOriginal = &SceneUtil::ShadowManager::instance(); }
                catch (const std::logic_error&) {}
            }
            ~RestoreSelection() { if (mOriginal) mOriginal->select(); }
        } restore;
        Shader::ShaderManager shaders;
        shaders.setShaderPath(OPENMW_PROJECT_SOURCE_DIR "/files/shaders");
        osg::ref_ptr<osg::Group> originalRoot = new osg::Group, originalScene = new osg::Group;
        osg::ref_ptr<osg::Group> candidateRoot = new osg::Group, candidateScene = new osg::Group;
        auto original = std::make_unique<SceneUtil::ShadowManager>(originalScene, originalRoot, 1, 2, 3,
            Settings::shadows(), shaders, SceneUtil::ShadowManager::Installation::Detached);
        original->select();
        auto candidate = std::make_unique<SceneUtil::ShadowManager>(candidateScene, candidateRoot, 4, 5, 6,
            Settings::shadows(), shaders, SceneUtil::ShadowManager::Installation::Detached);
        EXPECT_EQ(&SceneUtil::ShadowManager::instance(), original.get());
        EXPECT_EQ(originalRoot->getNumChildren(), 1u);
        EXPECT_EQ(candidateRoot->getNumChildren(), 1u);
        EXPECT_NE(originalRoot->getChild(0), candidateRoot->getChild(0));
        EXPECT_THROW((SceneUtil::ShadowManager(candidateScene, candidateRoot, 4, 5, 6,
            Settings::shadows(), shaders)), std::logic_error);
        candidate->select();
        EXPECT_EQ(&SceneUtil::ShadowManager::instance(), candidate.get());
        original->select();
        candidate.reset();
        EXPECT_EQ(&SceneUtil::ShadowManager::instance(), original.get());
        original.reset();
        EXPECT_THROW(SceneUtil::ShadowManager::instance(), std::logic_error);
        // Initial construction is valid again after the selected owner dies.
        auto replacement = std::make_unique<SceneUtil::ShadowManager>(candidateScene, candidateRoot, 4, 5, 6,
            Settings::shadows(), shaders);
        EXPECT_EQ(&SceneUtil::ShadowManager::instance(), replacement.get());
    }
}
