#ifndef OPENMW_MWWORLD_OBLIVIONTRESPASS_HPP
#define OPENMW_MWWORLD_OBLIVIONTRESPASS_HPP

#include <optional>
#include <components/esm/formkey.hpp>

namespace MWWorld
{
    class World;
    class Ptr;
    // Read-only classification shared by native conditions and door permission.
    std::optional<bool> oblivionActorTrespassing(World& world, const Ptr& actor,
        const ESM::FormKey& base, const ESM::FormKey& cell);
    // Original Player exit permission; does not unlock, activate or report crime.
    bool oblivionPlayerHasDoorExitPermission(World& world, const Ptr& door, const Ptr& actor);
}

#endif
