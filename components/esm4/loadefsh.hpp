#ifndef OPENMW_COMPONENTS_ESM4_LOADEFSH_H
#define OPENMW_COMPONENTS_ESM4_LOADEFSH_H

#include <array>
#include <optional>
#include <span>

#include <components/esm/defs.hpp>

#include "loadrawrecord.hpp"

namespace ESM4
{
    struct EffectShaderEnvelope
    {
        std::array<std::uint8_t, 4> mColor{};
        float mFadeIn = 0;
        float mFullTime = 0;
        float mFadeOut = 0;
        float mPersistentRatio = 0;
        float mPulseAmplitude = 0;
        float mPulseFrequency = 0;
    };

    struct EffectShaderParticles
    {
        std::uint32_t mSourceBlend = 0;
        std::uint32_t mBlendOperation = 0;
        std::uint32_t mZTest = 0;
        std::uint32_t mDestinationBlend = 0;
        float mBirthRampUp = 0;
        float mBirthFullTime = 0;
        float mBirthRampDown = 0;
        float mBirthFullRatio = 0;
        float mBirthPersistentRatio = 0;
        float mLifetime = 0;
        float mLifetimeVariation = 0;
        float mNormalSpeed = 0;
        float mNormalAcceleration = 0;
        std::array<float, 3> mVelocity{};
        std::array<float, 3> mAcceleration{};
        std::array<float, 2> mScale{};
        std::array<float, 2> mScaleTime{};
        std::array<std::array<std::uint8_t, 4>, 3> mColor{};
        std::array<float, 3> mAlpha{};
        std::array<float, 3> mColorTime{};
    };

    struct EffectShaderData
    {
        enum Flag : std::uint8_t
        {
            NoMembrane = 0x01,
            NoParticles = 0x08,
            InverseEdge = 0x10,
            SkinOnly = 0x20,
        };

        std::uint8_t mFlags = 0;
        std::uint32_t mSourceBlend = 0;
        std::uint32_t mBlendOperation = 0;
        std::uint32_t mZTest = 0;
        EffectShaderEnvelope mFill;
        std::array<float, 2> mTextureSpeed{};
        float mEdgeFalloff = 0;
        EffectShaderEnvelope mEdge;
        float mFillFullAlpha = 0;
        float mEdgeFullAlpha = 0;
        std::uint32_t mDestinationBlend = 0;
        // The two original 96-byte records do not author particle data.
        // Keep absence explicit rather than manufacturing renderer defaults.
        std::optional<EffectShaderParticles> mParticles;
    };

    EffectShaderData decodeEffectShaderData(std::span<const std::uint8_t> bytes);

    struct EffectShader : RawRecord
    {
        std::string mFillTexture;
        std::string mParticleTexture;
        std::optional<EffectShaderData> mData;

        void load(Reader& reader);
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_EFSH4;
    };
}

#endif
