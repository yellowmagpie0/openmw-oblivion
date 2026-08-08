#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include <components/esm/fourcc.hpp>
#include <components/esm4/formidfields.hpp>

namespace
{
    void put(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i)
            data.at(offset + i) = static_cast<std::uint8_t>(value >> (i * 8));
    }
}

TEST(Tes4FormIdFieldsTest, FindsPackedConditionReferencesByFunctionSignature)
{
    std::vector<std::uint8_t> data(24);
    data[0] = 4; // comparison value is a global
    data[8] = 60; // GetFactionRankDifference: two FormID parameters

    EXPECT_EQ(ESM4::findFormIdOffsets(ESM::fourCC("PACK"), ESM::fourCC("CTDA"), data),
        (std::vector<std::size_t>{ 4, 12, 16 }));

    data[0] = 0;
    data[8] = 59; // GetStageDone: quest plus numeric stage
    EXPECT_EQ(ESM4::findFormIdOffsets(ESM::fourCC("QUST"), ESM::fourCC("CTDA"), data),
        (std::vector<std::size_t>{ 12 }));
}

TEST(Tes4FormIdFieldsTest, DoesNotMisclassifyNumericConditionParameters)
{
    std::vector<std::uint8_t> data(20);
    data[8] = 14; // GetActorValue
    EXPECT_TRUE(ESM4::findFormIdOffsets(ESM::fourCC("INFO"), ESM::fourCC("CTDT"), data).empty());
}

TEST(Tes4FormIdFieldsTest, FindsOfficialLosslessRecordLayouts)
{
    EXPECT_EQ(ESM4::findFormIdOffsets(
                  ESM::fourCC("CLMT"), ESM::fourCC("WLST"), std::vector<std::uint8_t>(16)),
        (std::vector<std::size_t>{ 0, 8 }));
    EXPECT_EQ(ESM4::findFormIdOffsets(
                  ESM::fourCC("LVSP"), ESM::fourCC("LVLO"), std::vector<std::uint8_t>(24)),
        (std::vector<std::size_t>{ 4, 16 }));
    EXPECT_EQ(ESM4::findFormIdOffsets(
                  ESM::fourCC("WATR"), ESM::fourCC("GNAM"), std::vector<std::uint8_t>(12)),
        (std::vector<std::size_t>{ 0, 4, 8 }));
    EXPECT_EQ(ESM4::findFormIdOffsets(
                  ESM::fourCC("REGN"), ESM::fourCC("RDOT"), std::vector<std::uint8_t>(104)),
        (std::vector<std::size_t>{ 0, 52 }));
}

TEST(Tes4FormIdFieldsTest, AppliesMagicEffectUnionFlags)
{
    std::vector<std::uint8_t> data(68);
    EXPECT_EQ(ESM4::findFormIdOffsets(ESM::fourCC("MGEF"), ESM::fourCC("DATA"), data),
        (std::vector<std::size_t>{ 24, 32, 36, 40, 44, 48, 52 }));
    put(data, 0, 1u << 16);
    EXPECT_EQ(ESM4::findFormIdOffsets(ESM::fourCC("MGEF"), ESM::fourCC("DATA"), data),
        (std::vector<std::size_t>{ 8, 24, 32, 36, 40, 44, 48, 52 }));
}

TEST(Tes4FormIdFieldsTest, RejectsMalformedKnownLayouts)
{
    EXPECT_THROW(ESM4::findFormIdOffsets(
                     ESM::fourCC("MGEF"), ESM::fourCC("DATA"), std::vector<std::uint8_t>(22)),
        std::runtime_error);
    EXPECT_THROW(ESM4::findFormIdOffsets(
                     ESM::fourCC("REGN"), ESM::fourCC("RDOT"), std::vector<std::uint8_t>(3)),
        std::runtime_error);
    EXPECT_THROW(ESM4::findFormIdOffsets(
                     ESM::fourCC("INFO"), ESM::fourCC("CTDA"), std::vector<std::uint8_t>(19)),
        std::runtime_error);
}
