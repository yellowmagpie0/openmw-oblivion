#include "oblivionbounditems.hpp"

#include "esmstore.hpp"
#include "oblivionprofileservices.hpp"
#include "worldimp.hpp"
#include "worldmodel.hpp"
#include "inventorystore.hpp"
#include "../mwclass/esm4npc.hpp"
#include "../mwmechanics/oblivioncombat.hpp"
#include <components/esm4/boundequipment.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadweap.hpp>
#include <algorithm>
#include <cmath>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadspel.hpp>
#include <components/esm4/magiceffectid.hpp>
#include <components/esm4/referenceflags.hpp>
#include <set>
#include <stdexcept>

namespace MWWorld
{
    namespace
    {
        bool boundWeaponEffect(std::uint32_t code)
        {
            return code == ESM4::EFI_BWMA || code == ESM4::EFI_BWDA || code == ESM4::EFI_BWSW;
        }

        bool admittedBoundEffect(std::uint32_t code)
        {
            return code == ESM4::EFI_MYTH || code == ESM4::EFI_MYHL || boundWeaponEffect(code);
        }

        ESM4::RuntimeInventoryItem boundItem(const ESMStore& store, const ESM::FormKey& spell,
            std::uint32_t effectCode, std::uint32_t index)
        {
            const ESM4::EffectSetting* definition = nullptr;
            for (const auto& candidate : store.get<ESM4::EffectSetting>())
                if (candidate.mEffectCode == effectCode)
                {
                    if (definition) throw std::invalid_argument("ambiguous native bound effect code");
                    definition = &candidate;
                }
            if (!definition || !definition->mData || !definition->mData->mAssociatedForm
                || definition->mFlags & ESM4::Rec_Deleted)
                throw std::invalid_argument("native bound effect has no winning associated item");
            const auto& itemKey = *definition->mData->mAssociatedForm;
            const bool weapon = boundWeaponEffect(effectCode);
            std::optional<ESM4::InventoryItemDefinition> item;
            if (weapon)
            {
                const auto* record = store.get<ESM4::Weapon>().searchStatic(itemKey);
                if (record && !(record->mFlags & ESM4::Rec_Deleted))
                    item = OblivionProfileServices::itemDefinition(store, ESM::RefId(record->mId));
            }
            else
            {
                const auto* record = store.get<ESM4::Armor>().searchStatic(itemKey);
                if (record && !(record->mFlags & ESM4::Rec_Deleted))
                    item = OblivionProfileServices::itemDefinition(store, ESM::RefId(record->mId));
            }
            const std::uint32_t association = definition->mData->mFlags & 0x70000u;
            if (!item || !itemKey.isContent() || association != (weapon ? 0x10000u : 0x20000u)
                || item->mType != (weapon ? ESM4::InventoryItemType::Weapon : ESM4::InventoryItemType::Armor)
                || item->mSlots == 0 || item->mChooseOneSlot || item->mMaxCondition <= 0)
                throw std::invalid_argument("native bound effect has an incompatible associated item");
            ESM4::RuntimeInventoryItem grant;
            grant.mBase = itemKey; grant.mCount = 1; grant.mCondition = item->mMaxCondition;
            grant.mCharge = item->mMaxCharge; grant.mEquippedSlots = item->mSlots;
            grant.mBoundSource = ESM4::BoundItemSource{spell, index};
            return grant;
        }

        std::optional<ESM4::BoundEquipmentGrant> scriptedBoundGrant(const ESMStore& store,
            const ESM::FormKey& key, unsigned int& duration)
        {
            const auto* spell = store.get<ESM4::Spell>().searchStatic(key);
            if (!spell || !spell->mData || spell->mFlags & ESM4::Rec_Deleted || spell->mEffects.empty())
                return std::nullopt;
            duration = spell->mEffects.front().mDuration;
            ESM4::BoundEquipmentGrant grant{ key, {} };
            for (const auto& effect : spell->mEffects)
            {
                if ((!boundWeaponEffect(effect.mId) && effect.mId != ESM4::EFI_BAGA) || effect.mRange != 0
                    || effect.mArea != 0 || effect.mScriptEffect || !duration || effect.mDuration != duration)
                    return std::nullopt;
                grant.mItems.push_back(boundItem(store, key, effect.mId, static_cast<std::uint32_t>(grant.mItems.size())));
            }
            return grant;
        }

