#include "apps/openmw/mwgui/questbearing.hpp"

#include <numbers>

#include <gtest/gtest.h>

TEST(QuestBearing, TurnsClockwiseFromTheCurrentView)
{
    using MWGui::questBearing;
    constexpr float halfTurn = std::numbers::pi_v<float>;
    EXPECT_FLOAT_EQ(questBearing({ 0, 10 }, { 0, 1 }).value(), 0.f);
    EXPECT_FLOAT_EQ(questBearing({ 10, 0 }, { 0, 1 }).value(), halfTurn / 2);
    EXPECT_FLOAT_EQ(questBearing({ -10, 0 }, { 0, 1 }).value(), -halfTurn / 2);
    EXPECT_FLOAT_EQ(std::abs(questBearing({ 0, -10 }, { 0, 1 }).value()), halfTurn);
    EXPECT_FLOAT_EQ(questBearing({ -10, 0 }, { -1, 0 }).value(), 0.f);
    EXPECT_FLOAT_EQ(questBearing({ 0, 10 }, { -1, 0 }).value(), halfTurn / 2);
    EXPECT_FALSE(questBearing({ 0, 0 }, { 0, 1 }));
    EXPECT_FALSE(questBearing({ 0, 10 }, { 0, 0 }));
}

TEST(QuestBearing, FormatsHorizontalDistanceInMetresAndKilometres)
{
    const float metre = Constants::UnitsPerMeter;
    EXPECT_EQ(MWGui::questDistance({ 0, 0 }), "0 m");
    EXPECT_EQ(MWGui::questDistance({ 3 * metre, -4 * metre }), "5 m");
    EXPECT_EQ(MWGui::questDistance({ 999 * metre, 0 }), "999 m");
    EXPECT_EQ(MWGui::questDistance({ 1000 * metre, 0 }), "1.00 km");
    EXPECT_EQ(MWGui::questDistance({ -1500 * metre, 0 }), "1.50 km");
}
