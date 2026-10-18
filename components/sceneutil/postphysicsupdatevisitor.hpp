#ifndef OPENMW_COMPONENTS_SCENEUTIL_POSTPHYSICSUPDATEVISITOR_HPP
#define OPENMW_COMPONENTS_SCENEUTIL_POSTPHYSICSUPDATEVISITOR_HPP

#include <osg/NodeCallback>
#include <osgUtil/UpdateVisitor>

namespace SceneUtil
{
    // An owned physical scene advances before the world step. The later
    // viewer pass must reach this guard before running root StateSet callbacks.
    class PrePhysicsSceneCallback : public osg::NodeCallback
    {
    public:
        virtual bool finishAfterPhysics(osg::NodeVisitor& visitor) = 0;
    };

    class PostPhysicsUpdateVisitor : public osgUtil::UpdateVisitor
    {
        template <class T> void update(T& node)
        {
            auto* owner = dynamic_cast<PrePhysicsSceneCallback*>(node.getUpdateCallback());
            if (!owner || !owner->finishAfterPhysics(*this))
                osgUtil::UpdateVisitor::apply(node);
        }
    public:
        PostPhysicsUpdateVisitor() = default;
        void apply(osg::Node& node) override { update(node); }
        void apply(osg::Group& node) override { update(node); }
        void apply(osg::Transform& node) override { update(node); }
        void apply(osg::Geode& node) override { update(node); }
        void apply(osg::Drawable& node) override { update(node); }
        void apply(osg::Billboard& node) override { update(node); }
        void apply(osg::LightSource& node) override { update(node); }
        void apply(osg::Projection& node) override { update(node); }
        void apply(osg::Switch& node) override { update(node); }
        void apply(osg::LOD& node) override { update(node); }
        void apply(osg::OccluderNode& node) override { update(node); }
    };
}

#endif
