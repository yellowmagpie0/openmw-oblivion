#include "oblivioncombatmagic.hpp"

#include "esmstore.hpp"

#include <components/esm4/combatmagicprofile.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadspel.hpp>

#include <set>

namespace MWWorld
{
    bool supportsOblivionDefaultCombatParalysis(const ESMStore& store)
    {
        const auto key = ESM::FormKey::content("Oblivion.esm", 0x137);
        const auto* metadata = store.getFormKeyIndex().winner(key);
        const ESM4::Spell* spell = nullptr;
        // Original41C81B dynamically casts the lookup result to SpellItem;
        // absent, deleted or other-type forms select the fallback constructor.
        if (!metadata || (!metadata->mDeleted && metadata->mRecordType == ESM4::REC_SPEL))
        {
            spell = store.get<ESM4::Spell>().search(key);
            if (metadata && !spell)
                return false; // Indexed winning Spell must have a typed definition.
        }
        const auto& effects = store.get<ESM4::EffectSetting>();
        const ESM4::EffectSetting* paralysis = nullptr;
        std::set<ESM::FormId> seen;
        for (const auto& record : effects)
        {
            if (!seen.insert(record.mId).second)
                continue;
            const auto* effect = effects.search(record.mId);
            if (!effect || effect->mEffectCode != ESM::fourCC("PARA"))
                continue;
            if (const auto identity = effects.findFormKey(ESM::RefId(effect->mId)))
                if (const auto* winner = store.getFormKeyIndex().winner(*identity);
                    winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_MGEF))
                    continue;
            if (paralysis)
                return false; // Do not guess an order between different keys with the same code.
            paralysis = effect;
        }
        return ESM4::supportsDefaultCombatParalysisProfile(spell, paralysis);
    }
}
