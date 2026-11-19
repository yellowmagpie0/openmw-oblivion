#ifndef OPENMW_MWGUI_QUESTBEARING_H
#define OPENMW_MWGUI_QUESTBEARING_H

#include <cmath>
#include <format>
#include <optional>
#include <string>

#include <osg/Vec2f>

#include <components/misc/constants.hpp>

namespace MWGui
{
    inline std::optional<float> questBearing(const osg::Vec2f& offset, const osg::Vec2f& forward)
    {
        if (offset.length2() <= 1.f || forward.length2() <= 1e-6f)
            return std::nullopt;

        // MyGUI rotates clockwise from the texture's upward-pointing tip.
        return std::atan2(forward.y() * offset.x() - forward.x() * offset.y(), forward * offset);
    }

    inline std::string questDistance(const osg::Vec2f& offset)
    {
        const float metres = offset.length() / Constants::UnitsPerMeter;
        if (metres < 999.5f)
            return std::format("{:.0f} m", metres);
        return std::format("{:.2f} km", metres / 1000.f);
    }
}

#endif
