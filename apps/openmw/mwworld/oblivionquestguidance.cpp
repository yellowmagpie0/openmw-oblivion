#include "oblivionquestguidance.hpp"
#include "oblivionactorstats.hpp"

#include <algorithm>
#include <deque>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <tuple>

#include <components/esm4/aiconditions.hpp>
#include <components/esm4/crimerules.hpp>
#include <components/esm4/loadacti.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadqust.hpp>
#include <components/esm/util.hpp>
#include <components/esm4/loadscpt.hpp>
#include <components/esm4/loadwrld.hpp>
#include <components/esm4/runtimereferences.hpp>

#include "../mwbase/environment.hpp"
#include "../mwmechanics/oblivionai.hpp"

#include "class.hpp"
#include "oblivionscriptmanager.hpp"
#include "obliviontrespass.hpp"
#include "worldimp.hpp"
#include "worldspaceutils.hpp"

namespace
{
    ESM::RefId mapSpace(const ESM4::Cell& cell)
    {
        return cell.isExterior() ? cell.mParent : ESM::RefId(cell.mId);
    }
}

namespace MWMechanics
{
    std::vector<ESM::FormKey> OblivionAiService::nearbyMapMarkers(const MWWorld::Ptr& player, float radius) const
    {
        std::vector<ESM::FormKey> result;
        if (player.isEmpty() || !player.isInCell() || !player.getCell()->isExterior() || radius <= 0)
            return result;
        const auto& space = player.getCell()->getCell()->getWorldSpace();
        const auto position = player.getRefData().getPosition().asVec3();
        for (const auto index : mNamedMapMarkers)
        {
            const auto& marker = mUnloadedLocations[index];
            if ((marker.mPosition - position).length2() >= radius * radius)
                continue;
            const auto* cell = mWorld.mStore.search<ESM4::Cell>(marker.mCell);
            if (cell && cell->isExterior() && cell->mParent == space)
                result.push_back(marker.mReference);
        }
        return result;
    }

