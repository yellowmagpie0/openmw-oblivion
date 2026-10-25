#include <gtest/gtest.h>
#include <array>
#include <limits>

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
    EXPECT_EQ(ESM4::conditionFunctionName(254), "GetIsPlayableRace");
    EXPECT_EQ(ESM4::conditionFunctionName(365), "GetPlayerInSEWorld");
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

TEST(ESM4AIConditions, NativeComparisonUsesFloatStoresAndOriginalUnorderedBranches)
{
    // Original56AC50/56A3B0, comparison-corpus.json, both x87 words.
    EXPECT_TRUE(ESM4::nativeConditionComparison(16777217., 16777216.f, 0));
    EXPECT_TRUE(ESM4::nativeConditionComparison(1. + 0x1p-25, 1, 0));
    EXPECT_TRUE(ESM4::nativeConditionComparison(-0., 0, 0));
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto inf = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(ESM4::nativeConditionComparison(nan, 0, 0));
    for (unsigned op = 1; op < 8; ++op)
    {
        EXPECT_TRUE(ESM4::nativeConditionComparison(nan, 0, op));
        EXPECT_TRUE(ESM4::nativeConditionComparison(0, nan, op));
    }
    EXPECT_TRUE(ESM4::nativeConditionComparison(inf, inf, 0));
    EXPECT_TRUE(ESM4::nativeConditionComparison(1e40, inf, 0));
    EXPECT_TRUE(ESM4::nativeConditionComparison(-1e40, -inf, 0));
    EXPECT_FALSE(ESM4::nativeConditionComparison(inf, inf, 2));
    EXPECT_TRUE(ESM4::nativeConditionComparison(-inf, 0, 4));
}

TEST(ESM4AIConditions, NativeDispatchDistinguishesRequiredNullSubjectAndCallbackFailureFromZero)
{
    ESM4::NativeConditionEvaluationContext context;
    context.mHasActor = true;
    unsigned calls = 0;
    bool opposite = false;
    context.mResolve = [&](const auto&, auto subject, bool defaultReference) {
        ++calls;
        EXPECT_EQ(subject, ESM4::ConditionSubject::Target);
        opposite = defaultReference;
        return ESM4::ConditionValue::value(0);
    };
    auto c = condition(0);
    c.mRunOnTarget = true;
    c.mOperator = ESM4::ConditionOperator::Equal;
    c.mFunction = 101;
    EXPECT_EQ(ESM4::evaluateNativeCondition(c, context), ESM4::ConditionResult::False);
    EXPECT_EQ(calls, 0u);
    c.mFunction = 18; // Actual table marks GetCurrentTime subjectless.
    EXPECT_EQ(ESM4::evaluateNativeCondition(c, context), ESM4::ConditionResult::True);
    EXPECT_FALSE(opposite);
    c.mFunction = 53;
    EXPECT_EQ(ESM4::evaluateNativeCondition(c, context), ESM4::ConditionResult::True);
    EXPECT_TRUE(opposite); // Reference argument defaults after subject swap.
    c.mFunction = 79;
    EXPECT_EQ(ESM4::evaluateNativeCondition(c, context), ESM4::ConditionResult::True);
    EXPECT_FALSE(opposite); // Quest argument is not a reference argument.
    c.mFunction = 0;
    const auto before = calls;
    EXPECT_EQ(ESM4::evaluateNativeCondition(c, context), ESM4::ConditionResult::False);
    EXPECT_EQ(calls, before); // Native callback absent, not implementation unavailable.
    c.mFunction = 369;
    EXPECT_EQ(ESM4::evaluateNativeCondition(c, context), ESM4::ConditionResult::Unsupported);
    c.mFunction = 18;
    c.mOperator = ESM4::ConditionOperator::Unknown;
    c.mFlags = 0xe0;
    EXPECT_EQ(ESM4::evaluateNativeCondition(c, context), ESM4::ConditionResult::True);
    context.mResolve = [](const auto&, auto, bool) { return ESM4::ConditionValue::failed(); };
    EXPECT_EQ(ESM4::evaluateNativeCondition(c, context), ESM4::ConditionResult::False);
}

