#include "oblivioncombatmagic.hpp"

#include "esmstore.hpp"
#include "worldimp.hpp"
#include "timestamp.hpp"
#include "class.hpp"
#include "player.hpp"
#include "inventorystore.hpp"
#include "scene.hpp"
#include "worldmodel.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "projectilemanager.hpp"
#include "oblivionprofileservices.hpp"
#include "oblivionscriptmanager.hpp"
#include "oblivionactorstats.hpp"
#include "../mwbase/world.hpp"
#include "../mwmechanics/oblivioncombat.hpp"
#include "../mwmechanics/oblivionai.hpp"
#include "../mwmechanics/oblivionmelee.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwphysics/collisiontype.hpp"
#include <components/debug/debuglog.hpp>
#include <components/settings/values.hpp>

#include <components/esm4/combatmagicprofile.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadligh.hpp>
#include <components/esm4/loadspel.hpp>
#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadgmst.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadench.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/loadacti.hpp>
#include <components/esm4/runtimestate.hpp>
#include <components/esm4/runtimereferences.hpp>

#include <bit>
#include <set>
#include <cmath>
#include <sstream>
#include <limits>
#include <algorithm>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadbook.hpp>
#include "../mwbase/windowmanager.hpp"
#include <components/esm4/ingredientuse.hpp>

