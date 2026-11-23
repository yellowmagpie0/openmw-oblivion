#include "screenshotmanager.hpp"

#include <condition_variable>
#include <mutex>

#include <components/stereo/multiview.hpp>
#include <components/stereo/stereomanager.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"

#include "postprocessor.hpp"

namespace MWRender
{

    class ReadImageFromFramebufferCallback : public osg::Drawable::DrawCallback
    {
    public:
        ReadImageFromFramebufferCallback(osg::Image* image, int width, int height)
            : mWidth(width)
            , mHeight(height)
            , mImage(image)
        {
        }
        void drawImplementation(osg::RenderInfo& renderInfo, const osg::Drawable* /*drawable*/) const override
        {
            std::lock_guard lock(mMutex);
            if (mDone)
                return;
            int screenW = static_cast<int>(renderInfo.getCurrentCamera()->getViewport()->width());
            int screenH = static_cast<int>(renderInfo.getCurrentCamera()->getViewport()->height());
            if (Stereo::getStereo())
            {
                auto eyeRes = Stereo::Manager::instance().eyeResolution();
                screenW = eyeRes.x();
                screenH = eyeRes.y();
            }
            double imageaspect = double(mWidth) / double(mHeight);
            int leftPadding = std::max(0, static_cast<int>(screenW - screenH * imageaspect) / 2);
            int topPadding = std::max(0, static_cast<int>(screenH - screenW / imageaspect) / 2);
            int width = screenW - leftPadding * 2;
            int height = screenH - topPadding * 2;

            mImage->readPixels(leftPadding, topPadding, width, height, GL_RGB, GL_UNSIGNED_BYTE);
            mImage->scaleImage(mWidth, mHeight, 1);
            mDone = true;
            mCondition.notify_one();
        }

        void waitTillDone() const
        {
            std::unique_lock lock(mMutex);
            mCondition.wait(lock, [this] { return mDone; });
        }

    private:
        mutable std::condition_variable mCondition;
        mutable std::mutex mMutex;
        mutable bool mDone = false;
        int mWidth;
        int mHeight;
        osg::ref_ptr<osg::Image> mImage;
    };

    ScreenshotManager::ScreenshotManager(osgViewer::Viewer* viewer)
        : mViewer(viewer)
    {
    }

    ScreenshotManager::~ScreenshotManager() {}

    void ScreenshotManager::screenshot(osg::Image* image, int w, int h)
    {
        osg::Camera* camera = MWBase::Environment::get().getWorld()->getPostProcessor()->getHUDCamera();
        osg::ref_ptr<osg::Drawable> tempDrw = new osg::Drawable;
        osg::ref_ptr<ReadImageFromFramebufferCallback> readback = new ReadImageFromFramebufferCallback(image, w, h);
        tempDrw->setDrawCallback(readback);
        tempDrw->setCullingActive(false);
        tempDrw->getOrCreateStateSet()->setRenderBinDetails(100, "RenderBin",
            osg::StateSet::USE_RENDERBIN_DETAILS); // so its after all scene bins but before POST_RENDER gui camera
        camera->addChild(tempDrw);

        mViewer->eventTraversal();
        mViewer->updateTraversal();
        mViewer->renderingTraversals();
        // The screenshot hotkey also uses the main camera's final draw callback.
        // Wait for this thumbnail's own pixels before encoding or removing it.
        readback->waitTillDone();

        // now that we've "used up" the current frame, get a fresh frame number for the next frame() following after the
        // screenshot is completed
        mViewer->advance(mViewer->getFrameStamp()->getSimulationTime());
        camera->removeChild(tempDrw);
    }
}
