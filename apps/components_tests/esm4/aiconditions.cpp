#include <gtest/gtest.h>
#include <array>

#include <components/esm4/aiconditions.hpp>

namespace
{
    ESM4::PackageCondition condition(double expected, bool orWithFollowing = false)
    {
        ESM4::PackageCondition result;
        result.mComparisonValue = static_cast<float>(expected);
        result.mOperator = ESM4::ConditionOperator::GreaterOrEqual;
        result.mOr = orWithFollowing;
        return result;
    }
}

TEST(ESM4AIConditions, UsesExplicitSubjectContextAndGlobalComparison)
{
    ESM4::ConditionEvaluationContext context;
    context.mHasTarget = true;
    context.mResolve = [](const ESM4::PackageCondition&, ESM4::ConditionSubject subject) {
        return ESM4::ConditionValue::value(subject == ESM4::ConditionSubject::Target ? 8.0 : 2.0);
    };
    EXPECT_EQ(ESM4::evaluateCondition(condition(7), context), ESM4::ConditionResult::False);
    ESM4::PackageCondition target = condition(7);
    target.mRunOnTarget = true;
    EXPECT_EQ(ESM4::evaluateCondition(target, context), ESM4::ConditionResult::True);

    ESM4::PackageCondition global = condition(8);
    global.mUseGlobal = true;
    global.mComparisonGlobalKey = ESM::FormKey::content("Oblivion.esm", 0x123);
    context.mResolveGlobal = [](const ESM::FormKey&) { return std::optional<double>(8.0); };
    EXPECT_EQ(ESM4::evaluateCondition(global, context), ESM4::ConditionResult::False);
}

TEST(ESM4AIConditions, OrFlagGroupsWithTheFollowingCondition)
{
    ESM4::ConditionEvaluationContext context;
    context.mResolve = [](const ESM4::PackageCondition& value, ESM4::ConditionSubject) {
        return ESM4::ConditionValue::value(value.mComparisonValue == 1.f ? 0.0 : 10.0);
    };
    const std::vector<ESM4::PackageCondition> conditions{ condition(1, true), condition(9), condition(11) };
    EXPECT_EQ(ESM4::evaluateConditions(conditions, context), ESM4::ConditionResult::False);

    const std::vector<ESM4::PackageCondition> trueConditions{ condition(1, true), condition(9), condition(9) };
    EXPECT_EQ(ESM4::evaluateConditions(trueConditions, context), ESM4::ConditionResult::True);
}

TEST(ESM4AIConditions, MissingAndUnsupportedAreNotSilentlyFalse)
{
    ESM4::PackageCondition value = condition(1);
    ESM4::ConditionEvaluationContext context;
    EXPECT_EQ(ESM4::evaluateCondition(value, context), ESM4::ConditionResult::MissingContext);
    context.mResolve = [](const ESM4::PackageCondition&, ESM4::ConditionSubject) {
        return ESM4::ConditionValue::unsupported();
    };
    EXPECT_EQ(ESM4::evaluateCondition(value, context), ESM4::ConditionResult::Unsupported);

    value.mRunOn = ESM4::ConditionRunOn::Unknown;
    EXPECT_EQ(ESM4::evaluateCondition(value, context), ESM4::ConditionResult::Unsupported);
}

TEST(ESM4AIConditions, NamesNativeOblivionFunctions)
{
    EXPECT_EQ(ESM4::conditionFunctionName(45), "GetDetected");
    EXPECT_EQ(ESM4::conditionFunctionName(143), "GetCurrentAIProcedure");
    EXPECT_EQ(ESM4::conditionFunctionName(180), "GetDetectionLevel");
    EXPECT_EQ(ESM4::conditionFunctionName(339), "IsPlayersLastRiddenHorse");
    EXPECT_EQ(ESM4::conditionFunctionName(358), "IsPlayerMovingIntoNewSpace");
    EXPECT_TRUE(ESM4::conditionFunctionName(999).empty());
}

TEST(ESM4AIConditions, NativeAndFailureAndTrueOrGroupSkipUnusedPredicates)
{
    std::vector<int> visited;
    ESM4::ConditionEvaluationContext context;
    context.mResolve = [&](const ESM4::PackageCondition& value, ESM4::ConditionSubject) {
        visited.push_back(value.mFunction);
        return ESM4::ConditionValue::value(value.mFunction == 0 ? 1 : 0);
    };
    std::array<ESM4::PackageCondition, 3> conditions{condition(1), condition(1), condition(1)};
    for (int i = 0; i < 3; ++i) conditions[i].mFunction = i;
    EXPECT_EQ(ESM4::evaluateConditions(conditions, context), ESM4::ConditionResult::False);
    EXPECT_EQ(visited, (std::vector{0, 1}));
    visited.clear();
    conditions[0].mOr = true;
    EXPECT_EQ(ESM4::evaluateConditions(conditions, context), ESM4::ConditionResult::False);
    EXPECT_EQ(visited, (std::vector{0, 2}));
    visited.clear();
    conditions[1].mOr = true;
    EXPECT_EQ(ESM4::evaluateConditions(conditions, context), ESM4::ConditionResult::True);
    EXPECT_EQ(visited, (std::vector{0}));
    conditions[2].mOr = true; // Native accepts a trailing open OR group.
    visited.clear();
    EXPECT_EQ(ESM4::evaluateConditions(conditions, context), ESM4::ConditionResult::True);
    EXPECT_EQ(visited, (std::vector{0}));
    EXPECT_EQ(ESM4::evaluateConditions({}, context), ESM4::ConditionResult::True);
}

TEST(ESM4AIConditions, DiagnosticStatusesPropagateOnlyThroughVisitedOrGroups)
{
    ESM4::ConditionEvaluationContext context;
    context.mResolve = [](const ESM4::PackageCondition& value, ESM4::ConditionSubject) {
        if (value.mFunction == 0) return ESM4::ConditionValue::unsupported();
        if (value.mFunction == 1) return ESM4::ConditionValue::missing();
        return ESM4::ConditionValue::value(1);
    };
    std::array<ESM4::PackageCondition, 3> conditions{condition(1, true), condition(1, true), condition(1)};
    for (int i = 0; i < 3; ++i) conditions[i].mFunction = i;
    EXPECT_EQ(ESM4::evaluateConditions(conditions, context), ESM4::ConditionResult::True);
    conditions[1].mOr = false;
    EXPECT_EQ(ESM4::evaluateConditions(conditions, context), ESM4::ConditionResult::Unsupported);
    conditions[0].mFunction = 1;
    EXPECT_EQ(ESM4::evaluateConditions(conditions, context), ESM4::ConditionResult::MissingContext);
    conditions[0].mFunction = 2;
    EXPECT_EQ(ESM4::evaluateConditions(conditions, context), ESM4::ConditionResult::True);
}
