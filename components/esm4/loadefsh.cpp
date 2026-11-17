#include "loadefsh.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

#include "common.hpp"
#include "reader.hpp"

namespace ESM4
{
    namespace
    {
        std::uint32_t integer(std::span<const std::uint8_t> bytes, std::size_t offset)
        {
            return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
                | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
        }

        float number(std::span<const std::uint8_t> bytes, std::size_t offset)
        {
            const float value = std::bit_cast<float>(integer(bytes, offset));
            if (!std::isfinite(value))
                throw std::runtime_error("EFSH nonfinite numeric field at byte " + std::to_string(offset));
            return value;
        }

        std::array<std::uint8_t, 4> color(std::span<const std::uint8_t> bytes, std::size_t offset)
        {
            return { bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3] };
        }

        EffectShaderEnvelope envelope(std::span<const std::uint8_t> bytes, std::size_t offset)
        {
            return { color(bytes, offset), number(bytes, offset + 4), number(bytes, offset + 8),
                number(bytes, offset + 12), number(bytes, offset + 16), number(bytes, offset + 20),
                number(bytes, offset + 24) };
        }

        std::string texture(const RawSubRecord& sub)
        {
            const auto& bytes = sub.mData;
            if (bytes.empty() || bytes.back() != 0
                || std::find(bytes.begin(), bytes.end() - 1, 0) != bytes.end() - 1)
                throw std::runtime_error("EFSH texture must be a terminated string");
            return std::string(bytes.begin(), bytes.end() - 1);
        }
    }

    EffectShaderData decodeEffectShaderData(std::span<const std::uint8_t> bytes)
    {
        if (bytes.size() != 96 && bytes.size() != 224)
            throw std::runtime_error("EFSH DATA has invalid TES4 size " + std::to_string(bytes.size()));
        EffectShaderData value;
        value.mFlags = bytes[0];
        value.mSourceBlend = integer(bytes, 4);
        value.mBlendOperation = integer(bytes, 8);
        value.mZTest = integer(bytes, 12);
        value.mFill = envelope(bytes, 16);
        value.mTextureSpeed = { number(bytes, 44), number(bytes, 48) };
        value.mEdgeFalloff = number(bytes, 52);
        value.mEdge = envelope(bytes, 56);
        value.mFillFullAlpha = number(bytes, 84);
        value.mEdgeFullAlpha = number(bytes, 88);
        value.mDestinationBlend = integer(bytes, 92);
        if (bytes.size() == 224)
        {
            auto& p = value.mParticles.emplace();
            p.mSourceBlend = integer(bytes, 96);
            p.mBlendOperation = integer(bytes, 100);
            p.mZTest = integer(bytes, 104);
            p.mDestinationBlend = integer(bytes, 108);
            p.mBirthRampUp = number(bytes, 112);
            p.mBirthFullTime = number(bytes, 116);
            p.mBirthRampDown = number(bytes, 120);
            p.mBirthFullRatio = number(bytes, 124);
            p.mBirthPersistentRatio = number(bytes, 128);
            p.mLifetime = number(bytes, 132);
            p.mLifetimeVariation = number(bytes, 136);
            p.mNormalSpeed = number(bytes, 140);
            p.mNormalAcceleration = number(bytes, 144);
            for (std::size_t i = 0; i < 3; ++i)
            {
                p.mVelocity[i] = number(bytes, 148 + i * 4);
                p.mAcceleration[i] = number(bytes, 160 + i * 4);
                p.mColor[i] = color(bytes, 188 + i * 4);
                p.mAlpha[i] = number(bytes, 200 + i * 4);
                p.mColorTime[i] = number(bytes, 212 + i * 4);
            }
            for (std::size_t i = 0; i < 2; ++i)
            {
                p.mScale[i] = number(bytes, 172 + i * 4);
                p.mScaleTime[i] = number(bytes, 180 + i * 4);
            }
        }
        return value;
    }

    void EffectShader::load(Reader& reader)
    {
        if (reader.hasFormVersion() || (reader.esmVersionF() != 0.8f && reader.esmVersionF() != 1.f))
            reader.fail("EFSH semantic decoder supports TES4 only");
        *this = {};
        RawRecord::load(reader);
        bool fill = false, particles = false;
        for (const auto& sub : mSubRecords)
        {
            if (sub.mType == ESM::fourCC("DATA"))
            {
                if (mData)
                    reader.fail("EFSH has duplicate DATA");
                mData = decodeEffectShaderData(sub.mData);
            }
            else if (sub.mType == ESM::fourCC("ICON"))
            {
                if (fill)
                    reader.fail("EFSH has duplicate ICON");
                fill = true;
                mFillTexture = texture(sub);
            }
            else if (sub.mType == ESM::fourCC("ICO2"))
            {
                if (particles)
                    reader.fail("EFSH has duplicate ICO2");
                particles = true;
                mParticleTexture = texture(sub);
            }
        }
        if (!mData && !(mFlags & Rec_Deleted))
            reader.fail("EFSH is missing required DATA");
    }
}
