#ifndef OPENMW_ESM4_COMBATSTYLEPOLICY_H
#define OPENMW_ESM4_COMBATSTYLEPOLICY_H

#include "loadcsty.hpp"

namespace ESM4
{
    // Resolve parsed historical fields against a complete native default policy.
    // Null selects DefaultCombatStyle; an explicit record follows TES4's distinct
    // load-time defaults. This does not alter the lossless source record.
    CombatStyleStandard resolveCombatStyleStandard(
        const CombatStyleStandard* record, const CombatStyleStandard& defaults);

    CombatStyleAdvanced resolveCombatStyleAdvanced(
        const CombatStyle* record, const CombatStyleAdvanced& defaults);
}

#endif
