#ifndef OPENMW_ESM4_MAGICCASTER_H
#define OPENMW_ESM4_MAGICCASTER_H

#include <cstdint>

namespace ESM4
{
    struct SpellCostSettings
    {
        // Compiled defaults; callers resolve winning GMST overrides.
        float mAreaMultiplier = .15f;
        float mDurationMultiplier = .1f;
        float mMagnitudeExponent = 1.25f;
        float mTargetMultiplier = 1.5f;
        float mSkillBase = .1f;
        float mSkillMultiplier = 1.2f;
        float mLuckMultiplier = .4f;
        std::int32_t mLuckBase = -20;
    };

    struct SpellEffectCostInput
    {
        float mBaseCost;
        std::uint32_t mMagnitude;
        std::uint32_t mDuration;
        std::uint32_t mArea;
        bool mTarget;
    };

    // Inputs have already applied MGEF NoMagnitude/NoDuration/NoArea flags
    // and self-range area suppression. Script effects and school selection
    // belong to the caller. These queries do not debit Magicka or cast.
    float spellEffectMagickaCost(const SpellEffectCostInput& effect, std::int32_t skill,
        std::int32_t luck, const SpellCostSettings& settings);
    // Spell flag bit 0 selects stored cost, which keeps its fractional
    // skill-adjusted result instead of the calculated per-effect minimum.
    float manualSpellMagickaCost(std::uint32_t cost, std::int32_t skill,
        std::int32_t luck, const SpellCostSettings& settings);

    struct CombatSpellPowerSettings
    {
        float mFatigueBase = 0.f;
        float mFatigueMultiplier = .25f;
        float mArmorPenaltyMinimum = .05f;
        float mArmorPenaltyMaximum = .3f;
        std::int32_t mJourneymanMinimum = 50;
    };
    struct PlayerCombatSpellPower
    {
        std::int32_t mCurrentFatigue;
        std::int32_t mBaseFatigue;
        std::int32_t mLightArmor;
        std::int32_t mHeavyArmor;
        std::int32_t mLightCoverage;
        std::int32_t mHeavyCoverage;
    };
    // Player MagicCaster virtual+2C with ignore-fatigue=false and override=0.
    // Inputs are native integer getters and separately capped coverage sums;
    // Luck does not modify the armor skills on this path. NPC/Creature and
    // Player god mode return one in their caller. General casting flags and
    // explicit Fatigue overrides are outside this narrow self-cast adapter.
    float playerCombatSpellPower(const PlayerCombatSpellPower& input, const CombatSpellPowerSettings& settings);
    void validateCombatSpellPowerSettings(const CombatSpellPowerSettings& settings);
}
#endif
