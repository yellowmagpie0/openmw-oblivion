#ifndef OPENMW_MWWORLD_SHAREDACTORINITIALIZATION_H
#define OPENMW_MWWORLD_SHAREDACTORINITIALIZATION_H

#include <set>

#include "esmstore.hpp"
#include "worldmodel.hpp"

namespace MWWorld
{
    // One admission's ordered class-initialization predictions. Incoming
    // definitions must remain alive through installation and actor binding.
    // Discard the entire plan if any prepare call fails.
    class SharedActorInitialization
    {
        WorldModel& mModel;
        const ESMStore& mContent;
        WorldModel::PreparedActorCustomData mActors;
        ESMStore::SpellListRequests mSpellLists;
        std::set<ESM::RefId> mSpellBindings;
        std::unique_ptr<CustomData> mBeforePlayer;
        std::unique_ptr<CustomData> mAfterPlayer;
        bool mPlayerRecordSeen = false;
        bool mInstalled = false;

        void requestSpells(const ESM::RefId& id, int type);
        bool initialInventoryReadsPlayer(const ESM::ObjectState& owner, const ESMStore& incoming) const;
        std::unique_ptr<CustomData> preparePlayer(
            const ESMStore& incoming, const ESM::NPC* canonicalPlayer, bool spellsInitialized) const;

    public:
        SharedActorInitialization(WorldModel& model, const ESMStore& content);
        ~SharedActorInitialization();
        SharedActorInitialization(const SharedActorInitialization&) = delete;
        SharedActorInitialization& operator=(const SharedActorInitialization&) = delete;

        void prepare(const ESM::ObjectState& owner, const ESMStore& incoming,
            const ESM::NPC* canonicalPlayer = nullptr);
        const ESMStore::SpellListRequests& spellListRequests() const { return mSpellLists; }
        void install(const Ptr& player);
    };
}

#endif
