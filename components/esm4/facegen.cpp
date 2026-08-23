#include "facegen.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace
{
    constexpr std::uint32_t sMaxVertices = 2'000'000;
    constexpr std::uint32_t sMaxFaces = 4'000'000;
    constexpr std::uint32_t sMaxMorphs = 1024;
    constexpr std::uint32_t sMaxString = 1 << 20;
    constexpr std::uint64_t sMaxTextureBytes = 512ull * 1024ull * 1024ull;

    [[noreturn]] void fail(std::string_view format, std::string_view reason)
    {
        throw std::runtime_error(std::string(format) + " FaceGen file: " + std::string(reason));
    }

    void readBytes(std::istream& stream, void* target, std::size_t size, std::string_view format)
    {
        if (size == 0)
            return;
        stream.read(static_cast<char*>(target), static_cast<std::streamsize>(size));
        if (!stream)
            fail(format, "unexpected end of file");
    }

    template <class T>
    T read(std::istream& stream, std::string_view format)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        T result{};
        readBytes(stream, &result, sizeof(result), format);
        return result;
    }

    void expectHeader(std::istream& stream, std::string_view signature, std::string_view version)
    {
        std::array<char, 8> header{};
        readBytes(stream, header.data(), header.size(), signature);
        if (std::string_view(header.data(), 5) != signature || std::string_view(header.data() + 5, 3) != version)
            fail(signature, "unsupported signature or version");
    }

    std::uint32_t count(std::istream& stream, std::string_view format, std::uint32_t maximum)
    {
        const std::int32_t value = read<std::int32_t>(stream, format);
        if (value < 0 || static_cast<std::uint32_t>(value) > maximum)
            fail(format, "invalid element count");
        return static_cast<std::uint32_t>(value);
    }

    void skip(std::istream& stream, std::uint64_t bytes, std::string_view format)
    {
        if (bytes > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
            fail(format, "block is too large");
        stream.ignore(static_cast<std::streamsize>(bytes));
        if (!stream)
            fail(format, "unexpected end of file");
    }

    std::string readString(std::istream& stream, std::string_view format)
    {
        const std::uint32_t size = read<std::uint32_t>(stream, format);
        if (size > sMaxString)
            fail(format, "string is too large");
        std::string result(size, '\0');
        readBytes(stream, result.data(), result.size(), format);
        while (!result.empty() && result.back() == '\0')
            result.pop_back();
        return result;
    }

    ESM4::FaceGenMorph readMorph(
        std::istream& stream, std::uint32_t vertexCount, std::string_view format, bool named)
    {
        ESM4::FaceGenMorph result;
        if (named)
            result.mName = readString(stream, format);
        result.mScale = read<float>(stream, format);
        if (!std::isfinite(result.mScale))
            fail(format, "non-finite morph scale");
        result.mVertices.resize(vertexCount);
        readBytes(stream, result.mVertices.data(), result.mVertices.size() * sizeof(ESM4::FaceGenDelta), format);
        return result;
    }
}

namespace ESM4
{
    FaceGenTri loadFaceGenTri(std::istream& stream)
    {
        constexpr std::string_view format = "TRI";
        expectHeader(stream, "FRTRI", "003");

        FaceGenTri result;
        const std::uint32_t vertexCount = count(stream, format, sMaxVertices);
        result.mTriangleCount = count(stream, format, sMaxFaces);
        const std::uint32_t quadCount = count(stream, format, sMaxFaces);
        skip(stream, 2 * sizeof(std::int32_t), format);
        const std::uint32_t uvCount = count(stream, format, sMaxVertices);
        const std::uint32_t hasUv = read<std::uint32_t>(stream, format);
        const std::uint32_t morphCount = count(stream, format, sMaxMorphs);
        result.mModifierCount = count(stream, format, sMaxMorphs);
        const std::uint32_t modifierVertexCount = count(stream, format, sMaxVertices);
        skip(stream, 4 * sizeof(std::int32_t), format);

        result.mVertices.resize(vertexCount);
        readBytes(stream, result.mVertices.data(), result.mVertices.size() * sizeof(FaceGenVector), format);
        skip(stream, static_cast<std::uint64_t>(modifierVertexCount) * sizeof(FaceGenVector), format);
        skip(stream, static_cast<std::uint64_t>(result.mTriangleCount) * 3 * sizeof(std::int32_t), format);
        skip(stream, static_cast<std::uint64_t>(quadCount) * 4 * sizeof(std::int32_t), format);
        skip(stream, static_cast<std::uint64_t>(uvCount) * 2 * sizeof(float), format);
        if (hasUv != 0)
        {
            skip(stream, static_cast<std::uint64_t>(result.mTriangleCount) * 3 * sizeof(std::int32_t), format);
            skip(stream, static_cast<std::uint64_t>(quadCount) * 4 * sizeof(std::int32_t), format);
        }

        result.mMorphs.reserve(morphCount);
        for (std::uint32_t i = 0; i < morphCount; ++i)
            result.mMorphs.push_back(readMorph(stream, vertexCount, format, true));

        for (std::uint32_t i = 0; i < result.mModifierCount; ++i)
        {
            (void)readString(stream, format);
            const std::uint32_t vertices = count(stream, format, sMaxVertices);
            skip(stream, static_cast<std::uint64_t>(vertices) * sizeof(std::int32_t), format);
        }
        return result;
    }

