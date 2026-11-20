#ifndef OPENMW_MWWORLD_OBLIVIONQUESTGUIDANCE_H
#define OPENMW_MWWORLD_OBLIVIONQUESTGUIDANCE_H

#include <optional>
#include <string>
#include <vector>

#include <osg/Vec3f>

#include <components/esm/formkey.hpp>
#include <components/esm/refid.hpp>

namespace MWBase
{
    class World;
}

namespace ESM4
{
    struct Quest;
    struct RuntimeQuestJournalStage;
}

namespace MWWorld
{
    class World;
    class OblivionScriptManager;
    struct QuestMarker
    {
        ESM::RefId mSpace;
        osg::Vec3f mPosition;
    };

    struct QuestDestination
    {
        std::string mLabel;
        std::optional<QuestMarker> mNearby;
        std::optional<QuestMarker> mWorldMap;
    };

    struct QuestObjective
    {
        ESM::FormKey mQuest;
        std::string mTitle;
        std::string mText;
        std::vector<QuestDestination> mDestinations;
    };

    struct OblivionJournalStep
    {
        int mStage;
        std::string mText;
    };

    struct OblivionJournalQuest
    {
        ESM::FormKey mQuest;
        std::string mTitle;
        std::vector<OblivionJournalStep> mSteps;
        bool mFinished = false;
        bool mIncompleteHistory = false;
    };

    std::vector<OblivionJournalQuest> getOblivionJournal(const MWBase::World& world);
    std::string getOblivionJournalDebug(const MWBase::World& world, const ESM::FormKey& quest);
    std::optional<QuestObjective> getOblivionQuestObjective(MWBase::World& world, const ESM::FormKey& tracked = {});
    std::optional<ESM4::RuntimeQuestJournalStage> captureOblivionJournalStage(
        World& world, const OblivionScriptManager& scripts, const ESM4::Quest& quest, int stage);
}

#endif
