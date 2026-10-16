#ifndef OPENMW_COMPONENTS_ESM4_PLACEMENTSINCOS_H
#define OPENMW_COMPONENTS_ESM4_PLACEMENTSINCOS_H

#include <boost/multiprecision/cpp_int.hpp>
#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>

// Internal placement arithmetic. Callers validate finite |angle| < 2^63.
namespace ESM4::Detail
{
    inline std::pair<float, float> placementSinCos(float angle) noexcept
    {
        double reduced = angle;
        unsigned quadrant = 0;
        bool negative = false;
        if (std::abs(double(angle)) >= .7853981633974483)
        {
            // Intel SDM vol1, Approximation of Pi: the x87 reduction
            // constant has 66 significant bits. Host libm's more accurate
            // pi changes native float results at large finite angles.
            // Fixed-size integer arithmetic retains the exact remainder
            // without heap storage or a platform-specific long double.
            using boost::multiprecision::uint128_t;
            const auto bits = std::bit_cast<std::uint32_t>(angle) & 0x7fffffff;
            const uint128_t numerator = uint128_t((bits & 0x7fffff) | 0x800000)
                << (int(bits >> 23) - 85);
            const uint128_t halfPi = (uint128_t(3) << 64) | UINT64_C(0x243f6a8885a308d3);
            auto quotient = numerator / halfPi;
            auto remainder = numerator % halfPi;
            const bool upper = remainder * 2 > halfPi;
            if (upper) { ++quotient; remainder = halfPi - remainder; }
            reduced = std::ldexp(remainder.convert_to<double>(), -65);
            if (upper) reduced = -reduced;
            quadrant = (quotient & 3).convert_to<unsigned>();
            negative = std::signbit(angle);
        }
        const double sine = std::sin(reduced), cosine = std::cos(reduced);
        double s = sine, c = cosine;
        switch (quadrant)
        {
            case 1: s = cosine; c = -sine; break;
            case 2: s = -sine; c = -cosine; break;
            case 3: s = -cosine; c = sine; break;
        }
        if (negative) s = -s;
        return {static_cast<float>(s), static_cast<float>(c)};
    }
}


#endif
