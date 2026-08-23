#include "esm4npcanimation.hpp"

#include <array>
#include <cmath>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <osg/Group>
#include <osg/Image>
#include <osg/Material>
#include <osg/StateSet>
#include <components/esm4/loadarma.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadeyes.hpp>
#include <components/esm4/loadhair.hpp>
#include <components/esm4/loadhdpt.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrace.hpp>
#include <components/esm4/facegen.hpp>

#include <components/misc/resourcehelpers.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/nifosg/matrixtransform.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/imagemanager.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/attach.hpp>
#include <components/sceneutil/riggeometry.hpp>
#include <components/sceneutil/skeleton.hpp>
#include <components/vfs/manager.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwclass/esm4npc.hpp"
#include "../mwworld/esmstore.hpp"

#include "util.hpp"

namespace MWRender
{
    ESM4NpcAnimation::ESM4NpcAnimation(
        const MWWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem)
        : Animation(ptr, std::move(parentNode), resourceSystem)
    {
        const std::string skeleton = mPtr.getClass().getCorrectedModel(mPtr);
        setObjectRoot(skeleton, true, true, false);
        // A saved XRGD pose is already the final animation state of a corpse.
        // Do not let the generic non-actor controller start an idle KF whose
        // update callbacks would overwrite that pose during scene traversal.
        if (mPtr.getCellRef().getRagdollPose() == nullptr)
            addAnimDirectory(VFS::Path::Normalized(skeleton).parent());
        updateParts();
        const std::size_t ragdollBones = applyRagdollPose();
        const ESM4::Race* race = MWClass::ESM4Npc::getRace(mPtr);
        Log(Debug::Info) << "M11 actor assembled: ref=" << mPtr.getCellRef().getRefId() << " race="
                         << (race ? race->mEditorId : std::string("<missing>")) << " sex="
                         << (MWClass::ESM4Npc::isFemale(mPtr) ? "female" : "male") << " parts=" << mParts.size()
                         << " face_morph_meshes=" << mFaceMorphs.size()
                         << " animation_groups=" << mSupportedAnimations.size() << " ragdoll_bones=" << ragdollBones;
    }

    void ESM4NpcAnimation::updateParts()
    {
        if (mObjectRoot == nullptr)
            return;
        const ESM4::Npc* traits = MWClass::ESM4Npc::getTraitsRecord(mPtr);
        if (traits == nullptr)
            return;
        if (traits->mIsTES4)
            updatePartsTES4(*traits);
        else if (traits->mIsFONV)
        {
            // Not implemented yet
        }
        else
        {
            // There is no easy way to distinguish TES5 and FO3.
            // In case of FO3 the function shouldn't crash the game and will
            // only lead to the NPC not being rendered.
            updatePartsTES5(*traits);
        }
    }

    osg::ref_ptr<osg::Node> ESM4NpcAnimation::insertPart(
        std::string_view model, std::string_view attachBone, std::string_view texture,
        bool correctHeadPartOrientation)
    {
        if (model.empty())
            return {};

        Resource::SceneManager* sceneManager = mResourceSystem->getSceneManager();
        const VFS::Path::Normalized path
            = Misc::ResourceHelpers::correctMeshPath(VFS::Path::Normalized(model));
        try
        {
            osg::ref_ptr<const osg::Node> node = sceneManager->getTemplate(path);
            osg::Group* attachment = mObjectRoot.get();
            osg::Quat attachmentAttitude;
            const osg::Quat* attitude = nullptr;
            if (dynamic_cast<const SceneUtil::Skeleton*>(node.get()) == nullptr && !attachBone.empty())
            {
                const NodeMap& nodes = getNodeMap();
                const auto found = nodes.find(Misc::StringUtils::lowerCase(attachBone));
                if (found == nodes.end() || found->second->asGroup() == nullptr)
                {
                    Log(Debug::Warning) << "Unable to attach ESM4 actor part " << path << ": bone '" << attachBone
                                        << "' is missing";
                    return {};
                }
                attachment = found->second->asGroup();
                if (correctHeadPartOrientation && Misc::StringUtils::ciEqual(attachBone, "Bip01 Head"))
                {
                    const osg::MatrixList matrices = attachment->getWorldMatrices(mObjectRoot);
                    if (!matrices.empty())
                    {
                        attachmentAttitude = getTes4HeadPartCorrection(matrices.front());
                        attitude = &attachmentAttitude;
                    }
                }
            }

            osg::ref_ptr<osg::Node> attached
                = SceneUtil::attach(std::move(node), mObjectRoot, {}, attachment, sceneManager, attitude);
            isolateTes4ActorGeometry(*attached);
            if (!texture.empty())
            {
                const std::size_t overridden
                    = overrideAllTextures(VFS::Path::Normalized(texture), mResourceSystem, *attached);
                if (overridden == 0)
                    Log(Debug::Warning) << "Unable to override ESM4 actor part texture " << texture
                                        << " on " << path;
            }
            mParts.emplace_back(std::make_unique<PartHolder>(std::move(attached)));
            return mParts.back()->getNode();
        }
        catch (const std::exception& e)
        {
            Log(Debug::Warning) << "Unable to attach ESM4 actor part " << path << ": " << e.what();
            return {};
        }
    }

