#ifndef OPENMW_MWWORLD_OBLIVIONBOUNDITEMS_H
#define OPENMW_MWWORLD_OBLIVIONBOUNDITEMS_H

#include <components/esm4/runtimestate.hpp>
#include <vector>

namespace MWWorld
{
    class ESMStore;

    struct OblivionBoundItemInput
    {
        std::uint32_t mEffectIndex = 0;
        std::uint32_t mEffectCode = 0;
        ESM4::RuntimeInventoryItem mItem;
        friend bool operator==(const OblivionBoundItemInput&, const OblivionBoundItemInput&) = default;
    };

    struct OblivionBoundAbilityInput
    {
        ESM::FormKey mSpell;
        std::vector<OblivionBoundItemInput> mEffects;
        friend bool operator==(const OblivionBoundAbilityInput&, const OblivionBoundAbilityInput&) = default;
    };

    // Narrow native bound-equipment prerequisites for the tutorial. Resolves
    // winning SPEL/MGEF/item definitions; does not create inventory or pretend
    // that these associated-item effects are passive actor-value modifiers.
    // Ability4 lifetime is independent of EFIT duration (original68E670).
    std::vector<OblivionBoundAbilityInput> resolveOblivionNpcBoundAbilityInputs(
        const ESMStore& store, const ESM::FormKey& base);

    void validateOblivionBoundEquipment(const ESM4::RuntimeState& state, const ESMStore& store,
        const ESM::FormKeyResolver& resolver, const ESMStore* incoming = nullptr);
}
#endif
