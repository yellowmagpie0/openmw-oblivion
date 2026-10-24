#ifndef OPENMW_ESM4_MAGICCASTER_H
#define OPENMW_ESM4_MAGICCASTER_H

#include <cstdint>

namespace ESM4
{
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
