/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#ifndef OPENMW_COMPONENTS_ESM4_AICONDITIONS_H
#define OPENMW_COMPONENTS_ESM4_AICONDITIONS_H

#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "aipackagedata.hpp"

namespace ESM4
{
    enum class ConditionSubject : std::uint8_t
    {
        Self,
        Target,
        Reference,
        Player,
        CombatTarget,
        LinkedReference,
    };

    enum class ConditionValueStatus : std::uint8_t
    {
        Value,
        MissingContext,
        Unsupported,
    };

    struct ConditionValue
    {
        ConditionValueStatus mStatus = ConditionValueStatus::MissingContext;
        double mValue = 0.0;

        static ConditionValue value(double value) { return { ConditionValueStatus::Value, value }; }
        static ConditionValue missing() { return { ConditionValueStatus::MissingContext, 0.0 }; }
        static ConditionValue unsupported() { return { ConditionValueStatus::Unsupported, 0.0 }; }
    };

    struct ConditionEvaluationContext
    {
        std::function<ConditionValue(const PackageCondition&, ConditionSubject)> mResolve;
        std::function<std::optional<double>(const ESM::FormKey&)> mResolveGlobal;
        bool mHasTarget = false;
        bool mHasReference = false;
        bool mHasPlayer = false;
        bool mHasCombatTarget = false;
        bool mHasLinkedReference = false;
    };

    enum class ConditionResult : std::uint8_t
    {
        True,
        False,
        MissingContext,
        Unsupported,
    };

    ConditionResult evaluateCondition(
        const PackageCondition& condition, const ConditionEvaluationContext& context);

    ConditionResult evaluateConditions(
        std::span<const PackageCondition> conditions, const ConditionEvaluationContext& context);

    std::string_view conditionFunctionName(std::int32_t function);
}

#endif