    MWWorld::QuestDestination OblivionAiService::questDestination(
        const ESM::FormKey& reference, const MWWorld::Ptr& player) const
    {
        MWWorld::QuestDestination result;
        const auto entry = mUnloadedLocationByReference.find(reference);
        if (entry == mUnloadedLocationByReference.end()
            || !mWorld.getOblivionReferenceEnabled(reference).value_or(false))
            return result;

        const auto& location = mUnloadedLocations[entry->second];
        const auto& store = mWorld.getStore();
        ESM::FormKey cellKey = location.mCell;
        osg::Vec3f position = location.mPosition;
        const auto ptr = loadedPtrFor(reference);
        if (!ptr.isEmpty() && ptr.isInCell())
        {
            if (ptr.getBase()->isDeleted())
                return result;
            const auto* cell = store.get<ESM4::Cell>().search(ptr.getCell()->getCell()->getId());
            if (!cell)
                return result;
            cellKey = cell->mFormKey;
            position = ptr.getRefData().getPosition().asVec3();
            result.mLabel = ptr.getClass().getName(ptr);
        }
        else if (const auto* actor = state(reference); actor && !actor->mLastValidCell.isNull())
        {
            cellKey = actor->mLastValidCell;
            position = actor->mLastValidPosition.asVec3();
        }
        else if (const auto* saved = savedReferenceState(reference))
        {
            if (saved->mDeleted)
                return result;
            cellKey = saved->mCell;
            position = saved->mPosition.asVec3();
        }

        const auto* targetCell = store.search<ESM4::Cell>(cellKey);
        if (!targetCell)
            return result;
        if (result.mLabel.empty())
        {
            if (const auto* npc = store.search<ESM4::Npc>(location.mBase))
                result.mLabel = MWWorld::oblivionActorName(location.mBase, npc->mFullName);
            else if (const auto* placed = store.search<ESM4::Reference>(reference))
                result.mLabel = placed->mFullName;
        }
        if (result.mLabel.empty())
            if (const auto* activator = store.search<ESM4::Activator>(location.mBase))
                result.mLabel = activator->mFullName;
        if (result.mLabel.empty())
            result.mLabel = targetCell->mFullName;
        // A quest can target an unnamed XMarker in the persistent exterior
        // cell. Give that point its nearby named landmark on the compass.
        if (result.mLabel.empty() && targetCell->isExterior())
        {
            const float radius = static_cast<float>(ESM::getCellSize(targetCell->mParent));
            float nearest = radius * radius;
            for (const auto index : mNamedMapMarkers)
            {
                const auto& landmark = mUnloadedLocations[index];
                const auto delta = landmark.mPosition - position;
                const float distance = delta.x() * delta.x() + delta.y() * delta.y();
                if (distance >= nearest)
                    continue;
                const auto* cell = store.search<ESM4::Cell>(landmark.mCell);
                if (!cell || !cell->isExterior() || cell->mParent != targetCell->mParent
                    || !mWorld.getOblivionReferenceEnabled(landmark.mReference).value_or(false))
                    continue;
                if (const auto* marker = store.search<ESM4::Reference>(landmark.mReference))
                {
                    nearest = distance;
                    result.mLabel = marker->mFullName;
                }
            }
        }

        const auto* playerCell = player.getCell()->getCell();
        const ESM::RefId playerSpace = playerCell->getWorldSpace();
        const ESM::RefId targetSpace = mapSpace(*targetCell);
        ESM::RefId preferredExterior = playerCell->isExterior() ? playerSpace : ESM::RefId{};
        const auto setExterior = [&](const MWWorld::QuestMarker& marker) {
            if (preferredExterior.empty())
                preferredExterior = MWWorld::resolveWorldspaceInheritance(
                    store.get<ESM4::World>(), marker.mSpace, ESM4::World::UseFlag_Map);
            if (!result.mWorldMap
                || (marker.mSpace == preferredExterior && result.mWorldMap->mSpace != preferredExterior))
                result.mWorldMap = marker;
        };
        const auto doorMarker = [&](const UnloadedLocation& door, ESM::RefId space) {
            auto doorPosition = door.mPosition;
            if (const auto resident = loadedPtrFor(door.mReference); !resident.isEmpty())
                doorPosition = resident.getRefData().getPosition().asVec3();
            return MWWorld::QuestMarker{ space, doorPosition };
        };
        // Both walks read one snapshot. Index it once rather than scanning all saved references for each exit.
        std::optional<OblivionSavedReferenceIndex> references;
        const auto canUseDoor = [&](const UnloadedLocation& door, const ESM4::Cell& source, const ESM4::Cell& destination) {
            if (!references && mWorld.mOblivionRuntimeState)
                references.emplace(mWorld.mOblivionRuntimeState->mReferences);
            const auto state = currentDoorState(door, nullptr, references ? &*references : nullptr);
            if (!state.mAvailable)
                return false;
            if (!state.mLocked || mWorld.oblivionPlayerHasItem(state.mKey))
                return true;
            // Evaluate exit rights in the room the route would enter. An NPC's
            // locked front door can still be used to leave their house.
            if (source.isExterior() || source.mOwner.isZeroOrUnset()
                || (!destination.isExterior() && !(destination.mCellFlags & ESM4::CELL_Public)))
                return false;
            const auto* placed = store.search<ESM4::Reference>(door.mReference);
            if (!placed)
                return false;
            int level = placed->mLockLevel;
            if (const auto resident = loadedPtrFor(door.mReference); !resident.isEmpty())
                level = resident.getCellRef().getLockLevel();
            else if (const auto* saved = references ? references->find(door.mReference) : nullptr)
                level = saved->mLockLevel;
            const auto trespass = MWWorld::oblivionActorTrespassing(mWorld, player, {}, source.mFormKey);
            return ESM4::playerHasTrespassExitExemption({ trespass.value_or(false), true, true,
                static_cast<std::uint8_t>(level), true, static_cast<std::uint8_t>(destination.mCellFlags) });
        };
        // A single interior may contain several disconnected sewer passages. Track
        // which side of each teleport we can walk to, including its XTEL landing.
        using Stop = std::pair<ESM::FormKey, std::optional<std::uint32_t>>;
        const auto stopAt = [&](const ESM4::Cell& cell, const osg::Vec3f& point) {
            Stop stop{ cell.mFormKey, std::nullopt };
            if (!cell.isExterior())
                if (const auto* grid = store.getOblivionPathgridService().graphForCell(cell.mFormKey))
                    stop.second = grid->nearestEnabledNode({ point.x(), point.y(), point.z() });
            return stop;
        };
        const auto canWalk = [&](const Stop& from, const Stop& to) {
            if (!from.second || !to.second || from.second == to.second)
                return true;
            const auto* grid = store.getOblivionPathgridService().graphForCell(from.first);
            return static_cast<bool>(grid->route(*from.second, *to.second));
        };
        const auto* origin = store.get<ESM4::Cell>().search(playerCell->getId());
        const Stop start = origin ? stopAt(*origin, player.getRefData().getPosition().asVec3()) : Stop{};
        const Stop goal = stopAt(*targetCell, position);
        if (playerSpace == targetSpace && canWalk(start, goal))
            result.mNearby = MWWorld::QuestMarker{ targetSpace, position };
        if (targetCell->isExterior())
            setExterior({ targetSpace, position });

        // Walk the existing incoming XTEL index back from the target. This
        // preserves one-way links, and does not materialize other cells.
        using MapStep = std::pair<Stop, bool>;
        const MapStep initial{ goal, !playerCell->isExterior() && targetCell->isExterior() };
        std::deque<MapStep> pending{ initial };
        std::set<MapStep> visitedSteps{ initial };
        while (!pending.empty() && !(result.mNearby && result.mWorldMap && result.mWorldMap->mSpace == preferredExterior))
        {
            const auto [current, crossedExterior] = pending.front();
            pending.pop_front();
            if (crossedExterior && result.mWorldMap && result.mWorldMap->mSpace == preferredExterior)
                continue;
            const auto incoming = mUnloadedDoorsByDestinationCell.find(current.first);
            if (incoming == mUnloadedDoorsByDestinationCell.end())
                continue;
            const auto& currentCell = *store.search<ESM4::Cell>(current.first);
            for (const auto index : incoming->second)
            {
                const auto& door = mUnloadedLocations[index];
                const auto* source = store.search<ESM4::Cell>(door.mCell);
                if (!source)
                    continue;
                const Stop departure = stopAt(*source, door.mPosition);
                const MapStep next{ departure, crossedExterior || (!playerCell->isExterior() && source->isExterior()) };
                if (visitedSteps.contains(next) || !canUseDoor(door, *source, currentCell)
                    || (current.second && !canWalk(stopAt(currentCell, door.mTeleportPosition), current)))
                    continue;

                const auto space = mapSpace(*source);
                const auto marker = doorMarker(door, space);
                if (!crossedExterior && !result.mNearby && space == playerSpace && canWalk(start, departure))
                    result.mNearby = marker;
                if (source->isExterior())
                    setExterior(marker);
                // An exterior gate can project the target onto the parent map.
                // Beyond this point an indoor player needs a separate route
                // from their own room to the outdoors.
                if (!source->isExterior() || space != playerSpace)
                {
                    visitedSteps.insert(next);
                    pending.push_back(next);
                }
            }
        }

        // Walled cities and their districts are separate exterior worldspaces.
        // Their gates can land many grid cells from the quest's map marker.
        if (playerCell->isExterior() && !result.mNearby && result.mWorldMap && result.mWorldMap->mSpace != playerSpace)
        {
            std::vector<ESM::RefId> frontier{ result.mWorldMap->mSpace };
            std::set<ESM::RefId> seen{ frontier.front() };
            const auto playerPosition = player.getRefData().getPosition().asVec3();
            while (!frontier.empty() && !result.mNearby)
            {
                std::vector<ESM::RefId> next;
                std::optional<std::pair<float, ESM::FormKey>> best;
                for (const auto& space : frontier)
                {
                    const auto gates = mExteriorGatesByDestination.find(space);
                    if (gates == mExteriorGatesByDestination.end())
                        continue;
                    for (const auto index : gates->second)
                    {
                        const auto& door = mUnloadedLocations[index];
                        const auto& landing = mUnloadedLocations[mUnloadedLocationByReference.at(door.mTeleportDoor)];
                        const auto& from = *store.search<ESM4::Cell>(door.mCell);
                        if (seen.contains(from.mParent) || !canUseDoor(door, from, *store.search<ESM4::Cell>(landing.mCell)))
                            continue;
                        if (from.mParent == playerSpace)
                        {
                            const auto marker = doorMarker(door, playerSpace);
                            const auto cost = std::pair{ (marker.mPosition - playerPosition).length2(), door.mReference };
                            if (!best || cost < *best)
                            {
                                best = cost;
                                result.mNearby = marker;
                            }
                        }
                        else if (seen.insert(from.mParent).second)
                            next.push_back(from.mParent);
                    }
                }
                frontier = std::move(next);
            }
        }

        // An interior on the far side of the map is a separate walk to the
        // exterior. Find the first usable door on that leg.
        if (playerCell->isExterior() || result.mNearby || !result.mWorldMap)
            return result;
        if (!origin)
            return result;
        // Compare equal-door routes by walking distance. A short ladder exit
        // and a trip across several long sewer corridors can have the same
        // number of load doors.
        struct ExitPath
        {
            unsigned mDoors;
            float mDistance;
            Stop mStop;
            osg::Vec3f mArrival;
            std::optional<MWWorld::QuestMarker> mFirst;
        };
        const auto longer = [](const ExitPath& left, const ExitPath& right) {
            return std::tie(left.mDoors, left.mDistance, left.mStop)
                > std::tie(right.mDoors, right.mDistance, right.mStop);
        };
        std::priority_queue<ExitPath, std::vector<ExitPath>, decltype(longer)> exits(longer);
        exits.push({ 0, 0.f, start, player.getRefData().getPosition().asVec3(), std::nullopt });
        std::set<Stop> visited;
        const auto& worlds = store.get<ESM4::World>();
        const auto targetMap = MWWorld::resolveWorldspaceInheritance(worlds, result.mWorldMap->mSpace, ESM4::World::UseFlag_Map);
        while (!exits.empty())
        {
            const auto path = exits.top();
            exits.pop();
            if (!visited.insert(path.mStop).second)
                continue;
            const auto& currentCell = *store.search<ESM4::Cell>(path.mStop.first);
            if (currentCell.isExterior())
            {
                result.mNearby = path.mFirst;
                return result;
            }
            const auto outgoing = mUnloadedDoorsByCell.find(path.mStop.first);
            if (outgoing == mUnloadedDoorsByCell.end())
                continue;
            for (const auto index : outgoing->second)
            {
                const auto& door = mUnloadedLocations[index];
                const auto landing = mUnloadedLocationByReference.find(door.mTeleportDoor);
                if (landing == mUnloadedLocationByReference.end())
                    continue;
                const auto* destination = store.search<ESM4::Cell>(mUnloadedLocations[landing->second].mCell);
                if (!destination || (destination->isExterior()
                    && MWWorld::resolveWorldspaceInheritance(worlds, mapSpace(*destination), ESM4::World::UseFlag_Map) != targetMap))
                    continue;
                const Stop next = stopAt(*destination, door.mTeleportPosition);
                if (visited.contains(next) || !canUseDoor(door, currentCell, *destination)
                    || !canWalk(path.mStop, stopAt(currentCell, door.mPosition)))
                    continue;
                const auto first = path.mFirst ? *path.mFirst : doorMarker(door, playerSpace);
                const float distance = path.mDistance + (door.mPosition - path.mArrival).length();
                exits.push({ path.mDoors + 1, distance, next, door.mTeleportPosition, first });
            }
        }
        return result;
    }
}

