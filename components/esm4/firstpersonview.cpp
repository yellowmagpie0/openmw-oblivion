#include "firstpersonview.hpp"
#include "placementsincos.hpp"

#include <components/nif/nativetransform.hpp>
#include <components/nif/nativetransformmath.hpp>

#include <algorithm>
#include <cmath>

namespace ESM4
{
    std::optional<FirstPersonRootRotation> updateFirstPersonRootRotation(
        const FirstPersonRootInput& input, const FirstPersonViewSettings& settings) noexcept
    {
        for (float value : {input.mPreviousFollow, input.mElapsed, input.mPitch, input.mYaw,
                 input.mSecondaryYaw, settings.mHandFollowMultiplier, settings.mScaleSeconds})
            if (!std::isfinite(value))
                return std::nullopt;
        if (input.mElapsed < 0 || settings.mScaleSeconds <= 0
            || std::abs(double(input.mPitch)) >= 0x1p63)
            return std::nullopt;

        constexpr auto precision = Nif::NativeTransformPrecision::Single;
        const Nif::Detail::NativeTransformMath math(precision);
        const auto store = [](double value, float& result) {
            return Nif::tryStoreNativeTransformFloat(value, result);
        };
        const auto group1 = input.mGroup1 & 255, group3 = input.mGroup3 & 255;
        const float target = input.mProcessAction != 3
                && ((group3 >= 17 && group3 <= 26) || (group1 >= 34 && group1 <= 39))
            ? 1.f : settings.mHandFollowMultiplier;
        float follow = input.mPreviousFollow;
        if (follow != target)
        {
            const float duration = target == 1.f ? .1f : settings.mScaleSeconds;
            float delta;
            if (!store(math.multiply(math.add(target, -double(follow)),
                    math.divide(input.mElapsed, duration)), delta)
                || !store(math.add(follow, delta), follow))
                return std::nullopt;
            follow = target > input.mPreviousFollow ? std::min(follow, target) : std::max(follow, target);
        }

        float yaw;
        if (!store(math.add(input.mYaw, input.mSecondaryYaw), yaw))
            return std::nullopt;
        // Actual Player65DA60/65ABB0 heading read and one wrap, not fmod.
        constexpr double circle = 6.283185307179586;
        if (yaw < 0)
        {
            if (!store(math.add(yaw, circle), yaw))
                return std::nullopt;
        }
        else if (double(yaw) > circle && !store(math.add(yaw, -circle), yaw))
            return std::nullopt;
        if (std::abs(double(yaw)) >= 0x1p63)
            return std::nullopt;

        auto heading = Nif::NiTransform::getIdentity();
        const auto [sy, cy] = Detail::placementSinCos(yaw);
        heading.mRotation.mValues[0][0] = heading.mRotation.mValues[1][1] = cy;
        heading.mRotation.mValues[0][1] = sy;
        heading.mRotation.mValues[1][0] = -sy;
        const auto pitchRotation = [](float angle) {
            auto result = Nif::NiTransform::getIdentity();
            const auto [s, c] = Detail::placementSinCos(angle);
            result.mRotation.mValues[1][1] = result.mRotation.mValues[2][2] = c;
            result.mRotation.mValues[1][2] = s;
            result.mRotation.mValues[2][1] = -s;
            return result;
        };
        const auto base = Nif::tryComposeNativeTransform(heading, pitchRotation(input.mPitch), precision);
        if (!base)
            return std::nullopt;
        auto direction = Nif::NiTransform::getIdentity();
        direction.mTranslation.set(0, 1, 0);
        const auto projected = Nif::tryComposeNativeTransform(*base, direction, precision);
        if (!projected)
            return std::nullopt;
        const auto& forward = projected->mTranslation;
        float dot;
        if (!store(math.dot(forward.x(), forward.x(), forward.y(), forward.y(), forward.z(), 0.f), dot))
            return std::nullopt;
        dot = std::clamp(dot, -1.f, 1.f);
        const float angle = static_cast<float>(std::acos(double(dot)));
        float correction;
        if (!store(math.multiply(angle, math.add(1., -double(follow))), correction))
            return std::nullopt;
        if (forward.z() < 0)
            correction = -correction;
        if (std::abs(double(correction)) >= 0x1p63)
            return std::nullopt;
        const auto result = Nif::tryComposeNativeTransform(*base, pitchRotation(correction), precision);
        if (!result)
            return std::nullopt;
        return FirstPersonRootRotation{follow, *result};
    }
}
