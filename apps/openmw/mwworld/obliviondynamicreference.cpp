#include "worldimp.hpp"
#include "cellstore.hpp"
#include "class.hpp"
#include "esmstore.hpp"
#include "oblivionlevelled.hpp"
#include "oblivionspawn.hpp"
#include "oblivionscriptmanager.hpp"
#include "ptr.hpp"
#include "savedreference.hpp"
#include "scene.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include <components/debug/debuglog.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadlvlc.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadfact.hpp>
#include <components/esm4/loadscpt.hpp>
#include <components/esm4/runtimestate.hpp>
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace MWWorld
{
    namespace
    {
        const ESM4::Creature* placedActorTemplate(const ESMStore& store, const ESM::FormKey& actor)
        {
            const auto* placement = actor.isContent() ? store.get<ESM4::Reference>().search(actor) : nullptr;
            const auto* list = placement ? store.get<ESM4::LevelledCreature>().search(placement->mBaseObj) : nullptr;
            return list && !list->mTemplate.isZeroOrUnset() ? store.get<ESM4::Creature>().search(list->mTemplate) : nullptr;
        }
    }

    std::optional<ESM::FormKey> World::getOblivionReferenceScriptOverride(const ESM::FormKey& actor) const
    {
        if (const auto* actorTemplate = placedActorTemplate(mStore, actor))
            return ESM::FormKeyResolver(mContentFiles).toFormKey(actorTemplate->mScriptId);
        if (!actor.isDynamic() || actor.mNamespace != "native-reference") return {};
        const auto* state = mPendingOblivionRuntimeState ? mPendingOblivionRuntimeState.get() : mOblivionRuntimeState.get();
        if (state)
            for (const auto& reference : state->mReferences)
                if (reference.mKey == actor) return oblivionSpawnScript(reference);
        return {};
    }

    std::vector<ESM4::ActorFaction> World::getOblivionReferenceFactions(const ESM::FormKey& actor) const
    {
        const auto* state = mPendingOblivionRuntimeState ? mPendingOblivionRuntimeState.get() : mOblivionRuntimeState.get();
        if (const auto* actorTemplate = placedActorTemplate(mStore, actor))
            return getOblivionActorFactions(actorTemplate->mFormKey);
        if (state && actor.isDynamic() && actor.mNamespace == "native-reference")
            for (const auto& reference : state->mReferences)
                if (reference.mKey == actor)
                    if (const auto factions = oblivionSpawnFactions(reference))
                    {
                        const ESM::FormKeyResolver resolver(mContentFiles);
                        std::vector<ESM4::ActorFaction> result;
                        for (const auto& entry : *factions)
                        {
                            const auto id = resolver.toFormId(entry.mFaction);
                            if (!id) throw std::invalid_argument("Unresolved spawned actor faction");
                            result.push_back({id->toUint32(), entry.mRank, 0, 0, 0});
                        }
                        return result;
                    }
        const auto base = getOblivionReferenceBaseKey(actor);
        return base ? getOblivionActorFactions(*base) : std::vector<ESM4::ActorFaction>{};
    }

    void World::validateOblivionSpawn(const ESM4::RuntimeReferenceState& reference) const
    {
        const auto& custom = reference.mCustomState;
        const bool spawned = std::ranges::any_of(custom,
            [](const auto& entry) { return entry.first.starts_with("native.spawn."); });
        if (!spawned) return;
        if (custom.contains("native.spawn.inventory_pending"))
            throw std::invalid_argument("Native spawned actor inventory construction is incomplete");
        const auto key = [&](const char* name) {
            const auto found = custom.find(name);
            const auto* value = found == custom.end() ? nullptr : std::get_if<std::string>(&found->second);
            if (!value) throw std::invalid_argument("Missing native spawn provenance");
            const auto result = ESM::FormKey::deserialize(*value);
            if (result.isNull() || result.serialize() != *value)
                throw std::invalid_argument("Invalid native spawn provenance");
            return result;
        };
        if (!reference.mKey.isDynamic() || reference.mKey.mNamespace != "native-reference")
            throw std::invalid_argument("Native spawn must own a dynamic reference identity");
        const auto source = key("native.spawn.source");
        if (!source.isContent() && !(source.isDynamic() && source.mNamespace == "native-reference"))
            throw std::invalid_argument("Invalid native spawn anchor identity");
        const auto requested = key("native.spawn.requested_base");
        const auto levelIt = custom.find("native.spawn.player_level");
        const auto* level = levelIt == custom.end() ? nullptr : std::get_if<std::int64_t>(&levelIt->second);
        if (!level || *level < 1 || *level > 32767)
            throw std::invalid_argument("Invalid native spawn level");
        const auto* list = mStore.get<ESM4::LevelledCreature>().search(requested);
        if (list ? !oblivionLevelledActorListContains(mStore, requested, reference.mBase, int(*level))
                 : requested != reference.mBase)
            throw std::invalid_argument("Saved native spawn is not eligible in its original list");
        const auto script = oblivionSpawnScript(reference);
        const auto factions = oblivionSpawnFactions(reference);
        if (list && !list->mTemplate.isZeroOrUnset())
        {
            const auto* actorTemplate = mStore.get<ESM4::Creature>().search(list->mTemplate);
            const ESM::FormKeyResolver resolver(mContentFiles);
            if (!actorTemplate || (actorTemplate->mFlags & ESM4::Rec_Deleted)
                || !mStore.get<ESM4::Creature>().search(reference.mBase)
                || key("native.spawn.template") != actorTemplate->mFormKey
                || !script || *script != resolver.toFormKey(actorTemplate->mScriptId) || !factions
                || !supportsOblivionLevelledActor(mStore, requested, int(*level), true))
                throw std::invalid_argument("Invalid saved native actor template");
            if (!script->isNull() && !mStore.get<ESM4::Script>().search(*script))
                throw std::invalid_argument("Missing spawned actor script");
            for (const auto& entry : *factions)
            {
                const auto* faction = mStore.get<ESM4::Faction>().search(entry.mFaction);
                if (!faction || (faction->mFlags & ESM4::Rec_Deleted))
                    throw std::invalid_argument("Missing spawned actor faction");
            }
        }
        else if (script || factions || custom.contains("native.spawn.template"))
            throw std::invalid_argument("Unexpected native actor template snapshot");
    }

    ESM::FormKey World::placeOblivionActorAt(const Ptr& anchor, const ESM::FormKey& base)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion || mPendingOblivionRuntimeState
            || anchor.isEmpty() || !anchor.isInCell() || anchor.mRef->isDeleted()
            || !mWorldScene || !mWorldScene->isCellActive(*anchor.getCell()))
            throw std::invalid_argument("Native PlaceAtMe requires a resident marker in an active cell");
        const ESM::FormKeyResolver resolver(mContentFiles);
        const auto requested = resolver.toFormId(base);
        if (!requested)
            throw std::invalid_argument("Native PlaceAtMe requires an original actor or leveled actor base");
        const int level = getPlayerPtr().getClass().getCreatureStats(getPlayerPtr()).getLevel();
        auto random = getPrng();
        auto selected = requested;
        const auto* list = mStore.get<ESM4::LevelledCreature>().search(*requested);
        const ESM4::Creature* actorTemplate = nullptr;
        if (list)
        {
            if (!list->mTemplate.isZeroOrUnset())
            {
                actorTemplate = mStore.get<ESM4::Creature>().search(list->mTemplate);
                if (!actorTemplate || (actorTemplate->mFlags & ESM4::Rec_Deleted))
                    throw std::invalid_argument("Unsupported native PlaceAtMe actor template");
            }
            if (!supportsOblivionLevelledActor(mStore, base, level, actorTemplate != nullptr))
                throw std::invalid_argument("Unsupported native PlaceAtMe leveled actor list");
            selected = selectOblivionLevelledActor(mStore, base, level, random);
        }
        if (!selected)
        {
            getPrng() = random;
            return {};
        }
        const auto* npc = mStore.get<ESM4::Npc>().search(*selected);
        const auto* creature = mStore.get<ESM4::Creature>().search(*selected);
        if ((!npc && !creature) || (npc && (npc->mFlags & ESM4::Rec_Deleted))
            || (creature && (creature->mFlags & ESM4::Rec_Deleted)))
            throw std::invalid_argument("Native PlaceAtMe currently requires a winning NPC or creature");
        const auto* cellId = anchor.getCell()->getCell()->getId().getIf<ESM::FormId>();
        if (!cellId)
            throw std::invalid_argument("Native PlaceAtMe marker has no original cell identity");
        if (!mOblivionRuntimeState)
            mOblivionRuntimeState = std::make_unique<ESM4::RuntimeState>(captureOblivionRuntimeState());

        auto keys = prepareOblivionDynamicReferenceKey();
        const auto key = keys->key();
        ESM4::Reference placed{};
        placed.mFormKey = key;
        placed.mParent = ESM::RefId(*cellId);
        placed.mParentKey = resolver.toFormKey(*cellId);
        placed.mBaseObj = *selected;
        placed.mBaseKey = resolver.toFormKey(*selected);
        placed.mPos = anchor.getRefData().getPosition();
        auto insertion = prepareSavedNativeReference(mStore, *anchor.getCell(), placed);
        const Ptr actor = insertion->get();
        const std::array<Ptr, 1> inserted{actor};
        auto registry = mWorldModel.preparePtrReplacement({}, inserted);
        ESM4::RuntimeReferenceState saved;
        saved.mKey = key;
        saved.mBase = placed.mBaseKey;
        saved.mCell = placed.mParentKey;
        saved.mPosition = placed.mPos;
        saved.mCustomState["native.spawn.source"] = anchor.getCellRef().getFormKey().serialize();
        saved.mCustomState["native.spawn.requested_base"] = base.serialize();
        saved.mCustomState["native.spawn.player_level"] = std::int64_t(level);
        saved.mCustomState["native.spawn.inventory_pending"] = true;
        if (actorTemplate)
        {
            if (!creature)
                throw std::invalid_argument("Native creature template requires a creature leaf");
            saved.mCustomState["native.spawn.template"] = actorTemplate->mFormKey.serialize();
            saved.mCustomState["native.spawn.script"] = resolver.toFormKey(actorTemplate->mScriptId).serialize();
            std::vector<ESM4::RuntimeActorFaction> factions;
            for (const auto& faction : getOblivionActorFactions(actorTemplate->mFormKey))
                factions.push_back({resolver.toFormKey(ESM::FormId::fromUint32(faction.faction)), faction.rank});
            setOblivionSpawnFactions(saved, factions);
        }
        auto& references = mOblivionRuntimeState->mReferences;
        references.reserve(references.size() + 1);
        if (!keys->isValid() || !insertion->isValid() || !registry.isValid() || !keys->commit())
            throw std::logic_error("Stale native PlaceAtMe preparation");
        insertion->commit();
        registry.commit();
        references.insert(std::lower_bound(references.begin(), references.end(), key,
            [](const auto& ref, const auto& id) { return ref.mKey < id; }), std::move(saved));
        getPrng() = random;

        // Commit the selected base before lazy actor/inventory construction.
        // Its native key and level remain authoritative across later loads.
        actor.getClass().getInventoryStore(actor);
        auto inventory = captureOblivionInventory(actor);
        auto& currentReferences = mOblivionRuntimeState->mReferences;
        const auto live = std::lower_bound(currentReferences.begin(), currentReferences.end(), key,
            [](const auto& ref, const auto& id) { return ref.mKey < id; });
        if (live == currentReferences.end() || live->mKey != key)
            throw std::logic_error("Native PlaceAtMe lost its published actor");
        live->mInventory = std::move(inventory);
        live->mCustomState.erase("native.spawn.inventory_pending");
        mWorldScene->addObjectToScene(actor);
        if (mOblivionScriptManager)
            mOblivionScriptManager->dispatchObjectEvent(actor, "onload");
        Log(Debug::Info) << "Native PlaceAtMe actor=" << key.serialize()
                         << " base=" << placed.mBaseKey.serialize()
                         << " requested=" << base.serialize()
                         << " source=" << anchor.getCellRef().getFormKey().serialize();
        return key;
    }

    World::PreparedOblivionDynamicReferenceKey::PreparedOblivionDynamicReferenceKey(
        World& owner, std::vector<ESM::FormKey> keys,
        std::unique_ptr<WorldModel::PreparedPtrReplacement> registry)
        : mOwner(&owner), mIdentity(owner.mOblivionDynamicReferenceIdentity),
          mKeys(std::move(keys)), mRegistry(std::move(registry)) {}

    bool World::PreparedOblivionDynamicReferenceKey::isValid() const noexcept
    {
        const auto identity = mIdentity.lock();
        // Never touch the borrowed World after destruction or address reuse.
        return identity && !mCommitted
            && identity == mOwner->mOblivionDynamicReferenceIdentity
            && mOwner->mNextOblivionDynamicSerial == mKeys.front().mValue
            && mRegistry->isValid();
    }

    bool World::PreparedOblivionDynamicReferenceKey::commit() noexcept
    {
        if (!isValid()) return false;
        mOwner->mNextOblivionDynamicSerial += mKeys.size();
        mCommitted = true;
        return true;
    }

    std::unique_ptr<World::PreparedOblivionDynamicReferenceKey>
    World::prepareOblivionDynamicReferenceKey()
    {
        return prepareOblivionDynamicReferenceKeys(1);
    }

    std::unique_ptr<World::PreparedOblivionDynamicReferenceKey>
    World::prepareOblivionDynamicReferenceKeys(std::size_t count)
    {
        if (mGameProfile != ESM::GameProfile::Oblivion)
            throw std::invalid_argument("native reference identity requires the Oblivion profile");
        if (count == 0)
            throw std::invalid_argument("native reference identity group must not be empty");
        if (mNextOblivionDynamicSerial == 0
            || count > std::numeric_limits<std::uint64_t>::max() - mNextOblivionDynamicSerial)
            throw std::overflow_error("native reference identity namespace is exhausted");
        const auto key = ESM::FormKey::dynamic("native-reference", mNextOblivionDynamicSerial);
        const auto check = [&](const ESM::FormKey& existing) {
            if (existing.isDynamic() && existing.mNamespace == key.mNamespace
                && existing.mValue >= key.mValue)
                throw std::invalid_argument("native reference serial would reuse an existing identity");
        };
        for (const auto& ptr : mWorldModel.getResidentPtrs())
            check(ptr.getCellRef().getFormKey());
        if (mOblivionRuntimeState)
            for (const auto& reference : mOblivionRuntimeState->mReferences)
                check(reference.mKey);
        auto registry = std::make_unique<WorldModel::PreparedPtrReplacement>(
            mWorldModel.preparePtrReplacement({}, {}));
        if (!mOblivionDynamicReferenceIdentity)
            mOblivionDynamicReferenceIdentity = std::make_shared<const char>();
        std::vector<ESM::FormKey> keys;
        keys.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            keys.push_back(ESM::FormKey::dynamic("native-reference", mNextOblivionDynamicSerial + i));
        return std::unique_ptr<PreparedOblivionDynamicReferenceKey>(
            new PreparedOblivionDynamicReferenceKey(*this, std::move(keys), std::move(registry)));
    }
}
