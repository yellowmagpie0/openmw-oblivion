#ifndef OPENMW_MWRENDER_TES4HAIR_H
#define OPENMW_MWRENDER_TES4HAIR_H

namespace osg { class Node; class Vec4f; }
namespace Resource { class SceneManager; }

namespace MWRender
{
    // Hair meshes contain authoring vertex colors. The actor tint must also
    // replace the cached shader color mode, without modifying shared templates.
    void applyTes4HairTint(osg::Node& node, const osg::Vec4f& color, Resource::SceneManager& scenes);
}

#endif
