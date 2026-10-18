#include <components/nif/nativetransform.hpp>
#include <gtest/gtest.h>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>

namespace
{
    Nif::NiTransform transform(const std::array<float, 13>& values)
    {
        Nif::NiTransform result;
        for (unsigned i = 0; i < 9; ++i) result.mRotation.mValues[i / 3][i % 3] = values[i];
        for (unsigned i = 0; i < 3; ++i) result.mTranslation[i] = values[9 + i];
        result.mScale = values[12];
        return result;
    }
    std::array<std::uint32_t, 13> bits(const Nif::NiTransform& value)
    {
        std::array<std::uint32_t, 13> result;
        for (unsigned i = 0; i < 9; ++i) result[i] = std::bit_cast<std::uint32_t>(value.mRotation.mValues[i / 3][i % 3]);
        for (unsigned i = 0; i < 3; ++i) result[9 + i] = std::bit_cast<std::uint32_t>(value.mTranslation[i]);
        result[12] = std::bit_cast<std::uint32_t>(value.mScale);
        return result;
    }
    struct OriginalCase
    {
        std::array<float, 13> mParent, mLocal;
        std::array<std::uint32_t, 13> mExpected;
    };
    // Actual original 0053D7A0/007100A0/007101F0; image a8f31384... .
    // Selected independently emitted cases from drop-native-composition-oracle-01.
    const std::array<OriginalCase, 8> original{{
        {{0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x1.8800000000000p+3f, -0x1.3000000000000p+3f, 0x1.f490000000000p+9f, 0x0.0p+0f},
         {0x1.0000000000000p+1f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.e921de0000000p+0f, -0x1.2e9cda0000000p-1f, 0x0.0p+0f, 0x1.2e9cda0000000p-1f, 0x1.e921de0000000p+0f, 0x1.4000000000000p+0f, -0x1.4000000000000p+1f, 0x1.e000000000000p+1f, -0x1.0000000000000p+1f},
         {0x0u, 0xbff490efu, 0x3f174e6du, 0x40000000u, 0x0u, 0x0u, 0x0u, 0x3f174e6du, 0x3ff490efu, 0x41440000u, 0xc1180000u, 0x447a4800u, 0x80000000u}},
        {{0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x1.8800000000000p+3f, -0x1.3000000000000p+3f, 0x1.f490000000000p+9f, 0x0.0p+0f},
         {0x1.0000000000000p+1f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.e921de0000000p+0f, -0x1.2e9cda0000000p-1f, 0x0.0p+0f, 0x1.2e9cda0000000p-1f, 0x1.e921de0000000p+0f, 0x1.4000000000000p+0f, -0x1.4000000000000p+1f, 0x1.e000000000000p+1f, 0x1.0000000000000p-2f},
         {0x0u, 0xbff490efu, 0x3f174e6du, 0x40000000u, 0x0u, 0x0u, 0x0u, 0x3f174e6du, 0x3ff490efu, 0x41440000u, 0xc1180000u, 0x447a4800u, 0x0u}},
        {{0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x1.8800000000000p+3f, -0x1.3000000000000p+3f, 0x1.f490000000000p+9f, 0x0.0p+0f},
         {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x1.4000000000000p+0f, -0x1.4000000000000p+1f, 0x1.e000000000000p+1f, -0x1.0000000000000p+1f},
         {0x0u, 0x3f800000u, 0x0u, 0x3f800000u, 0x0u, 0x0u, 0x0u, 0x0u, 0x3f800000u, 0x41440000u, 0xc1180000u, 0x447a4800u, 0x80000000u}},
        {{0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x1.8800000000000p+3f, -0x1.3000000000000p+3f, 0x1.f490000000000p+9f, 0x0.0p+0f},
         {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x1.4000000000000p+0f, -0x1.4000000000000p+1f, 0x1.e000000000000p+1f, 0x1.0000000000000p-2f},
         {0x0u, 0x3f800000u, 0x0u, 0x3f800000u, 0x0u, 0x0u, 0x0u, 0x0u, 0x3f800000u, 0x41440000u, 0xc1180000u, 0x447a4800u, 0x0u}},
        {{0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x1.8800000000000p+3f, -0x1.3000000000000p+3f, 0x1.f490000000000p+9f, 0x1.0000000000000p+1f},
         {0x1.0000000000000p+1f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.e921de0000000p+0f, -0x1.2e9cda0000000p-1f, 0x0.0p+0f, 0x1.2e9cda0000000p-1f, 0x1.e921de0000000p+0f, 0x1.4000000000000p+0f, -0x1.4000000000000p+1f, 0x1.e000000000000p+1f, -0x1.0000000000000p+1f},
         {0x0u, 0xbff490efu, 0x3f174e6du, 0x40000000u, 0x0u, 0x0u, 0x0u, 0x3f174e6du, 0x3ff490efu, 0x418a0000u, 0xc0e00000u, 0x447c2800u, 0xc0800000u}},
        {{0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x1.8800000000000p+3f, -0x1.3000000000000p+3f, 0x1.f490000000000p+9f, 0x1.0000000000000p+1f},
         {0x1.0000000000000p+1f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.e921de0000000p+0f, -0x1.2e9cda0000000p-1f, 0x0.0p+0f, 0x1.2e9cda0000000p-1f, 0x1.e921de0000000p+0f, 0x1.4000000000000p+0f, -0x1.4000000000000p+1f, 0x1.e000000000000p+1f, 0x1.0000000000000p-2f},
         {0x0u, 0xbff490efu, 0x3f174e6du, 0x40000000u, 0x0u, 0x0u, 0x0u, 0x3f174e6du, 0x3ff490efu, 0x418a0000u, 0xc0e00000u, 0x447c2800u, 0x3f000000u}},
        {{0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x1.8800000000000p+3f, -0x1.3000000000000p+3f, 0x1.f490000000000p+9f, 0x1.0000000000000p+1f},
         {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x1.4000000000000p+0f, -0x1.4000000000000p+1f, 0x1.e000000000000p+1f, -0x1.0000000000000p+1f},
         {0x0u, 0x3f800000u, 0x0u, 0x3f800000u, 0x0u, 0x0u, 0x0u, 0x0u, 0x3f800000u, 0x418a0000u, 0xc0e00000u, 0x447c2800u, 0xc0800000u}},
        {{0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x1.8800000000000p+3f, -0x1.3000000000000p+3f, 0x1.f490000000000p+9f, 0x1.0000000000000p+1f},
         {0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, -0x1.0000000000000p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x0.0p+0f, 0x1.0000000000000p+0f, 0x1.4000000000000p+0f, -0x1.4000000000000p+1f, 0x1.e000000000000p+1f, 0x1.0000000000000p-2f},
         {0x0u, 0x3f800000u, 0x0u, 0x3f800000u, 0x0u, 0x0u, 0x0u, 0x0u, 0x3f800000u, 0x418a0000u, 0xc0e00000u, 0x447c2800u, 0x3f000000u}},
    }};