        void publishBoundInventory(WorldModel& world, const Ptr& actor, InventoryStore& contents)
        {
            auto& target = actor.getClass().getInventoryStore(actor);
            std::vector<Ptr> removed, inserted;
            for (auto it = target.begin(); it != target.end(); ++it)
                if (it->mRef->mWorldModel) removed.push_back(*it);
            for (auto it = contents.begin(); it != contents.end(); ++it)
            {
                Ptr item = *it;
                item.setContainerStore(&target);
                inserted.push_back(item);
            }
            auto registry = world.preparePtrReplacement(removed, inserted);
            if (!registry.isValid()) throw std::logic_error("stale bound equipment registry publication");
            registry.commit();
            target.swapPreparedContents(contents);
            target.setPtr(actor);
        }

        constexpr char remainingKey[] = "native.bound_power.remaining_seconds";

        // Select an independently castable, self-only equipment set. Mixed or competing
        // spells require a spell-selection policy beyond this combat opener.
        bool isBoundPower(const ESM4::Spell& spell)
        {
            return spell.mData && spell.mData->mType == 3 && spell.mData->mCost == 0
                && (spell.mData->mFlags & 1) && !(spell.mFlags & ESM4::Rec_Deleted) && !spell.mEffects.empty()
                && std::all_of(spell.mEffects.begin(), spell.mEffects.end(), [&](const auto& effect) {
                    return admittedBoundEffect(effect.mId) && !effect.mRange && !effect.mArea && !effect.mScriptEffect
                        && effect.mDuration && effect.mDuration == spell.mEffects.front().mDuration;
                });
        }

        std::vector<ESM4::BoundEquipmentGrant> grantsFor(const ESMStore& store, const ESM::FormKey& base,
            bool includePower = false, unsigned int* duration = nullptr)
        {
            std::vector<ESM4::BoundEquipmentGrant> grants;
            for (const auto& ability : resolveOblivionNpcBoundAbilityInputs(store, base, includePower))
            {
                if (duration && ability.mDuration) *duration = ability.mDuration;
                ESM4::BoundEquipmentGrant grant{ability.mSpell, {}};
                for (const auto& effect : ability.mEffects) grant.mItems.push_back(effect.mItem);
                grants.push_back(std::move(grant));
            }
            return grants;
        }
    }

    std::vector<OblivionBoundAbilityInput> resolveOblivionNpcBoundAbilityInputs(
        const ESMStore& store, const ESM::FormKey& base, bool includeCombatPower)
    {
        const auto* npc = store.get<ESM4::Npc>().search(base);
        if (!npc || !npc->mIsTES4) return {};
        std::vector<OblivionBoundAbilityInput> result;
        std::set<ESM::FormKey> seen;
        std::uint32_t occupiedSlots = 0;
        std::optional<ESM::FormId> weaponSpell;
        std::optional<ESM::FormId> combatPower;
        bool competing = false;
        // Bound weapons share one equipped slot. Keep the last authored
        // weapon ability, without discarding armor from earlier abilities.
        for (const auto id : npc->mSpell)
        {
            const auto* spell = store.get<ESM4::Spell>().search(id);
            if (includeCombatPower && spell && isBoundPower(*spell))
            {
                if (combatPower && *combatPower != id) competing = true;
                combatPower = id;
            }
            if (includeCombatPower && spell && spell->mData && spell->mData->mType == 4
                && std::any_of(spell->mEffects.begin(), spell->mEffects.end(),
                    [](const auto& effect) { return admittedBoundEffect(effect.mId); }))
                competing = true;
            if (spell && spell->mData && spell->mData->mType == 4
                && std::any_of(spell->mEffects.begin(), spell->mEffects.end(),
                    [](const auto& effect) { return boundWeaponEffect(effect.mId); }))
                weaponSpell = id;
        }
        for (const auto id : npc->mSpell)
        {
            const auto* spell = store.get<ESM4::Spell>().search(id);
            const bool timed = !competing && combatPower == id;
            if (!spell || !spell->mData || (spell->mData->mType != 4 && !timed)) continue;
            if (std::none_of(spell->mEffects.begin(), spell->mEffects.end(),
                [](const auto& effect) { return admittedBoundEffect(effect.mId); })) continue;
            const auto key = store.get<ESM4::Spell>().findFormKey(ESM::RefId(id));
            if (!key || spell->mFlags & ESM4::Rec_Deleted)
                throw std::invalid_argument("native bound ability requires a winning SPEL identity");
            if (!seen.insert(*key).second) continue;
            OblivionBoundAbilityInput ability{*key,{}};
            if (timed) ability.mDuration = spell->mEffects.front().mDuration;
            for (std::size_t index = 0; index < spell->mEffects.size(); ++index)
            {
                const auto& effect = spell->mEffects[index];
                if (!admittedBoundEffect(effect.mId) || effect.mRange != 0 || effect.mArea != 0 || effect.mScriptEffect)
                    throw std::invalid_argument("unsupported native bound ability effect class, range or area");
                if (boundWeaponEffect(effect.mId) && weaponSpell && id != *weaponSpell)
                    continue;
                auto grant = boundItem(store, *key, effect.mId, static_cast<std::uint32_t>(index));
                if (occupiedSlots & grant.mEquippedSlots)
                    throw std::invalid_argument("overlapping native bound equipment abilities are not admitted");
                occupiedSlots |= grant.mEquippedSlots;
                ability.mEffects.push_back({static_cast<std::uint32_t>(index),effect.mId,std::move(grant)});
            }
            if (!ability.mEffects.empty())
                result.push_back(std::move(ability));
        }
        return result;
    }

