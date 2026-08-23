#include <gtest/gtest.h>

#include <components/esm4/ragdoll.hpp>

#include <cstdint>
#include <limits>
#include <sstream>

namespace
{
    template <class T>
    void write(std::ostream& stream, const T& value)
    {
        stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    TEST(ESM4Ragdoll, decodesRootAndBoneTransforms)
    {
        std::ostringstream output(std::ios::binary);
        write(output, std::uint32_t{ 2 });
        for (float value : { 1.f, 2.f, 3.f, 0.1f, 0.2f, 0.3f })
            write(output, value);
        write(output, std::uint32_t{ 15 });
        for (float value : { 4.f, 5.f, 6.f, 0.4f, 0.5f, 0.6f })
            write(output, value);
        std::istringstream input(output.str(), std::ios::binary);

        const ESM4::RagdollPose pose = ESM4::loadRagdollPose(input, output.str().size());
        EXPECT_EQ(pose.mRootMetadata, 2u);
        EXPECT_EQ(pose.mRootPosition, (std::array<float, 3>{ 1.f, 2.f, 3.f }));
        ASSERT_EQ(pose.mBones.size(), 1u);
        EXPECT_EQ(pose.mBones[0].mBone, 15u);
        EXPECT_EQ(pose.mBones[0].mRotation, (std::array<float, 3>{ 0.4f, 0.5f, 0.6f }));
    }

    TEST(ESM4Ragdoll, rejectsInvalidSizeAndNonFiniteValues)
    {
        std::istringstream shortInput(std::string(27, '\0'), std::ios::binary);
        EXPECT_THROW(ESM4::loadRagdollPose(shortInput, 27), std::runtime_error);

        std::ostringstream output(std::ios::binary);
        write(output, std::uint32_t{ 2 });
        write(output, std::numeric_limits<float>::infinity());
        for (int i = 0; i < 5; ++i)
            write(output, 0.f);
        std::istringstream invalid(output.str(), std::ios::binary);
        EXPECT_THROW(ESM4::loadRagdollPose(invalid, output.str().size()), std::runtime_error);
    }

    TEST(ESM4Ragdoll, skipsOfficialUnavailableBoneSentinel)
    {
        std::ostringstream output(std::ios::binary);
        write(output, std::uint32_t{ 2 });
        for (float value : { 0.f, 0.f, 0.f, 0.1f, 0.2f, 0.3f })
            write(output, value);
        write(output, std::uint32_t{ 3 });
        for (int i = 0; i < 4; ++i)
            write(output, std::numeric_limits<float>::quiet_NaN());
        write(output, 1.57079632679f);
        write(output, 0.f);
        write(output, std::uint32_t{ 15 });
        for (float value : { 4.f, 5.f, 6.f, 0.4f, 0.5f, 0.6f })
            write(output, value);
        std::istringstream input(output.str(), std::ios::binary);

        const ESM4::RagdollPose pose = ESM4::loadRagdollPose(input, output.str().size());
        ASSERT_EQ(pose.mBones.size(), 1u);
        EXPECT_EQ(pose.mBones[0].mBone, 15u);
    }

    TEST(ESM4Ragdoll, mapsBipedIndicesAndIgnoresSavedFlagBits)
    {
        EXPECT_EQ(ESM4::getTes4RagdollBoneName(5), "Bip01 L UpperArm");
        EXPECT_EQ(ESM4::getTes4RagdollBoneName(8), "Bip01 L Thigh");
        EXPECT_EQ(ESM4::getTes4RagdollBoneName(10), "Bip01 L Foot");
        EXPECT_EQ(ESM4::getTes4RagdollBoneName(11), "Bip01 R UpperArm");
        EXPECT_EQ(ESM4::getTes4RagdollBoneName(0x10000012), "Bip01 Head");
        EXPECT_TRUE(ESM4::getTes4RagdollBoneName(19).empty());
    }
}
