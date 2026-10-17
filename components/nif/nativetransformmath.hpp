#ifndef OPENMW_COMPONENTS_NIF_NATIVETRANSFORMMATH_HPP
#define OPENMW_COMPONENTS_NIF_NATIVETRANSFORMMATH_HPP

#include "nativetransform.hpp"

#include <bit>
#include <cstdint>
#include <cmath>

namespace Nif::Detail
{
    // x87 single precision rounds each arithmetic instruction's significand,
    // while retaining a wider exponent than a binary32 memory store. All
    // intermediate exponents from the binary32 transform fields fit double.
    class NativeTransformMath
    {
        NativeTransformPrecision mPrecision;

        double round(double value) const noexcept
        {
            if (mPrecision == NativeTransformPrecision::Double)
                return value;
            auto bits = std::bit_cast<std::uint64_t>(value);
            if ((bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull)
                return value;
            constexpr std::uint64_t unit = 1ull << 29, half = unit >> 1, mask = unit - 1;
            const auto discarded = bits & mask;
            const bool increment = discarded > half || (discarded == half && (bits & unit));
            bits = (bits & ~mask) + (increment ? unit : 0);
            return std::bit_cast<double>(bits);
        }

    public:
        explicit NativeTransformMath(NativeTransformPrecision precision) : mPrecision(precision) {}
        bool valid() const noexcept
        {
            return mPrecision == NativeTransformPrecision::Single || mPrecision == NativeTransformPrecision::Double;
        }
        double add(double a, double b) const noexcept { return round(a + b); }
        double multiply(double a, double b) const noexcept { return round(a * b); }
        double divide(double a, double b) const noexcept { return round(a / b); }
        double reciprocal(double value) const noexcept { return round(1.0 / value); }
        double squareRoot(double value) const noexcept { return round(std::sqrt(value)); }
        double dot(float a0, float b0, float a1, float b1, float a2, float b2) const noexcept
        {
            return add(add(multiply(a0, b0), multiply(a1, b1)), multiply(a2, b2));
        }
    };
}

#endif
