#ifndef OPENMW_MWWORLD_OBLIVIONWEAPONDROP_H
#define OPENMW_MWWORLD_OBLIVIONWEAPONDROP_H

#include "ptr.hpp"
#include "worldmodel.hpp"
#include <memory>
#include <optional>
#include <span>

namespace MWRender { class ESM4NpcAnimation; class Objects; }
namespace MWPhysics { class PhysicsSystem; }
namespace MWWorld
{
    class World;
    struct EquippedWeaponDrop
    {
        Ptr mActor;
        Ptr mItem;
        MWRender::ESM4NpcAnimation& mAnimation;
        // Final wear is prepared on source and reference together. A compound
        // contact may also publish the same condition before the source debit.
        std::optional<float> mConditionAfterWear;
    };

    class PreparedEquippedWeaponDrops
    {
        struct Data;
        std::unique_ptr<Data> mData;
        explicit PreparedEquippedWeaponDrops(std::unique_ptr<Data> data);
        friend std::unique_ptr<PreparedEquippedWeaponDrops> prepareEquippedWeaponDrops(
            World&, MWRender::Objects&, MWPhysics::PhysicsSystem&, std::span<const EquippedWeaponDrop>);
    public:
        ~PreparedEquippedWeaponDrops();
        bool isValid() const;
        std::span<const Ptr> references() const noexcept;
        // No observers run here. On success, removal notifications are queued
        // for World's deferred equipment dispatch. Caller owns native policy,
        // contact wear, retained-item unequips, replacement and handling sound.
        Ptr commit(const WorldModel::LooseWeaponPublicationHooks& hooks = {});
    };

    // Only actual equipped native ACHR/NPC WEAP instances with a currently bound
    // carried NIF can be prepared. Explicit render context permits owned-asset
    // integration tests; physics must be the supplied World's current owner.
    std::unique_ptr<PreparedEquippedWeaponDrops> prepareEquippedWeaponDrops(
        World& world, MWRender::Objects& objects, MWPhysics::PhysicsSystem& physics,
        std::span<const EquippedWeaponDrop> drops);

    // After physics workers finish, publish loose native weapon root poses to
    // their reference and placement node. Cell crossings use World's normal
    // ownership migration without feeding the pose back into the rigid body.
    void synchronizeLooseWeaponPoses(World& world, MWPhysics::PhysicsSystem& physics);
}
#endif
