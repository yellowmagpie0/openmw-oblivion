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
        Failed,
    };

    struct ConditionValue
    {
        ConditionValueStatus mStatus = ConditionValueStatus::MissingContext;
        double mValue = 0.0;

        static ConditionValue value(double value) { return { ConditionValueStatus::Value, value }; }
        static ConditionValue missing() { return { ConditionValueStatus::MissingContext, 0.0 }; }
        static ConditionValue unsupported() { return { ConditionValueStatus::Unsupported, 0.0 }; }
        // An executed native callback returned failure, before comparison.
        static ConditionValue failed() { return { ConditionValueStatus::Failed, 0.0 }; }
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

    struct NativeConditionFunction
    {
        std::string_view mName;
        bool mHasCallback;
        bool mRequiresSubject;
        bool mDefaultOppositeReference;
    };

    // Actual 1.2.0416 command table properties, not another game's labels or
    // inferred requirements. Values outside the verified table are absent.
    std::optional<NativeConditionFunction> nativeConditionFunction(std::int32_t function);

    struct NativeConditionEvaluationContext
    {
        // The last flag classifies a null first reference/actor parameter:
        // the resolver substitutes the opposite context after subject swap.
        std::function<ConditionValue(const PackageCondition&, ConditionSubject, bool)> mResolve;
        std::function<std::optional<double>(const ESM::FormKey&)> mResolveGlobal;
        bool mHasActor = false;
        bool mHasTarget = false;
    };

    // Original56AC50 narrows the callback's double to float before56A3B0.
    // Unordered comparisons fail equality and pass the other operations;
    // encoded operations6/7 pass after a successful callback.
    bool nativeConditionComparison(double actual, float expected, std::uint8_t operation);
    ConditionResult evaluateNativeCondition(
        const PackageCondition& condition, const NativeConditionEvaluationContext& context);
    ConditionResult evaluateNativeConditions(
        std::span<const PackageCondition> conditions, const NativeConditionEvaluationContext& context);

    std::string_view conditionFunctionName(std::int32_t function);
}

#endif
