#include "nativetransform.hpp"
#include "nativetransformmath.hpp"

#include <cmath>
#include <limits>

namespace Nif
{
    namespace
    {
        bool finite(const NiTransform& value) noexcept
        {
            if (!std::isfinite(value.mScale)) return false;
            for (unsigned row = 0; row < 3; ++row)
            {
                if (!std::isfinite(value.mTranslation[row])) return false;
                for (float entry : value.mRotation.mValues[row])
                    if (!std::isfinite(entry)) return false;
            }
            return true;
        }

    }

    bool tryStoreNativeTransformFloat(double value, float& destination) noexcept
    {
        const double magnitude = std::abs(value);
        // Above the finite maximum, half an ULP remains before the native
        // nearest-even store overflows. Avoid an out-of-range C++ conversion.
        if (!std::isfinite(value) || magnitude >= 0x1.ffffffp+127) return false;
        constexpr float maximum = std::numeric_limits<float>::max();
        destination = magnitude > maximum ? (value < 0 ? -maximum : maximum) : static_cast<float>(value);
        return true;
    }

    std::optional<NiTransform> tryComposeNativeTransform(
        const NiTransform& parent, const NiTransform& local, NativeTransformPrecision precision) noexcept
    {
        const Detail::NativeTransformMath math(precision);
        if (!math.valid() || !finite(parent) || !finite(local)) return std::nullopt;
        NiTransform result;
        if (!tryStoreNativeTransformFloat(math.multiply(parent.mScale, local.mScale), result.mScale))
            return std::nullopt;
        for (unsigned row = 0; row < 3; ++row)
        {
            const auto& axis = parent.mRotation.mValues[row];
            for (unsigned column = 0; column < 3; ++column)
                if (!tryStoreNativeTransformFloat(math.dot(axis[0], local.mRotation.mValues[0][column],
                        axis[1], local.mRotation.mValues[1][column], axis[2], local.mRotation.mValues[2][column]),
                        result.mRotation.mValues[row][column]))
                    return std::nullopt;
            float rotated, scaled;
            if (!tryStoreNativeTransformFloat(math.dot(axis[0], local.mTranslation[0],
                    axis[1], local.mTranslation[1], axis[2], local.mTranslation[2]), rotated)
                || !tryStoreNativeTransformFloat(math.multiply(rotated, parent.mScale), scaled)
                || !tryStoreNativeTransformFloat(math.add(scaled, parent.mTranslation[row]), result.mTranslation[row]))
                return std::nullopt;
        }
        return result;
    }
}