    namespace
    {
        float faceCoefficient(
            const std::vector<float>& raceValues, const std::vector<float>& npcValues, std::size_t index)
        {
            const float race = index < raceValues.size() ? raceValues[index] : 0.f;
            const float npc = index < npcValues.size() ? npcValues[index] : 0.f;
            return race + npc;
        }

        std::string_view defaultTes4BodyMesh(std::size_t part, bool female)
        {
            switch (part)
            {
                case ESM4::Race::UpperBody:
                    return female ? "characters/_male/femaleupperbody.nif" : "characters/_male/upperbody.nif";
                case ESM4::Race::LowerBody:
                    return female ? "characters/_male/femalelowerbody.nif" : "characters/_male/lowerbody.nif";
                case ESM4::Race::Hands:
                    return "characters/_male/hand.nif";
                case ESM4::Race::Feet:
                    return "characters/_male/foot.nif";
                default:
                    return {};
            }
        }

        osg::ref_ptr<osg::Vec3Array> cloneVertices(const osg::Geometry& geometry)
        {
            const auto* source = dynamic_cast<const osg::Vec3Array*>(geometry.getVertexArray());
            if (source == nullptr)
                return {};
            return static_cast<osg::Vec3Array*>(source->clone(osg::CopyOp::DEEP_COPY_ALL));
        }

        struct GeometryVisitor : osg::NodeVisitor
        {
            GeometryVisitor()
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
            {
            }

            void apply(osg::Drawable& drawable) override
            {
                if (auto* rig = dynamic_cast<SceneUtil::RigGeometry*>(&drawable))
                    mRigs.emplace_back(rig);
                else if (auto* geometry = dynamic_cast<osg::Geometry*>(&drawable))
                    mGeometry.emplace_back(geometry);
                traverse(drawable);
            }

            std::vector<osg::observer_ptr<osg::Geometry>> mGeometry;
            std::vector<osg::observer_ptr<SceneUtil::RigGeometry>> mRigs;
        };

        template <class T, T (*Loader)(std::istream&)>
        std::shared_ptr<const T> loadCachedFaceGen(const VFS::Manager& vfs, VFS::Path::NormalizedView path)
        {
            static std::mutex mutex;
            static std::unordered_map<std::string, std::shared_ptr<const T>> cache;
            std::lock_guard lock(mutex);
            if (const auto found = cache.find(std::string(path.value())); found != cache.end())
                return found->second;
            const Files::IStreamPtr stream = vfs.find(path);
            if (stream == nullptr)
                return {};
            auto result = std::make_shared<T>(Loader(*stream));
            cache.emplace(path.value(), result);
            return result;
        }
    }

    std::size_t isolateTes4ActorGeometry(osg::Node& node)
    {
        struct IsolationVisitor : osg::NodeVisitor
        {
            IsolationVisitor()
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
            {
            }

            void apply(osg::Drawable& drawable) override
            {
                auto* geometry = dynamic_cast<osg::Geometry*>(&drawable);
                if (geometry != nullptr)
                {
                    const osg::NodePath& path = getNodePath();
                    if (path.size() > 1)
                    {
                        if (osg::Group* parent = path[path.size() - 2]->asGroup())
                            mGeometry.emplace_back(parent, geometry);
                    }
                }
                traverse(drawable);
            }

            std::vector<std::pair<osg::observer_ptr<osg::Group>, osg::observer_ptr<osg::Geometry>>> mGeometry;
        };

        IsolationVisitor visitor;
        node.accept(visitor);
        std::unordered_map<osg::Geometry*, osg::ref_ptr<osg::Geometry>> clones;
        std::size_t isolated = 0;
        for (const auto& [observedParent, observedGeometry] : visitor.mGeometry)
        {
            osg::Group* parent = observedParent.get();
            osg::Geometry* geometry = observedGeometry.get();
            if (parent == nullptr || geometry == nullptr)
                continue;
            osg::ref_ptr<osg::Geometry>& clone = clones[geometry];
            if (clone == nullptr)
            {
                const osg::CopyOp copyOp(osg::CopyOp::DEEP_COPY_ARRAYS | osg::CopyOp::DEEP_COPY_PRIMITIVES
                    | osg::CopyOp::DEEP_COPY_STATESETS | osg::CopyOp::DEEP_COPY_USERDATA);
                clone = dynamic_cast<osg::Geometry*>(geometry->clone(copyOp));
            }
            if (clone != nullptr && parent->replaceChild(geometry, clone))
                ++isolated;
        }
        return isolated;
    }

    osg::Quat getTes4HeadPartCorrection(const osg::Matrixf& headBindMatrix)
    {
        // TES4 FaceGen head parts use head-origin coordinates whose axes are
        // aligned with the actor. Bip01 Head uses Biped's bone-aligned axes.
        // Cancel only the bind orientation; keeping the bone translation and
        // all later relative animation makes the parts follow the head.
        return headBindMatrix.getRotate().inverse();
    }

    bool shouldCorrectTes4HeadPartOrientation(std::size_t partIndex)
    {
        // The head, ears, and eyeballs use actor-aligned FaceGen coordinates,
        // like hair. The inner-mouth pieces are instead authored in Bip01
        // Head's bone-aligned coordinates and must retain that orientation.
        return partIndex < ESM4::Race::Mouth || partIndex > ESM4::Race::Tongue;
    }

