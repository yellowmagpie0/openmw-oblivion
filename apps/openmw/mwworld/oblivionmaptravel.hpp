#ifndef OPENMW_MWWORLD_OBLIVIONMAPTRAVEL_HPP
#define OPENMW_MWWORLD_OBLIVIONMAPTRAVEL_HPP

#include <components/esm/formkey.hpp>
#include <cstdint>
#include <map>
#include <string_view>

namespace ESM4
{
    struct Reference;
}

namespace MWWorld
{
    class World;
    constexpr char mapMarkerRevealKey[] = "native.map_marker.reveal";
    using MapMarkerReveals = std::map<ESM::FormKey, std::uint8_t>;
    std::uint8_t oblivionMapMarkerFlags(const ESM4::Reference& marker, const MapMarkerReveals& reveals);
    // Returns a player-facing reason when travel is unavailable.
    std::string_view travelToOblivionMapMarker(World& world, const ESM4::Reference& marker);
}

#endif