    FaceGenEgm loadFaceGenEgm(std::istream& stream)
    {
        constexpr std::string_view format = "EGM";
        expectHeader(stream, "FREGM", "002");

        FaceGenEgm result;
        result.mVertexCount = count(stream, format, sMaxVertices);
        const std::uint32_t symmetric = count(stream, format, sMaxMorphs);
        const std::uint32_t asymmetric = count(stream, format, sMaxMorphs);
        skip(stream, sizeof(std::uint32_t) + 10 * sizeof(std::int32_t), format);

        result.mSymmetricMorphs.reserve(symmetric);
        result.mAsymmetricMorphs.reserve(asymmetric);
        for (std::uint32_t i = 0; i < symmetric; ++i)
            result.mSymmetricMorphs.push_back(readMorph(stream, result.mVertexCount, format, false));
        for (std::uint32_t i = 0; i < asymmetric; ++i)
            result.mAsymmetricMorphs.push_back(readMorph(stream, result.mVertexCount, format, false));
        return result;
    }

    FaceGenEgt loadFaceGenEgt(std::istream& stream)
    {
        constexpr std::string_view format = "EGT";
        expectHeader(stream, "FREGT", "003");

        FaceGenEgt result;
        result.mHeight = count(stream, format, 16384);
        result.mWidth = count(stream, format, 16384);
        const std::uint32_t symmetricCount = count(stream, format, sMaxMorphs);
        const std::uint32_t asymmetricCount = count(stream, format, sMaxMorphs);
        skip(stream, sizeof(std::uint32_t) + 9 * sizeof(std::int32_t), format);

        const std::uint64_t pixels = static_cast<std::uint64_t>(result.mWidth) * result.mHeight;
        if (pixels * (symmetricCount + asymmetricCount) * 3 > sMaxTextureBytes)
            fail(format, "texture data is too large");
        const auto readTextures = [&](std::vector<FaceGenTextureMode>& textures, std::uint32_t textureCount) {
            textures.resize(textureCount);
            for (FaceGenTextureMode& texture : textures)
            {
                texture.mScale = read<float>(stream, format);
                if (!std::isfinite(texture.mScale))
                    fail(format, "non-finite texture scale");
                texture.mRed.resize(pixels);
                texture.mGreen.resize(pixels);
                texture.mBlue.resize(pixels);
                readBytes(stream, texture.mRed.data(), pixels, format);
                readBytes(stream, texture.mGreen.data(), pixels, format);
                readBytes(stream, texture.mBlue.data(), pixels, format);
            }
        };
        readTextures(result.mSymmetricTextures, symmetricCount);
        readTextures(result.mAsymmetricTextures, asymmetricCount);
        return result;
    }

    FaceGenLip loadFaceGenLip(std::istream& stream)
    {
        constexpr std::string_view format = "LIP";
        FaceGenLip result;
        result.mVersion = read<std::uint32_t>(stream, format);
        result.mDurationTicks = read<std::uint32_t>(stream, format);
        result.mCurveCount = read<std::uint32_t>(stream, format);
        if (result.mVersion != 1)
            fail(format, "unsupported version");
        if (result.mDurationTicks == 0 || result.mCurveCount == 0 || result.mCurveCount > sMaxMorphs)
            fail(format, "invalid animation header");
        result.mPayload.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        if (result.mPayload.empty())
            fail(format, "missing animation payload");
        return result;
    }
}
