#ifndef OPENMW_MWRENDER_UTIL_H
#define OPENMW_MWRENDER_UTIL_H

#include <cstddef>

#include <components/vfs/pathutil.hpp>

#include <osg/NodeCallback>

namespace osg
{
    class Image;
    class Node;
}

namespace Resource
{
    class ResourceSystem;
}

namespace MWRender
{
    // Overrides the texture of nodes in the mesh that had the same NiTexturingProperty as the first NiTexturingProperty
    // of the .NIF file's root node, if it had a NiTexturingProperty. Used for applying "particle textures" to magic
    // effects.
    void overrideFirstRootTexture(
        VFS::Path::NormalizedView texture, Resource::ResourceSystem* resourceSystem, osg::Node& node);
    void overrideFirstRootTexture(
        osg::ref_ptr<osg::Image> image, Resource::ResourceSystem* resourceSystem, osg::Node& node);

    void overrideTexture(VFS::Path::NormalizedView texture, Resource::ResourceSystem* resourceSystem, osg::Node& node);
    void overrideTexture(osg::ref_ptr<osg::Image> image, Resource::ResourceSystem* resourceSystem, osg::Node& node);
    std::size_t overrideAllTextures(
        VFS::Path::NormalizedView texture, Resource::ResourceSystem* resourceSystem, osg::Node& node);
    std::size_t overrideAllTextures(
        osg::ref_ptr<osg::Image> image, Resource::ResourceSystem* resourceSystem, osg::Node& node);

    // Node callback to entirely skip the traversal.
    class NoTraverseCallback : public osg::NodeCallback
    {
    public:
        void operator()(osg::Node* node, osg::NodeVisitor* nv) override
        {
            // no traverse()
        }
    };

    bool shouldAddMSAAIntermediateTarget();
}

#endif
