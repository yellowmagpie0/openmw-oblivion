#include <components/esm4/animationblend.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <gtest/gtest.h>

#include <bit>
#include <filesystem>
#include <limits>

namespace
{
    using Bits = std::array<std::uint32_t, 8>;
    ESM4::SequenceBlendPose pose(const Bits& bits)
    {
        const auto value = [&](unsigned i) { return std::bit_cast<float>(bits[i]); };
        ESM4::SequenceBlendPose result;
        if (bits[0] != 0xff7fffffu) result.mTranslation = {value(0), value(1), value(2)};
        if (bits[4] != 0xff7fffffu) result.mRotation = {value(3), value(4), value(5), value(6)};
        if (bits[7] != 0xff7fffffu) result.mScale = value(7);
        return result;
    }

    Bits bits(const boost::property_tree::ptree& values)
    {
        Bits result{};
        unsigned i = 0;
        for (const auto& value : values) result.at(i++) = value.second.get_value<std::uint32_t>();
        if (i != result.size()) throw std::runtime_error("invalid native blend expectation fixture");
        return result;
    }
}

TEST(ESM4NativeAnimationBlend, OriginalSlotNormalizationAndTransformBits)
{
    boost::property_tree::ptree document;
    boost::property_tree::read_json((std::filesystem::path(OPENMW_PROJECT_SOURCE_DIR)
        / "scripts/data/oblivion_compat/m15_native_sequence_blend_expected.json").string(), document);
    unsigned cases = 0;
    for (const auto& entry : document.get_child("cases"))
    {
        const auto& test = entry.second;
        SCOPED_TRACE(test.get<std::string>("name"));
        std::vector<ESM4::SequenceBlendWeight> slots;
        std::vector<ESM4::SequenceBlendPose> poses;
        for (const auto& row : test.get_child("slots"))
        {
            const auto& slot = row.second;
            slots.push_back({slot.get<float>("weight"), slot.get<float>("ease"),
                static_cast<std::uint8_t>(slot.get<unsigned>("priority"))});
            poses.push_back(pose(bits(slot.get_child("pose_bits"))));
        }
        const auto weights = ESM4::normalizeSequenceBlendWeights(slots,
            test.get<float>("threshold"), test.get<bool>("only_highest"));
        unsigned i = 0;
        for (const auto& expected : test.get_child("expected_weight_bits"))
            EXPECT_EQ(std::bit_cast<std::uint32_t>(weights.at(i++)), expected.second.get_value<std::uint32_t>());
        EXPECT_EQ(i, weights.size());
        const auto actual = ESM4::blendSequenceTransforms(poses, weights);
        const auto expected = pose(bits(test.get_child("expected_pose_bits")));
        ASSERT_EQ(actual.mTranslation.has_value(), expected.mTranslation.has_value());
        ASSERT_EQ(actual.mRotation.has_value(), expected.mRotation.has_value());
        ASSERT_EQ(actual.mScale.has_value(), expected.mScale.has_value());
        if (actual.mTranslation)
        {
            for (i = 0; i < 3; ++i)
            {
                EXPECT_EQ(std::bit_cast<std::uint32_t>((*actual.mTranslation)[i]),
                    std::bit_cast<std::uint32_t>((*expected.mTranslation)[i]));
            }
        }
        if (actual.mRotation)
        {
            for (i = 0; i < 4; ++i)
            {
                EXPECT_EQ(std::bit_cast<std::uint32_t>((*actual.mRotation)[i]),
                    std::bit_cast<std::uint32_t>((*expected.mRotation)[i]));
            }
        }
        if (actual.mScale)
        {
            EXPECT_EQ(std::bit_cast<std::uint32_t>(*actual.mScale), std::bit_cast<std::uint32_t>(*expected.mScale));
        }
        EXPECT_EQ(bool(actual.mTranslation || actual.mRotation || actual.mScale), test.get<bool>("expected_valid"));
        ++cases;
    }
    EXPECT_EQ(cases, 18u);
}

TEST(ESM4NativeAnimationBlend, RejectsMalformedFramesBeforeComposition)
{
    std::array<ESM4::SequenceBlendWeight, 2> slots{};
    const float nan = std::numeric_limits<float>::quiet_NaN();
    for (float value : {-1.f, nan, std::numeric_limits<float>::infinity()})
    {
        EXPECT_THROW(ESM4::normalizeSequenceBlendWeights(slots, value), std::invalid_argument);
        auto bad = slots; bad[0].mWeight = value;
        EXPECT_THROW(ESM4::normalizeSequenceBlendWeights(bad), std::invalid_argument);
    }
    slots[0].mEase = 1.01f;
    EXPECT_THROW(ESM4::normalizeSequenceBlendWeights(slots), std::invalid_argument);
    EXPECT_THROW(ESM4::normalizeSequenceBlendWeights(std::vector<ESM4::SequenceBlendWeight>(256)), std::invalid_argument);
    std::array<ESM4::SequenceBlendPose, 2> poses{};
    const std::array<float, 2> weights{.5f, .5f};
    poses[0].mRotation = {0, 0, 0, 0};
    EXPECT_THROW(ESM4::blendSequenceTransforms(poses, weights), std::invalid_argument);
    poses[0].mRotation.reset(); poses[0].mTranslation = {nan, 0, 0};
    EXPECT_THROW(ESM4::blendSequenceTransforms(poses, weights), std::invalid_argument);
    EXPECT_THROW(ESM4::blendSequenceTransforms(poses, {}), std::invalid_argument);
    EXPECT_TRUE(ESM4::normalizeSequenceBlendWeights({}).empty());
    EXPECT_EQ(ESM4::blendSequenceTransforms({}, {}), ESM4::SequenceBlendPose{});
}
