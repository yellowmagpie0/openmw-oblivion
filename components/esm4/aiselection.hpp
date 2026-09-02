/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#ifndef OPENMW_COMPONENTS_ESM4_AISELECTION_H
#define OPENMW_COMPONENTS_ESM4_AISELECTION_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "aiconditions.hpp"

namespace ESM4
{
    enum class PackageSource : std::uint8_t
    {
        None,
        Base,
        Script,
    };

    struct PackageCandidate
    {
        ESM::FormKey mKey;
        AIPackageType mType = AIPackageType::Unknown;
        PackageSchedule mSchedule;
        std::vector<PackageCondition> mConditions;
        std::size_t mListIndex = 0;
    };

    struct PackageSelectionRequest
    {
        CalendarInstant mNow;
        std::vector<PackageCandidate> mBasePackages;
        std::optional<PackageCandidate> mScriptPackage;
        ConditionEvaluationContext mConditionContext;
        std::uint64_t mEvaluationGeneration = 0;
    };

    struct PackageSelection
    {
        PackageSource mSource = PackageSource::None;
        ESM::FormKey mPackage;
        AIPackageType mType = AIPackageType::Unknown;
        std::size_t mListIndex = 0;
        std::uint64_t mEvaluationGeneration = 0;
        ConditionResult mConditionResult = ConditionResult::False;
        std::optional<ScheduleWindow> mWindow;

        bool hasPackage() const { return mSource != PackageSource::None && !mPackage.isNull(); }
    };

    PackageSelection selectPackage(const PackageSelectionRequest& request);
}

#endif
