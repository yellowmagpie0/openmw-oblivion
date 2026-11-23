#ifndef OPENMW_MWRENDER_GROUNDCOVERPLACEMENT_H
#define OPENMW_MWRENDER_GROUNDCOVERPLACEMENT_H

#include <algorithm>
#include <cmath>
#include <span>
#include <utility>

#include <osg/Vec3f>

#include <components/esm4/loadland.hpp>

namespace MWRender
{
    inline bool acceptsGrassWaterDistance(unsigned mode, float distance, float heightAboveWater)
    {
        switch (mode)
        {
            case 0:
                return heightAboveWater >= distance;
            case 1:
                return heightAboveWater >= 0.f && heightAboveWater <= distance;
            case 2:
                return heightAboveWater <= -distance;
            case 3:
                return heightAboveWater <= 0.f && heightAboveWater >= -distance;
            case 4:
                return std::abs(heightAboveWater) >= distance;
            case 5:
                return std::abs(heightAboveWater) <= distance;
            case 6:
                return heightAboveWater <= distance;
            case 7:
                return heightAboveWater >= -distance;
            default:
                return true;
        }
    }

    inline std::pair<float, osg::Vec3f> sampleGrassSurface(std::span<const float> heights, float x, float y)
    {
        constexpr int side = ESM4::Land::sVertsPerSide;
        constexpr float step = static_cast<float>(ESM4::Land::sRealSize) / (side - 1);
        const float sx = std::clamp(x, 0.f, 1.f) * (side - 1);
        const float sy = std::clamp(y, 0.f, 1.f) * (side - 1);
        const int ix = std::min(static_cast<int>(sx), side - 2);
        const int iy = std::min(static_cast<int>(sy), side - 2);
        const float u = sx - ix;
        const float v = sy - iy;
        const auto h = heights.subspan(iy * side + ix);
        // Match the alternating diagonals in Terrain::BufferCache. Select the X and Y
        // edges of the triangle containing the sample, then solve its plane.
        const bool odd = (ix + iy) % 2 != 0;
        const bool top = odd ? u + v > 1.f : v > u;
        const bool right = odd ? top : !top;
        const float dx = top ? h[side + 1] - h[side] : h[1] - h[0];
        const float dy = right ? h[side + 1] - h[1] : h[side] - h[0];
        const float height = top ? h[side] + u * dx + (v - 1.f) * dy : h[0] + u * dx + v * dy;
        osg::Vec3f normal(-dx, -dy, step);
        normal.normalize();
        return { height, normal };
    }
}

#endif
