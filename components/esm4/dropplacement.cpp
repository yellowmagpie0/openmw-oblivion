#include "dropplacement.hpp"

#include <components/nif/niftypes.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ESM4
{
    float droppedReferenceScale(std::optional<float> sourceExtraScale)
    {
        if (!sourceExtraScale) return 1.f;
        const float scale = *sourceExtraScale;
        if (!std::isfinite(scale))
            throw std::invalid_argument("Nonfinite dropped-reference source scale");
        if (scale <= .01f) return .01f;
        if (scale >= 10.f) return 10.f;
        // For this bounded binary32 input, multiplication by 100 and addition
        // of 0.5 are exact in binary64. Original CRT decimal formatting rounds
        // positive half cents up, unlike printf implementations using ties-even.
        // Dividing the rounded integer reproduces the parsed binary32 value.
        return std::max(.01f, static_cast<float>(std::floor(double(scale) * 100. + .5) / 100.));
    }

    namespace
    {
        constexpr float halfPi = 1.57079637050628662109375f;
        constexpr float pi = 3.1415927410125732421875f;

        // Original 007070B0 explicitly stores each polynomial operation to
        // float. Double expressions retain the native coefficient precision.
        float store(double value) { return static_cast<float>(value); }

        float nativeAtan(float y, float x)
        {
            if (x == 0 && y == 0) return 0;
            const bool reciprocal = std::abs(x) < std::abs(y);
            const float ratio = reciprocal ? store(double(x) / y) : store(double(y) / x);
            if (ratio == 0)
                return reciprocal ? (y < 0 ? -halfPi : halfPi) : (x < 0 ? pi : 0.f);
            const float square = store(double(ratio) * ratio);
            float value = store(0.02083509974181652 * square);
            value = store(double(value) - 0.08513300120830536);
            value = store(double(value) * square);
            value = store(double(value) + 0.18014100193977356);
            value = store(double(value) * square);
            value = store(double(value) - 0.3302994966506958);
            value = store(double(square) * value);
            value = store(double(value) + 0.9998660087585449);
            value = store(double(ratio) * value);
            if (reciprocal) value = store(double(ratio > 0 ? halfPi : -halfPi) - value);
            if (x < 0) value = y < 0 ? store(double(value) - pi) : store(double(pi) + value);
            return value;
        }

        void validateRotation(const std::array<float, 9>& rotation)
        {
            for (float value : rotation)
                if (!std::isfinite(value)) throw std::invalid_argument("Nonfinite native drop rotation");
        }
    }

    DroppedItemPlacement droppedItemPlacement(
        const std::array<float, 9>& rotation, const std::array<float, 3>& translation)
    {
        validateRotation(rotation);
        for (float value : translation)
            if (!std::isfinite(value)) throw std::invalid_argument("Nonfinite native drop position");
        DroppedItemPlacement result;
        std::copy(translation.begin(), translation.end(), result.mPosition.pos);
        auto& angles = result.mPosition.rot;
        const float sine = rotation[7];
        angles[0] = sine <= -1 ? halfPi : sine >= 1 ? -halfPi : -store(std::asin(double(sine)));
        result.mUniqueRotation = angles[0] > -halfPi && angles[0] < halfPi;
        if (result.mUniqueRotation)
        {
            angles[1] = -nativeAtan(-rotation[6], rotation[8]);
            angles[2] = -nativeAtan(-rotation[1], rotation[4]);
        }
        else
        {
            angles[1] = 0;
            const float angle = nativeAtan(rotation[2], rotation[0]);
            angles[2] = angles[0] <= -halfPi ? store(double(angle) - 0.0) : store(0.0 - double(angle));
        }
        return result;
    }

    DroppedItemPlacement droppedItemPlacement(const Nif::NiTransform& worldTransform)
    {
        std::array<float, 9> rotation;
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned column = 0; column < 3; ++column)
                rotation[row * 3 + column] = worldTransform.mRotation.mValues[row][column];
        return droppedItemPlacement(rotation, {worldTransform.mTranslation.x(),
            worldTransform.mTranslation.y(), worldTransform.mTranslation.z()});
    }
}
