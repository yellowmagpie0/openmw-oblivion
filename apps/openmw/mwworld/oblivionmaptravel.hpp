#ifndef OPENMW_MWWORLD_OBLIVIONMAPTRAVEL_HPP
#define OPENMW_MWWORLD_OBLIVIONMAPTRAVEL_HPP

#include <string_view>

namespace ESM4
{
    struct Reference;
}

namespace MWWorld
{
    class World;
    // Returns a player-facing reason when travel is unavailable.
    std::string_view travelToOblivionMapMarker(World& world, const ESM4::Reference& marker);
}

#endif
