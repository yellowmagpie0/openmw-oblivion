#ifndef OPENMW_MWRENDER_RUNTIMEVIEWER_H
#define OPENMW_MWRENDER_RUNTIMEVIEWER_H

#include <osg/GraphicsContext>
#include <osg/observer_ptr>
#include <osgUtil/RenderBin>
#include <osgViewer/Scene>
#include <osgViewer/Viewer>
#include <memory>

namespace MWRender
{
    /// Keeps the ordinary Viewer bootstrap behavior; only prepared selection bypasses scene traversal.
    class RuntimeViewer final : public osgViewer::Viewer
    {
        struct CameraRegistration
        {
            osg::Camera* mCamera;
            osg::GraphicsOperation* mRenderer;
            bool mParked = false;
        };
    public:
        class Bindings
        {
            friend class RuntimeViewer;
            osg::observer_ptr<RuntimeViewer> mOwner;
            osg::ref_ptr<osgViewer::Scene> mScene;
            osg::ref_ptr<osg::Camera> mCamera;
            osg::ref_ptr<osg::GraphicsOperation> mRenderer;
            std::shared_ptr<CameraRegistration> mRegistration;
            osg::ref_ptr<osg::GraphicsContext> mContext;
            osg::ref_ptr<osg::GraphicsContext::ResizedCallback> mResizeCallback;
            osg::ref_ptr<osgUtil::RenderBin> mTransparentBin;
            osg::ref_ptr<osgUtil::RenderBin::DrawCallback> mTransparentCallback;
            osg::ref_ptr<osgUtil::RenderBin> mDistortionBin;
            osg::ref_ptr<osgUtil::RenderBin::DrawCallback> mDistortionCallback;
        };

        /// Capture after the scene and camera have been installed and their renderer prepared.
        /// The caller must keep backend callback targets alive until these bindings are released.
        Bindings captureBindings();

        /// Allocation is allowed during preparation, but the process clock must keep running.
        void prepareSceneData(osg::Node* node);

        /// Requires stopped viewer threads and an idle engine frame. Does not traverse the scene,
        /// allocate a renderer, or reset the process start tick. Backend ownership stays with the caller.
        bool selectBindings(const Bindings& bindings) noexcept;

    protected:
        osg::GraphicsOperation* createRenderer(osg::Camera* camera) override;

    private:
        bool mSelectingBindings = false;
        std::shared_ptr<CameraRegistration> mSelectedRegistration;
    };
}

#endif