TEST(ESM4AIConditions, NativeGlobalComparisonDistinguishesNullGlobalAndUnavailableResolver)
{
    ESM4::NativeConditionEvaluationContext context;
    context.mHasActor = true;
    context.mResolve = [](const auto&, auto, bool) { return ESM4::ConditionValue::value(0); };
    auto c = condition(123);
    c.mFunction = 101;
    c.mOperator = ESM4::ConditionOperator::Equal;
    c.mUseGlobal = true;
    EXPECT_EQ(ESM4::evaluateNativeCondition(c, context), ESM4::ConditionResult::True);
    c.mComparisonGlobalKey = ESM::FormKey::content("test.esm", 1);
    EXPECT_EQ(ESM4::evaluateNativeCondition(c, context), ESM4::ConditionResult::MissingContext);
    context.mResolveGlobal = [](const auto&) -> std::optional<double> { return std::nullopt; };
    EXPECT_EQ(ESM4::evaluateNativeCondition(c, context), ESM4::ConditionResult::True);
    context.mResolveGlobal = [](const auto&) -> std::optional<double> { return 1; };
    EXPECT_EQ(ESM4::evaluateNativeCondition(c, context), ESM4::ConditionResult::False);
}

TEST(ESM4AIConditions, NativeListPreservesDeferredDispositionCallsAndRejectsUnavailableOrBranches)
{
    ESM4::NativeConditionEvaluationContext context;
    context.mHasActor = true;
    std::vector<int> calls;
    context.mResolve = [&](const auto& c, auto, bool) {
        calls.push_back(c.mFunction);
        return ESM4::ConditionValue::value(0);
    };
    auto disposition = condition(1);
    disposition.mFunction = 76;
    disposition.mOperator = ESM4::ConditionOperator::Greater;
    auto random = condition(1);
    random.mFunction = 77;
    random.mOperator = ESM4::ConditionOperator::Equal;
    EXPECT_EQ(ESM4::evaluateNativeConditions(std::array{disposition, random}, context), ESM4::ConditionResult::False);
    EXPECT_EQ(calls, (std::vector<int>{76, 77}));
    calls.clear();
    EXPECT_EQ(ESM4::evaluateNativeConditions(std::array{random, disposition}, context), ESM4::ConditionResult::False);
    EXPECT_EQ(calls, (std::vector<int>{77}));
    disposition.mOr = true;
    random.mComparisonValue = 0;
    calls.clear();
    EXPECT_EQ(ESM4::evaluateNativeConditions(std::array{disposition, random}, context), ESM4::ConditionResult::True);
    EXPECT_EQ(calls, (std::vector<int>{76, 77}));
    context.mResolve = [](const auto&, auto, bool) { return ESM4::ConditionValue::unsupported(); };
    EXPECT_EQ(ESM4::evaluateNativeConditions(std::array{disposition, random}, context), ESM4::ConditionResult::Unsupported);
    EXPECT_EQ(ESM4::evaluateNativeConditions({}, context), ESM4::ConditionResult::True);
}

TEST(ESM4AIConditions, NativePackageTypesUsePublicMappingAndKnownAbsence)
{
    EXPECT_EQ(ESM4::nativeCurrentAiPackage(std::nullopt).mValue, 0);
    for (const auto& [raw, expected] : std::array<std::pair<std::uint8_t, int>, 8>{
             {{0, 0}, {2, 2}, {5, 13}, {6, 14}, {12, 5}, {32, 32}, {33, -1}, {255, -1}}})
    {
        const auto result = ESM4::nativeCurrentAiPackage(ESM4::NativePackageQueryView{raw, 0, {}, {}});
        EXPECT_EQ(result.mStatus, ESM4::ConditionValueStatus::Value);
        EXPECT_EQ(result.mValue, expected);
    }
}

