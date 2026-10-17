#include <components/esm4/npcplacement.hpp>
#include <gtest/gtest.h>
#include <array>
#include <bit>
#include <limits>

namespace
{
    struct OriginalPlacement
    {
        float mYaw, mScale;
        std::optional<ESM4::NpcPlacementMorphology> mMorphology;
        std::array<unsigned, 9> mExpected;
        unsigned mExpectedScale;
    };
    const std::array<OriginalPlacement, 21> original{{
        {0x0.0p+0f, 0x0.0p+0f, std::nullopt, {0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x0u},
        {0x1.3333340000000p-2f, 0x0.0p+0f, std::nullopt, {0x3f7490efu, 0x3e974e6du, 0x0u, 0xbe974e6du, 0x3f7490efu, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x0u},
        {0x1.921fb60000000p+1f, -0x1.0000000000000p+1f, std::nullopt, {0xbf800000u, 0xb3bbbd2eu, 0x0u, 0x33bbbd2eu, 0xbf800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x40000000u},
        {0x0.0p+0f, 0x1.0000000000000p-2f, ESM4::NpcPlacementMorphology{0x1.0000000000000p+0f, 0x1.0000000000000p+0f}, {0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x3e800000u},
        {0x1.3333340000000p-2f, 0x1.0000000000000p+0f, ESM4::NpcPlacementMorphology{0x1.0000000000000p+0f, 0x1.0000000000000p+0f}, {0x3f7490efu, 0x3e974e6du, 0x0u, 0xbe974e6du, 0x3f7490efu, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x3f800000u},
        {0x0.0p+0f, 0x0.0p+0f, ESM4::NpcPlacementMorphology{0x1.999999999999ap-1f, 0x1.3333333333333p+0f}, {0x3f4ccccdu, 0x0u, 0x0u, 0x0u, 0x3f4ccccdu, 0x0u, 0x0u, 0x0u, 0x3f99999au}, 0x0u},
        {0x1.921fb60000000p+1f, -0x1.0000000000000p+1f, ESM4::NpcPlacementMorphology{0x1.999999999999ap-1f, 0x1.3333333333333p+0f}, {0xbf4ccccdu, 0xb39630f2u, 0x0u, 0x339630f2u, 0xbf4ccccdu, 0x0u, 0x0u, 0x0u, 0x3f99999au}, 0x40000000u},
        {0x0.0p+0f, 0x1.0000000000000p-2f, ESM4::NpcPlacementMorphology{0x1.2000000000000p+0f, 0x1.c000000000000p-1f}, {0x3f900000u, 0x0u, 0x0u, 0x0u, 0x3f900000u, 0x0u, 0x0u, 0x0u, 0x3f600000u}, 0x3e800000u},
        {0x0.0p+0f, 0x1.0000000000000p+0f, ESM4::NpcPlacementMorphology{0x0.0p+0f, 0x0.0p+0f}, {0x0u, 0x0u, 0x0u, 0x0u, 0x0u, 0x0u, 0x0u, 0x0u, 0x0u}, 0x3f800000u},
        {0x1.921fb60000000p+1f, 0x1.0000000000000p-2f, ESM4::NpcPlacementMorphology{0x0.0p+0f, 0x0.0p+0f}, {0x0u, 0x0u, 0x0u, 0x0u, 0x0u, 0x0u, 0x0u, 0x0u, 0x0u}, 0x3e800000u},
        {0x0.0p+0f, 0x1.0000000000000p+1f, ESM4::NpcPlacementMorphology{-0x1.0000000000000p-1f, 0x1.0000000000000p+1f}, {0xbf000000u, 0x0u, 0x0u, 0x0u, 0xbf000000u, 0x0u, 0x0u, 0x0u, 0x40000000u}, 0x40000000u},
        {0x0.0p+0f, 0x1.0000000000000p+1f, ESM4::NpcPlacementMorphology{0x1.0000000000000p+1f, -0x1.0000000000000p-1f}, {0x40000000u, 0x0u, 0x0u, 0x0u, 0x40000000u, 0x0u, 0x0u, 0x0u, 0xbf000000u}, 0x40000000u},
        {0x0.0p+0f, 0x1.0000000000000p+0f, std::nullopt, {0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x3f800000u},
        {0x0.0p+0f, 0x1.0000000000000p-2f, ESM4::NpcPlacementMorphology{0x1.0000000000000p+0f, 0x1.0000000000000p+0f}, {0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x3e800000u},
        {0x0.0p+0f, 0x1.0000000000000p+1f, ESM4::NpcPlacementMorphology{0x1.2000000000000p+0f, 0x1.c000000000000p-1f}, {0x3f900000u, 0x0u, 0x0u, 0x0u, 0x3f900000u, 0x0u, 0x0u, 0x0u, 0x3f600000u}, 0x40000000u},
        {0x1.921fb40000000p+3f, 0x1.c000000000000p+2f, ESM4::NpcPlacementMorphology{-0x1.0000000000000p-1f, 0x1.0000000000000p+1f}, {0xbf000000u, 0x34a22169u, 0x0u, 0xb4a22169u, 0xbf000000u, 0x0u, 0x0u, 0x0u, 0x40000000u}, 0x40e00000u},
        {0x1.c6bf520000000p+49f, 0x1.0000000000000p+0f, std::nullopt, {0x3dd7c584u, 0x3f7e9341u, 0x0u, 0xbf7e9341u, 0x3dd7c584u, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x3f800000u},
        {0x1.bc16d60000000p+59f, 0x1.0000000000000p+0f, std::nullopt, {0x3f79fc6fu, 0xbe5ca705u, 0x0u, 0x3e5ca705u, 0x3f79fc6fu, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x3f800000u},
        {-0x1.74ca1c0000000p+51f, 0x1.0000000000000p+1f, std::nullopt, {0xbf7fab4cu, 0x3d502e2bu, 0x0u, 0xbd502e2bu, 0xbf7fab4cu, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x40000000u},
        {0x1.598aa00000000p+49f, -0x0.0p+0f, std::nullopt, {0xbf6f00bcu, 0x3eb7739fu, 0x0u, 0xbeb7739fu, 0xbf6f00bcu, 0x0u, 0x0u, 0x0u, 0x3f800000u}, 0x0u},
        {0x1.b727ba0000000p-121f, -0x0.0p+0f, ESM4::NpcPlacementMorphology{0x1.0000000000000p+1f, -0x1.0000000000000p-1f}, {0x40000000u, 0x3db93ddu, 0x0u, 0x83db93ddu, 0x40000000u, 0x0u, 0x0u, 0x0u, 0xbf000000u}, 0x0u},
    }};

    TEST(ESM4CreaturePlacement, MatchesOriginalActorYawAndStoredBaseScaleProduct)
    {
        struct OriginalCreaturePlacement
        {
            float mYaw, mReferenceScale, mBaseScale;
            unsigned mControlWord;
            std::array<unsigned, 9> mMatrix;
            unsigned mScale;
            bool mFinite;
        };
        constexpr OriginalCreaturePlacement originalCreature[]{
#include "creatureplacement_expected.inc"
        };
        for (const auto& sample : originalCreature)
        {
            SCOPED_TRACE(sample.mControlWord);
            SCOPED_TRACE(sample.mReferenceScale);
            SCOPED_TRACE(sample.mBaseScale);
            ESM::Position position{};
            position.pos[0] = 12.25f; position.pos[1] = -9.5f; position.pos[2] = 1001.125f;
            position.rot[0] = .7f; position.rot[1] = -.9f; position.rot[2] = sample.mYaw;
            const auto precision = sample.mControlWord == 0x7f ? Nif::NativeTransformPrecision::Single
                                                             : Nif::NativeTransformPrecision::Double;
            const auto result = ESM4::creaturePlacementRoot(
                position, sample.mReferenceScale, sample.mBaseScale, precision);
            ASSERT_EQ(result.has_value(), sample.mFinite);
            if (!result) continue;
            EXPECT_EQ(std::bit_cast<unsigned>(result->mScale), sample.mScale);
            EXPECT_EQ(result->mTranslation, position.asVec3());
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 3; ++column)
                    EXPECT_EQ(std::bit_cast<unsigned>(result->mRotation.mValues[row][column]),
                        sample.mMatrix[row * 3 + column]);
        }
        for (float invalid : {std::numeric_limits<float>::infinity(),
                 -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            EXPECT_FALSE(ESM4::creaturePlacementScale(invalid, 1));
            EXPECT_FALSE(ESM4::creaturePlacementScale(1, invalid));
        }
        EXPECT_FALSE(ESM4::creaturePlacementScale(1, 1, static_cast<Nif::NativeTransformPrecision>(99)));
    }

    TEST(ESM4NpcPlacement, MatchesOriginalActorRootIncludingSeparateScaleAndMorphology)
    {
        // Native i386 execution of complete original4D7AF0/4D7260,
        // including large angles where emulator/libm reduction differs.
        // Image a8f313845c1545e9...; native probe03 includes the FABS node store.
        for (const auto& sample : original)
        {
            ESM::Position position{};
            position.pos[0] = 12.25f; position.pos[1] = -9.5f; position.pos[2] = 1001.125f;
            position.rot[0] = .7f; position.rot[1] = -.9f; position.rot[2] = sample.mYaw;
            const auto result = ESM4::npcPlacementRoot(position, sample.mScale, sample.mMorphology);
            ASSERT_TRUE(result);
            EXPECT_EQ(std::bit_cast<unsigned>(result->mScale), sample.mExpectedScale);
            EXPECT_EQ(result->mTranslation, position.asVec3());
            for (unsigned i = 0; i < 9; ++i)
                EXPECT_EQ(std::bit_cast<unsigned>(result->mRotation.mValues[i / 3][i % 3]), sample.mExpected[i]);
            EXPECT_EQ(position.rot[0], .7f); EXPECT_EQ(position.rot[1], -.9f);
        }
    }

    TEST(ESM4NpcPlacement, RejectsNonfiniteConsumedFieldsAndNativeTrigRange)
    {
        ESM::Position position{};
        for (float invalid : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::quiet_NaN()})
        {
            EXPECT_FALSE(ESM4::npcPlacementRoot(position, invalid, std::nullopt));
            EXPECT_FALSE(ESM4::npcPlacementRoot(position, 1, ESM4::NpcPlacementMorphology{invalid, 1}));
            EXPECT_FALSE(ESM4::npcPlacementRoot(position, 1, ESM4::NpcPlacementMorphology{1, invalid}));
            position.rot[2] = invalid;
            EXPECT_FALSE(ESM4::npcPlacementRoot(position, 1, std::nullopt)); position.rot[2] = 0;
            for (float& coordinate : position.pos)
            {
                coordinate = invalid; EXPECT_FALSE(ESM4::npcPlacementRoot(position, 1, std::nullopt)); coordinate = 0;
            }
        }
        for (float yaw : {-0x1p63f, 0x1p63f, std::numeric_limits<float>::max()})
        {
            position.rot[2] = yaw;
            EXPECT_FALSE(ESM4::npcPlacementRoot(position, 1, std::nullopt));
        }
        position.rot[2] = 0;
        position.rot[0] = position.rot[1] = std::numeric_limits<float>::quiet_NaN();
        EXPECT_TRUE(ESM4::npcPlacementRoot(position, 0, std::nullopt)); // Native actor root ignores pitch/roll.
    }
}
