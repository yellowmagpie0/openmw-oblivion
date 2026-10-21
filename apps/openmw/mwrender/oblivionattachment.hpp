#ifndef OPENMW_MWRENDER_OBLIVIONATTACHMENT_HPP
#define OPENMW_MWRENDER_OBLIVIONATTACHMENT_HPP

#include <osg/Node>
#include <components/vfs/pathutil.hpp>
#include <string>
#include <string_view>

namespace Nif { class FileView; struct NiObjectNET; }
namespace Resource { class ResourceSystem; class SceneManager; }

namespace MWRender
{
    // Original47B090 removes geometry named UpperBody* from first-person
    // instances. Bone nodes and other geometry, including Arms, remain.
    std::size_t filterOblivionBodyParts(osg::Node& instance, bool firstPerson);

    // Exact native-string search, independent of the folded animation map.
    osg::ref_ptr<osg::Node> findOblivionSkeletonNode(osg::Node& skeleton, std::string_view name);

    // The loader may wrap the native model root. Resolve the unique record
    // instantiated from the original NIF root, preserving its authored pose.
    osg::ref_ptr<osg::Node> findOblivionModelRoot(const Nif::FileView& file, osg::Node& instance);

    // Original479140: exact Prn extra, optional object before model root.
    // A present extra of the wrong type does not fall back to the model.
    std::string oblivionRigidAttachmentParent(const Nif::FileView& file,
        std::string_view templateHash, const Nif::NiObjectNET* overrideObject = nullptr);

    osg::ref_ptr<osg::Node> attachOblivionRigidPart(const Nif::FileView& file,
        osg::ref_ptr<const osg::Node> model, Resource::SceneManager& sceneManager,
        osg::Node& skeleton);

    osg::ref_ptr<osg::Node> attachOblivionRigidPart(Resource::ResourceSystem& resources,
        VFS::Path::NormalizedView model, osg::Node& skeleton);
}

#endif
