#ifndef OPENMW_MWWORLD_OBLIVIONCOMBATMAGIC_H
#define OPENMW_MWWORLD_OBLIVIONCOMBATMAGIC_H

#include <array>
#include <optional>
namespace ESM4 { struct Potion; }

namespace MWWorld
{
    class ESMStore;

    // Complete admission for instant self restoration; no partial application.
    std::optional<std::array<float, 3>> resolveOblivionPotionRestoration(
        const ESMStore& store, const ESM4::Potion& potion);

    // Winning TES4 store preflight for the narrow default self-PARA owner.
    // No forms, actor state, selected spells or RNG are changed. The caller
    // owns Oblivion profile dispatch and post-contact actor/caster eligibility.
    bool supportsOblivionDefaultCombatParalysis(const ESMStore& store);
}

#endif
