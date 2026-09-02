#include <gtest/gtest.h>

#include <components/esm4/aiconditions.hpp>

namespace
{
    ESM4::PackageCondition condition(double expected, bool orWithPrevious = false)
    {
        ESM4::PackageCondition result;
        result.mComparisonValue = static_cast<float>(expected);
        result.mOperator = ESM4::ConditionOperator::GreaterOrEqual;
        result.mOr = orWithPrevious;
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

TEST(ESM4AIConditions, OrFlagGroupsWithThePreviousCondition)
{
    ESM4::ConditionEvaluationContext context;
    context.mResolve = [](const ESM4::PackageCondition& value, ESM4::ConditionSubject) {
        return ESM4::ConditionValue::value(value.mComparisonValue == 1.f ? 0.0 : 10.0);
    };
    const std::vector<ESM4::PackageCondition> conditions{ condition(1), condition(9, true), condition(11) };
    EXPECT_EQ(ESM4::evaluateConditions(conditions, context), ESM4::ConditionResult::False);

    const std::vector<ESM4::PackageCondition> trueConditions{ condition(1), condition(9, true), condition(9) };
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
