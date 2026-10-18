#include "actorragdollpose.hpp"

#include "skeleton.hpp"

#include <components/misc/osguservalues.hpp>
#include <components/nifbullet/ragdollbonepose.hpp>
#include <components/nif/nativetransform.hpp>
#include <components/nifosg/matrixtransform.hpp>

#include <osg/Group>
#include <osg/Transform>
#include <osg/observer_ptr>
#include <osg/ValueObject>

#include <optional>
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace SceneUtil
{
    namespace
    {
        void require(bool condition, const char* message)
        {
            if (!condition)
                throw std::invalid_argument(message);
        }

        Nif::NiTransform worldPose(const osg::Matrixf& matrix)
        {
            // Share the physical bridge's rigid/finite/affine admission gate.
            NifBullet::ragdollNativePoseFromBoneWorld(matrix);
            Nif::NiTransform result = Nif::NiTransform::getIdentity();
            result.mTranslation = matrix.getTrans();
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 3; ++column)
                    result.mRotation.mValues[row][column] = matrix(column, row);
            return result;
        }

        Nif::NiTransform nativePlacement(const Nif::NiTransform& pose)
        {
            // Validate rotation independently of its separate native scale.
            worldPose(pose.mRotation.toOsgMatrix());
            return NifBullet::composeRagdollBonePose(pose, Nif::NiTransform::getIdentity());
        }

        osg::Matrixf physicalWorldMatrix(const Nif::NiTransform& pose, float placementScale)
        {
            // Ordinary body sync consumes NiWorld rotation and position, not
            // its separate scale slot. Require authored physical bone scale
            // to remain neighboring-unit relative to the explicit placement.
            require(std::isfinite(pose.mScale) && pose.mScale > 0
                    && std::abs(double(pose.mScale) / placementScale - 1.0) <= 1e-4,
                "unadmitted physical bone world scale");
            auto matrix = pose.mRotation.toOsgMatrix();
            matrix.setTrans(pose.mTranslation);
            NifBullet::ragdollNativePoseFromBoneWorld(matrix);
            return matrix;
        }

        Nif::NiTransform localPose(const NifOsg::MatrixTransform& node)
        {
            require(std::isfinite(node.mScale) && node.mScale > 0, "invalid ragdoll renderer node scale");
            // Quaternion callbacks retain double precision in OSG's matrix.
            // Procedural rotate controllers deliberately leave the cached NIF
            // rotation unchanged. Capture their current local matrix using its
            // separate scale; traversal projections must not feed back here.
            osg::Matrixf rigid(node.getMatrix());
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 3; ++column)
                    rigid(row, column) = static_cast<float>(
                        node.getMatrix()(row, column) / double(node.mScale));
            auto result = worldPose(rigid);
            result.mScale = node.mScale;
            return result;
        }
    }

    std::optional<Nif::NiTransform> captureNativeNodeWorldPose(
        const osg::Group& root, std::string_view name, const Nif::NiTransform& placement)
    {
        require(!name.empty(), "empty native scene lookup name");
        std::vector<const osg::Node*> path, selected;
        const auto walk = [&](auto&& self, const osg::Node& node) -> void {
            require(std::find(path.begin(), path.end(), &node) == path.end(), "cyclic native scene lookup");
            path.push_back(&node);
            if (node.getName() == name)
            {
                require(selected.empty(), "ambiguous native scene lookup name");
                selected = path;
            }
            if (const auto* group = node.asGroup())
                for (unsigned i = 0; i < group->getNumChildren(); ++i)
                    self(self, *group->getChild(i));
            path.pop_back();
        };
        walk(walk, root);
        if (selected.empty()) return std::nullopt;
        require(dynamic_cast<const NifOsg::MatrixTransform*>(selected.back()),
            "native scene lookup target has no NIF transform");
        auto world = placement;
        // Even identity locals execute the original matrix product and stores.
        for (const auto* node : selected)
        {
            auto local = Nif::NiTransform::getIdentity();
            if (const auto* transform = dynamic_cast<const osg::Transform*>(node))
            {
                const auto* nif = dynamic_cast<const NifOsg::MatrixTransform*>(transform);
                require(nif && nif->getReferenceFrame() == osg::Transform::RELATIVE_RF,
                    "unsupported native scene lookup transform");
                local = nif->getControllerTransform();
            }
            else
                continue; // Renderer-only groups have no native transform.
            const auto composed = Nif::tryComposeNativeTransform(world, local, Nif::NativeTransformPrecision::Single);
            require(composed.has_value(), "unrepresentable native scene lookup world pose");
            world = *composed;
        }
        return world;
    }

    struct ActorRagdollLocalPose::State
    {
        struct Node
        {
            osg::Matrixd mMatrix;
            Nif::Matrix3 mRotationScale;
            float mScale;
            NifOsg::MatrixTransform::TraversalMatrix mTraversalMatrix;
        };
        std::shared_ptr<const char> mAuthority;
        std::vector<Node> mNodes;
        bool mNativeFrameProjection = false;
    };

    struct ActorRagdollPoseBinding::Impl
    {
        struct Node
        {
            osg::observer_ptr<NifOsg::MatrixTransform> mTransform;
            std::vector<osg::observer_ptr<osg::Node>> mPath;
            std::optional<std::size_t> mParent;
            std::uint32_t mNodeRecord;
        };
        struct Body
        {
            std::uint32_t mNodeRecord;
            std::size_t mNode;
            std::string mBone;
            std::uint32_t mBodyRecord = 0;
        };
        osg::observer_ptr<osg::Group> mRoot;
        osg::observer_ptr<osg::Group> mAssetRoot;
        std::string mSourceHash;
        std::shared_ptr<const char> mLocalAuthority = std::make_shared<const char>(0);
        std::vector<Node> mNodes;
        std::vector<Body> mBodies;
        std::optional<Body> mReferenceSource;
        RootSpace mRootSpace;
        bool mNativeFrameProjection = false;

        Impl(const NifBullet::ActorRagdollDefinition& definition, osg::Group& root, RootSpace rootSpace)
            : mRoot(&root), mSourceHash(definition.mSourceHash), mRootSpace(rootSpace)
        {
            require(rootSpace == RootSpace::ObjectLocal || rootSpace == RootSpace::NativeObjectLocal
                    || rootSpace == RootSpace::ReferenceWorld,
                "unknown ragdoll renderer root space");
            require(!definition.mBodies.empty() && !mSourceHash.empty(), "empty ragdoll renderer definition");
            std::string rootHash;
            if (root.getUserValue(Misc::OsgUserValues::sFileHash, rootHash))
                mAssetRoot = &root;
            else
            {
                // Animation::setObjectRoot may wrap an unskinned NIF group
                // in a Skeleton without copying the asset's user values.
                require(dynamic_cast<Skeleton*>(&root) && root.getNumChildren() == 1,
                    "unidentified ragdoll renderer root");
                mAssetRoot = root.getChild(0)->asGroup();
            }
            validateRoot();
            std::unordered_set<const osg::Node*> visited;
            std::unordered_map<std::uint32_t, std::size_t> records;
            const auto walk = [&](auto&& self, osg::Node& node,
                                  std::vector<osg::observer_ptr<osg::Node>> path,
                                  std::optional<std::size_t> parent) -> void {
                std::string assetHash;
                // Separately loaded equipment/parts have independent record indices.
                if (&node != &root && &node != mAssetRoot.get()
                    && node.getUserValue(Misc::OsgUserValues::sFileHash, assetHash))
                    return;
                require(visited.insert(&node).second, "cyclic or shared ragdoll renderer hierarchy");
                if (!path.empty())
                    require(node.getNumParents() == 1 && node.getParent(0) == path.back().get(),
                        "ambiguous ragdoll renderer parent");
                path.emplace_back(&node);
                if (auto* transform = dynamic_cast<NifOsg::MatrixTransform*>(&node))
                {
                    unsigned record;
                    require(node.getUserValue("recordIndex", record), "missing ragdoll renderer record identity");
                    const auto index = mNodes.size();
                    require(records.emplace(record, index).second, "duplicate ragdoll renderer record identity");
                    mNodes.push_back({transform, path, parent, record});
                    parent = index;
                }
                if (auto* group = node.asGroup())
                    for (unsigned i = 0; i < group->getNumChildren(); ++i)
                        self(self, *group->getChild(i), path, parent);
            };
            walk(walk, root, {}, {});
            std::unordered_set<std::uint32_t> bodyRecords, bodyNodes;
            for (const auto& body : definition.mBodies)
            {
                require(bodyRecords.insert(body.mRecord).second && bodyNodes.insert(body.mNodeRecord).second,
                    "duplicate ragdoll renderer body identity");
                const auto found = records.find(body.mNodeRecord);
                require(found != records.end()
                        && (!body.mBone.empty() || definition.mReferenceRoot == body.mNodeRecord)
                        && mNodes[found->second].mTransform->getName() == body.mBone,
                    "ragdoll renderer target identity mismatch");
                mBodies.push_back({body.mNodeRecord, found->second, body.mBone, body.mRecord});
            }
            if (const auto& source = definition.mReferenceCollisionSource)
            {
                const auto found = records.find(source->mNodeRecord);
                require(found != records.end() && !source->mBone.empty()
                        && mNodes[found->second].mTransform->getName() == source->mBone,
                    "ragdoll renderer relocation source identity mismatch");
                mReferenceSource = Body{source->mNodeRecord, found->second, source->mBone};
            }
            // Only physical targets and their ancestors participate. Other
            // authored helpers may use scale unsupported by rigid-body binding.
            std::vector<bool> needed(mNodes.size());
            for (const auto& body : mBodies)
            {
                std::optional<std::size_t> index = body.mNode;
                while (index && !needed[*index])
                {
                    needed[*index] = true;
                    index = mNodes[*index].mParent;
                }
            }
            if (mReferenceSource)
            {
                std::optional<std::size_t> index = mReferenceSource->mNode;
                while (index && !needed[*index])
                {
                    needed[*index] = true;
                    index = mNodes[*index].mParent;
                }
            }
            std::vector<std::size_t> remap(mNodes.size());
            std::vector<Node> selected;
            for (std::size_t i = 0; i < mNodes.size(); ++i)
            {
                if (!needed[i])
                    continue;
                auto entry = std::move(mNodes[i]);
                // Unrelated effects may use other transform types. Physical
                // targets still require a fully representable ancestor path.
                for (const auto& node : entry.mPath)
                    require(!dynamic_cast<osg::Transform*>(node.get())
                            || dynamic_cast<NifOsg::MatrixTransform*>(node.get()),
                        "unadmitted ragdoll renderer transform type");
                readLocal(*entry.mTransform);
                if (entry.mParent)
                    entry.mParent = remap[*entry.mParent];
                remap[i] = selected.size();
                selected.push_back(std::move(entry));
            }
            for (auto& body : mBodies)
                body.mNode = remap[body.mNode];
            if (mReferenceSource) mReferenceSource->mNode = remap[mReferenceSource->mNode];
            mNodes = std::move(selected);
            if (mRootSpace == RootSpace::ReferenceWorld)
            {
                require(definition.mReferenceRoot && !mNodes.empty()
                    && mNodes.front().mNodeRecord == *definition.mReferenceRoot && !mNodes.front().mParent,
                    "native reference root must be the physical hierarchy root");
                for (std::size_t i = 1; i < mNodes.size(); ++i)
                    require(mNodes[i].mParent.has_value(), "multiple native reference roots");
            }
        }

        Nif::NiTransform readLocal(const NifOsg::MatrixTransform& node) const
        {
            if (mRootSpace == RootSpace::ObjectLocal) return localPose(node);
            require(std::isfinite(node.mScale) && node.mScale > 0, "invalid native reference node scale");
            return node.getControllerTransform();
        }

        Nif::NiTransform compose(const Nif::NiTransform& parent, const Nif::NiTransform& local) const
        {
            if (mRootSpace == RootSpace::ObjectLocal) return NifBullet::composeRagdollBonePose(parent, local);
            const auto result = Nif::tryComposeNativeTransform(parent, local, Nif::NativeTransformPrecision::Single);
            require(result.has_value(), "invalid native reference composition");
            return *result;
        }

        void validateRoot() const
        {
            std::string hash;
            require(mRoot.valid() && mAssetRoot.valid()
                    && mAssetRoot->getUserValue(Misc::OsgUserValues::sFileHash, hash) && hash == mSourceHash,
                "expired or mismatched ragdoll renderer asset");
        }

        bool isValid() const noexcept
        {
            if (!mRoot.valid() || !mAssetRoot.valid()) return false;
            const auto* data = mAssetRoot->getUserDataContainer();
            if (!data) return false;
            const auto index = data->getUserObjectIndex(Misc::OsgUserValues::sFileHash);
            if (index >= data->getNumUserObjects()) return false;
            const auto* hash = dynamic_cast<const osg::StringValueObject*>(data->getUserObject(index));
            if (!hash || hash->getValue() != mSourceHash) return false;
            for (const auto& entry : mNodes)
            {
                if (!entry.mTransform.valid()) return false;
                unsigned record;
                if (!entry.mTransform->getUserValue("recordIndex", record) || record != entry.mNodeRecord)
                    return false;
                for (std::size_t i = 0; i < entry.mPath.size(); ++i)
                    if (!entry.mPath[i].valid() || (i && (entry.mPath[i]->getNumParents() != 1
                            || entry.mPath[i]->getParent(0) != entry.mPath[i - 1].get())))
                        return false;
            }
            for (const auto& body : mBodies)
                if (mNodes[body.mNode].mTransform->getName() != body.mBone) return false;
            if (mReferenceSource
                && mNodes[mReferenceSource->mNode].mTransform->getName() != mReferenceSource->mBone) return false;
            return true;
        }

        void validateHierarchy() const
        {
            validateRoot();
            for (const auto& entry : mNodes)
            {
                require(entry.mTransform.valid(), "expired ragdoll renderer bone");
                unsigned record;
                require(entry.mTransform->getUserValue("recordIndex", record) && record == entry.mNodeRecord,
                    "changed ragdoll renderer transform identity");
                for (std::size_t i = 0; i < entry.mPath.size(); ++i)
                {
                    require(entry.mPath[i].valid(), "expired ragdoll renderer path");
                    if (i)
                        require(entry.mPath[i]->getNumParents() == 1
                                && entry.mPath[i]->getParent(0) == entry.mPath[i - 1].get(),
                            "detached or ambiguous ragdoll renderer path");
                }
            }
            for (const auto& body : mBodies)
                require(mNodes[body.mNode].mTransform->getName() == body.mBone,
                    "changed ragdoll renderer bone identity");
            if (mReferenceSource)
                require(mNodes[mReferenceSource->mNode].mTransform->getName() == mReferenceSource->mBone,
                    "changed ragdoll renderer relocation source identity");
        }

        std::vector<Nif::NiTransform> locals() const
        {
            validateHierarchy();
            std::vector<Nif::NiTransform> result;
            result.reserve(mNodes.size());
            for (const auto& entry : mNodes)
                result.push_back(readLocal(*entry.mTransform));
            return result;
        }

        std::vector<Nif::NiTransform> worlds(const std::vector<Nif::NiTransform>& local,
            const Nif::NiTransform& objectWorld) const
        {
            std::vector<Nif::NiTransform> result;
            result.reserve(mNodes.size());
            for (std::size_t i = 0; i < mNodes.size(); ++i)
            {
                const auto& parent = mNodes[i].mParent ? result[*mNodes[i].mParent] : objectWorld;
                result.push_back(!mNodes[i].mParent && mRootSpace == RootSpace::ReferenceWorld
                    ? local[i] : compose(parent, local[i]));
            }
            return result;
        }
    };

    ActorRagdollPoseBinding::ActorRagdollPoseBinding(const NifBullet::ActorRagdollDefinition& definition, osg::Group& root,
        RootSpace rootSpace)
        : mImpl(std::make_unique<Impl>(definition, root, rootSpace))
    {
    }

    ActorRagdollPoseBinding::~ActorRagdollPoseBinding() = default;

    std::vector<std::uint32_t> ActorRagdollPoseBinding::bodyNodeOrder() const
    {
        mImpl->validateHierarchy();
        std::vector<std::pair<std::vector<unsigned>, std::uint32_t>> ordered;
        ordered.reserve(mImpl->mBodies.size());
        for (const auto& body : mImpl->mBodies)
        {
            const auto& path = mImpl->mNodes[body.mNode].mPath;
            std::vector<unsigned> slots;
            for (std::size_t i = 1; i < path.size(); ++i)
            {
                const auto* parent = path[i - 1]->asGroup();
                require(parent != nullptr, "invalid native controller traversal path");
                const auto slot = parent->getChildIndex(path[i].get());
                require(slot < parent->getNumChildren(), "detached native controller traversal child");
                slots.push_back(slot);
            }
            ordered.emplace_back(std::move(slots), body.mNodeRecord);
        }
        std::sort(ordered.begin(), ordered.end());
        std::vector<std::uint32_t> result;
        result.reserve(ordered.size());
        for (const auto& [slots, record] : ordered) result.push_back(record);
        return result;
    }

    std::vector<NifBullet::RagdollBoneWorldPose> ActorRagdollPoseBinding::applyNativeFrame(
        std::span<const NifBullet::RagdollNativeBlendPublication> publications,
        const Nif::NiTransform& objectWorld)
    {
        require(mImpl->mRootSpace == RootSpace::NativeObjectLocal, "native actor frame requires native object space");
        const auto placement = nativePlacement(objectWorld);
        auto local = mImpl->locals();
        std::unordered_map<std::size_t, const NifBullet::RagdollNativeBlendPublication*> selected;
        for (const auto& publication : publications)
        {
            const auto body = std::find_if(mImpl->mBodies.begin(), mImpl->mBodies.end(),
                [&](const auto& value) { return value.mBodyRecord == publication.mRecord; });
            require(body != mImpl->mBodies.end(), "unknown native renderer body publication");
            require(selected.emplace(body->mNode, &publication).second, "duplicate native renderer body publication");
            require(!(publication.mCollisionFlags & 0x40), "unsettled native renderer first synchronization");
        }
        std::vector<Nif::NiTransform> world;
        std::vector<bool> writeLocal(local.size(), false);
        std::vector<NifOsg::MatrixTransform::TraversalMatrix> projections;
        world.reserve(local.size()); projections.reserve(local.size());
        for (std::size_t i = 0; i < local.size(); ++i)
        {
            const auto& entry = mImpl->mNodes[i];
            const auto& parent = entry.mParent ? world[*entry.mParent] : placement;
            const auto found = selected.find(i);
            const auto* publication = found == selected.end() ? nullptr : found->second;
            const bool physical = publication && publication->mSceneTarget && (publication->mCollisionFlags & 1);
            // Inactive publications deliberately do not inspect their target.
            const auto desired = physical ? worldPose(*publication->mSceneTarget) : Nif::NiTransform::getIdentity();
            const auto frame = NifBullet::ragdollBoneFrame(local[i], desired, &parent,
                physical ? publication->mCollisionFlags : 0, Nif::NativeTransformPrecision::Single);
            local[i] = frame.mLocal; world.push_back(frame.mWorld);
            writeLocal[i] = physical && (publication->mCollisionFlags & 8);

            osg::Matrixd drawingParent;
            if (entry.mParent)
                drawingParent = world[*entry.mParent].toMatrix();
            else
            {
                // Native placement and rendering scaffolding are separate.
                // Only drawing projection uses the actual OSG ancestor path.
                osg::NodePath path;
                std::unordered_set<const osg::Node*> seen{entry.mTransform.get()};
                const osg::Node* node = entry.mTransform.get();
                while (node->getNumParents())
                {
                    require(node->getNumParents() == 1, "ambiguous native actor drawing placement");
                    node = node->getParent(0);
                    require(seen.insert(node).second, "cyclic native actor drawing placement");
                    path.insert(path.begin(), const_cast<osg::Node*>(node));
                }
                drawingParent = osg::computeLocalToWorld(path);
            }
            osg::Matrixd inverse;
            require(inverse.invert(drawingParent), "singular native actor drawing placement");
            projections.emplace_back(osg::Matrixd(frame.mWorld.toMatrix()) * inverse);
        }
        std::vector<NifBullet::RagdollBoneWorldPose> result;
        result.reserve(mImpl->mBodies.size());
        for (const auto& body : mImpl->mBodies)
            result.push_back({body.mNodeRecord, physicalWorldMatrix(world[body.mNode], placement.mScale)});
        mImpl->validateHierarchy();
        for (std::size_t i = 0; i < local.size(); ++i)
        {
            auto& node = *mImpl->mNodes[i].mTransform;
            if (writeLocal[i])
            {
                node.setRotation(local[i].mRotation);
                node.setTranslation(local[i].mTranslation);
            }
            node.setTraversalMatrix(projections[i]);
        }
        mImpl->mNativeFrameProjection = true;
        if (auto* skeleton = dynamic_cast<Skeleton*>(mImpl->mRoot.get())) skeleton->invalidateBoneMatrices();
        return result;
    }

    bool ActorRagdollPoseBinding::isValid() const noexcept
    {
        return mImpl->isValid();
    }

    bool ActorRagdollPoseBinding::matchesLocalPose(const ActorRagdollLocalPose& pose) const noexcept
    {
        const auto* state = pose.mState.get();
        if (!state || state->mAuthority != mImpl->mLocalAuthority || !isValid()
            || state->mNodes.size() != mImpl->mNodes.size()
            || state->mNativeFrameProjection != mImpl->mNativeFrameProjection)
            return false;
        for (std::size_t i = 0; i < state->mNodes.size(); ++i)
        {
            const auto& node = *mImpl->mNodes[i].mTransform;
            const auto& expected = state->mNodes[i];
            if (node.getMatrix() != expected.mMatrix
                || node.getTraversalMatrix() != expected.mTraversalMatrix
                || std::bit_cast<unsigned>(node.mScale) != std::bit_cast<unsigned>(expected.mScale))
                return false;
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 3; ++column)
                    if (std::bit_cast<unsigned>(node.mRotationScale.mValues[row][column])
                        != std::bit_cast<unsigned>(expected.mRotationScale.mValues[row][column]))
                        return false;
        }
        return true;
    }

    ActorRagdollLocalPose ActorRagdollPoseBinding::captureLocalPose() const
    {
        // Validate finite transform fields and the complete borrowed hierarchy
        // before retaining any state. Store the actual double matrices, not a
        // decomposed/recomposed float approximation of a procedural rotation.
        mImpl->locals();
        auto state = std::make_shared<ActorRagdollLocalPose::State>();
        state->mAuthority = mImpl->mLocalAuthority;
        state->mNativeFrameProjection = mImpl->mNativeFrameProjection;
        state->mNodes.reserve(mImpl->mNodes.size());
        for (const auto& entry : mImpl->mNodes)
        {
            // Missing KF rotation channels use this cache even when a
            // procedural callback currently renders a different rotation.
            // Both representations must be valid before retaining a checkpoint.
            worldPose(entry.mTransform->mRotationScale.toOsgMatrix());
            state->mNodes.push_back({entry.mTransform->getMatrix(),
                entry.mTransform->mRotationScale, entry.mTransform->mScale,
                entry.mTransform->getTraversalMatrix()});
        }
        ActorRagdollLocalPose result;
        result.mState = std::move(state);
        return result;
    }

    void ActorRagdollPoseBinding::restoreLocalPose(const ActorRagdollLocalPose& pose)
    {
        require(pose.mState && pose.mState->mAuthority == mImpl->mLocalAuthority
                && pose.mState->mNodes.size() == mImpl->mNodes.size(),
            "foreign or empty ragdoll renderer local checkpoint");
        // Do not validate the live numeric fields here: a failing animation
        // sampler must be able to roll back corrupted transform fields. The
        // checkpoint is immutable and was validated when captured. Identity
        // validation completes before any node or cached component is written.
        mImpl->validateHierarchy();
        for (std::size_t i = 0; i < mImpl->mNodes.size(); ++i)
        {
            auto& node = *mImpl->mNodes[i].mTransform;
            const auto& saved = pose.mState->mNodes[i];
            node.mScale = saved.mScale;
            node.mRotationScale = saved.mRotationScale;
            node.setMatrix(saved.mMatrix);
            node.setTraversalMatrix(saved.mTraversalMatrix);
        }
        mImpl->mNativeFrameProjection = pose.mState->mNativeFrameProjection;
        if (auto* skeleton = dynamic_cast<Skeleton*>(mImpl->mRoot.get()))
            skeleton->invalidateBoneMatrices();
    }

    std::vector<NifBullet::RagdollBoneWorldPose> ActorRagdollPoseBinding::captureWorldBones(
        const osg::Matrixf& objectWorld) const
    {
        return captureWorldBones(worldPose(objectWorld));
    }

    std::vector<NifBullet::RagdollBoneWorldPose> ActorRagdollPoseBinding::captureWorldBones(
        const Nif::NiTransform& objectWorld) const
    {
        const auto placement = nativePlacement(objectWorld);
        const auto world = mImpl->worlds(mImpl->locals(), placement);
        std::vector<NifBullet::RagdollBoneWorldPose> result;
        result.reserve(mImpl->mBodies.size());
        for (const auto& body : mImpl->mBodies)
            result.push_back({body.mNodeRecord, physicalWorldMatrix(world[body.mNode], placement.mScale)});
        return result;
    }

    void ActorRagdollPoseBinding::applyWorldBones(std::span<const NifBullet::RagdollBoneWorldPose> poses,
        const Nif::NiTransform& objectWorld)
    {
        require(poses.size() == mImpl->mBodies.size(), "ragdoll renderer snapshot count mismatch");
        const auto placement = nativePlacement(objectWorld);
        auto local = mImpl->locals();
        const auto previousWorld = mImpl->worlds(local, placement);
        std::unordered_map<std::uint32_t, Nif::NiTransform> desired;
        for (const auto& pose : poses)
            require(desired.emplace(pose.mNodeRecord, worldPose(pose.mPose)).second,
                "duplicate ragdoll renderer snapshot identity");
        std::unordered_map<std::size_t, Nif::NiTransform> targets;
        for (const auto& body : mImpl->mBodies)
        {
            const auto found = desired.find(body.mNodeRecord);
            require(found != desired.end(), "missing ragdoll renderer snapshot target");
            physicalWorldMatrix(previousWorld[body.mNode], placement.mScale);
            targets.emplace(body.mNode, found->second);
        }
        std::vector<Nif::NiTransform> candidateWorld;
        candidateWorld.reserve(local.size());
        for (std::size_t i = 0; i < local.size(); ++i)
        {
            const auto& parent = mImpl->mNodes[i].mParent
                ? candidateWorld[*mImpl->mNodes[i].mParent] : placement;
            if (const auto target = targets.find(i); target != targets.end())
            {
                // This adapter is explicitly for a dynamic physical snapshot.
                // Local writes are required to drive OSG's derived world/skinning matrices.
                const bool native = mImpl->mRootSpace != RootSpace::ObjectLocal;
                const auto projected = NifBullet::ragdollBonePoseWriteback(local[i], previousWorld[i],
                    target->second, mImpl->mRootSpace == RootSpace::ReferenceWorld
                            && !mImpl->mNodes[i].mParent ? nullptr : &parent, 9, true,
                    native ? Nif::NativeTransformPrecision::Single : Nif::NativeTransformPrecision::Double);
                local[i] = projected.mLocal;
                candidateWorld.push_back(projected.mWorld);
            }
            else
                candidateWorld.push_back(!mImpl->mNodes[i].mParent && mImpl->mRootSpace == RootSpace::ReferenceWorld
                    ? local[i] : mImpl->compose(parent, local[i]));
        }
        // Every identity, path and projection is validated before the first write.
        for (const auto& body : mImpl->mBodies)
        {
            auto& node = *mImpl->mNodes[body.mNode].mTransform;
            node.setRotation(local[body.mNode].mRotation);
            node.setTranslation(local[body.mNode].mTranslation);
        }
        // An explicit dynamic snapshot retires this binding's frame-only
        // drawing handles. Never leave an old world-only pose over new locals.
        if (mImpl->mNativeFrameProjection)
        {
            for (auto& entry : mImpl->mNodes) entry.mTransform->clearTraversalMatrix();
            mImpl->mNativeFrameProjection = false;
        }
        if (auto* skeleton = dynamic_cast<Skeleton*>(mImpl->mRoot.get()))
            skeleton->invalidateBoneMatrices();
    }

    void ActorRagdollPoseBinding::applyWorldBones(std::span<const NifBullet::RagdollBoneWorldPose> poses,
        const osg::Matrixf& objectWorld)
    {
        applyWorldBones(poses, worldPose(objectWorld));
    }
}
