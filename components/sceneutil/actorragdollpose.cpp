#include "actorragdollpose.hpp"

#include "skeleton.hpp"

#include <components/misc/osguservalues.hpp>
#include <components/nifbullet/ragdollbonepose.hpp>
#include <components/nifosg/matrixtransform.hpp>

#include <osg/Group>
#include <osg/Transform>
#include <osg/observer_ptr>

#include <optional>
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

        osg::Matrixf physicalWorldMatrix(const Nif::NiTransform& pose)
        {
            // Authored neighboring-unit scale values are retained in renderer
            // nodes. Ordinary body sync consumes NiWorld rotation and position,
            // not its separate scale slot; broader actor scaling remains unadmitted.
            require(std::isfinite(pose.mScale) && std::abs(pose.mScale - 1.f) <= 1e-4f,
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
            // Procedural rotate controllers also deliberately leave the cached
            // NIF rotation unchanged. Capture the currently rendered rotation
            // using its separately stored scale, rather than that cached base.
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

    struct ActorRagdollPoseBinding::Impl
    {
        struct Node
        {
            osg::observer_ptr<NifOsg::MatrixTransform> mTransform;
            std::vector<osg::observer_ptr<osg::Node>> mPath;
            std::optional<std::size_t> mParent;
        };
        struct Body
        {
            std::uint32_t mNodeRecord;
            std::size_t mNode;
            std::string mBone;
        };
        osg::observer_ptr<osg::Group> mRoot;
        osg::observer_ptr<osg::Group> mAssetRoot;
        std::string mSourceHash;
        std::vector<Node> mNodes;
        std::vector<Body> mBodies;

        Impl(const NifBullet::ActorRagdollDefinition& definition, osg::Group& root)
            : mRoot(&root), mSourceHash(definition.mSourceHash)
        {
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
                    mNodes.push_back({transform, path, parent});
                    parent = index;
                }
                else
                    require(dynamic_cast<osg::Transform*>(&node) == nullptr,
                        "unadmitted ragdoll renderer transform type");
                if (auto* group = node.asGroup())
                    for (unsigned i = 0; i < group->getNumChildren(); ++i)
                        self(self, *group->getChild(i), path, parent);
            };
            walk(walk, root, {}, {});
            std::unordered_set<std::uint32_t> bodyRecords, bodyNodes;
            for (const auto& body : definition.mBodies)
            {
                require(!body.mUsesRigidBodyTransform, "unadmitted bhkRigidBodyT renderer binding");
                require(bodyRecords.insert(body.mRecord).second && bodyNodes.insert(body.mNodeRecord).second,
                    "duplicate ragdoll renderer body identity");
                const auto found = records.find(body.mNodeRecord);
                require(found != records.end() && !body.mBone.empty()
                        && mNodes[found->second].mTransform->getName() == body.mBone,
                    "ragdoll renderer target identity mismatch");
                mBodies.push_back({body.mNodeRecord, found->second, body.mBone});
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
            std::vector<std::size_t> remap(mNodes.size());
            std::vector<Node> selected;
            for (std::size_t i = 0; i < mNodes.size(); ++i)
            {
                if (!needed[i])
                    continue;
                auto entry = std::move(mNodes[i]);
                localPose(*entry.mTransform);
                if (entry.mParent)
                    entry.mParent = remap[*entry.mParent];
                remap[i] = selected.size();
                selected.push_back(std::move(entry));
            }
            for (auto& body : mBodies)
                body.mNode = remap[body.mNode];
            mNodes = std::move(selected);
        }

        void validateRoot() const
        {
            std::string hash;
            require(mRoot.valid() && mAssetRoot.valid()
                    && mAssetRoot->getUserValue(Misc::OsgUserValues::sFileHash, hash) && hash == mSourceHash,
                "expired or mismatched ragdoll renderer asset");
        }

        std::vector<Nif::NiTransform> locals() const
        {
            validateRoot();
            std::vector<Nif::NiTransform> result;
            result.reserve(mNodes.size());
            for (const auto& entry : mNodes)
            {
                require(entry.mTransform.valid(), "expired ragdoll renderer bone");
                for (std::size_t i = 0; i < entry.mPath.size(); ++i)
                {
                    require(entry.mPath[i].valid(), "expired ragdoll renderer path");
                    if (i)
                        require(entry.mPath[i]->getNumParents() == 1
                                && entry.mPath[i]->getParent(0) == entry.mPath[i - 1].get(),
                            "detached or ambiguous ragdoll renderer path");
                }
                result.push_back(localPose(*entry.mTransform));
            }
            for (const auto& body : mBodies)
            {
                unsigned record;
                const auto& node = *mNodes[body.mNode].mTransform;
                require(node.getUserValue("recordIndex", record) && record == body.mNodeRecord
                        && node.getName() == body.mBone,
                    "changed ragdoll renderer bone identity");
            }
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
                result.push_back(NifBullet::composeRagdollBonePose(parent, local[i]));
            }
            return result;
        }
    };

    ActorRagdollPoseBinding::ActorRagdollPoseBinding(const NifBullet::ActorRagdollDefinition& definition, osg::Group& root)
        : mImpl(std::make_unique<Impl>(definition, root))
    {
    }

    ActorRagdollPoseBinding::~ActorRagdollPoseBinding() = default;

    std::vector<NifBullet::RagdollBoneWorldPose> ActorRagdollPoseBinding::captureWorldBones(
        const osg::Matrixf& objectWorld) const
    {
        const auto placement = worldPose(objectWorld);
        const auto world = mImpl->worlds(mImpl->locals(), placement);
        std::vector<NifBullet::RagdollBoneWorldPose> result;
        result.reserve(mImpl->mBodies.size());
        for (const auto& body : mImpl->mBodies)
            result.push_back({body.mNodeRecord, physicalWorldMatrix(world[body.mNode])});
        return result;
    }

    void ActorRagdollPoseBinding::applyWorldBones(std::span<const NifBullet::RagdollBoneWorldPose> poses,
        const osg::Matrixf& objectWorld)
    {
        require(poses.size() == mImpl->mBodies.size(), "ragdoll renderer snapshot count mismatch");
        const auto placement = worldPose(objectWorld);
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
            physicalWorldMatrix(previousWorld[body.mNode]);
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
                const auto projected = NifBullet::ragdollBonePoseWriteback(local[i], previousWorld[i],
                    target->second, &parent, 9, true);
                local[i] = projected.mLocal;
                candidateWorld.push_back(projected.mWorld);
            }
            else
                candidateWorld.push_back(NifBullet::composeRagdollBonePose(parent, local[i]));
        }
        // Every identity, path and projection is validated before the first write.
        for (const auto& body : mImpl->mBodies)
        {
            auto& node = *mImpl->mNodes[body.mNode].mTransform;
            node.setRotation(local[body.mNode].mRotation);
            node.setTranslation(local[body.mNode].mTranslation);
        }
        if (auto* skeleton = dynamic_cast<Skeleton*>(mImpl->mRoot.get()))
            skeleton->invalidateBoneMatrices();
    }
}
