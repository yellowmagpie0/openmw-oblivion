#ifndef OPENMW_COMPONENTS_SCENEUTIL_ACTORRAGDOLLPOSE_HPP
#define OPENMW_COMPONENTS_SCENEUTIL_ACTORRAGDOLLPOSE_HPP

#include <components/nifbullet/actorragdollphysics.hpp>

#include <memory>
#include <span>
#include <vector>

namespace osg
{
    class Group;
}

namespace SceneUtil
{
    // A borrowed binding to this asset's original NIF transform hierarchy.
    // It captures live world bones and applies complete physical world poses
    // atomically through the verified native local-pose projection. Actor
    // mode selection, physics stepping and persistence belong to their owners.
    // Placement is rigid; physical targets admit authored neighboring-unit
    // world scale while retaining their separate renderer scale fields.
    // Call on the renderer update thread, without concurrent topology changes.
    class ActorRagdollPoseBinding
    {
    public:
        ActorRagdollPoseBinding(const NifBullet::ActorRagdollDefinition& definition, osg::Group& root);
        ~ActorRagdollPoseBinding();
        ActorRagdollPoseBinding(const ActorRagdollPoseBinding&) = delete;
        ActorRagdollPoseBinding& operator=(const ActorRagdollPoseBinding&) = delete;

        std::vector<NifBullet::RagdollBoneWorldPose> captureWorldBones(const osg::Matrixf& objectWorld) const;
        void applyWorldBones(std::span<const NifBullet::RagdollBoneWorldPose> poses,
            const osg::Matrixf& objectWorld);

    private:
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}

#endif
