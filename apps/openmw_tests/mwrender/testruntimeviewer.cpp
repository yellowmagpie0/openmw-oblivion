#include <apps/openmw/mwrender/runtimeviewer.hpp>

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

        RuntimeViewerTest()
        {
            mViewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);
            mViewer->getCamera()->setGraphicsContext(mContext);
        }

        ~RuntimeViewerTest() override
        {
            mBin->setDrawCallback(mPreviousDraw);
            mContext->setResizedCallback(nullptr);
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
        const auto original = mViewer->captureBindings();

        osg::ref_ptr<osg::Camera> candidateCamera = new osg::Camera;
        candidateCamera->setGraphicsContext(mContext);
        candidateCamera->setViewport(0, 0, 32, 32);
        osg::ref_ptr<osg::Group> candidateScene = new osg::Group;
        osg::ref_ptr<ResizeCallback> candidateResize = new ResizeCallback;
        osg::ref_ptr<DrawCallback> candidateDraw = new DrawCallback;
        mViewer->setCamera(candidateCamera);
        mViewer->setSceneData(candidateScene);
        mContext->setResizedCallback(candidateResize);
        mBin->setDrawCallback(candidateDraw);
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
            EXPECT_EQ(mViewer->getStartTick(), 12345u);
            mContext->resized(0, 0, 32, 32);
            ASSERT_TRUE(mViewer->selectBindings(candidate));
            EXPECT_EQ(mViewer->getSceneData(), candidateScene.get());
            EXPECT_EQ(mViewer->getCamera(), candidateCamera.get());
            EXPECT_TRUE(candidateCamera->containsNode(candidateScene));
            EXPECT_EQ(candidateCamera->getRenderer(), candidateRenderer);
            EXPECT_EQ(mBin->getDrawCallback(), candidateDraw.get());
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
}
