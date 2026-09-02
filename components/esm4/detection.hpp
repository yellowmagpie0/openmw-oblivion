/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#ifndef OPENMW_COMPONENTS_ESM4_DETECTION_H
#define OPENMW_COMPONENTS_ESM4_DETECTION_H

#include <cstdint>

namespace ESM4
{
    // Inputs are deliberately scalar and side-effect free. World code supplies
    // the raycast/light/noise observations; this component never reaches into
    // a world singleton and never consumes a random generator.
    struct DetectionInput
    {
        double mObserverSneak = 0.0;
        double mObserverAgility = 0.0;
        double mObserverLuck = 0.0;
        double mTargetSneak = 0.0;
        double mDistance = 0.0;
        double mMaximumDistance = 4096.0;
        double mViewAngleRadians = 0.0;
        double mViewConeRadians = 1.5707963267948966;
        double mAmbientLight = 1.0;
        double mTargetIllumination = 1.0;
        double mMovementSpeed = 0.0;
        double mFootwearNoise = 0.0;
        double mArmorNoise = 0.0;
        double mInvisibility = 0.0;
        double mChameleon = 0.0;
        double mSoundModifier = 0.0;
        bool mLineOfSight = true;
        bool mTargetSleeping = false;

        // Profile-owned coefficients. They are initialized to neutral values
        // so fixtures can state only the factors they need to exercise.
        double mDistanceWeight = 0.35;
        double mAngleWeight = 0.25;
        double mLightWeight = 0.20;
        double mMovementWeight = 0.20;
        double mTargetSneakWeight = 0.45;
        double mAbilityWeight = 0.55;
        double mEffectWeight = 0.60;
        double mThreshold = 50.0;
        double mRandomSample = 0.5;
    };

    struct DetectionResult
    {
        double mLevel = 0.0; // [0, 100], suitable for GetDetectionLevel
        bool mDetected = false; // threshold/roll result for GetDetected
        bool mValid = true;

        friend bool operator==(const DetectionResult&, const DetectionResult&) = default;
    };

    DetectionResult calculateDetection(const DetectionInput& input);
}

#endif