namespace MWWorld
{
    namespace
    {
        const World* getNativeWorld(const MWBase::World& world)
        {
            return world.getGameProfile() == ESM::GameProfile::Oblivion ? dynamic_cast<const World*>(&world) : nullptr;
        }

        std::optional<double> questConditionGlobal(const MWBase::World& world, const ESM::FormKey& key)
        {
            const auto& store = world.getStore();
            if (const auto* global = store.search<ESM4::GlobalVariable>(key))
            {
                if (Misc::StringUtils::ciEqual(global->mEditorId, "GameHour"))
                    return world.getTimeStamp().getHour();
                return world.getGlobalFloat(GlobalVariableName(global->mEditorId));
            }
            return std::nullopt;
        }

        ESM4::NativeConditionEvaluationContext questConditionContext(
            const MWBase::World& world, const OblivionScriptManager& scripts)
        {
            ESM4::NativeConditionEvaluationContext context;
            const auto player = world.getPlayerConstPtr();
            context.mHasActor = !player.isEmpty() && player.isInCell();
            context.mResolveGlobal = [&world](const ESM::FormKey& key) { return questConditionGlobal(world, key); };
            context.mResolve = [&world, &scripts, player](
                                   const ESM4::PackageCondition& condition, ESM4::ConditionSubject subject, bool) {
                using Value = ESM4::ConditionValue;
                const auto& key = condition.mParameter1.mReferenceKey;
                switch (condition.mFunction)
                {
                    case 56: // GetQuestRunning
                    case 58: // GetStage
                    case 59: // GetStageDone
                    {
                        const auto* state = scripts.findQuestState(key);
                        if (condition.mFunction == 56)
                            return Value::value(state && state->mRunning);
                        if (condition.mFunction == 58)
                            return Value::value(state ? state->mStage : 0);
                        return Value::value(state
                            && std::ranges::binary_search(state->mCompletedStages, condition.mParameter2.mNumber));
                    }
                    case 74: // GetGlobalValue
                        if (const auto value = questConditionGlobal(world, key))
                            return Value::value(*value);
                        return Value::missing();
                    case 53: // GetScriptVariable
                    case 79: // GetQuestVariable
                        if (const auto value = condition.mFunction == 53
                                ? scripts.scriptVariable(key, condition.mParameter2.mNumber)
                                : scripts.questVariable(key, condition.mParameter2.mNumber))
                            return Value::value(*value);
                        return Value::missing();
                    case 67: // GetInCell
                        if (subject == ESM4::ConditionSubject::Self && !player.isEmpty() && player.isInCell())
                            if (const auto* cell = world.getStore().search<ESM4::Cell>(key))
                                return Value::value(player.getCell()->getCell()->getId() == ESM::RefId(cell->mId));
                        return Value::missing();
                    case ESM4::FUN_GetInCellParam:
                    {
                        if (subject != ESM4::ConditionSubject::Self || player.isEmpty() || !player.isInCell())
                            return Value::missing();
                        const auto* cell = world.getStore().search<ESM4::Cell>(key);
                        if (!cell)
                            return Value::missing();
                        if (player.getCell()->getCell()->getId() != cell->mId)
                            return Value::value(false);

                        const auto actor = ESM4::runtimeReferenceKey(condition.mParameter2.mReferenceKey);
                        if (actor == ESM::FormKey::dynamic("player", 1))
                            return Value::value(true);
                        if (const auto actorId = ESM::FormKeyResolver(world.getContentFiles()).toFormId(actor))
                        {
                            const auto ptr = MWBase::Environment::get().getWorldModel()->getPtr(*actorId);
                            if (!ptr.isEmpty() && ptr.isInCell())
                                return Value::value(ptr.getCell()->getCell()->getId() == cell->mId);
                        }
                        if (const auto* native = getNativeWorld(world))
                            if (const auto* ai = native->getOblivionAiService())
                                if (const auto* state = ai->state(actor))
                                {
                                    const auto& actorCell
                                        = state->mLastValidCell.isNull() ? state->mCell : state->mLastValidCell;
                                    if (!actorCell.isNull())
                                        return Value::value(actorCell == key);
                                }
                        return Value::missing();
                    }
                    default:
                        return Value::unsupported();
                }
            };
            return context;
        }

