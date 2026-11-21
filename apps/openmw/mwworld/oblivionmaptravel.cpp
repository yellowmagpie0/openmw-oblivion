#include "oblivionmaptravel.hpp"

#include <algorithm>

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
#include "worldimp.hpp"
#include "worldspaceutils.hpp"

namespace MWWorld
{
    std::string_view travelToOblivionMapMarker(World& world, const ESM4::Reference& marker)
    {
        const auto& store = world.getStore();
        if (!marker.mIsMapMarker || (marker.mMapMarkerFlags & 3) != 3
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
        if (world.isOblivionInCombat(player).value_or(false))
            return "You cannot fast travel with enemies nearby.";
        if (player.getClass().getEncumbrance(player) > player.getClass().getCapacity(player))
            return "You cannot fast travel while over-encumbered.";

        const auto pos = player.getRefData().getPosition().asVec3();
        // Estimate elapsed game time from travel distance and the player's movement speed.
        const float distance = (pos - marker.mPos.asVec3()).length();
        const double hours = distance / std::max(1.f, player.getClass().getMaxSpeed(player))
            * world.getTimeManager()->getGameTimeScale() / 3600.0;
        // Move nearby active native followers while their source cell is still resident.
        std::vector<MWWorld::Ptr> nearby;
        MWBase::Environment::get().getMechanicsManager()->getActorsInRange(pos, 800.f, nearby);
        const auto* ai = world.getOblivionAiService();
        // Persistent map markers share their world's dummy cell, irrespective of their coordinates.
        const auto location = ESM::positionToExteriorCellLocation(marker.mPos.pos[0], marker.mPos.pos[1], cell->mParent);
        auto& arrival = world.getWorldModel().getExterior(location);
        MWWorld::ActionTeleport travel(arrival.getCell()->getId(), marker.mPos, false);
        for (const auto& actor : nearby)
        {
            const auto* state = ai ? ai->state(actor) : nullptr;
            if (actor.isInCell() && actor.getCell()->isExterior()
                && actor.getCell()->getCell()->getWorldSpace() == player.getCell()->getCell()->getWorldSpace() && state && state->mPackageType == ESM4::AIPackageType::Follow
                && ESM4::runtimeReferenceKey(state->mTarget) == ESM::FormKey::dynamic("player", 1)
                && !ai->isPackageWaiting(state->mPackage) && !actor.getClass().getCreatureStats(actor).isDead())
                travel.execute(actor);
        }
        travel.execute(player);
        world.advanceTime(hours);
        Log(Debug::Info) << "Fast travelled to " << marker.mFullName << " (" << hours << " hours)";
        return {};
    }
}
