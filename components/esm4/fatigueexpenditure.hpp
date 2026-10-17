#ifndef OPENMW_COMPONENTS_ESM4_FATIGUEEXPENDITURE_H
#define OPENMW_COMPONENTS_ESM4_FATIGUEEXPENDITURE_H

namespace ESM4
{
    // Original5E07D0 accepts only negative expenditure requests, requires
    // actor+278 eligibility and positive current Fatigue, and limits the
    // requested magnitude to current Fatigue before modifier storage rounds.
    // Return a positive debit; zero means no DamageFloat dispatch. The caller
    // owns actor eligibility, god mode, modifier storage and direct hit damage.
    float fatigueExpenditureDebit(float currentFatigue, float requestedDelta, bool eligible = true);
}

#endif
