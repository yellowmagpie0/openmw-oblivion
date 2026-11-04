#include "oblivioncombatmagic.hpp"

#include "esmstore.hpp"

#include <components/esm4/combatmagicprofile.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadspel.hpp>

#include <set>
#include <components/esm4/loadalch.hpp>

namespace MWWorld
{
    std::optional<std::array<float, 3>> resolveOblivionPotionRestoration(
        const ESMStore& store, const ESM4::Potion& potion)
    {
        if ((potion.mFlags & ESM4::Rec_Deleted) || potion.mEffects.empty() || !potion.mScriptId.isZeroOrUnset())
            return std::nullopt;
        std::array<float, 3> amounts{};
        for (const auto& effect : potion.mEffects)
        {
            const int resource = effect.mId == ESM::fourCC("REHE") ? 0
                : effect.mId == ESM::fourCC("RESP") ? 1 : effect.mId == ESM::fourCC("REFA") ? 2 : -1;
            if (resource < 0 || effect.mDuration || effect.mArea || effect.mRange || effect.mScriptEffect)
                return std::nullopt;
            const auto& definitions = store.get<ESM4::EffectSetting>();
            const ESM4::EffectSetting* definition = nullptr;
            std::set<ESM::FormId> seen;
            for (const auto& record : definitions)
            {
                if (!seen.insert(record.mId).second) continue;
                const auto* candidate = definitions.search(record.mId);
                if (!candidate || candidate->mEffectCode != effect.mId) continue;
                if (const auto key = definitions.findFormKey(ESM::RefId(candidate->mId)))
                    if (const auto* winner = store.getFormKeyIndex().winner(*key);
                        winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_MGEF)) continue;
                if (definition) return std::nullopt;
                definition = candidate;
            }
            // Admit the original restoration definition. Altered effect classes
            // need their own execution semantics, not a guessed resource write.
            if (!definition || !definition->mData || (definition->mFlags & ESM4::Rec_Deleted))
                return std::nullopt;
            const auto& data = *definition->mData;
            if (data.mFlags != 0x01000870 || data.mAssociatedForm
                || data.mAssociatedData != static_cast<unsigned>(resource + 8)
                || data.mSchool != 5 || data.mResistanceActorValue != -1)
                return std::nullopt;
            amounts[resource] += static_cast<float>(effect.mMagnitude);
        }
        return amounts;
    }

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
