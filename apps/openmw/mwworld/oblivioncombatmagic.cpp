#include "oblivioncombatmagic.hpp"

#include "esmstore.hpp"
#include "worldimp.hpp"
#include "../mwbase/world.hpp"
#include "../mwmechanics/oblivioncombat.hpp"

#include <components/esm4/combatmagicprofile.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadspel.hpp>
#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadgmst.hpp>

#include <set>
#include <cmath>
#include <sstream>
#include <components/esm4/loadalch.hpp>

namespace MWWorld
{
    OblivionSpellPresentation describeOblivionPlayerSpell(MWBase::World& world, const ESM4::Spell& spell)
    {
        const auto* native = dynamic_cast<const World*>(&world);
        const auto* combat = native ? native->getOblivionCombatService() : nullptr;
        if (!combat) throw std::logic_error("native spell display requires actor authority");
        std::array<std::int32_t, 6> skills;
        for (std::size_t i = 0; i < skills.size(); ++i)
            skills[i] = combat->getPlayerIntegerValue(static_cast<std::uint8_t>(20 + i));
        return describeOblivionSpell(world.getStore(), spell, skills, combat->getPlayerIntegerValue(7));
    }

    OblivionSpellPresentation describeOblivionSpell(const ESMStore& store, const ESM4::Spell& spell,
        const std::array<std::int32_t, 6>& schoolSkills, std::int32_t luck)
    {
        OblivionSpellPresentation result;
        if (!spell.mData || (spell.mFlags & ESM4::Rec_Deleted)) return result;
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seenSettings;
        for (const auto& record : store.get<ESM4::GameSetting>())
            if (seenSettings.insert(record.mId).second)
                if (const auto* winner = store.get<ESM4::GameSetting>().search(record.mId);
                    winner && !(winner->mFlags & ESM4::Rec_Deleted)) settings.push_back(winner);
        const auto costs = ESM4::buildSpellCostSettings(settings);
        bool knownCost = spell.mData->mType == 0 && !spell.mEffects.empty();
        float total = 0;
        std::ostringstream description;
        for (const auto& effect : spell.mEffects)
        {
            const ESM4::EffectSetting* definition = nullptr;
            std::set<ESM::FormId> seen;
            for (const auto& record : store.get<ESM4::EffectSetting>())
            {
                if (!seen.insert(record.mId).second) continue;
                const auto* winner = store.get<ESM4::EffectSetting>().search(record.mId);
                if (!winner || winner->mEffectCode != effect.mId || (winner->mFlags & ESM4::Rec_Deleted)) continue;
                if (definition) throw std::invalid_argument("ambiguous native spell effect definition");
                definition = winner;
            }
            if (!definition || !definition->mData || effect.mScriptEffect || effect.mRange > 2)
            {
                knownCost = false;
                continue;
            }
            const auto& data = *definition->mData;
            if (description.tellp() > 0) description << '\n';
            description << definition->mFullName;
            if (!(data.mFlags & 0x100)) description << " " << effect.mMagnitude << " pts";
            if (!(data.mFlags & 0x80) && effect.mDuration) description << " for " << effect.mDuration << " sec";
            if (!(data.mFlags & 0x200) && effect.mRange && effect.mArea)
                description << " in " << effect.mArea << " ft";
            description << (effect.mRange == 0 ? " on Self" : effect.mRange == 1 ? " on Touch" : " on Target");
            if (result.mIcon.empty())
                for (const auto& sub : definition->mSubRecords)
                    if (sub.mType == ESM::fourCC("ICON") && !sub.mData.empty())
                    {
                        result.mIcon.assign(reinterpret_cast<const char*>(sub.mData.data()), sub.mData.size());
                        if (const auto end = result.mIcon.find('\0'); end != std::string::npos) result.mIcon.resize(end);
                    }
            if (data.mSchool >= schoolSkills.size()) { knownCost = false; continue; }
            if (spell.mData->mFlags & 1)
            {
                // Multi-effect manual spells need highest-cost school selection.
                if (spell.mEffects.size() != 1) knownCost = false;
                else total = ESM4::manualSpellMagickaCost(spell.mData->mCost, schoolSkills[data.mSchool], luck, costs);
            }
            else if (!(data.mFlags & 0x400000))
                total += ESM4::spellEffectMagickaCost({data.mBaseCost,
                    data.mFlags & 0x100 ? 0u : effect.mMagnitude,
                    data.mFlags & 0x80 ? 0u : effect.mDuration,
                    (data.mFlags & 0x200) || !effect.mRange ? 0u : effect.mArea,
                    effect.mRange == 2}, schoolSkills[data.mSchool], luck, costs);
        }
        result.mDescription = description.str();
        if (knownCost && std::isfinite(total)) result.mCost = total;
        return result;
    }

    static std::optional<std::array<float, 3>> resolveInstantRestoration(
        const ESMStore& store, const std::vector<ESM4::SpellEffect>& effects)
    {
        if (effects.empty()) return std::nullopt;
        std::array<float, 3> amounts{};
        for (const auto& effect : effects)
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

    std::optional<std::array<float, 3>> resolveOblivionPotionRestoration(
        const ESMStore& store, const ESM4::Potion& potion)
    {
        if ((potion.mFlags & ESM4::Rec_Deleted) || !potion.mScriptId.isZeroOrUnset()) return {};
        return resolveInstantRestoration(store, potion.mEffects);
    }

    std::optional<std::array<float, 3>> resolveOblivionSpellRestoration(
        const ESMStore& store, const ESM4::Spell& spell)
    {
        if ((spell.mFlags & ESM4::Rec_Deleted) || !spell.mData || spell.mData->mType != 0) return {};
        return resolveInstantRestoration(store, spell.mEffects);
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
