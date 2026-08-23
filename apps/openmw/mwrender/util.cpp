#include "util.hpp"

#include <osg/Node>
#include <osg/Drawable>
#include <osg/ValueObject>

#include <components/misc/resourcehelpers.hpp>
#include <components/resource/imagemanager.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/texturetype.hpp>
#include <components/sceneutil/visitor.hpp>
#include <components/settings/values.hpp>

namespace MWRender
{
    namespace
    {
        struct TextureOverrideVisitor : osg::NodeVisitor
        {
            explicit TextureOverrideVisitor(VFS::Path::NormalizedView texture, Resource::ResourceSystem* resourcesystem)
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
                , mTexture(texture)
                , mResourcesystem(resourcesystem)
            {
            }

            void apply(osg::Node& node) override
            {
                int index = 0;
                if (node.getUserValue("overrideFx", index))
                {
                    if (index == 1)
                        overrideTexture(mTexture, mResourcesystem, node);
                }
                traverse(node);
            }

            VFS::Path::NormalizedView mTexture;
            Resource::ResourceSystem* mResourcesystem;
        };

        struct ImageTextureOverrideVisitor : osg::NodeVisitor
        {
            ImageTextureOverrideVisitor(osg::ref_ptr<osg::Image> image, Resource::ResourceSystem* resourceSystem)
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
                , mImage(std::move(image))
                , mResourceSystem(resourceSystem)
            {
            }

            void apply(osg::Node& node) override
            {
                int index = 0;
                if (node.getUserValue("overrideFx", index) && index == 1)
                    overrideTexture(mImage, mResourceSystem, node);
                traverse(node);
            }

            osg::ref_ptr<osg::Image> mImage;
            Resource::ResourceSystem* mResourceSystem;
        };

        void setDiffuseTexture(osg::StateSet& stateSet, osg::Texture2D& texture)
        {
            constexpr osg::StateAttribute::GLModeValue modes
                = osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE;
            stateSet.setTextureAttributeAndModes(0, &texture, modes);
            stateSet.setTextureAttribute(0, new SceneUtil::TextureType("diffuseMap"), modes);
        }

        struct AllTextureOverrideVisitor : osg::NodeVisitor
        {
            explicit AllTextureOverrideVisitor(osg::ref_ptr<osg::Texture2D> texture)
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
                , mTexture(std::move(texture))
            {
            }

            void apply(osg::Node& node) override
            {
                if (node.getStateSet() != nullptr)
                {
                    osg::ref_ptr<osg::StateSet> stateSet
                        = new osg::StateSet(*node.getStateSet(), osg::CopyOp::SHALLOW_COPY);
                    setDiffuseTexture(*stateSet, *mTexture);
                    node.setStateSet(std::move(stateSet));
                    ++mCount;
                }
                traverse(node);
            }

            void apply(osg::Drawable& drawable) override
            {
                osg::ref_ptr<osg::StateSet> stateSet = drawable.getStateSet() != nullptr
                    ? new osg::StateSet(*drawable.getStateSet(), osg::CopyOp::SHALLOW_COPY)
                    : new osg::StateSet;
                setDiffuseTexture(*stateSet, *mTexture);
                drawable.setStateSet(std::move(stateSet));
                ++mCount;
                traverse(drawable);
            }

            osg::ref_ptr<osg::Texture2D> mTexture;
            std::size_t mCount = 0;
        };
    }

    void overrideFirstRootTexture(
        VFS::Path::NormalizedView texture, Resource::ResourceSystem* resourceSystem, osg::Node& node)
    {
        TextureOverrideVisitor overrideVisitor(texture, resourceSystem);
        node.accept(overrideVisitor);
    }

    void overrideFirstRootTexture(
        osg::ref_ptr<osg::Image> image, Resource::ResourceSystem* resourceSystem, osg::Node& node)
    {
        ImageTextureOverrideVisitor overrideVisitor(std::move(image), resourceSystem);
        node.accept(overrideVisitor);
    }

    void overrideTexture(VFS::Path::NormalizedView texture, Resource::ResourceSystem* resourceSystem, osg::Node& node)
    {
        if (texture.empty())
            return;
        const VFS::Path::Normalized correctedTexture
            = Misc::ResourceHelpers::correctTexturePath(texture, *resourceSystem->getVFS());
        overrideTexture(resourceSystem->getImageManager()->getImage(correctedTexture), resourceSystem, node);
    }

    void overrideTexture(osg::ref_ptr<osg::Image> image, Resource::ResourceSystem* resourceSystem, osg::Node& node)
    {
        if (image == nullptr)
            return;
        // Not sure if wrap settings should be pulled from the overridden texture?
        osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D(std::move(image));
        tex->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        tex->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        resourceSystem->getSceneManager()->applyFilterSettings(tex);

        osg::ref_ptr<osg::StateSet> stateset;
        if (const osg::StateSet* const src = node.getStateSet())
            stateset = new osg::StateSet(*src, osg::CopyOp::SHALLOW_COPY);
        else
            stateset = new osg::StateSet;

        setDiffuseTexture(*stateset, *tex);

        node.setStateSet(stateset);
    }

    std::size_t overrideAllTextures(
        VFS::Path::NormalizedView texture, Resource::ResourceSystem* resourceSystem, osg::Node& node)
    {
        if (texture.empty())
            return 0;
        const VFS::Path::Normalized correctedTexture
            = Misc::ResourceHelpers::correctTexturePath(texture, *resourceSystem->getVFS());
        return overrideAllTextures(
            resourceSystem->getImageManager()->getImage(correctedTexture), resourceSystem, node);
    }

    std::size_t overrideAllTextures(
        osg::ref_ptr<osg::Image> image, Resource::ResourceSystem* resourceSystem, osg::Node& node)
    {
        if (image == nullptr)
            return 0;
        osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D(std::move(image));
        texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        resourceSystem->getSceneManager()->applyFilterSettings(texture);
        AllTextureOverrideVisitor visitor(std::move(texture));
        node.accept(visitor);
        return visitor.mCount;
    }

    bool shouldAddMSAAIntermediateTarget()
    {
        return Settings::shaders().mAntialiasAlphaTest && Settings::video().mAntialiasing > 1;
    }
}
