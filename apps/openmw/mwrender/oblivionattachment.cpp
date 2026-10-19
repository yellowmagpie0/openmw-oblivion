#include "oblivionattachment.hpp"

#include <components/misc/osguservalues.hpp>
#include <components/nif/extra.hpp>
#include <components/nif/niffile.hpp>
#include <components/nif/node.hpp>
#include <components/resource/niffilemanager.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <osg/MatrixTransform>
#include <osg/NodeVisitor>
#include <stdexcept>

namespace MWRender
{
    namespace
    {
        std::string_view nativeString(std::string_view value)
        {
            return value.substr(0, value.find('\0'));
        }

        const Nif::Extra* parentExtra(const Nif::NiObjectNET& object)
        {
            const Nif::Extra* result = nullptr;
            for (const auto& extra : object.getExtraList())
            {
                if (extra.empty() || nativeString(extra->mName) != "Prn") continue;
                if (result) throw std::invalid_argument("duplicate native attachment parent extra");
                result = extra.getPtr();
            }
            return result;
        }

        class ParentVisitor : public osg::NodeVisitor
        {
        public:
            explicit ParentVisitor(std::string_view name)
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN), mName(name)
            {
                setNodeMaskOverride(~0u);
            }

            void apply(osg::Node& node) override
            {
                if (mFound) return;
                // Original70AC00/7073F0 searches self then children, using
                // exact C strings, independently of animation's folded map.
                if (auto* transform = dynamic_cast<osg::MatrixTransform*>(&node);
                    transform && nativeString(node.getName()) == mName)
                {
                    mFound = transform;
                    return;
                }
                traverse(node);
            }

            osg::ref_ptr<osg::MatrixTransform> mFound;
        private:
            std::string_view mName;
        };
    }

    std::string oblivionRigidAttachmentParent(const Nif::FileView& file,
        std::string_view templateHash, const Nif::NiObjectNET* overrideObject)
    {
        if (file.getHash().size() != 16 || file.getHash() != templateHash)
            throw std::invalid_argument("native attachment template and NIF identities differ");
        const auto version = file.getVersion();
        if (version != Nif::NIFFile::VER_OB_OLD && version != 0x14000004 && version != Nif::NIFFile::VER_OB)
            throw std::invalid_argument("native attachment requires an Oblivion NIF");
        if (file.numRoots() != 1 || file.getUseSkinning())
            throw std::invalid_argument("native rigid attachment requires one unskinned model root");
        const auto* root = dynamic_cast<const Nif::NiObjectNET*>(file.getRoot(0));
        if (!root) throw std::invalid_argument("native attachment has no named model root");
        const Nif::Extra* extra = overrideObject ? parentExtra(*overrideObject) : nullptr;
        if (!extra) extra = parentExtra(*root);
        const auto* parent = dynamic_cast<const Nif::NiStringExtraData*>(extra);
        if (!parent || nativeString(parent->mData).empty())
            throw std::invalid_argument("native attachment requires a Prn string parent");
        return std::string(nativeString(parent->mData));
    }

    osg::ref_ptr<osg::Node> attachOblivionRigidPart(const Nif::FileView& file,
        osg::ref_ptr<const osg::Node> model, Resource::SceneManager& sceneManager,
        osg::Node& skeleton)
    {
        if (!model) throw std::invalid_argument("native attachment has no scene template");
        std::string hash;
        model->getUserValue(Misc::OsgUserValues::sFileHash, hash);
        const auto name = oblivionRigidAttachmentParent(file, hash);
        ParentVisitor visitor(name);
        skeleton.accept(visitor);
        if (!visitor.mFound)
            throw std::invalid_argument("native attachment parent is absent: " + name);
        // Native479288 attaches the model with its authored local transform.
        // TES3 BoneOffset extraction and Left-bone mirroring do not apply.
        auto instance = sceneManager.getInstance(model);
        if (!visitor.mFound->addChild(instance))
            throw std::runtime_error("native attachment parent rejected its model");
        return instance;
    }

    osg::ref_ptr<osg::Node> attachOblivionRigidPart(Resource::ResourceSystem& resources,
        VFS::Path::NormalizedView model, osg::Node& skeleton)
    {
        auto* sceneManager = resources.getSceneManager();
        auto scene = sceneManager->getTemplate(model);
        auto file = resources.getNifFileManager()->get(model);
        return attachOblivionRigidPart(*file, std::move(scene), *sceneManager, skeleton);
    }
}
