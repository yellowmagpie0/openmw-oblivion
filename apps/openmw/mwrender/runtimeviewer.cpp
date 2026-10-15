#include "runtimeviewer.hpp"

#include <stdexcept>

namespace MWRender
{
    void RuntimeViewer::prepareSceneData(osg::Node* node)
    {
        if (areThreadsRunning())
            throw std::logic_error("Scene preparation requires stopped viewer threads");
        osgViewer::View::setSceneData(node);
    }

    RuntimeViewer::Bindings RuntimeViewer::captureBindings()
    {
        auto* camera = getCamera();
        auto* bin = osgUtil::RenderBin::getRenderBinPrototype("DepthSortedBin");
        if (areThreadsRunning() || !camera || !camera->getRenderer() || !camera->getGraphicsContext()
            || !getScene() || !getSceneData() || !camera->containsNode(getSceneData()) || !bin)
            throw std::logic_error("Viewer bindings require a prepared scene and stopped threads");

        Bindings result;
        result.mOwner = this;
        result.mScene = getScene();
        result.mCamera = camera;
        result.mRenderer = camera->getRenderer();
        if (!mSelectedRegistration || mSelectedRegistration->mCamera != camera
            || mSelectedRegistration->mRenderer != camera->getRenderer())
            mSelectedRegistration = std::make_shared<CameraRegistration>(camera, camera->getRenderer());
        result.mRegistration = mSelectedRegistration;
        result.mContext = camera->getGraphicsContext();
        result.mResizeCallback = result.mContext->getResizedCallback();
        result.mTransparentBin = bin;
        result.mTransparentCallback = bin->getDrawCallback();
        result.mDistortionBin = osgUtil::RenderBin::getRenderBinPrototype("Distortion");
        if (result.mDistortionBin)
            result.mDistortionCallback = result.mDistortionBin->getDrawCallback();
        return result;
    }

    bool RuntimeViewer::selectBindings(const Bindings& bindings) noexcept
    {
        if (areThreadsRunning() || bindings.mOwner.get() != this || !bindings.mScene || !bindings.mCamera
            || !bindings.mRegistration
            || (bindings.mCamera->getRenderer() != bindings.mRenderer.get()
                && !(bindings.mRegistration->mParked && !bindings.mCamera->getRenderer()))
            || bindings.mCamera->getGraphicsContext() != bindings.mContext.get())
            return false;

        // GraphicsContext::runOperations draws every registered camera, including a camera
        // no longer selected by View. Keep its context/GL objects and actual Renderer alive,
        // but park the camera's renderer slot so the archived graph cannot be drawn.
        auto* outgoing = getCamera();
        if (outgoing != bindings.mCamera.get())
        {
            if (mSelectedRegistration && mSelectedRegistration->mCamera == outgoing)
                mSelectedRegistration->mParked = true;
            outgoing->setRenderer(nullptr);
        }
        bindings.mCamera->setRenderer(bindings.mRenderer);
        bindings.mRegistration->mParked = false;
        mSelectedRegistration = bindings.mRegistration;
        // OSG's regular setSceneData traverses the graph and Viewer also resets the start tick.
        // Both cameras already own their prepared scene children; select the existing Scene directly.
        _scene = bindings.mScene;
        mSelectingBindings = true;
        setCamera(bindings.mCamera);
        mSelectingBindings = false;
        bindings.mContext->setResizedCallback(bindings.mResizeCallback);
        bindings.mTransparentBin->setDrawCallback(bindings.mTransparentCallback);
        if (bindings.mDistortionBin)
            bindings.mDistortionBin->setDrawCallback(bindings.mDistortionCallback);
        return true;
    }

    osg::GraphicsOperation* RuntimeViewer::createRenderer(osg::Camera* camera)
    {
        if (mSelectingBindings)
            return camera->getRenderer();
        return osgViewer::Viewer::createRenderer(camera);
    }
}
