/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#include "aiselection.hpp"

#include <utility>

namespace ESM4
{
    namespace
    {
        bool eligible(const PackageCandidate& package, const PackageSelectionRequest& request,
            ConditionResult& conditionResult, std::optional<ScheduleWindow>& window)
        {
            window = package.mSchedule.activeWindow(request.mNow);
            if (!window)
                return false;
            conditionResult = evaluateConditions(package.mConditions, request.mConditionContext);
            return conditionResult == ConditionResult::True;
        }

        PackageSelection makeSelection(const PackageCandidate& package, PackageSource source,
            const PackageSelectionRequest& request, ConditionResult conditionResult,
            std::optional<ScheduleWindow> window)
        {
            return { source, package.mKey, package.mType, package.mListIndex, request.mEvaluationGeneration,
                conditionResult, std::move(window) };
        }
    }

    PackageSelection selectPackage(const PackageSelectionRequest& request)
    {
        // A transient script package has precedence, but an invalid/unknown
        // transient package is not allowed to suppress a valid base schedule.
        if (request.mScriptPackage)
        {
            ConditionResult result = ConditionResult::False;
            std::optional<ScheduleWindow> window;
            if (eligible(*request.mScriptPackage, request, result, window))
                return makeSelection(*request.mScriptPackage, PackageSource::Script, request, result,
                    std::move(window));
        }

        for (const PackageCandidate& package : request.mBasePackages)
        {
            ConditionResult result = ConditionResult::False;
            std::optional<ScheduleWindow> window;
            if (eligible(package, request, result, window))
                return makeSelection(package, PackageSource::Base, request, result, std::move(window));
        }

        PackageSelection idle;
        idle.mEvaluationGeneration = request.mEvaluationGeneration;
        idle.mConditionResult = ConditionResult::False;
        return idle;
    }
}
