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

namespace MWWorld
{
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

    std::optional<QuestObjective> getOblivionQuestObjective(MWBase::World& world);
}

#endif
