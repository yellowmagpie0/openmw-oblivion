#include "referenceplacement.hpp"

#include "placementsincos.hpp"

#include <components/nif/nativetransformmath.hpp>

#include <cmath>

namespace ESM4
{
    std::optional<Nif::NiTransform> referencePlacementRoot(const ESM::Position& position,
        float referenceScale, Nif::NativeTransformPrecision precision) noexcept
    {
        if (!Nif::Detail::NativeTransformMath(precision).valid() || !std::isfinite(referenceScale))
            return std::nullopt;
        for (float coordinate : position.pos)
            if (!std::isfinite(coordinate)) return std::nullopt;
        for (float angle : position.rot)
            if (!std::isfinite(angle) || std::abs(double(angle)) >= 0x1p63) return std::nullopt;
        auto result = Nif::NiTransform::getIdentity();
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            auto rotation = Nif::NiTransform::getIdentity();
            const auto [sine, cosine] = Detail::placementSinCos(position.rot[axis]);
            const unsigned next = (axis + 1) % 3, last = (axis + 2) % 3;
            rotation.mRotation.mValues[next][next] = rotation.mRotation.mValues[last][last] = cosine;
            rotation.mRotation.mValues[next][last] = sine;
            rotation.mRotation.mValues[last][next] = -sine;
            // Retain each actual7100A0 product, including the initial identity
            // multiplication and its float stores/signed-zero behavior.
            const auto composed = Nif::tryComposeNativeTransform(result, rotation, precision);
            if (!composed) return std::nullopt;
            result = *composed;
        }
        result.mTranslation = position.asVec3();
        result.mScale = std::abs(referenceScale);
        return result;
    }
}
