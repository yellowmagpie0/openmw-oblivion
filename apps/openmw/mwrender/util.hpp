#ifndef OPENMW_MWRENDER_UTIL_H
#define OPENMW_MWRENDER_UTIL_H

#include <cstddef>
#include <optional>
#include <vector>

#include <osg/Matrix>

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
    class SceneManager;
}

namespace MWRender
{
    // Own the exact attachment path while a native drop is prepared. A later
    // scene move, detach, duplicate parent, or hidden attachment invalidates it.
    // This is an assembled model matrix, not an ESM reference rotation.
    class OblivionAttachedWeaponPose
    {
    public:
        static std::optional<OblivionAttachedWeaponPose> capture(osg::Node& placement, osg::Node& attached);
        const osg::Matrix& getWorldMatrix() const { return mWorldMatrix; }
        const osg::Node* getPlacementNode() const { return mPath.empty() ? nullptr : mPath.front().get(); }
        const osg::Node* getAttachedNode() const { return mPath.empty() ? nullptr : mPath.back().get(); }
        bool matchesCurrentScene() const noexcept;

    private:
        std::vector<osg::ref_ptr<osg::Node>> mPath;
        osg::Matrix mWorldMatrix;
    };

    // TES4 weapon meshes contain their sheath alongside the blade. Hide only
    // that geometry on the drawn instance, leaving the cached model intact.
    void hideOblivionWeaponScabbard(osg::Node& node);
    // Clone only the native arrow shape, retaining its authored local transform.
    // The ammunition model also contains its quiver and spare arrows.
    osg::ref_ptr<osg::Node> cloneOblivionArrowGeometry(
        const osg::Node& ammunition, Resource::SceneManager& sceneManager);


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
