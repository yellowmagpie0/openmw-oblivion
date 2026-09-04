#include <gtest/gtest.h>

#include <components/esm4/aiphase.hpp>

namespace
{
    ESM4::PackageSelection selection(ESM4::AIPackageType type)
    {
        ESM4::PackageSelection result;
        result.mSource = ESM4::PackageSource::Base;
        result.mPackage = ESM::FormKey::content("Oblivion.esm", 1);
        result.mType = type;
        result.mListIndex = 2;
        result.mEvaluationGeneration = 4;
        return result;
    }
}

TEST(ESM4AIPhase, MapsAllNativePackageProcedures)
{
    EXPECT_EQ(ESM4::packageProcedure(ESM4::AIPackageType::Wander), ESM4::PackageProcedure::Wander);
    EXPECT_EQ(ESM4::packageProcedure(ESM4::AIPackageType::UseItemAt), ESM4::PackageProcedure::UseItemAt);
    EXPECT_EQ(ESM4::packageProcedure(ESM4::AIPackageType::CastMagic), ESM4::PackageProcedure::CastMagic);
    EXPECT_EQ(ESM4::packageProcedure(ESM4::AIPackageType::Unknown), ESM4::PackageProcedure::None);
}

TEST(ESM4AIPhase, ResolvesRouteDoorAndActionInOrder)
{
    auto state = ESM4::beginPackagePhase(selection(ESM4::AIPackageType::Travel), {}, {});
    EXPECT_EQ(state.mPhase, ESM4::PackagePhase::Select);
    EXPECT_EQ(ESM4::advancePackagePhase(state, {}).mTo, ESM4::PackagePhase::Resolve);
    EXPECT_EQ(ESM4::advancePackagePhase(state, { 0, true }).mTo, ESM4::PackagePhase::Path);
    EXPECT_EQ(ESM4::advancePackagePhase(state, { 0, true, true, true, false, true }).mTo,
        ESM4::PackagePhase::Arrive);
    EXPECT_EQ(ESM4::advancePackagePhase(state, { 0, true, true, true, true, true, false, false, false }).mTo,
        ESM4::PackagePhase::Act);
    EXPECT_EQ(ESM4::advancePackagePhase(state, { 0, true, true, true, false, false, false, false, true }).mTo,
        ESM4::PackagePhase::Complete);
}

TEST(ESM4AIPhase, BoundsRepathAndKeepsM16BoundaryExplicit)
{
    auto state = ESM4::beginPackagePhase(selection(ESM4::AIPackageType::CastMagic), {}, {});
    state.mPhase = ESM4::PackagePhase::Act;
    const auto transition
        = ESM4::advancePackagePhase(state, { 0, true, true, true, false, false, false, false, false, true, false, true });
    EXPECT_EQ(transition.mTo, ESM4::PackagePhase::WaitingForM16Action);
    EXPECT_EQ(transition.mBoundary, ESM4::PhaseBoundary::WaitingForM16Action);

    state = ESM4::beginPackagePhase(selection(ESM4::AIPackageType::Travel), {}, {});
    state.mPhase = ESM4::PackagePhase::Path;
    state.mRepathAttempts = 8;
    EXPECT_EQ(ESM4::advancePackagePhase(state, { 0, true, false, false, false, false, false, false, false, false, false }).mTo,
        ESM4::PackagePhase::Stalled);
}
