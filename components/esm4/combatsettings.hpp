#ifndef OPENMW_ESM4_COMBATSETTINGS_H
#define OPENMW_ESM4_COMBATSETTINGS_H

#include "loadcsty.hpp"
#include "physicalcombat.hpp"
#include <span>

namespace ESM4
{
    struct GameSetting;

    struct CombatStyleDefaults
    {
        CombatStyleStandard mStandard;
        CombatStyleAdvanced mAdvanced;
    };

    // Input is the native winning setting inventory, never shared TES3 settings.
    // Known absent entries use independently verified original initializers.
    CombatStyleDefaults buildCombatStyleDefaults(std::span<const GameSetting* const> settings);
    PhysicalCombatSettings buildPhysicalCombatSettings(std::span<const GameSetting* const> settings);
    HandToHandSettings buildHandToHandSettings(std::span<const GameSetting* const> settings);
    BlockSettings buildBlockSettings(std::span<const GameSetting* const> settings);
    float buildDifficultyDamageMultiplier(std::span<const GameSetting* const> settings);
}

#endif