    void validateOblivionBoundEquipment(const ESM4::RuntimeState& state, const ESMStore& store,
        const ESM::FormKeyResolver& resolver, const ESMStore* incoming)
    {
        for (const auto& ref : state.mReferences)
            if (ref.mCustomState.contains(remainingKey) && !state.mNativeBoundAbilities.contains(ref.mKey))
                throw std::invalid_argument("timed bound equipment has no owner");
        for (const auto& [actor, abilities] : state.mNativeBoundAbilities)
        {
            if (actor == state.mPlayer.mReference)
            {
                std::vector<ESM4::BoundEquipmentGrant> grants;
                for (const auto& ability : abilities)
                {
                    unsigned int duration = 0;
                    auto grant = scriptedBoundGrant(store, ability.mSpell, duration);
                    if (!grant || ability.mRemainingSeconds <= 0 || ability.mRemainingSeconds > duration)
                        throw std::invalid_argument("invalid player bound spell or duration");
                    OblivionProfileServices::prepareActorInventory(store, resolver, ability.mDisplacedEquipment, incoming);
                    grants.push_back(std::move(*grant));
                }
                ESM4::prepareBoundEquipmentUpdate(state.mPlayer.mInventory, abilities, grants);
                continue;
            }
            const auto reference = std::find_if(state.mReferences.begin(), state.mReferences.end(),
                [&](const auto& ref) { return ref.mKey == actor; });
            if (reference == state.mReferences.end())
                throw std::invalid_argument("native bound equipment requires a saved NPC reference");
            const auto* npc = store.get<ESM4::Npc>().searchStatic(reference->mBase);
            if (!npc || !npc->mIsTES4 || (npc->mFlags & ESM4::Rec_Deleted))
                throw std::invalid_argument("native bound equipment owner is not a winning TES4 NPC");
            const auto timer = reference->mCustomState.find(remainingKey);
            unsigned int duration = 0;
            const auto grants = grantsFor(store, reference->mBase, timer != reference->mCustomState.end(), &duration);
            if (timer != reference->mCustomState.end())
            {
                const auto* remaining = std::get_if<double>(&timer->second);
                if (!remaining || !std::isfinite(*remaining) || *remaining <= 0 || *remaining > duration)
                    throw std::invalid_argument("invalid native bound power lifetime");
            }
            for (const auto& ability : abilities)
            {
                if (std::none_of(grants.begin(), grants.end(), [&](const auto& grant) {
                        return grant.mSpell == ability.mSpell;
                    }))
                    throw std::invalid_argument("saved bound equipment spell is absent from the NPC ability list");
                // Resolve displaced equipment too; structural ownership alone
                // does not prove the saved ordinary item/slot graph is valid.
                OblivionProfileServices::prepareActorInventory(store, resolver, ability.mDisplacedEquipment, incoming);
            }
            ESM4::prepareBoundEquipmentUpdate(reference->mInventory, abilities, grants);
        }
    }