    osg::ref_ptr<osg::Image> applyTes4FaceGenEgt(const osg::Image& base, const ESM4::FaceGenEgt& egt,
        const std::vector<float>& raceCoefficients, const std::vector<float>& npcCoefficients)
    {
        if (base.s() <= 0 || base.t() <= 0 || egt.mWidth == 0 || egt.mHeight == 0)
            return {};

        const std::size_t pixelCount = static_cast<std::size_t>(egt.mWidth) * egt.mHeight;
        std::vector<osg::Vec3f> deltas(pixelCount, osg::Vec3f());
        for (std::size_t mode = 0; mode < egt.mSymmetricTextures.size(); ++mode)
        {
            const ESM4::FaceGenTextureMode& textureMode = egt.mSymmetricTextures[mode];
            if (textureMode.mRed.size() < pixelCount || textureMode.mGreen.size() < pixelCount
                || textureMode.mBlue.size() < pixelCount)
                return {};
            // EGT RGB modes are stored in byte colour space. Convert the
            // scaled mode back to the normalized colour space used by OSG.
            const float weight
                = faceCoefficient(raceCoefficients, npcCoefficients, mode) * textureMode.mScale / 255.f;
            if (weight == 0.f)
                continue;
            for (std::size_t pixel = 0; pixel < pixelCount; ++pixel)
            {
                deltas[pixel].x() += textureMode.mRed[pixel] * weight;
                deltas[pixel].y() += textureMode.mGreen[pixel] * weight;
                deltas[pixel].z() += textureMode.mBlue[pixel] * weight;
            }
        }

        osg::ref_ptr<osg::Image> image = new osg::Image;
        image->allocateImage(base.s(), base.t(), 1, GL_RGBA, GL_UNSIGNED_BYTE);
        image->setOrigin(base.getOrigin());
        const bool flipRows = base.getOrigin() == osg::Image::BOTTOM_LEFT;
        for (std::uint32_t y = 0; y < static_cast<std::uint32_t>(base.t()); ++y)
        {
            for (std::uint32_t x = 0; x < static_cast<std::uint32_t>(base.s()); ++x)
            {
                const std::uint32_t egtX = x * egt.mWidth / static_cast<std::uint32_t>(base.s());
                const std::uint32_t unflippedY = y * egt.mHeight / static_cast<std::uint32_t>(base.t());
                const std::uint32_t egtY = flipRows ? egt.mHeight - 1 - unflippedY : unflippedY;
                const std::size_t pixel = static_cast<std::size_t>(egtY) * egt.mWidth + egtX;
                osg::Vec4f color = base.getColor(x, y);
                color.r() = std::clamp(color.r() + deltas[pixel].x(), 0.f, 1.f);
                color.g() = std::clamp(color.g() + deltas[pixel].y(), 0.f, 1.f);
                color.b() = std::clamp(color.b() + deltas[pixel].z(), 0.f, 1.f);
                image->setColor(color, x, y);
            }
        }
        return image;
    }

    bool applyTes4FaceGenEgm(osg::Geometry& geometry, const ESM4::FaceGenEgm& egm,
        const std::vector<float>& symmetric, const std::vector<float>& asymmetric)
    {
        osg::ref_ptr<osg::Vec3Array> vertices = cloneVertices(geometry);
        // EGM contains the TRI base vertices followed by auxiliary vertices
        // for statistical morphs such as the four eye-look targets. The NIF
        // contains the base mesh only, so its vertex array is a valid prefix.
        if (vertices == nullptr || vertices->size() > egm.mVertexCount)
            return false;
        const auto apply = [&](const std::vector<ESM4::FaceGenMorph>& morphs, const std::vector<float>& values) {
            for (std::size_t mode = 0; mode < morphs.size() && mode < values.size(); ++mode)
            {
                const ESM4::FaceGenMorph& morph = morphs[mode];
                if (morph.mVertices.size() < vertices->size())
                    return false;
                const float weight = values[mode] * morph.mScale;
                if (weight == 0.f)
                    continue;
                for (std::size_t vertex = 0; vertex < vertices->size(); ++vertex)
                {
                    const ESM4::FaceGenDelta& delta = morph.mVertices[vertex];
                    (*vertices)[vertex] += osg::Vec3f(delta.x, delta.y, delta.z) * weight;
                }
            }
            return true;
        };
        if (!apply(egm.mSymmetricMorphs, symmetric) || !apply(egm.mAsymmetricMorphs, asymmetric))
            return false;
        vertices->dirty();
        geometry.setVertexArray(vertices);
        geometry.dirtyBound();
        geometry.dirtyGLObjects();
        return true;
    }

