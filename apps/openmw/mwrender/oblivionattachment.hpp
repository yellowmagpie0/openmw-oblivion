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
