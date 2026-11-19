#include "oblivionquestguidance.hpp"

#include <algorithm>
#include <deque>
#include <set>
#include <tuple>

#include <components/esm4/aiconditions.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadqust.hpp>

#include "../mwmechanics/oblivionai.hpp"

#include "class.hpp"
#include "oblivionscriptmanager.hpp"
#include "worldimp.hpp"

namespace
{
    ESM::RefId mapSpace(const ESM4::Cell& cell)
    {
        return cell.isExterior() ? cell.mParent : ESM::RefId(cell.mId);
    }
}

namespace MWMechanics
{
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
                result.mLabel = npc->mFullName;
            else if (const auto* placed = store.search<ESM4::Reference>(reference))
                result.mLabel = placed->mFullName;
        }
        if (result.mLabel.empty())
            result.mLabel = targetCell->mFullName;

        const auto* playerCell = player.getCell()->getCell();
        const ESM::RefId playerSpace = playerCell->getWorldSpace();
        const ESM::RefId targetSpace = mapSpace(*targetCell);
        if (playerSpace == targetSpace)
            result.mNearby = MWWorld::QuestMarker{ targetSpace, position };
        if (targetCell->isExterior())
            result.mWorldMap = MWWorld::QuestMarker{ targetSpace, position };

        // Walk the existing incoming XTEL index back from the target. This
        // preserves one-way links, and does not materialize other cells.
        std::deque<ESM::FormKey> pending{ cellKey };
        std::set<ESM::FormKey> visited{ cellKey };
        while (!pending.empty() && !(result.mNearby && result.mWorldMap))
        {
            const auto incoming = mUnloadedDoorsByDestinationCell.find(pending.front());
            pending.pop_front();
            if (incoming == mUnloadedDoorsByDestinationCell.end())
                continue;
            for (const auto index : incoming->second)
            {
                const auto& door = mUnloadedLocations[index];
                if (visited.contains(door.mCell))
                    continue;
                const auto* source = store.search<ESM4::Cell>(door.mCell);
                const auto doorState = currentDoorState(door);
                if (!source || !doorState.mAvailable || doorState.mLocked)
                    continue;

                const auto space = mapSpace(*source);
                auto doorPosition = door.mPosition;
                if (const auto resident = loadedPtrFor(door.mReference); !resident.isEmpty())
                    doorPosition = resident.getRefData().getPosition().asVec3();
                const MWWorld::QuestMarker marker{ space, doorPosition };
                if (!result.mNearby && space == playerSpace)
                    result.mNearby = marker;
                if (source->isExterior() && !result.mWorldMap)
                    result.mWorldMap = marker;
                if (!source->isExterior())
                {
                    visited.insert(door.mCell);
                    pending.push_back(door.mCell);
                }
            }
        }
        return result;
    }
}

namespace MWWorld
{
    std::optional<QuestObjective> getOblivionQuestObjective(MWBase::World& world)
    {
        if (world.getGameProfile() != ESM::GameProfile::Oblivion)
            return std::nullopt;
        const auto* native = dynamic_cast<const World*>(&world);
        const auto* scripts = native ? native->getOblivionScriptManager() : nullptr;
        const auto* ai = native ? native->getOblivionAiService() : nullptr;
        const auto player = world.getPlayerPtr();
        if (!scripts || !ai || player.isEmpty() || !player.isInCell())
            return std::nullopt;

        const auto& store = world.getStore();
        ESM4::NativeConditionEvaluationContext context;
        context.mHasActor = true;
        context.mResolveGlobal = [&](const ESM::FormKey& key) -> std::optional<double> {
            if (const auto* global = store.search<ESM4::GlobalVariable>(key))
                return world.getGlobalFloat(GlobalVariableName(global->mEditorId));
            return std::nullopt;
        };
        context.mResolve = [&](const ESM4::PackageCondition& condition, ESM4::ConditionSubject subject, bool) {
            using Value = ESM4::ConditionValue;
            const auto& key = condition.mParameter1.mReferenceKey;
            switch (condition.mFunction)
            {
                case 56: // GetQuestRunning
                case 58: // GetStage
                case 59: // GetStageDone
                {
                    const auto* state = scripts->findQuestState(key);
                    if (condition.mFunction == 56)
                        return Value::value(state && state->mRunning);
                    if (condition.mFunction == 58)
                        return Value::value(state ? state->mStage : 0);
                    return Value::value(state && std::ranges::binary_search(
                        state->mCompletedStages, condition.mParameter2.mNumber));
                }
                case 74: // GetGlobalValue
                    if (const auto value = context.mResolveGlobal(key))
                        return Value::value(*value);
                    return Value::missing();
                case 79: // GetQuestVariable
                    if (const auto value = scripts->questVariable(key, condition.mParameter2.mNumber))
                        return Value::value(*value);
                    return Value::missing();
                case 67: // GetInCell
                    if (subject == ESM4::ConditionSubject::Self)
                        if (const auto* cell = store.search<ESM4::Cell>(key))
                            return Value::value(player.getCell()->getCell()->getId() == ESM::RefId(cell->mId));
                    return Value::missing();
                default: return Value::unsupported();
            }
        };
        const auto eligible = [&](std::span<const ESM4::PackageCondition> conditions) {
            return ESM4::evaluateNativeConditions(conditions, context) == ESM4::ConditionResult::True;
        };

        std::optional<QuestObjective> objective;
        std::tuple<bool, unsigned> best{};
        for (const auto& quest : store.get<ESM4::Quest>())
        {
            const auto* state = scripts->findQuestState(quest.mFormKey);
            if (!state || !state->mRunning || quest.mQuestName.empty())
                continue;

            QuestObjective candidate{ quest.mFormKey, quest.mQuestName, {}, {} };
            const ESM4::Quest::JournalEntry* journal = nullptr;
            bool complete = false;
            for (const auto& entry : quest.mJournalEntries)
            {
                if (!std::ranges::binary_search(state->mCompletedStages, entry.mStage)
                    || !eligible(entry.mConditions))
                    continue;
                if (entry.mFlags & ESM4::Quest::JournalEntry::CompleteQuest)
                    complete = true;
                if (!entry.mText.empty() && (!journal || entry.mStage >= journal->mStage))
                    journal = &entry;
            }
            if (complete)
                continue;
            if (journal)
                candidate.mText = journal->mText;
            for (const auto& target : quest.mTargets)
            {
                if (!eligible(target.mConditions))
                    continue;
                auto destination = ai->questDestination(target.mReference, player);
                if (destination.mNearby || destination.mWorldMap)
                    candidate.mDestinations.push_back(std::move(destination));
            }
            if (candidate.mText.empty() && candidate.mDestinations.empty())
                continue;
            const auto score = std::tuple{ !candidate.mDestinations.empty(), unsigned(quest.mData.priority) };
            if (!objective || score > best || (score == best && candidate.mQuest < objective->mQuest))
            {
                best = score;
                objective = std::move(candidate);
            }
        }
        return objective;
    }
}