    void applyTes4FaceGen(std::string_view model, osg::Node& node, const ESM4::Npc& traits,
        const ESM4::Race& race, bool isFemale, std::string_view texture, bool bodyTexture,
        Resource::ResourceSystem* resourceSystem, std::vector<Tes4FaceMorph>& morphs)
    {
        const VFS::Path::Normalized requested(model);
        const VFS::Path::Normalized source = requested.view().starts_with("meshes/")
            ? requested
            : Misc::ResourceHelpers::correctMeshPath(requested);
        VFS::Path::Normalized egmPath(source);
        egmPath.changeExtension(VFS::Path::ExtensionView("egm"));
        VFS::Path::Normalized triPath(source);
        triPath.changeExtension(VFS::Path::ExtensionView("tri"));
        VFS::Path::Normalized egtPath(source);
        egtPath.changeExtension(VFS::Path::ExtensionView("egt"));
        if (bodyTexture && !resourceSystem->getVFS()->exists(egtPath))
        {
            const bool upperBody = source.stem().find("upperbody") != std::string_view::npos;
            egtPath = VFS::Path::Normalized(upperBody
                    ? (isFemale ? "meshes/characters/_male/upperbodyhumanfemale.egt"
                                : "meshes/characters/_male/upperbodyhumanmale.egt")
                    : "meshes/characters/_male/body.egt");
        }

        std::vector<float> symmetric;
        const std::vector<float>& raceSymmetric
            = isFemale && !race.mSymShapeModeCoeffFemale.empty() ? race.mSymShapeModeCoeffFemale
                                                                  : race.mSymShapeModeCoefficients;
        const std::vector<float>& raceAsymmetric
            = isFemale && !race.mAsymShapeModeCoeffFemale.empty() ? race.mAsymShapeModeCoeffFemale
                                                                   : race.mAsymShapeModeCoefficients;
        const std::size_t symmetricCount = std::max(raceSymmetric.size(), traits.mSymShapeModeCoefficients.size());
        const std::size_t asymmetricCount = std::max(raceAsymmetric.size(), traits.mAsymShapeModeCoefficients.size());
        symmetric.resize(symmetricCount);
        std::vector<float> asymmetric(asymmetricCount);
        for (std::size_t i = 0; i < symmetric.size(); ++i)
            symmetric[i] = faceCoefficient(raceSymmetric, traits.mSymShapeModeCoefficients, i);
        for (std::size_t i = 0; i < asymmetric.size(); ++i)
            asymmetric[i] = faceCoefficient(raceAsymmetric, traits.mAsymShapeModeCoefficients, i);

        if (!texture.empty())
        {
            try
            {
                const std::shared_ptr<const ESM4::FaceGenEgt> cached
                    = loadCachedFaceGen<ESM4::FaceGenEgt, ESM4::loadFaceGenEgt>(
                        *resourceSystem->getVFS(), egtPath);
                if (cached != nullptr)
                {
                    const ESM4::FaceGenEgt& egt = *cached;
                    const std::vector<float>& raceTexture = isFemale && !race.mSymTextureModeCoeffFemale.empty()
                        ? race.mSymTextureModeCoeffFemale
                        : race.mSymTextureModeCoefficients;
                    const VFS::Path::Normalized corrected = Misc::ResourceHelpers::correctTexturePath(
                        VFS::Path::Normalized(texture), *resourceSystem->getVFS());
                    const osg::ref_ptr<osg::Image> base = resourceSystem->getImageManager()->getImage(corrected);
                    if (base != nullptr)
                    {
                        osg::ref_ptr<osg::Image> image = applyTes4FaceGenEgt(
                            *base, egt, raceTexture, traits.mSymTextureModeCoefficients);
                        if (image != nullptr)
                            overrideAllTextures(std::move(image), resourceSystem, node);
                    }
                }
            }
            catch (const std::exception& e)
            {
                Log(Debug::Warning) << "Unable to apply FaceGen EGT " << egtPath << ": " << e.what();
            }
        }

        GeometryVisitor visitor;
        node.accept(visitor);
        try
        {
            const std::shared_ptr<const ESM4::FaceGenEgm> egm
                = loadCachedFaceGen<ESM4::FaceGenEgm, ESM4::loadFaceGenEgm>(*resourceSystem->getVFS(), egmPath);
            if (egm != nullptr)
            {
                std::size_t incompatible = 0;
                for (const osg::observer_ptr<SceneUtil::RigGeometry>& rig : visitor.mRigs)
                {
                    if (rig == nullptr || rig->getSourceGeometry() == nullptr)
                        continue;
                    osg::ref_ptr<osg::Geometry> geometry
                        = new osg::Geometry(*rig->getSourceGeometry(), osg::CopyOp::SHALLOW_COPY);
                    if (applyTes4FaceGenEgm(*geometry, *egm, symmetric, asymmetric))
                        rig->setSourceGeometry(std::move(geometry));
                    else
                        ++incompatible;
                }
                for (const osg::observer_ptr<osg::Geometry>& geometry : visitor.mGeometry)
                    if (geometry != nullptr)
                    {
                        if (!applyTes4FaceGenEgm(*geometry, *egm, symmetric, asymmetric))
                            ++incompatible;
                    }
                if (incompatible != 0)
                    Log(Debug::Warning) << "Unable to apply FaceGen EGM " << egmPath << " to " << incompatible
                                        << " geometry object(s) with incompatible vertex layouts";
            }
        }
        catch (const std::exception& e)
        {
            Log(Debug::Warning) << "Unable to apply FaceGen EGM " << egmPath << ": " << e.what();
        }

        try
        {
            const std::shared_ptr<const ESM4::FaceGenTri> tri
                = loadCachedFaceGen<ESM4::FaceGenTri, ESM4::loadFaceGenTri>(*resourceSystem->getVFS(), triPath);
            if (tri == nullptr)
                return;
            const auto registerMorphs = [&](osg::Geometry* geometry) {
                if (geometry == nullptr)
                    return;
                osg::ref_ptr<osg::Vec3Array> base = cloneVertices(*geometry);
                if (base == nullptr || base->size() != tri->mVertices.size())
                    return;

                Tes4FaceMorph tracked;
                tracked.mGeometry = geometry;
                tracked.mBaseVertices = base;
                for (const ESM4::FaceGenMorph& target : tri->mMorphs)
                {
                    osg::ref_ptr<osg::Vec3Array> offsets = new osg::Vec3Array(target.mVertices.size());
                    for (std::size_t i = 0; i < target.mVertices.size(); ++i)
                    {
                        const ESM4::FaceGenDelta& delta = target.mVertices[i];
                        (*offsets)[i].set(
                            delta.x * target.mScale, delta.y * target.mScale, delta.z * target.mScale);
                    }
                    tracked.mOffsets.emplace_back(std::move(offsets));
                    tracked.mNames.push_back(Misc::StringUtils::lowerCase(target.mName));
                }
                morphs.emplace_back(std::move(tracked));
            };
            for (const osg::observer_ptr<SceneUtil::RigGeometry>& rig : visitor.mRigs)
            {
                if (rig != nullptr)
                {
                    const osg::ref_ptr<osg::Geometry> sourceGeometry = rig->getSourceGeometry();
                    registerMorphs(sourceGeometry.get());
                }
            }
            for (const osg::observer_ptr<osg::Geometry>& geometry : visitor.mGeometry)
                registerMorphs(geometry.get());
        }
        catch (const std::exception& e)
        {
            Log(Debug::Warning) << "Unable to apply FaceGen TRI " << triPath << ": " << e.what();
        }
    }

