#include "regionsoundselector.hpp"

#include <components/esm3/loadregn.hpp>
#include <components/esm4/loadregn.hpp>
#include <components/esm4/loadwthr.hpp>
#include <components/fallback/fallback.hpp>
#include <components/misc/rng.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/cell.hpp"
#include "../mwworld/weather.hpp"

namespace MWSound
{
    RegionSoundSelector::RegionSoundSelector()
        : mMinTimeBetweenSounds(Fallback::Map::getFloat("Weather_Minimum_Time_Between_Environmental_Sounds"))
        , mMaxTimeBetweenSounds(Fallback::Map::getFloat("Weather_Maximum_Time_Between_Environmental_Sounds"))
    {
    }

    ESM::RefId RegionSoundSelector::getNextRandom(float duration, const MWWorld::Cell& cell)
    {
        mTimePassed += duration;

        if (mTimePassed < mTimeToNextEnvSound)
            return {};

        const float a = Misc::Rng::rollClosedProbability();
        mTimeToNextEnvSound = mMinTimeBetweenSounds + (mMaxTimeBetweenSounds - mMinTimeBetweenSounds) * a;
        mTimePassed = 0;

        const auto store = MWBase::Environment::get().getESMStore();
        if (cell.isEsm4())
        {
            const ESM4::Region* region = store->get<ESM4::Region>().search(cell.getRegion());
            if (region == nullptr)
                return {};
            const std::uint8_t classification = MWBase::Environment::get().getWorld()->getCurrentWeather().mNativeClassification;
            std::uint32_t weatherFlag = 0;
            if ((classification & ESM4::Weather::Classification_Snow) != 0)
                weatherFlag = 3;
            else if ((classification & ESM4::Weather::Classification_Rainy) != 0)
                weatherFlag = 2;
            else if ((classification & ESM4::Weather::Classification_Cloudy) != 0)
                weatherFlag = 1;
            for (const ESM4::Region::RegionSound& sound : region->mSounds)
            {
                if (sound.flags == weatherFlag && static_cast<std::uint32_t>(Misc::Rng::roll0to99()) < sound.chance)
                    return ESM::RefId(sound.sound);
            }
            return {};
        }

        const ESM::Region* const region = store->get<ESM::Region>().search(cell.getRegion());

        if (region == nullptr)
            return {};

        for (const ESM::Region::SoundRef& sound : region->mSoundList)
        {
            if (Misc::Rng::roll0to99() < sound.mChance)
                return sound.mSound;
        }
        return {};
    }
}
