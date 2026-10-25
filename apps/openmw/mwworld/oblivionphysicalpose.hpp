#ifndef OPENMW_MWWORLD_OBLIVIONPHYSICALPOSE_HPP
#define OPENMW_MWWORLD_OBLIVIONPHYSICALPOSE_HPP

#include <components/esm4/physicalblendsettings.hpp>
#include <span>
#include <memory>
#include <string_view>
#include <osg/Vec3f>

namespace ESM { struct FormKey; }
namespace ESM4 { struct RuntimeActorRagdoll; }
namespace MWPhysics { class PhysicsSystem; }
namespace MWRender { class Animation; }
namespace Nif { struct NiTransform; }
namespace NifBullet
{
    enum class RagdollNativeKnockdownBlendDisposition;
    enum class RagdollNativeHitBlendDisposition;
    struct ActorRagdollDefinition;
    class ActorRagdollPhysics;
    struct RagdollInternalCollisionFilter;
    struct RagdollNativeGetUpNodeResult;
}

namespace MWWorld
{
    class Ptr;
    struct OblivionPhysicalDownRequest
    {
        std::uint32_t mNodeRecord;
        std::uint32_t mResolvedPackedFilter;
        osg::Vec3f mWorldVector;
    };
    struct OblivionPhysicalHitBlendRequest
    {
        std::uint32_t mNodeRecord;
        std::uint32_t mResolvedPackedFilter;
    };
    struct OblivionPhysicalHitVelocityRequest
    {
        std::uint32_t mNodeRecord;
        std::array<float, 4> mNativeSourceVector;
        float mResolvedMassMultiplier;
    };
    // Owns staged renderer callbacks/binding and an unpublished physical graph.
    // Snapshot restore and controller setup can fail here without taking live
    // renderer/capsule ownership. A consumed or expired candidate cannot revive.
    // Native admission retains the movement capsule independently of the
    // retained graph's body/constraint World registration selection.
    class PreparedNativeActorPhysicalPose
    {
        struct Data;
        std::unique_ptr<Data> mData;
        explicit PreparedNativeActorPhysicalPose(std::unique_ptr<Data> data);
    public:
        ~PreparedNativeActorPhysicalPose();
        PreparedNativeActorPhysicalPose(const PreparedNativeActorPhysicalPose&) = delete;
        PreparedNativeActorPhysicalPose& operator=(const PreparedNativeActorPhysicalPose&) = delete;
        static std::unique_ptr<PreparedNativeActorPhysicalPose> prepare(
            MWPhysics::PhysicsSystem& physics, MWRender::Animation& animation,
            const Ptr& actor, const NifBullet::ActorRagdollDefinition& authored,
            const Nif::NiTransform& placement, std::span<const std::uint32_t> resolvedPackedFilters,
            const ESM4::PhysicalBlendGainTable& resolvedGains, int collisionGroup, int collisionMask,
            const NifBullet::RagdollInternalCollisionFilter* internalFilter);
        bool isValid() const;
        NifBullet::ActorRagdollPhysics& physics();
        void restoreSnapshot(const ESM4::RuntimeActorRagdoll& snapshot,
            const ESM::FormKey& base, std::string_view model);
        ESM4::RuntimeActorRagdoll captureSnapshot(const ESM::FormKey& base, std::string_view model);
        bool commit();
    };

    // Borrowed renderer/physics lifetimes. The caller supplies resolved native
    // uniform placement, resolved packed body filters/gain table and collision policy; this does not select reactions,
    // infer NPC morphology, or install generated controllers.
    void beginNativeActorPhysicalPose(MWPhysics::PhysicsSystem& physics, MWRender::Animation& animation,
        const Ptr& actor, const NifBullet::ActorRagdollDefinition& authored,
        const Nif::NiTransform& placement, std::span<const std::uint32_t> resolvedPackedFilters,
        const ESM4::PhysicalBlendGainTable& resolvedGains, int collisionGroup, int collisionMask,
        const NifBullet::RagdollInternalCollisionFilter* internalFilter);
    void endNativeActorPhysicalPose(MWPhysics::PhysicsSystem& physics, MWRender::Animation& animation,
        const Ptr& actor);
}

#endif
