#ifndef OPENMW_ESM_WEATHERSTATE_H
#define OPENMW_ESM_WEATHERSTATE_H

#include <components/esm/refid.hpp>
#include <components/esm/formkey.hpp>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ESM
{
    class ESMReader;
    class ESMWriter;

    struct RegionWeatherState
    {
        int32_t mWeather;
        std::vector<uint8_t> mChances;
        std::vector<int32_t> mSelectionOrder = {};
        int32_t mFallbackWeather = 0;
    };

    struct WeatherState
    {
        ESM::RefId mCurrentRegion;
        float mTimePassed;
        bool mFastForward;
        float mWeatherUpdateTime;
        float mTransitionFactor;
        int32_t mCurrentWeather;
        int32_t mNextWeather;
        int32_t mQueuedWeather;
        bool mWeatherOverride = false;
        std::map<ESM::RefId, RegionWeatherState> mRegions;
        // Absent in legacy and Morrowind saves. Indices in native weather
        // state refer to this ordered, load-order-independent catalog.
        std::optional<std::vector<ESM::FormKey>> mWeatherIdentities = std::nullopt;

        // Native save admission rejects duplicate identities before the map
        // can merge them. Legacy callers retain their first-record behavior.
        void load(ESMReader& esm, bool rejectDuplicateRegions = false);
        void save(ESMWriter& esm) const;
    };

    void validateWeatherIdentityCatalog(std::span<const ESM::FormKey> identities);
}

#endif
