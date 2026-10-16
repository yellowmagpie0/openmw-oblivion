#ifndef OPENMW_COMPONENTS_SCENEUTIL_ACTORRAGDOLLPOSE_HPP
#define OPENMW_COMPONENTS_SCENEUTIL_ACTORRAGDOLLPOSE_HPP

#include <components/nifbullet/actorragdollphysics.hpp>
#include <components/nif/niftypes.hpp>

#include <memory>
#include <span>
#include <vector>

namespace osg
{
    class Group;
}

namespace SceneUtil
{
    // Immutable, binding-owned local checkpoint for temporarily sampling
    // animation targets. Retains OSG double matrices and separate NIF caches.
    // Ephemeral renderer state, not a save format or physical world snapshot.
    class ActorRagdollLocalPose
    {
    public:
        ActorRagdollLocalPose() = default;
    private:
        struct State;
        std::shared_ptr<const State> mState;
        friend class ActorRagdollPoseBinding;
    };

    // A borrowed binding to this asset's original NIF transform hierarchy.
    // It captures live world bones and applies complete physical world poses
    // atomically through the verified native local-pose projection. Actor
    // mode selection, physics stepping and persistence belong to their owners.
    // Capture uses controller/local matrices, independently of optional NIF
    // traversal projections. Checkpoints retain both representations.
    // Matrix placement is rigid. Native placement additionally admits separate
    // uniform scale; physical bones retain neighboring-unit authored scales.
    // Call on the renderer update thread, without concurrent topology changes.
    class ActorRagdollPoseBinding
    {
    public:
        enum class RootSpace { ObjectLocal, ReferenceWorld };
        // ReferenceWorld requires one declared native reference root. Its
        // author local already contains global R/T/scale; placement R/T is not
        // applied again. Placement scale still validates physical bone scales.
        // Native reference composition uses observed Single precision; existing
        // ObjectLocal behavior and precision remain unchanged.
        ActorRagdollPoseBinding(const NifBullet::ActorRagdollDefinition& definition, osg::Group& root,
            RootSpace rootSpace = RootSpace::ObjectLocal);
        ~ActorRagdollPoseBinding();
        ActorRagdollPoseBinding(const ActorRagdollPoseBinding&) = delete;
        ActorRagdollPoseBinding& operator=(const ActorRagdollPoseBinding&) = delete;

        // Borrowed asset/record/name/path identities only. No allocations or
        // transform writes; suitable for a prepared scene publication guard.
        bool isValid() const noexcept;
        bool matchesLocalPose(const ActorRagdollLocalPose& pose) const noexcept;

        ActorRagdollLocalPose captureLocalPose() const;
        // Requires this exact binding and unchanged asset/hierarchy identities.
        // Validate all identities before writing; can recover corrupted live
        // transform fields from the previously validated immutable checkpoint.
        void restoreLocalPose(const ActorRagdollLocalPose& pose);

        std::vector<NifBullet::RagdollBoneWorldPose> captureWorldBones(const osg::Matrixf& objectWorld) const;
        void applyWorldBones(std::span<const NifBullet::RagdollBoneWorldPose> poses,
            const osg::Matrixf& objectWorld);

        // Native placement retains uniform scale separately from rotation.
        // The caller must scale the physical graph's properties independently.
        // Authored physical bone scales must remain neighboring-unit relative
        // to this placement; nonuniform scale/shear is never admitted.
        std::vector<NifBullet::RagdollBoneWorldPose> captureWorldBones(const Nif::NiTransform& objectWorld) const;
        void applyWorldBones(std::span<const NifBullet::RagdollBoneWorldPose> poses,
            const Nif::NiTransform& objectWorld);

    private:
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}

#endif
