#include "npcplacement.hpp"

#include <components/nif/nativetransform.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <bit>
#include <cmath>
#include <cstdint>

namespace ESM4
{
    namespace
    {
        std::pair<float, float> nativeYawSinCos(float angle) noexcept
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

    std::optional<Nif::NiTransform> npcPlacementRoot(const ESM::Position& position,
        float referenceScale, std::optional<NpcPlacementMorphology> morphology) noexcept
    {
        if (!std::isfinite(referenceScale) || !std::isfinite(position.rot[2])
            || std::abs(double(position.rot[2])) >= 0x1p63)
            return std::nullopt;
        for (float coordinate : position.pos)
            if (!std::isfinite(coordinate)) return std::nullopt;
        auto yaw = Nif::NiTransform::getIdentity();
        const auto [sine, cosine] = nativeYawSinCos(position.rot[2]);
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