    TEST(NifNativeTransform, OriginalCompositionBitsAndInputOwnership)
    {
        for (const auto& sample : original)
        {
            const auto parent = transform(sample.mParent), local = transform(sample.mLocal);
            const auto parentBefore = bits(parent), localBefore = bits(local);
            const auto result = Nif::tryComposeNativeTransform(parent, local);
            ASSERT_TRUE(result);
            EXPECT_EQ(bits(*result), sample.mExpected);
            EXPECT_EQ(bits(parent), parentBefore);
            EXPECT_EQ(bits(local), localBefore);
        }
    }

    TEST(NifNativeTransform, RejectsNonfiniteInputsAndOverflowWithoutMutatingThem)
    {
        const std::array<float, 13> identity{1,0,0,0,1,0,0,0,1,0,0,0,1};
        const auto good = transform(identity);
        for (unsigned i = 0; i < 13; ++i)
        for (float bad : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::quiet_NaN()})
        {
            auto values = identity; values[i] = bad;
            const auto invalid = transform(values); const auto before = bits(invalid);
            EXPECT_FALSE(Nif::tryComposeNativeTransform(invalid, good));
            EXPECT_FALSE(Nif::tryComposeNativeTransform(good, invalid));
            EXPECT_EQ(bits(invalid), before);
        }
        for (unsigned i : {0u, 9u, 12u})
        {
            auto values = identity; values[i] = std::numeric_limits<float>::max();
            auto large = transform(values); auto twice = good;
            twice.mScale = 2; twice.mRotation.mValues[0][0] = 2;
            EXPECT_FALSE(Nif::tryComposeNativeTransform(twice, large));
        }
    }

    TEST(NifNativeTransform, SinglePrecisionRoundsEachInstructionWithNearestEvenTies)
    {
        // Original 0053D7A0, FPCW007F, native-single-compose-oracle-01/ties.
        auto parent = Nif::NiTransform::getIdentity(), local = parent;
        for (auto& row : parent.mRotation.mValues) for (auto& value : row) value = 0;
        for (auto& row : local.mRotation.mValues) for (auto& value : row) value = 0;
        parent.mRotation.mValues[0][0] = parent.mRotation.mValues[0][1]
            = parent.mRotation.mValues[0][2] = 1;
        local.mRotation.mValues[0][0] = 1;
        local.mRotation.mValues[1][0] = local.mRotation.mValues[2][0] = 0x1p-24f;
        local.mTranslation = {1, 0x1p-24f, 0x1p-24f};
        const auto single = Nif::tryComposeNativeTransform(parent, local, Nif::NativeTransformPrecision::Single);
        const auto oldDefault = Nif::tryComposeNativeTransform(parent, local);
        ASSERT_TRUE(single);
        ASSERT_TRUE(oldDefault);
        EXPECT_EQ(single->mRotation.mValues[0][0], 1.f);
        EXPECT_EQ(single->mTranslation.x(), 1.f);
        EXPECT_EQ(oldDefault->mRotation.mValues[0][0], 0x1.000002p0f);
        EXPECT_EQ(oldDefault->mTranslation.x(), 0x1.000002p0f);
        EXPECT_FALSE(Nif::tryComposeNativeTransform(parent, local, static_cast<Nif::NativeTransformPrecision>(99)));
    }

    TEST(NifNativeTransform, SinglePrecisionKeepsWideExponentUntilTheNativeFloatStore)
    {
        // Original wide-cancel/tiny-sum cases. A float cast after every
        // arithmetic instruction would overflow or lose both tiny products.
        auto parent = Nif::NiTransform::getIdentity(), local = parent;
        for (auto& row : parent.mRotation.mValues) for (auto& value : row) value = 0;
        for (auto& row : local.mRotation.mValues) for (auto& value : row) value = 0;
        parent.mRotation.mValues[0][0] = parent.mRotation.mValues[0][1] = 2;
        local.mRotation.mValues[0][0] = std::numeric_limits<float>::max();
        local.mRotation.mValues[1][0] = -std::numeric_limits<float>::max();
        local.mTranslation = {std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), 0};
        auto result = Nif::tryComposeNativeTransform(parent, local, Nif::NativeTransformPrecision::Single);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->mRotation.mValues[0][0], 0.f);
        EXPECT_EQ(result->mTranslation.x(), 0.f);
        parent.mRotation.mValues[0][0] = parent.mRotation.mValues[0][1] = 0x1p-75f;
        local.mRotation.mValues[0][0] = local.mRotation.mValues[1][0] = 0x1p-75f;
        local.mTranslation = {0x1p-75f, 0x1p-75f, 0};
        result = Nif::tryComposeNativeTransform(parent, local, Nif::NativeTransformPrecision::Single);
        ASSERT_TRUE(result);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(result->mRotation.mValues[0][0]), 1u);
        EXPECT_EQ(std::bit_cast<std::uint32_t>(result->mTranslation.x()), 1u);
    }

    TEST(NifNativeTransform, OriginalStoresRoundFiniteMaximumUntilTheOverflowMidpoint)
    {
        // Independent original 0053D7A0 matrix/vector/translation cases,
        // including both signs and the exact overflow tie.
        for (float sign : {-1.f, 1.f})
        {
            auto parent = Nif::NiTransform::getIdentity();
            auto local = Nif::NiTransform::getIdentity();
            local.mTranslation.x() = sign * std::numeric_limits<float>::max();
            parent.mTranslation.x() = sign * 0x1p102f;
            auto result = Nif::tryComposeNativeTransform(parent, local);
            ASSERT_TRUE(result);
            EXPECT_EQ(result->mTranslation.x(), local.mTranslation.x());
            parent.mTranslation.x() = sign * 0x1p103f;
            EXPECT_FALSE(Nif::tryComposeNativeTransform(parent, local));
            parent = Nif::NiTransform::getIdentity(); parent.mRotation.mValues[0][1] = 1;
            local.mTranslation.y() = sign * 0x1p102f;
            result = Nif::tryComposeNativeTransform(parent, local);
            ASSERT_TRUE(result);
            EXPECT_EQ(result->mTranslation.x(), local.mTranslation.x());
            local.mTranslation.y() = sign * 0x1p103f;
            EXPECT_FALSE(Nif::tryComposeNativeTransform(parent, local));
            local = Nif::NiTransform::getIdentity();
            local.mRotation.mValues[0][0] = sign * std::numeric_limits<float>::max();
            local.mRotation.mValues[1][0] = sign * 0x1p102f;
            result = Nif::tryComposeNativeTransform(parent, local);
            ASSERT_TRUE(result);
            EXPECT_EQ(result->mRotation.mValues[0][0], local.mRotation.mValues[0][0]);
            local.mRotation.mValues[1][0] = sign * 0x1p103f;
            EXPECT_FALSE(Nif::tryComposeNativeTransform(parent, local));
        }
        float destination = 17;
        EXPECT_FALSE(Nif::tryStoreNativeTransformFloat(0x1.ffffffp+127, destination));
        EXPECT_EQ(destination, 17);
    }

}

