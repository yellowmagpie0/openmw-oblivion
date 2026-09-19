#ifndef OPENMW_MWWORLD_OBLIVIONCOMBATDATA_H
#define OPENMW_MWWORLD_OBLIVIONCOMBATDATA_H

#include <components/esm/formkey.hpp>
#include <components/esm4/combatsettings.hpp>

namespace MWWorld
{
    class ESMStore;

    struct OblivionCombatPolicy
    {
        ESM::FormKey mActorBase;
        // Null identifies the native default style, not an unresolved record.
        ESM::FormKey mStyle;
        ESM4::CombatStyleStandard mStandard;
        ESM4::CombatStyleAdvanced mAdvanced;
    };

    ESM4::CombatStyleDefaults buildOblivionCombatDefaults(const ESMStore& store);
    OblivionCombatPolicy resolveOblivionCombatPolicy(const ESMStore& store,
        const ESM::FormKey& actorBase, const ESM4::CombatStyleDefaults& defaults);
}

#endif