    float getTes4FaceMorphWeight(std::string_view name, std::string_view active, std::string_view next,
        float visemeBlend, float speechWeight, float blink)
    {
        if (name.find("blink") != std::string_view::npos || name.find("eyes closed") != std::string_view::npos)
            return blink;
        if (speechWeight <= 0.f)
            return 0.f;

        float weight = 0.f;
        // FaceGen phoneme channels are exact names (for example "i" and "r").
        // Substring matching also selects expression channels such as DispAnger,
        // Surprise and BrowInRight, which can combine into an invalid face mesh.
        if (name == active)
            weight += speechWeight * (1.f - visemeBlend);
        if (name == next)
            weight += speechWeight * visemeBlend;
        return weight;
    }

    void animateTes4FaceGen(std::vector<Tes4FaceMorph>& morphs, float faceTime, bool speaking,
        bool hasLipCompanion, float loudness, float speechTime)
    {
        static constexpr std::array<std::string_view, 16> phonemes
            = { "aah", "bigaah", "bmp", "chjsh", "dst", "eee", "eh", "fv", "i", "k", "n", "oh",
                  "oohq", "r", "th", "w" };
        const float visemePosition = std::max(0.f, speechTime) * 12.f;
        const std::size_t visemeIndex = static_cast<std::size_t>(visemePosition) % phonemes.size();
        const std::string_view active = phonemes[visemeIndex];
        const std::string_view next = phonemes[(visemeIndex + 1) % phonemes.size()];
        const float visemeBlend = visemePosition - std::floor(visemePosition);
        const float speechWeight = speaking && hasLipCompanion ? std::clamp(loudness * 2.f, 0.f, 1.f) : 0.f;
        const float blinkPhase = std::fmod(faceTime, 4.3f);
        const float blink = blinkPhase < 0.15f ? std::sin(blinkPhase / 0.15f * 3.14159265f) : 0.f;
        for (Tes4FaceMorph& tracked : morphs)
        {
            osg::Geometry* geometry = tracked.mGeometry.get();
            osg::Vec3Array* vertices = geometry != nullptr
                ? dynamic_cast<osg::Vec3Array*>(geometry->getVertexArray())
                : nullptr;
            if (vertices == nullptr || tracked.mBaseVertices == nullptr
                || vertices->size() != tracked.mBaseVertices->size())
                continue;
            *vertices = *tracked.mBaseVertices;
            for (std::size_t i = 0; i < tracked.mNames.size() && i < tracked.mOffsets.size(); ++i)
            {
                const std::string& name = tracked.mNames[i];
                const float weight
                    = getTes4FaceMorphWeight(name, active, next, visemeBlend, speechWeight, blink);
                if (weight == 0.f)
                    continue;
                const osg::Vec3Array& offsets = *tracked.mOffsets[i];
                for (std::size_t vertex = 0; vertex < vertices->size(); ++vertex)
                    (*vertices)[vertex] += offsets[vertex] * weight;
            }
            vertices->dirty();
            geometry->dirtyBound();
            geometry->dirtyGLObjects();
        }
    }

