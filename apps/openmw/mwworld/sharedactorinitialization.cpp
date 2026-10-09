#include "sharedactorinitialization.hpp"

#include <stdexcept>

#include <components/esm3/creaturestate.hpp>
#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loadlevlist.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/npcstate.hpp>

#include "../mwclass/npc.hpp"
#include "customdata.hpp"

namespace MWWorld
{
    SharedActorInitialization::SharedActorInitialization(WorldModel& model, const ESMStore& content)
        : mModel(model)
        , mContent(content)
    {
    }

    SharedActorInitialization::~SharedActorInitialization() = default;

    void SharedActorInitialization::requestSpells(const ESM::RefId& id, int type)
    {
        auto [entry, inserted] = mSpellLists.try_emplace(id, type, 0);
        if (!inserted && entry->second.first != type)
            throw std::runtime_error("Conflicting shared spell-list actor kinds");
        ++entry->second.second;
    }

    bool SharedActorInitialization::initialInventoryReadsPlayer(
        const ESM::ObjectState& owner, const ESMStore& incoming) const
    {
        if (!owner.mHasCustomState)
            return false;
        const ESM::InventoryList* inventory = nullptr;
        if (const auto* npc = dynamic_cast<const ESM::NpcState*>(&owner);
            npc && npc->mCreatureStats.mMissingACDT)
        {
            const auto* base = mContent.searchForRestore<ESM::NPC>(owner.mRef.mRefID, incoming);
            if (!base) throw std::runtime_error("Prepared initial NPC inventory base is unavailable");
            inventory = &base->mInventory;
        }
        else if (const auto* creature = dynamic_cast<const ESM::CreatureState*>(&owner);
            creature && creature->mCreatureStats.mMissingACDT)
        {
            const auto* base = mContent.searchForRestore<ESM::Creature>(owner.mRef.mRefID, incoming);
            if (!base) throw std::runtime_error("Prepared initial creature inventory base is unavailable");
            inventory = &base->mInventory;
        }
        if (!inventory) return false;
        // addInitialItem skips zero counts and missing definitions. An initial
        // LEVI always queries Player before chance-none/candidate selection;
        // LEVC is not dispatched as an initial item list by ContainerStore.
        for (const auto& item : inventory->mList)
            if (item.mCount != 0 && mContent.findForRestore(item.mItem, incoming) == ESM::REC_LEVI)
                return true;
        return false;
    }

    std::unique_ptr<CustomData> SharedActorInitialization::preparePlayer(
        const ESMStore& incoming, const ESM::NPC* canonicalPlayer, bool spellsInitialized) const
    {
        const auto id = ESM::RefId::stringRefId("Player");
        const auto* base = canonicalPlayer ? canonicalPlayer : mContent.searchForRestore<ESM::NPC>(id, incoming);
        if (!base || base->mId != id)
            throw std::runtime_error("Prepared implicit Player base is unavailable");
        return MWClass::Npc::prepareLegacyCustomData(*base, mContent, incoming, true, spellsInitialized);
    }

    void SharedActorInitialization::prepare(
        const ESM::ObjectState& owner, const ESMStore& incoming, const ESM::NPC* canonicalPlayer)
    {
        if (mInstalled) throw std::logic_error("Shared actor initialization was already installed");
        const bool player = owner.mRef.mRefID == "Player";
        const int type = dynamic_cast<const ESM::NpcState*>(&owner) ? int(ESM::REC_NPC_)
            : dynamic_cast<const ESM::CreatureState*>(&owner) ? int(ESM::REC_CREA) : 0;
        if (type) requestSpells(owner.mRef.mRefID, type);
        // Player::readRecord forces a stats query even without custom state.
        const bool spellsInitialized = type && (owner.mHasCustomState || player)
            && !mSpellBindings.insert(owner.mRef.mRefID).second;
        auto data = mModel.prepareActorCustomData(owner, incoming,
            player ? canonicalPlayer : nullptr, spellsInitialized);
        if (data && !mActors.emplace(WorldModel::preparedInventoryKey(owner), std::move(data)).second)
            throw std::runtime_error("Duplicate shared actor custom-data owner");
        if (player)
        {
            mPlayerRecordSeen = true;
            // PLAY replaces RefData, discarding any early implicit instance.
            // A no-custom-state PLAY then initializes another base instance.
            if (!owner.mHasCustomState)
                mAfterPlayer = preparePlayer(incoming, canonicalPlayer, spellsInitialized);
        }
        else if (!mPlayerRecordSeen && !mBeforePlayer && initialInventoryReadsPlayer(owner, incoming))
        {
            const auto id = ESM::RefId::stringRefId("Player");
            const bool initialized = !mSpellBindings.insert(id).second;
            requestSpells(id, ESM::REC_NPC_);
            // This event occurs after the current owner's spell binding, but
            // before later owners and PLAY, just as the real inventory fill.
            mBeforePlayer = preparePlayer(incoming, canonicalPlayer, initialized);
        }
    }

    void SharedActorInitialization::install(const Ptr& player)
    {
        if (mInstalled) throw std::logic_error("Shared actor initialization was already installed");
        mModel.setPreparedActorCustomData(std::move(mActors), std::move(mBeforePlayer), std::move(mAfterPlayer), player);
        mInstalled = true;
    }
}
