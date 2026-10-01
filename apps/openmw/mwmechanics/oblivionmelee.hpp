#ifndef OPENMW_MWMECHANICS_OBLIVIONMELEE_H
#define OPENMW_MWMECHANICS_OBLIVIONMELEE_H

#include <cstdint>
#include <optional>
#include <components/esm4/physicalcombat.hpp>
#include "../mwworld/ptr.hpp"

namespace ESM4 { struct GameSetting; }
namespace MWBase { class World; }
namespace MWPhysics { class Actor; }

namespace MWMechanics
{
    struct OblivionMeleeAcquisitionSettings
    {
        float mConeDegrees;
        float mSlopeDifference;
    };
    // Winning native records override verified original compiled defaults.
    OblivionMeleeAcquisitionSettings resolveOblivionMeleeAcquisitionSettings(
        std::span<const ESM4::GameSetting* const> settings);

    // Physical body adapter: positions/vertical bounds come from the current
    // collision body; radial scale is the separately resolved native actor scale.
    ESM4::MeleeDistanceActor oblivionMeleeBody(const MWPhysics::Actor& body,
        float combatScale, bool swimming);

    // Read-only query for a pending, owned native action. nullopt means that
    // the action or physical context is unavailable; an engaged empty Ptr is
    // a physical miss. The caller resolves weapon/creature reach. No ID or AV
    // changes, damage, animation or contact publication occurs here.
    std::optional<MWWorld::Ptr> acquireOblivionMeleeContact(MWBase::World& world,
        std::uint64_t actionId, const MWWorld::Ptr& attacker,
        const MWWorld::Ptr& selectedTarget, float reach);
    // Ordinary physical misses consume the owned action and spend native
    // attack fatigue once. Unavailable physics and acquired victims return
    // false without mutation; successful-hit policy is a separate adapter.
    bool commitOblivionOrdinaryMeleeMiss(MWBase::World& world, std::uint64_t actionId,
        const MWWorld::Ptr& attacker, const MWWorld::Ptr& selectedTarget,
        float reach, float weaponWeight);

    // Read native equipped condition and AV authority. This is a query, not
    // damage, block, wear or effect execution; shared TES3 ratings are not used.
    // Resolve actual native actor binding before reading its process byte.
    std::int8_t oblivionKnockedState(MWBase::World& world, const MWWorld::Ptr& actor);

    float oblivionArmorRating(MWBase::World& world, const MWWorld::Ptr& actor);

}
#endif
