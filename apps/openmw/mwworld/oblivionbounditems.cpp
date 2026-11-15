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
            return code == ESM4::EFI_BWMA || code == ESM4::EFI_BWDA;
        }

        bool admittedBoundEffect(std::uint32_t code)
        {
            return code == ESM4::EFI_MYTH || code == ESM4::EFI_MYHL || boundWeaponEffect(code);
        }

        std::vector<ESM4::BoundEquipmentGrant> grantsFor(const ESMStore& store, const ESM::FormKey& base)
        {
            std::vector<ESM4::BoundEquipmentGrant> grants;
            for (const auto& ability : resolveOblivionNpcBoundAbilityInputs(store, base))
            {
                ESM4::BoundEquipmentGrant grant{ability.mSpell, {}};
                for (const auto& effect : ability.mEffects) grant.mItems.push_back(effect.mItem);
                grants.push_back(std::move(grant));
            }
            return grants;
        }
    }

    std::vector<OblivionBoundAbilityInput> resolveOblivionNpcBoundAbilityInputs(
        const ESMStore& store, const ESM::FormKey& base)
    {
        const auto* npc = store.get<ESM4::Npc>().search(base);
        if (!npc || !npc->mIsTES4) return {};
        std::vector<OblivionBoundAbilityInput> result;
        std::set<ESM::FormKey> seen;
        std::uint32_t occupiedSlots = 0;
        std::optional<ESM::FormId> weaponSpell;
        // Bound weapons share one equipped slot. Keep the last authored
        // weapon ability, without discarding armor from earlier abilities.
        for (const auto id : npc->mSpell)
        {
            const auto* spell = store.get<ESM4::Spell>().search(id);
            if (spell && spell->mData && spell->mData->mType == 4
                && std::any_of(spell->mEffects.begin(), spell->mEffects.end(),
                    [](const auto& effect) { return boundWeaponEffect(effect.mId); }))
                weaponSpell = id;
        }
        for (const auto id : npc->mSpell)
        {
            const auto* spell = store.get<ESM4::Spell>().search(id);
            if (!spell || !spell->mData || spell->mData->mType != 4) continue;
            if (std::none_of(spell->mEffects.begin(), spell->mEffects.end(),
                [](const auto& effect) { return admittedBoundEffect(effect.mId); })) continue;
            const auto key = store.get<ESM4::Spell>().findFormKey(ESM::RefId(id));
            if (!key || spell->mFlags & ESM4::Rec_Deleted)
                throw std::invalid_argument("native bound ability requires a winning SPEL identity");
            if (!seen.insert(*key).second) continue;
            OblivionBoundAbilityInput ability{*key,{}};
            for (std::size_t index = 0; index < spell->mEffects.size(); ++index)
            {
                const auto& effect = spell->mEffects[index];
                if (!admittedBoundEffect(effect.mId) || effect.mRange != 0 || effect.mArea != 0 || effect.mScriptEffect)
                    throw std::invalid_argument("unsupported native bound ability effect class, range or area");
                const ESM4::EffectSetting* definition = nullptr;
                for (const auto& candidate : store.get<ESM4::EffectSetting>())
                    if (candidate.mEffectCode == effect.mId)
                    {
                        if (definition) throw std::invalid_argument("ambiguous native bound effect code");
                        definition = &candidate;
                    }
                if (!definition || !definition->mData || !definition->mData->mAssociatedForm
                    || definition->mFlags & ESM4::Rec_Deleted)
                    throw std::invalid_argument("native bound effect has no winning associated item");
                const auto& itemKey = *definition->mData->mAssociatedForm;
                const bool weapon = boundWeaponEffect(effect.mId);
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
                if (weapon && weaponSpell && id != *weaponSpell)
                    continue;
                if (occupiedSlots & item->mSlots)
                    throw std::invalid_argument("overlapping native bound equipment abilities are not admitted");
                occupiedSlots |= item->mSlots;
                ESM4::RuntimeInventoryItem grant;
                grant.mBase = itemKey; grant.mCount = 1; grant.mCondition = item->mMaxCondition;
                grant.mCharge = item->mMaxCharge; grant.mEquippedSlots = item->mSlots;
                grant.mBoundSource = ESM4::BoundItemSource{*key, static_cast<std::uint32_t>(index)};
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
        for (const auto& [actor, abilities] : state.mNativeBoundAbilities)
        {
            const auto reference = std::find_if(state.mReferences.begin(), state.mReferences.end(),
                [&](const auto& ref) { return ref.mKey == actor; });
            if (reference == state.mReferences.end())
                throw std::invalid_argument("native bound equipment requires a saved NPC reference");
            const auto* npc = store.get<ESM4::Npc>().searchStatic(reference->mBase);
            if (!npc || !npc->mIsTES4 || (npc->mFlags & ESM4::Rec_Deleted))
                throw std::invalid_argument("native bound equipment owner is not a winning TES4 NPC");
            const auto grants = grantsFor(store, reference->mBase);
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

    void World::updateOblivionBoundEquipment(const Ptr& actor)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || !mOblivionCombat || actor.isEmpty()
            || actor.getType() != ESM::REC_NPC_4 || mUpdatingOblivionBoundEquipment
            || mPendingOblivionRuntimeState)
            return;
        // Initial cell insertion precedes Player placement. Grants publish a
        // validated whole-world snapshot, so defer them until that snapshot
        // can include the Player cell. startNewGame retries loaded actors.
        const auto player = getPlayerPtr();
        if (player.isEmpty() || !player.isInCell()) return;
        const auto key = actor.getCellRef().getFormKey();
        const auto* life = mOblivionCombat->findActorLife(key);
        if (!life) return;
        const ESM::FormKeyResolver resolver(mContentFiles);
        const auto base = resolver.toFormKey(actor.get<ESM4::Npc>()->mBase->mId);
        const auto grants = grantsFor(mStore, base);
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
        // The cached state owns persistent maps, but its actor/reference rows
        // can predate later registrations and dialogue writes. A transaction
        // that validates the whole world must merge the live authorities too.
        auto state = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());
        auto reference = std::find_if(state->mReferences.begin(), state->mReferences.end(),
            [&](const auto& ref) { return ref.mKey == key; });
        if (reference == state->mReferences.end())
            throw std::invalid_argument("bound equipment actor is absent from world capture");
        reference->mInventory = update.mInventory;
        if (update.mAbilities.empty()) state->mNativeBoundAbilities.erase(key);
        else state->mNativeBoundAbilities.insert_or_assign(key, std::move(update.mAbilities));
        state->validate();

        auto& target = actor.getClass().getInventoryStore(actor);
        std::vector<Ptr> removed, inserted;
        for (auto it = target.begin(); it != target.end(); ++it)
            if (it->mRef->mWorldModel) removed.push_back(*it);
        for (auto it = contents->begin(); it != contents->end(); ++it)
        {
            Ptr item = *it;
            item.setContainerStore(&target);
            inserted.push_back(item);
        }
        auto registry = mWorldModel.preparePtrReplacement(removed, inserted);
        if (!registry.isValid()) throw std::logic_error("stale bound equipment registry publication");
        registry.commit();
        target.swapPreparedContents(*contents);
        target.setPtr(actor);
        mOblivionRuntimeState.swap(state);
        MWClass::ESM4Npc::refreshNativeEquipment(actor);
        target.notifyPreparedEquipmentChange();
    }

    void World::removeOblivionDeadBoundEquipment()
    {
        if (!mOblivionRuntimeState || !mOblivionCombat) return;
        std::vector<ESM::FormKey> dead;
        for (const auto& [actor, abilities] : mOblivionRuntimeState->mNativeBoundAbilities)
            if (const auto* life = mOblivionCombat->findActorLife(actor);
                life && life->mPhase == ESM4::ActorLifePhase::Dead) dead.push_back(actor);
        for (const auto& key : dead)
        {
            const auto id = ESM::FormKeyResolver(mContentFiles).toFormId(key);
            const Ptr actor = key.isDynamic() ? mWorldModel.getDynamicNativePtr(key)
                : id ? mWorldModel.getPtr(*id) : Ptr{};
            if (!actor.isEmpty())
            {
                updateOblivionBoundEquipment(actor);
                continue;
            }
            auto state = std::make_unique<ESM4::RuntimeState>(*mOblivionRuntimeState);
            auto reference = std::find_if(state->mReferences.begin(), state->mReferences.end(),
                [&](const auto& ref) { return ref.mKey == key; });
            if (reference == state->mReferences.end())
                throw std::invalid_argument("dead bound equipment owner has no saved reference");
            auto update = ESM4::prepareBoundEquipmentUpdate(
                reference->mInventory, state->mNativeBoundAbilities.at(key), {}, true);
            reference->mInventory = std::move(update.mInventory);
            state->mNativeBoundAbilities.erase(key);
            mOblivionRuntimeState.swap(state);
        }
    }
}