TEST(ESM4AIConditions, NativeWaitingUsesPackageArrayFlagAndActualProcedure)
{
    EXPECT_EQ(ESM4::nativePackageIsWaiting(std::nullopt).mValue, 0);
    ESM4::NativePackageQueryView package{1, 0, 6, 0}; // [WAIT, FOLLOW, terminal]
    EXPECT_EQ(ESM4::nativePackageIsWaiting(package).mValue, 1);
    package.mProcedureCursor = 1;
    EXPECT_EQ(ESM4::nativePackageIsWaiting(package).mValue, 0);
    package.mProcedureCursor = 2;
    EXPECT_EQ(ESM4::nativePackageIsWaiting(package).mValue, 0);
    package.mFlags = 0x10000;
    package.mProcedureCursor.reset();
    EXPECT_EQ(ESM4::nativePackageIsWaiting(package).mValue, 1);
    package.mProcedureArrayIndex = -1;
    EXPECT_EQ(ESM4::nativePackageIsWaiting(package).mValue, 0); // Gate precedes flag.
    package.mProcedureArrayIndex.reset();
    EXPECT_EQ(ESM4::nativePackageIsWaiting(package).mStatus, ESM4::ConditionValueStatus::Unsupported);
}

TEST(ESM4AIConditions, NativeWaitingCanProveNoWaitWithoutInventingCursor)
{
    auto package = ESM4::nativeCombatControllerPackageView();
    EXPECT_EQ(package.mType, 12);
    EXPECT_EQ(package.mFlags, 6);
    EXPECT_EQ(package.mProcedureArrayIndex, 12);
    EXPECT_FALSE(package.mProcedureCursor);
    EXPECT_EQ(ESM4::nativePackageIsWaiting(package).mStatus, ESM4::ConditionValueStatus::Value);
    EXPECT_EQ(ESM4::nativePackageIsWaiting(package).mValue, 0);
    EXPECT_EQ(ESM4::nativeCurrentAiProcedure(package).mStatus, ESM4::ConditionValueStatus::Unsupported);
    package.mProcedureArrayIndex = 6;
    EXPECT_EQ(ESM4::nativePackageIsWaiting(package).mStatus, ESM4::ConditionValueStatus::Unsupported);
    package.mProcedureArrayIndex = 40;
    EXPECT_EQ(ESM4::nativePackageIsWaiting(package).mStatus, ESM4::ConditionValueStatus::Unsupported);
}

TEST(ESM4AIConditions, NativePublicProcedureMappingRejectsUnprovedOrInvalidProgress)
{
    EXPECT_EQ(ESM4::nativeCurrentAiProcedure(std::nullopt).mValue, 0);
    auto package = ESM4::nativeCombatControllerPackageView();
    package.mProcedureCursor = 0;
    EXPECT_EQ(ESM4::nativeCurrentAiProcedure(package).mValue, 13); // Raw COMBAT9.
    package.mProcedureCursor = 1;
    EXPECT_EQ(ESM4::nativeCurrentAiProcedure(package).mValue, 17); // Raw terminal44.
    package.mProcedureCursor = 2;
    EXPECT_EQ(ESM4::nativeCurrentAiProcedure(package).mStatus, ESM4::ConditionValueStatus::Unsupported);
    package.mProcedureArrayIndex = -1;
    EXPECT_EQ(ESM4::nativeCurrentAiProcedure(package).mStatus, ESM4::ConditionValueStatus::Unsupported);
    package.mProcedureArrayIndex = 6;
    package.mProcedureCursor = 0;
    EXPECT_EQ(ESM4::nativeCurrentAiProcedure(package).mValue, 3); // Raw WAIT13.
    package.mProcedureCursor = 1;
    EXPECT_EQ(ESM4::nativeCurrentAiProcedure(package).mValue, 11); // Raw FOLLOW6.
}
