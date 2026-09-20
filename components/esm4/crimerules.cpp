#include "crimerules.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ESM4
{
    namespace
    {
        void nonnegative(float value)
        {
            if (!std::isfinite(value) || value < 0)
                throw std::invalid_argument("invalid native crime amount");
        }
    }

    void validateCrimeFineSettings(const CrimeFineSettings& settings)
    {
        nonnegative(settings.mTheftMultiplier);
        for (auto value : {settings.mPickpocket, settings.mTrespass, settings.mAssault,
                 settings.mMurder, settings.mHorseTheft, settings.mJailBreak})
            if (value < 0)
                throw std::invalid_argument("negative native crime fine setting");
    }

    void validateJailSettings(const JailSettings& settings)
    {
        if (settings.mGoldPerDay <= 0)
            throw std::invalid_argument("invalid native jail sentence divisor");
    }

    float baseCrimeFine(CrimeOffense offense, std::int32_t itemValue, const CrimeFineSettings& settings)
    {
        validateCrimeFineSettings(settings);
        switch (offense)
        {
            case CrimeOffense::Theft:
            {
                if (itemValue < 0)
                    throw std::invalid_argument("invalid native stolen item value");
                const double fine = double(itemValue == 0 ? 1 : itemValue) * settings.mTheftMultiplier;
                if (fine > std::numeric_limits<float>::max())
                    throw std::invalid_argument("native theft fine overflow");
                return static_cast<float>(fine);
            }
            case CrimeOffense::Pickpocket: return static_cast<float>(settings.mPickpocket);
            case CrimeOffense::Trespass: return static_cast<float>(settings.mTrespass);
            case CrimeOffense::Assault: return static_cast<float>(settings.mAssault);
            case CrimeOffense::Murder: return static_cast<float>(settings.mMurder);
            case CrimeOffense::HorseTheft: return static_cast<float>(settings.mHorseTheft);
            case CrimeOffense::JailBreak: return static_cast<float>(settings.mJailBreak);
        }
        throw std::invalid_argument("invalid native crime offense");
    }

    float factionCrimeFine(float baseFine, std::span<const float> factionMultipliers)
    {
        nonnegative(baseFine);
        float multiplier = 1.f;
        for (float value : factionMultipliers)
        {
            nonnegative(value);
            multiplier = std::max(multiplier, value);
        }
        const double result = double(baseFine) * multiplier;
        if (result > std::numeric_limits<float>::max())
            throw std::invalid_argument("native faction fine overflow");
        return static_cast<float>(result);
    }

    JailSentence jailSentence(float bounty, const JailSettings& settings)
    {
        validateJailSettings(settings);
        nonnegative(bounty);
        const double days = std::max(1.0, std::trunc(double(bounty) / settings.mGoldPerDay));
        if (days > std::numeric_limits<std::int32_t>::max() / 24)
            throw std::invalid_argument("native jail time integer overflow");
        const auto count = static_cast<std::int32_t>(days);
        return {count, count * 24, static_cast<std::uint32_t>(std::min(count, 10))};
    }
}
