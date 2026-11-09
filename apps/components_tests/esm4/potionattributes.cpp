#include <components/esm4/potionattributes.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

TEST(ESM4PotionAttributes, SamePotionApplicationsRetainSeparatePausedAndExpiringLifetimes)
{
    const auto potion = ESM::FormKey::content("Oblivion.esm", 0xb1201);
    const ESM4::PotionAttributeState first{potion, 60.f, 59.75f, {{1, -1.f}, {2, -1.f}}};
    const ESM4::PotionAttributeState second{potion, 60.f, 34.75f, {{1, -1.f}, {2, -1.f}}};
    EXPECT_NO_THROW(first.validate());
    EXPECT_NO_THROW(second.validate());
    const auto expired = ESM4::advancePotionAttributes(first.mElapsed, first.mDuration, .25f);
    const auto active = ESM4::advancePotionAttributes(second.mElapsed, second.mDuration, .25f);
    EXPECT_TRUE(expired.mExpired);
    EXPECT_FLOAT_EQ(expired.mElapsed, 60.f);
    EXPECT_FALSE(active.mExpired);
    EXPECT_FLOAT_EQ(active.mElapsed, 35.f);
    EXPECT_FLOAT_EQ(ESM4::advancePotionAttributes(active.mElapsed, 60.f, 0.f).mElapsed, 35.f);
    EXPECT_TRUE(ESM4::advancePotionAttributes(active.mElapsed, 60.f, 1000.f).mExpired);
    EXPECT_THROW(ESM4::advancePotionAttributes(60.f, 60.f, 0.f), std::invalid_argument);
}

TEST(ESM4PotionAttributes, RejectsAmbiguousSourcesContributionsAndInvalidClockDomains)
{
    const ESM4::PotionAttributeState valid{ESM::FormKey::content("Oblivion.esm", 0xb1201),
        60.f, 1.f, {{1, -1.f}, {2, -1.f}}};
    auto bad = valid;
    bad.mPotion = ESM::FormKey::dynamic("item", 1);
    EXPECT_THROW(bad.validate(), std::invalid_argument);
    bad = valid; bad.mDuration = 0.f;
    EXPECT_THROW(bad.validate(), std::invalid_argument);
    bad = valid; bad.mElapsed = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(bad.validate(), std::invalid_argument);
    bad = valid; bad.mModifiers.clear();
    EXPECT_THROW(bad.validate(), std::invalid_argument);
    bad = valid; bad.mModifiers.push_back(valid.mModifiers.front());
    EXPECT_THROW(bad.validate(), std::invalid_argument);
    bad = valid; bad.mModifiers[0].mAttribute = 8;
    EXPECT_THROW(bad.validate(), std::invalid_argument);
    bad = valid; bad.mModifiers[0].mDelta = 0.f;
    EXPECT_THROW(bad.validate(), std::invalid_argument);
    bad = valid; bad.mModifiers[0].mDelta = std::numeric_limits<float>::infinity();
    EXPECT_THROW(bad.validate(), std::invalid_argument);
    EXPECT_THROW(ESM4::advancePotionAttributes(0.f, 60.f, -1.f), std::invalid_argument);
}
