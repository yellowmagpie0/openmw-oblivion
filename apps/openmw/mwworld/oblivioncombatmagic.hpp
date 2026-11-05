#ifndef OPENMW_MWWORLD_OBLIVIONCOMBATMAGIC_H
#define OPENMW_MWWORLD_OBLIVIONCOMBATMAGIC_H

#include <array>
#include <cstdint>
#include <optional>
#include <string>
namespace ESM4 { struct Potion; struct Spell; }
namespace MWBase { class World; }

namespace MWWorld
{
    class ESMStore;

    struct OblivionSpellPresentation
    {
        std::string mDescription;
        std::string mIcon;
        std::optional<float> mCost;
    };
    // Native display and cost inputs only; no selection, resource write or cast.
    OblivionSpellPresentation describeOblivionSpell(const ESMStore& store, const ESM4::Spell& spell,
        const std::array<std::int32_t, 6>& schoolSkills, std::int32_t luck);
    OblivionSpellPresentation describeOblivionPlayerSpell(MWBase::World& world, const ESM4::Spell& spell);

    // Complete admission for instant self restoration; no partial application.
    std::optional<std::array<float, 3>> resolveOblivionPotionRestoration(
        const ESMStore& store, const ESM4::Potion& potion);

    std::optional<std::array<float, 3>> resolveOblivionSpellRestoration(
        const ESMStore& store, const ESM4::Spell& spell);

    // Winning TES4 store preflight for the narrow default self-PARA owner.
    // No forms, actor state, selected spells or RNG are changed. The caller
    // owns Oblivion profile dispatch and post-contact actor/caster eligibility.
    bool supportsOblivionDefaultCombatParalysis(const ESMStore& store);
}

#endif
