#ifndef OPENMW_ESM4_CRIMERULES_H
#define OPENMW_ESM4_CRIMERULES_H

#include <cstdint>
#include <span>
#include <optional>

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
    // One original 15-bit draw per call. Null starts an attempt; candidates
    // below 12 require another draw. Results 12..20 are actor values, not
    // zero-based skill indices. Completed selections must not consume a draw.
    std::uint8_t advanceJailSkillSelection(std::optional<std::uint8_t> candidate, std::uint32_t draw);
    // Only the reachable penalty branch (selected actor values 12..20).
    // Null means the attempt consumes a check but does not change the skill.
    std::optional<std::uint8_t> jailSkillBaseAfterPenalty(std::uint8_t base, float current);
    void validateCrimeFineSettings(const CrimeFineSettings& settings);
    void validateJailSettings(const JailSettings& settings);
}

#endif
