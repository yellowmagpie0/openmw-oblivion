#include "oblivioncombatmagic.hpp"

#include "esmstore.hpp"
#include "worldimp.hpp"
#include "class.hpp"
#include "player.hpp"
#include "inventorystore.hpp"
#include "scene.hpp"
#include "worldmodel.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "projectilemanager.hpp"
#include "oblivionprofileservices.hpp"
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
#include <components/esm4/loadspel.hpp>
#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadgmst.hpp>
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

    bool supportsOblivionIngredientRestoration(const ESMStore& store, const ESM4::Ingredient& ingredient)
    {
        if ((ingredient.mFlags & ESM4::Rec_Deleted) || !ingredient.mScriptId.isZeroOrUnset()
            || ingredient.mEffects.empty()) return false;
        if (const auto key = store.get<ESM4::Ingredient>().findFormKey(ESM::RefId(ingredient.mId)))
            if (const auto* winner = store.getFormKeyIndex().winner(*key);
                winner && (winner->mDeleted || winner->mRecordType != ESM4::REC_INGR)) return false;
        const auto& effect = ingredient.mEffects.front();
        // Only this authored stock-food profile has an independent ingestion
        // oracle. Other magnitudes/durations must not inherit a guessed rule.
        if (effect.mId != ESM::fourCC("REFA") || effect.mRange != 0 || effect.mArea
            || effect.mMagnitude != 15 || effect.mDuration != 1
            || effect.mActorValue != 10 || effect.mScriptEffect) return false;
        const auto* definition = ingredientEffectDefinition(store, effect.mId);
        if (!definition || !definition->mData) return false;
        const auto& data = *definition->mData;
        return data.mFlags == 0x01000870 && !data.mAssociatedForm && data.mAssociatedData == 10
            && data.mSchool == 5 && data.mResistanceActorValue == -1;
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
        if (!ingredient || !supportsOblivionIngredientRestoration(store, *ingredient)) return false;
        const auto key = store.get<ESM4::Ingredient>().findFormKey(item.getCellRef().getRefId());
        const auto* before = combat->findActorValues(ESM::FormKey::dynamic("player", 1));
        const auto* life = combat->findActorLife(ESM::FormKey::dynamic("player", 1));
        if (!key || !before || !life || life->mPhase != ESM4::ActorLifePhase::Alive) return false;
        auto candidate = *before;
        candidate.mIngredientRestoration.push_back({*key, 0.f});
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
        Log(Debug::Info) << "TES4 ingredient consumed: ingredient=" << key->serialize()
            << " magnitude=" << ESM4::IngredientFatigueMagnitude << " duration=" << ESM4::IngredientFatigueDuration;
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
                if (!ingredient || !supportsOblivionIngredientRestoration(store, *ingredient))
                    throw std::invalid_argument("saved native ingredient restoration no longer resolves");
            }
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

    bool World::releaseOblivionPlayerSpellProjectile(const ESM::FormKey& spellKey, float power)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionCombat || !mPhysics) return false;
        const auto actor = getPlayerPtr();
        const auto caster = ESM::FormKey::dynamic("player", 1);
        const auto* cast = mOblivionCombat->findSpellCast(caster);
        const auto* spell = mStore.get<ESM4::Spell>().search(spellKey);
        const auto* scroll = spell ? nullptr : mStore.get<ESM4::Book>().search(spellKey);
        const auto damage = spell ? resolveOblivionTargetSpellDamage(mStore, *spell)
            : scroll ? resolveOblivionScrollDamage(mStore, *scroll) : std::nullopt;
        if (!cast || cast->mSpell != spellKey || cast->mReleased || !damage
            || !std::isfinite(power) || power < 0 || actor.isEmpty() || !actor.isInCell()
            || !actor.getCell()->getCell()->isEsm4()) return false;
        // Cost belongs to the pending cast, even if equipment or selection changes
        // during windup. Prepare resource and projectile owners before publication.
        const auto* current = mOblivionCombat->findActorValues(caster);
        if (!current) return false;
        auto values = *current;
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
        auto& projectiles = mOblivionRuntimeState->mNativeMagicProjectiles;
        if (projectiles.empty() || duration == 0)
        {
            mProjectileManager->syncOblivionMagicProjectiles(projectiles);
            return;
        }
        const auto player = getPlayerPtr();
        const auto settings = magicSettings(mStore);
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
                const auto& target = hit.mHitObject;
                if (!target.isEmpty() && target.getClass().isActor()
                    && !target.getClass().getCreatureStats(target).isDead())
                {
                    const bool targetPlayer = target == player;
                    const auto value = [&](std::uint8_t av) { return targetPlayer ? mOblivionCombat->getPlayerValue(av)
                        : mOblivionCombat->getNonPlayerValue(target, av); };
                    if (value(52) != 0 || value(53) != 0)
                        throw std::runtime_error("native projectile absorption and reflection are not implemented");
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
                }
                // Removal cannot throw after Health/death publication, so the
                // same contact is never retried even if a later frame fails.
                it = projectiles.erase(it);
                Log(Debug::Info) << "M16 magic projectile impact: id=" << serial
                    << " actor=" << (!target.isEmpty() && target.getClass().isActor()) << " damage=" << appliedDamage;
            }
            else if (next.mDistance >= next.mMaximumDistance) it = projectiles.erase(it);
            else { std::swap(it->second, next); ++it; }
        }
        mProjectileManager->syncOblivionMagicProjectiles(projectiles);
    }

    void World::validateOblivionMagicProjectiles(const ESM4::RuntimeState& state) const
    {
        for (const auto& [actor, cast] : state.mNativeSpellCasts)
        {
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
                damage = resolveOblivionTargetSpellDamage(mStore, *spell);
            else if (const auto* scroll = mStore.get<ESM4::Book>().search(projectile.mSource))
                damage = resolveOblivionScrollDamage(mStore, *scroll);
            else if (const auto* enchantment = mStore.get<ESM4::Enchantment>().search(projectile.mSource);
                enchantment && !(enchantment->mFlags & ESM4::Rec_Deleted)
                && enchantment->mData && enchantment->mData->mType == 1)
                damage = resolveTargetDamage(mStore, enchantment->mEffects);
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
