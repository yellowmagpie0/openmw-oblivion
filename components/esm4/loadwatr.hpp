#ifndef OPENMW_COMPONENTS_ESM4_LOADWATR_H
#define OPENMW_COMPONENTS_ESM4_LOADWATR_H

#include <array>
#include <cstdint>
#include <string>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    struct Water
    {
        enum Flag : std::uint8_t
        {
            Flag_CausesDamage = 1 << 0,
            Flag_Reflective = 1 << 1,
        };

        struct Data
        {
            float mWindVelocity = 0.f;
            float mWindDirection = 0.f;
            float mWaveAmplitude = 0.f;
            float mWaveFrequency = 0.f;
            float mSunPower = 0.f;
            float mReflectivity = 0.f;
            float mFresnelAmount = 0.f;
            float mScrollX = 0.f;
            float mScrollY = 0.f;
            float mFogNear = 0.f;
            float mFogFar = 0.f;
            std::uint32_t mShallowColor = 0;
            std::uint32_t mDeepColor = 0;
            std::uint32_t mReflectionColor = 0;
            std::uint8_t mTextureBlend = 0;
            std::array<std::uint8_t, 3> mUnused{};
            float mRainForce = 0.f;
            float mRainVelocity = 0.f;
            float mRainFalloff = 0.f;
            float mRainDamper = 0.f;
            float mRainStartingSize = 0.f;
            float mDisplacementForce = 0.f;
            float mDisplacementVelocity = 0.f;
            float mDisplacementFalloff = 0.f;
            float mDisplacementDamper = 0.f;
            float mDisplacementStartingSize = 0.f;
            std::uint16_t mDamage = 0;
        };

        struct RelatedWater
        {
            ESM::FormId mDaytime;
            ESM::FormId mNighttime;
            ESM::FormId mUnderwater;
        };

        ESM::FormId mId;
        std::uint32_t mFlags = 0;
        std::string mEditorId;
        std::string mTexture;
        std::uint8_t mOpacity = 0;
        std::uint8_t mWaterFlags = 0;
        std::string mMaterial;
        ESM::FormId mSound;
        Data mData{};
        RelatedWater mRelatedWater{};

        void load(Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::RecNameInts::REC_WATR4;
    };
}

#endif
