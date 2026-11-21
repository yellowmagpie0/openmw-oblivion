#ifndef OPENMW_MWGUI_WORLD_MAP_PROJECTION_H
#define OPENMW_MWGUI_WORLD_MAP_PROJECTION_H

#include <components/esm4/loadwrld.hpp>

#include <osg/Vec2f>

#include <algorithm>

namespace MWGui
{
    inline bool hasWorldMapBounds(const ESM4::World::Map& map)
    {
        return map.width > 0 && map.height > 0 && map.NWcellX < map.SEcellX && map.NWcellY > map.SEcellY;
    }

    inline float worldMapTextureBottom(const ESM4::World::Map& map, int textureWidth, int textureHeight)
    {
        if (textureHeight <= 0 || textureWidth <= 0)
            return 1.f;
        return std::min(1.f, (float(map.height) / map.width) * (float(textureWidth) / textureHeight));
    }

    // WRLD describes a north-up image in TES4 exterior cell coordinates.
    inline osg::Vec2f projectWorldMap(const ESM4::World::Map& map, float x, float y)
    {
        constexpr float cellSize = 4096.f;
        return { (x / cellSize - map.NWcellX) / (map.SEcellX - map.NWcellX) * map.width,
            (map.NWcellY - y / cellSize) / (map.NWcellY - map.SEcellY) * map.height };
    }
}
#endif
