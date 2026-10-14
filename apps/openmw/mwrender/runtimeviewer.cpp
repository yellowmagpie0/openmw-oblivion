#include "runtimeviewer.hpp"

#include <stdexcept>

namespace MWRender
{
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
        result.mContext = camera->getGraphicsContext();
        result.mResizeCallback = result.mContext->getResizedCallback();
        result.mTransparentBin = bin;
        result.mTransparentCallback = bin->getDrawCallback();
        return result;
    }

    bool RuntimeViewer::selectBindings(const Bindings& bindings) noexcept
    {
        if (areThreadsRunning() || bindings.mOwner.get() != this || !bindings.mScene || !bindings.mCamera
            || bindings.mCamera->getRenderer() != bindings.mRenderer.get()
            || bindings.mCamera->getGraphicsContext() != bindings.mContext.get())
            return false;

        // OSG's regular setSceneData traverses the graph and Viewer also resets the start tick.
        // Both cameras already own their prepared scene children; select the existing Scene directly.
        _scene = bindings.mScene;
        mSelectingBindings = true;
        setCamera(bindings.mCamera);
        mSelectingBindings = false;
        bindings.mContext->setResizedCallback(bindings.mResizeCallback);
        bindings.mTransparentBin->setDrawCallback(bindings.mTransparentCallback);
        return true;
    }

    osg::GraphicsOperation* RuntimeViewer::createRenderer(osg::Camera* camera)
    {
        if (mSelectingBindings)
            return camera->getRenderer();
        return osgViewer::Viewer::createRenderer(camera);
    }
}