namespace
{
    Nif::Matrix3 matrixFromBits(const std::array<std::uint32_t, 9>& source)
    {
        Nif::Matrix3 result;
        for (unsigned i = 0; i < 9; ++i) result.mValues[i / 3][i % 3] = std::bit_cast<float>(source[i]);
        return result;
    }

    std::array<std::uint32_t, 9> matrixBits(const Nif::Matrix3& source)
    {
        std::array<std::uint32_t, 9> result;
        for (unsigned i = 0; i < 9; ++i) result[i] = std::bit_cast<std::uint32_t>(source.mValues[i / 3][i % 3]);
        return result;
    }

    TEST(NifNativeTransform, MatrixProductsRetainOriginalAddRotationStoreBoundaries)
    {
        struct Case
        {
            Nif::NativeTransformPrecision mPrecision;
            std::array<std::uint32_t, 9> mParent, mLocal, mExtra, mExpected;
        };
        // Complete original88E9F0 with its two7100A0 products; captured before
        // C++ comparison in blend-addrotation-postpass-oracle-01.
        const std::array<Case, 2> cases{{
            {Nif::NativeTransformPrecision::Single,
             {1051138986u,3203013790u,1062455504u,3175607395u,3210860452u,3203386077u,1064413299u,1038507052u,3198081370u},
             {1042608974u,1062165941u,1058035636u,1059562395u,1051586122u,3207386224u,3208447962u,1056225561u,3203633362u},
             {1059384368u,3206035379u,1056329134u,3203276221u,3209546500u,3200112804u,1058762940u,1014509758u,3209387688u},
             {3208256036u,1037633297u,3207442187u,1058434851u,1058762741u,3205032292u,1052108156u,3209271172u,3204509764u}},
            {Nif::NativeTransformPrecision::Double,
             {3190376768u,3197543518u,1064368902u,3203504658u,3209725108u,3198994344u,1063105499u,3204447656u,3146318873u},
             {3211746399u,3190183166u,3198249166u,1051845140u,3206090732u,3208178912u,3180443832u,3209228767u,1058901901u},
             {1040473589u,1039068319u,3212580568u,1053446281u,1063755075u,1042472959u,1063834512u,3201417277u,1032919873u},
             {1057868522u,3210027092u,3157614371u,1062542537u,1057870182u,3156624901u,1013901070u,3144388864u,1065351324u}}
        }};
        for (const auto& row : cases)
        {
            const auto parent = matrixFromBits(row.mParent), local = matrixFromBits(row.mLocal), extra = matrixFromBits(row.mExtra);
            const auto first = Nif::tryMultiplyNativeMatrices(parent, local, row.mPrecision);
            ASSERT_TRUE(first);
            const auto second = Nif::tryMultiplyNativeMatrices(*first, extra, row.mPrecision);
            ASSERT_TRUE(second); EXPECT_EQ(matrixBits(*second), row.mExpected);
            EXPECT_EQ(matrixBits(parent), row.mParent); EXPECT_EQ(matrixBits(local), row.mLocal);
            EXPECT_EQ(matrixBits(extra), row.mExtra);
        }
    }