        bool reached(const ESM4::RuntimeQuestState& state, int stage)
        {
            return std::ranges::binary_search(state.mCompletedStages, stage);
        }

        void appendNote(OblivionJournalQuest& row, const ESM4::Quest::JournalEntry& entry)
        {
            row.mFinished |= (entry.mFlags & ESM4::Quest::JournalEntry::CompleteQuest) != 0;
            if (!entry.mText.empty() && std::ranges::none_of(row.mSteps, [&](const auto& step) {
                    return step.mStage == entry.mStage && step.mText == entry.mText;
                }))
                row.mSteps.push_back({ entry.mStage, entry.mText });
        }

        OblivionJournalQuest projectQuest(const ESM4::Quest& quest, const ESM4::RuntimeQuestState& state,
            const ESM4::NativeConditionEvaluationContext& context)
        {
            OblivionJournalQuest row{ quest.mFormKey, quest.mQuestName, {} };
            std::map<int, std::vector<const ESM4::Quest::JournalEntry*>> byStage;
            for (const auto& entry : quest.mJournalEntries)
                if (reached(state, entry.mStage))
                    byStage[entry.mStage].push_back(&entry);

            std::set<int> captured;
            for (const auto& update : state.mJournal)
                captured.insert(update.mStage);
            // Stages recorded before the journal existed precede new events.
            // Historical conditions may have changed since SetStage.
            for (const auto& [stage, entries] : byStage)
            {
                if (captured.contains(stage))
                    continue;
                for (const auto* entry : entries)
                {
                    if (entry->mConditions.empty())
                    {
                        appendNote(row, *entry);
                        continue;
                    }
                    row.mIncompleteHistory
                        |= !entry->mText.empty() || (entry->mFlags & ESM4::Quest::JournalEntry::CompleteQuest) != 0;
                    if (entry->mFlags & ESM4::Quest::JournalEntry::CompleteQuest)
                        row.mFinished |= ESM4::evaluateNativeConditions(entry->mConditions, context)
                            == ESM4::ConditionResult::True;
                }
            }
            for (const auto& update : state.mJournal)
            {
                row.mIncompleteHistory |= update.mUnresolved;
                const auto entries = byStage.find(update.mStage);
                for (const auto ordinal : update.mEntries)
                {
                    if (entries == byStage.end() || ordinal >= entries->second.size())
                        row.mIncompleteHistory = true;
                    else
                        appendNote(row, *entries->second[ordinal]);
                }
            }
            return row;
        }

