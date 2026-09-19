#ifndef OPENMW_ESM4_COMBATSETTINGS_H
#define OPENMW_ESM4_COMBATSETTINGS_H

#include "loadcsty.hpp"
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
}

#endif
