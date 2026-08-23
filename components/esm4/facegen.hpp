#ifndef OPENMW_COMPONENTS_ESM4_FACEGEN_H
#define OPENMW_COMPONENTS_ESM4_FACEGEN_H

#include <cstdint>
#include <istream>
#include <string>
#include <vector>

namespace ESM4
{
    struct FaceGenVector
    {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
    };

    struct FaceGenDelta
    {
        std::int16_t x = 0;
        std::int16_t y = 0;
        std::int16_t z = 0;
    };

    struct FaceGenMorph
    {
        std::string mName;
        float mScale = 0.f;
        std::vector<FaceGenDelta> mVertices;
    };

    struct FaceGenTri
    {
        std::vector<FaceGenVector> mVertices;
        std::vector<FaceGenMorph> mMorphs;
        std::uint32_t mTriangleCount = 0;
        std::uint32_t mModifierCount = 0;
    };

    struct FaceGenEgm
    {
        std::uint32_t mVertexCount = 0;
        std::vector<FaceGenMorph> mSymmetricMorphs;
        std::vector<FaceGenMorph> mAsymmetricMorphs;
    };

    struct FaceGenTextureMode
    {
        float mScale = 0.f;
        std::vector<std::int8_t> mRed;
        std::vector<std::int8_t> mGreen;
        std::vector<std::int8_t> mBlue;
    };

    struct FaceGenEgt
    {
        std::uint32_t mWidth = 0;
        std::uint32_t mHeight = 0;
        std::vector<FaceGenTextureMode> mSymmetricTextures;
        std::vector<FaceGenTextureMode> mAsymmetricTextures;
    };

    struct FaceGenLip
    {
        std::uint32_t mVersion = 0;
        std::uint32_t mDurationTicks = 0;
        std::uint32_t mCurveCount = 0;
        // Oblivion embeds a legacy FaceFX archive after the stable 12-byte
        // envelope. Its first bytes are curve-dependent compressed data, not
        // a fixed frame-count header (some official files begin with values
        // that only happen to resemble one). Keep the archive intact so all
        // official encodings remain valid inputs.
        std::vector<std::uint8_t> mPayload;
    };

    FaceGenTri loadFaceGenTri(std::istream& stream);
    FaceGenEgm loadFaceGenEgm(std::istream& stream);
    FaceGenEgt loadFaceGenEgt(std::istream& stream);
    FaceGenLip loadFaceGenLip(std::istream& stream);
}

#endif
