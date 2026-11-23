#include <array>
#include <string_view>

#include <gtest/gtest.h>

#include <apps/openmw/mwrender/groundcoverplacement.hpp>

namespace
{
    TEST(OblivionGroundcover, LittoralPlantsRespectBothSidesOfTheWaterline)
    {
        // All eight GRAS modes in file order, at a distance of forty world units.
        constexpr std::array<float, 7> heights{ -41, -40, -1, 0, 1, 40, 41 };
        constexpr std::string_view accepted[]{ "0000011", "0001110", "1100000", "0111000", "1100011", "0111110",
            "1111110", "0111111" };
        for (unsigned mode = 0; mode < std::size(accepted); ++mode)
            for (std::size_t i = 0; i < heights.size(); ++i)
                EXPECT_EQ(MWRender::acceptsGrassWaterDistance(mode, 40, heights[i]), accepted[mode][i] == '1')
                    << "mode " << mode << " height above water " << heights[i];
        EXPECT_FALSE(MWRender::acceptsGrassWaterDistance(0, 0, -1));
    }

    TEST(OblivionGroundcover, SamplesTheVisibleTerrainTriangleOnBothDiagonals)
    {
        constexpr int side = ESM4::Land::sVertsPerSide;
        std::array<float, side * side> heights{};
        heights[side + 1] = 128;
        const auto sample
            = [&](float x, float y) { return MWRender::sampleGrassSurface(heights, x / (side - 1), y / (side - 1)); };
        const auto [height, normal] = sample(0.25f, 0.75f);
        EXPECT_FLOAT_EQ(height, 32.f);
        EXPECT_NEAR(normal.x(), -std::sqrt(0.5f), 1e-6f);
        EXPECT_FLOAT_EQ(normal.y(), 0.f);
        EXPECT_FLOAT_EQ(sample(0.75f, 0.25f).first, 32.f);
        EXPECT_FLOAT_EQ(sample(1.25f, 0.25f).first, 32.f);
        EXPECT_FLOAT_EQ(sample(1.75f, 0.75f).first, 32.f);
        EXPECT_FLOAT_EQ(sample(1.f, 1.f).first, 128.f);
        heights.back() = 800.f;
        EXPECT_FLOAT_EQ(sample(side - 1, side - 1).first, 800.f);
    }
}
