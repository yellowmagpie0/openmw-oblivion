#ifndef OPENMW_MWWORLD_OBLIVIONDOORANIMATION_HPP
#define OPENMW_MWWORLD_OBLIVIONDOORANIMATION_HPP

#include <optional>
#include "doorstate.hpp"

namespace MWWorld
{
    class World;
    class Ptr;
    class ConstPtr;

    // Embedded doors move their model, not the reference yaw. Read the active
    // playback owner, or its existing saved pose when it is not resident.
    std::optional<int> oblivionAnimatedDoorState(const World& world, const ConstPtr& door);
    bool playOblivionDoorAnimation(World& world, const Ptr& door, DoorState transition);
}

#endif
