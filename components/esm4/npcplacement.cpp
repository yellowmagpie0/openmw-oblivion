#include "npcplacement.hpp"
#include "placementsincos.hpp"

#include <components/nif/nativetransform.hpp>
#include <components/nif/nativetransformmath.hpp>
#include <cmath>

namespace ESM4
{
    std::optional<float> creaturePlacementScale(
        float referenceScale, float baseScale, Nif::NativeTransformPrecision precision) noexcept
    {
        const Nif::Detail::NativeTransformMath math(precision);
        if (!math.valid() || !std::isfinite(referenceScale) || !std::isfinite(baseScale))
            return std::nullopt;
        float product;
        if (!Nif::tryStoreNativeTransformFloat(math.multiply(baseScale, referenceScale), product))
            return std::nullopt;
        return std::abs(product);
    }

    std::optional<Nif::NiTransform> creaturePlacementRoot(const ESM::Position& position,
        float referenceScale, float baseScale, Nif::NativeTransformPrecision precision) noexcept
    {
        const auto scale = creaturePlacementScale(referenceScale, baseScale, precision);
        if (!scale) return std::nullopt;
        return npcPlacementRoot(position, *scale, std::nullopt);
    }

    std::optional<Nif::NiTransform> npcPlacementRoot(const ESM::Position& position,
        float referenceScale, std::optional<NpcPlacementMorphology> morphology) noexcept
    {
        if (!std::isfinite(referenceScale) || !std::isfinite(position.rot[2])
            || std::abs(double(position.rot[2])) >= 0x1p63)
            return std::nullopt;
        for (float coordinate : position.pos)
            if (!std::isfinite(coordinate)) return std::nullopt;
        auto yaw = Nif::NiTransform::getIdentity();
        const auto [sine, cosine] = Detail::placementSinCos(position.rot[2]);
        yaw.mRotation.mValues[0][0] = yaw.mRotation.mValues[1][1] = cosine;
        yaw.mRotation.mValues[0][1] = sine;
        yaw.mRotation.mValues[1][0] = -sine;
        auto result = Nif::tryComposeNativeTransform(Nif::NiTransform::getIdentity(), yaw);
        if (!result) return std::nullopt;
        if (morphology)
        {
            auto race = Nif::NiTransform::getIdentity();
            race.mRotation.mValues[0][0] = race.mRotation.mValues[1][1] = morphology->mWeight;
            race.mRotation.mValues[2][2] = morphology->mHeight;
            result = Nif::tryComposeNativeTransform(*result, race);
            if (!result) return std::nullopt;
        }
        result->mTranslation = position.asVec3();
        // GenerateNiNode4E43DA applies FABS after the generic scale getter.
        result->mScale = std::abs(referenceScale);
        return result;
    }
}
