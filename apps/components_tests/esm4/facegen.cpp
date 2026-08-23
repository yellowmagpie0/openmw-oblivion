#include <gtest/gtest.h>

#include <components/esm4/facegen.hpp>

#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
    template <class T>
    void append(std::string& data, T value)
    {
        data.append(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    void appendHeader(std::string& data, const char* signature, const char* version)
    {
        data.append(signature, 5);
        data.append(version, 3);
    }
}

TEST(Esm4FaceGen, LoadsEgmShapeModes)
{
    std::string data;
    appendHeader(data, "FREGM", "002");
    append<std::int32_t>(data, 2);
    append<std::int32_t>(data, 1);
    append<std::int32_t>(data, 1);
    append<std::uint32_t>(data, 2001060901);
    for (int i = 0; i < 10; ++i)
        append<std::int32_t>(data, 0);
    append<float>(data, 0.5f);
    for (int i = 0; i < 6; ++i)
        append<std::int16_t>(data, static_cast<std::int16_t>(i + 1));
    append<float>(data, 0.25f);
    for (int i = 0; i < 6; ++i)
        append<std::int16_t>(data, static_cast<std::int16_t>(-i - 1));

    std::istringstream stream(data);
    const ESM4::FaceGenEgm egm = ESM4::loadFaceGenEgm(stream);
    ASSERT_EQ(egm.mVertexCount, 2u);
    ASSERT_EQ(egm.mSymmetricMorphs.size(), 1u);
    ASSERT_EQ(egm.mAsymmetricMorphs.size(), 1u);
    EXPECT_FLOAT_EQ(egm.mSymmetricMorphs[0].mScale, 0.5f);
    EXPECT_EQ(egm.mSymmetricMorphs[0].mVertices[1].z, 6);
    EXPECT_EQ(egm.mAsymmetricMorphs[0].mVertices[0].x, -1);
}

TEST(Esm4FaceGen, RejectsNonFiniteMorphScale)
{
    std::string data;
    appendHeader(data, "FREGM", "002");
    append<std::int32_t>(data, 0);
    append<std::int32_t>(data, 1);
    append<std::int32_t>(data, 0);
    append<std::uint32_t>(data, 2001060901);
    for (int i = 0; i < 10; ++i)
        append<std::int32_t>(data, 0);
    append<float>(data, std::numeric_limits<float>::infinity());

    std::istringstream stream(data);
    EXPECT_THROW(ESM4::loadFaceGenEgm(stream), std::runtime_error);
}

TEST(Esm4FaceGen, LoadsTriExpressionNamesAndDeltas)
{
    std::string data;
    appendHeader(data, "FRTRI", "003");
    append<std::int32_t>(data, 1); // vertices
    append<std::int32_t>(data, 0); // triangles
    append<std::int32_t>(data, 0); // quads
    append<std::int32_t>(data, 0);
    append<std::int32_t>(data, 0);
    append<std::int32_t>(data, 0); // uvs
    append<std::int32_t>(data, 0); // has uv
    append<std::int32_t>(data, 1); // morphs
    append<std::int32_t>(data, 0); // modifiers
    append<std::int32_t>(data, 0); // modifier vertices
    for (int i = 0; i < 4; ++i)
        append<std::int32_t>(data, 0);
    append<float>(data, 1.f);
    append<float>(data, 2.f);
    append<float>(data, 3.f);
    append<std::uint32_t>(data, 4);
    data.append("Aah\0", 4);
    append<float>(data, 0.1f);
    append<std::int16_t>(data, 4);
    append<std::int16_t>(data, 5);
    append<std::int16_t>(data, 6);

    std::istringstream stream(data);
    const ESM4::FaceGenTri tri = ESM4::loadFaceGenTri(stream);
    ASSERT_EQ(tri.mVertices.size(), 1u);
    ASSERT_EQ(tri.mMorphs.size(), 1u);
    EXPECT_EQ(tri.mMorphs[0].mName, "Aah");
    EXPECT_FLOAT_EQ(tri.mMorphs[0].mScale, 0.1f);
    EXPECT_EQ(tri.mMorphs[0].mVertices[0].z, 6);
}

TEST(Esm4FaceGen, LoadsEgtTextureModesAndRejectsTruncation)
{
    std::string data;
    appendHeader(data, "FREGT", "003");
    append<std::int32_t>(data, 2);
    append<std::int32_t>(data, 1);
    append<std::int32_t>(data, 1);
    append<std::int32_t>(data, 1);
    for (int i = 0; i < 10; ++i)
        append<std::int32_t>(data, 0);
    append<float>(data, 0.25f);
    data.append("\x01\xff", 2);
    data.append("\x02\xfe", 2);
    data.append("\x03\xfd", 2);
    append<float>(data, 0.5f);
    data.append("\x04\xfc", 2);
    data.append("\x05\xfb", 2);
    data.append("\x06\xfa", 2);

    std::istringstream stream(data);
    const ESM4::FaceGenEgt egt = ESM4::loadFaceGenEgt(stream);
    ASSERT_EQ(egt.mSymmetricTextures.size(), 1u);
    ASSERT_EQ(egt.mAsymmetricTextures.size(), 1u);
    EXPECT_EQ(egt.mWidth, 1u);
    EXPECT_EQ(egt.mHeight, 2u);
    EXPECT_FLOAT_EQ(egt.mSymmetricTextures[0].mScale, 0.25f);
    EXPECT_EQ(egt.mSymmetricTextures[0].mRed[1], -1);
    EXPECT_FLOAT_EQ(egt.mAsymmetricTextures[0].mScale, 0.5f);
    EXPECT_EQ(egt.mAsymmetricTextures[0].mBlue[1], -6);

    data.pop_back();
    std::istringstream truncated(data);
    EXPECT_THROW(ESM4::loadFaceGenEgt(truncated), std::runtime_error);
}

TEST(Esm4FaceGen, ValidatesOblivionLipEnvelope)
{
    std::string data;
    append<std::uint32_t>(data, 1);
    append<std::uint32_t>(data, 10056);
    append<std::uint32_t>(data, 1);
    // The following bytes are the start of the opaque legacy FaceFX archive.
    // In this official encoding they happen to look like a 76/3/-9 header;
    // other valid Oblivion files use values such as 139 here.
    append<std::uint16_t>(data, 76);
    append<std::uint16_t>(data, 3);
    append<std::int32_t>(data, -9);
    data.append("face", 4);
    std::istringstream stream(data);
    const ESM4::FaceGenLip lip = ESM4::loadFaceGenLip(stream);
    EXPECT_EQ(lip.mVersion, 1u);
    EXPECT_EQ(lip.mDurationTicks, 10056u);
    EXPECT_EQ(lip.mCurveCount, 1u);
    EXPECT_EQ(lip.mPayload.size(), 12u);

    data[14] = static_cast<char>(0x8b);
    std::istringstream alternateOfficialEncoding(data);
    EXPECT_NO_THROW(ESM4::loadFaceGenLip(alternateOfficialEncoding));

    data[0] = 2;
    std::istringstream invalid(data);
    EXPECT_THROW(ESM4::loadFaceGenLip(invalid), std::runtime_error);
}
