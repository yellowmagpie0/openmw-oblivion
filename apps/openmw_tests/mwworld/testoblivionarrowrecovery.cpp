#include "apps/openmw/mwworld/oblivionarrowrecovery.hpp"
#include <components/esm4/runtimestate.hpp>
#include <gtest/gtest.h>
#include <limits>

TEST(OblivionArrowRecovery, RetainsSeparateFlightProvenanceAndSettledLifetimeWithoutTouchingUnrelatedState)
{
    ESM4::RuntimeReferenceState reference;
    reference.mCustomState["unrelated"] = std::int64_t(7);
    EXPECT_FALSE(MWWorld::readRecoveredArrow(reference));
    const MWWorld::RecoveredArrowState state{ESM::FormKey::content("Oblivion.esm", 0x47aca),
        ESM::FormKey::dynamic("player", 1), ESM::FormKey::content("Oblivion.esm", 0x49be8),
        6, {30.f, 1.f, false}};
    MWWorld::writeRecoveredArrow(reference, state);
    auto saved = MWWorld::readRecoveredArrow(reference);
    ASSERT_TRUE(saved);
    EXPECT_EQ(saved->mBow, state.mBow);
    EXPECT_EQ(saved->mCaster, state.mCaster);
    EXPECT_EQ(saved->mSurface, state.mSurface);
    EXPECT_EQ(saved->mProjectile, 6u);
    EXPECT_EQ(saved->mLifetime.mAge, 30.f);
    EXPECT_EQ(saved->mLifetime.mOpacity, 1.f);
    EXPECT_FALSE(saved->mLifetime.mFading);
    const auto next = ESM4::advanceArrowLifetime(saved->mLifetime, 61.f, {90.f});
    EXPECT_TRUE(next.mState.mFading);
    saved->mLifetime = next.mState;
    MWWorld::writeRecoveredArrow(reference, *saved);
    const auto fading = MWWorld::readRecoveredArrow(reference);
    ASSERT_TRUE(fading);
    EXPECT_TRUE(fading->mLifetime.mFading);
    EXPECT_EQ(fading->mProjectile, 6u);
    EXPECT_EQ(std::get<std::int64_t>(reference.mCustomState.at("unrelated")), 7);
}

TEST(OblivionArrowRecovery, RejectsPartialNoncanonicalAndMalformedOwnershipBeforeReplacingState)
{
    ESM4::RuntimeReferenceState reference;
    const MWWorld::RecoveredArrowState state{ESM::FormKey::content("Oblivion.esm", 0x47aca),
        ESM::FormKey::dynamic("player", 1), ESM::FormKey::content("Oblivion.esm", 0x49be8),
        6, {0.f, 1.f, false}};
    MWWorld::writeRecoveredArrow(reference, state);
    const auto before = reference;
    for (const auto* key : {"native.arrow.bow", "native.arrow.caster", "native.arrow.surface",
            "native.arrow.projectile", "native.arrow.age", "native.arrow.opacity", "native.arrow.fading"})
    {
        auto partial = before;
        partial.mCustomState.erase(key);
        EXPECT_THROW(MWWorld::readRecoveredArrow(partial), std::invalid_argument);
    }
    for (const auto& serial : {"0", "06", "-6", "18446744073709551616", "6x"})
    {
        auto invalid = before;
        invalid.mCustomState["native.arrow.projectile"] = std::string(serial);
        EXPECT_THROW(MWWorld::readRecoveredArrow(invalid), std::invalid_argument);
    }
    for (float age : {-1.f, std::numeric_limits<float>::infinity()})
    {
        auto invalid = state;
        invalid.mLifetime.mAge = age;
        EXPECT_THROW(MWWorld::writeRecoveredArrow(reference, invalid), std::invalid_argument);
        EXPECT_EQ(reference, before);
    }
    auto invalid = before;
    invalid.mCustomState["native.arrow.opacity"] = 2.;
    EXPECT_THROW(MWWorld::readRecoveredArrow(invalid), std::invalid_argument);
    invalid = before;
    invalid.mCustomState["native.arrow.unknown"] = 0.;
    EXPECT_THROW(MWWorld::readRecoveredArrow(invalid), std::invalid_argument);
}
