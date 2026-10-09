#ifndef GAME_MWMECHANICS_NPCSTATS_H
#define GAME_MWMECHANICS_NPCSTATS_H

#include "creaturestats.hpp"
#include <components/esm/refid.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadskil.hpp>
#include <map>
#include <memory>
#include <components/esm3/npcstats.hpp>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace ESM
{
    struct Class;
    struct NpcStats;
}

namespace MWWorld { class ESMStore; }

namespace MWMechanics
{
    class PreparedNpcStats;
    /// \brief Additional stats for NPCs

    class NpcStats : public CreatureStats
    {
        friend class OblivionActorProjection;
        friend class PreparedNpcStats;
        int mDisposition;
        int mCrimeDispositionModifier;
        std::map<ESM::RefId, SkillValue> mSkills; // SkillValue.mProgress used by the player only

        int mReputation;
        std::optional<int> mNativeReputation;
        int mCrimeId;

        // ----- used by the player only, maybe should be moved at some point -------
        int mBounty;
        // Derived integer compatibility view, never the native float authority.
        std::optional<int> mNativeBounty;
        int mWerewolfKills;
        /// Used only for the player and for NPC's with ranks, modified by scripts; other NPCs have maximum one faction
        /// defined in their NPC record
        std::map<ESM::RefId, int> mFactionRank;
        std::set<ESM::RefId> mExpelled;
        std::map<ESM::RefId, int> mFactionReputation;
        int mLevelProgress; // 0-10
        std::map<ESM::RefId, int>
            mSkillIncreases; // number of skill increases for each attribute (resets after leveling up)
        std::vector<int> mSpecIncreases; // number of skill increases for each specialization (accumulates throughout
                                         // the entire game)
        std::set<ESM::RefId> mUsedIds;
        // ---------------------------------------------------------------------------

        /// Countdown to getting damage while underwater
        float mTimeToStartDrowning;

        bool mIsWerewolf;

    public:
        NpcStats();
        explicit NpcStats(const MWWorld::ESMStore* initializationStore);

        static std::unique_ptr<PreparedNpcStats> prepareReadState(const ESM::NpcStats& state,
            const MWWorld::ESMStore& content, const MWWorld::ESMStore* incoming = nullptr);

        int getBaseDisposition() const;
        void setBaseDisposition(int disposition);

        int getCrimeDispositionModifier() const;
        void setCrimeDispositionModifier(int value);
        void modCrimeDispositionModifier(int value);

        int getReputation() const;
        void setReputation(int reputation);

        int getCrimeId() const;
        void setCrimeId(int id);

        const SkillValue& getSkill(ESM::RefId id) const;
        SkillValue& getSkill(ESM::RefId id);
        void setSkill(ESM::RefId id, const SkillValue& value);

        int getFactionRank(const ESM::RefId& faction) const;
        const std::map<ESM::RefId, int>& getFactionRanks() const;

        /// Join this faction, setting the initial rank to 0.
        void joinFaction(const ESM::RefId& faction);
        /// Sets the rank in this faction to a specified value, if such a rank exists.
        void setFactionRank(const ESM::RefId& faction, int value);

        const std::set<ESM::RefId>& getExpelled() const { return mExpelled; }
        bool getExpelled(const ESM::RefId& factionID) const;
        void expell(const ESM::RefId& factionID, bool printMessage);
        void clearExpelled(const ESM::RefId& factionID);

        bool isInFaction(const ESM::RefId& faction) const;

        float getSkillProgressRequirement(ESM::RefId id, const ESM::Class& npcClass) const;

        int getLevelProgress() const;
        void setLevelProgress(int progress);

        int getLevelupAttributeMultiplier(ESM::Attribute::AttributeID attribute) const;
        int getSkillIncreasesForAttribute(ESM::Attribute::AttributeID attribute) const;
        void setSkillIncreasesForAttribute(ESM::Attribute::AttributeID, int increases);

        int getSkillIncreasesForSpecialization(ESM::Class::Specialization spec) const;
        void setSkillIncreasesForSpecialization(ESM::Class::Specialization spec, int increases);

        void levelUp();

        void updateHealth();
        ///< Calculate health based on endurance and strength.
        ///  Called at character creation.

        void flagAsUsed(const ESM::RefId& id);
        ///< @note Id must be lower-case

        bool hasBeenUsed(const ESM::RefId& id) const;
        ///< @note Id must be lower-case

        int getBounty() const;

        void setBounty(int bounty);

        int getFactionReputation(const ESM::RefId& faction) const;

        void setFactionReputation(const ESM::RefId& faction, int value);

        bool hasSkillsForRank(const ESM::RefId& factionId, int rank) const;

        bool isWerewolf() const;

        void setWerewolf(bool set);

        int getWerewolfKills() const;

        /// Increments mWerewolfKills by 1.
        void addWerewolfKill();

        float getTimeToStartDrowning() const;
        /// Sets time left for the creature to drown if it stays underwater.
        /// @param time value from [0,20]
        void setTimeToStartDrowning(float time);

        void writeState(ESM::CreatureStats& state) const;
        void writeState(ESM::NpcStats& state) const;

        void readState(const ESM::CreatureStats& state, PreparedCreatureStats* prepared = nullptr);
        void readState(const ESM::NpcStats& state);

        const std::map<ESM::RefId, SkillValue>& getSkills() const { return mSkills; }
    };
    // Owns only the additional NPC fields; publication preserves CreatureStats
    // and fields omitted by old saves, matching ordinary readState overlays.
    class PreparedNpcStats
    {
        friend class NpcStats;
        ESM::NpcStats mState;
        std::map<ESM::RefId, SkillValue> mSkills;
        std::map<ESM::RefId, int> mFactionRank;
        std::set<ESM::RefId> mExpelled;
        std::map<ESM::RefId, int> mFactionReputation;
        std::set<ESM::RefId> mUsedIds;
        std::vector<int> mSpecIncreases;
        bool mConsumed = false;

    public:
        PreparedNpcStats(const PreparedNpcStats&) = delete;
        PreparedNpcStats& operator=(const PreparedNpcStats&) = delete;
        void install(NpcStats& target);

    private:
        PreparedNpcStats() = default;
    };

}

#endif
