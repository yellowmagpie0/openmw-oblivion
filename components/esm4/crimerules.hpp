#ifndef OPENMW_ESM4_CRIMERULES_H
#define OPENMW_ESM4_CRIMERULES_H

#include <cstdint>
#include <span>

namespace ESM4
{
    enum class CrimeOffense { Theft, Pickpocket, Trespass, Assault, Murder, HorseTheft, JailBreak };
    struct CrimeFineSettings
    {
        float mTheftMultiplier;
        std::int32_t mPickpocket;
        std::int32_t mTrespass;
        std::int32_t mAssault;
        std::int32_t mMurder;
        std::int32_t mHorseTheft;
        std::int32_t mJailBreak;
    };
    struct JailSettings
    {
        std::int32_t mGoldPerDay;
    };
    struct JailSentence
    {
        std::int32_t mDays;
        std::int32_t mHours;
        std::uint32_t mSkillChecks; // Attempts, not guaranteed changes or unique skills.
    };
    // Base per-incident fine, not legality, witness reporting or committed bounty.
    // Theft value comes from the incident item or its supplied value fallback.
    float baseCrimeFine(CrimeOffense offense, std::int32_t itemValue, const CrimeFineSettings& settings);
    float factionCrimeFine(float baseFine, std::span<const float> factionMultipliers);
    JailSentence jailSentence(float bounty, const JailSettings& settings);
    void validateCrimeFineSettings(const CrimeFineSettings& settings);
    void validateJailSettings(const JailSettings& settings);
}

#endif