        std::string conditionParameter(const ESM4::ConditionParameter& parameter)
        {
            return parameter.mIsFormId ? parameter.mReferenceKey.serialize() : std::to_string(parameter.mNumber);
        }

        void describeConditions(std::ostringstream& out, std::span<const ESM4::PackageCondition> conditions,
            const ESM4::NativeConditionEvaluationContext& context)
        {
            for (const auto& condition : conditions)
            {
                out << "    " << ESM4::conditionFunctionName(condition.mFunction) << '('
                    << conditionParameter(condition.mParameter1) << ", " << conditionParameter(condition.mParameter2)
                    << ") " << ESM4::conditionOperatorName(condition.mOperator) << ' ';
                if (condition.mUseGlobal)
                    out << condition.mComparisonGlobalKey.serialize();
                else
                    out << condition.mComparisonValue;
                if (condition.mRunOnTarget)
                    out << " [target]";
                switch (condition.mFunction)
                {
                    case 53:
                    case 56:
                    case 58:
                    case 59:
                    case 74:
                    case 79:
                    case ESM4::FUN_GetInCellParam:
                    {
                        auto standalone = condition;
                        standalone.mOr = false;
                        const auto result = ESM4::evaluateNativeConditions({ &standalone, 1 }, context);
                        if (result == ESM4::ConditionResult::True)
                            out << " [met now]";
                        else if (result == ESM4::ConditionResult::False)
                            out << " [unmet now]";
                        break;
                    }
                }
                out << (condition.mOr ? " OR\n" : "\n");
            }
        }