    TEST(NifNativeTransform, MatrixProductsRejectNonfiniteInputsAndStoredOverflowWithoutInputWrites)
    {
        const Nif::Matrix3 identity;
        for (unsigned axis = 0; axis < 9; ++axis)
            for (const float value : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
            {
                auto invalid = identity; invalid.mValues[axis / 3][axis % 3] = value;
                const auto before = matrixBits(invalid);
                EXPECT_FALSE(Nif::tryMultiplyNativeMatrices(invalid, identity));
                EXPECT_FALSE(Nif::tryMultiplyNativeMatrices(identity, invalid));
                EXPECT_EQ(matrixBits(invalid), before);
            }
        auto large = identity, twice = identity;
        large.mValues[0][0] = std::numeric_limits<float>::max(); twice.mValues[0][0] = 2;
        EXPECT_FALSE(Nif::tryMultiplyNativeMatrices(large, twice));
        EXPECT_FALSE(Nif::tryMultiplyNativeMatrices(identity, identity, static_cast<Nif::NativeTransformPrecision>(255)));
    }

    TEST(NifNativeTransform, MatrixProductsKeepSingleMantissaRoundingAndWideIntermediateExponents)
    {
        Nif::Matrix3 left, right;
        left.mValues[0][0] = left.mValues[0][1] = left.mValues[0][2] = 1;
        right.mValues[0][0] = 1; right.mValues[1][0] = right.mValues[2][0] = 0x1p-24f;
        const auto single = Nif::tryMultiplyNativeMatrices(left, right, Nif::NativeTransformPrecision::Single);
        const auto wider = Nif::tryMultiplyNativeMatrices(left, right);
        ASSERT_TRUE(single); ASSERT_TRUE(wider);
        EXPECT_EQ(single->mValues[0][0], 1); EXPECT_EQ(wider->mValues[0][0], 1 + 0x1p-23f);
        left.mValues[0][0] = left.mValues[0][1] = std::numeric_limits<float>::max();
        right.mValues[0][0] = 2; right.mValues[1][0] = -2; right.mValues[2][0] = 1;
        const auto cancelled = Nif::tryMultiplyNativeMatrices(left, right, Nif::NativeTransformPrecision::Single);
        ASSERT_TRUE(cancelled); EXPECT_EQ(cancelled->mValues[0][0], 1);
    }
}
