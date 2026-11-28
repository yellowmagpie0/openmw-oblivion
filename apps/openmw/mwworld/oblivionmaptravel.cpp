#include "oblivionmaptravel.hpp"

#include <algorithm>
#include <set>

#include <components/debug/debuglog.hpp>
#include <components/esm/util.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadwrld.hpp>
#include <components/esm4/runtimereferences.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/oblivionai.hpp"

#include "actionteleport.hpp"
#include "cellstore.hpp"
#include "class.hpp"
#include "datetimemanager.hpp"
#include "oblivionscriptmanager.hpp"
#include "player.hpp"
#include "worldimp.hpp"
#include "worldspaceutils.hpp"

namespace MWWorld
{
    std::uint8_t oblivionMapMarkerFlags(const ESM4::Reference& marker, const MapMarkerReveals& reveals)
    {
        const auto it = reveals.find(marker.mFormKey);
        return it == reveals.end() ? marker.mMapMarkerFlags : marker.mMapMarkerFlags | it->second;
    }

    std::string_view travelToOblivionMapMarker(World& world, const ESM4::Reference& marker)
    {
        const auto& store = world.getStore();
        const auto* scripts = world.getOblivionScriptManager();
        const auto flags = scripts ? oblivionMapMarkerFlags(marker, scripts->revealedMapMarkers()) : marker.mMapMarkerFlags;
        if (!marker.mIsMapMarker || (flags & 3) != 3
            || (marker.mFlags & ESM4::Rec_Deleted)
            || !world.getOblivionReferenceEnabled(marker.mFormKey).value_or(!(marker.mFlags & ESM4::Rec_Disabled)))
            return "You have not discovered this location yet.";
        const auto player = world.getPlayerPtr();
        const auto* cell = store.get<ESM4::Cell>().search(marker.mParent);
        const auto& worlds = store.get<ESM4::World>();
        if (!player.isInCell() || !player.getCell()->isExterior())
            return "You cannot fast travel from this location.";
        const auto* source = worlds.search(player.getCell()->getCell()->getWorldSpace());
        const auto* destination = cell ? worlds.search(cell->mParent) : nullptr;
        if (!world.isTeleportingEnabled() || !source || !destination
            || ((source->mWorldFlags | destination->mWorldFlags) & ESM4::World::WLD_NoFastTravel)
            || MWWorld::resolveWorldspaceInheritance(worlds, ESM::RefId(source->mId), ESM4::World::UseFlag_Map)
                != MWWorld::resolveWorldspaceInheritance(worlds, cell->mParent, ESM4::World::UseFlag_Map))
            return "You cannot fast travel from this location.";
        if (world.getPlayer().enemiesNearby())
            return "You cannot fast travel with enemies nearby.";
        if (player.getClass().getEncumbrance(player) > player.getClass().getCapacity(player))
            return "You cannot fast travel while over-encumbered.";

        const auto pos = player.getRefData().getPosition().asVec3();
        // Estimate elapsed game time from travel distance and the player's movement speed.
        const float distance = (pos - marker.mPos.asVec3()).length();
        const double hours = distance / std::max(1.f, player.getClass().getMaxSpeed(player))
            * world.getTimeManager()->getGameTimeScale() / 3600.0;
        // Persistent map markers share their world's dummy cell, irrespective of their coordinates.
        const auto location = ESM::positionToExteriorCellLocation(marker.mPos.pos[0], marker.mPos.pos[1], cell->mParent);
        auto& arrival = world.getWorldModel().getExterior(location);
        teleportOblivionParty(world, arrival.getCell()->getId(), marker.mPos);
        world.advanceTime(hours);
        Log(Debug::Info) << "Fast travelled to " << marker.mFullName << " (" << hours << " hours)";
        return {};
    }

    void teleportOblivionParty(World& world, const ESM::RefId& cellId, const ESM::Position& position)
    {
        const auto player = world.getPlayerPtr();
        auto* ai = world.getOblivionAiService();
        // Resolve the whole party before moving its leaders out of the source
        // scene. Follow distance is measured to the actor each package follows.
        using PartyMember = std::pair<ESM::FormKey, MWWorld::Ptr>;
        std::vector<PartyMember> party{ { ESM::FormKey::dynamic("player", 1), player } };
        std::set<ESM::FormKey> visited{ party.front().first };
        for (std::size_t i = 0; ai && i < party.size(); ++i)
        {
            const auto [leaderKey, leader] = party[i];
            std::vector<MWWorld::Ptr> nearby;
            MWBase::Environment::get().getMechanicsManager()->getActorsInRange(
                leader.getRefData().getPosition().asVec3(), 800.f, nearby);
            for (const auto& actor : nearby)
            {
                const auto* state = ai->state(actor);
                if (actor.isInCell() && (actor.getCell() == player.getCell()
                    || (player.getCell()->isExterior() && actor.getCell()->isExterior()
                        && actor.getCell()->getCell()->getWorldSpace() == player.getCell()->getCell()->getWorldSpace()))
                    && state && (state->mPackageType == ESM4::AIPackageType::Follow
                        || state->mPackageType == ESM4::AIPackageType::Accompany)
                    && ESM4::runtimeReferenceKey(state->mTarget) == leaderKey
                    && !ai->isPackageWaiting(state->mPackage) && !actor.getClass().getCreatureStats(actor).isDead()
                    && visited.insert(state->mActor).second)
                    party.emplace_back(state->mActor, actor);
            }
        }
        ActionTeleport travel(cellId, position, false);
        for (std::size_t i = 1; i < party.size(); ++i)
            travel.execute(party[i].second);
        travel.execute(player);
        // The NPC was moved while the destination scene was inactive. Publish its new
        // resident identity before scripts or an immediate save read the AI snapshot.
        for (std::size_t i = 1; i < party.size(); ++i)
        {
            auto actor = ai->resolveReference(party[i].first);
            if (actor.isEmpty())
                continue;
            const auto feet = actor.getRefData().getPosition().asVec3();
            if (const auto clear = world.findClearDoorArrival(actor, feet); clear && *clear != feet)
                actor = world.moveObject(actor, *clear);
            ai->evaluatePackage(actor);
        }
    }
}
