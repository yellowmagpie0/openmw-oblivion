#ifndef OPENMW_MWWORLD_OBLIVIONCOMBATMAGIC_H
#define OPENMW_MWWORLD_OBLIVIONCOMBATMAGIC_H

namespace MWWorld
{
    class ESMStore;

    // Winning TES4 store preflight for the narrow default self-PARA owner.
    // No forms, actor state, selected spells or RNG are changed. The caller
    // owns Oblivion profile dispatch and post-contact actor/caster eligibility.
    bool supportsOblivionDefaultCombatParalysis(const ESMStore& store);
}

#endif
