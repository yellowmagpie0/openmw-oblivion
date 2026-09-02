#include <gtest/gtest.h>

#include <components/esm4/aiselection.hpp>

#include <cstdint>

namespace
{
    ESM::FormKey key(std::uint32_t id)
    {
        return ESM::FormKey::content("Oblivion.esm", id);
    }

    ESM4::PackageCandidate package(std::uint32_t id, std::size_t index)
    {
        ESM4::PackageCandidate result;
        result.mKey = key(id);
        result.mType = ESM4::AIPackageType::Travel;
        result.mListIndex = index;
        return result;
    }
}

TEST(ESM4AISelection, KeepsDeclaredPriorityAndHasTypedIdleState)
{
    ESM4::PackageSelectionRequest request;
    request.mBasePackages = { package(2, 0), package(1, 1) };
    request.mEvaluationGeneration = 7;
    request.mConditionContext.mResolve = [](const ESM4::PackageCondition&, ESM4::ConditionSubject) {
        return ESM4::ConditionValue::value(1.0);
    };
    const auto selected = ESM4::selectPackage(request);
    EXPECT_EQ(selected.mPackage, key(2));
    EXPECT_EQ(selected.mListIndex, 0u);
    EXPECT_EQ(selected.mEvaluationGeneration, 7u);

    request.mBasePackages.clear();
    const auto idle = ESM4::selectPackage(request);
    EXPECT_FALSE(idle.hasPackage());
    EXPECT_EQ(idle.mType, ESM4::AIPackageType::Unknown);
}

TEST(ESM4AISelection, ScriptPackageWinsOnlyWhenValid)
{
    ESM4::PackageSelectionRequest request;
    request.mBasePackages = { package(2, 3) };
    request.mScriptPackage = package(3, 99);
    ESM4::PackageCondition condition;
    condition.mOperator = ESM4::ConditionOperator::Equal;
    condition.mComparisonValue = 1.0f;
    request.mScriptPackage->mConditions.push_back(condition);
    request.mConditionContext.mResolve = [](const ESM4::PackageCondition&, ESM4::ConditionSubject) {
        return ESM4::ConditionValue::value(1.0);
    };
    EXPECT_EQ(ESM4::selectPackage(request).mSource, ESM4::PackageSource::Script);

    request.mConditionContext.mResolve = [](const ESM4::PackageCondition&, ESM4::ConditionSubject) {
        return ESM4::ConditionValue::value(0.0);
    };
    EXPECT_EQ(ESM4::selectPackage(request).mPackage, key(2));
}