        const ESM4::ScriptDefinition* findResult(
            std::span<const ESM4::ScriptDefinition> definitions, std::uint32_t ordinal)
        {
            for (const auto& definition : definitions)
                if (definition.hasPayload() && ordinal-- == 0)
                    return &definition;
            return nullptr;
        }

        void describeScriptGuards(
            std::ostringstream& out, const ESM4::ScriptDefinition* definition, std::span<const std::uint32_t> guards)
        {
            if (!definition || guards.empty())
                return;
            std::istringstream source(definition->scriptSource);
            std::string line;
            std::uint32_t number = 0;
            for (const auto guard : guards)
            {
                while (number < guard && std::getline(source, line))
                    ++number;
                if (number != guard)
                    break;
                const auto first = line.find_first_not_of(" \t");
                const auto last = line.find_last_not_of(" \t\r");
                if (first != std::string::npos && last != std::string::npos && first <= last)
                    out << "    Script guard: " << line.substr(first, last - first + 1) << '\n';
            }
        }
    }

    bool isOblivionQuestFinished(const World& world, const OblivionScriptManager& scripts,
        const ESM4::Quest& quest, const ESM4::RuntimeQuestState& state)
    {
        return projectQuest(quest, state, questConditionContext(world, scripts)).mFinished;
    }

    std::optional<ESM4::RuntimeQuestJournalStage> evaluateOblivionStageEntries(World& world,
        const OblivionScriptManager& scripts, const ESM4::Quest& quest, int stage, std::vector<std::uint32_t>& eligible)
    {
        const auto context = questConditionContext(world, scripts);
        const auto* ai = world.getOblivionAiService();
        const auto player = world.getPlayerPtr();
        ESM4::RuntimeQuestJournalStage update;
        update.mStage = stage;
        std::uint32_t ordinal = 0;
        bool hasJournalEntry = false;
        for (const auto& entry : quest.mJournalEntries)
        {
            if (entry.mStage != stage)
                continue;
            const auto index = ordinal++;
            const bool journal = !quest.mQuestName.empty()
                && (!entry.mText.empty() || (entry.mFlags & ESM4::Quest::JournalEntry::CompleteQuest));
            hasJournalEntry |= journal;
            auto result = ESM4::evaluateNativeConditions(entry.mConditions, context);
            if (result != ESM4::ConditionResult::True && result != ESM4::ConditionResult::False && ai)
                result = ai->evaluateNativeIdleConditions(player, entry.mConditions, {});
            if (result == ESM4::ConditionResult::True)
            {
                eligible.push_back(index);
                if (journal)
                    update.mEntries.push_back(index);
            }
            else if (journal && result != ESM4::ConditionResult::False)
                update.mUnresolved = true;
        }
        if (!hasJournalEntry)
            return std::nullopt;
        return update;
    }

