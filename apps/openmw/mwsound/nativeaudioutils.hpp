#ifndef OPENMW_MWSOUND_NATIVEAUDIOUTILS_H
#define OPENMW_MWSOUND_NATIVEAUDIOUTILS_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace MWSound
{
    struct NativeSoundParams
    {
        float mVolume;
        float mMinDistance;
        float mMaxDistance;
    };

    inline NativeSoundParams makeNativeSoundParams(std::uint16_t staticAttenuation, std::uint8_t minAttenuation,
        std::uint8_t maxAttenuation, float defaultMinDistance, float defaultMaxDistance, float minDistanceMultiplier,
        float maxDistanceMultiplier)
    {
        float minDistance = minAttenuation;
        float maxDistance = maxAttenuation;
        if (minAttenuation == 0 && maxAttenuation == 0)
        {
            minDistance = defaultMinDistance;
            maxDistance = defaultMaxDistance;
        }
        minDistance = std::max(1.f, minDistance * minDistanceMultiplier);
        maxDistance = std::max(minDistance, maxDistance * maxDistanceMultiplier);
        return { std::pow(10.f, -static_cast<float>(staticAttenuation) / 2000.f), minDistance, maxDistance };
    }

    inline std::string_view nativeMusicPlaylist(bool inCombat, bool isExterior, std::uint8_t musicType)
    {
        if (inCombat)
            return "music/battle";
        if (!isExterior && musicType == 2)
            return "music/dungeon";
        if (!isExterior && musicType == 1)
            return "music/public";
        return isExterior ? "music/explore" : "music/dungeon";
    }

    inline std::uint32_t nativeRegionWeatherMask(std::uint8_t weatherClassification)
    {
        constexpr std::uint8_t cloudy = 2;
        constexpr std::uint8_t rainy = 4;
        constexpr std::uint8_t snow = 8;
        if ((weatherClassification & snow) != 0)
            return 8;
        if ((weatherClassification & rainy) != 0)
            return 4;
        if ((weatherClassification & cloudy) != 0)
            return 2;
        return 1;
    }

    inline std::uint32_t nativeRegionSoundChance(std::uint32_t encodedChance)
    {
        // TES4 RDSD stores percent in units of 1/100000 (1,500,000 is 15%).
        return std::min(encodedChance / 100000, 100u);
    }

    inline bool nativeRegionSoundMatches(
        std::uint32_t flags, std::uint32_t encodedChance, std::uint32_t weatherMask, std::uint32_t roll)
    {
        return (flags & weatherMask) != 0 && roll < nativeRegionSoundChance(encodedChance);
    }
}

#endif
