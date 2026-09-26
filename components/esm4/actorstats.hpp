#ifndef OPENMW_ESM4_ACTORSTATS_H
#define OPENMW_ESM4_ACTORSTATS_H

#include "actor.hpp"

#include <cstdint>
#include <optional>

namespace ESM4
{
    // Pure native ACBS lookup. The optional player level represents its resolved
    // base-record field. Fixed levels bypass offset clamps. The returned signed
    // word preserves native wrapping and bounds order, including malformed raw
    // records; callers must diagnose unsupported effective actor configurations.
    std::int16_t resolveActorLevel(const ACBS_TES4& base, std::optional<std::uint16_t> playerLevel);

    struct NpcDynamicStatsSettings
    {
        float mAttributeHealthMultiplier;
        std::int32_t mPerLevelHealthMultiplier;
        std::int32_t mLowLevelMaximum;
        float mLowLevelHealthMultiplier;
        float mMagickaMultiplier;
    };

    struct NpcDynamicStatsInput
    {
        std::uint8_t mStrength;
        std::uint8_t mIntelligence;
        std::uint8_t mWillpower;
        std::uint8_t mAgility;
        std::uint8_t mEndurance;
        std::int16_t mLevel;
        bool mFavoredEndurance;
        std::uint32_t mSpecialization; // Combat=0, Magic=1, Stealth=2.
    };

    struct NpcDynamicStats
    {
        std::uint32_t mHealth;
        std::uint16_t mMagicka;
        std::uint16_t mFatigue;
    };

    void validateNpcDynamicStatsSettings(const NpcDynamicStatsSettings& settings);
    // Auto-calculated NPC base values, before racial/spell/runtime modifiers.
    // Reject unsupported levels, specialization and overflowing base storage.
    NpcDynamicStats calculateNpcDynamicStats(
        const NpcDynamicStatsInput& input, const NpcDynamicStatsSettings& settings);
}

#endif
