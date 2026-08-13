#ifndef OPENMW_COMPONENTS_ESM4_LOADWTHR_H
#define OPENMW_COMPONENTS_ESM4_LOADWTHR_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>
#include <components/esm/path.hpp>

namespace ESM4
{
    class Reader;

    struct Weather
    {
        enum ColorType : std::size_t
        {
            Color_SkyUpper,
            Color_Fog,
            Color_CloudsLower,
            Color_Ambient,
            Color_Sunlight,
            Color_Sun,
            Color_Stars,
            Color_SkyLower,
            Color_Horizon,
            Color_CloudsUpper,
            Color_Count,
        };

        enum Time : std::size_t
        {
            Time_Sunrise,
            Time_Day,
            Time_Sunset,
            Time_Night,
            Time_Count,
        };

        enum Classification : std::uint8_t
        {
            Classification_Pleasant = 1,
            Classification_Cloudy = 2,
            Classification_Rainy = 4,
            Classification_Snow = 8,
        };

#pragma pack(push, 1)
        struct FogDistance
        {
            float mDayNear = 0.f;
            float mDayFar = 0.f;
            float mNightNear = 0.f;
            float mNightFar = 0.f;
        };

        struct HdrData
        {
            std::array<float, 14> mValues{};
        };

        struct Data
        {
            std::uint8_t mWindSpeed = 0;
            std::uint8_t mLowerCloudSpeed = 0;
            std::uint8_t mUpperCloudSpeed = 0;
            std::uint8_t mTransitionDelta = 0;
            std::uint8_t mSunGlare = 0;
            std::uint8_t mSunDamage = 0;
            std::uint8_t mPrecipitationBeginFadeIn = 0;
            std::uint8_t mPrecipitationEndFadeOut = 0;
            std::uint8_t mLightningBeginFadeIn = 0;
            std::uint8_t mLightningEndFadeOut = 0;
            std::uint8_t mLightningFrequency = 0;
            std::uint8_t mClassification = 0;
            std::uint8_t mLightningRed = 0;
            std::uint8_t mLightningGreen = 0;
            std::uint8_t mLightningBlue = 0;
        };
#pragma pack(pop)

        struct Sound
        {
            ESM::FormId mSound;
            std::uint32_t mType = 0;
        };

        ESM::FormId mId;
        std::uint32_t mFlags = 0;
        std::string mEditorId;
        std::string mLowerCloudTexture;
        std::string mUpperCloudTexture;
        ESM::Path mModel;
        float mBoundRadius = 0.f;
        std::array<std::array<std::uint32_t, Time_Count>, Color_Count> mColors{};
        FogDistance mFog{};
        HdrData mHdr{};
        Data mData{};
        std::vector<Sound> mSounds;

        void load(Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::RecNameInts::REC_WTHR4;
    };
}

#endif
