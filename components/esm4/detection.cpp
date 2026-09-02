/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#include "detection.hpp"

#include <algorithm>
#include <cmath>

namespace ESM4
{
    namespace
    {
        constexpr double clamp01(double value)
        {
            return std::clamp(value, 0.0, 1.0);
        }

        bool finite(const DetectionInput& input)
        {
            return std::isfinite(input.mObserverSneak) && std::isfinite(input.mObserverAgility)
                && std::isfinite(input.mObserverLuck) && std::isfinite(input.mTargetSneak)
                && std::isfinite(input.mDistance) && std::isfinite(input.mMaximumDistance)
                && std::isfinite(input.mViewAngleRadians) && std::isfinite(input.mViewConeRadians)
                && std::isfinite(input.mAmbientLight) && std::isfinite(input.mTargetIllumination)
                && std::isfinite(input.mMovementSpeed) && std::isfinite(input.mFootwearNoise)
                && std::isfinite(input.mArmorNoise) && std::isfinite(input.mInvisibility)
                && std::isfinite(input.mChameleon) && std::isfinite(input.mSoundModifier)
                && std::isfinite(input.mDistanceWeight) && std::isfinite(input.mAngleWeight)
                && std::isfinite(input.mLightWeight) && std::isfinite(input.mMovementWeight)
                && std::isfinite(input.mTargetSneakWeight) && std::isfinite(input.mAbilityWeight)
                && std::isfinite(input.mEffectWeight) && std::isfinite(input.mThreshold)
                && std::isfinite(input.mRandomSample);
        }
    }

    DetectionResult calculateDetection(const DetectionInput& input)
    {
        if (!finite(input) || input.mMaximumDistance <= 0.0 || input.mViewConeRadians <= 0.0)
            return { 0.0, false, false };
        if (!input.mLineOfSight || input.mDistance < 0.0 || std::abs(input.mViewAngleRadians) > input.mViewConeRadians)
            return { 0.0, false, true };

        const double distance = clamp01(1.0 - input.mDistance / input.mMaximumDistance);
        const double angle = clamp01(1.0 - std::abs(input.mViewAngleRadians) / input.mViewConeRadians);
        const double light = clamp01(input.mAmbientLight) * clamp01(input.mTargetIllumination);
        const double ability = clamp01((input.mObserverSneak + input.mObserverAgility + input.mObserverLuck) / 300.0);
        const double targetConcealment = clamp01(input.mTargetSneak / 100.0);
        const double movement = clamp01(input.mMovementSpeed / 300.0 + input.mFootwearNoise + input.mArmorNoise);
        const double effects = clamp01(input.mInvisibility + input.mChameleon);
        const double sound = clamp01(input.mSoundModifier);

        // Each term is normalized before weighting. The ability and
        // concealment terms are complementary, which makes the monotonicity
        // expected by scripts and package conditions explicit.
        const double weighted = input.mDistanceWeight * distance + input.mAngleWeight * angle
            + input.mLightWeight * light + input.mMovementWeight * movement;
        const double base = 100.0 * ((1.0 - input.mAbilityWeight) * weighted + input.mAbilityWeight * ability);
        const double concealment = 1.0 - input.mTargetSneakWeight * targetConcealment;
        const double effect = 1.0 - input.mEffectWeight * effects;
        const double sleepFactor = input.mTargetSleeping ? 1.15 : 1.0;
        const double score = std::clamp(base * concealment * effect * sleepFactor + sound * 10.0, 0.0, 100.0);
        const double threshold = std::clamp(input.mThreshold, 0.0, 100.0);
        const double sample = std::clamp(input.mRandomSample, 0.0, 1.0);
        return { score, score >= threshold && sample <= score / 100.0, true };
    }
}