    void World::updateOblivionBoundEquipment(const Ptr& actor, bool castPower, bool expirePower)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionCombat || actor.isEmpty()
            || actor.getType() != ESM::REC_NPC_4 || mUpdatingOblivionBoundEquipment
            || mPendingOblivionRuntimeState)
            return;
        // Initial cell insertion precedes Player placement. The first owner
        // may need a world snapshot. startNewGame retries loaded actors.
        const auto player = getPlayerPtr();
        if (player.isEmpty() || !player.isInCell()) return;
        const auto key = actor.getCellRef().getFormKey();
        const auto* life = mOblivionCombat->findActorLife(key);
        if (!life) return;
        const ESM::FormKeyResolver resolver(mContentFiles);
        const auto base = resolver.toFormKey(actor.get<ESM4::Npc>()->mBase->mId);
        const auto* saved = findOblivionSavedReference(key);
        const bool activePower = saved && saved->mCustomState.contains(remainingKey);
        unsigned int duration = 0;
        const auto grants = grantsFor(mStore, base, castPower || (activePower && !expirePower), &duration);
        if (castPower && (!duration || activePower)) return;
        std::vector<ESM4::RuntimeBoundAbility> owners;
        if (mOblivionRuntimeState)
            if (const auto found = mOblivionRuntimeState->mNativeBoundAbilities.find(key);
                found != mOblivionRuntimeState->mNativeBoundAbilities.end()) owners = found->second;
        const bool dead = life->mPhase == ESM4::ActorLifePhase::Dead;
        if (owners.empty() && (grants.empty() || dead)) return;
        struct Guard
        {
            bool& mFlag;
            explicit Guard(bool& flag) : mFlag(flag) { mFlag = true; }
            ~Guard() { mFlag = false; }
        } guard(mUpdatingOblivionBoundEquipment);
        const auto inventory = captureOblivionInventory(actor);
        auto update = ESM4::prepareBoundEquipmentUpdate(inventory, owners, grants, dead);
        if (update.mInventory == inventory && update.mAbilities == owners)
        {
            MWClass::ESM4Npc::refreshNativeEquipment(actor);
            return;
        }

        // Stage all items and the canonical inventory/ownership baseline before
        // touching the live store. Snapshot capture can lazily construct other
        // actor views; the guard prevents recursive grant publication there.
        auto contents = OblivionProfileServices::stageActorInventory(
            OblivionProfileServices::prepareActorInventory(mStore, resolver, update.mInventory));
        // Most actors already have a reference row. Stage only the validated
        // inventory and its owners so equipping an item does not resnapshot
        // every actor and cell in the middle of a combat frame.
        std::unique_ptr<ESM4::RuntimeState> initial;
        auto* reference = findOblivionSavedReference(key);
        if (!reference)
        {
            initial = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());
            auto it = std::ranges::find(initial->mReferences, key, &ESM4::RuntimeReferenceState::mKey);
            if (it == initial->mReferences.end())
                throw std::invalid_argument("bound equipment actor is absent from world capture");
            reference = &*it;
        }
        auto& state = initial ? *initial : *mOblivionRuntimeState;
        auto abilities = state.mNativeBoundAbilities;
        if (update.mAbilities.empty()) abilities.erase(key);
        else abilities.insert_or_assign(key, std::move(update.mAbilities));
        auto custom = reference->mCustomState;
        if (dead || expirePower) custom.erase(remainingKey);
        else if (duration && !activePower) custom[remainingKey] = double(duration);

        publishBoundInventory(mWorldModel, actor, *contents);
        reference->mInventory.swap(update.mInventory);
        reference->mCustomState.swap(custom);
        state.mNativeBoundAbilities.swap(abilities);
        if (initial) mOblivionRuntimeState.swap(initial);
        MWClass::ESM4Npc::refreshNativeEquipment(actor);
        actor.getClass().getInventoryStore(actor).notifyPreparedEquipmentChange();
    }

    bool World::updateOblivionPlayerBoundSpells(const ESM::FormKey& cast, double elapsed)
    {
        if (!mOblivionRuntimeState)
            return false;
        auto& state = *mOblivionRuntimeState;
        const auto key = state.mPlayer.mReference;
        auto& owners = state.mNativeBoundAbilities;
        auto active = owners.find(key);
        if (cast.isNull() && active == owners.end())
            return false;
        if (cast.isNull() && std::all_of(active->second.begin(), active->second.end(),
                [&](const auto& entry) { return entry.mRemainingSeconds > elapsed; }))
        {
            for (auto& entry : active->second)
                entry.mRemainingSeconds -= elapsed;
            return true;
        }

        std::vector<ESM4::BoundEquipmentGrant> grants;
        std::map<ESM::FormKey, double> timers;
        std::uint32_t replacedSlots = 0;
        if (!cast.isNull())
        {
            unsigned int duration = 0;
            auto grant = scriptedBoundGrant(mStore, cast, duration);
            if (!grant)
                return false;
            for (const auto& item : grant->mItems)
                replacedSlots |= item.mEquippedSlots;
            grants.push_back(std::move(*grant));
            timers.emplace(cast, duration);
        }
        const std::vector<ESM4::RuntimeBoundAbility> empty;
        const auto& old = active == owners.end() ? empty : active->second;
        for (const auto& entry : old)
        {
            if (entry.mRemainingSeconds <= elapsed || entry.mSpell == cast)
                continue;
            unsigned int duration = 0;
            auto grant = scriptedBoundGrant(mStore, entry.mSpell, duration);
            if (!grant)
                throw std::invalid_argument("active player bound spell is missing its grant");
            if (std::any_of(grant->mItems.begin(), grant->mItems.end(),
                    [&](const auto& item) { return (item.mEquippedSlots & replacedSlots) != 0; }))
                continue;
            timers.emplace(entry.mSpell, entry.mRemainingSeconds - elapsed);
            grants.push_back(std::move(*grant));
        }

        const auto player = getPlayerPtr();
        auto update = ESM4::prepareBoundEquipmentUpdate(captureOblivionInventory(player), old, grants);
        for (auto& entry : update.mAbilities)
            entry.mRemainingSeconds = timers.at(entry.mSpell);
        auto contents = OblivionProfileServices::stageActorInventory(OblivionProfileServices::prepareActorInventory(
            mStore, ESM::FormKeyResolver(mContentFiles), update.mInventory));
        auto next = owners;
        if (update.mAbilities.empty())
            next.erase(key);
        else
            next.insert_or_assign(key, std::move(update.mAbilities));
        publishBoundInventory(mWorldModel, player, *contents);
        state.mPlayer.mInventory.swap(update.mInventory);
        owners.swap(next);
        player.getClass().getInventoryStore(player).notifyPreparedEquipmentChange();
        return true;
    }

    void World::advanceOblivionBoundEquipment(double seconds)
    {
        if (!mOblivionRuntimeState || !mOblivionCombat) return;
        updateOblivionPlayerBoundSpells({}, seconds);
        std::vector<ESM::FormKey> remove;
        for (const auto& [actor, abilities] : mOblivionRuntimeState->mNativeBoundAbilities)
        {
            if (actor == mOblivionRuntimeState->mPlayer.mReference)
                continue;
            if (const auto* life = mOblivionCombat->findActorLife(actor);
                life && life->mPhase == ESM4::ActorLifePhase::Dead)
                remove.push_back(actor);
            else if (seconds > 0)
                if (auto* reference = findOblivionSavedReference(actor))
                    if (const auto timer = reference->mCustomState.find(remainingKey); timer != reference->mCustomState.end())
                    {
                        auto& remaining = std::get<double>(timer->second);
                        if (seconds >= remaining) remove.push_back(actor);
                        else remaining -= seconds;
                    }
        }
        for (const auto& key : remove)
        {
            const auto id = ESM::FormKeyResolver(mContentFiles).toFormId(key);
            // A streamed-out NPC can retain a live inventory in its cell store
            // after leaving the registry. Expire that inventory and its owners together.
            const Ptr actor = key.isDynamic() ? mWorldModel.getDynamicNativePtr(key)
                : id ? mWorldModel.getResidentPtr(*id) : Ptr{};
            if (!actor.isEmpty())
            {
                updateOblivionBoundEquipment(actor, false, true);
                continue;
            }
            auto* reference = findOblivionSavedReference(key);
            if (!reference) throw std::invalid_argument("bound equipment owner has no saved reference");
            const auto* life = mOblivionCombat->findActorLife(key);
            const bool dead = life && life->mPhase == ESM4::ActorLifePhase::Dead;
            auto update = ESM4::prepareBoundEquipmentUpdate(
                reference->mInventory, mOblivionRuntimeState->mNativeBoundAbilities.at(key),
                grantsFor(mStore, reference->mBase), dead);
            reference->mInventory = std::move(update.mInventory);
            reference->mCustomState.erase(remainingKey);
            if (update.mAbilities.empty()) mOblivionRuntimeState->mNativeBoundAbilities.erase(key);
            else mOblivionRuntimeState->mNativeBoundAbilities.at(key) = std::move(update.mAbilities);
        }
    }
}
