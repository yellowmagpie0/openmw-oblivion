#ifndef OPENMW_ESM4_ACTORSTATS_H
#define OPENMW_ESM4_ACTORSTATS_H

#include "actor.hpp"

#include <cstdint>
#include <array>
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

    struct NpcAutoStatsSettings
    {
        float mPrimaryAttributeBonus;
        float mSecondaryAttributeBonus;
    };

    struct NpcSkillDefinition
    {
        std::uint32_t mGoverningAttribute;
        std::uint32_t mSpecialization;
    };

    struct NpcRaceSkillBonus
    {
        std::int8_t mSkill = -1; // Native AV 12..32, -1 for an unused pair.
        std::int8_t mBonus = 0;
    };

    struct NpcAutoStatsInput
    {
        std::int16_t mLevel;
        std::array<std::uint8_t, 8> mRaceAttributes; // Resolved actor sex.
        std::uint8_t mAuthoredPersonality;
        std::array<std::uint32_t, 2> mFavoredAttributes;
        std::array<std::uint32_t, 7> mMajorSkills; // Native AVs 12..32.
        std::uint32_t mSpecialization;
        std::array<NpcSkillDefinition, 21> mSkills; // Winning SKILs in AV order.
        std::array<NpcRaceSkillBonus, 7> mRaceBonuses; // Ordered, signed, duplicates retained.
    };

    struct NpcAutoStats
    {
        std::array<std::uint8_t, 8> mAttributes;
        std::array<std::uint8_t, 21> mSkills;
    };

    void validateNpcAutoStatsSettings(const NpcAutoStatsSettings& settings);
    // NPC-only base auto-calculation. Personality retains its authored value.
    // Native byte storage wraps negative rounded values; no lower clamp exists.
    NpcAutoStats calculateNpcAutoStats(const NpcAutoStatsInput& input, const NpcAutoStatsSettings& settings);
    struct CreatureBaseStatsSettings
    {
        float mCombatMultiplier;
        float mMagicMultiplier;
        float mStealthMultiplier;
        float mDamageMultiplier;
    };

    struct CreatureBaseStatsInput
    {
        bool mPlayerLevelOffset;
        std::int16_t mResolvedLevel;
        std::uint8_t mCombat;
        std::uint8_t mMagic;
        std::uint8_t mStealth;
        std::uint16_t mHealth;
        std::uint16_t mMagicka;
        std::uint16_t mFatigue;
        std::uint16_t mDamage;
    };

    struct CreatureBaseStats
    {
        std::uint8_t mCombat;
        std::uint8_t mMagic;
        std::uint8_t mStealth;
        std::uint16_t mHealth;
        std::uint16_t mMagicka;
        std::uint16_t mFatigue;
        std::uint16_t mDamage;
    };

    void validateCreatureBaseStatsSettings(const CreatureBaseStatsSettings& settings);
    // Native CREA getters scale only PC-level-offset records. Attributes stay
    // authored. Skill/damage conversion truncates then wraps byte/word storage.
    CreatureBaseStats calculateCreatureBaseStats(
        const CreatureBaseStatsInput& input, const CreatureBaseStatsSettings& settings);
}

#endif
