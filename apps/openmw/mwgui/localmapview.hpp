#ifndef OPENMW_MWGUI_LOCALMAPVIEW_H
#define OPENMW_MWGUI_LOCALMAPVIEW_H

#include <MyGUI_Types.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace MWGui::LocalMapView
{
    inline MyGUI::IntRect gridForViewport(const MyGUI::IntRect& full, double x, double y,
        int width, int height, double cellSize)
    {
        if (!std::isfinite(cellSize) || cellSize <= 0 || !std::isfinite(x) || !std::isfinite(y))
            throw std::invalid_argument("Invalid local-map viewport");
        const auto axis = [&](int low, int high, double center, int pixels) {
            const auto extent = std::int64_t(high) - low + 1;
            if (extent <= 0) throw std::invalid_argument("Inverted local-map grid");
            const auto count = static_cast<std::int64_t>(std::min(double(extent),
                std::ceil(std::max(pixels, 0) / cellSize) + 4));
            const double start = std::clamp(std::floor(center) - double(count / 2),
                double(low), double(std::int64_t(high) - count + 1));
            return std::pair{static_cast<int>(start), static_cast<int>(std::int64_t(start) + count - 1)};
        };
        const auto [left, right] = axis(full.left, full.right, x, width);
        const auto [top, bottom] = axis(full.top, full.bottom, y, height);
        return {left, top, right, bottom};
    }

    // Off-canvas markers retain their logical coordinates. Their clipped widget
    // coordinates leave room for MyGUI's rectangle and parent-offset arithmetic.
    inline int pixel(double value)
    {
        constexpr int limit = std::numeric_limits<int>::max() / 4;
        if (!std::isfinite(value)) return 0;
        return static_cast<int>(std::clamp(std::round(value), -double(limit), double(limit)));
    }

    inline MyGUI::IntPoint position(const MyGUI::IntRect& grid, int cellX, int cellY,
        float nx, float ny, double cellSize)
    {
        return {pixel((double(nx) + double(cellX) - grid.left) * cellSize),
            pixel((double(ny) - double(cellY) + grid.bottom) * cellSize)};
    }
}
#endif