    std::vector<OblivionJournalQuest> getOblivionJournal(const MWBase::World& world)
    {
        std::vector<OblivionJournalQuest> result;
        const auto* native = getNativeWorld(world);
        const auto* scripts = native ? native->getOblivionScriptManager() : nullptr;
        if (!scripts)
            return result;

        const auto context = questConditionContext(world, *scripts);
        for (const auto& quest : world.getStore().get<ESM4::Quest>())
        {
            const auto* state = scripts->findQuestState(quest.mFormKey);
            if (!state || quest.mQuestName.empty() || state->mCompletedStages.empty())
                continue;

            auto row = projectQuest(quest, *state, context);
            if (row.mSteps.empty() && !row.mIncompleteHistory && !row.mFinished && quest.mTargets.empty())
                continue;
            result.push_back(std::move(row));
        }
        std::ranges::sort(result,
            [](const auto& a, const auto& b) { return std::tie(a.mTitle, a.mQuest) < std::tie(b.mTitle, b.mQuest); });
        return result;
    }

    std::string getOblivionJournalDebug(const MWBase::World& world, const ESM::FormKey& key)
    {
        const auto* native = getNativeWorld(world);
        const auto* scripts = native ? native->getOblivionScriptManager() : nullptr;
        const auto& store = world.getStore();
        const auto* quest = scripts ? store.search<ESM4::Quest>(key) : nullptr;
        const auto* state = quest ? scripts->findQuestState(key) : nullptr;
        if (!state)
            return {};

        const auto context = questConditionContext(world, *scripts);
        std::ostringstream out;
        out << "DEBUG — quest spoilers\n"
            << quest->mEditorId << " (" << key.serialize() << ")\nCurrent stage " << state->mStage << " | Quest script "
            << (state->mRunning ? "running" : "stopped") << "\nReached stages";
        for (const int stage : state->mCompletedStages)
            out << ' ' << stage;
        out << "\n\nPossible progression sources\n"
               "Static calls can be conditional. Indirect calls may be unresolved.\n";
        bool foundSource = false;
        for (const auto& source : scripts->findQuestStageSources(key))
        {
            if (source.mStage && reached(*state, *source.mStage))
                continue;
            foundSource = true;
            if (source.mStage)
                out << "Stage " << *source.mStage;
            else
                out << "Dynamic stage";
            out << " — " << ObScript::toString(source.mUnit.mContext) << ' ' << source.mUnit.mOwner.serialize();
            if (source.mUnit.mStage)
                out << " stage " << *source.mUnit.mStage;
            if (source.mUnit.mStageEntry)
                out << " entry " << *source.mUnit.mStageEntry;
            out << " / " << source.mEvent << " / line " << source.mLine << '\n';
            const ESM4::ScriptDefinition* definition = nullptr;
            if (source.mUnit.mContext == ObScript::ExecutionContext::DialogueResult)
            {
                if (const auto* info = store.search<ESM4::DialogInfo>(source.mUnit.mOwner))
                {
                    if (!info->mResponses.empty())
                        out << "    “" << info->mResponses.front().mText << "”\n";
                    describeConditions(out, info->mCanonicalConditions, context);
                    definition = findResult(info->mResultScripts, source.mUnit.mOrdinal);
                }
            }
            else if (source.mUnit.mContext == ObScript::ExecutionContext::QuestResult)
            {
                if (const auto* owner = store.search<ESM4::Quest>(source.mUnit.mOwner))
                    definition = findResult(owner->mResultScripts, source.mUnit.mOrdinal);
            }
            else if (const auto* script = store.search<ESM4::Script>(source.mUnit.mOwner))
                definition = &script->mScript;
            describeScriptGuards(out, definition, source.mGuardLines);
        }
        if (!foundSource)
            out << "Progression source unresolved.\n";
        out << "\nUnreached authored stages\n";
        bool foundStage = false;
        for (const auto& entry : quest->mJournalEntries)
        {
            if (reached(*state, entry.mStage))
                continue;
            foundStage = true;
            out << "Stage " << entry.mStage;
            if (entry.mFlags & ESM4::Quest::JournalEntry::CompleteQuest)
                out << " [completion]";
            if (!entry.mText.empty())
                out << " — " << entry.mText;
            out << '\n';
            describeConditions(out, entry.mConditions, context);
        }
        if (!foundStage)
            out << "None.\n";
        return out.str();
    }

