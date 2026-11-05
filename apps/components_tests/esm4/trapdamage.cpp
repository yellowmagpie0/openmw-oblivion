#include <components/esm4/trapdamage.hpp>

#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

namespace
{
    TEST(ESM4TrapImpact, CavernMaceUsesVictimLevelAndLiveScriptPhase)
    {
        const ESM4::TrapImpactSettings active{20, 1.5f, 200};
        EXPECT_NEAR(ESM4::trapImpactDamage(active, 1, 600, 0), 21.5f, .001f);
        EXPECT_NEAR(ESM4::trapImpactDamage(active, 10, 600, 0), 35.f, .001f);
        const ESM4::TrapImpactSettings settled{5, 1.5f, 200};
        EXPECT_NEAR(ESM4::trapImpactDamage(settled, 1, 600, 0), 6.5f, .001f);
        EXPECT_EQ(ESM4::trapImpactDamage({}, 10, 600, 0), 0);
    }

    TEST(ESM4TrapImpact, MinimumSpeedRejectsSlowBodiesButIncludesThreshold)
    {
        const ESM4::TrapImpactSettings active{20, 1.5f, 200};
        EXPECT_EQ(ESM4::trapImpactDamage(active, 1, 0, 0), 0);
        EXPECT_EQ(ESM4::trapImpactDamage(active, 1, 199.9f, 0), 0);
        EXPECT_NEAR(ESM4::trapImpactDamage(active, 1, 200, 0), 21.5f, .001f);
        EXPECT_NEAR(ESM4::trapImpactDamage({20, 0, -1}, 1, 0, 0), 20.f, .001f);
    }

    TEST(ESM4TrapImpact, ResistanceScalesDamageAndDoesNotHeal)
    {
        const ESM4::TrapImpactSettings active{20, 1.5f, 200};
        EXPECT_NEAR(ESM4::trapImpactDamage(active, 1, 600, 25), 16.125f, .001f);
        EXPECT_NEAR(ESM4::trapImpactDamage(active, 1, 600, -50), 32.25f, .001f);
        EXPECT_EQ(ESM4::trapImpactDamage(active, 1, 600, 100), 0);
        EXPECT_EQ(ESM4::trapImpactDamage(active, 1, 600, 150), 0);
        EXPECT_EQ(ESM4::trapImpactDamage({-20, 1, 0}, 1, 600, 0), 0);
    }

    TEST(ESM4TrapImpact, RejectsInvalidInputsBeforePublishingHealth)
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float inf = std::numeric_limits<float>::infinity();
        EXPECT_THROW(ESM4::trapImpactDamage({nan, 0, 0}, 1, 600, 0), std::invalid_argument);
        EXPECT_THROW(ESM4::trapImpactDamage({20, inf, 0}, 1, 600, 0), std::invalid_argument);
        EXPECT_THROW(ESM4::trapImpactDamage({20, 0, nan}, 1, 600, 0), std::invalid_argument);
        EXPECT_THROW(ESM4::trapImpactDamage({20, 0, 0}, 1, -1, 0), std::invalid_argument);
        EXPECT_THROW(ESM4::trapImpactDamage({20, 0, 0}, 1, inf, 0), std::invalid_argument);
        EXPECT_THROW(ESM4::trapImpactDamage({20, 0, 0}, 1, 600, nan), std::invalid_argument);
        EXPECT_THROW(ESM4::trapImpactDamage({0, std::numeric_limits<float>::max(), 0}, 2, 600, 0),
            std::invalid_argument);
    }
}
