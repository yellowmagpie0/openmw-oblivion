#ifndef OPENMW_COMPONENTS_RESOURCE_TREEMODEL_H
#define OPENMW_COMPONENTS_RESOURCE_TREEMODEL_H

#include <components/vfs/pathutil.hpp>

#include <osg/ref_ptr>

namespace osg
{
    class Node;
}
namespace VFS
{
    class Manager;
}
namespace Resource
{
    class ImageManager;

    struct TreeModel
    {
        VFS::Path::Normalized mSource;
        VFS::Path::Normalized mLeaves;
        float mWidth = 0;
        float mHeight = 0;
    };

    osg::ref_ptr<osg::Node> loadTree(const TreeModel& model, const VFS::Manager& vfs, ImageManager& images);
}

#endif
