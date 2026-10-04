#ifndef OPENMW_MWMECHANICS_OBLIVIONRANGED_H
#define OPENMW_MWMECHANICS_OBLIVIONRANGED_H

#include "../mwworld/oblivioncombatdata.hpp"
#include "../mwworld/ptr.hpp"

namespace MWBase { class World; }

namespace MWMechanics
{
    // Read the actual equipped native bow/ammunition and AV/condition authority.
    // Caller supplies the player timer; NPC full draw does not use it.
    // This is read-only preparation, not release admission or ammo/AV mutation.
    MWWorld::OblivionArrowLaunch sampleOblivionEquippedArrowLaunch(
        MWBase::World& world, const MWWorld::Ptr& actor, float playerBowTimer);
}
#endif
