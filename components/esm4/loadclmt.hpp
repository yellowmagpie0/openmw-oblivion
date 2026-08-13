#ifndef OPENMW_COMPONENTS_ESM4_LOADCLMT_H
#define OPENMW_COMPONENTS_ESM4_LOADCLMT_H

#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>
#include <components/esm/path.hpp>

namespace ESM4
{
    class Reader;

    struct Climate
    {
        struct WeatherChance
        {
            ESM::FormId mWeather;
            std::int32_t mChance = 0;
        };

#pragma pack(push, 1)
        struct Timing
        {
            std::uint8_t mSunriseBegin = 36;
            std::uint8_t mSunriseEnd = 42;
            std::uint8_t mSunsetBegin = 108;
            std::uint8_t mSunsetEnd = 114;
            std::uint8_t mVolatility = 0;
            std::uint8_t mMoonsPhase = 0;
        };
#pragma pack(pop)

        enum MoonFlag : std::uint8_t
        {
            Moon_Masser = 0x40,
            Moon_Secunda = 0x80,
        };

        ESM::FormId mId;
        std::uint32_t mFlags = 0;
        std::string mEditorId;
        std::vector<WeatherChance> mWeather;
        std::string mSunTexture;
        std::string mSunGlareTexture;
        ESM::Path mModel;
        float mBoundRadius = 0.f;
        Timing mTiming{};

        void load(Reader& reader);

        [[nodiscard]] static float decodeTime(std::uint8_t value) { return static_cast<float>(value) / 6.f; }
        [[nodiscard]] bool hasMasser() const { return (mTiming.mMoonsPhase & Moon_Masser) != 0; }
        [[nodiscard]] bool hasSecunda() const { return (mTiming.mMoonsPhase & Moon_Secunda) != 0; }
        [[nodiscard]] unsigned phaseLength() const { return mTiming.mMoonsPhase & 0x3f; }

        static constexpr ESM::RecNameInts sRecordId = ESM::RecNameInts::REC_CLMT4;
    };
}

#endif
