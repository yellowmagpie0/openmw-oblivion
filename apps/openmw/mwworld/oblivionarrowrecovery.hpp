#ifndef OPENMW_MWWORLD_OBLIVIONARROWRECOVERY_H
#define OPENMW_MWWORLD_OBLIVIONARROWRECOVERY_H

#include <components/esm4/projectilerules.hpp>
#include <memory>
#include <osg/Vec3f>

namespace ESM4 { struct RuntimeReferenceState; }
namespace MWPhysics { class PhysicsSystem; }
namespace MWRender { class Objects; }
namespace MWWorld
{
    class World;
    class ESMStore;
    class Ptr;

    struct RecoveredArrowState
    {
        ESM::FormKey mBow, mCaster, mSurface;
        std::uint64_t mProjectile = 0;
        ESM4::ArrowLifetimeState mLifetime;
    };
    std::optional<RecoveredArrowState> readRecoveredArrow(const ESM4::RuntimeReferenceState& reference);
    void writeRecoveredArrow(ESM4::RuntimeReferenceState& reference, const RecoveredArrowState& state);
    void validateRecoveredArrowSource(const ESMStore& store, const ESM4::RuntimeReferenceState& reference,
        const RecoveredArrowState& state);

    class PreparedArrowRecovery
    {
        struct Data;
        std::unique_ptr<Data> mData;
        explicit PreparedArrowRecovery(std::unique_ptr<Data> data);
        friend std::unique_ptr<PreparedArrowRecovery> prepareArrowRecovery(World&, MWRender::Objects&,
            MWPhysics::PhysicsSystem&, const Ptr&, const ESM4::ArrowProjectileState&, std::uint64_t,
            const osg::Vec3f&);
    public:
        ~PreparedArrowRecovery();
        const ESM4::RuntimeReferenceState& reference() const;
        // Both callbacks belong to one prepared impact. Publication must be
        // noexcept and perform no allocation, registration or observers.
        Ptr commit(void* context, bool (*validate)(void*) noexcept, void (*publish)(void*) noexcept);
    };
    std::unique_ptr<PreparedArrowRecovery> prepareArrowRecovery(World& world, MWRender::Objects& objects,
        MWPhysics::PhysicsSystem& physics, const Ptr& surface, const ESM4::ArrowProjectileState& arrow,
        std::uint64_t serial, const osg::Vec3f& hit);
}
#endif
