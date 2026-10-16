#include <components/esm4/referenceplacement.hpp>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <limits>

namespace
{
    TEST(ESM4ReferencePlacement, MatchesOriginalHardwareAxisProductsAndSeparateScale)
    {
        struct Case
        {
            Nif::NativeTransformPrecision mPrecision;
            std::array<unsigned, 3> mAngles;
            unsigned mScale;
            std::array<unsigned, 9> mExpectedRotation;
            unsigned mExpectedScale;
        };
        // Actual complete original4D7AF0 nonactor path and4E43C8 scale prefix
        // on i386 hardware. Native02 corpus cb86ff0c4efdcf95...; no engine oracle.
        const std::array<Case, 10> cases{{
            {Nif::NativeTransformPrecision::Single, {0x0u, 0x0u, 0x0u}, 0x3f2147aeu,
                {0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x3f2147aeu},
            {Nif::NativeTransformPrecision::Double, {0x0u, 0x0u, 0x0u}, 0xbfb0a3d7u,
                {0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x3fb0a3d7u},
            {Nif::NativeTransformPrecision::Double, {0x0u, 0x0u, 0x0u}, 0x3f2147aeu,
                {0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x3f2147aeu},
            {Nif::NativeTransformPrecision::Single, {0xc131b654u, 0xc10c18b9u, 0xc1084ebfu}, 0x3f800000u,
                {0x3ef7f5aeu, 0x3f1e074au, 0x3f1eb84bu, 0x3eef85b0u, 0x3ed50e63u, 0xbf479cbcu, 0xbf3d448cu, 0x3f2aec5du, 0xbdb2b29eu}, 0x3f800000u},
            {Nif::NativeTransformPrecision::Double, {0xc131b654u, 0xc10c18b9u, 0xc1084ebfu}, 0xbfb0a3d7u,
                {0x3ef7f5aeu, 0x3f1e074au, 0x3f1eb84bu, 0x3eef85b0u, 0x3ed50e63u, 0xbf479cbcu, 0xbf3d448bu, 0x3f2aec5du, 0xbdb2b29eu}, 0x3fb0a3d7u},
            {Nif::NativeTransformPrecision::Single, {0xc3ccad2du, 0x1069bfb8u, 0x4b3635ccu}, 0x40000000u,
                {0xbd567db2u, 0xbf7fa615u, 0x9069bfb8u, 0x3f158f4fu, 0xbcfaf69du, 0xbf4f9ee7u, 0x3f4f55fau, 0xbd2df4c6u, 0x3f15c3eau}, 0x40000000u},
            {Nif::NativeTransformPrecision::Double, {0xc3ccad2du, 0x1069bfb8u, 0x4b3635ccu}, 0x3f800000u,
                {0xbd567db2u, 0xbf7fa615u, 0x9069bfb8u, 0x3f158f4fu, 0xbcfaf69du, 0xbf4f9ee7u, 0x3f4f55fau, 0xbd2df4c6u, 0x3f15c3eau}, 0x3f800000u},
            {Nif::NativeTransformPrecision::Single, {0x4a51e5du, 0x4b312a71u, 0xd365f5eu}, 0x3f2147aeu,
                {0x3ec6ee91u, 0xc8db7b7u, 0xbf6be2fdu, 0x8d365f12u, 0x3f800000u, 0x4004f5cu, 0x3f6be2fdu, 0xd280aedu, 0x3ec6ee91u}, 0x3f2147aeu},
            {Nif::NativeTransformPrecision::Single, {0x0u, 0x0u, 0x80000000u}, 0x0u,
                {0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x0u},
            {Nif::NativeTransformPrecision::Single, {0x0u, 0x0u, 0xdeffffffu}, 0x7f7fffffu,
                {0xbf7a7091u, 0xbe544316u, 0x0u, 0x3e544316u, 0xbf7a7091u, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x7f7fffffu},
        }};
        for (std::size_t index = 0; index < cases.size(); ++index)
        {
            SCOPED_TRACE(index);
            const auto& row = cases[index];
            ESM::Position position{};
            for (unsigned i = 0; i < 3; ++i) position.rot[i] = std::bit_cast<float>(row.mAngles[i]);
            position.pos[0] = -0.f;
            position.pos[1] = std::numeric_limits<float>::denorm_min();
            position.pos[2] = std::numeric_limits<float>::max();
            const auto result = ESM4::referencePlacementRoot(position, std::bit_cast<float>(row.mScale), row.mPrecision);
            ASSERT_TRUE(result);
            for (unsigned i = 0; i < 9; ++i)
                EXPECT_EQ(std::bit_cast<unsigned>(result->mRotation.mValues[i / 3][i % 3]), row.mExpectedRotation[i]);
            for (unsigned i = 0; i < 3; ++i)
            {
                EXPECT_EQ(std::bit_cast<unsigned>(result->mTranslation[i]), std::bit_cast<unsigned>(position.pos[i]));
                EXPECT_EQ(std::bit_cast<unsigned>(position.rot[i]), row.mAngles[i]);
            }
            EXPECT_EQ(std::bit_cast<unsigned>(result->mScale), row.mExpectedScale);
            if (row.mPrecision == Nif::NativeTransformPrecision::Single)
            {
                const auto live = ESM4::referencePlacementRoot(position, std::bit_cast<float>(row.mScale));
                ASSERT_TRUE(live);
                for (unsigned i = 0; i < 9; ++i)
                    EXPECT_EQ(std::bit_cast<unsigned>(live->mRotation.mValues[i / 3][i % 3]), row.mExpectedRotation[i]);
            }
        }
    }

    TEST(ESM4ReferencePlacement, RejectsNonfiniteConsumedFieldsUnknownPrecisionAndNativeTrigRange)
    {
        ESM::Position position{};
        EXPECT_FALSE(ESM4::referencePlacementRoot(position, 1, static_cast<Nif::NativeTransformPrecision>(99)));
        for (const float value : {std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
        {
            EXPECT_FALSE(ESM4::referencePlacementRoot(position, value));
            for (unsigned i = 0; i < 3; ++i)
            {
                position.pos[i] = value;
                EXPECT_FALSE(ESM4::referencePlacementRoot(position, 1));
                position.pos[i] = 0;
                position.rot[i] = value;
                EXPECT_FALSE(ESM4::referencePlacementRoot(position, 1));
                position.rot[i] = 0;
            }
        }
        for (float value : {-0x1p63f, 0x1p63f, std::numeric_limits<float>::max()})
            for (float& angle : position.rot)
            {
                angle = value;
                EXPECT_FALSE(ESM4::referencePlacementRoot(position, 1));
                angle = 0;
            }
    }
}