namespace MWWorld
{
    namespace
    {
        const ESM4::EffectSetting* ingredientEffectDefinition(const ESMStore& store, std::uint32_t code)
        {
            const ESM4::EffectSetting* result = nullptr;
            std::set<ESM::FormId> seen;
            for (const auto& row : store.get<ESM4::EffectSetting>())
            {
                if (!seen.insert(row.mId).second) continue;
                const auto* definition = store.get<ESM4::EffectSetting>().search(row.mId);
                if (!definition || (definition->mFlags & ESM4::Rec_Deleted) || definition->mEffectCode != code) continue;
                if (const auto key = store.get<ESM4::EffectSetting>().findFormKey(ESM::RefId(definition->mId)))
                    if (const auto* winner = store.getFormKeyIndex().winner(*key);
                        winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_MGEF)) continue;
                if (result) return nullptr;
                result = definition;
            }
            return result;
        }
    }

    std::optional<ESM4::IngredientFatigueProfile> resolveOblivionIngredientFatigue(
        const ESMStore& store, const ESM4::Ingredient& ingredient)
    {
        if ((ingredient.mFlags & ESM4::Rec_Deleted) || !ingredient.mScriptId.isZeroOrUnset()
            || ingredient.mEffects.empty()) return {};
        if (const auto key = store.get<ESM4::Ingredient>().findFormKey(ESM::RefId(ingredient.mId)))
            if (const auto* winner = store.getFormKeyIndex().winner(*key);
                winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_INGR)) return {};
        const auto& effect = ingredient.mEffects.front();
        if (effect.mRange != 0 || effect.mArea || effect.mMagnitude != 15 || effect.mDuration != 1
            || effect.mActorValue != 10 || effect.mScriptEffect) return {};
        const auto* definition = ingredientEffectDefinition(store, effect.mId);
        if (!definition || !definition->mData) return {};
        const auto& data = *definition->mData;
        if (data.mAssociatedForm || data.mAssociatedData != 10) return {};
        // ENIT's upper three bytes are padding in original records. Food and
        // non-food ingestion have different potency; never infer one from EFIT.
        const auto flags = ingredient.mEnchantment.flags & 0xff;
        if (effect.mId == ESM::fourCC("REFA") && flags == 3
            && data.mFlags == 0x01000870 && data.mSchool == 5 && data.mResistanceActorValue == -1)
            return ESM4::IngredientFatigueProfile{};
        if (effect.mId == ESM::fourCC("DGFA") && flags == 1
            && data.mFlags == 0x01001c75 && data.mBaseCost == 4.4f
            && data.mSchool == 2 && data.mResistanceActorValue == 64)
            return ESM4::NoviceIngredientFatigueDamage;
        return {};
    }

    namespace
    {
        // Bounded original Nirnroot food ingestion. Authored 20/120 EFIT is
        // not Wortcraft potency; original novice use resolves -5 Health/1s.
        bool supportsNirnrootDrain(const ESMStore& store, const ESM4::Ingredient& ingredient)
        {
            const auto key = store.get<ESM4::Ingredient>().findFormKey(ESM::RefId(ingredient.mId));
            if (!key || *key != ESM::FormKey::content("Oblivion.esm", 0x4e940)
                || (ingredient.mFlags & ESM4::Rec_Deleted) || ingredient.mEffects.empty()
                || (ingredient.mEnchantment.flags & 0xff) != 3) return false;
            if (const auto* winner = store.getFormKeyIndex().winner(*key);
                winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_INGR)) return false;
            const auto& effect = ingredient.mEffects.front();
            const auto* definition = ingredientEffectDefinition(store, effect.mId);
            return effect.mId == ESM::fourCC("DRHE") && effect.mRange == 0 && !effect.mArea
                && effect.mMagnitude == 20 && effect.mDuration == 120 && effect.mActorValue == 8
                && !effect.mScriptEffect && definition && definition->mData
                && !definition->mData->mAssociatedForm && definition->mData->mAssociatedData == 8
                && definition->mData->mFlags == 0x01001c77 && definition->mData->mSchool == 2
                && definition->mData->mResistanceActorValue == 64;
        }
    }

    std::string describeOblivionIngredient(MWBase::World& world, const ConstPtr& item)
    {
        const auto& store = world.getStore();
        const auto* ingredient = store.get<ESM4::Ingredient>().search(item.getCellRef().getRefId());
        const auto* native = dynamic_cast<const World*>(&world);
        const auto* combat = native ? native->getOblivionCombatService() : nullptr;
        if (!ingredient || !combat) return {};
        const auto skill = combat->getPlayerIntegerValue(19); // Native Alchemy.
        const auto mastery = resolveOblivionFatigueSettings(store).mMastery;
        std::size_t known = 1;
        for (std::size_t i = 0; i < 3; ++i)
            if (skill >= mastery.mMinimumSkill[i]) ++known;
        std::string result;
        for (std::size_t i = 0; i < std::min(known, ingredient->mEffects.size()); ++i)
            if (const auto* effect = ingredientEffectDefinition(store, ingredient->mEffects[i].mId))
                result += "\n" + effect->mFullName;
        return result;
    }

    bool consumeOblivionIngredient(MWBase::World& world, const Ptr& item, const Ptr& actor)
    {
        auto* native = dynamic_cast<World*>(&world);
        auto* combat = native ? native->getOblivionCombatService() : nullptr;
        if (!combat || item.isEmpty() || actor.isEmpty() || actor != world.getPlayerPtr()
            || item.getCellRef().getCount() < 1 || item.getCellRef().getNativeBoundSource()
            || actor.getClass().getCreatureStats(actor).isDead()) return false;
        auto& inventory = actor.getClass().getInventoryStore(actor);
        if (item.getContainerStore() != &inventory) return false;
        const auto& store = world.getStore();
        const auto* ingredient = store.get<ESM4::Ingredient>().search(item.getCellRef().getRefId());
        const bool healthDrain = ingredient && supportsNirnrootDrain(store, *ingredient);
        const auto profile = healthDrain ? std::optional<ESM4::IngredientFatigueProfile>{{-5.f, 1.f}}
            : ingredient ? resolveOblivionIngredientFatigue(store, *ingredient) : std::nullopt;
        if (!profile) return false;
        if (healthDrain && (combat->getPlayerValue(19) != 5.f || combat->getPlayerValue(7) != 50.f
                || combat->getPlayerValue(64) != 0.f)) return false;
        const auto scriptOwner = item.getCellRef().getNativeScriptOwner();
        auto* scripts = native->getOblivionScriptManager();
        if (!scriptOwner.isNull() && (item.getCellRef().getCount() != 1
                || !scripts || !scripts->canRetireConsumedInventoryOwner())) return false;
        if (*profile == ESM4::NoviceIngredientFatigueDamage
            && (combat->getPlayerValue(19) != 5.f || combat->getPlayerValue(7) != 50.f
                || combat->getPlayerValue(64) != 0.f)) return false;
        const auto key = store.get<ESM4::Ingredient>().findFormKey(item.getCellRef().getRefId());
        const auto* before = combat->findActorValues(ESM::FormKey::dynamic("player", 1));
        const auto* life = combat->findActorLife(ESM::FormKey::dynamic("player", 1));
        if (!key || !before || !life || life->mPhase != ESM4::ActorLifePhase::Alive) return false;
        auto candidate = *before;
        candidate.mIngredientRestoration.push_back(
            {*key, 0.f, profile->mMagnitude, profile->mDuration, std::uint8_t(healthDrain ? 8 : 10)});
        if (healthDrain)
            candidate.mValues[8] = ESM4::changeActorValueModifier(candidate.mValues[8], candidate.mOwner, 8,
                ESM4::ActorValueModifier::Maximum, profile->mMagnitude);
        candidate.validate();
        const auto settings = resolveOblivionPlayerDynamicBaseSettings(store);
        auto debit = inventory.prepareItemRemoval(item, 1);
        if (debit->getCount() != 1 || !debit->isValid()) return false;
        // Allocate/validate before publication. publishPlayerValues prepares the
        // facade before its no-callback swap; the already validated debit then
        // commits without allocation or callbacks. Observers see both owners.
        MWBase::Environment::get().getWorldModel()->registerPtr(item);
        combat->publishPlayerValues(world.getPlayer(), std::move(candidate), settings);
        if (!debit->commit()) throw std::logic_error("native ingredient debit changed during publication");
        if (!scriptOwner.isNull()) scripts->retireConsumedInventoryOwner(scriptOwner);
        Log(Debug::Info) << "TES4 ingredient consumed: ingredient=" << key->serialize()
            << " magnitude=" << profile->mMagnitude << " duration=" << profile->mDuration;
        MWBase::Environment::get().getLuaManager()->itemConsumed(item, actor);
        debit->notify();
        return true;
    }

    void validateOblivionIngredientRestoration(const ESMStore& store, const ESM4::RuntimeState& state)
    {
        for (const auto& actor : state.mNativeActorValues)
            for (const auto& effect : actor.mIngredientRestoration)
            {
                const auto* ingredient = store.get<ESM4::Ingredient>().search(effect.mIngredient);
                const auto profile = ingredient && effect.mActorValue == 8 && supportsNirnrootDrain(store, *ingredient)
                    ? std::optional<ESM4::IngredientFatigueProfile>{{-5.f, 1.f}}
                    : ingredient && effect.mActorValue == 10 ? resolveOblivionIngredientFatigue(store, *ingredient)
                    : std::nullopt;
                if (!profile || profile->mMagnitude != effect.mMagnitude || profile->mDuration != effect.mDuration)
                    throw std::invalid_argument("saved native ingredient Fatigue profile no longer resolves");
            }
    }

    bool supportsOblivionStarOfWest(const ESMStore& store, const ESM4::Spell& spell)
    {
        if (spell.mFormKey != ESM::FormKey::content("Oblivion.esm", 0x47ade)
            || (spell.mFlags & ESM4::Rec_Deleted) || !spell.mData
            || spell.mData->mType != 2 || spell.mData->mCost != 0
            || (spell.mData->mFlags & 0xff) != 1 || spell.mEffects.size() != 1) return false;
        if (const auto* winner = store.getFormKeyIndex().winner(spell.mFormKey);
            winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_SPEL)) return false;
        const auto& effect = spell.mEffects.front();
        if (effect.mId != ESM::fourCC("ABFA") || effect.mMagnitude != 100 || effect.mArea
            || effect.mDuration || effect.mRange != 1 || effect.mActorValue != 10 || effect.mScriptEffect)
            return false;
        const auto* definition = ingredientEffectDefinition(store, effect.mId);
        if (!definition || !definition->mData) return false;
        const auto& data = *definition->mData;
        return data.mFlags == 0x01003825 && !data.mAssociatedForm
            && data.mAssociatedData == 10 && data.mSchool == 5 && data.mResistanceActorValue == 64;
    }

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
        if (supportsOblivionStarOfWest(store, spell) || resolveOblivionVoiceOfEmperor(store, spell)) result.mCost = 0.f;
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

    std::optional<ESM4::PotionAttributeState> resolveOblivionPotionAttributes(
        const ESMStore& store, const ESM4::Potion& potion)
    {
        if ((potion.mFlags & ESM4::Rec_Deleted) || !potion.mScriptId.isZeroOrUnset()
            || (potion.mItem.flags & 0xffu) != 3 || potion.mEffects.size() != 3) return {};
        const auto key = store.get<ESM4::Potion>().findFormKey(ESM::RefId(potion.mId));
        if (!key) return {};
        if (const auto* winner = store.getFormKeyIndex().winner(*key);
            winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_ALCH)) return {};
        for (unsigned i = 0; i < 2; ++i)
        {
            const auto& effect = potion.mEffects[i];
            if (effect.mId != ESM::fourCC("DRAT") || effect.mMagnitude != ESM4::MeadAttributeDrain
                || effect.mDuration != ESM4::MeadDuration || effect.mArea || effect.mRange
                || effect.mActorValue != i + 1 || effect.mScriptEffect) return {};
        }
        const auto& restoration = potion.mEffects.back();
        if (restoration.mId != ESM::fourCC("REFA") || restoration.mMagnitude != ESM4::MeadFatigueRestoration
            || restoration.mDuration || restoration.mArea || restoration.mRange
            || restoration.mActorValue != 10 || restoration.mScriptEffect) return {};
        const auto* drain = ingredientEffectDefinition(store, ESM::fourCC("DRAT"));
        if (!drain || !drain->mData) return {};
        const auto& data = *drain->mData;
        if (data.mFlags != 0x00101c77 || data.mAssociatedForm || data.mAssociatedData != 0
            || data.mSchool != 2 || data.mResistanceActorValue != 64) return {};
        if (!resolveInstantRestoration(store, {restoration})) return {};
        return ESM4::PotionAttributeState{*key, ESM4::MeadDuration, 0.f,
            {{1, -ESM4::MeadAttributeDrain}, {2, -ESM4::MeadAttributeDrain}}};
    }

    bool consumeOblivionPotionAttributes(MWBase::World& world, const Ptr& item, const Ptr& actor)
    {
        auto* native = dynamic_cast<World*>(&world);
        auto* combat = native ? native->getOblivionCombatService() : nullptr;
        if (!combat || item.isEmpty() || actor.isEmpty() || actor != world.getPlayerPtr()
            || item.getCellRef().getCount() < 1 || item.getCellRef().getNativeBoundSource()
            || actor.getClass().getCreatureStats(actor).isDead()) return false;
        auto& inventory = actor.getClass().getInventoryStore(actor);
        if (item.getContainerStore() != &inventory) return false;
        const auto& store = world.getStore();
        const auto* potion = store.get<ESM4::Potion>().search(item.getCellRef().getRefId());
        const auto effect = potion ? resolveOblivionPotionAttributes(store, *potion) : std::nullopt;
        const auto key = ESM::FormKey::dynamic("player", 1);
        const auto* before = combat->findActorValues(key);
        const auto* life = combat->findActorLife(key);
        if (!effect || !before || !life || life->mPhase != ESM4::ActorLifePhase::Alive) return false;
        auto candidate = *before;
        candidate.mPotionAttributeEffects.push_back(*effect);
        for (const auto& modifier : effect->mModifiers)
            candidate.mValues[modifier.mAttribute] = ESM4::changeActorValueModifier(
                candidate.mValues[modifier.mAttribute], candidate.mOwner, modifier.mAttribute,
                ESM4::ActorValueModifier::Maximum, modifier.mDelta);
        const float fatigueDamage = candidate.mValues[10].mModifiers[2].value_or(0.f);
        candidate.mValues[10] = ESM4::changeActorValueModifier(candidate.mValues[10], candidate.mOwner,
            10, ESM4::ActorValueModifier::Damage, ESM4::MeadFatigueRestoration);
        const float restored = candidate.mValues[10].mModifiers[2].value_or(0.f) - fatigueDamage;
        candidate.validate();
        const auto settings = resolveOblivionPlayerDynamicBaseSettings(store);
        auto debit = inventory.prepareItemRemoval(item, 1);
        if (debit->getCount() != 1 || !debit->isValid()) return false;
        MWBase::Environment::get().getWorldModel()->registerPtr(item);
        combat->publishPlayerValues(world.getPlayer(), std::move(candidate), settings);
        if (!debit->commit()) throw std::logic_error("native potion debit changed during publication");
        Log(Debug::Info) << "M16 potion attributes applied: potion=" << effect->mPotion.serialize()
            << " duration=" << effect->mDuration << " intelligence_delta=-1 willpower_delta=-1"
            << " fatigue_restored=" << restored << " count=1";
        MWBase::Environment::get().getLuaManager()->itemConsumed(item, actor);
        debit->notify();
        return true;
    }

    namespace
    {
        std::optional<ESM4::RuntimePotionLight> resolvePotionLight(const ESMStore& store, const ESM4::Potion& potion)
        {
            if ((potion.mFlags & ESM4::Rec_Deleted) || !potion.mScriptId.isZeroOrUnset()
                || potion.mEffects.size() != 1)
                return {};
            const auto& effect = potion.mEffects.front();
            if (effect.mId != ESM::fourCC("LGHT") || effect.mMagnitude <= 0 || effect.mDuration <= 0
                || effect.mArea || effect.mRange || effect.mActorValue != 8 || effect.mScriptEffect)
                return {};
            const auto key = store.get<ESM4::Potion>().findFormKey(ESM::RefId(potion.mId));
            if (!key || !key->isContent()) return {};
            if (const auto* winner = store.getFormKeyIndex().winner(*key);
                winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_ALCH))
                return {};
            const auto* definition = ingredientEffectDefinition(store, effect.mId);
            if (!definition || !definition->mData) return {};
            const auto& data = *definition->mData;
            if (data.mFlags != 0x80001872 || data.mAssociatedForm || data.mAssociatedData != 0
                || data.mSchool != 3 || data.mResistanceActorValue != -1 || !data.mLight || !data.mLight->isContent())
                return {};
            const auto* light = store.get<ESM4::Light>().search(*data.mLight);
            if (!light || (light->mFlags & ESM4::Rec_Deleted) || (light->mData.flags & ESM4::Light::Negative))
                return {};
            if (const auto* winner = store.getFormKeyIndex().winner(*data.mLight);
                winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_LIGH))
                return {};
            return ESM4::RuntimePotionLight{
                *key, *data.mLight, static_cast<float>(effect.mMagnitude), static_cast<float>(effect.mDuration), 0.f};
        }
    }

    bool consumeOblivionPotionLight(World& world, const Ptr& item, const Ptr& actor, const ESM4::Potion& potion)
    {
        auto* combat = world.getOblivionCombatService();
        if (!combat || actor != world.getPlayerPtr() || actor.getClass().getCreatureStats(actor).isDead())
            return false;
        const auto effect = resolvePotionLight(world.getStore(), potion);
        const auto key = ESM::FormKey::dynamic("player", 1);
        const auto* before = combat->findActorValues(key);
        const auto* life = combat->findActorLife(key);
        if (!effect || !before || !life || life->mPhase != ESM4::ActorLifePhase::Alive)
            return false;
        auto candidate = *before;
        candidate.mPotionLightEffects.push_back(*effect);
        candidate.validate();
        const auto settings = resolveOblivionPlayerDynamicBaseSettings(world.getStore());
        auto& inventory = actor.getClass().getInventoryStore(actor);
        auto debit = inventory.prepareItemRemoval(item, 1);
        if (debit->getCount() != 1 || !debit->isValid()) return false;
        MWBase::Environment::get().getWorldModel()->registerPtr(item);
        combat->publishPlayerValues(world.getPlayer(), std::move(candidate), settings);
        if (!debit->commit()) throw std::logic_error("native potion Light debit changed during publication");
        Log(Debug::Info) << "M16 potion Light applied: potion=" << effect->mPotion.serialize()
                         << " light=" << effect->mLight.serialize() << " magnitude=" << effect->mMagnitude
                         << " duration=" << effect->mDuration << " count=1";
        MWBase::Environment::get().getLuaManager()->itemConsumed(item, actor);
        debit->notify();
        return true;
    }

    void validateOblivionPotionLight(const ESMStore& store, const ESM4::RuntimeState& state)
    {
        for (const auto& actor : state.mNativeActorValues)
            for (const auto& effect : actor.mPotionLightEffects)
            {
                const auto* potion = store.get<ESM4::Potion>().search(effect.mPotion);
                auto profile = potion ? resolvePotionLight(store, *potion) : std::nullopt;
                if (profile) profile->mElapsed = effect.mElapsed;
                if (!profile || *profile != effect)
                    throw std::invalid_argument("saved native potion Light source no longer resolves");
            }
    }

    std::string describeOblivionPotion(MWBase::World& world, const ConstPtr& item)
    {
        const auto* potion = world.getStore().get<ESM4::Potion>().search(item.getCellRef().getRefId());
        if (!potion) return {};
        if (resolvePotionLight(world.getStore(), *potion))
        {
            const auto& light = potion->mEffects.front();
            return "Light " + std::to_string(light.mMagnitude) + " ft for " + std::to_string(light.mDuration)
                + "s on Self";
        }
        if (!resolveOblivionPotionAttributes(world.getStore(), *potion)) return {};
        return "Drain Intelligence 1 pt for 60s on Self\nDrain Willpower 1 pt for 60s on Self"
            "\nRestore Fatigue 2 pts on Self";
    }

    void validateOblivionPotionAttributes(const ESMStore& store, const ESM4::RuntimeState& state)
    {
        for (const auto& actor : state.mNativeActorValues)
            for (const auto& effect : actor.mPotionAttributeEffects)
            {
                const auto* potion = store.get<ESM4::Potion>().search(effect.mPotion);
                const auto profile = potion ? resolveOblivionPotionAttributes(store, *potion) : std::nullopt;
                if (!profile || effect.mDuration != profile->mDuration || effect.mModifiers != profile->mModifiers)
                    throw std::invalid_argument("saved native potion attribute source no longer resolves");
            }
    }

    std::optional<std::array<float, 3>> resolveOblivionSpellRestoration(
        const ESMStore& store, const ESM4::Spell& spell)
    {
        if ((spell.mFlags & ESM4::Rec_Deleted) || !spell.mData || spell.mData->mType != 0) return {};
        return resolveInstantRestoration(store, spell.mEffects);
    }

    namespace
    {
        constexpr std::array<std::string_view, 4> sFireFields{
            "native.fire.spell", "native.fire.magnitude", "native.fire.duration", "native.fire.elapsed"};

        void validateFire(const OblivionEnvironmentalFire& fire)
        {
            if (!fire.mSpell.isContent() || ESM::FormKey::deserialize(fire.mSpell.serialize()) != fire.mSpell
                || !std::isfinite(fire.mMagnitude) || fire.mMagnitude <= 0
                || !std::isfinite(fire.mDuration) || fire.mDuration <= 0
                || !std::isfinite(fire.mElapsed) || fire.mElapsed < 0 || fire.mElapsed >= fire.mDuration)
                throw std::invalid_argument("invalid native environmental fire effect");
        }
    }

    std::optional<OblivionEnvironmentalFire> resolveOblivionEnvironmentalFire(
        const ESMStore& store, const ESM4::Spell& spell)
    {
        if ((spell.mFlags & ESM4::Rec_Deleted) || !spell.mData || spell.mData->mType != 0
            || spell.mData->mFlags != 0x40 // Audited environmental fire's no-absorb/reflect profile.
            || spell.mEffects.size() != 1) return {};
        const auto& effect = spell.mEffects.front();
        if (effect.mId != ESM::fourCC("FIDG") || effect.mRange != 1 || !effect.mDuration
            || !effect.mMagnitude || effect.mArea || effect.mActorValue != 8 || effect.mScriptEffect) return {};
        const ESM4::EffectSetting* definition = nullptr;
        std::set<ESM::FormId> seen;
        for (const auto& row : store.get<ESM4::EffectSetting>())
        {
            if (!seen.insert(row.mId).second) continue;
            const auto* winner = store.get<ESM4::EffectSetting>().search(row.mId);
            if (!winner || (winner->mFlags & ESM4::Rec_Deleted) || winner->mEffectCode != effect.mId) continue;
            if (definition) return {};
            definition = winner;
        }
        if (!definition || !definition->mData) return {};
        const auto& data = *definition->mData;
        if (data.mAssociatedData != 8 || data.mSchool != 2 || data.mResistanceActorValue != 61
            || (data.mFlags & 0x51) != 0x51 || (data.mFlags & 0x180)) return {};
        return OblivionEnvironmentalFire{spell.mFormKey, static_cast<float>(effect.mMagnitude),
            static_cast<float>(effect.mDuration), 0};
    }

    std::optional<OblivionEnvironmentalFire> readOblivionEnvironmentalFire(
        const ESM4::RuntimeReferenceState& reference)
    {
        const auto& values = reference.mCustomState;
        const auto count = std::count_if(values.begin(), values.end(),
            [](const auto& entry) { return entry.first.starts_with("native.fire."); });
        if (!count) return {};
        if (static_cast<std::size_t>(count) != sFireFields.size()
            || std::any_of(sFireFields.begin(), sFireFields.end(), [&](auto key) { return !values.contains(key); }))
            throw std::invalid_argument("incomplete native environmental fire ownership");
        const auto* spell = std::get_if<std::string>(&values.find(sFireFields[0])->second);
        if (!spell) throw std::invalid_argument("invalid native environmental fire spell");
        const auto number = [&](std::size_t index) {
            const auto& value = values.find(sFireFields[index])->second;
            const auto* real = std::get_if<double>(&value);
            const auto* integer = std::get_if<std::int64_t>(&value);
            if (!real && !integer) throw std::invalid_argument("invalid native environmental fire number");
            const double result = real ? *real : static_cast<double>(*integer);
            if (!std::isfinite(result) || std::abs(result) > std::numeric_limits<float>::max())
                throw std::invalid_argument("nonfinite native environmental fire number");
            return static_cast<float>(result);
        };
        OblivionEnvironmentalFire result{ESM::FormKey::deserialize(*spell), number(1), number(2), number(3)};
        validateFire(result);
        if (result.mSpell.serialize() != *spell)
            throw std::invalid_argument("noncanonical native environmental fire spell");
        return result;
    }

    void writeOblivionEnvironmentalFire(ESM4::RuntimeReferenceState& reference,
        const std::optional<OblivionEnvironmentalFire>& effect)
    {
        if (effect) validateFire(*effect);
        auto values = reference.mCustomState;
        std::erase_if(values, [](const auto& entry) { return entry.first.starts_with("native.fire."); });
        if (effect)
        {
            values.emplace(sFireFields[0], effect->mSpell.serialize());
            values.emplace(sFireFields[1], double(effect->mMagnitude));
            values.emplace(sFireFields[2], double(effect->mDuration));
            values.emplace(sFireFields[3], double(effect->mElapsed));
        }
        reference.mCustomState.swap(values);
    }

    bool World::castOblivionEnvironmentalFire(const Ptr& source, const ESM::FormKey& spell,
        const ESM::FormKey& target)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || source.isEmpty()
            || source.getClass().getType() != ESM::REC_ACTI4 || source.mRef->isDeleted()
            || ESM4::runtimeReferenceKey(target) != ESM::FormKey::dynamic("player", 1)
            || !mOblivionCombat) return false;
        const auto* definition = mStore.get<ESM4::Spell>().search(spell);
        const auto effect = definition ? resolveOblivionEnvironmentalFire(mStore, *definition) : std::nullopt;
        if (!effect) return false;
        if (getPlayerPtr().getClass().getCreatureStats(getPlayerPtr()).isDead()) return true;
        const auto key = source.getCellRef().getFormKey();
        if (!key.isContent()) return false;
        if (!mOblivionRuntimeState)
            mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());
        auto& references = mOblivionRuntimeState->mReferences;
        auto owner = std::find_if(references.begin(), references.end(), [&](const auto& ref) { return ref.mKey == key; });
        if (owner == references.end())
        {
            // Import only the newly encountered source. Replacing the whole
            // cached snapshot could discard unrelated script-owned state.
            auto live = captureOblivionRuntimeState();
            const auto current = std::find_if(live.mReferences.begin(), live.mReferences.end(),
                [&](const auto& ref) { return ref.mKey == key; });
            if (current == live.mReferences.end()) throw std::runtime_error("environmental fire source not captured");
            writeOblivionEnvironmentalFire(*current, effect);
            references.push_back(std::move(*current));
            std::sort(references.begin(), references.end(), [](const auto& a, const auto& b) { return a.mKey < b.mKey; });
        }
        else writeOblivionEnvironmentalFire(*owner, effect); // A recast replaces this source's remaining effect.
        Log(Debug::Info) << "TES4 environmental fire cast: source=" << key.serialize()
            << " spell=" << spell.serialize() << " magnitude=" << effect->mMagnitude << " duration=" << effect->mDuration;
        return true;
    }

    void World::updateOblivionEnvironmentalFire(float duration)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionRuntimeState || !mOblivionCombat) return;
        if (!std::isfinite(duration) || duration < 0) throw std::invalid_argument("invalid environmental fire frame");
        if (duration == 0) return;
        std::vector<ESM::FormKey> sources;
        for (const auto& reference : mOblivionRuntimeState->mReferences)
            if (readOblivionEnvironmentalFire(reference)) sources.push_back(reference.mKey);
        if (sources.empty()) return;
        const auto player = getPlayerPtr();
        const auto recovery = resolveOblivionEssentialRecoverySettings(mStore);
        const auto playerBase = resolveOblivionPlayerDynamicBaseSettings(mStore);
        for (const auto& source : sources)
        {
            const auto findOwner = [&]() {
                auto& references = mOblivionRuntimeState->mReferences;
                return std::find_if(references.begin(), references.end(), [&](const auto& ref) { return ref.mKey == source; });
            };
            const auto owner = findOwner();
            auto effect = *readOblivionEnvironmentalFire(*owner);
            auto next = *owner;
            const float elapsed = static_cast<float>(std::min(double(effect.mDuration), double(effect.mElapsed) + duration));
            const float time = elapsed - effect.mElapsed;
            effect.mElapsed = elapsed;
            const bool dead = player.getClass().getCreatureStats(player).isDead();
            std::optional<OblivionEnvironmentalFire> pending = effect;
            if (dead || elapsed >= effect.mDuration) pending.reset();
            writeOblivionEnvironmentalFire(next, pending);
            float damage = 0;
            if (!dead)
            {
                damage = effect.mMagnitude * time
                    * std::max(0.f, 1.f - mOblivionCombat->getPlayerValue(61) / 100.f)
                    * std::max(0.f, 1.f - mOblivionCombat->getPlayerValue(64) / 100.f);
                if (!std::isfinite(damage)) throw std::invalid_argument("nonfinite environmental fire damage");
                mOblivionCombat->changePlayerHealth(*mPlayer, -damage, {}, player.getClass().isEssential(player),
                    recovery, playerBase, getGodModeState());
                if (player.getClass().getCreatureStats(player).isDead())
                    std::erase_if(next.mCustomState,
                        [](const auto& entry) { return entry.first.starts_with("native.fire."); });
            }
            // All timer allocation/validation precedes the atomic Health/life
            // write. Re-find after it: lifecycle preparation may grow references.
            findOwner()->mCustomState.swap(next.mCustomState);
            Log(Debug::Verbose) << "TES4 environmental fire tick: source=" << source.serialize()
                << " damage=" << (getGodModeState() ? 0.f : damage) << " elapsed=" << elapsed
                << " health=" << mOblivionCombat->getPlayerValue(8);
        }
    }

    void World::validateOblivionEnvironmentalFire(const ESM4::RuntimeState& state) const
    {
        for (const auto& reference : state.mReferences)
        {
            const auto effect = readOblivionEnvironmentalFire(reference);
            if (!effect) continue;
            const auto base = getOblivionReferenceBaseKey(reference.mKey);
            const auto* spell = mStore.get<ESM4::Spell>().search(effect->mSpell);
            const auto expected = spell ? resolveOblivionEnvironmentalFire(mStore, *spell) : std::nullopt;
            if (!reference.mKey.isContent() || !base || *base != reference.mBase
                || !mStore.get<ESM4::Activator>().search(*base) || !expected
                || expected->mMagnitude != effect->mMagnitude || expected->mDuration != effect->mDuration)
                throw std::invalid_argument("saved environmental fire content no longer resolves");
        }
    }

    static std::optional<OblivionTargetDamage> resolveTargetDamage(
        const ESMStore& store, const std::vector<ESM4::SpellEffect>& effects)
    {
        if (effects.size() != 1) return {};
        const auto& effect = effects.front();
        const int resistance = effect.mId == ESM::fourCC("SHDG") ? 68
            : effect.mId == ESM::fourCC("FIDG") ? 61 : -1;
        if (resistance < 0 || effect.mRange != 2 || effect.mDuration || effect.mArea
            || effect.mActorValue != 8 || effect.mScriptEffect) return {};
        const ESM4::EffectSetting* definition = nullptr;
        std::set<ESM::FormId> seen;
        for (const auto& row : store.get<ESM4::EffectSetting>())
        {
            if (!seen.insert(row.mId).second) continue;
            const auto* winner = store.get<ESM4::EffectSetting>().search(row.mId);
            if (!winner || (winner->mFlags & ESM4::Rec_Deleted) || winner->mEffectCode != effect.mId) continue;
            if (definition) return {}; // Ambiguous codes cannot select an arbitrary asset or rule.
            definition = winner;
        }
        if (!definition || !definition->mData) return {};
        const auto& data = *definition->mData;
        if (data.mAssociatedData != 8 || data.mSchool != 2 || data.mResistanceActorValue != resistance
            || (data.mFlags & 0x41) != 0x41 || (data.mFlags & 0x100)) return {};
        std::optional<float> speed;
        std::optional<std::string> model;
        for (const auto& sub : definition->mSubRecords)
        {
            if (sub.mType == ESM::fourCC("DATA"))
            {
                if (speed || sub.mData.size() < 32) return {};
                const auto& bytes = sub.mData;
                const std::uint32_t bits = std::uint32_t(bytes[28]) | (std::uint32_t(bytes[29]) << 8)
                    | (std::uint32_t(bytes[30]) << 16) | (std::uint32_t(bytes[31]) << 24);
                speed = std::bit_cast<float>(bits);
            }
            else if (sub.mType == ESM::fourCC("MODL"))
            {
                if (model || sub.mData.size() < 2 || sub.mData.back() != 0) return {};
                model.emplace(reinterpret_cast<const char*>(sub.mData.data()), sub.mData.size() - 1);
                if (model->find('\0') != std::string::npos) return {};
            }
        }
        if (!speed || !std::isfinite(*speed) || *speed <= 0 || !model || model->empty()) return {};
        return OblivionTargetDamage{effect.mId, static_cast<float>(effect.mMagnitude),
            static_cast<std::uint8_t>(resistance), *speed, std::move(*model)};
    }

    std::optional<OblivionTargetDamage> resolveOblivionVoiceOfEmperor(
        const ESMStore& store, const ESM4::Spell& spell)
    {
        if (spell.mFormKey != ESM::FormKey::content("Oblivion.esm", 0x47add)
            || (spell.mFlags & ESM4::Rec_Deleted) || !spell.mData
            || spell.mData->mType != 2 || spell.mData->mCost != 0
            || spell.mData->mFlags != 1 || spell.mEffects.size() != 1) return {};
        if (const auto* winner = store.getFormKeyIndex().winner(spell.mFormKey);
            winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_SPEL)) return {};
        const auto& effect = spell.mEffects.front();
        if (effect.mId != ESM::fourCC("CHRM") || effect.mMagnitude != 30 || effect.mDuration != 30
            || effect.mRange != 2 || effect.mArea || effect.mActorValue != 6 || effect.mScriptEffect) return {};
        const auto* definition = ingredientEffectDefinition(store, effect.mId);
        if (!definition || !definition->mData) return {};
        const auto& data = *definition->mData;
        if (data.mFlags != 0x01000c62 || data.mAssociatedData != 6
            || data.mSchool != 3 || data.mResistanceActorValue != 64) return {};
        std::optional<float> speed;
        std::optional<std::string> model;
        for (const auto& sub : definition->mSubRecords)
        {
            if (sub.mType == ESM::fourCC("DATA"))
            {
                if (speed || sub.mData.size() < 32) return {};
                const auto& bytes = sub.mData;
                const std::uint32_t bits = std::uint32_t(bytes[28]) | (std::uint32_t(bytes[29]) << 8)
                    | (std::uint32_t(bytes[30]) << 16) | (std::uint32_t(bytes[31]) << 24);
                speed = std::bit_cast<float>(bits);
            }
            else if (sub.mType == ESM::fourCC("MODL"))
            {
                if (model || sub.mData.size() < 2 || sub.mData.back() != 0) return {};
                model.emplace(reinterpret_cast<const char*>(sub.mData.data()), sub.mData.size() - 1);
                if (model->find('\0') != std::string::npos) return {};
            }
        }
        if (!speed || !std::isfinite(*speed) || *speed <= 0 || !model || model->empty()) return {};
        return OblivionTargetDamage{effect.mId, 30.f, 64, *speed, std::move(*model)};
    }

    namespace
    {
        constexpr std::array<std::string_view, 4> sCharmFields{
            "native.charm.spell", "native.charm.caster", "native.charm.magnitude", "native.charm.elapsed"};
        void validateCharm(const OblivionCharm& effect)
        {
            if (effect.mSpell != ESM::FormKey::content("Oblivion.esm", 0x47add)
                || effect.mCaster != ESM::FormKey::dynamic("player", 1)
                || !std::isfinite(effect.mMagnitude) || effect.mMagnitude <= 0 || effect.mMagnitude > 30
                || !std::isfinite(effect.mElapsed) || effect.mElapsed < 0 || effect.mElapsed >= 30)
                throw std::invalid_argument("invalid native Charm ownership or timer");
        }
    }

    std::optional<OblivionCharm> readOblivionCharm(const ESM4::RuntimeReferenceState& reference)
    {
        const auto& values = reference.mCustomState;
        const auto count = std::count_if(values.begin(), values.end(),
            [](const auto& entry) { return entry.first.starts_with("native.charm."); });
        if (!count) return {};
        if (static_cast<std::size_t>(count) != sCharmFields.size()
            || std::any_of(sCharmFields.begin(), sCharmFields.end(), [&](auto key) { return !values.contains(key); }))
            throw std::invalid_argument("incomplete native Charm ownership");
        const auto key = [&](std::size_t i) {
            const auto* text = std::get_if<std::string>(&values.find(sCharmFields[i])->second);
            if (!text) throw std::invalid_argument("invalid native Charm identity");
            const auto result = ESM::FormKey::deserialize(*text);
            if (result.serialize() != *text) throw std::invalid_argument("noncanonical native Charm identity");
            return result;
        };
        const auto number = [&](std::size_t i) {
            const auto& value = values.find(sCharmFields[i])->second;
            const auto* real = std::get_if<double>(&value);
            const auto* integer = std::get_if<std::int64_t>(&value);
            if (!real && !integer) throw std::invalid_argument("invalid native Charm number");
            const double result = real ? *real : double(*integer);
            if (!std::isfinite(result) || result < 0 || result > 30)
                throw std::invalid_argument("invalid native Charm number range");
            return static_cast<float>(result);
        };
        OblivionCharm result{key(0), key(1), number(2), number(3)};
        validateCharm(result);
        return result;
    }

    void writeOblivionCharm(ESM4::RuntimeReferenceState& reference, const std::optional<OblivionCharm>& effect)
    {
        if (effect) validateCharm(*effect);
        auto values = reference.mCustomState;
        std::erase_if(values, [](const auto& entry) { return entry.first.starts_with("native.charm."); });
        if (effect)
        {
            values.emplace(sCharmFields[0], effect->mSpell.serialize());
            values.emplace(sCharmFields[1], effect->mCaster.serialize());
            values.emplace(sCharmFields[2], double(effect->mMagnitude));
            values.emplace(sCharmFields[3], double(effect->mElapsed));
        }
        reference.mCustomState.swap(values);
    }

    std::int32_t oblivionCharmModifier(const ESM4::RuntimeState& state,
        const ESM::FormKey& actor, const ESM::FormKey& viewer)
    {
        const auto owner = std::find_if(state.mReferences.begin(), state.mReferences.end(),
            [&](const auto& ref) { return ref.mKey == actor; });
        if (owner == state.mReferences.end()) return 0;
        const auto effect = readOblivionCharm(*owner);
        return effect && effect->mCaster == viewer ? static_cast<std::int32_t>(effect->mMagnitude) : 0;
    }

    std::optional<OblivionTargetDamage> resolveOblivionTargetSpellDamage(
        const ESMStore& store, const ESM4::Spell& spell)
    {
        if ((spell.mFlags & ESM4::Rec_Deleted) || !spell.mData || spell.mData->mType != 0) return {};
        return resolveTargetDamage(store, spell.mEffects);
    }

    std::optional<OblivionTargetDamage> resolveOblivionScrollDamage(const ESMStore& store, const ESM4::Book& book)
    {
        if ((book.mFlags & ESM4::Rec_Deleted) || !(book.mData.flags & ESM4::Book::Flag_Scroll)
            || !book.mScriptId.isZeroOrUnset()) return {};
        if (const auto key = store.get<ESM4::Book>().findFormKey(ESM::RefId(book.mId)))
            if (const auto* winner = store.getFormKeyIndex().winner(*key);
                winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_BOOK)) return {};
        const auto* enchantment = store.get<ESM4::Enchantment>().search(book.mEnchantment);
        if (!enchantment || (enchantment->mFlags & ESM4::Rec_Deleted) || !enchantment->mData
            || enchantment->mData->mType != 0 || enchantment->mEffects.size() != 1
            || enchantment->mEffects.front().mId != ESM::fourCC("FIDG")
            || enchantment->mEffects.front().mMagnitude != 20) return {};
        if (const auto* winner = store.getFormKeyIndex().winner(enchantment->mFormKey);
            winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_ENCH)) return {};
        return resolveTargetDamage(store, enchantment->mEffects);
    }

    std::optional<ESM::FormKey> resolveOblivionScrollChameleon(const ESMStore& store, const ESM4::Book& book)
    {
        if ((book.mFlags & ESM4::Rec_Deleted) || !(book.mData.flags & ESM4::Book::Flag_Scroll)
            || !book.mScriptId.isZeroOrUnset()) return {};
        if (const auto key = store.get<ESM4::Book>().findFormKey(ESM::RefId(book.mId)))
            if (const auto* winner = store.getFormKeyIndex().winner(*key);
                winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_BOOK)) return {};
        const auto* enchantment = store.get<ESM4::Enchantment>().search(book.mEnchantment);
        if (!enchantment || (enchantment->mFlags & ESM4::Rec_Deleted) || !enchantment->mData
            || enchantment->mData->mType != 0 || enchantment->mEffects.size() != 1) return {};
        if (const auto* winner = store.getFormKeyIndex().winner(enchantment->mFormKey);
            winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_ENCH)) return {};
        const auto& effect = enchantment->mEffects.front();
        if (effect.mId != ESM::fourCC("CHML") || effect.mMagnitude != ESM4::ScrollChameleonMagnitude
            || effect.mDuration != ESM4::ScrollChameleonDuration || effect.mArea || effect.mRange
            || effect.mActorValue != 46 || effect.mScriptEffect) return {};
        const auto* definition = ingredientEffectDefinition(store, effect.mId);
        if (!definition || !definition->mData) return {};
        const auto& data = *definition->mData;
        // Fixed associated AV46, Recover, percentage magnitude and Self support.
        // Hostile/detrimental/NoDuration/NoMagnitude profiles are not substituted.
        if (data.mAssociatedData != 46 || data.mSchool != 3 || data.mResistanceActorValue != -1
            || (data.mFlags & 0x0100001a) != 0x0100001a || (data.mFlags & 0x185)) return {};
        return enchantment->mFormKey;
    }

    bool releaseOblivionScrollChameleon(MWBase::World& world, const ESM::FormKey& scroll)
    {
        auto* native = dynamic_cast<World*>(&world);
        auto* combat = native ? native->getOblivionCombatService() : nullptr;
        const auto actor = ESM::FormKey::dynamic("player", 1);
        const auto* cast = combat ? combat->findSpellCast(actor) : nullptr;
        const auto* life = combat ? combat->findActorLife(actor) : nullptr;
        const auto* current = combat ? combat->findActorValues(actor) : nullptr;
        const auto* book = world.getStore().get<ESM4::Book>().search(scroll);
        const auto enchantment = book ? resolveOblivionScrollChameleon(world.getStore(), *book) : std::nullopt;
        if (!cast || cast->mSpell != scroll || cast->mReleased || cast->mCost != 0
            || !life || life->mPhase != ESM4::ActorLifePhase::Alive || !current || !enchantment) return false;
        auto values = *current;
        auto& effects = values.mScrollChameleon;
        const auto previous = std::find_if(effects.begin(), effects.end(),
            [&](const auto& effect) { return effect.mEnchantment == *enchantment; });
        const bool refresh = previous != effects.end();
        const float delta = ESM4::applyScrollChameleon(effects,
            {scroll, *enchantment, ESM4::ScrollChameleonMagnitude, 0.f});
        if (delta != 0)
            values.mValues[46] = ESM4::changeActorValueModifier(values.mValues[46], values.mOwner, 46,
                ESM4::ActorValueModifier::Maximum, delta);
        values.validate();
        combat->publishPlayerValues(world.getPlayer(), std::move(values),
            resolveOblivionPlayerDynamicBaseSettings(world.getStore()));
        Log(Debug::Info) << "M16 Chameleon applied: scroll=" << scroll.serialize()
            << " enchantment=" << enchantment->serialize() << " magnitude=" << ESM4::ScrollChameleonMagnitude
            << " duration=" << ESM4::ScrollChameleonDuration << " refresh=" << refresh;
        return true;
    }

    void validateOblivionScrollChameleon(const ESMStore& store, const ESM4::RuntimeState& state)
    {
        for (const auto& actor : state.mNativeActorValues)
            for (const auto& effect : actor.mScrollChameleon)
            {
                const auto* book = store.get<ESM4::Book>().search(effect.mScroll);
                const auto source = book ? resolveOblivionScrollChameleon(store, *book) : std::nullopt;
                if (!source || *source != effect.mEnchantment)
                    throw std::invalid_argument("saved native Chameleon source no longer resolves");
            }
    }

    std::string describeOblivionScroll(MWBase::World& world, const ConstPtr& item)
    {
        const auto* book = world.getStore().get<ESM4::Book>().search(item.getCellRef().getRefId());
        const auto damage = book ? resolveOblivionScrollDamage(world.getStore(), *book) : std::nullopt;
        if (!damage)
        {
            if (!book || !resolveOblivionScrollChameleon(world.getStore(), *book)) return {};
            const auto* effect = ingredientEffectDefinition(world.getStore(), ESM::fourCC("CHML"));
            return effect ? effect->mFullName + " 25 pts for 30s on Self" : std::string{};
        }
        const auto* effect = ingredientEffectDefinition(world.getStore(), damage->mEffectCode);
        return effect ? effect->mFullName + " 20 pts on Target" : std::string{};
    }

    bool selectOblivionScroll(MWBase::World& world, const Ptr& item, const Ptr& actor)
    {
        if (actor.isEmpty() || actor != world.getPlayerPtr() || item.isEmpty()
            || item.getCellRef().getCount() < 1 || item.getCellRef().getNativeBoundSource()) return false;
        auto& inventory = actor.getClass().getInventoryStore(actor);
        if (item.getContainerStore() != &inventory) return false;
        const auto* book = world.getStore().get<ESM4::Book>().search(item.getCellRef().getRefId());
        const auto key = world.getStore().get<ESM4::Book>().findFormKey(item.getCellRef().getRefId());
        if (!book || !key || (!resolveOblivionScrollDamage(world.getStore(), *book)
            && !resolveOblivionScrollChameleon(world.getStore(), *book))) return false;
        for (auto it = inventory.begin(); it != inventory.end(); ++it)
            if (*it == item)
            {
                MWBase::Environment::get().getWindowManager()->setSelectedEnchantItem(item);
                inventory.setSelectedEnchantItem(it);
                Log(Debug::Info) << "M16 native scroll selected: book="
                    << key->serialize();
                return true;
            }
        return false;
    }

    bool beginOblivionScrollCast(MWBase::World& world, const Ptr& item, ESM4::SpellCastState cast)
    {
        auto* native = dynamic_cast<World*>(&world);
        auto* combat = native ? native->getOblivionCombatService() : nullptr;
        const auto actor = world.getPlayerPtr();
        if (!combat || item.isEmpty() || item.getCellRef().getCount() < 1
            || item.getCellRef().getNativeBoundSource() || cast.mCost != 0) return false;
        const auto& store = world.getStore();
        const auto* book = store.get<ESM4::Book>().search(item.getCellRef().getRefId());
        const auto key = store.get<ESM4::Book>().findFormKey(item.getCellRef().getRefId());
        if (!book || !key || cast.mSpell != *key || (!resolveOblivionScrollDamage(store, *book)
            && !resolveOblivionScrollChameleon(store, *book))) return false;
        auto& inventory = actor.getClass().getInventoryStore(actor);
        if (item.getContainerStore() != &inventory) return false;
        MWBase::Environment::get().getWorldModel()->registerPtr(item);
        auto debit = inventory.prepareItemRemoval(item, 1);
        if (debit->getCount() != 1 || !debit->isValid()) return false;
        if (!combat->beginSpellCast(ESM::FormKey::dynamic("player", 1), std::move(cast), false)) return false;
        // No callbacks between admitted cast ownership and its prepared debit.
        if (!debit->commit()) throw std::logic_error("native scroll debit changed during admission");
        Log(Debug::Info) << "M16 native scroll consumed: book=" << key->serialize() << " count=1 magicka_cost=0";
        const auto windows = MWBase::Environment::get().getWindowManager();
        if (windows->getSelectedEnchantItem() == item)
        {
            inventory.setSelectedEnchantItem(inventory.end());
            windows->unsetSelectedSpell();
        }
        MWBase::Environment::get().getLuaManager()->itemConsumed(item, actor);
        debit->notify();
        return true;
    }

    std::optional<OblivionStaffCastInputs> resolveOblivionStaffCastInputs(
        const ESMStore& store, const ESM4::Weapon& weapon)
    {
        if ((weapon.mFlags & ESM4::Rec_Deleted) || weapon.mData.type != 4
            || !weapon.mScriptId.isZeroOrUnset()) return {};
        const auto* enchantment = store.get<ESM4::Enchantment>().search(weapon.mEnchantment);
        if (!enchantment || (enchantment->mFlags & ESM4::Rec_Deleted) || !enchantment->mData
            || enchantment->mData->mType != 1) return {};
        const auto damage = resolveTargetDamage(store, enchantment->mEffects);
        if (!damage) return {};
        return OblivionStaffCastInputs{enchantment->mFormKey,
            static_cast<float>(enchantment->mData->mCost), static_cast<float>(weapon.mEnchantmentPoints), *damage};
    }

    namespace
    {
        std::vector<const ESM4::GameSetting*> magicSettings(const ESMStore& store)
        {
            std::vector<const ESM4::GameSetting*> result;
            std::set<ESM::FormId> seen;
            for (const auto& row : store.get<ESM4::GameSetting>())
                if (seen.insert(row.mId).second)
                    if (const auto* winner = store.get<ESM4::GameSetting>().search(row.mId);
                        winner && !(winner->mFlags & ESM4::Rec_Deleted)) result.push_back(winner);
            return result;
        }

        float projectileSetting(const std::vector<const ESM4::GameSetting*>& settings,
            std::string_view name, float fallback)
        {
            bool found = false;
            for (const auto* setting : settings)
                if (setting->mEditorId == name)
                {
                    const auto* value = std::get_if<float>(&setting->mData);
                    if (found || !value || !std::isfinite(*value) || *value <= 0)
                        throw std::invalid_argument("invalid or ambiguous native projectile setting");
                    found = true; fallback = *value;
                }
            return fallback;
        }
    }

    bool World::releaseOblivionStaffProjectile(const Ptr& actor, const ESM::FormKey& enchantment, float chargeCost)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionCombat || !mPhysics
            || actor.isEmpty() || !actor.isInCell() || !actor.getCell()->getCell()->isEsm4()) return false;
        auto& inventory = actor.getClass().getInventoryStore(actor);
        const auto item = inventory.getSlot(InventoryStore::Slot_CarriedRight);
        if (item == inventory.end()) return false;
        const auto id = OblivionProfileServices::nativeItemId(mStore, item->getCellRef().getRefId());
        const auto* weapon = mStore.get<ESM4::Weapon>().search(id);
        const auto inputs = weapon ? resolveOblivionStaffCastInputs(mStore, *weapon) : std::nullopt;
        if (!inputs || inputs->mEnchantment != enchantment || !std::isfinite(chargeCost) || chargeCost < 0
            || inputs->mAuthoredChargeCost != chargeCost || !item->getCellRef().supportsNativeItemCondition()) return false;
        const float rawCharge = item->getCellRef().getEnchantmentCharge();
        const float charge = rawCharge == -1 ? inputs->mMaximumCharge : rawCharge;
        if (!std::isfinite(charge) || charge < chargeCost) return false;
        const bool player = actor == getPlayerPtr();
        const auto casterKey = player ? ESM::FormKey::dynamic("player", 1) : actor.getCellRef().getFormKey();
        osg::Vec3f position = actor.getRefData().getPosition().asVec3();
        position.z() += mPhysics->getRenderingHalfExtents(actor).z() * 1.5f;
        osg::Vec3f direction;
        if (player)
        {
            const auto& rotation = actor.getRefData().getPosition().rot;
            direction = (osg::Quat(rotation[0], osg::Vec3f(-1, 0, 0))
                * osg::Quat(rotation[2], osg::Vec3f(0, 0, -1))) * osg::Vec3f(0, 1, 0);
        }
        else
        {
            const auto targetKey = getOblivionCombatTarget(casterKey);
            if (!targetKey || targetKey->isNull() || !mOblivionAi) return false;
            const auto target = *targetKey == ESM::FormKey::dynamic("player", 1)
                ? getPlayerPtr() : mOblivionAi->resolveReference(*targetKey);
            if (target.isEmpty() || !target.isInCell() || !mPhysics->getActor(target)
                || target.getClass().getCreatureStats(target).isDead()) return false;
            auto aim = target.getRefData().getPosition().asVec3();
            aim.z() += mPhysics->getRenderingHalfExtents(target).z() * 1.5f;
            direction = aim - position;
        }
        if (direction.normalize() == 0) return false;
        const auto settings = magicSettings(mStore);
        // Verified original compiled defaults; the installed master has no overrides.
        const float speed = projectileSetting(settings, "fMagicProjectileBaseSpeed", 1000.f)
            * inputs->mDamage.mProjectileSpeedFactor;
        ESM4::MagicProjectileState projectile;
        projectile.mCaster = casterKey; projectile.mSource = enchantment;
        projectile.mCell = actor.getCell()->getCell()->getEsm4().mFormKey;
        projectile.mEffectCode = inputs->mDamage.mEffectCode; projectile.mMagnitude = inputs->mDamage.mMagnitude;
        for (std::size_t i = 0; i < 3; ++i) { projectile.mPosition[i] = position[i]; projectile.mVelocity[i] = direction[i] * speed; }
        projectile.mMaximumDistance = projectileSetting(settings, "fMagicProjectileMaxDistance", 10000.f);
        projectile.mModel = inputs->mDamage.mModel;
        projectile.validate();
        if (!mOblivionRuntimeState) mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());
        const auto serial = mOblivionRuntimeState->mNextMagicProjectile;
        if (!serial || serial == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("native magic projectile serial exhausted");
        auto projectiles = mOblivionRuntimeState->mNativeMagicProjectiles;
        if (!projectiles.emplace(serial, std::move(projectile)).second)
            throw std::logic_error("native magic projectile serial reused");
        auto nextCharge = item->getCellRef();
        nextCharge.setEnchantmentCharge(charge - chargeCost);
        // All allocation/validation precedes this no-throw publication. The
        // controller publishes its released flag immediately after return.
        item->getCellRef().swapNativeEnchantmentCharge(nextCharge);
        mOblivionRuntimeState->mNativeMagicProjectiles.swap(projectiles);
        mOblivionRuntimeState->mNextMagicProjectile = serial + 1;
        return true;
    }

    bool World::releaseOblivionPlayerAbsorbFatiguePower(const ESM::FormKey& spellKey, Ptr& hitTarget)
    {
        hitTarget = {};
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionCombat || !mPhysics) return false;
        const auto* spell = mStore.get<ESM4::Spell>().search(spellKey);
        if (!spell || !supportsOblivionStarOfWest(mStore, *spell)) return false;
        const auto actor = getPlayerPtr();
        const auto time = getTimeStamp();
        const double gameHours = double(time.getDay()) * 24. + time.getHour();
        if (!mOblivionCombat->canUsePlayerPower(spellKey, gameHours)) return false;
        const auto settings = magicSettings(mStore);
        const float reach = projectileSetting(settings, "fMagicDefaultTouchDistance", 256.f);
        osg::Vec3f origin = actor.getRefData().getPosition().asVec3();
        origin.z() += mPhysics->getRenderingHalfExtents(actor).z() * 1.7f;
        const auto& rotation = actor.getRefData().getPosition().rot;
        osg::Vec3f direction = (osg::Quat(rotation[0], osg::Vec3f(-1, 0, 0))
            * osg::Quat(rotation[2], osg::Vec3f(0, 0, -1))) * osg::Vec3f(0, 1, 0);
        if (direction.normalize() == 0) return false;
        const auto hit = mPhysics->castRay(origin, origin + direction * reach, {actor}, {},
            0xff, MWPhysics::CollisionType_Projectile);
        Ptr target = hit.mHit && !hit.mHitObject.isEmpty() && hit.mHitObject.getClass().isActor()
            ? hit.mHitObject : Ptr{};
        float magnitude = 100.f;
        if (!target.isEmpty())
        {
            const auto* values = mOblivionCombat->findActorValues(target.getCellRef().getFormKey());
            const auto* life = mOblivionCombat->findActorLife(target.getCellRef().getFormKey());
            if (!values || !life || life->mPhase != ESM4::ActorLifePhase::Alive) target = {};
            else magnitude *= 1.f - std::clamp(ESM4::composeActorValue(values->mValues[64],
                values->mOwner, values->mProcess), 0.f, 100.f) / 100.f;
        }
        const auto targetKey = target.isEmpty() ? ESM::FormKey{} : target.getCellRef().getFormKey();
        const auto before = mOblivionCombat->getPlayerValue(10);
        const auto targetBefore = target.isEmpty() ? 0.f : mOblivionCombat->getNonPlayerValue(target, 10);
        hitTarget = target;
        if (!mOblivionCombat->releasePlayerAbsorbFatiguePower(*mPlayer, target, spellKey, gameHours, magnitude,
                resolveOblivionPlayerDynamicBaseSettings(mStore))) return false;
        Log(Debug::Info) << "M16 Star of the West released: spell=" << spellKey.serialize()
            << " target=" << targetKey.serialize() << " magnitude=" << (target.isEmpty() ? 0.f : magnitude)
            << " caster_before=" << before << " caster_after=" << mOblivionCombat->getPlayerValue(10)
            << " target_before=" << targetBefore << " target_after="
            << (target.isEmpty() ? 0.f : mOblivionCombat->getNonPlayerValue(target, 10))
            << " used_at=" << gameHours;
        return true;
    }

    bool World::releaseOblivionPlayerSpellProjectile(const ESM::FormKey& spellKey, float power)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionCombat || !mPhysics) return false;
        const auto actor = getPlayerPtr();
        const auto caster = ESM::FormKey::dynamic("player", 1);
        const auto* cast = mOblivionCombat->findSpellCast(caster);
        const auto* spell = mStore.get<ESM4::Spell>().search(spellKey);
        const auto* scroll = spell ? nullptr : mStore.get<ESM4::Book>().search(spellKey);
        const auto charm = spell ? resolveOblivionVoiceOfEmperor(mStore, *spell) : std::nullopt;
        const auto damage = charm ? charm : spell ? resolveOblivionTargetSpellDamage(mStore, *spell)
            : scroll ? resolveOblivionScrollDamage(mStore, *scroll) : std::nullopt;
        if (!cast || cast->mSpell != spellKey || cast->mReleased || !damage
            || !std::isfinite(power) || power < 0 || actor.isEmpty() || !actor.isInCell()
            || !actor.getCell()->getCell()->isEsm4()) return false;
        // Cost belongs to the pending cast, even if equipment or selection changes
        // during windup. Prepare resource and projectile owners before publication.
        const auto* current = mOblivionCombat->findActorValues(caster);
        if (!current) return false;
        auto values = *current;
        const auto time = getTimeStamp();
        const double gameHours = double(time.getDay()) * 24. + time.getHour();
        if (charm)
        {
            if (cast->mCost != 0 || !mOblivionCombat->canUsePlayerPower(spellKey, gameHours)) return false;
            const auto use = std::find_if(values.mUsedPowers.begin(), values.mUsedPowers.end(),
                [&](const auto& entry) { return entry.mSpell == spellKey; });
            if (use == values.mUsedPowers.end()) values.mUsedPowers.push_back({spellKey, gameHours});
            else use->mUsedAt = gameHours;
        }
        if (scroll && cast->mCost != 0) return false;
        if (!scroll && ESM4::composeActorValue(values.mValues[9], values.mOwner, values.mProcess) < cast->mCost)
            return false;
        if (!scroll)
            values.mValues[9] = ESM4::changeActorValueModifier(values.mValues[9], values.mOwner, 9,
                ESM4::ActorValueModifier::Damage, -cast->mCost);
        osg::Vec3f position = actor.getRefData().getPosition().asVec3();
        position.z() += mPhysics->getRenderingHalfExtents(actor).z() * 1.5f;
        const auto& rotation = actor.getRefData().getPosition().rot;
        osg::Vec3f direction = (osg::Quat(rotation[0], osg::Vec3f(-1, 0, 0))
            * osg::Quat(rotation[2], osg::Vec3f(0, 0, -1))) * osg::Vec3f(0, 1, 0);
        if (direction.normalize() == 0) return false;
        const auto settings = magicSettings(mStore);
        const float speed = projectileSetting(settings, "fMagicProjectileBaseSpeed", 1000.f)
            * damage->mProjectileSpeedFactor;
        ESM4::MagicProjectileState projectile;
        projectile.mCaster = caster; projectile.mSource = spellKey;
        projectile.mCell = actor.getCell()->getCell()->getEsm4().mFormKey;
        projectile.mEffectCode = damage->mEffectCode;
        projectile.mMagnitude = damage->mMagnitude * (scroll ? 1.f : power);
        for (std::size_t i = 0; i < 3; ++i)
        {
            projectile.mPosition[i] = position[i];
            projectile.mVelocity[i] = direction[i] * speed;
        }
        projectile.mMaximumDistance = projectileSetting(settings, "fMagicProjectileMaxDistance", 10000.f);
        projectile.mModel = damage->mModel;
        projectile.validate();
        if (!mOblivionRuntimeState) mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());
        const auto serial = mOblivionRuntimeState->mNextMagicProjectile;
        if (!serial || serial == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("native magic projectile serial exhausted");
        auto projectiles = mOblivionRuntimeState->mNativeMagicProjectiles;
        if (!projectiles.emplace(serial, std::move(projectile)).second)
            throw std::logic_error("native magic projectile serial reused");
        mOblivionCombat->publishPlayerValues(*mPlayer, std::move(values), resolveOblivionPlayerDynamicBaseSettings(mStore));
        // Resource publication is transactional; the remaining ownership swaps
        // cannot throw. advanceItemCast then publishes the released flag once.
        mOblivionRuntimeState->mNativeMagicProjectiles.swap(projectiles);
        mOblivionRuntimeState->mNextMagicProjectile = serial + 1;
        return true;
    }

    void World::updateOblivionMagicProjectiles(float duration)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionRuntimeState || !mOblivionCombat
            || !mPhysics || !mWorldScene || !mProjectileManager) return;
        if (!std::isfinite(duration) || duration < 0) throw std::invalid_argument("invalid native magic frame");
        if (duration > 0)
            for (auto& reference : mOblivionRuntimeState->mReferences)
                if (auto effect = readOblivionCharm(reference))
                {
                    const float elapsed = static_cast<float>(std::min(30., double(effect->mElapsed) + duration));
                    const auto* life = mOblivionCombat->findActorLife(reference.mKey);
                    if (elapsed >= 30 || (life && life->mPhase != ESM4::ActorLifePhase::Alive))
                    {
                        writeOblivionCharm(reference, std::nullopt);
                        Log(Debug::Info) << "M16 Charm expired: target=" << reference.mKey.serialize();
                    }
                    else
                    {
                        effect->mElapsed = elapsed;
                        writeOblivionCharm(reference, effect);
                    }
                }
        auto& projectiles = mOblivionRuntimeState->mNativeMagicProjectiles;
        if (projectiles.empty() || duration == 0)
        {
            mProjectileManager->syncOblivionMagicProjectiles(projectiles);
            return;
        }
        const auto player = getPlayerPtr();
        const auto settings = magicSettings(mStore);
        std::size_t destructionUses = 0;
        for (auto it = projectiles.begin(); it != projectiles.end();)
        {
            const auto& projectile = it->second;
            const bool active = std::any_of(mWorldScene->getActiveCells().begin(), mWorldScene->getActiveCells().end(),
                [&](const auto* cell) { return cell->getCell()->isEsm4()
                    && cell->getCell()->getEsm4().mFormKey == projectile.mCell; });
            if (!active) { it = projectiles.erase(it); continue; }
            auto next = ESM4::advanceMagicProjectile(projectile, duration);
            const osg::Vec3f from(projectile.mPosition[0], projectile.mPosition[1], projectile.mPosition[2]);
            const osg::Vec3f to(next.mPosition[0], next.mPosition[1], next.mPosition[2]);
            const auto caster = projectile.mCaster == ESM::FormKey::dynamic("player", 1) ? player
                : mOblivionAi ? mOblivionAi->resolveReference(projectile.mCaster) : Ptr();
            const auto hit = mPhysics->castRay(from, to, caster.isEmpty() ? std::vector<ConstPtr>{} : std::vector<ConstPtr>{caster},
                {}, 0xff, MWPhysics::CollisionType_Projectile);
            if (hit.mHit)
            {
                const auto serial = it->first;
                float appliedDamage = 0;
                bool trainDestruction = false;
                std::optional<std::pair<ESM::FormKey, float>> appliedCharm;
                const auto& target = hit.mHitObject;
                if (!target.isEmpty() && target.getClass().isActor()
                    && !target.getClass().getCreatureStats(target).isDead())
                {
                    const bool targetPlayer = target == player;
                    const auto value = [&](std::uint8_t av) { return targetPlayer ? mOblivionCombat->getPlayerValue(av)
                        : mOblivionCombat->getNonPlayerValue(target, av); };
                    if (value(52) != 0 || value(53) != 0)
                        throw std::runtime_error("native projectile absorption and reflection are not implemented");
                    if (projectile.mEffectCode == ESM::fourCC("CHRM"))
                    {
                        const float magnitude = projectile.mMagnitude * std::max(0.f, 1.f - value(64) / 100.f);
                        if (magnitude > 0)
                        {
                            if (projectile.mCaster != ESM::FormKey::dynamic("player", 1) || targetPlayer)
                                throw std::runtime_error("unsupported native Charm caster or recipient");
                            auto references = mOblivionRuntimeState->mReferences;
                            const auto key = target.getCellRef().getFormKey();
                            auto owner = std::find_if(references.begin(), references.end(),
                                [&](const auto& ref) { return ref.mKey == key; });
                            if (owner == references.end())
                            {
                                auto live = captureOblivionRuntimeState();
                                const auto found = std::find_if(live.mReferences.begin(), live.mReferences.end(),
                                    [&](const auto& ref) { return ref.mKey == key; });
                                if (found == live.mReferences.end()) throw std::runtime_error("Charm recipient not captured");
                                references.push_back(*found);
                                owner = std::prev(references.end());
                            }
                            writeOblivionCharm(*owner, OblivionCharm{projectile.mSource, projectile.mCaster, magnitude, 0});
                            std::sort(references.begin(), references.end(), [](const auto& a, const auto& b) { return a.mKey < b.mKey; });
                            appliedCharm.emplace(key, magnitude);
                            mOblivionRuntimeState->mReferences.swap(references);
                        }
                    }
                    else
                    {
                        // Resolve the learned-spell owner before publishing Health.
                        // Scrolls, powers and nonplayer sources do not train here.
                        const auto* source = caster == player && !targetPlayer
                            ? mStore.get<ESM4::Spell>().search(projectile.mSource) : nullptr;
                        const auto* winner = source ? mStore.getFormKeyIndex().winner(projectile.mSource) : nullptr;
                        const bool ordinaryDestruction = source
                            && (!winner || (!winner->mDeleted && winner->mRecordType == ESM4::REC_SPEL))
                            && resolveOblivionTargetSpellDamage(mStore, *source).has_value();
                        const auto resistance = projectile.mEffectCode == ESM::fourCC("SHDG") ? 68 : 61;
                        float damage = projectile.mMagnitude * std::max(0.f, 1.f - value(resistance) / 100.f)
                            * std::max(0.f, 1.f - value(64) / 100.f);
                        const auto role = targetPlayer ? ESM4::PlayerDamageRole::Victim
                            : caster == player ? ESM4::PlayerDamageRole::Attacker : ESM4::PlayerDamageRole::Unaffected;
                        damage = ESM4::difficultyDamage(damage,
                            MWMechanics::oblivionNormalizedDifficulty(Settings::game().mDifficulty),
                            ESM4::buildDifficultyDamageMultiplier(settings), role);
                        auto adoption = mOblivionCombat->guardLifeAdoption(target, targetPlayer ? mPlayer.get() : nullptr);
                        adoptOblivionActorLife(target);
                        const auto recovery = resolveOblivionEssentialRecoverySettings(mStore);
                        if (targetPlayer) mOblivionCombat->changePlayerHealth(*mPlayer, -damage, projectile.mCaster,
                            target.getClass().isEssential(target), recovery, resolveOblivionPlayerDynamicBaseSettings(mStore), getGodModeState());
                        else mOblivionCombat->changeNonPlayerHealth(target, -damage, projectile.mCaster,
                            target.getClass().isEssential(target), recovery);
                        adoption.commit();
                        appliedDamage = damage;
                        trainDestruction = ordinaryDestruction && damage > 0;
                    }
                }
                // Removal cannot throw after Health/death publication, so the
                // same contact is never retried even if a later frame fails.
                it = projectiles.erase(it);
                if (trainDestruction) ++destructionUses;
                if (appliedCharm)
                    Log(Debug::Info) << "M16 Charm applied: target=" << appliedCharm->first.serialize()
                        << " caster=dynamic:player:0000000000000001 magnitude=" << appliedCharm->second << " duration=30";
                Log(Debug::Info) << "M16 magic projectile impact: id=" << serial
                    << " actor=" << (!target.isEmpty() && target.getClass().isActor()) << " damage=" << appliedDamage
                    << " destruction_use=" << trainDestruction;
            }
            else if (next.mDistance >= next.mMaximumDistance) it = projectiles.erase(it);
            else { std::swap(it->second, next); ++it; }
        }
        // Every impact owner is consumed before progression or observers can
        // save. Loading a pending flight can award once; a completed hit cannot.
        for (std::size_t i = 0; i < destructionUses; ++i)
            useOblivionPlayerSkill(22, 1);
        mProjectileManager->syncOblivionMagicProjectiles(projectiles);
    }

    void World::validateOblivionMagicProjectiles(const ESM4::RuntimeState& state) const
    {
        for (const auto& reference : state.mReferences)
            if (const auto effect = readOblivionCharm(reference))
            {
                const auto* spell = mStore.get<ESM4::Spell>().search(effect->mSpell);
                const auto actor = std::find_if(state.mNativeActorValues.begin(), state.mNativeActorValues.end(),
                    [&](const auto& values) { return values.mActor == reference.mKey; });
                if (!spell || !resolveOblivionVoiceOfEmperor(mStore, *spell)
                    || actor == state.mNativeActorValues.end() || actor->mBase != reference.mBase
                    || (!mStore.get<ESM4::Npc>().search(reference.mBase)
                        && !mStore.get<ESM4::Creature>().search(reference.mBase)))
                    throw std::invalid_argument("saved native Charm recipient or source no longer resolves");
            }
        for (const auto& actor : state.mNativeActorValues)
            for (const auto& use : actor.mUsedPowers)
            {
                const auto* spell = mStore.get<ESM4::Spell>().search(use.mSpell);
                if (!spell || (!supportsOblivionStarOfWest(mStore, *spell) && !resolveOblivionVoiceOfEmperor(mStore, *spell)))
                    throw std::invalid_argument("saved native greater-power source no longer resolves");
            }
        for (const auto& [actor, cast] : state.mNativeSpellCasts)
        {
            if (const auto* spell = mStore.get<ESM4::Spell>().search(cast.mSpell);
                spell && spell->mData && spell->mData->mType == 2)
            {
                if (actor != ESM::FormKey::dynamic("player", 1) || cast.mCost != 0
                    || (!supportsOblivionStarOfWest(mStore, *spell) && !resolveOblivionVoiceOfEmperor(mStore, *spell)))
                    throw std::invalid_argument("saved native greater-power cast no longer resolves");
                if (cast.mReleased)
                {
                    const auto values = std::find_if(state.mNativeActorValues.begin(), state.mNativeActorValues.end(),
                        [&](const auto& entry) { return entry.mActor == actor; });
                    if (values == state.mNativeActorValues.end()
                        || std::none_of(values->mUsedPowers.begin(), values->mUsedPowers.end(),
                            [&](const auto& use) { return use.mSpell == cast.mSpell; }))
                        throw std::invalid_argument("released native power has no daily-use owner");
                }
            }
            const auto* book = mStore.get<ESM4::Book>().search(cast.mSpell);
            const auto* winner = mStore.getFormKeyIndex().winner(cast.mSpell);
            if (book || (winner && winner->mRecordType == ESM4::REC_BOOK))
                if (!book || actor != ESM::FormKey::dynamic("player", 1) || cast.mCost != 0
                    || (!resolveOblivionScrollDamage(mStore, *book)
                        && !resolveOblivionScrollChameleon(mStore, *book)))
                    throw std::invalid_argument("saved native scroll cast content no longer resolves");
        }
        for (const auto& [id, projectile] : state.mNativeMagicProjectiles)
        {
            std::optional<OblivionTargetDamage> damage;
            if (const auto* spell = mStore.get<ESM4::Spell>().search(projectile.mSource))
                {
                damage = resolveOblivionVoiceOfEmperor(mStore, *spell);
                if (!damage) damage = resolveOblivionTargetSpellDamage(mStore, *spell);
            }
            else if (const auto* scroll = mStore.get<ESM4::Book>().search(projectile.mSource))
                damage = resolveOblivionScrollDamage(mStore, *scroll);
            else if (const auto* enchantment = mStore.get<ESM4::Enchantment>().search(projectile.mSource);
                enchantment && !(enchantment->mFlags & ESM4::Rec_Deleted)
                && enchantment->mData && enchantment->mData->mType == 1)
                damage = resolveTargetDamage(mStore, enchantment->mEffects);
            if (projectile.mEffectCode == ESM::fourCC("CHRM"))
            {
                const auto playerKey = ESM::FormKey::dynamic("player", 1);
                const auto values = std::find_if(state.mNativeActorValues.begin(), state.mNativeActorValues.end(),
                    [&](const auto& actor) { return actor.mActor == playerKey; });
                if (projectile.mCaster != playerKey || projectile.mMagnitude != 30
                    || values == state.mNativeActorValues.end()
                    || std::none_of(values->mUsedPowers.begin(), values->mUsedPowers.end(),
                        [&](const auto& use) { return use.mSpell == projectile.mSource; }))
                    throw std::invalid_argument("saved native Charm flight has no Player daily-use owner");
            }
            const auto* cell = mStore.get<ESM4::Cell>().search(projectile.mCell);
            if (!damage || !cell || (cell->mFlags & ESM4::Rec_Deleted)
                || damage->mEffectCode != projectile.mEffectCode || damage->mModel != projectile.mModel)
                throw std::invalid_argument("saved native magic projectile content no longer resolves");
        }
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