    osg::Vec3f ESM4NpcAnimation::runAnimation(float timepassed)
    {
        const osg::Vec3f movement = Animation::runAnimation(timepassed);
        mFaceAnimationTime += timepassed;
        MWBase::SoundManager* soundManager = MWBase::Environment::get().getSoundManager();
        const float loudness = soundManager->getSaySoundLoudness(mPtr);
        const bool speaking = soundManager->sayActive(mPtr);
        const VFS::Path::Normalized voice = soundManager->getSaySoundFile(mPtr);
        if (voice != mVoiceFile)
        {
            mVoiceFile = voice;
            mHasLipCompanion = false;
            if (!voice.empty())
            {
                VFS::Path::Normalized lip(voice);
                lip.changeExtension(VFS::Path::ExtensionView("lip"));
                if (const Files::IStreamPtr stream = mResourceSystem->getVFS()->find(lip))
                {
                    try
                    {
                        const ESM4::FaceGenLip header = ESM4::loadFaceGenLip(*stream);
                        mHasLipCompanion = true;
                        Log(Debug::Info) << "M11 lip companion: ref=" << mPtr.getCellRef().getRefId()
                                         << " voice=" << voice << " lip=" << lip
                                         << " duration_ticks=" << header.mDurationTicks
                                         << " curves=" << header.mCurveCount
                                         << " facefx_bytes=" << header.mPayload.size();
                    }
                    catch (const std::exception& e)
                    {
                        Log(Debug::Warning) << "Unable to validate FaceFX lip companion " << lip << ": " << e.what();
                    }
                }
            }
        }
        const float speechTime = speaking ? soundManager->getSaySoundOffset(mPtr) : mFaceAnimationTime;
        animateTes4FaceGen(
            mFaceMorphs, mFaceAnimationTime, speaking, mHasLipCompanion, loudness, speechTime);
        applyRagdollPose();
        return movement;
    }

    namespace
    {
        template <class NodeMap>
        std::size_t applyTes4Ragdoll(const ESM4::RagdollPose* pose, const NodeMap& nodes, osg::Group* objectRoot)
        {
            if (pose == nullptr || objectRoot == nullptr)
                return 0;
            std::size_t applied = 0;
            const auto applyTransform = [&](osg::MatrixTransform* node, const std::array<float, 3>& position,
                                            const std::array<float, 3>& angles) {
                if (node == nullptr)
                    return false;
                // XRGD uses the same Euler convention as TES4 DATA: inverse
                // axes and Z/Y/X composition when converted to OSG space.
                const osg::Quat rotation = osg::Quat(angles[2], osg::Vec3f(0, 0, -1))
                    * osg::Quat(angles[1], osg::Vec3f(0, -1, 0))
                    * osg::Quat(angles[0], osg::Vec3f(-1, 0, 0));
                if (auto* nifTransform = dynamic_cast<NifOsg::MatrixTransform*>(node))
                {
                    nifTransform->setRotation(rotation);
                    nifTransform->setTranslation(osg::Vec3f(position[0], position[1], position[2]));
                }
                else
                    node->setMatrix(osg::Matrixf::rotate(rotation)
                        * osg::Matrixf::translate(position[0], position[1], position[2]));
                return true;
            };
            if (const auto root = nodes.find("bip01"); root != nodes.end() && root->second != nullptr)
            {
                if (applyTransform(root->second, pose->mRootPosition, pose->mRootRotation))
                    ++applied;
            }
            for (const ESM4::RagdollTransform& transform : pose->mBones)
            {
                const std::string_view boneName = ESM4::getTes4RagdollBoneName(transform.mBone);
                if (boneName.empty())
                    continue;
                const auto found = nodes.find(Misc::StringUtils::lowerCase(boneName));
                if (found == nodes.end() || found->second == nullptr)
                    continue;
                // XRGD stores the saved local pose for each Havok-backed biped
                // node. Intermediary NIF-only nodes (clavicles and neck links)
                // keep their bind transforms while the mapped child receives
                // its saved local translation and articulation.
                if (applyTransform(found->second, transform.mPosition, transform.mRotation))
                    ++applied;
            }
            return applied;
        }
    }

    std::size_t ESM4NpcAnimation::applyRagdollPose()
    {
        return applyTes4Ragdoll(mPtr.getCellRef().getRagdollPose(), getNodeMap(), mObjectRoot);
    }

    template <class Record>
    static std::string_view chooseTes4EquipmentModel(const Record* rec, bool isFemale)
    {
        if (isFemale && !rec->mModelFemale.empty())
            return rec->mModelFemale.getOriginal();
        if (!rec->mModelMale.empty())
            return rec->mModelMale.getOriginal();
        if (!rec->mModelFemale.empty())
            return rec->mModelFemale.getOriginal();
        return rec->mModel.getOriginal();
    }

