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
#include <limits>
#include <stdexcept>

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

    void validateNativeDetectionSettings(const NativeDetectionSettings& settings)
    {
        for (float value : {settings.mMaximumDistance, settings.mExteriorDistanceMultiplier,
                 settings.mBootWeightBase, settings.mBootWeightMultiplier, settings.mTargetCombatBonus,
                 settings.mRunningMultiplier, settings.mSoundWithoutLosMultiplier, settings.mSoundMultiplier,
                 settings.mLightOffset, settings.mLightMultiplier, settings.mSkillMultiplier,
                 settings.mTargetAttackBonus, settings.mSwimmingLightMultiplier, settings.mSleepBonus, settings.mBase})
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native detection setting");
        if (settings.mMaximumDistance <= 0 || settings.mExteriorDistanceMultiplier <= 0)
            throw std::invalid_argument("invalid native detection distance divisor");
    }

    std::int32_t nativeDetectionAwareness(const NativeDetectionInput& input, const NativeDetectionSettings& settings)
    {
        validateNativeDetectionSettings(settings);
        if (!std::isfinite(input.mDistance) || input.mDistance < 0 || input.mBootWeight < 0
            || input.mObserverBlindness < 0 || input.mTargetChameleon < 0 || input.mTargetChameleon > 100)
            throw std::invalid_argument("invalid native detection input");
        const auto stored = [](double value) {
            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
                throw std::invalid_argument("native detection arithmetic overflow");
            return static_cast<float>(value);
        };
        const float maximum = input.mExterior
            ? stored(double(settings.mMaximumDistance) * settings.mExteriorDistanceMultiplier)
            : settings.mMaximumDistance;
        if (maximum <= 0)
            throw std::invalid_argument("native detection distance underflow");
        if (input.mDistance > maximum && !input.mTargetAttacking)
            return 0;
        const float distance = stored((double(maximum) - input.mDistance) / maximum);
        const float boots = input.mTargetMoving
            ? stored(double(input.mBootWeight) * settings.mBootWeightMultiplier + settings.mBootWeightBase) : 0.f;
        const float running = input.mTargetRunning ? settings.mRunningMultiplier : 1.f;
        const float combat = input.mTargetInCombat ? settings.mTargetCombatBonus : 0.f;
        const float soundLos = input.mLineOfSight ? 1.f : settings.mSoundWithoutLosMultiplier;
        float sound = std::max(0.f,
            stored((double(running) * boots + combat) * distance * soundLos * settings.mSoundMultiplier));
        float light = std::max(0.f, stored((double(input.mTargetLight) + settings.mLightOffset)
            * (input.mLineOfSight ? double(distance) : 0.0)
            * (100.0 - input.mObserverBlindness) / 100.0
            * ((100.0 - input.mTargetChameleon) / 100.0) * settings.mLightMultiplier));
        const float skill = stored((double(std::min(input.mObserverSneak, 100)) * distance
            - (input.mTargetSneaking ? double(std::min(input.mTargetSneak, 100)) : 0.0)) * settings.mSkillMultiplier);
        const float attack = input.mTargetAttacking ? settings.mTargetAttackBonus : 0.f;
        if (input.mObserverUnderwater)
        {
            sound = 0;
            light = stored(double(settings.mSwimmingLightMultiplier) * light);
        }
        if (input.mObserverSleeping)
            light = settings.mSleepBonus;
        const float total = stored(double(settings.mBase) + sound + light + attack + skill);
        if (double(total) < std::numeric_limits<std::int32_t>::min()
            || double(total) > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("native detection score integer overflow");
        return total > 0 && total < 1 ? 1 : static_cast<std::int32_t>(total);
    }

    SneakDetectionNoise sneakDetectionNoise(std::int32_t baseSneak, bool sneaking,
        std::int32_t bootWeight, bool moving, bool running, const CombatMasterySettings& mastery)
    {
        if (bootWeight < 0)
            throw std::invalid_argument("negative native detection boot weight");
        const auto rank = combatMastery(baseSneak, mastery);
        if (sneaking && rank >= CombatMastery::Journeyman)
            bootWeight = 0;
        if (sneaking && rank >= CombatMastery::Expert)
            moving = running = false;
        return {bootWeight, moving, running};
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