    std::optional<QuestObjective> getOblivionQuestObjective(MWBase::World& world, const ESM::FormKey& tracked)
    {
        const auto* native = getNativeWorld(world);
        const auto* scripts = native ? native->getOblivionScriptManager() : nullptr;
        const auto* ai = native ? native->getOblivionAiService() : nullptr;
        if (!scripts || !ai)
            return std::nullopt;
        const auto player = world.getPlayerPtr();
        if (player.isEmpty() || !player.isInCell())
            return std::nullopt;

        const auto& store = world.getStore();
        const auto context = questConditionContext(world, *scripts);
        const auto inPlayerCell = [&](const ESM4::PackageCondition& condition) {
            if (condition.mFunction != 67 && (condition.mFunction != ESM4::FUN_GetInCellParam
                    || ESM4::runtimeReferenceKey(condition.mParameter2.mReferenceKey) != ESM::FormKey::dynamic("player", 1)))
                return false;
            const auto* cell = store.search<ESM4::Cell>(condition.mParameter1.mReferenceKey);
            return cell && player.getCell()->getCell()->getId() == cell->mId;
        };
        auto outside = context;
        outside.mResolve = [&](const auto& condition, auto subject, bool opposite) {
            if (subject == ESM4::ConditionSubject::Self && inPlayerCell(condition))
                return ESM4::ConditionValue::value(0);
            return context.mResolve(condition, subject, opposite);
        };

        struct Candidate
        {
            const ESM4::Quest* mQuest;
            const ESM4::RuntimeQuestState* mState;
        };
        std::vector<Candidate> quests;
        for (const auto& quest : store.get<ESM4::Quest>())
        {
            const auto* state = scripts->findQuestState(quest.mFormKey);
            const bool manual = tracked == quest.mFormKey && !tracked.isNull();
            if (!state || (!manual && !state->mRunning) || quest.mQuestName.empty())
                continue;
            quests.emplace_back(&quest, state);
        }
        // Resolve doors in selection order. A lower-priority quest may need an
        // entire cross-world route that the compass will never display.
        std::sort(quests.begin(), quests.end(), [&](const auto& a, const auto& b) {
            const auto& left = *a.mQuest;
            const auto& right = *b.mQuest;
            if ((left.mFormKey == tracked) != (right.mFormKey == tracked))
                return left.mFormKey == tracked;
            if (left.mData.priority != right.mData.priority)
                return left.mData.priority > right.mData.priority;
            return left.mFormKey < right.mFormKey;
        });

        std::optional<QuestObjective> textOnly;
        for (const auto& [record, state] : quests)
        {
            const auto& quest = *record;
            auto journal = projectQuest(quest, *state, context);
            if (journal.mFinished)
                continue;
            QuestObjective candidate{ quest.mFormKey, std::move(journal.mTitle), {}, {} };
            if (!journal.mSteps.empty())
                candidate.mText = std::move(journal.mSteps.back().mText);
            bool arrived = false;
            const auto stage = std::lower_bound(state->mCompletedStages.begin(), state->mCompletedStages.end(), state->mStage);
            for (const auto& target : quest.mTargets)
            {
                if (ESM4::evaluateNativeConditions(target.mConditions, context) != ESM4::ConditionResult::True)
                {
                    // Some objectives hide their marker when the player enters the room.
                    // Keep the current lead instead of switching to another quest at the door.
                    if (!arrived && !candidate.mText.empty() && std::ranges::any_of(target.mConditions, inPlayerCell))
                        arrived = ESM4::evaluateNativeConditions(target.mConditions, outside) == ESM4::ConditionResult::True;
                    // Entering a targeted door can advance the journal to an
                    // exploration stage with no marker until the next conversation.
                    if (!arrived && !candidate.mText.empty() && !player.getCell()->isExterior()
                        && stage != state->mCompletedStages.begin())
                    {
                        const auto* door = store.search<ESM4::Reference>(target.mReference);
                        const auto* landing = door && !door->mDoor.destDoor.isZeroOrUnset()
                            ? store.get<ESM4::Reference>().search(door->mDoor.destDoor) : nullptr;
                        if (landing && landing->mParent == player.getCell()->getCell()->getId())
                        {
                            auto beforeEntry = context;
                            beforeEntry.mResolve = [&](const auto& condition, auto subject, bool opposite) {
                                if (condition.mFunction == 58 && condition.mParameter1.mReferenceKey == quest.mFormKey)
                                    return ESM4::ConditionValue::value(*std::prev(stage));
                                return context.mResolve(condition, subject, opposite);
                            };
                            arrived = ESM4::evaluateNativeConditions(target.mConditions, beforeEntry) == ESM4::ConditionResult::True;
                        }
                    }
                    continue;
                }
                auto destination = ai->questDestination(target.mReference, player);
                // Keep a known target while its NPC is behind a locked door. The
                // label and quest stay active even when there is no usable route.
                if (!destination.mLabel.empty() || destination.mNearby || destination.mWorldMap)
                    candidate.mDestinations.push_back(std::move(destination));
            }
            if (quest.mFormKey == tracked || arrived || !candidate.mDestinations.empty())
                return candidate;
            if (!textOnly && !candidate.mText.empty())
                textOnly = std::move(candidate);
        }
        return textOnly;
    }
}