    void ESM4NpcAnimation::updatePartsTES4(const ESM4::Npc& traits)
    {
        const ESM4::Race* race = MWClass::ESM4Npc::getRace(mPtr);
        if (race == nullptr)
            return;
        bool isFemale = MWClass::ESM4Npc::isFemale(mPtr);

        struct Equipment
        {
            std::string_view mModel;
            std::uint32_t mSlots = 0;
        };
        std::vector<Equipment> equipment;
        for (const ESM4::Armor* armor : MWClass::ESM4Npc::getEquippedArmor(mPtr))
            equipment.push_back({ chooseTes4EquipmentModel(armor, isFemale), armor->mArmorFlags & 0xffffu });
        for (const ESM4::Clothing* clothing : MWClass::ESM4Npc::getEquippedClothing(mPtr))
            equipment.push_back({ chooseTes4EquipmentModel(clothing, isFemale), clothing->mClothingFlags & 0xffffu });

        std::uint32_t covered = 0;
        std::set<std::string, std::less<>> attachedModels;
        for (const Equipment& item : equipment)
        {
            // Inventory records are the only equipped-state source before M13.
            // Resolve mutually exclusive biped slots deterministically and never
            // attach the same mesh twice through inventory plus default outfit.
            if (item.mModel.empty() || (item.mSlots != 0 && (item.mSlots & ~covered) == 0))
                continue;
            const VFS::Path::Normalized normalized(item.mModel);
            if (!attachedModels.emplace(normalized.value()).second)
                continue;
            insertPart(item.mModel, (item.mSlots & (ESM4::Armor::TES4_Head | ESM4::Armor::TES4_Hair))
                    ? "Bip01 Head"
                    : std::string_view{});
            covered |= item.mSlots;
        }

        static constexpr std::array<std::uint32_t, ESM4::Race::NumBodyParts> bodySlots = {
            ESM4::Armor::TES4_UpperBody, ESM4::Armor::TES4_LowerBody, ESM4::Armor::TES4_Hands,
            ESM4::Armor::TES4_Feet, ESM4::Armor::TES4_Tail
        };
        const auto& bodyParts = isFemale ? race->mBodyPartsFemale : race->mBodyPartsMale;
        for (std::size_t i = 0; i < bodyParts.size() && i < bodySlots.size(); ++i)
            if ((covered & bodySlots[i]) == 0)
            {
                const std::string_view model
                    = bodyParts[i].mesh.empty() ? defaultTes4BodyMesh(i, isFemale) : bodyParts[i].mesh;
                if (osg::ref_ptr<osg::Node> node = insertPart(model, {}, bodyParts[i].texture))
                    applyTes4FaceGen(model, *node, traits, *race, isFemale, bodyParts[i].texture, true,
                        mResourceSystem, mFaceMorphs);
            }

        const auto& headParts = isFemale && !race->mHeadPartsFemale.empty() ? race->mHeadPartsFemale : race->mHeadParts;
        if ((covered & ESM4::Armor::TES4_Head) == 0)
        {
            for (std::size_t i = 0; i < headParts.size(); ++i)
            {
                if ((i == ESM4::Race::EarMale && isFemale) || (i == ESM4::Race::EarFemale && !isFemale))
                    continue;
                const ESM4::Race::BodyPart& part = headParts[i];
                std::string_view texture = part.texture;
                if ((i == ESM4::Race::EyeLeft || i == ESM4::Race::EyeRight) && !traits.mEyes.isZeroOrUnset())
                {
                    const MWWorld::ESMStore* store = MWBase::Environment::get().getESMStore();
                    if (const ESM4::Eyes* eyes = store->get<ESM4::Eyes>().search(traits.mEyes))
                        texture = eyes->mIcon;
                }
                if (osg::ref_ptr<osg::Node> node = insertPart(part.mesh, "Bip01 Head", texture,
                        shouldCorrectTes4HeadPartOrientation(i)))
                    applyTes4FaceGen(
                        part.mesh, *node, traits, *race, isFemale, texture, false, mResourceSystem, mFaceMorphs);
            }
        }

        const ESM::FormId hairId
            = traits.mHair.isZeroOrUnset() ? race->mDefaultHair[isFemale ? 1 : 0] : traits.mHair;
        if ((covered & (ESM4::Armor::TES4_Head | ESM4::Armor::TES4_Hair)) == 0 && !hairId.isZeroOrUnset())
        {
            const MWWorld::ESMStore* store = MWBase::Environment::get().getESMStore();
            if (const ESM4::Hair* hair = store->get<ESM4::Hair>().search(hairId))
            {
                if (osg::ref_ptr<osg::Node> node
                    = insertPart(hair->mModel.getOriginal(), "Bip01 Head", hair->mIcon, true))
                {
                    applyTes4FaceGen(hair->mModel.getOriginal(), *node, traits, *race, isFemale, {}, false,
                        mResourceSystem, mFaceMorphs);
                    osg::ref_ptr<osg::Material> material = new osg::Material;
                    const float red = traits.mHairColour.red / 255.f;
                    const float green = traits.mHairColour.green / 255.f;
                    const float blue = traits.mHairColour.blue / 255.f;
                    material->setColorMode(osg::Material::OFF);
                    material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4f(red, green, blue, 1.f));
                    material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4f(red, green, blue, 1.f));
                    node->getOrCreateStateSet()->setAttributeAndModes(
                        material, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
                }
            }
            else
                Log(Debug::Error) << "Hair not found: " << ESM::RefId(hairId);
        }
    }

    void ESM4NpcAnimation::insertHeadParts(
        const std::vector<ESM::FormId>& partIds, std::set<uint32_t>& usedHeadPartTypes)
    {
        const MWWorld::ESMStore* store = MWBase::Environment::get().getESMStore();
        for (ESM::FormId partId : partIds)
        {
            if (partId.isZeroOrUnset())
                continue;
            const ESM4::HeadPart* part = store->get<ESM4::HeadPart>().search(partId);
            if (!part)
            {
                Log(Debug::Error) << "Head part not found: " << ESM::RefId(partId);
                continue;
            }
            if (usedHeadPartTypes.emplace(part->mType).second)
                insertPart(part->mModel.getOriginal());
        }
    }

    void ESM4NpcAnimation::updatePartsTES5(const ESM4::Npc& traits)
    {
        const MWWorld::ESMStore* store = MWBase::Environment::get().getESMStore();

        const ESM4::Race* race = MWClass::ESM4Npc::getRace(mPtr);
        bool isFemale = MWClass::ESM4Npc::isFemale(mPtr);

        std::vector<const ESM4::ArmorAddon*> armorAddons;

        auto findArmorAddons = [&](const ESM4::Armor* armor) {
            for (ESM::FormId armaId : armor->mAddOns)
            {
                if (armaId.isZeroOrUnset())
                    continue;
                const ESM4::ArmorAddon* arma = store->get<ESM4::ArmorAddon>().search(armaId);
                if (!arma)
                {
                    Log(Debug::Error) << "ArmorAddon not found: " << ESM::RefId(armaId);
                    continue;
                }
                bool compatibleRace = arma->mRacePrimary == traits.mRace;
                for (auto r : arma->mRaces)
                    if (r == traits.mRace)
                        compatibleRace = true;
                if (compatibleRace)
                    armorAddons.push_back(arma);
            }
        };

        for (const ESM4::Armor* armor : MWClass::ESM4Npc::getEquippedArmor(mPtr))
            findArmorAddons(armor);
        if (!traits.mWornArmor.isZeroOrUnset())
        {
            if (const ESM4::Armor* armor = store->get<ESM4::Armor>().search(traits.mWornArmor))
                findArmorAddons(armor);
            else
                Log(Debug::Error) << "Worn armor not found: " << ESM::RefId(traits.mWornArmor);
        }
        if (!race->mSkin.isZeroOrUnset())
        {
            if (const ESM4::Armor* armor = store->get<ESM4::Armor>().search(race->mSkin))
                findArmorAddons(armor);
            else
                Log(Debug::Error) << "Skin not found: " << ESM::RefId(race->mSkin);
        }

        if (isFemale)
            std::sort(armorAddons.begin(), armorAddons.end(),
                [](auto x, auto y) { return x->mFemalePriority > y->mFemalePriority; });
        else
            std::sort(armorAddons.begin(), armorAddons.end(),
                [](auto x, auto y) { return x->mMalePriority > y->mMalePriority; });

        uint32_t usedParts = 0;
        for (const ESM4::ArmorAddon* arma : armorAddons)
        {
            const uint32_t covers = arma->mBodyTemplate.bodyPart;
            // if body is already covered, skip to avoid clipping
            if (covers & usedParts & ESM4::Armor::TES5_Body)
                continue;
            // if covers at least something that wasn't covered before - add model
            if (covers & ~usedParts)
            {
                usedParts |= covers;
                insertPart(isFemale ? arma->mModelFemale.getOriginal() : arma->mModelMale.getOriginal());
            }
        }

        std::set<uint32_t> usedHeadPartTypes;
        if (usedParts & ESM4::Armor::TES5_Hair)
            usedHeadPartTypes.insert(ESM4::HeadPart::Type_Hair);
        insertHeadParts(traits.mHeadParts, usedHeadPartTypes);
        insertHeadParts(isFemale ? race->mHeadPartIdsFemale : race->mHeadPartIdsMale, usedHeadPartTypes);
    }

    ESM4CreatureAnimation::ESM4CreatureAnimation(const MWWorld::Ptr& ptr, std::string_view model,
        osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem)
        : Animation(ptr, std::move(parentNode), resourceSystem)
    {
        const VFS::Path::Normalized source(model);
        const VFS::Path::Normalized path = source.view().starts_with("meshes/")
            ? source
            : Misc::ResourceHelpers::correctMeshPath(source);
        setObjectRoot(path.value(), true, false, true);
        if (mPtr.getCellRef().getRagdollPose() == nullptr)
            addAnimDirectory(path.parent());
        const ESM4::Creature* creature = mPtr.get<ESM4::Creature>()->mBase;
        Resource::SceneManager* sceneManager = mResourceSystem->getSceneManager();
        for (const std::string& partModel : creature->mNif)
        {
            try
            {
                const VFS::Path::Normalized partPath = path.parent() / VFS::Path::Normalized(partModel);
                osg::ref_ptr<const osg::Node> node = sceneManager->getTemplate(partPath);
                osg::ref_ptr<osg::Node> attached
                    = SceneUtil::attach(std::move(node), mObjectRoot, {}, mObjectRoot, sceneManager);
                mParts.emplace_back(std::make_unique<PartHolder>(std::move(attached)));
            }
            catch (const std::exception& e)
            {
                Log(Debug::Warning) << "Unable to attach ESM4 creature part " << partModel << " below "
                                    << path.parent() << ": " << e.what();
            }
        }
        const std::size_t ragdollBones = applyRagdollPose();
        Log(Debug::Info) << "M11 creature assembled: ref=" << mPtr.getCellRef().getRefId() << " model=" << path
                         << " parts=" << mParts.size() << " animation_groups=" << mSupportedAnimations.size()
                         << " ragdoll_bones=" << ragdollBones;
    }

    std::size_t ESM4CreatureAnimation::applyRagdollPose()
    {
        return applyTes4Ragdoll(mPtr.getCellRef().getRagdollPose(), getNodeMap(), mObjectRoot);
    }

    osg::Vec3f ESM4CreatureAnimation::runAnimation(float timepassed)
    {
        const osg::Vec3f movement = Animation::runAnimation(timepassed);
        applyRagdollPose();
        return movement;
    }
}
