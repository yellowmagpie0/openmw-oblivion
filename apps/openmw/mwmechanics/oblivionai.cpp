/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#include "oblivionai.hpp"
#include "oblivioncombat.hpp"
#include "oblivionaidestination.hpp"
#include "oblivionaigait.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iterator>
#include <limits>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>

#include <components/debug/debuglog.hpp>
#include <components/detournavigator/navigatorutils.hpp>
#include <components/esm/attr.hpp>
#include <components/esm/formkey.hpp>
#include <components/esm/refid.hpp>
#include <components/esm/util.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadachr.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/playermechanics.hpp>
#include <components/esm4/runtimereferences.hpp>
#include <components/esm4/loadwthr.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/datetimemanager.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/globalvariablename.hpp"
#include "../mwworld/action.hpp"
#include "../mwworld/oblivionprofileservices.hpp"
#include "../mwworld/oblivionscriptmanager.hpp"
#include "../mwworld/worldimp.hpp"
#include "../mwworld/worldmodel.hpp"

#include "creaturestats.hpp"
#include "activespells.hpp"
#include "magiceffects.hpp"
#include "movement.hpp"
#include "npcstats.hpp"
#include "steering.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/oblivionscriptmanager.hpp"
#include "../mwworld/player.hpp"
#include "../mwworld/weather.hpp"

namespace MWMechanics
{
    namespace
    {
        constexpr float sFixedStep = 1.f / 30.f;
        constexpr float sArrivalTolerance = 64.f;
        constexpr float sDoorLookAheadDistance = 2.f * sArrivalTolerance;
        constexpr float sRouteTailApproachDistance = 2.f * sDoorLookAheadDistance;
        constexpr float sMaximumFrameCatchup = 8.f * sFixedStep;

        bool nativeActor(const MWWorld::ConstPtr& ptr)
        {
            if (ptr.isEmpty())
                return false;
            const unsigned type = ptr.getClass().getType();
            return type == ESM::REC_NPC_4 || type == ESM::REC_CREA4;
        }

        float distanceSquared(const osg::Vec3f& left, const osg::Vec3f& right)
        {
            return (left - right).length2();
        }

        float distanceToSegmentSquared(const osg::Vec3f& point, const osg::Vec3f& start, const osg::Vec3f& end)
        {
            const osg::Vec3f segment = end - start;
            const float lengthSquared = segment.length2();
            if (lengthSquared <= 0.f)
                return distanceSquared(point, start);
            const osg::Vec3f offset = point - start;
            const float projection = std::clamp(
                (offset.x() * segment.x() + offset.y() * segment.y() + offset.z() * segment.z()) / lengthSquared,
                0.f, 1.f);
            return distanceSquared(point, start + segment * projection);
        }

        MWWorld::Ptr findBlockingOblivionDoor(const MWWorld::Ptr& actor, const MWPhysics::PhysicsSystem& physics,
            const osg::Vec3f& destination, float maximumDistance)
        {
            const osg::Vec3f position = actor.getRefData().getPosition().asVec3();
            osg::Vec3f direction(destination.x() - position.x(), destination.y() - position.y(), 0.f);
            const float distance = direction.normalize();
            if (distance <= 0.f)
                return {};

            const osg::Vec3f from = position + osg::Vec3f(0.f, 0.f, 32.f);
            const auto hit = physics.castRay(from,
                from + direction * std::min(distance, maximumDistance), { actor }, {},
                MWPhysics::CollisionType_World | MWPhysics::CollisionType_Door);
            const MWWorld::Ptr door = hit.mHitObject;
            if (!hit.mHit || door.isEmpty() || door.getClass().getType() != ESM::REC_DOOR4
                || !door.getRefData().isEnabled() || door.mRef->isDeleted()
                || door.getCellRef().getTeleport()
                || door.getClass().getDoorState(door) != MWWorld::DoorState::Idle)
                return {};
            return door;
        }

        MWWorld::Ptr mutablePtr(const MWWorld::ConstPtr& ptr)
        {
            return MWWorld::Ptr(const_cast<MWWorld::LiveCellRefBase*>(ptr.mRef),
                const_cast<MWWorld::CellStore*>(ptr.mCell));
        }

        float wrapAngle(float angle)
        {
            while (angle > osg::PIf)
                angle -= 2.f * osg::PIf;
            while (angle < -osg::PIf)
                angle += 2.f * osg::PIf;
            return angle;
        }

        std::string jsonQuote(std::string_view value)
        {
            std::string result;
            result.reserve(value.size() + 2);
            result.push_back('"');
            for (const unsigned char character : value)
            {
                switch (character)
                {
                    case '"':
                        result += "\\\"";
                        break;
                    case '\\':
                        result += "\\\\";
                        break;
                    case '\n':
                        result += "\\n";
                        break;
                    case '\r':
                        result += "\\r";
                        break;
                    case '\t':
                        result += "\\t";
                        break;
                    default:
                        if (character < 0x20)
                            result += "?";
                        else
                            result.push_back(static_cast<char>(character));
                        break;
                }
            }
            result.push_back('"');
            return result;
        }

        bool containsInsensitive(std::string_view value, std::string_view needle)
        {
            if (needle.empty() || value.size() < needle.size())
                return false;
            for (std::size_t offset = 0; offset + needle.size() <= value.size(); ++offset)
            {
                bool matches = true;
                for (std::size_t index = 0; index < needle.size(); ++index)
                    if (std::tolower(static_cast<unsigned char>(value[offset + index]))
                        != std::tolower(static_cast<unsigned char>(needle[index])))
                    {
                        matches = false;
                        break;
                    }
                if (matches)
                    return true;
            }
            return false;
        }

        ESM4::PackagePhaseState toPhaseState(const ESM4::RuntimeActorAiState& state)
        {
            ESM4::PackagePhaseState result;
            result.mActor = state.mActor;
            result.mBase = state.mBase;
            result.mPackage = state.mPackage;
            result.mTarget = state.mTarget;
            result.mCell = state.mCell;
            result.mPathgrid = state.mPathgrid;
            result.mDoor = state.mDoor;
            result.mSource = state.mSource;
            result.mPackageType = state.mPackageType;
            result.mProcedure = state.mProcedure;
            result.mPhase = state.mPhase;
            result.mTier = state.mTier;
            result.mBoundary = state.mBoundary;
            result.mListIndex = state.mListIndex;
            result.mSelectionGeneration = state.mSelectionGeneration;
            result.mRouteGeneration = state.mRouteGeneration;
            result.mTransitionGeneration = state.mTransitionGeneration;
            result.mPathNode = state.mPathNode;
            result.mRepathAttempts = state.mRepathAttempts;
            result.mActionTimer = state.mActionTimer;
            result.mDurationRemaining = state.mDurationRemaining;
            result.mNoProgressSeconds = state.mNoProgressSeconds;
            result.mRestrained = state.mRestrained;
            result.mActionReserved = state.mActionReserved;
            result.mInterruptionReason = state.mInterruptionReason;
            return result;
        }

        void fromPhaseState(ESM4::RuntimeActorAiState& state, const ESM4::PackagePhaseState& phase)
        {
            state.mPackage = phase.mPackage;
            state.mTarget = phase.mTarget;
            state.mCell = phase.mCell;
            state.mPathgrid = phase.mPathgrid;
            state.mDoor = phase.mDoor;
            state.mSource = phase.mSource;
            state.mPackageType = phase.mPackageType;
            state.mProcedure = phase.mProcedure;
            state.mPhase = phase.mPhase;
            state.mTier = phase.mTier;
            state.mBoundary = phase.mBoundary;
            state.mListIndex = phase.mListIndex;
            state.mSelectionGeneration = phase.mSelectionGeneration;
            state.mRouteGeneration = phase.mRouteGeneration;
            state.mTransitionGeneration = phase.mTransitionGeneration;
            state.mPathNode = phase.mPathNode;
            state.mRepathAttempts = phase.mRepathAttempts;
            state.mActionTimer = phase.mActionTimer;
            state.mDurationRemaining = phase.mDurationRemaining;
            state.mNoProgressSeconds = phase.mNoProgressSeconds;
            state.mRestrained = phase.mRestrained;
            state.mActionReserved = phase.mActionReserved;
            state.mInterruptionReason = phase.mInterruptionReason;
        }

        ESM4::ConditionValue value(double number)
        {
            return std::isfinite(number) ? ESM4::ConditionValue::value(number)
                                         : ESM4::ConditionValue::missing();
        }

        const std::array<ESM::RefId, 21>& actorValueSkills()
        {
            static const std::array<ESM::RefId, 21> skills = {
                ESM::Skill::Armorer,
                ESM::Skill::Athletics,
                ESM::Skill::LongBlade,
                ESM::Skill::Block,
                ESM::Skill::BluntWeapon,
                ESM::Skill::HandToHand,
                ESM::Skill::HeavyArmor,
                ESM::Skill::Alchemy,
                ESM::Skill::Alteration,
                ESM::Skill::Conjuration,
                ESM::Skill::Destruction,
                ESM::Skill::Illusion,
                ESM::Skill::Mysticism,
                ESM::Skill::Restoration,
                ESM::Skill::Acrobatics,
                ESM::Skill::LightArmor,
                ESM::Skill::Marksman,
                ESM::Skill::Mercantile,
                ESM::Skill::Security,
                ESM::Skill::Sneak,
                ESM::Skill::Speechcraft,
            };
            return skills;
        }
    }

    OblivionAiService::OblivionAiService(MWWorld::World& world)
        : mWorld(world)
    {
        if (const char* eventPath = std::getenv("OPENMW_OBLIVION_AI_EVENTS"); eventPath != nullptr
            && *eventPath != '\0')
        {
            mEventStream.open(eventPath, std::ios::out | std::ios::trunc);
            if (!mEventStream)
                Log(Debug::Error) << "Unable to open OPENMW_OBLIVION_AI_EVENTS file " << eventPath;
        }
        if (const char* seed = std::getenv("OPENMW_OBLIVION_AI_SEED"); seed != nullptr && *seed != '\0')
        {
            char* end = nullptr;
            const unsigned long long parsed = std::strtoull(seed, &end, 10);
            if (end != seed && *end == '\0' && parsed != 0)
                mNextEvaluationGeneration = static_cast<std::uint64_t>(parsed);
            else
                Log(Debug::Warning) << "Ignoring invalid OPENMW_OBLIVION_AI_SEED value " << seed;
        }
        seedActors();
        buildUnloadedLocationIndex();
    }

    OblivionAiService::~OblivionAiService()
    {
        flushDiagnosticCounters();
    }

    void OblivionAiService::flushDiagnosticCounters()
    {
        if (!mEventStream)
            return;
        for (const auto& [key, count] : mDiagnosticCounters)
        {
            mEventStream << "{\"event\":\"diagnostic-summary\",\"key\":" << jsonQuote(key)
                         << ",\"count\":" << count << "}\n";
        }
        mEventStream.flush();
        mDiagnosticCounters.clear();
    }

    void OblivionAiService::clear()
    {
        for (auto& [_, live] : mActors)
            releasePhysicalDoorCollision(loadedPtrFor(live.mState.mActor), live);
        mPendingPackageDone.restore({});
        mActors.clear();
        mDetectionVectors.clear();
        flushDiagnosticCounters();
        mNextEvaluationGeneration = 1;
        seedActors();
    }

    bool OblivionAiService::handles(const MWWorld::ConstPtr& actor) const
    {
        return nativeActor(actor) || (!actor.isEmpty() && actor == mWorld.getPlayerConstPtr());
    }

    ESM::FormKey OblivionAiService::actorKey(const MWWorld::ConstPtr& actor) const
    {
        if (actor.isEmpty())
            return {};
        if (actor == mWorld.getPlayerConstPtr())
            return ESM::FormKey::dynamic("player", 1);
        return actor.getCellRef().getFormKey();
    }

    ESM::FormKey OblivionAiService::baseKey(const MWWorld::ConstPtr& actor) const
    {
        if (actor.isEmpty())
            return {};
        if (actor == mWorld.getPlayerConstPtr())
            return ESM::FormKey::dynamic("player-base", 1);
        if (actor.getClass().getType() == ESM::REC_NPC_4)
        {
            const auto* ref = actor.get<ESM4::Npc>();
            return ref != nullptr && ref->mBase != nullptr ? ref->mBase->mFormKey : ESM::FormKey{};
        }
        if (actor.getClass().getType() == ESM::REC_CREA4)
        {
            const auto* ref = actor.get<ESM4::Creature>();
            return ref != nullptr && ref->mBase != nullptr ? ref->mBase->mFormKey : ESM::FormKey{};
        }
        return {};
    }

    ESM::FormKey OblivionAiService::cellKey(const MWWorld::ConstPtr& actor) const
    {
        if (actor.isEmpty() || !actor.isInCell())
            return {};
        if (actor == mWorld.getPlayerConstPtr() && actor.getCell()->isExterior())
        {
            // The player is backed by the legacy ESM3 NPC projection even in
            // an Oblivion world. Its CellStore may therefore be the
            // persistent/player group rather than the native ESM4 exterior
            // cell containing the current global position. Use the native
            // worldspace and spatial coordinates as the stable identity.
            const ESM::ExteriorCellLocation location = ESM::positionToExteriorCellLocation(
                actor.getRefData().getPosition().pos[0], actor.getRefData().getPosition().pos[1],
                actor.getCell()->getCell()->getWorldSpace());
            if (const ESM4::Cell* cell = mWorld.mStore.get<ESM4::Cell>().searchExterior(location))
                return cell->mFormKey;
        }
        if (const ESM::FormId* id = actor.getCell()->getCell()->getId().getIf<ESM::FormId>())
            return ESM::FormKeyResolver(mWorld.mContentFiles).toFormKey(*id);
        if (actor.getCell()->getCell()->isEsm4())
            return actor.getCell()->getCell()->getEsm4().mFormKey;
        return {};
    }

    MWWorld::Ptr OblivionAiService::loadedPtrFor(const ESM::FormKey& key) const
    {
        if (key.isNull())
            return {};
        if (ESM4::runtimeReferenceKey(key) == ESM::FormKey::dynamic("player", 1))
            return mWorld.getPlayerPtr();

        // Native actor records carry the same RefNum that the world model
        // registers for resident references. Resolve those through the
        // registry first; scanning every resident cell for every actor would
        // make the low-process pass quadratic in a content-sized world.
        if (const ESM4::ActorCharacter* reference = mWorld.mStore.get<ESM4::ActorCharacter>().search(key))
            return mWorld.mWorldModel.getPtr(reference->mId);
        if (const ESM4::ActorCreature* reference = mWorld.mStore.get<ESM4::ActorCreature>().search(key))
            return mWorld.mWorldModel.getPtr(reference->mId);
        if (const ESM4::Reference* reference = mWorld.mStore.get<ESM4::Reference>().search(key))
            return mWorld.mWorldModel.getPtr(reference->mId);

        const ESM::FormKeyResolver resolver(mWorld.mContentFiles);
        const std::optional<ESM::FormId> id = resolver.toFormId(key);
        if (!id)
            return {};

        // CellStore::getPtr intentionally loads an unloaded/preloaded cell.
        // This helper is used by the per-frame scheduler to distinguish high
        // process actors from state-only actors, so allowing that side effect
        // would turn one frame into a scan/load of the entire world. Search
        // only stores that are already resident; ptrFor() remains the explicit
        // force-load path for a destination or an interaction target.
        MWWorld::Ptr result;
        mWorld.mWorldModel.forEachLoadedCellStore([&](MWWorld::CellStore& cell) {
            if (!result.isEmpty() || cell.getState() != MWWorld::CellStore::State_Loaded)
                return;
            result = cell.search(ESM::RefId(*id));
        });
        return result;
    }

    void OblivionAiService::buildUnloadedLocationIndex()
    {
        mUnloadedLocations.clear();
        mUnloadedLocationByReference.clear();
        mUnloadedLocationsByBase.clear();
        mUnloadedLocationsByType.clear();
        mUnloadedDoorsByCell.clear();
        mUnloadedDoorsByDestinationCell.clear();

        const ESM::FormKeyResolver resolver(mWorld.mContentFiles);
        const auto ownerKey = [resolver](const ESM::RefId& owner) {
            if (const ESM::FormId* id = owner.getIf<ESM::FormId>())
                return resolver.toFormKey(*id);
            return ESM::FormKey{};
        };
        const auto cellForParent = [this](const ESM::RefId& parent, const ESM::FormKey& fallback) {
            if (const ESM4::Cell* cell = mWorld.mStore.get<ESM4::Cell>().search(parent))
                return cell->mFormKey;
            if (!fallback.isNull())
                if (const ESM4::Cell* cell = mWorld.mStore.get<ESM4::Cell>().search(fallback))
                    return cell->mFormKey;
            return ESM::FormKey{};
        };
        const auto add = [this](UnloadedLocation location) {
            if (location.mReference.isNull())
                return;
            const std::size_t index = mUnloadedLocations.size();
            mUnloadedLocations.push_back(std::move(location));
            mUnloadedLocationByReference.emplace(mUnloadedLocations.back().mReference, index);
            if (!mUnloadedLocations.back().mBase.isNull())
                mUnloadedLocationsByBase[mUnloadedLocations.back().mBase].push_back(index);
            for (std::uint32_t objectType = 1; objectType <= 35; ++objectType)
                if (ESM4::packageObjectTypeMatches(objectType, mUnloadedLocations.back().mType))
                    mUnloadedLocationsByType[objectType].push_back(index);
            if (mUnloadedLocations.back().mType == ESM::REC_DOOR4
                && !mUnloadedLocations.back().mCell.isNull()
                && !mUnloadedLocations.back().mTeleportDoor.isNull())
                mUnloadedDoorsByCell[mUnloadedLocations.back().mCell].push_back(index);
        };

        for (const ESM4::Reference& reference : mWorld.mStore.get<ESM4::Reference>())
        {
            UnloadedLocation location{ reference.mFormKey, resolver.toFormKey(reference.mBaseObj),
                cellForParent(reference.mParent, reference.mParentKey), reference.mPos.asVec3(),
                static_cast<ESM::RecNameInts>(mWorld.mStore.find(reference.mBaseObj)),
                (reference.mFlags & (ESM4::Rec_Disabled | ESM4::Rec_Deleted)) == 0, reference.mIsLocked,
                resolver.toFormKey(reference.mOwner), ESM::RefId(reference.mKey), {}, {} };
            location.mTeleportDoor = resolver.toFormKey(reference.mDoor.destDoor);
            location.mTeleportPosition = reference.mDoor.destPos.asVec3();
            add(std::move(location));
        }
        for (const ESM4::ActorCharacter& reference : mWorld.mStore.get<ESM4::ActorCharacter>())
        {
            add(UnloadedLocation{ reference.mFormKey, resolver.toFormKey(reference.mBaseObj),
                cellForParent(reference.mParent, reference.mParentKey), reference.mPos.asVec3(), ESM::REC_ACHR4,
                (reference.mFlags & (ESM4::Rec_Disabled | ESM4::Rec_Deleted)) == 0, false,
                resolver.toFormKey(reference.mOwner), {}, {}, {} });
        }
        for (const ESM4::ActorCreature& reference : mWorld.mStore.get<ESM4::ActorCreature>())
        {
            add(UnloadedLocation{ reference.mFormKey, resolver.toFormKey(reference.mBaseObj),
                cellForParent(reference.mParent, reference.mParentKey), reference.mPos.asVec3(), ESM::REC_ACRE4,
                (reference.mFlags & (ESM4::Rec_Disabled | ESM4::Rec_Deleted)) == 0, false,
                resolver.toFormKey(reference.mOwner), {}, {}, {} });
        }

        // The destination marker is another placed reference. Build the
        // reverse cell index only after all references have been inserted so
        // forward references are resolved independently of record order.
        for (const auto& [_, doors] : mUnloadedDoorsByCell)
            for (const std::size_t index : doors)
            {
                const UnloadedLocation& door = mUnloadedLocations[index];
                const auto destination = mUnloadedLocationByReference.find(door.mTeleportDoor);
                if (destination == mUnloadedLocationByReference.end())
                    continue;
                const UnloadedLocation& marker = mUnloadedLocations[destination->second];
                // Index all authored edges. Availability is mutable and is
                // checked when planning/traversing, not frozen at startup.
                if (marker.mCell.isNull())
                    continue;
                mUnloadedDoorsByDestinationCell[marker.mCell].push_back(index);
            }
        const auto byReference = [this](std::size_t left, std::size_t right) {
            return mUnloadedLocations[left].mReference < mUnloadedLocations[right].mReference;
        };
        for (auto& [_, doors] : mUnloadedDoorsByCell)
            std::sort(doors.begin(), doors.end(), byReference);
        for (auto& [_, doors] : mUnloadedDoorsByDestinationCell)
            std::sort(doors.begin(), doors.end(), byReference);
    }

    void OblivionAiService::seedActors()
    {
        const ESM::FormKey playerKey = ESM::FormKey::dynamic("player", 1);
        const ESM::FormKeyResolver resolver(mWorld.mContentFiles);
        const auto add = [&](const ESM4::ActorCharacter& reference) {
            if (reference.mFormKey.isNull() || reference.mFormKey == playerKey)
                return;

            const ESM::FormKey base = resolver.toFormKey(reference.mBaseObj);
            const ESM4::Npc* npc = base.isNull() ? nullptr : mWorld.mStore.search<ESM4::Npc>(base);
            const ESM4::Creature* creature = base.isNull() ? nullptr : mWorld.mStore.search<ESM4::Creature>(base);
            if (npc == nullptr && creature == nullptr)
                return;

            const ESM4::Cell* cell = mWorld.mStore.get<ESM4::Cell>().search(reference.mParent);
            if (cell == nullptr || cell->mFormKey.isNull())
                return;

            LiveActor live;
            live.mState.mActor = reference.mFormKey;
            live.mState.mBase = base;
            live.mState.mCell = cell->mFormKey;
            live.mState.mLastValidCell = cell->mFormKey;
            live.mState.mLastValidPosition = reference.mPos;
            live.mState.mSource = ESM4::PackageSource::None;
            live.mState.mPackageType = ESM4::AIPackageType::Unknown;
            live.mState.mProcedure = ESM4::PackageProcedure::None;
            setPhase(live, ESM4::PackagePhase::Select);
            live.mState.mTier = ESM4::ProcessTier::Low;
            live.mState.mNextLowProcessTick = 0.f;
            live.mLastRiddenHorse = reference.mHorseKey;
            live.mNeedsSelection = true;
            mActors.emplace(reference.mFormKey, std::move(live));
        };

        for (const ESM4::ActorCharacter& reference : mWorld.mStore.get<ESM4::ActorCharacter>())
            add(reference);
        for (const ESM4::ActorCreature& reference : mWorld.mStore.get<ESM4::ActorCreature>())
            add(reference);
    }

    OblivionAiService::LiveActor& OblivionAiService::ensure(const MWWorld::ConstPtr& actor)
    {
        const ESM::FormKey key = actorKey(actor);
        if (key.isNull())
            throw std::runtime_error("Native TES4 actor has no stable reference key");
        LiveActor& result = mActors[key];
        if (result.mState.mActor.isNull())
        {
            result.mState.mActor = key;
            result.mState.mSource = ESM4::PackageSource::None;
            result.mState.mPackageType = ESM4::AIPackageType::Unknown;
            result.mState.mProcedure = ESM4::PackageProcedure::None;
            setPhase(result, ESM4::PackagePhase::Select);
            result.mNeedsSelection = true;
        }
        synchronizeIdentity(result, actor);
        return result;
    }

    void OblivionAiService::setPhase(LiveActor& live, ESM4::PackagePhase phase)
    {
        if (live.mState.mPhase == ESM4::PackagePhase::Door && phase != ESM4::PackagePhase::Door)
            live.mState.mDoorAnimationStarted = false;
        live.mState.mPhase = phase;
    }

    void OblivionAiService::synchronizeIdentity(LiveActor& live, const MWWorld::ConstPtr& actor)
    {
        const ESM::FormKey currentCell = cellKey(actor);
        const osg::Vec3f currentPosition = actor.getRefData().getPosition().asVec3();
        const bool preserveAbstractPosition = !live.mState.mCell.isNull()
            && preserveOblivionAbstractPosition(live.mAbstractPositionDirty, live.mHasObservedPosition,
                currentCell, live.mLastObservedCell, currentPosition, live.mLastObservedPosition);
        if (live.mAbstractPositionDirty && !preserveAbstractPosition)
            live.mAbstractPositionDirty = false;

        live.mState.mActor = actorKey(actor);
        live.mState.mBase = baseKey(actor);
        if (!preserveAbstractPosition)
            live.mState.mCell = currentCell;
        if (!live.mState.mCell.isNull())
        {
            live.mState.mLastValidCell = live.mState.mCell;
            if (!preserveAbstractPosition)
                live.mState.mLastValidPosition = actor.getRefData().getPosition();
        }

        // XHRS is stored on the actor reference, not on the base actor.  It
        // identifies the last-ridden horse and does not by itself mean that
        // the actor is mounted right now. Current mounting is restored from
        // the reciprocal runtime relation instead.
        ESM::FormKey persistedLastRiddenHorse;
        const ESM::RefNum refNum = actor.getCellRef().getRefNum();
        if (const ESM4::ActorCharacter* reference = mWorld.mStore.get<ESM4::ActorCharacter>().search(refNum))
            persistedLastRiddenHorse = reference->mHorseKey;
        if (persistedLastRiddenHorse.isNull())
            if (const ESM4::ActorCreature* reference = mWorld.mStore.get<ESM4::ActorCreature>().search(refNum))
                persistedLastRiddenHorse = reference->mHorseKey;
        // XHRS is the serialized fallback.  A native mount/dismount can be
        // newer than the reference record during the current session, so a
        // null XHRS must not erase the authoritative runtime relationship.
        if (!persistedLastRiddenHorse.isNull() || live.mLastRiddenHorse.isNull())
            live.mLastRiddenHorse = persistedLastRiddenHorse;
    }

    const ESM4::AIPackage* OblivionAiService::package(const ESM::FormKey& key) const
    {
        return key.isNull() ? nullptr : mWorld.mStore.search<ESM4::AIPackage>(key);
    }

    std::optional<ESM4::PackageCandidate> OblivionAiService::packageCandidate(const ESM::FormKey& key) const
    {
        const ESM4::AIPackage* record = package(key);
        if (record == nullptr || record->mPackageType == ESM4::AIPackageType::Unknown)
            return std::nullopt;
        ESM4::PackageCandidate result;
        result.mKey = record->mFormKey;
        result.mType = record->mPackageType;
        result.mSchedule = record->mScheduleData;
        result.mConditions = record->mCanonicalConditions;
        return result;
    }

    std::vector<ESM4::PackageCandidate> OblivionAiService::basePackages(const MWWorld::ConstPtr& actor) const
    {
        std::vector<ESM4::PackageCandidate> result;
        if (actor.isEmpty())
            return result;

        std::span<const ESM::FormKey> keys;
        if (actor.getClass().getType() == ESM::REC_NPC_4)
            keys = actor.get<ESM4::Npc>()->mBase->mAIPackageKeys;
        else if (actor.getClass().getType() == ESM::REC_CREA4)
            keys = actor.get<ESM4::Creature>()->mBase->mAIPackageKeys;
        result.reserve(keys.size());
        for (std::size_t index = 0; index < keys.size(); ++index)
        {
            if (const auto candidate = packageCandidate(keys[index]))
            {
                ESM4::PackageCandidate ordered = *candidate;
                ordered.mListIndex = index;
                result.push_back(std::move(ordered));
            }
            else if (!keys[index].isNull())
            {
                Log(Debug::Warning) << "TES4 actor " << actorKey(actor).serialize() << " references missing PACK "
                                    << keys[index].serialize();
            }
        }
        return result;
    }

    std::vector<ESM4::PackageCandidate> OblivionAiService::basePackages(const ESM::FormKey& base) const
    {
        std::vector<ESM4::PackageCandidate> result;
        std::span<const ESM::FormKey> keys;
        if (const ESM4::Npc* npc = mWorld.mStore.search<ESM4::Npc>(base))
            keys = npc->mAIPackageKeys;
        else if (const ESM4::Creature* creature = mWorld.mStore.search<ESM4::Creature>(base))
            keys = creature->mAIPackageKeys;

        result.reserve(keys.size());
        for (std::size_t index = 0; index < keys.size(); ++index)
            if (const auto candidate = packageCandidate(keys[index]))
            {
                ESM4::PackageCandidate ordered = *candidate;
                ordered.mListIndex = index;
                result.push_back(std::move(ordered));
            }
        return result;
    }

    ESM4::ConditionEvaluationContext OblivionAiService::unloadedConditionContext(const LiveActor& live) const
    {
        struct StableLocation
        {
            ESM::FormKey mReference;
            ESM::FormKey mBase;
            ESM::FormKey mCell;
            osg::Vec3f mPosition;
            ESM::RecNameInts mType = ESM::REC_REFR4;
        };

        const ESM4::RuntimeActorAiState snapshot = live.mState;
        const ESM::FormKeyResolver resolver(mWorld.mContentFiles);
        const auto cellForParent = [this](const ESM::RefId& parent, const ESM::FormKey& fallback) {
            if (const ESM4::Cell* cell = mWorld.mStore.get<ESM4::Cell>().search(parent))
                return cell->mFormKey;
            if (!fallback.isNull())
                if (const ESM4::Cell* cell = mWorld.mStore.get<ESM4::Cell>().search(fallback))
                    return cell->mFormKey;
            return ESM::FormKey{};
        };
        const auto locationFor = [this, resolver, cellForParent](const ESM::FormKey& key)
            -> std::optional<StableLocation> {
            if (key.isNull())
                return std::nullopt;
            if (const auto found = mActors.find(key);
                found != mActors.end() && key != ESM::FormKey::dynamic("player", 1))
                return StableLocation{ key, found->second.mState.mBase,
                    found->second.mState.mLastValidCell.isNull() ? found->second.mState.mCell
                                                                   : found->second.mState.mLastValidCell,
                    found->second.mState.mLastValidPosition.asVec3(),
                    found->second.mState.mBase.isNull() ? ESM::REC_REFR4
                                                        : mWorld.mStore.search<ESM4::Creature>(found->second.mState.mBase)
                            != nullptr
                        ? ESM::REC_ACRE4
                        : ESM::REC_ACHR4 };
            if (const auto found = mUnloadedLocationByReference.find(key);
                found != mUnloadedLocationByReference.end())
            {
                const UnloadedLocation& location = mUnloadedLocations[found->second];
                return StableLocation{ location.mReference, location.mBase, location.mCell, location.mPosition,
                    location.mType };
            }
            if (const MWWorld::Ptr loaded = loadedPtrFor(key); !loaded.isEmpty())
                return StableLocation{ actorKey(loaded), baseKey(loaded), cellKey(loaded), loaded.getRefData().getPosition().asVec3(),
                    static_cast<ESM::RecNameInts>(loaded.getClass().getType()) };

            if (const ESM4::ActorCharacter* reference = mWorld.mStore.get<ESM4::ActorCharacter>().search(key))
            {
                const ESM::FormKey base = resolver.toFormKey(reference->mBaseObj);
                return StableLocation{ key, base, cellForParent(reference->mParent, reference->mParentKey),
                    reference->mPos.asVec3(), ESM::REC_ACHR4 };
            }
            if (const ESM4::ActorCreature* reference = mWorld.mStore.get<ESM4::ActorCreature>().search(key))
            {
                const ESM::FormKey base = resolver.toFormKey(reference->mBaseObj);
                return StableLocation{ key, base, cellForParent(reference->mParent, reference->mParentKey),
                    reference->mPos.asVec3(), ESM::REC_ACRE4 };
            }
            if (const ESM4::Reference* reference = mWorld.mStore.get<ESM4::Reference>().search(key))
            {
                const ESM::FormKey base = resolver.toFormKey(reference->mBaseObj);
                return StableLocation{ key, base, cellForParent(reference->mParent, reference->mParentKey),
                    reference->mPos.asVec3(), static_cast<ESM::RecNameInts>(mWorld.mStore.find(reference->mBaseObj)) };
            }
            return std::nullopt;
        };

        ESM4::ConditionEvaluationContext context;
        context.mHasTarget = locationFor(snapshot.mTarget).has_value();
        context.mHasReference = true;
        context.mHasPlayer = !mWorld.getPlayerPtr().isEmpty();
        context.mResolveGlobal = [this](const ESM::FormKey& key) -> std::optional<double> {
            const ESM4::GlobalVariable* global = mWorld.mStore.search<ESM4::GlobalVariable>(key);
            if (global == nullptr || global->mEditorId.empty())
                return std::nullopt;
            return static_cast<double>(mWorld.mGlobalVariables[MWWorld::GlobalVariableName(global->mEditorId)].getFloat());
        };
        const std::function<std::optional<double>(const ESM::FormKey&)> resolveGlobal = context.mResolveGlobal;
        context.mResolve = [this, snapshot, resolver, locationFor, resolveGlobal](const ESM4::PackageCondition& condition,
                           ESM4::ConditionSubject subject) -> ESM4::ConditionValue {
            ESM::FormKey subjectKey;
            switch (subject)
            {
                case ESM4::ConditionSubject::Self:
                    subjectKey = snapshot.mActor;
                    break;
                case ESM4::ConditionSubject::Target:
                case ESM4::ConditionSubject::CombatTarget:
                    subjectKey = snapshot.mTarget;
                    break;
                case ESM4::ConditionSubject::Reference:
                case ESM4::ConditionSubject::LinkedReference:
                    subjectKey = condition.mRunOnReferenceKey;
                    break;
                case ESM4::ConditionSubject::Player:
                    subjectKey = ESM::FormKey::dynamic("player", 1);
                    break;
            }
            const std::optional<StableLocation> subjectLocation = locationFor(subjectKey);
            if (!subjectLocation)
                return ESM4::ConditionValue::missing();

            const ESM4::ConditionParameter& parameter = condition.mParameter1;
            const ESM::FormKey parameterKey = parameter.mReferenceKey;
            const std::string_view function = ESM4::conditionFunctionName(condition.mFunction);
            const auto subjectState = [&]() -> const ESM4::RuntimeActorAiState* {
                if (subjectKey == snapshot.mActor)
                    return &snapshot;
                const auto found = mActors.find(subjectKey);
                return found == mActors.end() ? nullptr : &found->second.mState;
            };
            const auto comparisonLocation = [&]() -> std::optional<StableLocation> {
                return parameterKey.isNull() ? locationFor(snapshot.mTarget) : locationFor(parameterKey);
            };
            const ESM4::Npc* npc = mWorld.mStore.search<ESM4::Npc>(subjectLocation->mBase);
            const ESM4::Creature* creature = mWorld.mStore.search<ESM4::Creature>(subjectLocation->mBase);
            const auto actorValue = [&]() -> ESM4::ConditionValue {
                if (parameter.mNumber < 0 || parameter.mNumber >= 72)
                    return ESM4::ConditionValue::unsupported();
                try
                {
                    if (const auto native = mWorld.getOblivionScriptActorValue(
                            subjectKey, static_cast<std::uint8_t>(parameter.mNumber), false))
                        return value(*native);
                }
                catch (const std::invalid_argument&)
                {
                    return ESM4::ConditionValue::unsupported();
                }
                const auto attribute = [](const ESM4::AttributeValues& values, std::int32_t index) {
                    switch (index)
                    {
                        case 0: return static_cast<double>(values.strength);
                        case 1: return static_cast<double>(values.intelligence);
                        case 2: return static_cast<double>(values.willpower);
                        case 3: return static_cast<double>(values.agility);
                        case 4: return static_cast<double>(values.speed);
                        case 5: return static_cast<double>(values.endurance);
                        case 6: return static_cast<double>(values.personality);
                        case 7: return static_cast<double>(values.luck);
                        default: return 0.0;
                    }
                };
                if (parameter.mNumber >= 0 && parameter.mNumber < ESM::Attribute::Length)
                    return value(npc != nullptr ? attribute(npc->mData.attribs, parameter.mNumber)
                                                : creature != nullptr ? attribute(creature->mData.attribs, parameter.mNumber)
                                                                      : 0.0);
                if (parameter.mNumber == 8)
                    return value(npc != nullptr ? npc->mData.health : creature != nullptr ? creature->mData.health : 0.0);
                if (parameter.mNumber == 9)
                    return value(npc != nullptr ? npc->mBaseConfig.tes4.baseSpell
                                                : creature != nullptr ? creature->mBaseConfig.tes4.baseSpell : 0.0);
                if (parameter.mNumber == 10)
                    return value(npc != nullptr ? npc->mBaseConfig.tes4.fatigue
                                                : creature != nullptr ? creature->mBaseConfig.tes4.fatigue : 0.0);
                if (parameter.mNumber == 11)
                {
                    double encumbrance = 0.0;
                    const auto addWeight = [&](const std::vector<ESM4::InventoryItem>& inventory) {
                        for (const ESM4::InventoryItem& item : inventory)
                        {
                            const ESM::FormId id = ESM::FormId::fromUint32(item.item);
                            if (const auto definition = MWWorld::OblivionProfileServices::itemDefinition(
                                    mWorld.mStore, ESM::RefId(id)))
                                encumbrance += definition->mWeight * ESM4::inventoryItemCount(item);
                        }
                    };
                    if (npc != nullptr)
                        addWeight(npc->mInventory);
                    else if (creature != nullptr)
                        addWeight(creature->mInventory);
                    return value(encumbrance);
                }
                if (npc != nullptr && parameter.mNumber >= 12 && parameter.mNumber < 33)
                {
                    const std::array<std::uint8_t, 21> skills = { npc->mData.skills.armorer, npc->mData.skills.athletics,
                        npc->mData.skills.blade, npc->mData.skills.block, npc->mData.skills.blunt,
                        npc->mData.skills.handToHand, npc->mData.skills.heavyArmor, npc->mData.skills.alchemy,
                        npc->mData.skills.alteration, npc->mData.skills.conjuration, npc->mData.skills.destruction,
                        npc->mData.skills.illusion, npc->mData.skills.mysticism, npc->mData.skills.restoration,
                        npc->mData.skills.acrobatics, npc->mData.skills.lightArmor, npc->mData.skills.marksman,
                        npc->mData.skills.mercantile, npc->mData.skills.security, npc->mData.skills.sneak,
                        npc->mData.skills.speechcraft };
                    return value(skills[static_cast<std::size_t>(parameter.mNumber - 12)]);
                }
                return ESM4::ConditionValue::unsupported();
            };
            const auto staticInventory = [&]() {
                std::vector<ESM4::RuntimeInventoryItem> inventory;
                const auto add = [&](const std::vector<ESM4::InventoryItem>& source) {
                    for (const ESM4::InventoryItem& item : source)
                    {
                        if (item.count == 0)
                            continue;
                        ESM4::RuntimeInventoryItem projected;
                        projected.mBase = resolver.toFormKey(ESM::FormId::fromUint32(item.item));
                        projected.mCount = ESM4::inventoryItemCount(item);
                        inventory.push_back(std::move(projected));
                    }
                };
                if (npc != nullptr)
                    add(npc->mInventory);
                else if (creature != nullptr)
                    add(creature->mInventory);
                return inventory;
            };
            const auto staticInventoryCount = [&](const ESM::FormKey& itemKey) {
                if (itemKey.isNull())
                    return std::int64_t(0);
                std::int64_t count = 0;
                const auto add = [&](const std::vector<ESM4::InventoryItem>& inventory) {
                    for (const ESM4::InventoryItem& item : inventory)
                        if (resolver.toFormKey(ESM::FormId::fromUint32(item.item)) == itemKey)
                            count += ESM4::inventoryItemCount(item);
                };
                if (npc != nullptr)
                    add(npc->mInventory);
                else if (creature != nullptr)
                    add(creature->mInventory);
                return count;
            };
            const auto nativeItemDefinition = [&](const ESM::FormKey& itemKey) {
                if (itemKey.isNull())
                    return std::optional<ESM4::InventoryItemDefinition>{};
                const std::optional<ESM::FormId> id = resolver.toFormId(itemKey);
                return id ? MWWorld::OblivionProfileServices::itemDefinition(mWorld.mStore, ESM::RefId(*id))
                           : std::optional<ESM4::InventoryItemDefinition>{};
            };
            const auto staticIsEquipped = [&](const ESM::FormKey& itemKey) {
                if (const MWWorld::Ptr loaded = loadedPtrFor(subjectKey); !loaded.isEmpty())
                {
                    const std::optional<ESM::FormId> id = resolver.toFormId(itemKey);
                    if (!id)
                        return false;
                    return loaded.getClass().getInventoryStore(loaded).isEquipped(ESM::RefId(*id));
                }

                std::vector<ESM4::RuntimeInventoryItem> inventory = staticInventory();
                struct Candidate
                {
                    ESM::FormKey mKey;
                    ESM4::InventoryItemDefinition mDefinition;
                };
                std::vector<Candidate> candidates;
                for (const ESM4::RuntimeInventoryItem& item : inventory)
                {
                    const auto definition = nativeItemDefinition(item.mBase);
                    if (!definition || (definition->mType != ESM4::InventoryItemType::Armor
                                           && definition->mType != ESM4::InventoryItemType::Clothing)
                        || definition->mSlots == 0 || item.mCount <= 0)
                        continue;
                    ESM4::InventoryItemDefinition copy = *definition;
                    copy.mBase = item.mBase;
                    candidates.push_back({ item.mBase, std::move(copy) });
                }
                std::sort(candidates.begin(), candidates.end(), [](const Candidate& left, const Candidate& right) {
                    if (left.mDefinition.mValue != right.mDefinition.mValue)
                        return left.mDefinition.mValue > right.mDefinition.mValue;
                    return left.mKey < right.mKey;
                });
                for (const Candidate& candidate : candidates)
                    static_cast<void>(ESM4::equipInventoryItem(inventory, candidate.mDefinition));
                return std::any_of(inventory.begin(), inventory.end(), [&](const ESM4::RuntimeInventoryItem& item) {
                    return item.mBase == itemKey && item.mEquippedSlots != 0;
                });
            };
            const auto staticArmorRating = [&]() {
                float result = 0.f;
                const std::vector<ESM4::RuntimeInventoryItem> inventory = staticInventory();
                for (const ESM4::RuntimeInventoryItem& item : inventory)
                {
                    if (!staticIsEquipped(item.mBase))
                        continue;
                    const std::optional<ESM::FormId> id = resolver.toFormId(item.mBase);
                    if (!id)
                        continue;
                    if (const ESM4::Armor* armor = mWorld.mStore.get<ESM4::Armor>().search(ESM::RefId(*id)))
                        result += armor->mData.armor;
                }
                return result;
            };
            if (function == "GetCurrentTime")
                return value(now().mHour);
            if (function == "GetDayofWeek")
                return value(ESM4::calendarDayOfWeek(now()));
            if (function == "GetGlobalValue")
            {
                const auto global = resolveGlobal(parameterKey);
                return global ? value(*global) : ESM4::ConditionValue::missing();
            }
            if (function == "GetDistance")
            {
                const auto comparison = comparisonLocation();
                return comparison ? value((subjectLocation->mPosition - comparison->mPosition).length())
                                   : ESM4::ConditionValue::missing();
            }
            if (function == "GetLineOfSight")
            {
                const std::optional<StableLocation> comparison = comparisonLocation();
                if (!comparison)
                    return ESM4::ConditionValue::missing();
                const MWWorld::Ptr subjectPtr = loadedPtrFor(subjectKey);
                const MWWorld::Ptr target = loadedPtrFor(comparison->mReference);
                if (!subjectPtr.isEmpty() && !target.isEmpty())
                    return value(mWorld.getLOS(subjectPtr, target) ? 1.0 : 0.0);
                const auto found = mDetectionVectors.find({ subjectKey, comparison->mReference });
                return value(found != mDetectionVectors.end() && found->second.mLineOfSight ? 1.0 : 0.0);
            }
            if (function == "GetIsInSameCell")
                return value(subjectLocation->mCell == snapshot.mCell ? 1.0 : 0.0);
            if (function == "GetIsID")
                return value(!parameterKey.isNull() && subjectLocation->mBase == parameterKey ? 1.0 : 0.0);
            if (function == "GetIsReference")
                return value(!parameterKey.isNull()
                    && subjectLocation->mReference == ESM4::runtimeReferenceKey(parameterKey) ? 1.0 : 0.0);
            if (function == "GetInCell")
                return value(!parameterKey.isNull() && subjectLocation->mCell == parameterKey ? 1.0 : 0.0);
            if (function == "GetIsCreature")
                return value(subjectLocation->mType == ESM::REC_ACRE4 ? 1.0 : 0.0);
            if (function == "IsActor")
                return value(subjectLocation->mType == ESM::REC_ACHR4 || subjectLocation->mType == ESM::REC_ACRE4
                        || subjectLocation->mType == ESM::REC_NPC_4 || subjectLocation->mType == ESM::REC_CREA4
                    ? 1.0
                    : 0.0);
            if (function == "GetLevel" || function == "GetActorValue")
            {
                if (function == "GetActorValue")
                    return actorValue();
                if (const ESM4::Npc* levelNpc = mWorld.mStore.search<ESM4::Npc>(subjectLocation->mBase))
                    return value(std::max(1, static_cast<int>(levelNpc->mBaseConfig.tes4.levelOrOffset)));
                if (const ESM4::Creature* levelCreature = mWorld.mStore.search<ESM4::Creature>(subjectLocation->mBase))
                    return value(std::max(1, static_cast<int>(levelCreature->mBaseConfig.tes4.levelOrOffset)));
                return value(1.0);
            }
            if (function == "GetLocked")
            {
                if (const MWWorld::Ptr loaded = loadedPtrFor(subjectKey); !loaded.isEmpty())
                    return value(loaded.getCellRef().isLocked() ? 1.0 : 0.0);
                if (const ESM4::Reference* reference = mWorld.mStore.get<ESM4::Reference>().search(subjectKey))
                    return value(reference->mIsLocked ? 1.0 : 0.0);
                return value(0.0);
            }
            if (function == "GetDisabled")
            {
                if (const MWWorld::Ptr loaded = loadedPtrFor(subjectKey); !loaded.isEmpty())
                    return value(loaded.getRefData().isEnabled() ? 0.0 : 1.0);
                if (const ESM4::ActorCharacter* actor = mWorld.mStore.get<ESM4::ActorCharacter>().search(subjectKey))
                    return value((actor->mFlags & ESM4::Rec_Disabled) != 0 ? 1.0 : 0.0);
                if (const ESM4::ActorCreature* actor = mWorld.mStore.get<ESM4::ActorCreature>().search(subjectKey))
                    return value((actor->mFlags & ESM4::Rec_Disabled) != 0 ? 1.0 : 0.0);
                if (const ESM4::Reference* reference = mWorld.mStore.get<ESM4::Reference>().search(subjectKey))
                    return value((reference->mFlags & ESM4::Rec_Disabled) != 0 ? 1.0 : 0.0);
                return value(0.0);
            }
            if (function == "GetGold")
            {
                if (const MWWorld::Ptr loaded = loadedPtrFor(subjectKey); !loaded.isEmpty())
                {
                    const MWWorld::Ptr gold = loaded.getClass().getContainerStore(loaded).search(
                        MWWorld::ContainerStore::sGoldId);
                    return value(gold.isEmpty() ? 0.0 : gold.getCellRef().getCount());
                }
                ESM::FormKey goldKey;
                for (const ESM4::MiscItem& item : mWorld.mStore.get<ESM4::MiscItem>())
                    if (containsInsensitive(item.mEditorId, "gold001"))
                    {
                        goldKey = resolver.toFormKey(item.mId);
                        break;
                    }
                return value(staticInventoryCount(goldKey));
            }
            if (function == "GetTalkedToPC")
            {
                if (const MWWorld::Ptr loaded = loadedPtrFor(subjectKey); !loaded.isEmpty())
                    return value(loaded.getClass().getCreatureStats(loaded).hasTalkedToPlayer() ? 1.0 : 0.0);
                return value(0.0);
            }
            if (function == "GetScriptVariable")
            {
                if (parameterKey.isNull())
                    return value(0.0);
                const MWWorld::OblivionScriptManager* scripts = mWorld.getOblivionScriptManager();
                if (scripts == nullptr)
                    return ESM4::ConditionValue::missing();
                const std::optional<double> local = scripts->scriptVariable(parameterKey, condition.mParameter2.mNumber);
                return local ? value(*local) : ESM4::ConditionValue::missing();
            }
            if (function == "GetQuestRunning" || function == "GetStage" || function == "GetStageDone")
            {
                const MWWorld::OblivionScriptManager* scripts = mWorld.getOblivionScriptManager();
                const ESM4::RuntimeQuestState* quest
                    = scripts == nullptr || parameterKey.isNull() ? nullptr : scripts->findQuestState(parameterKey);
                if (function == "GetQuestRunning")
                    return value(quest != nullptr && quest->mRunning ? 1.0 : 0.0);
                if (function == "GetStage")
                    return value(quest == nullptr ? 0.0 : quest->mStage);
                return value(quest != nullptr
                        && std::binary_search(quest->mCompletedStages.begin(), quest->mCompletedStages.end(),
                            condition.mParameter2.mNumber)
                    ? 1.0
                    : 0.0);
            }
            if (function == "GetQuestVariable")
            {
                if (parameterKey.isNull())
                    return value(0.0);
                const MWWorld::OblivionScriptManager* scripts = mWorld.getOblivionScriptManager();
                if (scripts == nullptr)
                    return ESM4::ConditionValue::missing();
                const std::optional<double> local = scripts->questVariable(parameterKey, condition.mParameter2.mNumber);
                return local ? value(*local) : ESM4::ConditionValue::missing();
            }
            if (function == "GetItemCount")
            {
                if (!parameter.mIsFormId)
                    return ESM4::ConditionValue::unsupported();
                if (const MWWorld::Ptr loaded = loadedPtrFor(subjectKey); !loaded.isEmpty())
                {
                    const MWWorld::Ptr item = loaded.getClass().getContainerStore(loaded).search(
                        ESM::RefId(parameter.mReference));
                    return value(item.isEmpty() ? 0.0 : item.getCellRef().getCount());
                }
                return value(staticInventoryCount(parameterKey));
            }
            if (function == "GetArmorRating")
            {
                if (const MWWorld::Ptr loaded = loadedPtrFor(subjectKey); !loaded.isEmpty())
                    return value(loaded.getClass().getArmorRating(loaded, false));
                return value(staticArmorRating());
            }
            if (function == "GetDetectionLevel" || function == "GetDetected")
            {
                const ESM::FormKey observerKey = subjectKey;
                const ESM::FormKey targetKey = snapshot.mTarget;
                if (targetKey.isNull())
                    return ESM4::ConditionValue::missing();
                const MWWorld::Ptr observer = loadedPtrFor(observerKey);
                const MWWorld::Ptr target = loadedPtrFor(targetKey);
                ESM4::DetectionResult result;
                if (!observer.isEmpty() && !target.isEmpty())
                    result = detection(observer, target);
                else
                {
                    const auto found = mDetectionVectors.find({ observerKey, targetKey });
                    if (found == mDetectionVectors.end())
                        return value(0.0);
                    result.mLevel = found->second.mScore;
                    result.mDetected = found->second.mDetected;
                }
                return value(function == "GetDetectionLevel" ? result.mLevel
                                                               : result.mDetected ? 1.0 : 0.0);
            }
            if (function == "GetDead" || function == "GetAttacked" || function == "GetIsAlerted")
            {
                if (const MWWorld::Ptr loaded = loadedPtrFor(subjectKey); !loaded.isEmpty())
                {
                    const CreatureStats& stats = loaded.getClass().getCreatureStats(loaded);
                    if (function == "GetDead")
                        return value(stats.isDead() ? 1.0 : 0.0);
                    if (function == "GetAttacked")
                        return value(stats.getAttacked() ? 1.0 : 0.0);
                    return value(stats.isAlarmed() ? 1.0 : 0.0);
                }
                return value(0.0);
            }
            if (function == "GetCurrentAIPackage" || function == "GetCurrentAIProcedure"
                || function == "GetIsCurrentPackage" || function == "GetRestrained" || function == "IsRidingHorse"
                || function == "GetSleeping" || function == "GetSitting")
            {
                const ESM4::RuntimeActorAiState* ai = subjectState();
                if (function == "GetCurrentAIPackage")
                    return value(ai == nullptr || ai->mPackageType == ESM4::AIPackageType::Unknown
                            ? -1.0
                            : static_cast<double>(static_cast<std::uint8_t>(ai->mPackageType)));
                if (function == "GetCurrentAIProcedure")
                    return value(ai == nullptr ? 0.0 : static_cast<double>(ai->mProcedure));
                if (function == "GetIsCurrentPackage")
                    return value(ai != nullptr && !parameterKey.isNull() && ai->mPackage == parameterKey ? 1.0 : 0.0);
                if (function == "GetRestrained")
                    return value(ai != nullptr && ai->mRestrained ? 1.0 : 0.0);
                const bool sleeping = ai != nullptr && ai->mPackageType == ESM4::AIPackageType::Sleep
                    && (ai->mPhase == ESM4::PackagePhase::Act || ai->mPhase == ESM4::PackagePhase::Wait);
                return value(function == "IsRidingHorse" ? ai != nullptr && !ai->mMount.isNull()
                                                           : sleeping
                    ? 1.0
                    : 0.0);
            }
            if (function == "GetInFaction")
            {
                if (const MWWorld::Ptr loaded = loadedPtrFor(subjectKey); !loaded.isEmpty())
                {
                    const std::optional<ESM::FormId> factionId = resolver.toFormId(parameterKey);
                    if (!factionId)
                        return value(0.0);
                    if (loaded.getClass().isNpc())
                        return value(loaded.getClass().getNpcStats(loaded).isInFaction(ESM::RefId(*factionId))
                                ? 1.0
                                : 0.0);
                    return value(loaded.getClass().getPrimaryFaction(loaded) == ESM::RefId(*factionId) ? 1.0 : 0.0);
                }
                const std::optional<ESM::FormId> factionId = resolver.toFormId(parameterKey);
                if (!factionId)
                    return value(0.0);
                const auto containsFaction = [factionId](const auto* actor) {
                    return actor != nullptr
                        && std::any_of(actor->mFactions.begin(), actor->mFactions.end(), [factionId](const auto& item) {
                               return ESM::FormId::fromUint32(item.faction) == *factionId;
                           });
                };
                return value(containsFaction(npc) || containsFaction(creature) ? 1.0 : 0.0);
            }
            if (function == "GetDeadCount")
            {
                if (parameterKey.isNull())
                    return value(0.0);
                std::int32_t count = 0;
                for (const auto& [key, liveActor] : mActors)
                {
                    if (liveActor.mState.mBase != parameterKey)
                        continue;
                    if (const MWWorld::Ptr loaded = loadedPtrFor(key); !loaded.isEmpty()
                        && loaded.getClass().getCreatureStats(loaded).isDead())
                        ++count;
                }
                return value(count);
            }
            if (function == "IsInInterior")
            {
                const ESM4::Cell* cell = mWorld.mStore.search<ESM4::Cell>(subjectLocation->mCell);
                return value(cell != nullptr && (cell->mCellFlags & ESM4::CELL_Interior) != 0 ? 1.0 : 0.0);
            }
            if (function == "IsInCombat")
            {
                const auto* combat = mWorld.getOblivionCombatService();
                return value(combat && combat->isInCombat(ESM4::runtimeReferenceKey(subjectKey)) ? 1.0 : 0.0);
            }
            if (function == "GetPCInFaction")
            {
                const MWWorld::Ptr player = mWorld.getPlayerPtr();
                if (parameterKey.isNull() || player.isEmpty() || !player.getClass().isNpc())
                    return value(0.0);
                const std::optional<ESM::FormId> factionId = resolver.toFormId(parameterKey);
                return value(factionId && player.getClass().getNpcStats(player).isInFaction(ESM::RefId(*factionId))
                        ? 1.0
                        : 0.0);
            }
            if (function == "IsRaining" || function == "IsSnowing" || function == "IsPleasant")
            {
                const std::uint32_t classification = mWorld.getCurrentWeather().mNativeClassification;
                const std::uint32_t flag = function == "IsRaining" ? ESM4::Weather::Classification_Rainy
                    : function == "IsSnowing" ? ESM4::Weather::Classification_Snow
                                               : ESM4::Weather::Classification_Pleasant;
                return value((classification & flag) != 0 ? 1.0 : 0.0);
            }
            if (function == "IsPlayerInJail")
                return value(mWorld.isPlayerInJail() ? 1.0 : 0.0);
            if (function == "GetEquipped")
            {
                if (!parameter.mIsFormId)
                    return ESM4::ConditionValue::unsupported();
                return value(staticIsEquipped(parameterKey) ? 1.0 : 0.0);
            }
            if (function == "GetPCExpelled")
            {
                const MWWorld::Ptr player = mWorld.getPlayerPtr();
                if (parameterKey.isNull() || player.isEmpty() || !player.getClass().isNpc())
                    return value(0.0);
                const std::optional<ESM::FormId> factionId = resolver.toFormId(parameterKey);
                return value(factionId && player.getClass().getNpcStats(player).getExpelled(ESM::RefId(*factionId))
                        ? 1.0
                        : 0.0);
            }
            if (function == "IsSpellTarget")
            {
                if (!parameter.mIsFormId)
                    return ESM4::ConditionValue::unsupported();
                if (const MWWorld::Ptr loaded = loadedPtrFor(subjectKey); !loaded.isEmpty())
                    return value(loaded.getClass().getCreatureStats(loaded).getActiveSpells().isSpellActive(
                                      ESM::RefId(parameter.mReference))
                            ? 1.0
                            : 0.0);
                return value(0.0);
            }
            if (function == "GetInCellParam")
            {
                const std::optional<StableLocation> parameterActor = locationFor(condition.mParameter2.mReferenceKey);
                return value(!parameterKey.isNull() && parameterActor
                        && subjectLocation->mCell == parameterKey && parameterActor->mCell == parameterKey
                    ? 1.0
                    : 0.0);
            }
            if (function == "GetPCFame")
            {
                const MWWorld::Ptr player = mWorld.getPlayerPtr();
                return value(player.isEmpty() || !player.getClass().isNpc()
                        ? 0.0
                        : player.getClass().getNpcStats(player).getReputation());
            }
            if (function == "IsCellOwner")
            {
                const ESM::FormKey factionKey = condition.mParameter2.mReferenceKey;
                const ESM4::Cell* cell = parameterKey.isNull()
                    ? nullptr
                    : mWorld.mStore.search<ESM4::Cell>(parameterKey);
                const std::optional<ESM::FormId> factionId = resolver.toFormId(factionKey);
                return value(cell != nullptr && factionId && subjectLocation->mCell == parameterKey
                        && cell->mOwner == *factionId
                    ? 1.0
                    : 0.0);
            }
            if (function == "GetInWorldspace")
            {
                if (!parameter.mIsFormId)
                    return value(0.0);
                const ESM4::Cell* cell = mWorld.mStore.search<ESM4::Cell>(subjectLocation->mCell);
                return value(cell != nullptr && cell->mParentKey == parameterKey ? 1.0 : 0.0);
            }
            if (function == "IsPlayersLastRiddenHorse")
            {
                const MWWorld::Ptr player = mWorld.getPlayerPtr();
                if (player.isEmpty())
                    return value(0.0);
                ESM::FormKey lastRidden;
                if (const auto found = mActors.find(actorKey(player)); found != mActors.end())
                    lastRidden = found->second.mLastRiddenHorse;
                const ESM::RefNum playerRef = player.getCellRef().getRefNum();
                if (lastRidden.isNull())
                    if (const ESM4::ActorCharacter* reference = mWorld.mStore.get<ESM4::ActorCharacter>().search(playerRef))
                    lastRidden = reference->mHorseKey;
                if (lastRidden.isNull())
                    if (const ESM4::ActorCreature* reference = mWorld.mStore.get<ESM4::ActorCreature>().search(playerRef))
                        lastRidden = reference->mHorseKey;
                return value(lastRidden == subjectKey && !lastRidden.isNull() ? 1.0 : 0.0);
            }
            if (function == "IsPlayerMovingIntoNewSpace")
                return value((mWorld.isPlayerTraveling() || mWorld.getPlayer().wasTeleported()) ? 1.0 : 0.0);
            if (function == "GetRandomPercent")
                return value(static_cast<double>(stableChoice(snapshot.mActor, snapshot.mSelectionGeneration) % 100));
            if (function == "IsChild")
                return value(0.0);
            return ESM4::ConditionValue::unsupported();
        };
        return context;
    }

    ESM4::CalendarInstant OblivionAiService::now() const
    {
        if (mSimulationNow)
            return *mSimulationNow;
        const ESM::EpochTimeStamp stamp = mWorld.mTimeManager->getEpochTimeStamp();
        return { stamp.mYear, stamp.mMonth, stamp.mDay, stamp.mGameHour };
    }

    MWWorld::Ptr OblivionAiService::ptrFor(const ESM::FormKey& key) const
    {
        if (key.isNull())
            return {};
        if (MWWorld::Ptr result = loadedPtrFor(key); !result.isEmpty())
            return result;
        const ESM::FormKeyResolver resolver(mWorld.mContentFiles);
        const std::optional<ESM::FormId> id = resolver.toFormId(key);
        if (!id)
            return {};
        const ESM::FormRecordMetadata* metadata = mWorld.mStore.getFormKeyIndex().resolve(key);
        if (metadata != nullptr && metadata->mParent)
            if (const std::optional<ESM::FormId> parent = resolver.toFormId(*metadata->mParent))
            {
                MWWorld::CellStore& cell = mWorld.mWorldModel.getCell(ESM::RefId(*parent));
                return cell.getPtr(ESM::RefId(*id));
            }
        return {};
    }

    std::optional<MWWorld::Ptr> OblivionAiService::findReference(const MWWorld::ConstPtr& actor,
        const ESM::FormKey& key, std::optional<std::uint32_t> objectType) const
    {
        if (actor.isEmpty() || (key.isNull() && !objectType))
            return std::nullopt;

        // Specific references and dynamic actors are already indexed by the
        // world model. Object IDs, however, name a base record and therefore
        // need the loaded reference scan below.
        if (!key.isNull())
        {
            const MWWorld::Ptr direct = ptrFor(key);
            if (!direct.isEmpty() && direct.getRefData().isEnabled()
                && (!objectType
                    || ESM4::packageObjectTypeMatches(*objectType,
                        static_cast<ESM::RecNameInts>(direct.getClass().getType()))))
                return direct;
        }

        std::vector<MWWorld::ConstPtr> candidates;
        const osg::Vec3f origin = actor.getRefData().getPosition().asVec3();
        const ESM::FormKeyResolver resolver(mWorld.mContentFiles);
        const auto baseOf = [&resolver](const MWWorld::ConstPtr& candidate) {
            const ESM::RefId baseId = candidate.getCellRef().getRefId();
            const ESM::FormId* id = baseId.getIf<ESM::FormId>();
            return id == nullptr ? ESM::FormKey{} : resolver.toFormKey(*id);
        };
        const auto matches = [&](const MWWorld::ConstPtr& candidate) {
            if (candidate.isEmpty() || !candidate.isInCell() || !candidate.getRefData().isEnabled())
                return false;
            if (objectType
                && !ESM4::packageObjectTypeMatches(*objectType,
                    static_cast<ESM::RecNameInts>(candidate.getClass().getType())))
                return false;
            return key.isNull() || baseOf(candidate) == key;
        };
        const auto collect = [&](const MWWorld::CellStore& cell) {
            cell.forEachConst([&](const MWWorld::ConstPtr& candidate) {
                if (matches(candidate))
                    candidates.push_back(candidate);
                return true;
            });
        };

        if (actor.isInCell())
        {
            collect(*actor.getCell());
            mWorld.mWorldModel.forEachLoadedCellStore([&](MWWorld::CellStore& cell) {
                if (&cell != actor.getCell())
                    collect(cell);
            });
        }
        else
            mWorld.mWorldModel.forEachLoadedCellStore([&](MWWorld::CellStore& cell) { collect(cell); });

        std::sort(candidates.begin(), candidates.end(), [&](const MWWorld::ConstPtr& left,
                                                                const MWWorld::ConstPtr& right) {
            const float leftDistance = distanceSquared(origin, left.getRefData().getPosition().asVec3());
            const float rightDistance = distanceSquared(origin, right.getRefData().getPosition().asVec3());
            if (leftDistance != rightDistance)
                return leftDistance < rightDistance;
            return left.getCellRef().getFormKey() < right.getCellRef().getFormKey();
        });
        if (candidates.empty())
            return std::nullopt;
        return mutablePtr(candidates.front());
    }

    ESM4::ConditionResult OblivionAiService::evaluateDialogueConditions(const MWWorld::Ptr& speaker,
        const MWWorld::Ptr& target, std::span<const ESM4::PackageCondition> conditions, bool diagnose) const
    {
        if (speaker.isEmpty() || !handles(speaker))
            return ESM4::ConditionResult::MissingContext;
        LiveActor contextActor;
        if (const auto existing = mActors.find(actorKey(speaker)); existing != mActors.end())
            contextActor.mState = existing->second.mState;
        contextActor.mState.mActor = actorKey(speaker);
        contextActor.mState.mBase = baseKey(speaker);
        contextActor.mState.mCell = cellKey(speaker);
        contextActor.mState.mTarget = target.isEmpty() ? ESM::FormKey{} : actorKey(target);
        auto context = conditionContext(speaker, contextActor);
        if (diagnose)
        {
            const auto resolve = context.mResolve;
            context.mResolve = [resolve](const auto& condition, auto subject) {
                const auto value = resolve(condition, subject);
                Log(Debug::Info) << "Native dialogue condition: function=" << condition.mFunction
                    << " subject=" << static_cast<int>(subject) << " actual=" << value.mValue
                    << " status=" << static_cast<int>(value.mStatus) << " expected=" << condition.mComparisonValue
                    << " parameter=" << condition.mParameter1.mReferenceKey.serialize();
                return value;
            };
        }
        return ESM4::evaluateConditions(conditions, context);
    }

    ESM4::ConditionEvaluationContext OblivionAiService::conditionContext(
        const MWWorld::Ptr& actor, const LiveActor& live) const
    {
        ESM4::ConditionEvaluationContext context;
        const MWWorld::Ptr target = ptrFor(live.mState.mTarget);
        context.mHasTarget = !target.isEmpty();
        // A reference run-on context is always syntactically available.  The
        // resolver still returns MissingContext when the encoded reference
        // cannot be resolved; advertising it here prevents a valid explicit
        // run-on condition from being rejected before its typed resolver is
        // called.
        context.mHasReference = true;
        context.mHasPlayer = !mWorld.getPlayerPtr().isEmpty();
        context.mResolveGlobal = [this](const ESM::FormKey& key) -> std::optional<double> {
            const ESM4::GlobalVariable* global = mWorld.mStore.search<ESM4::GlobalVariable>(key);
            if (global == nullptr || global->mEditorId.empty())
                return std::nullopt;
            return static_cast<double>(mWorld.mGlobalVariables[MWWorld::GlobalVariableName(global->mEditorId)].getFloat());
        };
        const auto resolveGlobal = context.mResolveGlobal;
        context.mResolve = [this, actor, target, &live, resolveGlobal](const ESM4::PackageCondition& condition,
                           ESM4::ConditionSubject subject) -> ESM4::ConditionValue {
            MWWorld::Ptr subjectPtr;
            switch (subject)
            {
                case ESM4::ConditionSubject::Self:
                    subjectPtr = actor;
                    break;
                case ESM4::ConditionSubject::Target:
                    subjectPtr = target;
                    break;
                case ESM4::ConditionSubject::Reference:
                    subjectPtr = ptrFor(condition.mRunOnReferenceKey);
                    break;
                case ESM4::ConditionSubject::Player:
                    subjectPtr = mWorld.getPlayerPtr();
                    break;
                case ESM4::ConditionSubject::CombatTarget:
                    // Combat target state belongs to M15.  The M14 context
                    // deliberately advertises no combat target, so this is
                    // normally unreachable; retaining the target adapter
                    // keeps the resolver total for explicit test contexts.
                    subjectPtr = target;
                    break;
                case ESM4::ConditionSubject::LinkedReference:
                    subjectPtr = ptrFor(condition.mRunOnReferenceKey);
                    break;
            }
            if (subjectPtr.isEmpty())
                return ESM4::ConditionValue::missing();

            const std::string_view function = ESM4::conditionFunctionName(condition.mFunction);
            const ESM4::ConditionParameter& parameter = condition.mParameter1;
            const ESM::FormKey parameterKey = parameter.mReferenceKey;
            const auto sameCell = [&]() {
                return subjectPtr.isInCell() && actor.isInCell() && subjectPtr.getCell() == actor.getCell();
            };
            const auto nativeStats = [&]() -> const CreatureStats& {
                return subjectPtr.getClass().getCreatureStats(subjectPtr);
            };
            const auto nativeNpcStats = [&]() -> const NpcStats* {
                return subjectPtr.getClass().isNpc() ? &subjectPtr.getClass().getNpcStats(subjectPtr) : nullptr;
            };
            const auto factionMatches = [&](const ESM::FormKey& factionKey) {
                if (factionKey.isNull())
                    return false;
                const ESM::RefId faction = ESM::RefId(factionKey.isContent()
                        ? ESM::FormKeyResolver(mWorld.mContentFiles).toFormId(factionKey).value_or(ESM::FormId{})
                        : ESM::FormId{});
                if (faction.empty())
                    return false;
                if (const NpcStats* stats = nativeNpcStats())
                    return stats->isInFaction(faction);
                return subjectPtr.getClass().getPrimaryFaction(subjectPtr) == faction;
            };
            const auto playerPtr = mWorld.getPlayerPtr();
            const auto playerNpcStats = [&]() -> const NpcStats* {
                return !playerPtr.isEmpty() && playerPtr.getClass().isNpc()
                    ? &playerPtr.getClass().getNpcStats(playerPtr)
                    : nullptr;
            };
            const auto actorValue = [&]() -> ESM4::ConditionValue {
                if (parameter.mNumber < 0 || parameter.mNumber >= 72)
                    return ESM4::ConditionValue::unsupported();
                try
                {
                    if (const auto native = mWorld.getOblivionScriptActorValue(
                            actorKey(subjectPtr), static_cast<std::uint8_t>(parameter.mNumber), false))
                        return value(*native);
                }
                catch (const std::invalid_argument&)
                {
                    return ESM4::ConditionValue::unsupported();
                }
                const std::int32_t index = parameter.mNumber;
                if (index >= 0 && index < ESM::Attribute::Length)
                    return value(nativeStats().getAttribute(ESM::Attribute::indexToRefId(index)).getModified());
                if (index == 8)
                    return value(nativeStats().getHealth().getCurrent());
                if (index == 9)
                    return value(nativeStats().getMagicka().getCurrent());
                if (index == 10)
                    return value(nativeStats().getFatigue().getCurrent());
                if (index == 11)
                    return value(subjectPtr.getClass().getEncumbrance(subjectPtr));
                const auto& skills = actorValueSkills();
                if (index >= 12 && index < 12 + static_cast<std::int32_t>(skills.size()))
                    return value(subjectPtr.getClass().getSkill(subjectPtr, skills[index - 12]));
                return ESM4::ConditionValue::unsupported();
            };
            const ESM4::RuntimeActorAiState* subjectAi = state(actorKey(subjectPtr));
            const ESM4::RuntimeActorAiState emptyAi;
            const ESM4::RuntimeActorAiState& currentAi = actorKey(subjectPtr) == live.mState.mActor
                ? live.mState
                : subjectAi != nullptr ? *subjectAi : emptyAi;
            const auto sleeping = [&]() {
                return currentAi.mPackageType == ESM4::AIPackageType::Sleep
                    && (currentAi.mPhase == ESM4::PackagePhase::Act
                        || currentAi.mPhase == ESM4::PackagePhase::Wait);
            };
            if (function == "GetCurrentTime")
                return value(now().mHour);
            if (function == "GetLocked")
                return value(subjectPtr.getCellRef().isLocked() ? 1.0 : 0.0);
            if (function == "GetActorValue")
                return actorValue();
            if (function == "GetDistance")
            {
                const MWWorld::Ptr comparison = parameterKey.isNull() ? target : ptrFor(parameterKey);
                return comparison.isEmpty() ? ESM4::ConditionValue::missing()
                                             : value((subjectPtr.getRefData().getPosition().asVec3()
                                                         - comparison.getRefData().getPosition().asVec3())
                                                         .length());
            }
            if (function == "GetLineOfSight")
            {
                const MWWorld::Ptr comparison = parameterKey.isNull() ? target : ptrFor(parameterKey);
                return comparison.isEmpty() ? ESM4::ConditionValue::missing()
                                             : value(mWorld.getLOS(subjectPtr, comparison) ? 1.0 : 0.0);
            }
            if (function == "GetIsInSameCell")
                return value(sameCell() ? 1.0 : 0.0);
            if (function == "GetDisabled")
                return value(subjectPtr.getRefData().isEnabled() ? 0.0 : 1.0);
            if (function == "GetDead")
                return value(nativeStats().isDead() ? 1.0 : 0.0);
            if (function == "GetIsCreature")
                return value(subjectPtr.getClass().getType() == ESM::REC_CREA4 ? 1.0 : 0.0);
            if (function == "GetGold")
            {
                const MWWorld::Ptr gold = subjectPtr.getClass().getContainerStore(subjectPtr).search(
                    MWWorld::ContainerStore::sGoldId);
                return value(gold.isEmpty() ? 0.0 : gold.getCellRef().getCount());
            }
            if (function == "GetTalkedToPC")
                return value(nativeStats().hasTalkedToPlayer() ? 1.0 : 0.0);
            if (function == "GetScriptVariable")
            {
                if (parameterKey.isNull())
                    return value(0.0);
                const MWWorld::OblivionScriptManager* scripts = mWorld.getOblivionScriptManager();
                if (scripts == nullptr)
                    return ESM4::ConditionValue::missing();
                const std::optional<double> local = scripts->scriptVariable(parameterKey, condition.mParameter2.mNumber);
                return local ? value(*local) : ESM4::ConditionValue::missing();
            }
            if (function == "GetQuestRunning" || function == "GetStage" || function == "GetStageDone")
            {
                const MWWorld::OblivionScriptManager* scripts = mWorld.getOblivionScriptManager();
                const ESM4::RuntimeQuestState* quest
                    = scripts == nullptr || parameterKey.isNull() ? nullptr : scripts->findQuestState(parameterKey);
                if (function == "GetQuestRunning")
                    return value(quest != nullptr && quest->mRunning ? 1.0 : 0.0);
                if (function == "GetStage")
                    return value(quest == nullptr ? 0.0 : quest->mStage);
                return value(quest != nullptr
                        && std::binary_search(quest->mCompletedStages.begin(), quest->mCompletedStages.end(),
                            condition.mParameter2.mNumber)
                    ? 1.0
                    : 0.0);
            }
            if (function == "GetItemCount")
            {
                if (!parameter.mIsFormId)
                    return ESM4::ConditionValue::unsupported();
                const MWWorld::Ptr item = subjectPtr.getClass().getContainerStore(subjectPtr).search(
                    ESM::RefId(parameter.mReference));
                return value(item.isEmpty() ? 0.0 : item.getCellRef().getCount());
            }
            if (function == "GetLevel")
                return value(nativeStats().getLevel());
            if (function == "GetArmorRating")
                return value(subjectPtr.getClass().getArmorRating(subjectPtr, false));
            if (function == "GetCurrentAIPackage")
                return value(currentAi.mPackageType == ESM4::AIPackageType::Unknown
                        ? -1.0
                        : static_cast<double>(static_cast<std::uint8_t>(currentAi.mPackageType)));
            if (function == "GetCurrentAIProcedure")
                return value(static_cast<double>(currentAi.mProcedure));
            if (function == "GetIsCurrentPackage")
                return value(!parameterKey.isNull() && currentAi.mPackage == parameterKey ? 1.0 : 0.0);
            if (function == "GetDetectionLevel")
            {
                if (target.isEmpty())
                    return ESM4::ConditionValue::missing();
                return value(detection(subjectPtr, target).mLevel);
            }
            if (function == "GetDetected")
            {
                if (target.isEmpty())
                    return ESM4::ConditionValue::missing();
                return value(detection(subjectPtr, target).mDetected ? 1.0 : 0.0);
            }
            if (function == "GetSleeping")
                return value(sleeping() ? 1.0 : 0.0);
            if (function == "GetAttacked")
                return value(nativeStats().getAttacked() ? 1.0 : 0.0);
            if (function == "GetInFaction")
                return value(factionMatches(parameterKey) ? 1.0 : 0.0);
            if (function == "GetQuestVariable")
            {
                if (parameterKey.isNull())
                    return value(0.0);
                const MWWorld::OblivionScriptManager* scripts = mWorld.getOblivionScriptManager();
                if (scripts == nullptr)
                    return ESM4::ConditionValue::missing();
                const std::optional<double> local = scripts->questVariable(parameterKey, condition.mParameter2.mNumber);
                return local ? value(*local) : ESM4::ConditionValue::missing();
            }
            if (function == "GetDeadCount")
            {
                if (parameterKey.isNull())
                    return value(0.0);
                std::int32_t count = 0;
                mWorld.mWorldModel.forEachLoadedCellStore([&](MWWorld::CellStore& cell) {
                    cell.forEachConst([&](const MWWorld::ConstPtr& candidate) {
                        if (!nativeActor(candidate) || baseKey(candidate) != parameterKey)
                            return true;
                        const MWWorld::Ptr mutableCandidate(
                            const_cast<MWWorld::LiveCellRefBase*>(candidate.mRef),
                            const_cast<MWWorld::CellStore*>(candidate.mCell));
                        if (mutableCandidate.getClass().getCreatureStats(mutableCandidate).isDead())
                            ++count;
                        return true;
                    });
                });
                return value(count);
            }
            if (function == "GetIsAlerted")
                return value(nativeStats().isAlarmed() ? 1.0 : 0.0);
            if (function == "GetRestrained")
                return value(currentAi.mRestrained ? 1.0 : 0.0);
            if (function == "IsMoving")
            {
                const Movement& movement = subjectPtr.getClass().getMovementSettings(subjectPtr);
                return value(std::hypot(movement.mPosition[0], movement.mPosition[1]) > 0.01 ? 1.0 : 0.0);
            }
            if (function == "IsSneaking")
                return value(nativeStats().getMovementFlag(CreatureStats::Flag_Sneak) ? 1.0 : 0.0);
            if (function == "IsRunning")
                return value(nativeStats().getMovementFlag(CreatureStats::Flag_Run) ? 1.0 : 0.0);
            if (function == "IsInCombat")
            {
                const auto* combat = mWorld.getOblivionCombatService();
                return value(combat && combat->isInCombat(actorKey(subjectPtr)) ? 1.0 : 0.0);
            }
            if (function == "IsRaining")
                return value((mWorld.getCurrentWeather().mNativeClassification & ESM4::Weather::Classification_Rainy)
                        != 0
                    ? 1.0
                    : 0.0);
            if (function == "IsSnowing")
                return value((mWorld.getCurrentWeather().mNativeClassification & ESM4::Weather::Classification_Snow)
                        != 0
                    ? 1.0
                    : 0.0);
            if (function == "IsInInterior")
                return value(subjectPtr.isInCell() && !subjectPtr.getCell()->isExterior() ? 1.0 : 0.0);
            if (function == "IsRidingHorse")
                return value(!currentAi.mMount.isNull() ? 1.0 : 0.0);
            if (function == "IsActor")
                return value(subjectPtr.getClass().isActor() ? 1.0 : 0.0);
            if (function == "IsEssential")
                return value(subjectPtr.getClass().isEssential(subjectPtr) ? 1.0 : 0.0);
            if (function == "GetPCInFaction")
            {
                if (parameterKey.isNull() || playerNpcStats() == nullptr)
                    return value(0.0);
                const std::optional<ESM::FormId> factionId
                    = ESM::FormKeyResolver(mWorld.mContentFiles).toFormId(parameterKey);
                return value(factionId && playerNpcStats()->isInFaction(ESM::RefId(*factionId)) ? 1.0 : 0.0);
            }
            if (function == "GetIsID")
                return value(!parameterKey.isNull() && baseKey(subjectPtr) == parameterKey ? 1.0 : 0.0);
            if (function == "GetIsRace" || function == "GetIsSex")
            {
                // The TES4 player intentionally uses the shared ESM::NPC
                // mechanics proxy. Its current race/sex are authoritative,
                // including character-generation edits and reloads.
                if (subjectPtr == mWorld.getPlayerPtr())
                {
                    const ESM::NPC& player = *subjectPtr.get<ESM::NPC>()->mBase;
                    if (function == "GetIsSex")
                        return value(parameter.mNumber == (player.isMale() ? 0 : 1) ? 1.0 : 0.0);
                    const ESM::FormId* race = player.mRace.getIf<ESM::FormId>();
                    return value(race != nullptr && !parameterKey.isNull()
                        && ESM::FormKeyResolver(mWorld.mContentFiles).toFormKey(*race) == parameterKey ? 1.0 : 0.0);
                }
                if (subjectPtr.getClass().getType() != ESM::REC_NPC_4)
                    return value(0.0);
                const ESM4::Npc& npc = *subjectPtr.get<ESM4::Npc>()->mBase;
                if (function == "GetIsRace")
                    return value(!parameterKey.isNull()
                        && ESM::FormKeyResolver(mWorld.mContentFiles).toFormKey(npc.mRace) == parameterKey ? 1.0 : 0.0);
                const bool female = (npc.mBaseConfig.tes4.flags & ESM4::Npc::TES4_Female) != 0;
                return value(parameter.mNumber == (female ? 1 : 0) ? 1.0 : 0.0);
            }
            if (function == "GetIsReference")
                return value(!parameterKey.isNull()
                    && actorKey(subjectPtr) == ESM4::runtimeReferenceKey(parameterKey) ? 1.0 : 0.0);
            if (function == "GetInCell")
                return value(!parameterKey.isNull() && cellKey(subjectPtr) == parameterKey ? 1.0 : 0.0);
            if (function == "GetDayofWeek")
                return value(ESM4::calendarDayOfWeek(now()));
            if (function == "IsPlayerInJail")
                return value(mWorld.isPlayerInJail() ? 1.0 : 0.0);
            if (function == "GetEquipped")
            {
                if (!parameter.mIsFormId)
                    return ESM4::ConditionValue::unsupported();
                return value(subjectPtr.getClass().getInventoryStore(subjectPtr).isEquipped(
                                  ESM::RefId(parameter.mReference))
                        ? 1.0
                        : 0.0);
            }
            if (function == "GetPCExpelled")
            {
                if (parameterKey.isNull() || playerNpcStats() == nullptr)
                    return value(0.0);
                const std::optional<ESM::FormId> factionId
                    = ESM::FormKeyResolver(mWorld.mContentFiles).toFormId(parameterKey);
                return value(factionId && playerNpcStats()->getExpelled(ESM::RefId(*factionId)) ? 1.0 : 0.0);
            }
            if (function == "IsSpellTarget")
            {
                if (!parameter.mIsFormId)
                    return ESM4::ConditionValue::unsupported();
                // Active spell identity is already maintained by the shared
                // actor-effects subsystem; M16 owns casting, not this query.
                return value(nativeStats().getActiveSpells().isSpellActive(ESM::RefId(parameter.mReference))
                        ? 1.0
                        : 0.0);
            }
            if (function == "GetInCellParam")
            {
                const ESM::FormKey parameterActorKey = condition.mParameter2.mReferenceKey;
                const MWWorld::Ptr parameterActor = ptrFor(parameterActorKey);
                return value(!parameterKey.isNull() && !parameterActor.isEmpty()
                        && cellKey(subjectPtr) == parameterKey && cellKey(parameterActor) == parameterKey
                    ? 1.0
                    : 0.0);
            }
            if (function == "GetPCFame")
                return value(playerNpcStats() == nullptr ? 0.0 : playerNpcStats()->getReputation());
            if (function == "IsPleasant")
                return value((mWorld.getCurrentWeather().mNativeClassification & ESM4::Weather::Classification_Pleasant)
                        != 0
                    ? 1.0
                    : 0.0);
            if (function == "IsCellOwner")
            {
                const ESM::FormKey factionKey = condition.mParameter2.mReferenceKey;
                const ESM::FormKey currentCell = cellKey(subjectPtr);
                const ESM4::Cell* cell = parameterKey.isNull() ? nullptr : mWorld.mStore.search<ESM4::Cell>(parameterKey);
                if (cell == nullptr || factionKey.isNull() || currentCell != parameterKey)
                    return value(0.0);
                const std::optional<ESM::FormId> factionId
                    = ESM::FormKeyResolver(mWorld.mContentFiles).toFormId(factionKey);
                return value(factionId && cell->mOwner == *factionId ? 1.0 : 0.0);
            }
            if (function == "GetInWorldspace")
            {
                if (!parameter.mIsFormId || !subjectPtr.isInCell())
                    return value(0.0);
                return value(subjectPtr.getCell()->getCell()->getWorldSpace() == ESM::RefId(parameter.mReference)
                        ? 1.0
                        : 0.0);
            }
            if (function == "IsPlayersLastRiddenHorse")
            {
                if (playerPtr.isEmpty())
                    return value(0.0);
                const ESM::RefNum playerRef = playerPtr.getCellRef().getRefNum();
                const ESM::FormKey horseKey = actorKey(subjectPtr);
                ESM::FormKey lastRidden;
                if (const auto found = mActors.find(actorKey(playerPtr)); found != mActors.end())
                    lastRidden = found->second.mLastRiddenHorse;
                if (!lastRidden.isNull())
                    return value(lastRidden == horseKey ? 1.0 : 0.0);
                if (const ESM4::ActorCharacter* reference = mWorld.mStore.get<ESM4::ActorCharacter>().search(playerRef))
                    return value(reference->mHorseKey == horseKey ? 1.0 : 0.0);
                if (const ESM4::ActorCreature* reference = mWorld.mStore.get<ESM4::ActorCreature>().search(playerRef))
                    return value(reference->mHorseKey == horseKey ? 1.0 : 0.0);
                return value(0.0);
            }
            if (function == "IsPlayerMovingIntoNewSpace")
                return value((mWorld.isPlayerTraveling() || mWorld.getPlayer().wasTeleported()) ? 1.0 : 0.0);
            if (function == "IsChild")
                return value(0.0);
            if (function == "GetSitting")
                return value(sleeping() ? 1.0 : 0.0);
            if (function == "GetGlobalValue")
            {
                const auto global = resolveGlobal(parameterKey);
                return global ? value(*global) : ESM4::ConditionValue::missing();
            }
            if (function == "GetRandomPercent")
                return value(static_cast<double>(stableChoice(actorKey(actor), live.mState.mSelectionGeneration) % 100));
            return ESM4::ConditionValue::unsupported();
        };
        return context;
    }

    ESM4::PackageSelection OblivionAiService::selectUnloaded(LiveActor& live, bool restart)
    {
        ESM4::PackageSelectionRequest request;
        request.mNow = now();
        request.mBasePackages = basePackages(live.mState.mBase);
        if (live.mScriptPackage)
            request.mScriptPackage = live.mScriptPackage;
        request.mConditionContext = unloadedConditionContext(live);
        request.mEvaluationGeneration = mNextEvaluationGeneration++;
        if (mNextEvaluationGeneration == 0)
            mNextEvaluationGeneration = 1;

        const ESM4::PackageSelection result = ESM4::selectPackage(request);
        const bool sameSelection = !restart
            && oblivionPackageSelectionMatches(live.mState, live.mSelectedWindow, result, request.mNow);
        if (sameSelection)
        {
            live.mSelectionCheckTimer = 1.0f;
            live.mNeedsSelection = false;
            return result;
        }

        live.mState.mSelectionGeneration = result.mEvaluationGeneration;
        live.mState.mConditionResult = result.mConditionResult;
        live.mState.mScheduleWindow = result.mWindow;
        live.mState.mSource = result.mSource;
        live.mState.mPackage = result.mPackage;
        live.mState.mScriptPackage = live.mScriptPackage ? live.mScriptPackage->mKey : ESM::FormKey{};
        live.mState.mPackageType = result.mType;
        live.mState.mProcedure = ESM4::packageProcedure(result.mType);
        live.mState.mListIndex = static_cast<std::uint32_t>(result.mListIndex);
        setPhase(live, result.hasPackage() ? ESM4::PackagePhase::Select : ESM4::PackagePhase::Wait);
        live.mState.mBoundary = ESM4::PhaseBoundary::None;
        live.mState.mTarget = {};
        live.mState.mTargetBase = {};
        live.mState.mCompanionGroup = {};
        live.mState.mCompanionSideWith = {};
        live.mState.mPathgrid = {};
        live.mState.mPathNode = 0;
        live.mState.mDoor = {};
        live.mState.mDestinationCell = {};
        live.mState.mDestinationPosition = {};
        live.mState.mHasDestination = false;
        live.mState.mActionItem = {};
        live.mState.mActionTimer = 0.f;
        live.mState.mDurationRemaining = result.mWindow
            ? static_cast<float>(std::max(0.0, ESM4::calendarHoursUntil(request.mNow, result.mWindow->mEnd)))
            : 0.f;
        live.mState.mNoProgressSeconds = 0.f;
        live.mState.mRepathAttempts = 0;
        live.mState.mActionReserved = false;
        live.mState.mInterruptionReason.clear();
        live.mRoute.clear();
        live.mContinuousRoute.clear();
        live.mRouteCursor = 0;
        live.mContinuousRouteCursor = 0;
        live.mForeignRouteTarget.reset();
        live.mRouteDoor.reset();
        live.mDestination.reset();
        live.mDestinationCell = {};
        live.mSelectedWindow = result.mWindow;
        live.mSelectionCheckTimer = 1.0f;
        live.mPendingDoor = false;
        live.mNeedsSelection = false;

        std::ostringstream selection;
        selection << "source=" << static_cast<unsigned>(result.mSource) << " type="
                  << static_cast<unsigned>(result.mType) << " list_index=" << result.mListIndex << " low-process=true";
        logEvent("selection", live, selection.str());
        return result;
    }

    std::optional<osg::Vec3f> OblivionAiService::resolveUnloadedDestination(
        const ESM4::AIPackage& record, LiveActor& live) const
    {
        using StableLocation = UnloadedLocation;

        const ESM::FormKeyResolver resolver(mWorld.mContentFiles);
        const auto ownerKey = [resolver](const ESM::RefId& owner) {
            if (const ESM::FormId* id = owner.getIf<ESM::FormId>())
                return resolver.toFormKey(*id);
            return ESM::FormKey{};
        };
        const auto locationFor = [this, ownerKey](const ESM::FormKey& key)
            -> std::optional<StableLocation> {
            if (key.isNull())
                return std::nullopt;
            if (const auto found = mActors.find(key); found != mActors.end())
                return StableLocation{ key, found->second.mState.mBase,
                    found->second.mState.mCell, found->second.mState.mLastValidPosition.asVec3(),
                    found->second.mState.mBase.isNull() ? ESM::REC_REFR4
                                                        : mWorld.mStore.search<ESM4::Creature>(found->second.mState.mBase)
                            != nullptr
                        ? ESM::REC_ACRE4
                        : ESM::REC_ACHR4,
                    true, false, {}, {}, {}, {} };
            if (const MWWorld::Ptr loaded = loadedPtrFor(key); !loaded.isEmpty())
                return StableLocation{ actorKey(loaded), baseKey(loaded), cellKey(loaded), loaded.getRefData().getPosition().asVec3(),
                    static_cast<ESM::RecNameInts>(loaded.getClass().getType()), loaded.getRefData().isEnabled(),
                    loaded.getCellRef().isLocked(), ownerKey(loaded.getCellRef().getOwner()), {}, {}, {} };
            const auto found = mUnloadedLocationByReference.find(key);
            if (found != mUnloadedLocationByReference.end())
                return mUnloadedLocations[found->second];
            return std::nullopt;
        };
        const auto findObject = [&](const ESM::FormKey& base, std::optional<std::uint32_t> objectType)
            -> std::optional<StableLocation> {
            const auto matches = [&](const StableLocation& candidate) {
                return candidate.mEnabled && (base.isNull() || candidate.mBase == base)
                    && (!objectType || ESM4::packageObjectTypeMatches(
                           *objectType, candidate.mType));
            };
            const osg::Vec3f origin = live.mState.mLastValidPosition.asVec3();
            std::optional<StableLocation> best;
            const auto consider = [&](std::size_t index) {
                StableLocation candidate = mUnloadedLocations[index];
                // Native actors may have an abstract low-process position
                // newer than their placement record. Use that position while
                // retaining the indexed record for static references.
                if (candidate.mType == ESM::REC_ACHR4 || candidate.mType == ESM::REC_ACRE4)
                {
                    const auto found = mActors.find(candidate.mReference);
                    if (found != mActors.end())
                    {
                        candidate.mBase = found->second.mState.mBase;
                        candidate.mCell = found->second.mState.mCell;
                        candidate.mPosition = found->second.mState.mLastValidPosition.asVec3();
                        candidate.mEnabled = true;
                        candidate.mLocked = false;
                    }
                }
                if (!matches(candidate))
                    return;
                if (!best)
                {
                    best = std::move(candidate);
                    return;
                }
                const float candidateDistance = distanceSquared(origin, candidate.mPosition);
                const float bestDistance = distanceSquared(origin, best->mPosition);
                if (candidateDistance < bestDistance
                    || (candidateDistance == bestDistance && candidate.mReference < best->mReference))
                    best = std::move(candidate);
            };

            // A null ObjectId/near-reference has no identity to resolve.
            // Returning no candidate is both deterministic and safe; picking
            // an arbitrary nearest object would be a semantic error and would
            // turn every such package into an O(number of placed references)
            // search in the low-process scheduler.
            if (base.isNull() && !objectType)
                return std::nullopt;
            if (!base.isNull())
            {
                const auto found = mUnloadedLocationsByBase.find(base);
                if (found != mUnloadedLocationsByBase.end())
                    for (const std::size_t index : found->second)
                        consider(index);
            }
            else if (objectType)
            {
                const auto found = mUnloadedLocationsByType.find(*objectType);
                if (found != mUnloadedLocationsByType.end())
                    for (const std::size_t index : found->second)
                        consider(index);
            }
            else
            {
                for (std::size_t index = 0; index < mUnloadedLocations.size(); ++index)
                    consider(index);
            }
            return best;
        };
        const auto targetFor = [&](const ESM4::PackageTarget& target) -> std::optional<StableLocation> {
            switch (target.mKind)
            {
                case ESM4::PackageTargetKind::SpecificReference:
                case ESM4::PackageTargetKind::LinkedReference:
                    return locationFor(target.mReferenceKey);
                case ESM4::PackageTargetKind::ObjectId:
                    return findObject(target.mReferenceKey, std::nullopt);
                case ESM4::PackageTargetKind::ObjectType:
                    return findObject({}, target.mObjectType);
                case ESM4::PackageTargetKind::None:
                case ESM4::PackageTargetKind::Unknown:
                    return std::nullopt;
            }
            return std::nullopt;
        };
        const auto actorOwns = [&](const ESM::FormKey& owner) {
            if (owner.isNull())
                return true;
            if (owner == live.mState.mActor || owner == live.mState.mBase)
                return true;
            if (const MWWorld::Ptr actor = loadedPtrFor(live.mState.mActor); !actor.isEmpty())
            {
                if (actor.getClass().isNpc())
                {
                    if (actor.getClass().getNpcStats(actor).isInFaction(
                            ESM::RefId(resolver.toFormId(owner).value_or(ESM::FormId{}))))
                        return true;
                }
                else if (actor.getClass().getPrimaryFaction(actor)
                    == ESM::RefId(resolver.toFormId(owner).value_or(ESM::FormId{})))
                    return true;
            }
            const ESM::FormId ownerId = resolver.toFormId(owner).value_or(ESM::FormId{});
            if (const ESM4::Npc* npc = mWorld.mStore.search<ESM4::Npc>(live.mState.mBase))
                return std::any_of(npc->mFactions.begin(), npc->mFactions.end(), [ownerId](const auto& faction) {
                    return ESM::FormId::fromUint32(faction.faction) == ownerId;
                });
            if (const ESM4::Creature* creature = mWorld.mStore.search<ESM4::Creature>(live.mState.mBase))
                return std::any_of(creature->mFactions.begin(), creature->mFactions.end(), [ownerId](const auto& faction) {
                    return ESM::FormId::fromUint32(faction.faction) == ownerId;
                });
            return false;
        };
        const auto usableFurniture = [&](const StableLocation& furniture) {
            return furniture.mType == ESM::REC_FURN4 && !furniture.mLocked && actorOwns(furniture.mOwner);
        };

        const bool followLike = record.mPackageType == ESM4::AIPackageType::Follow
            || record.mPackageType == ESM4::AIPackageType::Accompany
            || record.mPackageType == ESM4::AIPackageType::Pursue;
        const bool escort = record.mPackageType == ESM4::AIPackageType::Escort;
        const bool flee = record.mPackageType == ESM4::AIPackageType::FleeNotCombat;
        const bool targetFallback = record.mPackageType == ESM4::AIPackageType::Find
            || record.mPackageType == ESM4::AIPackageType::Ambush;
        std::optional<StableLocation> target;
        if (followLike || escort || flee || targetFallback)
            target = targetFor(record.mTargetData);
        if (!target && !live.mState.mTarget.isNull())
            target = locationFor(live.mState.mTarget);
        if (target)
        {
            live.mState.mTarget = target->mReference;
            live.mState.mTargetBase = target->mBase;
            if (record.mPackageType == ESM4::AIPackageType::Follow
                || record.mPackageType == ESM4::AIPackageType::Escort
                || record.mPackageType == ESM4::AIPackageType::Accompany)
                live.mState.mCompanionGroup = target->mReference;
        }

        const osg::Vec3f actorPosition = live.mState.mLastValidPosition.asVec3();
        const auto setCurrentDestination = [&]() {
            live.mDestinationCell = live.mState.mCell;
            return actorPosition;
        };
        const auto graphDestination = [&](const ESM::FormKey& cell, std::uint64_t generation)
            -> std::optional<osg::Vec3f> {
            const ESM4::PathgridService& service = mWorld.mStore.getOblivionPathgridService();
            const ESM4::PathgridGraph* graph = service.graphForCell(cell);
            if (graph == nullptr || graph->nodeCount() == 0)
                return std::nullopt;
            std::vector<std::uint32_t> nodes;
            for (std::uint32_t node = 0; node < graph->nodeCount(); ++node)
                if (graph->isEnabled(node))
                    nodes.push_back(node);
            if (nodes.empty())
                return std::nullopt;
            const ESM4::PathgridPoint point = graph->worldPoint(
                nodes[stableChoice(live.mState.mActor, generation) % nodes.size()]);
            live.mDestinationCell = cell;
            return osg::Vec3f(point.mX, point.mY, point.mZ);
        };

        if (followLike)
        {
            if (!target)
                return std::nullopt;
            live.mDestinationCell = target->mCell;
            osg::Vec3f offset = actorPosition - target->mPosition;
            offset.z() = 0.f;
            if (offset.length2() <= 1.f)
                offset = osg::Vec3f(0.f, -1.f, 0.f);
            else
                offset.normalize();
            const float requested = static_cast<float>(record.mTargetData.mDistance);
            const float followDistance = requested > 0.f ? requested : 128.f;
            osg::Vec3f destination = target->mPosition + offset * followDistance;
            destination.z() = target->mPosition.z();
            return destination;
        }

        if (flee)
        {
            if (!target)
                return std::nullopt;
            const ESM4::PathgridService& service = mWorld.mStore.getOblivionPathgridService();
            const ESM4::PathgridGraph* graph = service.graphForCell(live.mState.mCell);
            if (graph == nullptr)
                return std::nullopt;
            const auto start = service.nearestEnabledNode(graph->pathgridKey(),
                { actorPosition.x(), actorPosition.y(), actorPosition.z() }, 2048.f);
            if (!start)
                return std::nullopt;
            std::optional<ESM4::PathgridNodeKey> best;
            float bestDistance = -1.f;
            for (const ESM4::PathgridNodeKey& node : service.reachableComponent(*start))
            {
                const ESM4::PathgridGraph* nodeGraph = service.graph(node.mPathgrid);
                if (nodeGraph == nullptr)
                    continue;
                const ESM4::PathgridPoint point = nodeGraph->worldPoint(node.mNode);
                const float distanceFromThreat = distanceSquared(
                    { point.mX, point.mY, point.mZ }, target->mPosition);
                if (!best || distanceFromThreat > bestDistance
                    || (distanceFromThreat == bestDistance && node < *best))
                {
                    best = node;
                    bestDistance = distanceFromThreat;
                }
            }
            if (!best)
                return std::nullopt;
            const ESM4::PathgridGraph* bestGraph = service.graph(best->mPathgrid);
            const ESM4::PathgridPoint point = bestGraph->worldPoint(best->mNode);
            live.mDestinationCell = bestGraph->cellKey();
            return osg::Vec3f(point.mX, point.mY, point.mZ);
        }

        if (escort && record.mLocationData.mKind == ESM4::PackageLocationKind::None)
        {
            if (!target)
                return std::nullopt;
            live.mDestinationCell = target->mCell;
            return target->mPosition;
        }

        if (record.mPackageType == ESM4::AIPackageType::Wander)
        {
            ESM::FormKey centreCell = live.mState.mCell;
            osg::Vec3f origin = actorPosition;
            if (record.mLocationData.mKind == ESM4::PackageLocationKind::InCell)
                centreCell = record.mLocationData.mReferenceKey;
            else if (record.mLocationData.mKind == ESM4::PackageLocationKind::NearReference
                || (record.mLocationData.mKind == ESM4::PackageLocationKind::EditorLocation
                    && !record.mLocationData.mReferenceKey.isNull())
                || record.mLocationData.mKind == ESM4::PackageLocationKind::ObjectId
                || record.mLocationData.mKind == ESM4::PackageLocationKind::ObjectType)
            {
                const auto reference = record.mLocationData.mKind == ESM4::PackageLocationKind::ObjectType
                    ? findObject({}, record.mLocationData.mObjectType)
                    : (record.mLocationData.mKind == ESM4::PackageLocationKind::NearReference
                            || record.mLocationData.mKind == ESM4::PackageLocationKind::EditorLocation)
                        ? locationFor(record.mLocationData.mReferenceKey)
                        : findObject(record.mLocationData.mReferenceKey, std::nullopt);
                if (!reference)
                    return std::nullopt;
                centreCell = reference->mCell;
                origin = reference->mPosition;
            }
            const ESM4::PathgridGraph* graph = mWorld.mStore.getOblivionPathgridService().graphForCell(centreCell);
            if (graph == nullptr)
            {
                const bool implicitEditorLocation = record.mLocationData.mKind == ESM4::PackageLocationKind::EditorLocation
                    && record.mLocationData.mReferenceKey.isNull();
                if (record.mLocationData.mKind == ESM4::PackageLocationKind::None
                    || record.mLocationData.mKind == ESM4::PackageLocationKind::CurrentLocation
                    || implicitEditorLocation
                    || (record.mLocationData.mKind == ESM4::PackageLocationKind::InCell
                        && record.mLocationData.mReferenceKey == live.mState.mCell))
                    return setCurrentDestination();
                return std::nullopt;
            }
            std::vector<std::uint32_t> candidates;
            const float radius = static_cast<float>(std::max(0, record.mLocationData.mRadius));
            for (std::uint32_t node = 0; node < graph->nodeCount(); ++node)
            {
                if (!graph->isEnabled(node))
                    continue;
                const ESM4::PathgridPoint point = graph->worldPoint(node);
                if (radius <= 0.f || distanceSquared(origin, { point.mX, point.mY, point.mZ }) <= radius * radius)
                    candidates.push_back(node);
            }
            if (candidates.empty())
            {
                // Some stock EditorLocation Wander packages use a small
                // radius around an actor's editor placement. If that cell's
                // graph has no node inside the radius, the native result is
                // an idle current placement, not an unresolved reference.
                if (record.mLocationData.mKind == ESM4::PackageLocationKind::EditorLocation
                    && record.mLocationData.mReferenceKey.isNull())
                    return setCurrentDestination();
                return std::nullopt;
            }
            const ESM4::PathgridPoint point = graph->worldPoint(
                candidates[stableChoice(live.mState.mActor, live.mState.mSelectionGeneration) % candidates.size()]);
            live.mDestinationCell = centreCell;
            return osg::Vec3f(point.mX, point.mY, point.mZ);
        }

        const ESM4::PackageLocation& location = record.mLocationData;
        switch (location.mKind)
        {
            case ESM4::PackageLocationKind::CurrentLocation:
                if (record.mPackageType == ESM4::AIPackageType::Sleep)
                {
                    const auto furniture = findObject({}, 12);
                    if (!furniture || !usableFurniture(*furniture))
                        return std::nullopt;
                    live.mState.mTarget = furniture->mReference;
                    live.mState.mTargetBase = furniture->mBase;
                    live.mDestinationCell = furniture->mCell;
                    return furniture->mPosition;
                }
                return setCurrentDestination();
            case ESM4::PackageLocationKind::InCell:
                if (location.mReferenceKey == live.mState.mCell)
                    return setCurrentDestination();
                return graphDestination(location.mReferenceKey, live.mState.mSelectionGeneration);
            case ESM4::PackageLocationKind::AtPackageLocation:
                if (live.mState.mHasDestination)
                {
                    live.mDestinationCell = live.mState.mDestinationCell;
                    return osg::Vec3f(live.mState.mDestinationPosition.pos[0],
                        live.mState.mDestinationPosition.pos[1], live.mState.mDestinationPosition.pos[2]);
                }
                return std::nullopt;
            case ESM4::PackageLocationKind::NearReference:
            case ESM4::PackageLocationKind::EditorLocation:
            case ESM4::PackageLocationKind::ObjectId:
            case ESM4::PackageLocationKind::ObjectType:
            {
                // A null EditorLocation is the package's implicit editor
                // location. For a low-process actor its current abstract
                // placement is the only stable representation available.
                if (location.mKind == ESM4::PackageLocationKind::EditorLocation
                    && location.mReferenceKey.isNull())
                    return setCurrentDestination();
                const auto reference = location.mKind == ESM4::PackageLocationKind::ObjectType
                    ? findObject({}, location.mObjectType)
                    : (location.mKind == ESM4::PackageLocationKind::NearReference
                            || location.mKind == ESM4::PackageLocationKind::EditorLocation)
                        ? locationFor(location.mReferenceKey)
                        : findObject(location.mReferenceKey, std::nullopt);
                if (reference)
                {
                    if (record.mPackageType == ESM4::AIPackageType::Sleep && !usableFurniture(*reference))
                        return std::nullopt;
                    live.mDestinationCell = reference->mCell;
                    if (record.mPackageType == ESM4::AIPackageType::Sleep
                        || record.mPackageType == ESM4::AIPackageType::Travel)
                    {
                        live.mState.mTarget = reference->mReference;
                        live.mState.mTargetBase = reference->mBase;
                    }
                    return reference->mPosition;
                }
                if (targetFallback && target)
                    return target->mPosition;
                return std::nullopt;
            }
            case ESM4::PackageLocationKind::None:
                if (target)
                {
                    live.mDestinationCell = target->mCell;
                    return target->mPosition;
                }
                if (record.mPackageType == ESM4::AIPackageType::Sleep)
                {
                    const auto furniture = findObject({}, 12);
                    if (!furniture || !usableFurniture(*furniture))
                        return std::nullopt;
                    live.mState.mTarget = furniture->mReference;
                    live.mState.mTargetBase = furniture->mBase;
                    live.mDestinationCell = furniture->mCell;
                    return furniture->mPosition;
                }
                if (record.mPackageType == ESM4::AIPackageType::Eat
                    || record.mPackageType == ESM4::AIPackageType::UseItemAt
                    || record.mPackageType == ESM4::AIPackageType::CastMagic)
                    return setCurrentDestination();
                return std::nullopt;
            case ESM4::PackageLocationKind::NearLinkedReference:
            case ESM4::PackageLocationKind::Unknown:
                return std::nullopt;
        }
        return std::nullopt;
    }

    ESM4::PackageSelection OblivionAiService::select(const MWWorld::Ptr& actor, LiveActor& live, bool restart)
    {
        ESM4::PackageSelectionRequest request;
        request.mNow = now();
        request.mBasePackages = basePackages(actor);
        if (live.mScriptPackage)
            request.mScriptPackage = live.mScriptPackage;
        request.mConditionContext = conditionContext(actor, live);
        request.mEvaluationGeneration = mNextEvaluationGeneration++;
        if (mNextEvaluationGeneration == 0)
            mNextEvaluationGeneration = 1;

        const ESM4::PackageSelection result = ESM4::selectPackage(request);
        const bool sameSelection = !restart
            && oblivionPackageSelectionMatches(live.mState, live.mSelectedWindow, result, request.mNow);
        if (sameSelection)
        {
            // Selection is a bounded reevaluation, not a phase restart.  A
            // package that is still the winner keeps its route, reservation,
            // and action boundary intact.
            live.mSelectionCheckTimer = 1.0f;
            live.mNeedsSelection = false;
            return result;
        }

        live.mState.mSelectionGeneration = result.mEvaluationGeneration;
        live.mState.mConditionResult = result.mConditionResult;
        live.mState.mScheduleWindow = result.mWindow;
        live.mState.mSource = result.mSource;
        live.mState.mPackage = result.mPackage;
        live.mState.mScriptPackage = live.mScriptPackage ? live.mScriptPackage->mKey : ESM::FormKey{};
        live.mState.mPackageType = result.mType;
        live.mState.mProcedure = ESM4::packageProcedure(result.mType);
        live.mState.mListIndex = static_cast<std::uint32_t>(result.mListIndex);
        setPhase(live, result.hasPackage() ? ESM4::PackagePhase::Select : ESM4::PackagePhase::Wait);
        live.mState.mBoundary = ESM4::PhaseBoundary::None;
        live.mState.mTarget = {};
        live.mState.mTargetBase = {};
        live.mState.mCompanionGroup = {};
        live.mState.mCompanionSideWith = {};
        live.mState.mPathgrid = {};
        live.mState.mPathNode = 0;
        live.mState.mDoor = {};
        live.mState.mDestinationCell = {};
        live.mState.mDestinationPosition = {};
        live.mState.mHasDestination = false;
        live.mState.mActionItem = {};
        live.mState.mActionTimer = 0.f;
        live.mState.mDurationRemaining = result.mWindow
            ? static_cast<float>(std::max(0.0, ESM4::calendarHoursUntil(request.mNow, result.mWindow->mEnd)))
            : 0.f;
        live.mState.mNoProgressSeconds = 0.f;
        live.mState.mRepathAttempts = 0;
        live.mState.mActionReserved = false;
        live.mState.mInterruptionReason.clear();
        live.mRoute.clear();
        live.mContinuousRoute.clear();
        live.mRouteCursor = 0;
        live.mContinuousRouteCursor = 0;
        live.mForeignRouteTarget.reset();
        live.mRouteDoor.reset();
        live.mDestination.reset();
        live.mDestinationCell = {};
        live.mSelectedWindow = result.mWindow;
        live.mSelectionCheckTimer = 1.0f;
        live.mPendingDoor = false;
        live.mNeedsSelection = false;
        std::ostringstream selection;
        selection << "source=" << static_cast<unsigned>(result.mSource) << " type="
                  << static_cast<unsigned>(result.mType) << " list_index=" << result.mListIndex;
        logEvent("selection", live, selection.str());
        return result;
    }

    std::optional<osg::Vec3f> OblivionAiService::resolveTarget(const MWWorld::Ptr& actor,
        const ESM4::AIPackage& record, LiveActor& live, ESM::FormKey& targetKey) const
    {
        targetKey = {};
        std::optional<MWWorld::Ptr> target;
        const ESM4::PackageTarget& data = record.mTargetData;
        switch (data.mKind)
        {
            case ESM4::PackageTargetKind::SpecificReference:
            case ESM4::PackageTargetKind::LinkedReference:
                if (!data.mReferenceKey.isNull())
                    target = ptrFor(data.mReferenceKey);
                break;
            case ESM4::PackageTargetKind::ObjectId:
                target = findReference(actor, data.mReferenceKey, std::nullopt);
                break;
            case ESM4::PackageTargetKind::ObjectType:
                target = findReference(actor, {}, data.mObjectType);
                break;
            case ESM4::PackageTargetKind::None:
            case ESM4::PackageTargetKind::Unknown:
                break;
        }
        if (!target && !live.mState.mTarget.isNull())
        {
            const MWWorld::Ptr savedTarget = ptrFor(live.mState.mTarget);
            if (!savedTarget.isEmpty())
                target = savedTarget;
        }
        if (!target || target->isEmpty())
            return std::nullopt;

        // Package target identity is the placed reference selected by the
        // runtime, not the base object requested by PTDT. This makes an
        // ObjectId/ObjectType target stable for follow/escort companions while
        // retaining the base identity separately for conditions and reports.
        targetKey = actorKey(*target);
        if (targetKey.isNull())
            return std::nullopt;
        live.mState.mTargetBase = baseKey(*target);
        if (record.mPackageType == ESM4::AIPackageType::Follow
            || record.mPackageType == ESM4::AIPackageType::Escort
            || record.mPackageType == ESM4::AIPackageType::Accompany)
            live.mState.mCompanionGroup = targetKey;
        if (const auto logical = mActors.find(targetKey);
            logical != mActors.end() && logical->second.mAbstractPositionDirty
            && targetKey != ESM::FormKey::dynamic("player", 1))
            return logical->second.mState.mLastValidPosition.asVec3();
        return target->getRefData().getPosition().asVec3();
    }

    std::optional<osg::Vec3f> OblivionAiService::resolveDestination(const MWWorld::Ptr& actor,
        const ESM4::AIPackage& record, LiveActor& live, ESM::FormKey& targetKey) const
    {
        targetKey = {};
        const osg::Vec3f actorPosition = live.mAbstractPositionDirty
            ? live.mState.mLastValidPosition.asVec3() : actor.getRefData().getPosition().asVec3();
        const bool followLike = record.mPackageType == ESM4::AIPackageType::Follow
            || record.mPackageType == ESM4::AIPackageType::Accompany
            || record.mPackageType == ESM4::AIPackageType::Pursue;
        const bool escort = record.mPackageType == ESM4::AIPackageType::Escort;
        const bool flee = record.mPackageType == ESM4::AIPackageType::FleeNotCombat;
        const bool targetFallback = record.mPackageType == ESM4::AIPackageType::Find
            || record.mPackageType == ESM4::AIPackageType::Ambush;
        std::optional<osg::Vec3f> targetPosition;

        const auto usableFurniture = [&](const MWWorld::Ptr& furniture) {
            if (furniture.isEmpty() || static_cast<ESM::RecNameInts>(furniture.getClass().getType()) != ESM::REC_FURN4
                || !furniture.getRefData().isEnabled() || furniture.getCellRef().isLocked())
                return false;
            const ESM::RefId owner = furniture.getCellRef().getOwner();
            if (owner.empty())
                return true;

            const ESM::FormKeyResolver resolver(mWorld.mContentFiles);
            const ESM::FormKey ownerKey = owner.getIf<ESM::FormId>() != nullptr
                ? resolver.toFormKey(*owner.getIf<ESM::FormId>())
                : ESM::FormKey{};
            const ESM::RefId primaryFaction = actor.getClass().getPrimaryFaction(actor);
            return owner == primaryFaction || ownerKey == actorKey(actor) || ownerKey == baseKey(actor);
        };
        const auto resolveSleepFurniture = [&](const std::optional<MWWorld::Ptr>& reference)
            -> std::optional<osg::Vec3f> {
            if (!reference || !usableFurniture(*reference))
                return std::nullopt;
            targetKey = actorKey(*reference);
            live.mState.mTarget = targetKey;
            live.mState.mTargetBase = baseKey(*reference);
            live.mDestinationCell = cellKey(*reference);
            return reference->getRefData().getPosition().asVec3();
        };

        if (followLike || escort || flee || targetFallback)
        {
            targetPosition = resolveTarget(actor, record, live, targetKey);
            if (!targetPosition && (followLike || escort || flee))
                return std::nullopt;
        }

        const auto setCurrentDestination = [&]() {
            live.mDestinationCell = live.mState.mCell;
            return actorPosition;
        };
        const auto resolveLocationReference = [&]() -> std::optional<MWWorld::Ptr> {
            const ESM4::PackageLocation& location = record.mLocationData;
            switch (location.mKind)
            {
                case ESM4::PackageLocationKind::NearReference:
                case ESM4::PackageLocationKind::EditorLocation:
                case ESM4::PackageLocationKind::ObjectId:
                    return findReference(actor, location.mReferenceKey, std::nullopt);
                case ESM4::PackageLocationKind::ObjectType:
                    return findReference(actor, {}, location.mObjectType);
                case ESM4::PackageLocationKind::None:
                case ESM4::PackageLocationKind::InCell:
                case ESM4::PackageLocationKind::CurrentLocation:
                case ESM4::PackageLocationKind::NearLinkedReference:
                case ESM4::PackageLocationKind::AtPackageLocation:
                case ESM4::PackageLocationKind::Unknown:
                    return std::nullopt;
            }
            return std::nullopt;
        };
        const auto resolveCellDestination = [&](const ESM::FormKey& destinationCell)
            -> std::optional<osg::Vec3f> {
            if (destinationCell.isNull())
                return std::nullopt;
            const ESM4::PathgridService& service = mWorld.mStore.getOblivionPathgridService();
            const ESM4::PathgridGraph* graph = service.graphForCell(destinationCell);
            if (graph == nullptr || graph->nodeCount() == 0)
                return std::nullopt;
            std::vector<std::uint32_t> nodes;
            std::set<ESM4::PathgridNodeKey> reachable;
            if (destinationCell == live.mState.mCell)
            {
                if (const auto start = service.nearestEnabledNode(graph->pathgridKey(),
                        { actorPosition.x(), actorPosition.y(), actorPosition.z() }, 2048.f))
                    reachable = service.reachableComponent(*start);
            }
            for (std::uint32_t node = 0; node < graph->nodeCount(); ++node)
                if (graph->isEnabled(node)
                    && (reachable.empty()
                        || reachable.contains({ graph->pathgridKey(), node })))
                    nodes.push_back(node);
            if (nodes.empty())
                return std::nullopt;
            const std::uint32_t node = nodes[stableChoice(actorKey(actor), live.mState.mSelectionGeneration)
                % nodes.size()];
            const ESM4::PathgridPoint point = graph->worldPoint(node);
            live.mDestinationCell = destinationCell;
            return osg::Vec3f(point.mX, point.mY, point.mZ);
        };

        if (followLike)
        {
            const MWWorld::Ptr target = ptrFor(targetKey);
            if (target.isEmpty())
                return std::nullopt;
            live.mDestinationCell = cellKey(target);
            if (const auto logical = mActors.find(targetKey);
                logical != mActors.end() && logical->second.mAbstractPositionDirty
                && targetKey != ESM::FormKey::dynamic("player", 1))
                live.mDestinationCell = logical->second.mState.mCell;
            const float requestedDistance = static_cast<float>(record.mTargetData.mDistance);
            const float followDistance = requestedDistance > 0.f ? requestedDistance : 128.f;
            osg::Vec3f offset = actorPosition - *targetPosition;
            offset.z() = 0.f;
            if (offset.length2() <= 1.f)
            {
                const float yaw = target.getRefData().getPosition().rot[2];
                offset = osg::Vec3f(-std::sin(yaw), -std::cos(yaw), 0.f);
            }
            else
                offset.normalize();
            osg::Vec3f destination = *targetPosition + offset * followDistance;
            destination.z() = targetPosition->z();
            return destination;
        }

        if (flee)
        {
            const MWWorld::Ptr threat = ptrFor(targetKey);
            if (threat.isEmpty())
                return std::nullopt;
            const ESM4::PathgridService& service = mWorld.mStore.getOblivionPathgridService();
            const ESM4::PathgridGraph* graph = service.graphForCell(live.mState.mCell);
            if (graph == nullptr)
                return std::nullopt;
            const osg::Vec3f threatPosition = threat.getRefData().getPosition().asVec3();
            const auto start = service.nearestEnabledNode(graph->pathgridKey(),
                { actorPosition.x(), actorPosition.y(), actorPosition.z() }, 2048.f);
            if (!start)
                return std::nullopt;
            const std::set<ESM4::PathgridNodeKey> reachable = service.reachableComponent(*start);
            const float currentThreatDistance = distanceSquared(actorPosition, threatPosition);
            std::optional<ESM4::PathgridNodeKey> best;
            float bestThreatDistance = -1.f;
            float bestRouteCost = -1.f;
            for (const ESM4::PathgridNodeKey& node : reachable)
            {
                const ESM4::PathgridGraph* nodeGraph = service.graph(node.mPathgrid);
                if (nodeGraph == nullptr)
                    continue;
                const auto route = service.route(*start, node);
                if (!route)
                    continue;
                const ESM4::PathgridPoint point = nodeGraph->worldPoint(node.mNode);
                const float threatDistance = distanceSquared({ point.mX, point.mY, point.mZ }, threatPosition);
                const bool safer = threatDistance > currentThreatDistance + 1.f;
                const bool bestSafer = bestThreatDistance > currentThreatDistance + 1.f;
                if (!best || (safer && !bestSafer)
                    || (safer == bestSafer
                        && (threatDistance > bestThreatDistance + 1.f
                            || (std::abs(threatDistance - bestThreatDistance) <= 1.f
                                && (route.mRoute->mCost > bestRouteCost + 1.f
                                    || (std::abs(route.mRoute->mCost - bestRouteCost) <= 1.f && node < *best))))))
                {
                    best = node;
                    bestThreatDistance = threatDistance;
                    bestRouteCost = route.mRoute->mCost;
                }
            }
            if (!best)
                return std::nullopt;
            const ESM4::PathgridGraph* bestGraph = service.graph(best->mPathgrid);
            const ESM4::PathgridPoint point = bestGraph->worldPoint(best->mNode);
            live.mDestinationCell = bestGraph->cellKey();
            return osg::Vec3f(point.mX, point.mY, point.mZ);
        }

        // Escort keeps its actor target in mTarget while its destination is
        // the package location. If no location was supplied, the target is the
        // only unambiguous destination and is therefore used as a documented
        // fallback rather than silently idling.
        if (escort && record.mLocationData.mKind == ESM4::PackageLocationKind::None)
        {
            const MWWorld::Ptr target = ptrFor(targetKey);
            if (target.isEmpty() || !targetPosition)
                return std::nullopt;
            live.mDestinationCell = cellKey(target);
            return *targetPosition;
        }

        if (record.mPackageType == ESM4::AIPackageType::Wander)
        {
            if (const auto node = chooseWanderNode(actor, record, live))
            {
                const ESM4::PathgridGraph* graph
                    = mWorld.mStore.getOblivionPathgridService().graph(node->mPathgrid);
                if (graph != nullptr)
                {
                    live.mDestinationCell = graph->cellKey();
                    const ESM4::PathgridPoint point = graph->worldPoint(node->mNode);
                    return osg::Vec3f(point.mX, point.mY, point.mZ);
                }
            }
            // A cell without a native graph is a valid idle wander state only
            // when the package has no external centre. An explicit, unresolved
            // location must interrupt the package instead of degrading into a
            // successful idle/teleport-free result at the actor's position.
            if (record.mLocationData.mKind == ESM4::PackageLocationKind::None
                || record.mLocationData.mKind == ESM4::PackageLocationKind::CurrentLocation
                || (record.mLocationData.mKind == ESM4::PackageLocationKind::EditorLocation
                    && record.mLocationData.mReferenceKey.isNull())
                || (record.mLocationData.mKind == ESM4::PackageLocationKind::InCell
                    && record.mLocationData.mReferenceKey == live.mState.mCell))
                return setCurrentDestination();
            return std::nullopt;
        }

        const ESM4::PackageLocation& location = record.mLocationData;
        switch (location.mKind)
        {
            case ESM4::PackageLocationKind::CurrentLocation:
                if (record.mPackageType == ESM4::AIPackageType::Sleep)
                    return resolveSleepFurniture(findReference(actor, {}, 12));
                return setCurrentDestination();
            case ESM4::PackageLocationKind::InCell:
                if (location.mReferenceKey == live.mState.mCell)
                    return setCurrentDestination();
                if (const auto destination = resolveCellDestination(location.mReferenceKey))
                    return destination;
                return std::nullopt;
            case ESM4::PackageLocationKind::AtPackageLocation:
                if (live.mState.mHasDestination && live.mState.mDestinationCell == live.mState.mCell)
                {
                    live.mDestinationCell = live.mState.mDestinationCell;
                    return osg::Vec3f(live.mState.mDestinationPosition.pos[0],
                        live.mState.mDestinationPosition.pos[1], live.mState.mDestinationPosition.pos[2]);
                }
                return std::nullopt;
            case ESM4::PackageLocationKind::NearReference:
            case ESM4::PackageLocationKind::EditorLocation:
            case ESM4::PackageLocationKind::ObjectId:
            case ESM4::PackageLocationKind::ObjectType:
            {
                if (location.mKind == ESM4::PackageLocationKind::EditorLocation
                    && location.mReferenceKey.isNull())
                    return setCurrentDestination();
                const auto reference = resolveLocationReference();
                if (reference)
                {
                    if (record.mPackageType == ESM4::AIPackageType::Sleep)
                        return resolveSleepFurniture(reference);
                    live.mDestinationCell = cellKey(*reference);
                    if (record.mPackageType == ESM4::AIPackageType::Travel)
                    {
                        targetKey = actorKey(*reference);
                        live.mState.mTargetBase = baseKey(*reference);
                    }
                    return reference->getRefData().getPosition().asVec3();
                }
                if (targetFallback && targetPosition)
                    return *targetPosition;
                return std::nullopt;
            }
            case ESM4::PackageLocationKind::None:
                if (targetPosition)
                    return *targetPosition;
                if (record.mPackageType == ESM4::AIPackageType::Sleep)
                    return resolveSleepFurniture(findReference(actor, {}, 12));
                if (record.mPackageType == ESM4::AIPackageType::Eat
                    || record.mPackageType == ESM4::AIPackageType::UseItemAt
                    || record.mPackageType == ESM4::AIPackageType::CastMagic)
                    return setCurrentDestination();
                return std::nullopt;
            case ESM4::PackageLocationKind::NearLinkedReference:
            case ESM4::PackageLocationKind::Unknown:
                return std::nullopt;
        }
        return std::nullopt;
    }

    bool OblivionAiService::followDestinationReached(const MWWorld::Ptr& actor,
        const ESM4::AIPackage& record, const LiveActor& live, bool highProcess) const
    {
        if (record.mPackageType != ESM4::AIPackageType::Follow
            || record.mLocationData.mKind == ESM4::PackageLocationKind::None)
            return false;

        LiveActor targetProbe = live;
        ESM::FormKey targetKey = live.mState.mTarget;
        if (highProcess)
        {
            ESM::FormKey refreshedTarget;
            if (!resolveTarget(actor, record, targetProbe, refreshedTarget))
                return false;
            targetKey = refreshedTarget;
        }
        else if (!resolveUnloadedDestination(record, targetProbe))
            return false;
        else
            targetKey = targetProbe.mState.mTarget;

        ESM::FormKey targetCell;
        osg::Vec3f targetPosition;
        if (const auto found = mActors.find(targetKey); found != mActors.end())
        {
            targetCell = found->second.mState.mCell;
            targetPosition = found->second.mState.mLastValidPosition.asVec3();
        }
        else if (const MWWorld::Ptr target = loadedPtrFor(targetKey); !target.isEmpty())
        {
            targetCell = cellKey(target);
            targetPosition = target.getRefData().getPosition().asVec3();
        }
        else if (const auto unloaded = mUnloadedLocationByReference.find(targetKey);
            unloaded != mUnloadedLocationByReference.end())
        {
            const UnloadedLocation& unloadedTarget = mUnloadedLocations[unloaded->second];
            targetCell = unloadedTarget.mCell;
            targetPosition = unloadedTarget.mPosition;
        }
        else
            return false;

        if (record.mLocationData.mKind == ESM4::PackageLocationKind::InCell)
            return !targetCell.isNull() && targetCell == record.mLocationData.mReferenceKey;

        ESM4::AIPackage locationRecord = record;
        locationRecord.mPackageType = ESM4::AIPackageType::Travel;
        locationRecord.mTargetData = {};
        LiveActor locationProbe = live;
        std::optional<osg::Vec3f> destination;
        if (highProcess)
        {
            ESM::FormKey ignoredTarget;
            destination = resolveDestination(actor, locationRecord, locationProbe, ignoredTarget);
        }
        else
            destination = resolveUnloadedDestination(locationRecord, locationProbe);
        return destination && oblivionFollowDestinationReached(targetCell, targetPosition,
            locationProbe.mDestinationCell, *destination, record.mLocationData.mRadius);
    }

    bool OblivionAiService::prepareDoorRoute(LiveActor& live, const ESM4::PathgridNodeKey& start,
        const ESM::FormKey& destinationCell, const MWWorld::Ptr* actor)
    {
        if (destinationCell.isNull() || destinationCell == live.mState.mCell)
        {
            live.mLastRouteFailure = "door-route-same-or-missing-cell";
            return false;
        }

        const ESM4::PathgridService& service = mWorld.mStore.getOblivionPathgridService();
        const ESM4::PathgridGraph* sourceGraph = service.graph(start.mPathgrid);
        if (sourceGraph == nullptr || sourceGraph->cellKey() != live.mState.mCell
            || !sourceGraph->contains(start.mNode) || !sourceGraph->isEnabled(start.mNode))
        {
            live.mLastRouteFailure = "door-route-invalid-source-node";
            return false;
        }

        // Resolve a shortest coarse route backwards from the requested cell.
        // A native package destination may be behind several interiors (for
        // example, an actor house -> city exterior -> another house). The
        // current segment still ends at one real source-side door; the next
        // segment is rebuilt after the paired marker commits the intermediate
        // cell. This keeps every boundary an actual XTEL edge.
        std::map<ESM::FormKey, std::size_t> remainingHops;
        std::queue<ESM::FormKey> pendingCells;
        remainingHops.emplace(destinationCell, 0);
        pendingCells.push(destinationCell);
        std::size_t reverseDoors = 0;
        std::size_t reverseLocked = 0;
        while (!pendingCells.empty())
        {
            const ESM::FormKey cell = pendingCells.front();
            pendingCells.pop();
            const std::size_t distance = remainingHops.at(cell);
            const auto incoming = mUnloadedDoorsByDestinationCell.find(cell);
            if (incoming == mUnloadedDoorsByDestinationCell.end())
                continue;
            for (const std::size_t index : incoming->second)
            {
                const UnloadedLocation& door = mUnloadedLocations[index];
                ++reverseDoors;
                if (door.mType != ESM::REC_DOOR4 || door.mCell.isNull()
                    || (door.mReference == live.mState.mLastTransitionDoor && live.mState.mDoorCooldown > 0.f))
                    continue;
                bool locked = false;
                if (!canUseUnloadedDoor(live, door, locked, actor))
                {
                    reverseLocked += locked ? 1 : 0;
                    continue;
                }
                if (remainingHops.emplace(door.mCell, distance + 1).second)
                    pendingCells.push(door.mCell);
            }
        }

        std::optional<ESM4::PathgridRoute> bestRoute;
        std::optional<ESM::FormKey> bestDoor;
        osg::Vec3f bestDoorPosition;
        std::size_t bestHopCount = std::numeric_limits<std::size_t>::max();
        float bestCost = std::numeric_limits<float>::max();
        std::size_t sourceDoorCount = 0;
        std::size_t sourceUsableCount = 0;
        std::size_t sourceReachableCellCount = 0;
        std::size_t sourceNodeCount = 0;
        std::size_t sourceRouteCount = 0;
        std::size_t sourceLockedCount = 0;
        const auto considerDoorCell = [&](const ESM::FormKey& candidateCell) {
            // A door need not be in the actor's current cell. Exterior PGRI
            // edges can lead across several cells before the first XTEL edge.
            // Native pathgrid routing determines whether a candidate door
            // source is reachable without teleporting.
            if (remainingHops.find(candidateCell) == remainingHops.end())
                return;
            const auto sourceDoors = mUnloadedDoorsByCell.find(candidateCell);
            if (sourceDoors == mUnloadedDoorsByCell.end())
                return;
            for (const std::size_t index : sourceDoors->second)
            {
                const UnloadedLocation& door = mUnloadedLocations[index];
                ++sourceDoorCount;
                if (door.mType != ESM::REC_DOOR4 || door.mTeleportDoor.isNull()
                    || (door.mReference == live.mState.mLastTransitionDoor && live.mState.mDoorCooldown > 0.f))
                    continue;
                bool locked = false;
                if (!canUseUnloadedDoor(live, door, locked, actor))
                {
                    sourceLockedCount += locked ? 1 : 0;
                    continue;
                }
                ++sourceUsableCount;

                const auto destination = mUnloadedLocationByReference.find(door.mTeleportDoor);
                if (destination == mUnloadedLocationByReference.end())
                    continue;
                const UnloadedLocation& marker = mUnloadedLocations[destination->second];
                if (marker.mCell.isNull() || !currentDoorState(marker).mAvailable)
                    continue;
                const auto remaining = remainingHops.find(marker.mCell);
                if (remaining == remainingHops.end())
                    continue;
                ++sourceReachableCellCount;

                const ESM4::PathgridGraph* doorGraph = service.graphForCell(door.mCell);
                const auto doorNode = doorGraph
                    ? service.nearestEnabledNode(doorGraph->pathgridKey(),
                          { door.mPosition.x(), door.mPosition.y(), door.mPosition.z() }, 4096.f)
                    : std::nullopt;
                if (!doorNode)
                    continue;
                ++sourceNodeCount;
                const ESM4::PathgridRouteResult route = service.route(start, *doorNode);
                if (!route)
                    continue;
                ++sourceRouteCount;

                const ESM4::PathgridPoint nodePoint = doorGraph->worldPoint(doorNode->mNode);
                const float cost = route.mRoute->mCost
                    + (osg::Vec3f(nodePoint.mX, nodePoint.mY, nodePoint.mZ) - door.mPosition).length();
                const std::size_t hopCount = remaining->second + 1;
                if (!bestRoute || hopCount < bestHopCount
                    || (hopCount == bestHopCount
                        && (cost < bestCost || (cost == bestCost && door.mReference < *bestDoor))))
                {
                    bestHopCount = hopCount;
                    bestCost = cost;
                    bestDoor = door.mReference;
                    bestDoorPosition = door.mPosition;
                    bestRoute = *route.mRoute;
                }
            }
        };

        // Nearly every interior route begins with a resident-cell door. Keep
        // that common path cheap; search other door-source cells only when no
        // current-cell edge reaches the destination through the door graph.
        considerDoorCell(live.mState.mCell);
        if (!bestRoute)
            for (const auto& [candidateCell, _] : remainingHops)
                if (candidateCell != live.mState.mCell)
                    considerDoorCell(candidateCell);

        if (!bestRoute || !bestDoor)
        {
            std::ostringstream diagnostic;
            diagnostic << "door-route-unavailable source_doors=" << sourceDoorCount
                       << " source_usable=" << sourceUsableCount
                       << " source_locked=" << sourceLockedCount
                       << " source_reaching_destination=" << sourceReachableCellCount
                       << " source_nodes=" << sourceNodeCount
                       << " source_routes=" << sourceRouteCount
                       << " reverse_cells=" << remainingHops.size()
                       << " reverse_doors=" << reverseDoors
                       << " reverse_locked=" << reverseLocked;
            live.mLastRouteFailure = diagnostic.str();
            return false;
        }
        live.mRoute = std::move(bestRoute->mNodes);
        live.mRouteCursor = live.mRoute.size() > 1 ? 1 : live.mRoute.size();
        live.mForeignRouteTarget.reset();
        live.mRouteDoor = *bestDoor;
        live.mRouteDoorPosition = bestDoorPosition;
        live.mState.mDoor = *bestDoor;
        live.mState.mPathgrid = live.mRoute.front().mPathgrid;
        live.mState.mPathNode = live.mRoute.front().mNode;
        live.mState.mRouteGeneration = bestRoute->mGeneration;
        return true;
    }

    bool OblivionAiService::prepareRoute(const MWWorld::Ptr& actor, LiveActor& live,
        const osg::Vec3f& destination, const ESM::FormKey& destinationCell)
    {
        const bool sameCell = destinationCell.isNull() || destinationCell == live.mState.mCell;
        live.mContinuousRoute.clear();
        live.mContinuousRouteCursor = 0;
        live.mContinuousRouteEndCursor.reset();
        live.mRouteDoor.reset();

        // Recast/Detour is the continuous movement authority for every
        // resident-cell segment, including approaches to doors and seamless
        // PGRI boundaries. Native PGRD remains the stable intent graph.
        const auto prepareContinuousRoute = [&](const osg::Vec3f& target,
                                                std::span<const osg::Vec3f> checkpoints) {
            bool success = false;
            DetourNavigator::Status status = DetourNavigator::Status::NavMeshNotFound;
            if (DetourNavigator::Navigator* navigator = mWorld.getNavigator())
            {
                DetourNavigator::Flags flags = DetourNavigator::Flag_none;
                if (actor.getClass().canWalk(actor) && actor.getClass().getWalkSpeed(actor) > 0.f)
                    flags |= DetourNavigator::Flag_walk;
                if (actor.getClass().canSwim(actor) && actor.getClass().getSwimSpeed(actor) > 0.f)
                    flags |= DetourNavigator::Flag_swim;
                if (live.mState.mPackageType != ESM4::AIPackageType::Wander)
                    flags |= DetourNavigator::Flag_openDoor;

                if (flags != DetourNavigator::Flag_none)
                {
                    const float walkSpeed = (flags & DetourNavigator::Flag_walk) != 0
                        ? std::max(0.f, actor.getClass().getRunSpeed(actor))
                        : 0.f;
                    const float swimSpeed = (flags & DetourNavigator::Flag_swim) != 0
                        ? std::max(0.f, actor.getClass().getSwimSpeed(actor))
                        : 0.f;
                    const float maxSpeed = std::max(walkSpeed, swimSpeed);
                    DetourNavigator::AreaCosts costs;
                    if (maxSpeed > 0.f)
                    {
                        if (swimSpeed > 0.f)
                            costs.mWater = maxSpeed / swimSpeed;
                        if (walkSpeed > 0.f)
                        {
                            costs.mDoor = maxSpeed / walkSpeed;
                            costs.mPathgrid = maxSpeed / walkSpeed;
                            costs.mGround = maxSpeed / walkSpeed;
                        }
                    }

                    status = DetourNavigator::findPath(*navigator, mWorld.getPathfindingAgentBounds(actor),
                        actor.getRefData().getPosition().asVec3(), target, flags, costs, sArrivalTolerance,
                        checkpoints, std::back_inserter(live.mContinuousRoute));
                    success = status == DetourNavigator::Status::Success && !live.mContinuousRoute.empty();
                    if (!success)
                        live.mContinuousRoute.clear();
                }
            }

            std::ostringstream navmeshEvent;
            navmeshEvent << "status=" << DetourNavigator::getMessage(status)
                         << " continuous=" << (success ? "true" : "false");
            logEvent("route-navmesh", live, navmeshEvent.str());
            return success;
        };

        bool hasContinuousRoute = false;
        if (sameCell)
        {
            hasContinuousRoute = prepareContinuousRoute(destination, {});
        }

        if (!destinationCell.isNull() && live.mState.mCell != destinationCell)
        {
            // A cross-cell route is valid only when the native graph contains
            // a foreign edge.  We retain the route intent and let the door
            // phase handle the boundary; no direct position mutation occurs.
        }

        const ESM4::PathgridService& service = mWorld.mStore.getOblivionPathgridService();
        const ESM4::PathgridGraph* graph = service.graphForCell(live.mState.mCell);
        if (graph == nullptr)
            return hasContinuousRoute || ((destinationCell.isNull() || destinationCell == live.mState.mCell)
                && distanceSquared(actor.getRefData().getPosition().asVec3(), destination) <= sArrivalTolerance
                    * sArrivalTolerance);
        const auto start = service.nearestEnabledNode(graph->pathgridKey(),
            { actor.getRefData().getPosition().pos[0], actor.getRefData().getPosition().pos[1],
                actor.getRefData().getPosition().pos[2] }, 2048.f);
        const ESM::FormKey targetGraphCell = destinationCell.isNull() ? live.mState.mCell : destinationCell;
        const ESM4::PathgridGraph* endGraph = service.graphForCell(targetGraphCell);
        const auto destinationNode = endGraph
            ? service.nearestEnabledNode(endGraph->pathgridKey(), { destination.x(), destination.y(), destination.z() },
                4096.f)
            : std::nullopt;
        if (!start)
            return hasContinuousRoute || ((destinationCell.isNull() || destinationCell == live.mState.mCell)
                && distanceSquared(actor.getRefData().getPosition().asVec3(), destination) <= sArrivalTolerance
                    * sArrivalTolerance);
        if (!destinationNode)
        {
            if (!sameCell && prepareDoorRoute(live, *start, destinationCell, &actor))
            {
                std::vector<osg::Vec3f> checkpoints;
                checkpoints.reserve(live.mRoute.size());
                for (std::size_t index = live.mRouteCursor; index < live.mRoute.size(); ++index)
                {
                    const ESM4::PathgridGraph* routeGraph = service.graph(live.mRoute[index].mPathgrid);
                    if (routeGraph == nullptr || routeGraph->cellKey() != live.mState.mCell)
                        break;
                    const ESM4::PathgridPoint point = routeGraph->worldPoint(live.mRoute[index].mNode);
                    checkpoints.emplace_back(point.mX, point.mY, point.mZ);
                }
                hasContinuousRoute = prepareContinuousRoute(live.mRouteDoorPosition, checkpoints);
                if (hasContinuousRoute)
                    live.mContinuousRouteEndCursor = live.mRoute.size();
                std::ostringstream routeEvent;
                routeEvent << "generation=" << live.mState.mRouteGeneration << " nodes=" << live.mRoute.size()
                           << " destination_cell=" << destinationCell.serialize()
                           << " door=" << live.mState.mDoor.serialize()
                           << " continuous=" << (hasContinuousRoute ? "true" : "false");
                logEvent("route", live, routeEvent.str());
                return true;
            }
            return hasContinuousRoute || (sameCell
                && distanceSquared(actor.getRefData().getPosition().asVec3(), destination)
                    <= sArrivalTolerance * sArrivalTolerance);
        }
        const ESM4::PathgridRouteResult route = service.route(*start, *destinationNode);
        if (!route)
        {
            if (!sameCell && prepareDoorRoute(live, *start, destinationCell, &actor))
            {
                std::vector<osg::Vec3f> checkpoints;
                checkpoints.reserve(live.mRoute.size());
                for (std::size_t index = live.mRouteCursor; index < live.mRoute.size(); ++index)
                {
                    const ESM4::PathgridGraph* routeGraph = service.graph(live.mRoute[index].mPathgrid);
                    if (routeGraph == nullptr || routeGraph->cellKey() != live.mState.mCell)
                        break;
                    const ESM4::PathgridPoint point = routeGraph->worldPoint(live.mRoute[index].mNode);
                    checkpoints.emplace_back(point.mX, point.mY, point.mZ);
                }
                hasContinuousRoute = prepareContinuousRoute(live.mRouteDoorPosition, checkpoints);
                if (hasContinuousRoute)
                    live.mContinuousRouteEndCursor = live.mRoute.size();
                std::ostringstream routeEvent;
                routeEvent << "generation=" << live.mState.mRouteGeneration << " nodes=" << live.mRoute.size()
                           << " destination_cell=" << destinationCell.serialize()
                           << " door=" << live.mState.mDoor.serialize()
                           << " continuous=" << (hasContinuousRoute ? "true" : "false");
                logEvent("route", live, routeEvent.str());
                return true;
            }
            return hasContinuousRoute;
        }
        live.mRoute = route.mRoute->mNodes;
        live.mRouteCursor = live.mRoute.size() > 1 ? 1 : live.mRoute.size();
        live.mForeignRouteTarget.reset();
        live.mRouteDoor.reset();
        live.mState.mDoor = {};
        live.mState.mPathgrid = live.mRoute.front().mPathgrid;
        live.mState.mPathNode = live.mRoute.front().mNode;
        live.mState.mRouteGeneration = route.mRoute->mGeneration;
        if (!sameCell)
        {
            const auto foreign = std::find_if(live.mRoute.begin() + live.mRouteCursor, live.mRoute.end(),
                [&](const ESM4::PathgridNodeKey& node) {
                    const ESM4::PathgridGraph* routeGraph = service.graph(node.mPathgrid);
                    return routeGraph != nullptr && routeGraph->cellKey() != live.mState.mCell;
                });
            if (foreign != live.mRoute.end() && foreign != live.mRoute.begin())
            {
                const std::size_t foreignIndex = static_cast<std::size_t>(foreign - live.mRoute.begin());
                const ESM4::PathgridNodeKey& boundary = live.mRoute[foreignIndex - 1];
                if (const ESM4::PathgridGraph* boundaryGraph = service.graph(boundary.mPathgrid))
                {
                    std::vector<osg::Vec3f> checkpoints;
                    checkpoints.reserve(foreignIndex - live.mRouteCursor);
                    for (std::size_t index = live.mRouteCursor; index < foreignIndex; ++index)
                    {
                        const ESM4::PathgridGraph* routeGraph = service.graph(live.mRoute[index].mPathgrid);
                        if (routeGraph == nullptr)
                            break;
                        const ESM4::PathgridPoint point = routeGraph->worldPoint(live.mRoute[index].mNode);
                        checkpoints.emplace_back(point.mX, point.mY, point.mZ);
                    }
                    const ESM4::PathgridPoint point = boundaryGraph->worldPoint(boundary.mNode);
                    hasContinuousRoute = prepareContinuousRoute(
                        osg::Vec3f(point.mX, point.mY, point.mZ), checkpoints);
                    if (hasContinuousRoute)
                        live.mContinuousRouteEndCursor = foreignIndex;
                }
            }
        }
        std::ostringstream routeEvent;
        routeEvent << "generation=" << live.mState.mRouteGeneration << " nodes=" << live.mRoute.size()
                   << " destination_cell=" << destinationCell.serialize()
                   << " continuous=" << (hasContinuousRoute ? "true" : "false");
        logEvent("route", live, routeEvent.str());
        return true;
    }

    bool OblivionAiService::prepareUnloadedRoute(LiveActor& live)
    {
        live.mLastRouteFailure.clear();
        if (!live.mDestination)
        {
            live.mLastRouteFailure = "missing-destination";
            return false;
        }

        const ESM4::PathgridService& service = mWorld.mStore.getOblivionPathgridService();
        const osg::Vec3f position = live.mState.mLastValidPosition.asVec3();
        const ESM4::PathgridGraph* graph = service.graphForCell(live.mState.mCell);
        const ESM::FormKey destinationCell
            = live.mDestinationCell.isNull() ? live.mState.mCell : live.mDestinationCell;
        if (graph == nullptr)
        {
            if (destinationCell == live.mState.mCell
                && distanceSquared(position, *live.mDestination) <= sArrivalTolerance * sArrivalTolerance)
                return true;
            live.mLastRouteFailure = "missing-source-pathgrid";
            return false;
        }

        const auto start = service.nearestEnabledNode(graph->pathgridKey(),
            { position.x(), position.y(), position.z() }, 2048.f);
        const ESM4::PathgridGraph* endGraph = service.graphForCell(destinationCell);
        if (endGraph == nullptr)
        {
            live.mLastRouteFailure = "missing-destination-pathgrid";
            return false;
        }
        const auto destinationNode = endGraph
            ? service.nearestEnabledNode(endGraph->pathgridKey(),
                  { live.mDestination->x(), live.mDestination->y(), live.mDestination->z() }, 4096.f)
            : std::nullopt;
        if (!start)
        {
            live.mLastRouteFailure = "missing-source-pathgrid-node";
            return false;
        }
        if (!destinationNode)
        {
            live.mLastRouteFailure = "missing-destination-pathgrid-node";
            return false;
        }

        const ESM4::PathgridRouteResult route = service.route(*start, *destinationNode);
        if (!route)
        {
            if (destinationCell != live.mState.mCell && prepareDoorRoute(live, *start, destinationCell))
            {
                std::ostringstream routeEvent;
                routeEvent << "generation=" << live.mState.mRouteGeneration << " nodes=" << live.mRoute.size()
                           << " destination_cell=" << destinationCell.serialize()
                           << " door=" << live.mState.mDoor.serialize() << " low-process=true";
                logEvent("route", live, routeEvent.str());
                return true;
            }
            if (live.mLastRouteFailure.empty())
                live.mLastRouteFailure = route.mDiagnostic.empty() ? "pathgrid-route-unavailable" : route.mDiagnostic;
            return false;
        }
        live.mRoute = route.mRoute->mNodes;
        live.mRouteCursor = live.mRoute.size() > 1 ? 1 : live.mRoute.size();
        live.mForeignRouteTarget.reset();
        live.mRouteDoor.reset();
        live.mState.mDoor = {};
        live.mState.mPathgrid = live.mRoute.front().mPathgrid;
        live.mState.mPathNode = live.mRoute.front().mNode;
        live.mState.mRouteGeneration = route.mRoute->mGeneration;
        std::ostringstream routeEvent;
        routeEvent << "generation=" << live.mState.mRouteGeneration << " nodes=" << live.mRoute.size()
                   << " destination_cell=" << destinationCell.serialize() << " low-process=true";
        logEvent("route", live, routeEvent.str());
        return true;
    }

    void OblivionAiService::refreshMovingTargetRoute(
        LiveActor& live, const ESM4::AIPackage& current, const MWWorld::Ptr& residentActor)
    {
        const auto type = live.mState.mPackageType;
        const bool travelToReference = type == ESM4::AIPackageType::Travel
            && current.mLocationData.mKind == ESM4::PackageLocationKind::NearReference;
        if (type != ESM4::AIPackageType::Follow && type != ESM4::AIPackageType::Accompany
            && type != ESM4::AIPackageType::Pursue && type != ESM4::AIPackageType::Escort && !travelToReference)
            return;
        // Finish an already-entered boundary before considering another
        // target route. A moving leader must not make us oscillate on an edge.
        if (live.mForeignRouteTarget || !live.mState.mDoor.isNull() || live.mPendingDoor)
            return;
        const ESM::FormKey oldCell = live.mDestinationCell;
        ESM::FormKey target;
        const std::optional<osg::Vec3f> destination = residentActor.isEmpty()
            ? resolveUnloadedDestination(current, live)
            : resolveDestination(residentActor, current, live, target);
        if (!destination)
        {
            // Retain the last known route. The existing Wait/Resolve handling
            // reports an unavailable target once that route has been consumed.
            live.mDestinationCell = oldCell;
            return;
        }
        if (!residentActor.isEmpty())
            live.mState.mTarget = target;
        if (!updateOblivionMovingDestination(live.mState, live.mDestinationCell, *destination))
            return;
        live.mDestination = destination;
        live.mRoute.clear();
        live.mContinuousRoute.clear();
        live.mRouteCursor = 0;
        live.mContinuousRouteCursor = 0;
        live.mContinuousRouteEndCursor.reset();
        live.mRouteDoor.reset();
        live.mState.mPathgrid = {};
        live.mState.mPathNode = 0;
        // Keep no-progress and retry budgets: target motion is not evidence
        // that this actor actually moved. Path prepares the new route once.
        logEvent("target-route-refresh", live, "moving-target");
    }

    bool OblivionAiService::shouldRunPackage(const LiveActor& live, const ESM4::AIPackage* current,
        const osg::Vec3f& position) const
    {
        std::optional<float> targetDistance;
        bool differentCell = false;
        if (live.mState.mPackageType == ESM4::AIPackageType::Follow
            || live.mState.mPackageType == ESM4::AIPackageType::Accompany)
        {
            // A low-process target's realized Ptr can lag behind its logical
            // position. Prefer the AI authority, then handle the player and
            // other resident targets that are not in the actor registry.
            if (const auto target = mActors.find(live.mState.mTarget);
                target != mActors.end() && live.mState.mTarget != ESM::FormKey::dynamic("player", 1))
            {
                targetDistance = distanceSquared(position, target->second.mState.mLastValidPosition.asVec3());
                differentCell = live.mState.mCell != target->second.mState.mCell;
            }
            else if (const MWWorld::Ptr targetPtr = loadedPtrFor(live.mState.mTarget); !targetPtr.isEmpty())
            {
                targetDistance = distanceSquared(position, targetPtr.getRefData().getPosition().asVec3());
                differentCell = live.mState.mCell != cellKey(targetPtr);
            }
        }
        return oblivionPackageShouldRun(live.mState.mPackageType,
            current != nullptr ? current->mPackageFlags : ESM4::PackageFlags{},
            current != nullptr ? static_cast<float>(current->mTargetData.mDistance) : 0.f,
            targetDistance, differentCell);
    }

    float OblivionAiService::travelArrivalRadius(const LiveActor& live) const
    {
        if (live.mState.mPackageType != ESM4::AIPackageType::Travel)
            return 0.f;
        const ESM4::AIPackage* current = package(live.mState.mPackage);
        if (current == nullptr && live.mTransientPackage && live.mTransientPackage->mFormKey == live.mState.mPackage)
            current = &*live.mTransientPackage;
        return current == nullptr ? 0.f : static_cast<float>(current->mLocationData.mRadius);
    }

    float OblivionAiService::unloadedMovementSpeed(const LiveActor& live) const
    {
        const ESM4::Npc* npc = mWorld.mStore.search<ESM4::Npc>(live.mState.mBase);
        const ESM4::Creature* creature = npc == nullptr
            ? mWorld.mStore.search<ESM4::Creature>(live.mState.mBase)
            : nullptr;
        if (npc == nullptr && creature == nullptr)
            return 0.f;

        const ESM4::AttributeValues attributes = npc != nullptr ? npc->mData.attribs : creature->mData.attribs;
        const float strength = static_cast<float>(attributes.strength);
        float encumbrance = 0.f;
        const auto addWeight = [&](const std::vector<ESM4::InventoryItem>& inventory) {
            for (const ESM4::InventoryItem& item : inventory)
            {
                if (item.count == 0)
                    continue;
                const ESM::FormId id = ESM::FormId::fromUint32(item.item);
                const auto definition = MWWorld::OblivionProfileServices::itemDefinition(
                    mWorld.mStore, ESM::RefId(id));
                if (definition)
                    encumbrance += definition->mWeight * static_cast<float>(ESM4::inventoryItemCount(item));
            }
        };
        if (npc != nullptr)
            addWeight(npc->mInventory);
        else
            addWeight(creature->mInventory);

        const ESM4::AIPackage* current = package(live.mState.mPackage);
        if (current == nullptr && live.mTransientPackage && live.mTransientPackage->mFormKey == live.mState.mPackage)
            current = &*live.mTransientPackage;
        const ESM4::AIPackage* script = package(live.mState.mScriptPackage);
        if (current == nullptr)
            current = script;
        const bool sneaking = current != nullptr && current->mPackageFlags.has(ESM4::PackageFlag::AlwaysSneak);
        const bool running = shouldRunPackage(live, current, live.mState.mLastValidPosition.asVec3());
        const float capacity = std::max(0.f, strength * 5.f);
        const float speed = static_cast<float>(attributes.speed);
        return running ? ESM4::playerRunSpeed(speed, encumbrance, capacity)
                       : ESM4::playerWalkSpeed(speed, encumbrance, capacity, sneaking);
    }

    bool OblivionAiService::advanceUnloadedMovement(
        LiveActor& live, float duration, bool& reached, std::optional<float> movementSpeed)
    {
        reached = false;
        if (!live.mDestination)
            return false;

        const float radius = travelArrivalRadius(live);
        if (live.mState.mPackageType == ESM4::AIPackageType::Travel
            && oblivionDestinationReached(live.mState.mCell, live.mState.mLastValidPosition.asVec3(),
                live.mDestinationCell, *live.mDestination, sArrivalTolerance, live.mState.mPackageType, radius))
        {
            reached = true;
            return true;
        }

        const ESM4::PathgridService& service = mWorld.mStore.getOblivionPathgridService();
        const float speed = movementSpeed ? *movementSpeed : unloadedMovementSpeed(live);
        float remaining = std::max(0.f, duration) * speed;
        auto moveToward = [&](const osg::Vec3f& destination) {
            osg::Vec3f position = live.mState.mLastValidPosition.asVec3();
            const osg::Vec3f delta = destination - position;
            const float length = delta.length();
            if (length <= 0.f)
                return false;
            const float distance = std::min(remaining, length);
            position += delta * (distance / length);
            remaining -= distance;
            live.mState.mLastValidPosition.pos[0] = position.x();
            live.mState.mLastValidPosition.pos[1] = position.y();
            live.mState.mLastValidPosition.pos[2] = position.z();
            live.mAbstractPositionDirty = true;
            return distance > 0.f;
        };

        while (live.mRouteCursor < live.mRoute.size())
        {
            const ESM4::PathgridNodeKey node = live.mRoute[live.mRouteCursor];
            const ESM4::PathgridGraph* nodeGraph = service.graph(node.mPathgrid);
            if (nodeGraph == nullptr || !nodeGraph->contains(node.mNode))
                return false;
            if (nodeGraph->cellKey() != live.mState.mCell)
            {
                const bool followingForeignEdge = live.mForeignRouteTarget && *live.mForeignRouteTarget == node;
                if (!followingForeignEdge)
                {
                    const ESM4::PathgridNodeKey source = live.mRouteCursor == 0
                        ? ESM4::PathgridNodeKey{ live.mState.mPathgrid, live.mState.mPathNode }
                        : live.mRoute[live.mRouteCursor - 1];
                    const ESM4::PathgridGraph* sourceGraph = service.graph(source.mPathgrid);
                    if (sourceGraph == nullptr || !sourceGraph->contains(source.mNode))
                        return false;
                    const ESM4::PathgridPoint point = sourceGraph->worldPoint(source.mNode);
                    const osg::Vec3f sourcePosition(point.mX, point.mY, point.mZ);
                    if (distanceSquared(live.mState.mLastValidPosition.asVec3(), sourcePosition)
                        > sArrivalTolerance * sArrivalTolerance)
                    {
                        static_cast<void>(moveToward(sourcePosition));
                        return true;
                    }
                    const std::optional<ESM::FormKey> door = doorForEdge(source, node);
                    if (door)
                    {
                        live.mForeignRouteTarget.reset();
                        live.mState.mDoor = *door;
                        reached = true;
                        return true;
                    }

                    // PGRI is a walkable connection between pathgrids.  Only
                    // PGRL object links require a door action.  Retain the
                    // target while the low-process position crosses the
                    // boundary so the next tick cannot steer back to source.
                    live.mForeignRouteTarget = node;
                }

                const ESM4::PathgridPoint point = nodeGraph->worldPoint(node.mNode);
                const osg::Vec3f nodePosition(point.mX, point.mY, point.mZ);
                if (distanceSquared(live.mState.mLastValidPosition.asVec3(), nodePosition)
                    > sArrivalTolerance * sArrivalTolerance)
                {
                    static_cast<void>(moveToward(nodePosition));
                    return true;
                }

                // Commit the logical CellStore identity only at the resolved
                // PGRI endpoint.  This is the state-only equivalent of a
                // resident actor walking across the boundary.
                live.mState.mCell = nodeGraph->cellKey();
                live.mState.mLastValidCell = nodeGraph->cellKey();
                live.mState.mPathgrid = node.mPathgrid;
                live.mState.mPathNode = node.mNode;
                live.mForeignRouteTarget.reset();
                ++live.mRouteCursor;
                if (remaining <= 0.f)
                    return true;
                continue;
            }

            const ESM4::PathgridPoint point = nodeGraph->worldPoint(node.mNode);
            const osg::Vec3f nodePosition(point.mX, point.mY, point.mZ);
            live.mState.mPathgrid = node.mPathgrid;
            live.mState.mPathNode = node.mNode;
            if (distanceSquared(live.mState.mLastValidPosition.asVec3(), nodePosition)
                <= sArrivalTolerance * sArrivalTolerance)
            {
                ++live.mRouteCursor;
                if (remaining <= 0.f)
                    return true;
                continue;
            }
            static_cast<void>(moveToward(nodePosition));
            if (distanceSquared(live.mState.mLastValidPosition.asVec3(), nodePosition)
                <= sArrivalTolerance * sArrivalTolerance)
            {
                ++live.mRouteCursor;
                if (remaining > 0.f)
                    continue;
            }
            return true;
        }

        if (live.mDestinationCell != live.mState.mCell && live.mRouteDoor)
        {
            if (distanceSquared(live.mState.mLastValidPosition.asVec3(), live.mRouteDoorPosition)
                > sArrivalTolerance * sArrivalTolerance)
            {
                static_cast<void>(moveToward(live.mRouteDoorPosition));
                return true;
            }
            live.mState.mDoor = *live.mRouteDoor;
            live.mRouteDoor.reset();
            reached = true;
            return true;
        }
        if (live.mDestinationCell != live.mState.mCell)
        {
            // Matching coordinates are not sufficient to arrive in another
            // CellStore.  A route may be empty when either endpoint has no
            // usable graph; that must remain a blocked cross-cell intent,
            // never an implicit teleport through the boundary.
            return false;
        }
        // Consuming the last graph node is not package arrival. The nearest
        // node can be hundreds of units from the authored/follow destination.
        // Spend only the remaining movement budget on the actual route tail.
        if (oblivionDestinationReached(live.mState.mCell, live.mState.mLastValidPosition.asVec3(),
                live.mDestinationCell, *live.mDestination, sArrivalTolerance, live.mState.mPackageType, radius))
        {
            reached = true;
            return true;
        }
        static_cast<void>(moveToward(*live.mDestination));
        reached = oblivionDestinationReached(live.mState.mCell, live.mState.mLastValidPosition.asVec3(),
            live.mDestinationCell, *live.mDestination, sArrivalTolerance, live.mState.mPackageType, radius);
        return true;
    }

    bool OblivionAiService::transitionUnloaded(LiveActor& live, const ESM4::PackagePhaseInput& input)
    {
        const ESM4::PackagePhase oldPhase = live.mState.mPhase;
        ESM4::PackagePhaseState phase = toPhaseState(live.mState);
        const ESM4::PackagePhaseTransition transition = ESM4::advancePackagePhase(phase, input);
        fromPhaseState(live.mState, phase);
        mPendingPackageDone.record(transition.mFrom, transition.mTo, live.mState.mActor, live.mState.mPackage);
        if (oldPhase == ESM4::PackagePhase::Door && live.mState.mPhase != oldPhase)
            live.mState.mDoorAnimationStarted = false;
        if (transition.mFrom != transition.mTo)
            logTransition(live, oldPhase, transition.mReason);
        if (live.mState.mPhase == ESM4::PackagePhase::Interrupted)
        {
            live.mRoute.clear();
            live.mContinuousRoute.clear();
            live.mRouteCursor = 0;
            live.mContinuousRouteCursor = 0;
            live.mForeignRouteTarget.reset();
            live.mRouteDoor.reset();
            live.mDestination.reset();
            live.mState.mActionReserved = false;
            live.mState.mActionItem = {};
            live.mState.mActionTimer = 0.f;
        }
        return transition.mFrom != transition.mTo;
    }

    void OblivionAiService::executeUnloadedFixedStep(LiveActor& live, float duration)
    {
        live.mSelectionCheckTimer = std::max(0.f, live.mSelectionCheckTimer - duration);
        if (live.mState.mDoorCooldown > 0.f)
            live.mState.mDoorCooldown = std::max(0.f, live.mState.mDoorCooldown - duration);

        const bool routeChanged = (live.mState.mPhase == ESM4::PackagePhase::Interrupted
                                      || live.mState.mPhase == ESM4::PackagePhase::Stalled)
            && !live.mState.mPathgrid.isNull()
            && live.mState.mRouteGeneration
                != mWorld.mStore.getOblivionPathgridService().generation(live.mState.mPathgrid);
        const bool doorRestored = live.mSelectionCheckTimer <= 0.f && doorInterruptionResolved(live);
        if (doorRestored)
            logEvent("door-access-restored", live, "door=" + live.mState.mDoor.serialize());
        if (live.mNeedsSelection
            || (live.mSelectionCheckTimer <= 0.f
                && (live.mState.mPhase == ESM4::PackagePhase::Interrupted
                    || live.mState.mPhase == ESM4::PackagePhase::Complete
                    || (live.mState.mPhase == ESM4::PackagePhase::Wait && live.mState.mPackage.isNull())
                    || routeChanged)))
        {
            const bool idleWait = live.mState.mPhase == ESM4::PackagePhase::Wait && live.mState.mPackage.isNull();
            const bool restart = live.mNeedsSelection || (!idleWait && (routeChanged || doorRestored));
            selectUnloaded(live, restart);
        }

        const ESM4::AIPackage* current = package(live.mState.mPackage);
        if (current == nullptr && live.mTransientPackage
            && live.mTransientPackage->mFormKey == live.mState.mPackage)
            current = &*live.mTransientPackage;
        if (current == nullptr)
        {
            setPhase(live, ESM4::PackagePhase::Wait);
            return;
        }

        if (live.mState.mPhase == ESM4::PackagePhase::Select)
        {
            transitionUnloaded(live, { duration, true, true, false, false, true, false, false, false, false, false,
                true });
            return;
        }
        if (live.mState.mPhase == ESM4::PackagePhase::Resolve)
        {
            live.mDestination = resolveUnloadedDestination(*current, live);
            if (!live.mDestination)
            {
                transitionUnloaded(live, { duration, false, false, false, false, false, false, false, false, false,
                    false, true });
                return;
            }
            if (live.mDestinationCell.isNull())
                live.mDestinationCell = live.mState.mCell;
            live.mState.mDestinationCell = live.mDestinationCell;
            live.mState.mDestinationPosition = live.mState.mLastValidPosition;
            live.mState.mDestinationPosition.pos[0] = live.mDestination->x();
            live.mState.mDestinationPosition.pos[1] = live.mDestination->y();
            live.mState.mDestinationPosition.pos[2] = live.mDestination->z();
            live.mState.mHasDestination = true;
            live.mRoute.clear();
            live.mContinuousRoute.clear();
            live.mRouteCursor = 0;
            live.mContinuousRouteCursor = 0;
            live.mForeignRouteTarget.reset();
            live.mRouteDoor.reset();
            const bool route = prepareUnloadedRoute(live);
            transitionUnloaded(live, { duration, true, route, false, false, true, false, false, false, false, false,
                true });
            return;
        }
        if (live.mState.mPhase == ESM4::PackagePhase::Path)
        {
            refreshMovingTargetRoute(live, *current);
            if (live.mRoute.empty() && live.mDestination)
                static_cast<void>(prepareUnloadedRoute(live));
            const osg::Vec3f previous = live.mState.mLastValidPosition.asVec3();
            bool reached = false;
            const bool route = advanceUnloadedMovement(live, duration, reached);
            const bool foreignDestination = !live.mDestinationCell.isNull()
                && live.mDestinationCell != live.mState.mCell;
            const bool door = reached && !live.mState.mDoor.isNull();
            const bool validReached = reached && (!foreignDestination || door);
            const bool validRoute = route && (!foreignDestination || !reached || door);
            const bool progress = validRoute && (validReached
                || distanceSquared(previous, live.mState.mLastValidPosition.asVec3()) > 0.01f);
            if (foreignDestination && !validRoute)
            {
                const std::string reason = door ? "foreign-edge"
                                                 : (live.mLastRouteFailure.empty()
                                                         ? "foreign-edge-without-door"
                                                         : live.mLastRouteFailure);
                logEvent("route-blocked", live, reason);
            }
            const bool permanentRouteFailure = !validRoute && !live.mLastRouteFailure.empty();
            transitionUnloaded(live, { duration, true, validRoute, validReached, door, !door, false, false, false, false, false,
                progress, validRoute, permanentRouteFailure });
            return;
        }
        if (live.mState.mPhase == ESM4::PackagePhase::Door)
        {
            // An actor with no resident CellStore cannot run the animation
            // action, but it can still commit a resolved, paired XTEL edge as
            // state-only progress. This is a logical door transition, not a
            // free position snap: the source route already reached the
            // source door, and the destination is the winning placed marker
            // from the same XTEL pair. Promotion later realizes the normal
            // resident activation/placement path from this stable state.
            const auto source = mUnloadedLocationByReference.find(live.mState.mDoor);
            if (source == mUnloadedLocationByReference.end())
            {
                transitionUnloaded(live,
                    { duration, true, true, true, true, false, false, false, false, false, false, true, false });
                return;
            }
            const UnloadedLocation& door = mUnloadedLocations[source->second];
            const auto destination = mUnloadedLocationByReference.find(door.mTeleportDoor);
            if (door.mType != ESM::REC_DOOR4
                || destination == mUnloadedLocationByReference.end())
            {
                transitionUnloaded(live,
                    { duration, true, true, true, true, false, false, false, false, false, false, true, false });
                return;
            }
            const UnloadedLocation& marker = mUnloadedLocations[destination->second];
            if (marker.mCell.isNull() || !currentDoorState(marker).mAvailable)
            {
                transitionUnloaded(live,
                    { duration, true, true, true, true, false, false, false, false, false, false, true, false });
                return;
            }
            bool locked = false;
            if (!canUseUnloadedDoor(live, door, locked))
            {
                transitionUnloaded(live,
                    { duration, true, true, true, true, false, locked, false, false, false, false, true, false });
                return;
            }
            live.mState.mLastTransitionDoor = live.mState.mDoor;
            live.mState.mDoorCooldown = 2.f;
            live.mState.mCell = marker.mCell;
            live.mState.mLastValidCell = marker.mCell;
            live.mState.mLastValidPosition.pos[0] = marker.mPosition.x();
            live.mState.mLastValidPosition.pos[1] = marker.mPosition.y();
            live.mState.mLastValidPosition.pos[2] = marker.mPosition.z();
            live.mAbstractPositionDirty = true;
            live.mRoute.clear();
            live.mContinuousRoute.clear();
            live.mRouteCursor = 0;
            live.mContinuousRouteCursor = 0;
            live.mForeignRouteTarget.reset();
            live.mRouteDoor.reset();
            live.mState.mPathgrid = {};
            live.mState.mPathNode = 0;
            live.mState.mDoor = {};
            logEvent("door-transition", live, "door=" + live.mState.mLastTransitionDoor.serialize()
                + " low-process-state-transition");
            transitionUnloaded(live,
                { duration, true, true, true, true, true, false, false, true, false, false, true, true });
            return;
        }
        if (live.mState.mPhase == ESM4::PackagePhase::Arrive)
        {
            transitionUnloaded(live, { duration, true, true, true, false, true, false, false, false,
                live.mState.mPackageType == ESM4::AIPackageType::CastMagic, false, true });
            return;
        }
        if (live.mState.mPhase == ESM4::PackagePhase::Act)
        {
            const bool timed = live.mState.mPackageType == ESM4::AIPackageType::Eat
                || live.mState.mPackageType == ESM4::AIPackageType::Sleep
                || live.mState.mPackageType == ESM4::AIPackageType::UseItemAt;
            if (timed && live.mState.mActionTimer == 0.f)
            {
                if (live.mState.mPackageType == ESM4::AIPackageType::Eat
                    || live.mState.mPackageType == ESM4::AIPackageType::UseItemAt)
                {
                    const ESM::FormKey requested = current->mTargetData.hasReference()
                        ? current->mTargetData.mReferenceKey
                        : ESM::FormKey{};
                    const ESM::FormKeyResolver resolver(mWorld.mContentFiles);
                    const auto chooseItem = [&](const auto* base) {
                        if (base == nullptr)
                            return ESM::FormKey{};
                        for (const ESM4::InventoryItem& item : base->mInventory)
                        {
                            const ESM::FormKey itemKey = resolver.toFormKey(ESM::FormId::fromUint32(item.item));
                            if (!requested.isNull() && requested != itemKey)
                                continue;
                            const auto definition = MWWorld::OblivionProfileServices::itemDefinition(
                                mWorld.mStore, ESM::RefId(ESM::FormId::fromUint32(item.item)));
                            if (!definition || (live.mState.mPackageType == ESM4::AIPackageType::Eat
                                    && definition->mType != ESM4::InventoryItemType::Ingredient && !definition->mFood)
                                || (live.mState.mPackageType == ESM4::AIPackageType::UseItemAt
                                    && !definition->mConsumable))
                                continue;
                            return itemKey;
                        }
                        return ESM::FormKey{};
                    };
                    live.mState.mActionItem = chooseItem(mWorld.mStore.search<ESM4::Npc>(live.mState.mBase));
                    if (live.mState.mActionItem.isNull())
                        live.mState.mActionItem = chooseItem(mWorld.mStore.search<ESM4::Creature>(live.mState.mBase));
                    if (live.mState.mActionItem.isNull())
                    {
                        live.mState.mInterruptionReason = "action-target-unresolved";
                        transitionUnloaded(live, { duration, true, true, true, false, true, false, false, false,
                            false, true, false });
                        return;
                    }
                }
                live.mState.mActionTimer = live.mState.mPackageType == ESM4::AIPackageType::Sleep
                    ? std::max(1.f, live.mState.mDurationRemaining * 3600.f)
                    : 1.f;
            }
            transitionUnloaded(live, { duration, true, true, true, false, true, false, timed, !timed,
                live.mState.mPackageType == ESM4::AIPackageType::CastMagic, false, true });
            return;
        }
        if (live.mState.mPhase == ESM4::PackagePhase::Wait)
        {
            if (followDestinationReached({}, *current, live, false))
            {
                transitionUnloaded(live,
                    { duration, true, true, true, false, true, false, false, true, false, false, true });
                return;
            }
            const bool movingTarget = live.mState.mPackageType == ESM4::AIPackageType::Follow
                || live.mState.mPackageType == ESM4::AIPackageType::Accompany
                || live.mState.mPackageType == ESM4::AIPackageType::Escort
                || live.mState.mPackageType == ESM4::AIPackageType::Pursue;
            if (movingTarget && live.mState.mDurationRemaining > std::max(0.f, duration) / 3600.f)
            {
                const std::optional<osg::Vec3f> destination = resolveUnloadedDestination(*current, live);
                if (!destination)
                {
                    live.mState.mInterruptionReason = "moving-target-unresolved";
                    transitionUnloaded(live,
                        { duration, true, true, true, false, true, false, false, false, false, true, false });
                    return;
                }
                if (updateOblivionMovingDestination(live.mState, live.mDestinationCell, *destination))
                {
                    live.mDestination = destination;
                    live.mRoute.clear();
                    live.mContinuousRoute.clear();
                    live.mRouteCursor = 0;
                    live.mContinuousRouteCursor = 0;
                    live.mForeignRouteTarget.reset();
                    live.mRouteDoor.reset();
                    live.mState.mPathgrid = {};
                    live.mState.mPathNode = 0;
                    live.mState.mDoor = {};
                    live.mState.mRepathAttempts = 0;
                    setPhase(live, ESM4::PackagePhase::Path);
                    static_cast<void>(prepareUnloadedRoute(live));
                    logTransition(live, ESM4::PackagePhase::Wait, "moving-target-repath");
                    live.mState.mDurationRemaining
                        = std::max(0.f, live.mState.mDurationRemaining - std::max(0.f, duration) / 3600.f);
                    return;
                }
            }
            const bool deferredItem = live.mState.mActionReserved
                && (live.mState.mPackageType == ESM4::AIPackageType::Eat
                    || live.mState.mPackageType == ESM4::AIPackageType::UseItemAt)
                && live.mState.mActionTimer > 0.f
                && live.mState.mActionTimer <= duration;
            if (deferredItem)
            {
                // Keep the reservation as a commit token. It is consumed only
                // after the actor is resident and the M13 inventory authority
                // can validate the exact item stack.
                live.mState.mActionTimer = 0.f;
                live.mState.mDurationRemaining = std::max(0.f,
                    live.mState.mDurationRemaining - duration / 3600.f);
                return;
            }
            const bool actionCompleted = live.mState.mActionReserved
                && live.mState.mPackageType == ESM4::AIPackageType::Sleep
                && live.mState.mActionTimer <= duration;
            transitionUnloaded(live, { duration, true, true, true, false, true, false, false, false,
                actionCompleted, false, true });
        }
    }

    void OblivionAiService::updateUnloaded(float duration)
    {
        if (!std::isfinite(duration) || duration <= 0.f)
            return;
        const float elapsed = std::clamp(duration, 0.f, 5.f);
        for (auto& [_, live] : mActors)
        {
            if (!loadedPtrFor(live.mState.mActor).isEmpty())
                continue;
            try
            {
                live.mState.mTier = ESM4::ProcessTier::Low;
                if (!std::isfinite(live.mState.mNextLowProcessTick)
                    || live.mState.mNextLowProcessTick < 0.f)
                {
                    Log(Debug::Warning) << "TES4 AI recovered an invalid low-process tick for actor "
                                        << live.mState.mActor.serialize();
                    live.mState.mNextLowProcessTick = 0.25f;
                }
                else if (live.mState.mNextLowProcessTick == 0.f)
                    live.mState.mNextLowProcessTick = 0.25f;
                live.mState.mNextLowProcessTick -= elapsed;
                unsigned steps = 0;
                while (live.mState.mNextLowProcessTick <= 0.f && steps < 8)
                {
                    executeUnloadedFixedStep(live, 0.25f);
                    live.mState.mNextLowProcessTick += 0.25f;
                    ++steps;
                }
                if (steps == 8 && live.mState.mNextLowProcessTick < 0.f)
                    live.mState.mNextLowProcessTick = 0.f;
                if (!std::isfinite(live.mState.mNextLowProcessTick)
                    || live.mState.mNextLowProcessTick < 0.f)
                    live.mState.mNextLowProcessTick = 0.f;
            }
            catch (const std::exception& error)
            {
                Log(Debug::Error) << "TES4 low-process AI update failed for actor "
                                  << live.mState.mActor.serialize() << ": " << error.what();
            }
        }
    }

    std::optional<ESM::FormKey> OblivionAiService::doorForEdge(
        const ESM4::PathgridNodeKey& source, const ESM4::PathgridNodeKey& destination) const
    {
        const auto isDoorReference = [this](const MWWorld::Ptr& candidate, const ESM::FormKey& key) {
            if (candidate.isEmpty() || !candidate.getRefData().isEnabled() || candidate.mRef->isDeleted())
                return false;
            if (candidate.getClass().isDoor())
                return true;

            // A resident pointer normally has the class selected from its
            // resolved base record. Keep the stable-base check as a guard for
            // references loaded during a cell transition, where the pointer
            // can briefly still expose the generic placed-reference class.
            const ESM4::Reference* reference = mWorld.mStore.get<ESM4::Reference>().search(key);
            return reference != nullptr && mWorld.mStore.search<ESM4::Door>(reference->mBaseKey) != nullptr;
        };
        const ESM4::PathgridService& service = mWorld.mStore.getOblivionPathgridService();
        const ESM4::PathgridGraph* graph = service.graph(source.mPathgrid);
        if (graph == nullptr)
            return std::nullopt;

        std::optional<ESM::FormKey> result;
        for (const ESM4::PathgridForeignLink& link : graph->foreignLinks())
        {
            if (link.mSourceNode != source.mNode || link.mAmbiguous || !link.mDestination
                || *link.mDestination != destination)
                continue;
            for (const ESM4::PathgridObjectLink& object : graph->objectLinks())
            {
                if (std::find(object.mNodes.begin(), object.mNodes.end(), source.mNode) == object.mNodes.end())
                    continue;
                if (object.mKind != ESM4::PathgridObjectKind::Door)
                    continue;
                const MWWorld::Ptr candidate = loadedPtrFor(object.mObject);
                if (candidate.isEmpty())
                {
                    // Low-process routing must not force-load a destination
                    // cell just to identify the door at a foreign edge. Use
                    // the immutable placed-reference index first; the store
                    // lookup is retained only for callers that reach this
                    // helper before the index has a matching reference.
                    const auto indexed = mUnloadedLocationByReference.find(object.mObject);
                    if (indexed != mUnloadedLocationByReference.end())
                    {
                        const UnloadedLocation& location = mUnloadedLocations[indexed->second];
                        if (location.mType != ESM::REC_DOOR4 || !currentDoorState(location).mAvailable)
                            continue;
                    }
                    else
                    {
                        const ESM4::Reference* reference
                            = mWorld.mStore.get<ESM4::Reference>().search(object.mObject);
                        if (reference == nullptr
                            || static_cast<ESM::RecNameInts>(mWorld.mStore.find(reference->mBaseObj))
                                != ESM::REC_DOOR4)
                            continue;
                    }
                }
                else if (!isDoorReference(candidate, object.mObject))
                    continue;
                if (!result || object.mObject < *result)
                    result = object.mObject;
            }
        }
        return result;
    }

    bool OblivionAiService::canUseDoor(const MWWorld::Ptr& actor, const LiveActor& live, bool& locked) const
    {
        locked = false;
        if (live.mState.mDoor.isNull())
            return false;
        if (const auto indexed = mUnloadedLocationByReference.find(live.mState.mDoor);
            indexed != mUnloadedLocationByReference.end())
            return canUseUnloadedDoor(live, mUnloadedLocations[indexed->second], locked, &actor);
        const MWWorld::Ptr door = loadedPtrFor(live.mState.mDoor);
        ESM::RefId key;
        ESM::FormKey owner;
        const ESM::FormKeyResolver resolver(mWorld.mContentFiles);
        if (door.isEmpty())
        {
            return false;
        }
        else
        {
            const ESM4::Reference* reference = mWorld.mStore.get<ESM4::Reference>().search(live.mState.mDoor);
            const bool isDoor = door.getClass().isDoor()
                || (reference != nullptr && mWorld.mStore.search<ESM4::Door>(reference->mBaseKey) != nullptr);
            if (!isDoor || !door.getRefData().isEnabled() || door.mRef->isDeleted())
                return false;
            if (!door.getCellRef().isLocked())
                return true;
            key = door.getCellRef().getKey();
            const ESM::RefId doorOwner = door.getCellRef().getOwner();
            if (const ESM::FormId* id = doorOwner.getIf<ESM::FormId>())
                owner = resolver.toFormKey(*id);
        }
        bool ownsConnectedCell = false;
        if (const auto indexed = mUnloadedLocationByReference.find(live.mState.mDoor);
            indexed != mUnloadedLocationByReference.end())
        {
            const UnloadedLocation& location = mUnloadedLocations[indexed->second];
            ownsConnectedCell = hasCellOwnershipPermission(live, location.mCell, &actor);
            if (!ownsConnectedCell)
                if (const auto marker = mUnloadedLocationByReference.find(location.mTeleportDoor);
                    marker != mUnloadedLocationByReference.end())
                    ownsConnectedCell
                        = hasCellOwnershipPermission(live, mUnloadedLocations[marker->second].mCell, &actor);
        }
        if (hasDoorOwnershipPermission(live, owner, &actor) || ownsConnectedCell)
            return true;
        const bool hasKey = !key.empty() && !actor.getClass().getContainerStore(actor).search(key).isEmpty();
        locked = !hasKey;
        return hasKey;
    }

    bool OblivionAiService::hasDoorOwnershipPermission(
        const LiveActor& live, const ESM::FormKey& owner, const MWWorld::Ptr* actor) const
    {
        if (owner.isNull())
            return false;
        if (owner == live.mState.mActor || owner == live.mState.mBase)
            return true;

        // Oblivion's GenericOwner faction is an ownership/crime marker for
        // the player, not a faction that stock NPCs join. City gates and
        // other scheduled AI boundaries use it on locked doors, so native
        // actors may traverse those routes without silently generalizing the
        // exception to arbitrary owned locks.
        static const ESM::FormKey sGenericOwner = ESM::FormKey::content("oblivion.esm", 0x0534f2);
        if (owner == sGenericOwner
            && (mWorld.mStore.search<ESM4::Npc>(live.mState.mBase) != nullptr
                || mWorld.mStore.search<ESM4::Creature>(live.mState.mBase) != nullptr))
            return true;

        const auto matchesFaction = [&](ESM::FormId value) {
            if (value.isZeroOrUnset() || !value.hasContentFile())
                return false;
            std::size_t contentFile = static_cast<std::size_t>(value.mContentFile);
            if (contentFile >= mWorld.mContentFiles.size())
            {
                // Loaded TES4 references use the file-collection index, which
                // includes the builtin script slot that World::mContentFiles omits.
                if (contentFile == 0 || --contentFile >= mWorld.mContentFiles.size())
                    return false;
            }
            return ESM::FormKey::content(mWorld.mContentFiles[contentFile], value.mIndex) == owner;
        };
        if (actor != nullptr && !actor->isEmpty())
        {
            const ESM::RefId faction = actor->getClass().getPrimaryFaction(*actor);
            if (const ESM::FormId* id = faction.getIf<ESM::FormId>(); id != nullptr && matchesFaction(*id))
                return true;
        }

        const auto containsFaction = [&matchesFaction](const auto* base) {
            return base != nullptr
                && std::any_of(base->mFactions.begin(), base->mFactions.end(), [&matchesFaction](const auto& item) {
                       return item.faction != 0 && matchesFaction(ESM::FormId::fromUint32(item.faction));
                   });
        };
        if (containsFaction(mWorld.mStore.search<ESM4::Npc>(live.mState.mBase))
            || containsFaction(mWorld.mStore.search<ESM4::Creature>(live.mState.mBase)))
            return true;
        return false;
    }

    bool OblivionAiService::hasCellOwnershipPermission(
        const LiveActor& live, const ESM::FormKey& cell, const MWWorld::Ptr* actor) const
    {
        if (cell.isNull())
            return false;
        const ESM4::Cell* record = mWorld.mStore.search<ESM4::Cell>(cell);
        if (record == nullptr || record->mOwner == ESM::FormId{})
            return false;
        const ESM::FormKey owner = ESM::FormKeyResolver(mWorld.mContentFiles).toFormKey(record->mOwner);
        return hasDoorOwnershipPermission(live, owner, actor);
    }

    bool OblivionAiService::doorInterruptionResolved(const LiveActor& live, const MWWorld::Ptr* actor) const
    {
        if (!isOblivionDoorInterruption(live.mState))
            return false;
        bool locked = false;
        if (actor != nullptr)
            return canUseDoor(*actor, live, locked);
        const auto indexed = mUnloadedLocationByReference.find(live.mState.mDoor);
        return indexed != mUnloadedLocationByReference.end()
            && canUseUnloadedDoor(live, mUnloadedLocations[indexed->second], locked);
    }

    OblivionDoorState OblivionAiService::currentDoorState(const UnloadedLocation& door) const
    {
        const OblivionDoorState authored{ door.mEnabled, door.mLocked, door.mKey, door.mOwner };
        if (const MWWorld::Ptr loadedDoor = loadedPtrFor(door.mReference); !loadedDoor.isEmpty())
        {
            OblivionDoorState resident{ loadedDoor.getRefData().isEnabled() && !loadedDoor.mRef->isDeleted(),
                loadedDoor.getCellRef().isLocked(), loadedDoor.getCellRef().getKey(), {} };
            const ESM::RefId owner = loadedDoor.getCellRef().getOwner();
            if (const ESM::FormId* id = owner.getIf<ESM::FormId>())
            {
                std::size_t contentFile = static_cast<std::size_t>(id->mContentFile);
                if (contentFile >= mWorld.mContentFiles.size() && contentFile > 0)
                    --contentFile;
                if (contentFile < mWorld.mContentFiles.size())
                    resident.mOwner = ESM::FormKey::content(
                        mWorld.mContentFiles[contentFile], id->mIndex);
            }
            return resolveOblivionDoorState(authored, nullptr, resident);
        }
        else if (mWorld.mOblivionRuntimeState)
        {
            const auto savedDoor = std::find_if(mWorld.mOblivionRuntimeState->mReferences.begin(),
                mWorld.mOblivionRuntimeState->mReferences.end(),
                [&door](const ESM4::RuntimeReferenceState& reference) {
                    return reference.mKey == door.mReference;
                });
            if (savedDoor != mWorld.mOblivionRuntimeState->mReferences.end())
                return resolveOblivionDoorState(authored, &*savedDoor, std::nullopt);
        }
        return authored;
    }

    bool OblivionAiService::canUseUnloadedDoor(
        const LiveActor& live, const UnloadedLocation& door, bool& locked, const MWWorld::Ptr* actor) const
    {
        locked = false;
        if (door.mType != ESM::REC_DOOR4)
            return false;
        const OblivionDoorState current = currentDoorState(door);
        if (!current.mAvailable)
            return false;
        if (!door.mTeleportDoor.isNull())
        {
            const auto marker = mUnloadedLocationByReference.find(door.mTeleportDoor);
            if (marker == mUnloadedLocationByReference.end()
                || !currentDoorState(mUnloadedLocations[marker->second]).mAvailable)
                return false;
        }
        if (!current.mLocked)
            return true;
        bool ownsConnectedCell = hasCellOwnershipPermission(live, door.mCell, actor);
        if (!ownsConnectedCell)
            if (const auto marker = mUnloadedLocationByReference.find(door.mTeleportDoor);
                marker != mUnloadedLocationByReference.end())
                ownsConnectedCell = hasCellOwnershipPermission(live, mUnloadedLocations[marker->second].mCell, actor);
        if (hasDoorOwnershipPermission(live, current.mOwner, actor) || ownsConnectedCell)
            return true;

        const ESM::RefId& doorKey = current.mKey;
        const ESM::FormId* keyId = doorKey.getIf<ESM::FormId>();
        if (keyId == nullptr)
        {
            locked = true;
            return false;
        }
        const ESM::FormKeyResolver resolver(mWorld.mContentFiles);
        const ESM::FormKey key = resolver.toFormKey(*keyId);
        if (key.isNull())
        {
            locked = true;
            return false;
        }

        // An empty live/saved inventory is authoritative too. Falling back
        // to the base inventory would resurrect a key removed at runtime.
        const MWWorld::Ptr residentActor = actor != nullptr ? *actor : loadedPtrFor(live.mState.mActor);
        if (!residentActor.isEmpty())
        {
            const bool hasKey = !residentActor.getClass().getContainerStore(residentActor).search(doorKey).isEmpty();
            locked = !hasKey;
            return hasKey;
        }

        const auto containsKey = [key](const auto& inventory) {
            return std::any_of(inventory.begin(), inventory.end(), [key](const auto& item) {
                return item.mCount > 0 && item.mBase == key;
            });
        };
        if (mWorld.mOblivionRuntimeState)
        {
            const auto saved = std::find_if(mWorld.mOblivionRuntimeState->mReferences.begin(),
                mWorld.mOblivionRuntimeState->mReferences.end(),
                [&live](const ESM4::RuntimeReferenceState& reference) {
                    return reference.mKey == live.mState.mActor;
                });
            if (saved != mWorld.mOblivionRuntimeState->mReferences.end())
            {
                const bool hasKey = containsKey(saved->mInventory);
                locked = !hasKey;
                return hasKey;
            }
        }

        if (const ESM4::Npc* npc = mWorld.mStore.search<ESM4::Npc>(live.mState.mBase))
        {
            if (std::any_of(npc->mInventory.begin(), npc->mInventory.end(), [key, &resolver](const auto& item) {
                    return ESM4::inventoryItemCount(item) > 0 && resolver.toFormKey(ESM::FormId::fromUint32(item.item)) == key;
                }))
                return true;
        }
        if (const ESM4::Creature* creature = mWorld.mStore.search<ESM4::Creature>(live.mState.mBase))
        {
            if (std::any_of(creature->mInventory.begin(), creature->mInventory.end(), [key, &resolver](const auto& item) {
                    return ESM4::inventoryItemCount(item) > 0 && resolver.toFormKey(ESM::FormId::fromUint32(item.item)) == key;
                }))
                return true;
        }

        locked = true;
        return false;
    }

    bool OblivionAiService::reserveAction(
        const MWWorld::Ptr& actor, LiveActor& live, const ESM4::AIPackage& record)
    {
        if (live.mState.mActionReserved)
            return true;
        if (record.mPackageType == ESM4::AIPackageType::Sleep)
            return true;

        const bool eat = record.mPackageType == ESM4::AIPackageType::Eat;
        const bool use = record.mPackageType == ESM4::AIPackageType::UseItemAt;
        if (!eat && !use)
            return true;

        const ESM::FormKey requested = record.mTargetData.hasReference() ? record.mTargetData.mReferenceKey
                                                                           : ESM::FormKey{};
        const ESM::FormKeyResolver resolver(mWorld.mContentFiles);
        std::vector<std::pair<ESM::FormKey, MWWorld::Ptr>> candidates;
        MWWorld::ContainerStore& inventory = actor.getClass().getContainerStore(actor);
        for (MWWorld::ContainerStoreIterator iterator = inventory.begin(); iterator != inventory.end(); ++iterator)
        {
            const MWWorld::Ptr item = *iterator;
            const ESM::RefId nativeId
                = MWWorld::OblivionProfileServices::nativeItemId(mWorld.mStore, item.getCellRef().getRefId());
            const ESM::FormId* formId = nativeId.getIf<ESM::FormId>();
            if (formId == nullptr)
                continue;
            const ESM::FormKey key = resolver.toFormKey(*formId);
            if (!requested.isNull() && key != requested)
                continue;
            const auto definition = MWWorld::OblivionProfileServices::itemDefinition(mWorld.mStore, nativeId);
            if (!definition)
                continue;
            if (eat && definition->mType != ESM4::InventoryItemType::Ingredient && !definition->mFood)
                continue;
            if (use && !definition->mConsumable)
                continue;
            if (item.getCellRef().getCount() <= 0)
                continue;
            candidates.emplace_back(key, item);
        }
        std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
            return left.first < right.first;
        });
        if (candidates.empty())
            return false;
        live.mState.mActionItem = candidates.front().first;
        logEvent("action-reserve", live, "item=" + live.mState.mActionItem.serialize());
        return true;
    }

    bool OblivionAiService::completeAction(const MWWorld::Ptr& actor, LiveActor& live)
    {
        if (live.mState.mActionItem.isNull())
            return true; // Sleep has no inventory side effect.
        const ESM::FormKey itemKey = live.mState.mActionItem;
        const ESM::FormKeyResolver resolver(mWorld.mContentFiles);
        const std::optional<ESM::FormId> nativeId = resolver.toFormId(itemKey);
        if (!nativeId)
            return false;
        const ESM::RefId sharedId = MWWorld::OblivionProfileServices::sharedItemId(mWorld.mStore, ESM::RefId(*nativeId));
        const MWWorld::Ptr item = actor.getClass().getContainerStore(actor).search(sharedId);
        if (item.isEmpty())
            return false;
        if (live.mState.mPackageType == ESM4::AIPackageType::Eat)
        {
            // Eating is an M14 inventory transaction. Calling the generic
            // item-use action here would apply potion/alchemy effects and
            // cross the M17 boundary. Remove exactly one reserved stack
            // unit only after the timed action reaches its commit point.
            const int removed = actor.getClass().getContainerStore(actor).remove(item, 1);
            if (removed != 1)
                return false;
            logEvent("action-commit", live, "eat=" + itemKey.serialize());
            live.mState.mActionItem = {};
            return true;
        }
        std::unique_ptr<MWWorld::Action> action = item.getClass().use(item, true);
        if (!action || action->isNullAction())
            return false;
        action->execute(actor, true);
        logEvent("action-commit", live, "item=" + itemKey.serialize());
        live.mState.mActionItem = {};
        return true;
    }

    bool OblivionAiService::reconcileAbstractPosition(
        const MWWorld::Ptr& actor, LiveActor& live, bool allowCellChange)
    {
        if (!live.mAbstractPositionDirty)
            return true;

        const osg::Vec3f abstractPosition = live.mState.mLastValidPosition.asVec3();
        const ESM::FormKey currentCell = cellKey(actor);
        const ESM4::PathgridService& service = mWorld.mStore.getOblivionPathgridService();
        bool valid = actor.isInCell() && live.mState.mLastValidCell == live.mState.mCell;

        if (valid)
        {
            constexpr float validationTolerance = 128.f;
            const float validationDistance = validationTolerance * validationTolerance;
            if (!live.mRoute.empty())
            {
                valid = false;
                std::optional<osg::Vec3f> previous;
                for (const ESM4::PathgridNodeKey& node : live.mRoute)
                {
                    const ESM4::PathgridGraph* graph = service.graph(node.mPathgrid);
                    if (graph == nullptr || !graph->contains(node.mNode))
                    {
                        valid = false;
                        break;
                    }
                    const ESM4::PathgridPoint point = graph->worldPoint(node.mNode);
                    const osg::Vec3f position(point.mX, point.mY, point.mZ);
                    if (previous && distanceToSegmentSquared(abstractPosition, *previous, position)
                        <= validationDistance)
                    {
                        valid = true;
                        break;
                    }
                    previous = position;
                }
                if (!valid && previous)
                    valid = distanceSquared(abstractPosition, *previous) <= validationDistance;
            }
            else if (live.mState.mHasDestination && live.mState.mDestinationCell == live.mState.mCell)
            {
                const osg::Vec3f destination(live.mState.mDestinationPosition.pos[0],
                    live.mState.mDestinationPosition.pos[1], live.mState.mDestinationPosition.pos[2]);
                valid = distanceSquared(abstractPosition, destination) <= validationDistance;
            }
            else if (!live.mState.mPathgrid.isNull())
            {
                const ESM4::PathgridGraph* graph = service.graph(live.mState.mPathgrid);
                if (graph != nullptr && graph->contains(live.mState.mPathNode))
                {
                    const ESM4::PathgridPoint point = graph->worldPoint(live.mState.mPathNode);
                    valid = distanceSquared(abstractPosition, { point.mX, point.mY, point.mZ })
                        <= 2048.f * 2048.f;
                }
                else
                    valid = false;
            }
            else
                valid = false;
        }

        if (!valid)
        {
            logEvent("low-process-reconcile-failed", live, "bounded-position-validation");
            live.mState.mCell = currentCell;
            live.mState.mLastValidCell = currentCell;
            live.mState.mLastValidPosition = actor.getRefData().getPosition();
            live.mAbstractPositionDirty = false;
            live.mRoute.clear();
            live.mContinuousRoute.clear();
            live.mRouteCursor = 0;
            live.mContinuousRouteCursor = 0;
            live.mForeignRouteTarget.reset();
            live.mRouteDoor.reset();
            live.mState.mPathgrid = {};
            live.mState.mPathNode = 0;
            live.mState.mDoor = {};
            live.mState.mRepathAttempts = 0;
            if (live.mState.mPhase == ESM4::PackagePhase::Door)
                setPhase(live, ESM4::PackagePhase::Path);
            return false;
        }

        if (currentCell != live.mState.mCell && !allowCellChange)
        {
            // Changing CellStores can add/remove mechanics actors. Defer it
            // until the actor-update iteration has finished, like door use.
            live.mPendingRealization = true;
            return false;
        }

        try
        {
            const auto destinationId = ESM::FormKeyResolver(mWorld.mContentFiles).toFormId(live.mState.mCell);
            if (!destinationId)
                throw std::runtime_error("abstract destination cell is not a native cell");
            MWWorld::CellStore& destinationCell = mWorld.mWorldModel.getCell(ESM::RefId(*destinationId));
            const MWWorld::Ptr moved = mWorld.moveObject(actor, &destinationCell, abstractPosition, true, false);
            if (moved.isEmpty())
                throw std::runtime_error("moveObject returned an empty actor pointer");
            live.mState.mLastValidPosition = moved.getRefData().getPosition();
            live.mState.mLastValidCell = cellKey(moved);
            live.mAbstractPositionDirty = false;
            // Logical routes do not contain a resident Recast corridor. The
            // next update must build one from the newly realized position.
            if (live.mState.mPhase == ESM4::PackagePhase::Path)
            {
                live.mRoute.clear();
                live.mContinuousRoute.clear();
                live.mRouteCursor = 0;
                live.mContinuousRouteCursor = 0;
                live.mContinuousRouteEndCursor.reset();
                live.mForeignRouteTarget.reset();
                live.mRouteDoor.reset();
            }
            logEvent("low-process-reconcile", live,
                "bounded-route-position from=" + currentCell.serialize() + " to=" + live.mState.mCell.serialize());
            return true;
        }
        catch (const std::exception& error)
        {
            Log(Debug::Warning) << "TES4 AI low-process position reconciliation failed for actor "
                                << live.mState.mActor.serialize() << ": " << error.what();
            logEvent("low-process-reconcile-failed", live, "move-object");
            live.mState.mCell = currentCell;
            live.mState.mLastValidCell = currentCell;
            live.mState.mLastValidPosition = actor.getRefData().getPosition();
            live.mAbstractPositionDirty = false;
            live.mRoute.clear();
            live.mContinuousRoute.clear();
            live.mRouteCursor = 0;
            live.mContinuousRouteCursor = 0;
            live.mForeignRouteTarget.reset();
            live.mRouteDoor.reset();
            live.mState.mPathgrid = {};
            live.mState.mPathNode = 0;
            live.mState.mDoor = {};
            live.mState.mRepathAttempts = 0;
            if (live.mState.mPhase == ESM4::PackagePhase::Door)
                setPhase(live, ESM4::PackagePhase::Path);
            return false;
        }
    }

    bool OblivionAiService::updateMountedAttachment(const MWWorld::Ptr& rider, LiveActor& live, bool highProcess)
    {
        if (live.mState.mMount.isNull())
            return false;

        const ESM::FormKey horseKey = live.mState.mMount;
        const MWWorld::Ptr horse = ptrFor(horseKey);
        const auto horseFound = mActors.find(horseKey);
        const ESM::FormKey horseCell = horseFound != mActors.end() ? horseFound->second.mState.mCell
                                                                     : cellKey(horse);
        if (!horseCell.isNull() && horseCell != live.mState.mCell)
        {
            // A mounted actor cannot cross a door independently.  The door
            // coordinator will reselect a normal follow/travel route after a
            // clean, explicit dismount at the boundary.
            dismountHorse(rider);
            logEvent("horse-boundary-dismount", live, "cell-changed");
            return true;
        }

        if (horse.isEmpty() && horseFound == mActors.end())
        {
            stopMovement(rider);
            return true;
        }

        osg::Vec3f horsePosition;
        if (highProcess && !horse.isEmpty())
            horsePosition = horse.getRefData().getPosition().asVec3();
        else if (horseFound != mActors.end())
            horsePosition = horseFound->second.mState.mLastValidPosition.asVec3();
        else
            horsePosition = horse.getRefData().getPosition().asVec3();

        float saddleHeight = 48.f;
        if (!horse.isEmpty())
            if (const auto* reference = horse.get<ESM4::Creature>(); reference != nullptr && reference->mBase != nullptr)
                saddleHeight = std::max(saddleHeight, reference->mBase->mBoundRadius * 0.35f);
        const osg::Vec3f attachedPosition = horsePosition + osg::Vec3f(0.f, 0.f, saddleHeight);

        if (highProcess && !horse.isEmpty() && rider.isInCell() && horse.isInCell())
        {
            if (distanceSquared(rider.getRefData().getPosition().asVec3(), attachedPosition) > 1.f)
                try
                {
                    mWorld.moveObject(rider, rider.getCell(), attachedPosition, true, false);
                }
                catch (const std::exception& error)
                {
                    Log(Debug::Warning) << "TES4 AI horse attachment failed for actor "
                                        << live.mState.mActor.serialize() << ": " << error.what();
                    dismountHorse(rider);
                    return true;
                }
            live.mState.mLastValidPosition = rider.getRefData().getPosition();
            live.mState.mLastValidCell = live.mState.mCell;
            live.mAbstractPositionDirty = false;
        }
        else
        {
            live.mState.mLastValidPosition = rider.getRefData().getPosition();
            live.mState.mLastValidPosition.pos[0] = attachedPosition.x();
            live.mState.mLastValidPosition.pos[1] = attachedPosition.y();
            live.mState.mLastValidPosition.pos[2] = attachedPosition.z();
            live.mState.mLastValidCell = live.mState.mCell;
            live.mAbstractPositionDirty = true;
        }
        stopMovement(rider);
        return true;
    }

    bool OblivionAiService::advanceMovement(const MWWorld::Ptr& actor, LiveActor& live, float duration,
        bool highProcess, bool& reached)
    {
        reached = false;
        if (!live.mDestination)
            return false;

        const ESM4::PathgridService& service = mWorld.mStore.getOblivionPathgridService();
        osg::Vec3f destination = *live.mDestination;
        osg::Vec3f position = highProcess
            ? actor.getRefData().getPosition().asVec3()
            : live.mState.mLastValidPosition.asVec3();
        const ESM4::AIPackage* current = package(live.mState.mPackage);
        if (current == nullptr && live.mTransientPackage && live.mTransientPackage->mFormKey == live.mState.mPackage)
            current = &*live.mTransientPackage;
        const bool sneak = current != nullptr && current->mPackageFlags.has(ESM4::PackageFlag::AlwaysSneak);
        const bool run = shouldRunPackage(live, current, position);
        // Resident low-process movement uses the same native stats, inventory
        // and immobilization checks as high process. Set the requested gait
        // before querying speed; getMaxSpeed observes the movement flags.
        CreatureStats& stats = actor.getClass().getCreatureStats(actor);
        stats.setMovementFlag(CreatureStats::Flag_Run, run);
        stats.setMovementFlag(CreatureStats::Flag_Sneak, sneak);
        const float speed = std::max(0.f, actor.getClass().getMaxSpeed(actor));
        if (!highProcess)
            return advanceUnloadedMovement(live, duration, reached, speed);

        const auto useBlockingDoor = [&](const osg::Vec3f& target) {
            if (live.mState.mDoorCooldown > 0.f)
                return false;
            const MWWorld::Ptr door = findBlockingOblivionDoor(actor, *mWorld.mPhysics, target, 192.f);
            if (door.isEmpty())
                return false;
            live.mState.mDoor = actorKey(door);
            live.mState.mDoorAnimationStarted = false;
            live.mPhysicalDoorOpened = false;
            bool locked = false;
            const bool usable = canUseDoor(actor, live, locked);
            reached = true;
            stopMovement(actor);
            logEvent("door-approach", live, "door=" + live.mState.mDoor.serialize()
                    + " same-cell=true usable=" + (usable ? "true" : "false")
                    + " locked=" + (locked ? "true" : "false"));
            return true;
        };
        const float radius = travelArrivalRadius(live);
        if (live.mState.mPackageType == ESM4::AIPackageType::Travel
            && oblivionDestinationReached(live.mState.mCell, position,
                live.mDestinationCell.isNull() ? live.mState.mCell : live.mDestinationCell,
                destination, sArrivalTolerance, live.mState.mPackageType, radius))
        {
            reached = true;
            return true;
        }

        if (!live.mContinuousRoute.empty())
        {
            while (live.mContinuousRouteCursor < live.mContinuousRoute.size())
            {
                const osg::Vec3f& point = live.mContinuousRoute[live.mContinuousRouteCursor];
                if (distanceSquared(position, point) <= sArrivalTolerance * sArrivalTolerance)
                {
                    ++live.mContinuousRouteCursor;
                    continue;
                }

                if (useBlockingDoor(point))
                    return true;
                faceAndMove(actor, point, run, sneak);
                return true;
            }

            if (live.mContinuousRouteEndCursor)
            {
                live.mRouteCursor = std::max(live.mRouteCursor, *live.mContinuousRouteEndCursor);
                live.mContinuousRouteEndCursor.reset();
            }

            if (live.mDestinationCell.isNull() || live.mDestinationCell == live.mState.mCell)
            {
                // findSmoothPath normally ends within the requested tolerance.
                // Keep the final arrival check explicit so a partial output
                // cannot be interpreted as arrival.
                if (oblivionDestinationReached(live.mState.mCell, position, live.mState.mCell,
                        destination, sArrivalTolerance, live.mState.mPackageType, radius))
                {
                    reached = true;
                    return true;
                }
                // Detour's coarse endpoint is not the final Travel standing
                // slot. Finish a bounded residual under ordinary collision
                // instead of interpreting a consumed corridor as route loss.
                if (oblivionRouteTailWithinDirectApproach(
                        position, destination, sRouteTailApproachDistance))
                {
                    if (useBlockingDoor(destination))
                        return true;
                    faceAndMove(actor, destination, run, sneak);
                    return true;
                }
                return false;
            }
            // The continuous segment ends at a real native boundary. Resume
            // the PGRD/PGRL route below to cross it through its typed path.
        }

        if (live.mRouteCursor < live.mRoute.size())
        {
            const ESM4::PathgridNodeKey node = live.mRoute[live.mRouteCursor];
            const ESM4::PathgridGraph* graph = service.graph(node.mPathgrid);
            if (graph == nullptr)
                return false;
            if (graph->cellKey() != live.mState.mCell)
            {
                const bool followingForeignEdge = live.mForeignRouteTarget && *live.mForeignRouteTarget == node;
                if (!followingForeignEdge)
                {
                    const ESM4::PathgridNodeKey source = live.mRouteCursor == 0
                        ? ESM4::PathgridNodeKey{ live.mState.mPathgrid, live.mState.mPathNode }
                        : live.mRoute[live.mRouteCursor - 1];
                    const ESM4::PathgridGraph* sourceGraph = service.graph(source.mPathgrid);
                    if (sourceGraph == nullptr || !sourceGraph->contains(source.mNode))
                        return false;
                    const ESM4::PathgridPoint sourcePoint = sourceGraph->worldPoint(source.mNode);
                    const osg::Vec3f sourcePosition(sourcePoint.mX, sourcePoint.mY, sourcePoint.mZ);
                    const float distanceToSource = (sourcePosition - position).length();
                    if (distanceToSource > sArrivalTolerance)
                    {
                        faceAndMove(actor, sourcePosition, run, sneak);
                        return true;
                    }
                    const std::optional<ESM::FormKey> door = doorForEdge(source, node);
                    if (door)
                    {
                        live.mForeignRouteTarget.reset();
                        live.mState.mDoor = *door;
                        reached = true;
                        return true;
                    }

                    // PGRI is a walkable foreign connection.  PGRL object
                    // links are the only cross-cell edges that enter Door.
                    live.mForeignRouteTarget = node;
                }

                const ESM4::PathgridPoint foreignPoint = graph->worldPoint(node.mNode);
                const osg::Vec3f foreignPosition(foreignPoint.mX, foreignPoint.mY, foreignPoint.mZ);
                // A resident actor must cross the actual cell boundary.  The
                // scene/physics layer updates the identity; this controller
                // only keeps steering toward the resolved PGRI endpoint.
                faceAndMove(actor, foreignPosition, run, sneak);
                return true;
            }
            const ESM4::PathgridPoint point = graph->worldPoint(node.mNode);
            destination = osg::Vec3f(point.mX, point.mY, point.mZ);
            live.mState.mPathgrid = node.mPathgrid;
            live.mState.mPathNode = node.mNode;
            if (live.mForeignRouteTarget && *live.mForeignRouteTarget == node)
                live.mForeignRouteTarget.reset();
        }

        if (live.mDestinationCell != live.mState.mCell && live.mRouteDoor)
        {
            if (distanceSquared(position, live.mRouteDoorPosition) > sArrivalTolerance * sArrivalTolerance)
            {
                faceAndMove(actor, live.mRouteDoorPosition, run, sneak);
                return true;
            }
            live.mState.mDoor = *live.mRouteDoor;
            live.mRouteDoor.reset();
            reached = true;
            return true;
        }

        osg::Vec3f doorTarget = destination;
        if (live.mRouteCursor + 1 < live.mRoute.size()
            && distanceSquared(position, destination) <= sDoorLookAheadDistance * sDoorLookAheadDistance)
        {
            const ESM4::PathgridNodeKey next = live.mRoute[live.mRouteCursor + 1];
            const ESM4::PathgridGraph* nextGraph = service.graph(next.mPathgrid);
            if (nextGraph != nullptr && nextGraph->cellKey() == live.mState.mCell)
            {
                const ESM4::PathgridPoint point = nextGraph->worldPoint(next.mNode);
                doorTarget = osg::Vec3f(point.mX, point.mY, point.mZ);
            }
        }
        if (useBlockingDoor(doorTarget))
            return true;

        if (distanceSquared(position, destination) <= sArrivalTolerance * sArrivalTolerance)
        {
            if (live.mRouteCursor < live.mRoute.size())
                ++live.mRouteCursor;
            if (live.mRouteCursor >= live.mRoute.size())
            {
                reached = oblivionDestinationReached(live.mState.mCell, position,
                    live.mDestinationCell.isNull() ? live.mState.mCell : live.mDestinationCell,
                    *live.mDestination, sArrivalTolerance, live.mState.mPackageType, radius);
                if (!reached && (live.mDestinationCell.isNull() || live.mDestinationCell == live.mState.mCell))
                    faceAndMove(actor, *live.mDestination, run, sneak);
            }
            else
            {
                const ESM4::PathgridNodeKey next = live.mRoute[live.mRouteCursor];
                const ESM4::PathgridGraph* nextGraph = service.graph(next.mPathgrid);
                if (nextGraph != nullptr && nextGraph->cellKey() != live.mState.mCell)
                {
                    const ESM4::PathgridNodeKey source = live.mRoute[live.mRouteCursor - 1];
                    const std::optional<ESM::FormKey> door = doorForEdge(source, next);
                    if (door)
                    {
                        live.mState.mDoor = *door;
                        reached = true;
                    }
                }
            }
            return true;
        }

        faceAndMove(actor, destination, run, sneak);
        return true;
    }

    void OblivionAiService::stopMovement(const MWWorld::Ptr& actor) const
    {
        if (actor.isEmpty())
            return;
        Movement& movement = actor.getClass().getMovementSettings(actor);
        movement.mPosition[0] = 0.f;
        movement.mPosition[1] = 0.f;
        movement.mPosition[2] = 0.f;
    }

    void OblivionAiService::releasePhysicalDoorCollision(
        const MWWorld::Ptr& actor, LiveActor& live) const
    {
        if (live.mIgnoredPhysicalDoor.isNull())
            return;
        const MWWorld::Ptr door = loadedPtrFor(live.mIgnoredPhysicalDoor);
        if (!actor.isEmpty() && !door.isEmpty())
            mWorld.mPhysics->setIgnoreCollision(actor, door, false);
        live.mIgnoredPhysicalDoor = {};
    }

    void OblivionAiService::faceAndMove(const MWWorld::Ptr& actor, const osg::Vec3f& destination, bool run, bool sneak) const
    {
        const osg::Vec3f position = actor.getRefData().getPosition().asVec3();
        const float desired = std::atan2(destination.x() - position.x(), destination.y() - position.y());
        // Steering must update orientation as well as local translation. In
        // particular, a point behind the actor has no forward component and
        // cannot be reached by strafing indefinitely at the authored yaw.
        static_cast<void>(zTurn(actor, desired));
        const float yaw = actor.getRefData().getPosition().rot[2];
        const float relative = wrapAngle(desired - yaw);
        Movement& movement = actor.getClass().getMovementSettings(actor);
        movement.mPosition[0] = std::sin(relative);
        movement.mPosition[1] = std::max(std::cos(relative), 0.f);
        movement.mPosition[2] = 0.f;
        CreatureStats& stats = actor.getClass().getCreatureStats(actor);
        stats.setMovementFlag(CreatureStats::Flag_Run, run);
        stats.setMovementFlag(CreatureStats::Flag_Sneak, sneak);
    }

    bool OblivionAiService::transitionPackage(const MWWorld::Ptr& actor, LiveActor& live,
        const ESM4::PackagePhaseInput& input)
    {
        const ESM4::PackagePhase oldPhase = live.mState.mPhase;
        ESM4::PackagePhaseState phase = toPhaseState(live.mState);
        const ESM4::PackagePhaseTransition transition = ESM4::advancePackagePhase(phase, input);
        fromPhaseState(live.mState, phase);
        mPendingPackageDone.record(transition.mFrom, transition.mTo, live.mState.mActor, live.mState.mPackage);
        if (oldPhase == ESM4::PackagePhase::Door && live.mState.mPhase != oldPhase)
            live.mState.mDoorAnimationStarted = false;
        if (transition.mFrom != transition.mTo)
            logTransition(live, oldPhase, transition.mReason);
        if (live.mState.mPhase == ESM4::PackagePhase::Interrupted)
        {
            stopMovement(actor);
            live.mRoute.clear();
            live.mContinuousRoute.clear();
            live.mRouteCursor = 0;
            live.mContinuousRouteCursor = 0;
            live.mForeignRouteTarget.reset();
            live.mRouteDoor.reset();
            live.mDestination.reset();
            // Reservations are intent, not side effects.  Releasing them on
            // every interruption prevents an obsolete Eat/UseItemAt item
            // from being committed after a package reevaluation.
            live.mState.mActionReserved = false;
            live.mState.mActionItem = {};
            live.mState.mActionTimer = 0.f;
        }
        return transition.mFrom != transition.mTo;
    }

    void OblivionAiService::logTransition(const LiveActor& live, ESM4::PackagePhase oldPhase,
        std::string_view reason) const
    {
        Log(Debug::Verbose) << "ai actor=" << live.mState.mActor.serialize() << " package="
                            << live.mState.mPackage.serialize() << " phase="
                            << static_cast<unsigned>(oldPhase) << "->" << static_cast<unsigned>(live.mState.mPhase)
                            << " reason=" << reason;
        if (mEventStream)
        {
            mEventStream << "{\"event\":\"phase\",\"actor\":"
                         << jsonQuote(live.mState.mActor.serialize()) << ",\"package\":"
                         << jsonQuote(live.mState.mPackage.serialize()) << ",\"from\":"
                         << static_cast<unsigned>(oldPhase) << ",\"to\":"
                         << static_cast<unsigned>(live.mState.mPhase) << ",\"tier\":"
                         << static_cast<unsigned>(live.mState.mTier) << ",\"reason\":" << jsonQuote(reason)
                         << ",\"game_hour\":" << std::setprecision(9) << now().mHour << "}\n";
            mEventStream.flush();
        }
    }

    void OblivionAiService::logEvent(std::string_view event, const LiveActor& live, std::string_view reason) const
    {
        if (!mEventStream)
            return;

        const bool diagnostic = event == "route-blocked" || event == "door-failure"
            || event == "action-commit-failed" || event == "low-process-reconcile-failed"
            || event == "fast-forward-bounded";
        std::uint64_t diagnosticCount = 0;
        if (diagnostic)
        {
            const std::string key = std::string(event) + "|actor=" + live.mState.mActor.serialize()
                + "|reason=" + std::string(reason);
            const std::uint64_t count = diagnosticCount = ++mDiagnosticCounters[key];
            // Preserve the first observations and exponentially spaced
            // repeats in the event stream. Resets and destruction emit the exact
            // aggregate count, so a persistent obstruction is observable
            // without flooding a long-running save.
            if (count > 1 && (count & (count - 1)) != 0 && count % 32 != 0)
                return;
        }
        mEventStream << "{\"event\":" << jsonQuote(event) << ",\"actor\":"
                     << jsonQuote(live.mState.mActor.serialize()) << ",\"package\":"
                     << jsonQuote(live.mState.mPackage.serialize()) << ",\"phase\":"
                     << static_cast<unsigned>(live.mState.mPhase) << ",\"tier\":"
                     << static_cast<unsigned>(live.mState.mTier) << ",\"reason\":" << jsonQuote(reason)
                     << ",\"game_hour\":" << std::setprecision(9) << now().mHour;
        if (diagnostic)
            mEventStream << ",\"diagnostic_count\":" << diagnosticCount;
        mEventStream << "}\n";
        mEventStream.flush();
    }

    void OblivionAiService::reportPhysicalDoorTransition(const LiveActor& initiator,
        const ESM::FormKey& door, const ESM::FormKey& cell, std::string_view reason) const
    {
        logEvent("door-transition", initiator, reason);
        for (const auto& [_, observer] : mActors)
        {
            if (!observesOblivionPhysicalDoorTransition(
                    observer.mState, initiator.mState.mActor, cell))
                continue;
            logEvent("door-transition", observer,
                "door=" + door.serialize() + " observed=true initiator="
                    + initiator.mState.mActor.serialize());
        }
    }

    bool OblivionAiService::updateDialogueApproach(const MWWorld::Ptr& actor, LiveActor& live)
    {
        const ESM::FormKey playerKey = ESM::FormKey::dynamic("player", 1);
        const MWWorld::Ptr player = mWorld.getPlayerPtr();
        const bool ready = live.mState.mPhase == ESM4::PackagePhase::ReadyForDialogue;
        const bool eligiblePackage = live.mState.mPackageType == ESM4::AIPackageType::Unknown
            || live.mState.mPackageType == ESM4::AIPackageType::Wander;
        if (!actor.getClass().isNpc() || player.isEmpty() || !eligiblePackage
            || live.mState.mActionReserved || live.mState.mBoundary == ESM4::PhaseBoundary::ReadyForM15Combat
            || live.mState.mBoundary == ESM4::PhaseBoundary::WaitingForM16Action)
        {
            if (ready)
            {
                setPhase(live, ESM4::PackagePhase::Wait);
                live.mState.mBoundary = ESM4::PhaseBoundary::None;
                if (live.mState.mTarget == playerKey)
                    live.mState.mTarget = {};
                logEvent("dialogue-resume", live, "ineligible");
            }
            return false;
        }

        const CreatureStats& actorStats = actor.getClass().getCreatureStats(actor);
        const CreatureStats& playerStats = player.getClass().getCreatureStats(player);
        const auto& settings = mWorld.mStore.get<ESM::GameSetting>();
        const int multiplier = settings.find("iGreetDistanceMultiplier")->mValue.getInteger();
        const float resetDistance = settings.find("fGreetDistanceReset")->mValue.getFloat();
        const float helloDistance
            = std::max(0.f, static_cast<float>(actorStats.getAiSetting(AiSetting::Hello).getModified() * multiplier));
        const osg::Vec3f delta = player.getRefData().getPosition().asVec3()
            - actor.getRefData().getPosition().asVec3();
        const float distance = delta.length();
        const bool inRange = distance <= helloDistance;
        const bool visible = inRange && !playerStats.isDead() && !actorStats.isDead() && !actorStats.isParalyzed()
            && !mWorld.isSwimming(actor) && getLOS(player, actor);
        if (ready && (!visible || distance >= resetDistance))
        {
            setPhase(live, ESM4::PackagePhase::Wait);
            live.mState.mBoundary = ESM4::PhaseBoundary::None;
            if (live.mState.mTarget == playerKey)
                live.mState.mTarget = {};
            logEvent("dialogue-resume", live, visible ? "distance-reset" : "visibility-lost");
            return false;
        }
        if (!visible)
            return ready;

        if (!ready)
        {
            live.mState.mTarget = playerKey;
            live.mState.mTargetBase = {};
            setPhase(live, ESM4::PackagePhase::ReadyForDialogue);
            live.mState.mBoundary = ESM4::PhaseBoundary::ReadyForDialogue;
            logEvent("dialogue-ready", live, "target=" + playerKey.serialize());
        }
        stopMovement(actor);
        const float desired = std::atan2(delta.x(), delta.y());
        static_cast<void>(zTurn(actor, desired, osg::DegreesToRadians(5.f)));
        return true;
    }

    bool OblivionAiService::executeFixedStep(const MWWorld::Ptr& sourceActor, LiveActor& live, float duration,
        bool highProcess)
    {
        // A previous fixed step may have realized a low-process cell crossing.
        // Resolve the registered Ptr before reading its CellStore again.
        const MWWorld::Ptr registeredActor = loadedPtrFor(live.mState.mActor);
        const MWWorld::Ptr& actor = registeredActor.isEmpty() ? sourceActor : registeredActor;
        if (!live.mIgnoredPhysicalDoor.isNull())
        {
            const MWWorld::Ptr ignoredDoor = loadedPtrFor(live.mIgnoredPhysicalDoor);
            const bool traversingDoor
                = highProcess && (live.mState.mPhase == ESM4::PackagePhase::Path
                    || live.mState.mPhase == ESM4::PackagePhase::Door);
            if (!traversingDoor || ignoredDoor.isEmpty()
                || distanceSquared(actor.getRefData().getPosition().asVec3(),
                       ignoredDoor.getRefData().getPosition().asVec3())
                    > sDoorLookAheadDistance * sDoorLookAheadDistance)
                releasePhysicalDoorCollision(actor, live);
        }
        const osg::Vec3f observedPosition = actor.getRefData().getPosition().asVec3();
        const bool madeProgress = !live.mHasObservedPosition
            || (observedPosition - live.mLastObservedPosition).length2() > 0.25f;
        synchronizeIdentity(live, actor);
        live.mLastObservedPosition = observedPosition;
        live.mLastObservedCell = cellKey(actor);
        live.mHasObservedPosition = true;
        const ESM4::ProcessTier previousTier = live.mState.mTier;
        live.mState.mTier = highProcess ? ESM4::ProcessTier::High : ESM4::ProcessTier::Low;
        if (previousTier != live.mState.mTier)
        {
            std::ostringstream tier;
            tier << "from=" << static_cast<unsigned>(previousTier) << " to="
                 << static_cast<unsigned>(live.mState.mTier);
            logEvent("tier", live, tier.str());
        }
        live.mState.mDoorCooldown = std::max(0.f, live.mState.mDoorCooldown - std::max(0.f, duration));
        live.mSelectionCheckTimer = std::max(0.f, live.mSelectionCheckTimer - std::max(0.f, duration));
        const bool wasMounted = updateMountedAttachment(actor, live, highProcess);
        const bool remainsMounted = wasMounted && !live.mState.mMount.isNull();
        if (highProcess && live.mAbstractPositionDirty && !remainsMounted)
        {
            static_cast<void>(reconcileAbstractPosition(actor, live));
            // A cell-crossing realization can replace the Ptr registered by
            // the world. Resume on the next update with that registered Ptr.
            stopMovement(actor);
            return false;
        }
        if (highProcess && previousTier == ESM4::ProcessTier::Low
            && live.mState.mPhase == ESM4::PackagePhase::Path)
        {
            live.mRoute.clear();
            live.mContinuousRoute.clear();
            live.mRouteCursor = 0;
            live.mContinuousRouteCursor = 0;
            live.mContinuousRouteEndCursor.reset();
            live.mForeignRouteTarget.reset();
            live.mRouteDoor.reset();
        }
        if (live.mNeedsSelection)
            select(actor, live);
        if (live.mState.mRestrained)
        {
            stopMovement(actor);
            ESM4::PackagePhaseState phase = toPhaseState(live.mState);
            phase.mActionTimer += std::max(0.f, duration);
            fromPhaseState(live.mState, phase);
            return false;
        }

        if (updateDialogueApproach(actor, live))
            return true;

        // A rider follows the horse's realized transform rather than running a
        // second path controller. Treat a local path as reached once the
        // attachment is valid, but force a clean dismount at a real cell/door
        // boundary so a mounted actor can never cross an unloaded edge by
        // changing only its saved position.
        if (remainsMounted)
        {
            bool foreignBoundary = !live.mDestinationCell.isNull() && live.mDestinationCell != live.mState.mCell;
            if (!foreignBoundary && live.mRouteCursor < live.mRoute.size())
            {
                const ESM4::PathgridGraph* nextGraph
                    = mWorld.mStore.getOblivionPathgridService().graph(live.mRoute[live.mRouteCursor].mPathgrid);
                foreignBoundary = nextGraph != nullptr && nextGraph->cellKey() != live.mState.mCell;
            }
            if (foreignBoundary || live.mState.mPhase == ESM4::PackagePhase::Door)
            {
                dismountHorse(actor);
            }
            else if (live.mState.mPhase == ESM4::PackagePhase::Path)
            {
                transitionPackage(actor, live,
                    { duration, true, true, true, false, true, false, false, false, false, false, true });
                return true;
            }
        }

        const bool stalledRouteChanged = (live.mState.mPhase == ESM4::PackagePhase::Stalled
                                             || live.mState.mPhase == ESM4::PackagePhase::Interrupted)
            && !live.mState.mPathgrid.isNull()
            && live.mState.mRouteGeneration
                != mWorld.mStore.getOblivionPathgridService().generation(live.mState.mPathgrid);
        const bool doorRestored = live.mSelectionCheckTimer <= 0.f && doorInterruptionResolved(live, &actor);
        if (doorRestored)
            logEvent("door-access-restored", live, "door=" + live.mState.mDoor.serialize());
        if (live.mSelectionCheckTimer <= 0.f
            && (live.mState.mPhase == ESM4::PackagePhase::Interrupted || live.mState.mPhase == ESM4::PackagePhase::Complete
                || (live.mState.mPhase == ESM4::PackagePhase::Wait && live.mState.mPackage.isNull())
                || stalledRouteChanged))
        {
            // A terminal package remains terminal while it is still the
            // selected winner. Retry after an explicit graph change or when
            // access to the exact interrupted door has actually returned;
            // schedule and condition reevaluation can still choose a new
            // package without restarting the old one.
            const bool idleWait = live.mState.mPhase == ESM4::PackagePhase::Wait && live.mState.mPackage.isNull();
            const bool restart = !idleWait && (stalledRouteChanged || doorRestored);
            select(actor, live, restart);
        }

        const ESM4::AIPackage* current = package(live.mState.mPackage);
        if (current == nullptr && live.mScriptPackage)
            current = package(live.mScriptPackage->mKey);
        if (current == nullptr && live.mTransientPackage
            && live.mTransientPackage->mFormKey == live.mState.mPackage)
            current = &*live.mTransientPackage;
        if (current == nullptr)
        {
            stopMovement(actor);
            if (live.mState.mPhase == ESM4::PackagePhase::Wait)
                return false;
            return transitionPackage(actor, live, { duration, true, true, true, false, true, false, false, false,
                false, false, true });
        }

        if (live.mState.mPhase == ESM4::PackagePhase::Select)
        {
            transitionPackage(actor, live, { duration, true, true, false, false, true, false, false, false, false,
                false, true });
            return true;
        }

        if (live.mState.mPhase == ESM4::PackagePhase::Resolve)
        {
            ESM::FormKey targetKey;
            const std::optional<osg::Vec3f> destination = highProcess
                ? resolveDestination(actor, *current, live, targetKey)
                : resolveUnloadedDestination(*current, live);
            if (!highProcess)
                targetKey = live.mState.mTarget;
            live.mState.mTarget = targetKey;
            live.mDestination = destination;
            if (destination)
            {
                if (live.mDestinationCell.isNull())
                    live.mDestinationCell = live.mState.mCell;
                live.mState.mDestinationCell = live.mDestinationCell;
                live.mState.mDestinationPosition = actor.getRefData().getPosition();
                live.mState.mDestinationPosition.pos[0] = destination->x();
                live.mState.mDestinationPosition.pos[1] = destination->y();
                live.mState.mDestinationPosition.pos[2] = destination->z();
                live.mState.mHasDestination = true;
                const bool route = highProcess ? prepareRoute(actor, live, *destination, live.mDestinationCell)
                                               : prepareUnloadedRoute(live);
                transitionPackage(actor, live,
                    { duration, true, route, false, false, true, false, false, false, false, false, true });
            }
            else
                transitionPackage(actor, live,
                    { duration, false, false, false, false, false, false, false, false, false, false, true });
            return true;
        }

        if (live.mState.mPhase == ESM4::PackagePhase::Path)
        {
            refreshMovingTargetRoute(live, *current, highProcess ? actor : MWWorld::Ptr{});
            if (live.mRoute.empty() && live.mDestination)
            {
                // A save stores the stable destination intent, not ephemeral
                // route vectors. Rebuild the route after the cell stores and
                // pathgrid service have been restored.
                if (highProcess)
                    static_cast<void>(prepareRoute(actor, live, *live.mDestination, live.mDestinationCell));
                else
                    static_cast<void>(prepareUnloadedRoute(live));
            }
            const osg::Vec3f previousLogicalPosition = live.mState.mLastValidPosition.asVec3();
            bool reached = false;
            const bool route = advanceMovement(actor, live, duration, highProcess, reached);
            const bool movementProgress = highProcess ? madeProgress
                : distanceSquared(previousLogicalPosition, live.mState.mLastValidPosition.asVec3()) > 0.01f;
            const bool foreignDestination = !live.mDestinationCell.isNull()
                && live.mDestinationCell != live.mState.mCell;
            const bool door = reached && !live.mState.mDoor.isNull();
            const bool validReached = reached && (!foreignDestination || door);
            const bool validRoute = route && (!foreignDestination || !reached || door);
            if (foreignDestination && !validRoute)
            {
                const std::string reason = door ? "foreign-edge"
                                                 : (live.mLastRouteFailure.empty()
                                                         ? "foreign-edge-without-door"
                                                         : live.mLastRouteFailure);
                logEvent("route-blocked", live, reason);
            }
            if (!validRoute)
                stopMovement(actor);
            transitionPackage(actor, live,
                { duration, true, validRoute, validReached, door, !door, false, false, false, false, false,
                    movementProgress || validReached, validRoute });
            return validRoute;
        }

        stopMovement(actor);
        if (live.mState.mPhase == ESM4::PackagePhase::Door)
        {
            if (live.mState.mDoorCooldown > 0.f)
                return false;
            bool locked = false;
            const bool available = canUseDoor(actor, live, locked);
            if (!available)
            {
                transitionPackage(actor, live,
                    { duration, true, true, true, true, false, locked, false, false, false, false, true });
                return false;
            }
            live.mPendingDoor = true;
            return true;
        }
        if (live.mState.mPhase == ESM4::PackagePhase::Arrive)
        {
            transitionPackage(actor, live,
                { duration, true, true, true, false, true, false, false, false,
                    live.mState.mPackageType == ESM4::AIPackageType::CastMagic, false, true });
            return true;
        }
        if (live.mState.mPhase == ESM4::PackagePhase::Act)
        {
            const bool timed = live.mState.mPackageType == ESM4::AIPackageType::Eat
                || live.mState.mPackageType == ESM4::AIPackageType::Sleep
                || live.mState.mPackageType == ESM4::AIPackageType::UseItemAt;
            if (timed)
            {
                if (!reserveAction(actor, live, *current))
                {
                    live.mState.mInterruptionReason = "action-target-unresolved";
                    transitionPackage(actor, live,
                        { duration, true, true, true, false, true, false, false, false, false, true, false });
                    return false;
                }
                if (live.mState.mActionTimer == 0.f)
                {
                    live.mState.mActionTimer = live.mState.mPackageType == ESM4::AIPackageType::Sleep
                        ? std::max(1.f, live.mState.mDurationRemaining * 3600.f)
                        : 1.f;
                }
            }
            transitionPackage(actor, live,
                { duration, true, true, true, false, true, false, timed, !timed,
                    live.mState.mPackageType == ESM4::AIPackageType::CastMagic, false, true });
            return true;
        }
        if (live.mState.mPhase == ESM4::PackagePhase::Wait)
        {
            if (followDestinationReached(actor, *current, live, highProcess))
            {
                transitionPackage(actor, live,
                    { duration, true, true, true, false, true, false, false, true, false, false, true });
                return true;
            }
            const bool movingTarget = live.mState.mPackageType == ESM4::AIPackageType::Follow
                || live.mState.mPackageType == ESM4::AIPackageType::Accompany
                || live.mState.mPackageType == ESM4::AIPackageType::Escort
                || live.mState.mPackageType == ESM4::AIPackageType::Pursue;
            if (movingTarget && live.mState.mDurationRemaining > std::max(0.f, duration) / 3600.f)
            {
                ESM::FormKey refreshedTarget;
                const std::optional<osg::Vec3f> refreshedDestination = highProcess
                    ? resolveDestination(actor, *current, live, refreshedTarget)
                    : resolveUnloadedDestination(*current, live);
                if (!highProcess)
                    refreshedTarget = live.mState.mTarget;
                if (!refreshedDestination)
                {
                    live.mState.mInterruptionReason = "moving-target-unresolved";
                    transitionPackage(actor, live,
                        { duration, true, true, true, false, true, false, false, false, false, true, false });
                    return false;
                }

                live.mState.mTarget = refreshedTarget;
                if (updateOblivionMovingDestination(live.mState, live.mDestinationCell, *refreshedDestination))
                {
                    live.mDestination = refreshedDestination;
                    const ESM4::PackagePhase oldPhase = live.mState.mPhase;
                    live.mRoute.clear();
                    live.mContinuousRoute.clear();
                    live.mRouteCursor = 0;
                    live.mContinuousRouteCursor = 0;
                    live.mForeignRouteTarget.reset();
                    live.mRouteDoor.reset();
                    live.mState.mPathgrid = {};
                    live.mState.mPathNode = 0;
                    live.mState.mDoor = {};
                    live.mState.mRepathAttempts = 0;
                    setPhase(live, ESM4::PackagePhase::Path);
                    if (highProcess)
                        static_cast<void>(prepareRoute(actor, live, *refreshedDestination, live.mDestinationCell));
                    else
                        static_cast<void>(prepareUnloadedRoute(live));
                    logTransition(live, oldPhase, "moving-target-repath");
                    live.mState.mDurationRemaining
                        = std::max(0.f, live.mState.mDurationRemaining - std::max(0.f, duration) / 3600.f);
                    return true;
                }
            }
            bool actionCompleted = false;
            if (live.mState.mActionReserved && live.mState.mActionTimer <= std::max(0.f, duration))
            {
                actionCompleted = completeAction(actor, live);
                if (!actionCompleted)
                {
                    live.mState.mInterruptionReason = "action-failed";
                    transitionPackage(actor, live,
                        { duration, true, true, true, false, true, false, false, false, false, true, true });
                    return false;
                }
            }
            transitionPackage(actor, live, { duration, true, true, true, false, true, false, false, actionCompleted,
                false, false, true });
            return true;
        }
        return false;
    }

    void OblivionAiService::update(const MWWorld::Ptr& actor, float duration, bool highProcess)
    {
        if (!handles(actor) || actor == mWorld.getPlayerPtr())
            return;
        if (!std::isfinite(duration))
        {
            Log(Debug::Warning) << "TES4 AI ignored a non-finite update interval for actor "
                                << actorKey(actor).serialize();
            return;
        }
        LiveActor& live = ensure(actor);
        const float elapsed = std::clamp(duration, 0.f, 5.f);

        if (!highProcess)
        {
            // Low process is an abstract, fixed 250 ms clock.  It advances
            // route/action timers without driving a render-frame controller;
            // promotion to high process resumes the same stable intent.
            if (live.mNeedsSelection)
                static_cast<void>(executeFixedStep(actor, live, 0.f, false));
            if (!std::isfinite(live.mState.mNextLowProcessTick)
                || live.mState.mNextLowProcessTick < 0.f)
            {
                Log(Debug::Warning) << "TES4 AI recovered an invalid low-process tick for actor "
                                    << live.mState.mActor.serialize();
                live.mState.mNextLowProcessTick = 0.25f;
            }
            else if (live.mState.mNextLowProcessTick == 0.f)
                live.mState.mNextLowProcessTick = 0.25f;
            live.mState.mNextLowProcessTick -= elapsed;
            unsigned steps = 0;
            while (live.mState.mNextLowProcessTick <= 0.f && steps < 8)
            {
                static_cast<void>(executeFixedStep(actor, live, 0.25f, false));
                live.mState.mNextLowProcessTick += 0.25f;
                ++steps;
            }
            if (steps == 8 && live.mState.mNextLowProcessTick < 0.f)
                live.mState.mNextLowProcessTick = 0.f;
            if (!std::isfinite(live.mState.mNextLowProcessTick)
                || live.mState.mNextLowProcessTick < 0.f)
                live.mState.mNextLowProcessTick = 0.f;
            return;
        }

        live.mState.mNextLowProcessTick = 0.25f;
        live.mFixedRemainder += elapsed;
        if (elapsed == 0.f && live.mState.mPhase == ESM4::PackagePhase::Select)
            static_cast<void>(executeFixedStep(actor, live, 0.f, highProcess));
        float simulated = 0.f;
        unsigned steps = 0;
        while (live.mFixedRemainder >= sFixedStep && steps < 8)
        {
            static_cast<void>(executeFixedStep(actor, live, sFixedStep, highProcess));
            live.mFixedRemainder -= sFixedStep;
            simulated += sFixedStep;
            ++steps;
        }
        if (live.mFixedRemainder > sMaximumFrameCatchup)
        {
            Log(Debug::Warning) << "TES4 AI dropped bounded catch-up time for actor "
                                << live.mState.mActor.serialize();
            live.mFixedRemainder = std::fmod(live.mFixedRemainder, sFixedStep);
        }
        static_cast<void>(simulated);
    }

    void OblivionAiService::commitPendingTransitions()
    {
        // mActors is ordered by stable key, which makes simultaneous door
        // requests deterministic and keeps this pass independent of the
        // order in which the scene enumerated actors.
        for (auto& [_, live] : mActors)
        {
            if (live.mPendingRealization)
            {
                live.mPendingRealization = false;
                try
                {
                    const MWWorld::Ptr actor = loadedPtrFor(live.mState.mActor);
                    if (!actor.isEmpty())
                        static_cast<void>(reconcileAbstractPosition(actor, live, true));
                }
                catch (const std::exception& error)
                {
                    Log(Debug::Error) << "TES4 AI realization commit failed for actor "
                                      << live.mState.mActor.serialize() << ": " << error.what();
                }
            }
            if (!live.mPendingDoor)
                continue;
            live.mPendingDoor = false;

            try
            {

                const MWWorld::Ptr actor = ptrFor(live.mState.mActor);
                bool locked = false;
                if (actor.isEmpty() || !canUseDoor(actor, live, locked))
                {
                    if (!actor.isEmpty())
                        transitionPackage(actor, live,
                            { 0.f, true, true, true, true, false, locked, false, false, false, false, true });
                    continue;
                }

                const ESM::FormKey doorKey = live.mState.mDoor;
                const bool crossingCell = !live.mDestinationCell.isNull()
                    && live.mDestinationCell != live.mState.mCell;
                bool physicalDoor = false;
                try
                {
                    const MWWorld::Ptr door = ptrFor(doorKey);
                    // Authorization was checked above. Physical doors can open
                    // for their owner without silently clearing their lock;
                    // teleport doors still use the ordinary activation action.
                    if (door.getCellRef().getTeleport())
                    {
                        mWorld.activateOblivionReferenceDefault(door, actor);
                        logEvent("door-transition", live, "door=" + doorKey.serialize());
                    }
                    else
                    {
                        physicalDoor = true;
                        MWBase::MechanicsManager* mechanics
                            = MWBase::Environment::get().getMechanicsManager();
                        if (live.mState.mDoorAnimationStarted)
                        {
                            if (mechanics->checkAnimationPlaying(door, "open"))
                            {
                                live.mState.mDoorCooldown = 0.1f;
                                continue;
                            }
                        }
                        else
                        {
                            live.mState.mDoorAnimationStarted
                                = mechanics->playAnimationGroup(door, "open", 1, 1, true);
                            if (live.mState.mDoorAnimationStarted)
                            {
                                if (MWWorld::OblivionScriptManager* scripts
                                    = mWorld.getOblivionScriptManager())
                                    scripts->persistAnimationState(doorKey, "open", 1, true, true);
                                const osg::Vec3f doorPosition = door.getRefData().getPosition().asVec3();
                                reportPhysicalDoorTransition(live, doorKey, cellKey(door),
                                    "door=" + doorKey.serialize() + " animated=true position="
                                        + std::to_string(doorPosition.x()) + ','
                                        + std::to_string(doorPosition.y()) + ','
                                        + std::to_string(doorPosition.z()));
                                live.mState.mDoorCooldown = 0.1f;
                                continue;
                            }

                            const MWWorld::DoorState state = door.getClass().getDoorState(door);
                            const float current = door.getRefData().getPosition().rot[2];
                            const float closed = door.getCellRef().getPosition().rot[2];
                            if (state != MWWorld::DoorState::Idle || current == closed)
                            {
                                if (state != MWWorld::DoorState::Opening)
                                {
                                    mWorld.activateDoor(door, MWWorld::DoorState::Opening);
                                    reportPhysicalDoorTransition(live, doorKey, cellKey(door),
                                        "door=" + doorKey.serialize() + " animated=false");
                                }
                                live.mState.mDoorCooldown = 0.1f;
                                continue;
                            }
                        }
                        if (!live.mPhysicalDoorOpened)
                        {
                            if (!live.mIgnoredPhysicalDoor.isNull()
                                && live.mIgnoredPhysicalDoor != doorKey)
                                releasePhysicalDoorCollision(actor, live);
                            mWorld.mPhysics->setIgnoreCollision(actor, door, true);
                            live.mIgnoredPhysicalDoor = doorKey;
                            live.mPhysicalDoorOpened = true;
                            live.mState.mDoorCooldown = 1.f;
                            continue;
                        }
                    }
                }
                catch (const std::exception& error)
                {
                    Log(Debug::Warning) << "TES4 AI door transition failed for actor "
                                        << live.mState.mActor.serialize() << ": " << error.what();
                    logEvent("door-failure", live, "door=" + doorKey.serialize());
                    transitionPackage(actor, live,
                        { 0.f, true, true, true, true, false, false, false, false, false, true, true });
                    continue;
                }

                const MWWorld::Ptr movedActor = ptrFor(live.mState.mActor);
                if (!movedActor.isEmpty())
                    synchronizeIdentity(live, movedActor);
                live.mState.mLastTransitionDoor = doorKey;
                live.mState.mDoorCooldown = 2.f;
                live.mState.mDoorAnimationStarted = false;
                live.mPhysicalDoorOpened = false;
                // A teleport door invalidates the source-cell route. Rebuild from
                // the realized destination cell on the next fixed step instead of
                // interpreting a consumed source route as arrival.
                const MWWorld::Ptr routeActor = movedActor.isEmpty() ? actor : movedActor;
                const bool destinationReached = live.mDestination
                    && oblivionDestinationReached(live.mState.mCell,
                        routeActor.getRefData().getPosition().asVec3(),
                        live.mDestinationCell.isNull() ? live.mState.mCell : live.mDestinationCell,
                        *live.mDestination, sArrivalTolerance, live.mState.mPackageType,
                        travelArrivalRadius(live));
                const bool continueRoute = crossingCell || live.mRouteCursor < live.mRoute.size()
                    || live.mContinuousRouteCursor < live.mContinuousRoute.size()
                    || (physicalDoor && !destinationReached);
                if (crossingCell)
                {
                    live.mRoute.clear();
                    live.mContinuousRoute.clear();
                    live.mRouteCursor = 0;
                    live.mContinuousRouteCursor = 0;
                    live.mContinuousRouteEndCursor.reset();
                    live.mForeignRouteTarget.reset();
                    live.mRouteDoor.reset();
                    live.mState.mPathgrid = {};
                    live.mState.mPathNode = 0;
                    live.mState.mDoor = {};
                }
                else if (physicalDoor)
                {
                    // The continuous route was built with door traversal
                    // enabled. Keep its corridor after the embedded Open
                    // sequence finishes; falling back to the coarse PGRD
                    // node can steer a full-sized actor into the door frame.
                    live.mState.mDoor = {};
                }
                transitionPackage(movedActor.isEmpty() ? actor : movedActor, live,
                    { 0.f, true, true, true, true, true, false, false, true, false, false, true, continueRoute });
            }
            catch (const std::exception& error)
            {
                Log(Debug::Error) << "TES4 AI transition commit failed for actor "
                                  << live.mState.mActor.serialize() << ": " << error.what();
            }
        }
        dispatchPendingPackageDone();
    }

    void OblivionAiService::dispatchPendingPackageDone()
    {
        MWWorld::OblivionScriptManager* scripts = mWorld.getOblivionScriptManager();
        if (scripts == nullptr || !mWorld.mScriptsEnabled)
            return;
        // Work only on the FIFO present on entry. Script callbacks can change
        // packages, move/delete actors or request saves. No actor-map iterator
        // or LiveActor reference may survive a callback. Remove its event
        // before invocation so a callback save retains only outstanding work.
        mPendingPackageDone.dispatch([&](const ESM4::RuntimePackageDoneEvent& event) {
            try
            {
                const bool handled = scripts->dispatchObjectEvent(event.mActor, "onpackagedone", event.mPackage);
                if (handled)
                    Log(Debug::Info) << "Native AI package done: actor=" << event.mActor.serialize()
                        << " package=" << event.mPackage.serialize();
            }
            catch (const std::exception& error)
            {
                Log(Debug::Error) << "TES4 AI package completion dispatch failed for actor "
                    << event.mActor.serialize() << ": " << error.what();
            }
        });
    }

    void OblivionAiService::fastForward(float gameHours)
    {
        if (!std::isfinite(gameHours) || gameHours < 0.f)
            return;
        const float seconds = gameHours * 3600.f;
        const ESM4::CalendarInstant end = now();
        for (auto& [_, live] : mActors)
        {
            const MWWorld::Ptr actor = loadedPtrFor(live.mState.mActor);
            if (!actor.isEmpty())
                synchronizeIdentity(live, actor);

            // Rest/fast-forward advances native package time from the instant
            // before the jump to the current world clock.  The simulation
            // clock is local to this actor and is never written back to the
            // world's authoritative time manager.
            mSimulationNow = ESM4::calendarAddHours(end, -static_cast<double>(gameHours));
            live.mState.mTier = ESM4::ProcessTier::Low;

            float remaining = seconds;
            bool bootstrap = true;
            std::size_t iterations = 0;
            const std::size_t iterationLimit = std::max<std::size_t>(256,
                static_cast<std::size_t>(std::ceil(seconds / 300.f)) + 512);
            while (bootstrap || remaining > 0.f)
            {
                if (++iterations > iterationLimit)
                {
                    logEvent("fast-forward-bounded", live, "iteration-limit");
                    break;
                }

                // An unloaded actor can commit a validated paired XTEL edge
                // without waiting for promotion. Resolve a Door phase before
                // consuming the next large fast-forward slice so rest/clock
                // jumps can continue through a multi-cell route. Resident
                // actors still stop at the boundary and use the ordinary
                // activation path when they are promoted/updated.
                if (actor.isEmpty() && live.mState.mPhase == ESM4::PackagePhase::Door)
                {
                    executeUnloadedFixedStep(live, 0.f);
                    if (live.mState.mPhase == ESM4::PackagePhase::Door)
                    {
                        remaining = 0.f;
                        break;
                    }
                }

                const ESM4::PackagePhase previousPhase = live.mState.mPhase;
                const std::size_t previousRouteCursor = live.mRouteCursor;
                const osg::Vec3f previousPosition = live.mState.mLastValidPosition.asVec3();
                const float previousActionTimer = live.mState.mActionTimer;
                const float previousDuration = live.mState.mDurationRemaining;
                const float step = remaining > 0.f
                    ? std::min(remaining,
                        live.mState.mPhase == ESM4::PackagePhase::Path ? 300.f : remaining)
                    : 0.f;

                executeUnloadedFixedStep(live, step);

                // A low-process Eat/UseItemAt reservation becomes commit-ready
                // when its timer reaches zero.  A resident actor can use the
                // normal M13 InventoryStore authority even though movement was
                // simulated through the state-only executor.
                if (!actor.isEmpty() && live.mState.mActionReserved && live.mState.mActionTimer <= 0.f
                    && !live.mState.mActionItem.isNull())
                {
                    if (completeAction(actor, live))
                        live.mState.mActionReserved = false;
                    else
                        logEvent("action-commit-failed", live, "fast-forward-resident-inventory");
                }

                if (step > 0.f)
                {
                    remaining -= step;
                    mSimulationNow = ESM4::calendarAddHours(*mSimulationNow,
                        static_cast<double>(step) / 3600.0);
                }
                bootstrap = false;

                if (live.mState.mPhase == ESM4::PackagePhase::Door)
                {
                    if (actor.isEmpty())
                    {
                        // The state-only executor can commit this validated
                        // paired XTEL edge. Continue the same jump from the
                        // destination marker instead of leaving a route at
                        // every intermediate door.
                        executeUnloadedFixedStep(live, 0.f);
                        if (live.mState.mPhase != ESM4::PackagePhase::Door)
                            continue;
                    }
                    // Resident actors retain the door intent for the normal
                    // activation path after promotion. Consuming the rest of
                    // the jump avoids polling an unavailable CellStore action.
                    remaining = 0.f;
                    break;
                }

                const bool changed = previousPhase != live.mState.mPhase
                    || previousRouteCursor != live.mRouteCursor
                    || distanceSquared(previousPosition, live.mState.mLastValidPosition.asVec3()) > 0.01f
                    || previousActionTimer != live.mState.mActionTimer
                    || previousDuration != live.mState.mDurationRemaining;
                if (step == 0.f && !changed)
                    break;
            }

            if (!actor.isEmpty() && live.mAbstractPositionDirty && live.mState.mPhase != ESM4::PackagePhase::Door
                && (live.mDestinationCell.isNull() || live.mDestinationCell == live.mState.mCell))
                static_cast<void>(reconcileAbstractPosition(actor, live));
            mSimulationNow.reset();
        }
        dispatchPendingPackageDone();
    }

    bool OblivionAiService::evaluatePackage(const MWWorld::Ptr& actor)
    {
        if (!handles(actor))
            return false;
        LiveActor& live = ensure(actor);
        static_cast<void>(select(actor, live, false));
        if (live.mState.mPhase == ESM4::PackagePhase::Select)
            static_cast<void>(executeFixedStep(actor, live, 0.f, true));
        return !live.mState.mPackage.isNull();
    }

    bool OblivionAiService::addScriptPackage(const MWWorld::Ptr& actor, const ESM::FormKey& packageKey)
    {
        if (!handles(actor))
            return false;
        const auto candidate = packageCandidate(packageKey);
        if (!candidate)
            throw std::runtime_error("AddScriptPackage target is not a valid native PACK: " + packageKey.serialize());
        LiveActor& live = ensure(actor);
        live.mScriptPackage = *candidate;
        live.mTransientPackage.reset();
        live.mScriptPackage->mListIndex = std::numeric_limits<std::size_t>::max();
        live.mState.mScriptPackage = packageKey;
        setPhase(live, ESM4::PackagePhase::Select);
        live.mNeedsSelection = true;
        static_cast<void>(executeFixedStep(actor, live, 0.f, true));
        return true;
    }

    bool OblivionAiService::forceFlee(const MWWorld::Ptr& actor, const ESM::FormKey& threat, float durationHours)
    {
        if (!handles(actor) || threat.isNull() || !std::isfinite(durationHours) || durationHours < 0.f
            || durationHours > static_cast<float>(std::numeric_limits<std::int32_t>::max()))
            return false;
        const float effectiveDuration = std::max(1.f, durationHours);
        LiveActor& live = ensure(actor);
        ESM4::PackageCandidate candidate;
        candidate.mKey = ESM::FormKey::dynamic("force-flee", live.mState.mSelectionGeneration + 1);
        candidate.mType = ESM4::AIPackageType::FleeNotCombat;
        candidate.mSchedule.mStartHour = -1;
        candidate.mSchedule.mDuration = static_cast<std::int32_t>(effectiveDuration);
        ESM4::AIPackage transient;
        transient.mFormKey = candidate.mKey;
        transient.mPackageType = candidate.mType;
        transient.mScheduleData = candidate.mSchedule;
        transient.mTargetData.mKind = ESM4::PackageTargetKind::SpecificReference;
        transient.mTargetData.mReferenceKey = threat;
        live.mTransientPackage = std::move(transient);
        live.mScriptPackage = candidate;
        live.mState.mScriptPackage = candidate.mKey;
        live.mState.mTarget = threat;
        live.mState.mDurationRemaining = effectiveDuration;
        setPhase(live, ESM4::PackagePhase::Select);
        live.mNeedsSelection = true;
        static_cast<void>(executeFixedStep(actor, live, 0.f, true));
        return true;
    }

    bool OblivionAiService::setRestrained(const MWWorld::Ptr& actor, bool restrained)
    {
        if (!handles(actor))
            return false;
        LiveActor& live = ensure(actor);
        live.mState.mRestrained = restrained;
        if (restrained)
            stopMovement(actor);
        return true;
    }

    bool OblivionAiService::isHorse(const MWWorld::ConstPtr& actor) const
    {
        if (actor.isEmpty() || actor.getClass().getType() != ESM::REC_CREA4)
            return false;
        const auto* reference = actor.get<ESM4::Creature>();
        if (reference == nullptr || reference->mBase == nullptr)
            return false;
        const ESM4::Creature* base = reference->mBase;
        return containsInsensitive(base->mEditorId, "horse") || containsInsensitive(base->mFullName, "horse")
            || containsInsensitive(base->mEditorId, "shadowmere");
    }

    bool OblivionAiService::mountHorse(const MWWorld::Ptr& rider, const MWWorld::Ptr& horse)
    {
        if (!handles(rider) || !handles(horse) || rider == horse || !isHorse(horse)
            || !rider.isInCell() || !horse.isInCell() || cellKey(rider) != cellKey(horse))
            return false;
        const osg::Vec3f separation = rider.getRefData().getPosition().asVec3()
            - horse.getRefData().getPosition().asVec3();
        if (separation.length2() > 256.f * 256.f)
            return false;

        LiveActor& riderState = ensure(rider);
        LiveActor& horseState = ensure(horse);
        if (riderState.mState.mMount == actorKey(horse))
            return true;
        if (!riderState.mState.mMount.isNull() || !riderState.mState.mRider.isNull()
            || !horseState.mState.mRider.isNull())
            return false;
        if (rider.getClass().getCreatureStats(rider).isDead() || horse.getClass().getCreatureStats(horse).isDead()
            || rider.getClass().getCreatureStats(rider).getKnockedDown()
            || horse.getClass().getCreatureStats(horse).getKnockedDown())
            return false;

        const ESM::FormKey riderKey = actorKey(rider);
        const ESM::FormKey horseKey = actorKey(horse);
        riderState.mState.mMount = horseKey;
        riderState.mState.mRider = {};
        riderState.mLastRiddenHorse = horseKey;
        horseState.mState.mRider = riderKey;
        horseState.mState.mMount = {};
        riderState.mRoute.clear();
        riderState.mContinuousRoute.clear();
        riderState.mRouteCursor = 0;
        riderState.mContinuousRouteCursor = 0;
        riderState.mForeignRouteTarget.reset();
        riderState.mRouteDoor.reset();
        riderState.mDestination.reset();
        riderState.mDestinationCell = {};
        riderState.mState.mPathgrid = {};
        riderState.mState.mPathNode = 0;
        riderState.mState.mDoor = {};
        riderState.mState.mLastTransitionDoor = {};
        riderState.mState.mRepathAttempts = 0;
        riderState.mState.mNoProgressSeconds = 0.f;
        riderState.mState.mBoundary = ESM4::PhaseBoundary::None;
        riderState.mState.mActionReserved = false;
        riderState.mState.mActionItem = {};
        riderState.mState.mActionTimer = 0.f;
        riderState.mPendingDoor = false;
        setPhase(riderState, riderState.mState.mPackage.isNull()
            ? ESM4::PackagePhase::Select
            : ESM4::PackagePhase::Resolve);
        riderState.mNeedsSelection = riderState.mState.mPackage.isNull();
        stopMovement(rider);
        logEvent("horse-mount", riderState, "horse=" + horseKey.serialize());
        logEvent("horse-rider", horseState, "rider=" + riderKey.serialize());
        return true;
    }

    bool OblivionAiService::dismountHorse(const MWWorld::Ptr& rider)
    {
        if (!handles(rider))
            return false;
        LiveActor& riderState = ensure(rider);
        const ESM::FormKey horseKey = riderState.mState.mMount;
        if (horseKey.isNull())
            return false;

        const MWWorld::Ptr horse = ptrFor(horseKey);
        if (!horse.isEmpty() && horse.isInCell() && rider.isInCell() && cellKey(horse) == cellKey(rider))
        {
            const float yaw = horse.getRefData().getPosition().rot[2];
            const osg::Vec3f horsePosition = horse.getRefData().getPosition().asVec3();
            const osg::Vec3f dismountPosition
                = horsePosition + osg::Vec3f(-std::sin(yaw) * 96.f, -std::cos(yaw) * 96.f, 0.f);
            try
            {
                mWorld.moveObject(rider, rider.getCell(), dismountPosition, true, false);
            }
            catch (const std::exception& error)
            {
                Log(Debug::Warning) << "TES4 AI dismount placement failed for actor " << riderState.mState.mActor
                                    << ": " << error.what();
            }
        }

        if (!horse.isEmpty() && handles(horse))
        {
            LiveActor& horseState = ensure(horse);
            if (horseState.mState.mRider == riderState.mState.mActor)
                horseState.mState.mRider = {};
        }
        else if (auto foundHorse = mActors.find(horseKey); foundHorse != mActors.end()
            && foundHorse->second.mState.mRider == riderState.mState.mActor)
            foundHorse->second.mState.mRider = {};
        riderState.mState.mMount = {};
        riderState.mLastRiddenHorse = horseKey;
        stopMovement(rider);
        logEvent("horse-dismount", riderState, "horse=" + horseKey.serialize());
        return true;
    }

    void OblivionAiService::invalidateOverlayRoutes(const std::set<ESM::FormKey>& changedPathgrids)
    {
        for (auto& [_, live] : mActors)
        {
            if (!oblivionRouteAffectedByOverlay(
                    live.mState.mPhase, live.mState.mPathgrid, live.mRoute, changedPathgrids))
                continue;
            live.mRoute.clear();
            live.mContinuousRoute.clear();
            live.mRouteCursor = 0;
            live.mContinuousRouteCursor = 0;
            live.mContinuousRouteEndCursor.reset();
            live.mForeignRouteTarget.reset();
            live.mRouteDoor.reset();
            live.mState.mDoor = {};
            // Rebuild on the next Path tick without changing package/action
            // state or granting a fresh no-progress/retry budget.
            logEvent("route-invalidated", live, "pathgrid-overlay");
        }
    }

    bool OblivionAiService::setPathPoint(const ESM::FormKey& pathgrid, std::uint32_t node, bool enabled)
    {
        const bool changed = mWorld.mStore.getOblivionPathgridService().setNodeEnabled({ pathgrid, node }, enabled);
        if (changed)
        {
            invalidateOverlayRoutes({ pathgrid });
            if (mWorld.mWorldScene)
                mWorld.mWorldScene->refreshOblivionPathgrid(pathgrid);
        }
        return changed;
    }

    bool OblivionAiService::hasLinkedPathPoints(const ESM::FormKey& object) const
    {
        if (object.isNull())
            return false;
        const ESM4::PathgridService& service = mWorld.mStore.getOblivionPathgridService();
        for (const auto& [_, graph] : service.graphs())
            for (const ESM4::PathgridObjectLink& link : graph.objectLinks())
                if (link.mObject == object && !link.mNodes.empty())
                    return true;
        return false;
    }

    bool OblivionAiService::setLinkedPathPoints(const ESM::FormKey& object, bool enabled)
    {
        if (object.isNull())
            return false;
        ESM4::PathgridService& service = mWorld.mStore.getOblivionPathgridService();
        bool changed = false;
        std::set<ESM::FormKey> changedPathgrids;
        for (const auto& [pathgrid, graph] : service.graphs())
        {
            for (const ESM4::PathgridObjectLink& link : graph.objectLinks())
            {
                if (link.mObject != object)
                    continue;
                for (const std::uint32_t node : link.mNodes)
                {
                    const bool nodeChanged = service.setNodeEnabled({ pathgrid, node }, enabled);
                    changed = nodeChanged || changed;
                    if (nodeChanged)
                        changedPathgrids.insert(pathgrid);
                }
            }
        }
        if (changed)
            invalidateOverlayRoutes(changedPathgrids);
        if (mWorld.mWorldScene)
            for (const ESM::FormKey& pathgrid : changedPathgrids)
                mWorld.mWorldScene->refreshOblivionPathgrid(pathgrid);
        return changed;
    }

    const ESM4::RuntimeActorAiState* OblivionAiService::state(const MWWorld::ConstPtr& actor) const
    {
        return state(actorKey(actor));
    }

    const ESM4::RuntimeActorAiState* OblivionAiService::state(const ESM::FormKey& key) const
    {
        const auto found = mActors.find(key);
        return found == mActors.end() ? nullptr : &found->second.mState;
    }

    std::optional<ESM::FormKey> OblivionAiService::currentPackage(const MWWorld::ConstPtr& actor) const
    {
        const ESM4::RuntimeActorAiState* current = state(actor);
        if (current == nullptr || current->mPackage.isNull())
            return std::nullopt;
        return current->mPackage;
    }

    ESM4::PackageProcedure OblivionAiService::currentProcedure(const MWWorld::ConstPtr& actor) const
    {
        const ESM4::RuntimeActorAiState* current = state(actor);
        return current == nullptr ? ESM4::PackageProcedure::None : current->mProcedure;
    }

    bool OblivionAiService::isCurrentPackage(const MWWorld::ConstPtr& actor, const ESM::FormKey& packageKey) const
    {
        const ESM4::RuntimeActorAiState* current = state(actor);
        return current != nullptr && current->mPackage == packageKey;
    }

    bool OblivionAiService::isRestrained(const MWWorld::ConstPtr& actor) const
    {
        const ESM4::RuntimeActorAiState* current = state(actor);
        return current != nullptr && current->mRestrained;
    }

    bool OblivionAiService::isRidingHorse(const MWWorld::ConstPtr& actor) const
    {
        const ESM4::RuntimeActorAiState* current = state(actor);
        return current != nullptr && !current->mMount.isNull();
    }

    bool OblivionAiService::getLOS(const MWWorld::ConstPtr& observer, const MWWorld::ConstPtr& target) const
    {
        return !observer.isEmpty() && !target.isEmpty() && mWorld.getLOS(observer, target);
    }

    ESM4::DetectionResult OblivionAiService::detection(
        const MWWorld::ConstPtr& observer, const MWWorld::ConstPtr& target) const
    {
        if (observer.isEmpty() || target.isEmpty())
            return { 0.0, false, false };
        const osg::Vec3f observerPosition = observer.getRefData().getPosition().asVec3();
        const osg::Vec3f targetPosition = target.getRefData().getPosition().asVec3();
        const osg::Vec3f delta = targetPosition - observerPosition;
        const float distance = delta.length();
        const float yaw = observer.getRefData().getPosition().rot[2];
        const float angle = wrapAngle(std::atan2(delta.x(), delta.y()) - yaw);
        const MWWorld::Ptr mutableObserver(
            const_cast<MWWorld::LiveCellRefBase*>(observer.mRef), const_cast<MWWorld::CellStore*>(observer.mCell));
        const MWWorld::Ptr mutableTarget(
            const_cast<MWWorld::LiveCellRefBase*>(target.mRef), const_cast<MWWorld::CellStore*>(target.mCell));
        ESM4::DetectionInput input;
        input.mDistance = distance;
        input.mViewAngleRadians = angle;
        input.mLineOfSight = getLOS(observer, target);
        if (observer.isInCell())
        {
            const std::uint32_t ambient = observer.getCell()->getCell()->getMood().mAmbiantColor;
            const double red = static_cast<double>((ambient >> 0) & 0xffu) / 255.0;
            const double green = static_cast<double>((ambient >> 8) & 0xffu) / 255.0;
            const double blue = static_cast<double>((ambient >> 16) & 0xffu) / 255.0;
            input.mAmbientLight = 0.2126 * red + 0.7152 * green + 0.0722 * blue;
        }
        if (observer.getClass().isActor())
        {
            input.mObserverAgility = mutableObserver.getClass().getCreatureStats(mutableObserver)
                .getAttribute(ESM::Attribute::indexToRefId(3)).getModified();
            input.mObserverLuck = mutableObserver.getClass().getCreatureStats(mutableObserver)
                .getAttribute(ESM::Attribute::indexToRefId(7)).getModified();
            input.mObserverSneak = mutableObserver.getClass().getSkill(mutableObserver, ESM::RefId(ESM::Skill::Sneak));
        }
        if (target.getClass().isActor())
        {
            const CreatureStats& targetStats = mutableTarget.getClass().getCreatureStats(mutableTarget);
            const Movement& movement = mutableTarget.getClass().getMovementSettings(mutableTarget);
            input.mTargetSneak = mutableTarget.getClass().getSkill(mutableTarget, ESM::RefId(ESM::Skill::Sneak));
            input.mMovementSpeed = std::hypot(movement.mPosition[0], movement.mPosition[1])
                * mutableTarget.getClass().getCurrentSpeed(mutableTarget);
            input.mTargetSleeping = [&]() {
                const ESM4::RuntimeActorAiState* targetState = state(actorKey(target));
                return targetState != nullptr && targetState->mPackageType == ESM4::AIPackageType::Sleep
                    && (targetState->mPhase == ESM4::PackagePhase::Act
                        || targetState->mPhase == ESM4::PackagePhase::Wait);
            }();
            const MagicEffects& effects = targetStats.getMagicEffects();
            input.mInvisibility = std::max(0.f,
                effects.getOrDefault(ESM::MagicEffect::Invisibility).getMagnitude()) / 100.0;
            input.mChameleon = std::max(0.f,
                effects.getOrDefault(ESM::MagicEffect::Chameleon).getMagnitude()) / 100.0;

            const ESM::FormKey targetBase = baseKey(target);
            if (const ESM4::Npc* npc = mWorld.mStore.search<ESM4::Npc>(targetBase))
                input.mFootwearNoise = std::clamp(static_cast<double>(npc->mFootWeight), 0.0, 1.0) * 0.1;
            else if (const ESM4::Creature* creature = mWorld.mStore.search<ESM4::Creature>(targetBase))
                input.mFootwearNoise = std::clamp(static_cast<double>(creature->mFootWeight), 0.0, 1.0) * 0.1;
            const float capacity = mutableTarget.getClass().getCapacity(mutableTarget);
            if (capacity > 0.f)
                input.mArmorNoise = std::clamp(
                    static_cast<double>(mutableTarget.getClass().getEncumbrance(mutableTarget) / capacity), 0.0, 1.0)
                    * 0.1;
        }
        const ESM::FormKey observerKey = actorKey(observer);
        const ESM::FormKey targetKey = actorKey(target);
        const std::uint64_t evaluationGeneration = mNextEvaluationGeneration++;
        if (mNextEvaluationGeneration == 0)
            mNextEvaluationGeneration = 1;
        input.mRandomSample = static_cast<double>(
            stableChoice(observerKey, stableChoice(targetKey, evaluationGeneration)) % 10000)
            / 10000.0;
        const ESM4::DetectionResult result = ESM4::calculateDetection(input);
        if (!observerKey.isNull() && !targetKey.isNull() && observerKey != targetKey && result.mValid)
            mDetectionVectors[{ observerKey, targetKey }] = {
                observerKey, targetKey, result.mLevel, result.mDetected, input.mLineOfSight };
        if (mEventStream)
        {
            mEventStream << "{\"event\":\"detection\",\"observer\":"
                         << jsonQuote(observerKey.serialize()) << ",\"target\":"
                         << jsonQuote(targetKey.serialize()) << ",\"score\":"
                         << std::setprecision(9) << result.mLevel << ",\"detected\":"
                         << (result.mDetected ? "true" : "false") << ",\"line_of_sight\":"
                         << (input.mLineOfSight ? "true" : "false") << ",\"random_sample\":"
                         << input.mRandomSample << ",\"generation\":" << evaluationGeneration << "}\n";
            mEventStream.flush();
        }
        return result;
    }

    MWWorld::Ptr OblivionAiService::resolveReference(const ESM::FormKey& key) const
    {
        return ptrFor(key);
    }

    std::uint64_t OblivionAiService::stableChoice(const ESM::FormKey& actor, std::uint64_t generation) const
    {
        std::uint64_t value = generation ^ (actor.mValue + 0x9e3779b97f4a7c15ULL);
        for (const unsigned char character : actor.mNamespace)
        {
            value ^= character;
            value *= 1099511628211ULL;
        }
        value ^= static_cast<std::uint64_t>(actor.mKind) << 56;
        value ^= value >> 30;
        value *= 0xbf58476d1ce4e5b9ULL;
        value ^= value >> 27;
        value *= 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }

    std::optional<ESM4::PathgridNodeKey> OblivionAiService::chooseWanderNode(
        const MWWorld::Ptr& actor, const ESM4::AIPackage& record, const LiveActor& live) const
    {
        ESM::FormKey centreCell = live.mState.mCell;
        osg::Vec3f origin = actor.getRefData().getPosition().asVec3();
        const ESM4::PackageLocation& location = record.mLocationData;
        if (location.mKind == ESM4::PackageLocationKind::InCell)
            centreCell = location.mReferenceKey;
        else if (location.mKind == ESM4::PackageLocationKind::NearReference
            || (location.mKind == ESM4::PackageLocationKind::EditorLocation
                && !location.mReferenceKey.isNull())
            || location.mKind == ESM4::PackageLocationKind::ObjectId
            || location.mKind == ESM4::PackageLocationKind::ObjectType)
        {
            std::optional<MWWorld::Ptr> reference;
            if (location.mKind == ESM4::PackageLocationKind::ObjectType)
                reference = findReference(actor, {}, location.mObjectType);
            else
                reference = findReference(actor, location.mReferenceKey, std::nullopt);
            if (!reference && location.mKind != ESM4::PackageLocationKind::EditorLocation)
                return std::nullopt;
            if (reference)
            {
                origin = reference->getRefData().getPosition().asVec3();
                centreCell = cellKey(*reference);
            }
        }
        const ESM4::PathgridGraph* graph
            = mWorld.mStore.getOblivionPathgridService().graphForCell(centreCell);
        if (graph == nullptr || graph->nodeCount() == 0)
            return std::nullopt;
        std::set<ESM4::PathgridNodeKey> reachable;
        const ESM4::PathgridService& service = mWorld.mStore.getOblivionPathgridService();
        // A wander location in another cell is resolved by the package's
        // cross-cell route, not by intersecting that cell's candidates with
        // the actor's current connected component. Applying the latter to a
        // different graph would make every valid remote location disappear.
        if (centreCell == live.mState.mCell)
            if (const ESM4::PathgridGraph* actorGraph = service.graphForCell(live.mState.mCell))
                if (const auto start = service.nearestEnabledNode(actorGraph->pathgridKey(),
                        { actor.getRefData().getPosition().asVec3().x(), actor.getRefData().getPosition().asVec3().y(),
                            actor.getRefData().getPosition().asVec3().z() }, 2048.f))
                    reachable = service.reachableComponent(*start);
        std::vector<std::uint32_t> candidates;
        const float radius = static_cast<float>(std::max(0, record.mLocationData.mRadius));
        for (std::uint32_t node = 0; node < graph->nodeCount(); ++node)
        {
            if (!graph->isEnabled(node))
                continue;
            if (!reachable.empty() && !reachable.contains({ graph->pathgridKey(), node }))
                continue;
            const ESM4::PathgridPoint point = graph->worldPoint(node);
            const osg::Vec3f position(point.mX, point.mY, point.mZ);
            if (radius <= 0.f || distanceSquared(origin, position) <= radius * radius)
                candidates.push_back(node);
        }
        if (candidates.empty())
            return std::nullopt;
        const std::size_t index = stableChoice(actorKey(actor), live.mState.mSelectionGeneration)
            % candidates.size();
        return ESM4::PathgridNodeKey{ graph->pathgridKey(), candidates[index] };
    }

    void OblivionAiService::capture(ESM4::RuntimeState& state) const
    {
        state.mPendingPackageDone = mPendingPackageDone.capture();
        state.mAiRngState = mNextEvaluationGeneration == 0 ? 1 : mNextEvaluationGeneration;
        state.mActorAi.clear();
        state.mActorAi.reserve(mActors.size());
        state.mCompanions.clear();
        state.mMounts.clear();
        state.mDetectionVectors.clear();
        state.mDetectionVectors.reserve(mDetectionVectors.size());
        for (const auto& [_, vector] : mDetectionVectors)
            state.mDetectionVectors.push_back(vector);
        std::set<std::pair<ESM::FormKey, ESM::FormKey>> companionPairs;
        std::set<std::pair<ESM::FormKey, ESM::FormKey>> mountPairs;

        std::map<ESM::FormKey, std::vector<ESM::FormKey>> companionMembers;
        const auto isCompanionPackage = [](ESM4::AIPackageType type) {
            return type == ESM4::AIPackageType::Follow || type == ESM4::AIPackageType::Escort
                || type == ESM4::AIPackageType::Accompany;
        };
        for (const auto& [member, live] : mActors)
            if (isCompanionPackage(live.mState.mPackageType) && !live.mState.mTarget.isNull())
                companionMembers[live.mState.mTarget].push_back(member);
        std::map<ESM::FormKey, std::int32_t> derivedFormation;
        for (auto& [leader, members] : companionMembers)
        {
            std::sort(members.begin(), members.end());
            for (std::size_t index = 0; index < members.size(); ++index)
                derivedFormation[members[index]] = static_cast<std::int32_t>(index);
        }
        for (const auto& [actorKeyValue, live] : mActors)
        {
            ESM4::RuntimeActorAiState saved = live.mState;
            if (!std::isfinite(saved.mNextLowProcessTick) || saved.mNextLowProcessTick < 0.f)
            {
                Log(Debug::Warning) << "TES4 AI normalized an invalid low-process tick while saving actor "
                                    << saved.mActor.serialize();
                saved.mNextLowProcessTick = 0.f;
            }
            if (isCompanionPackage(saved.mPackageType) && !saved.mTarget.isNull())
            {
                if (saved.mCompanionGroup.isNull())
                    saved.mCompanionGroup = saved.mTarget;
                if (saved.mFormationIndex < 0)
                    saved.mFormationIndex = derivedFormation[actorKeyValue];
            }
            state.mActorAi.push_back(std::move(saved));
        }
        for (const auto& [actorKeyValue, live] : mActors)
        {
            const auto type = live.mState.mPackageType;
            if (!live.mState.mTarget.isNull()
                && (type == ESM4::AIPackageType::Follow || type == ESM4::AIPackageType::Escort
                    || type == ESM4::AIPackageType::Accompany)
                && companionPairs.emplace(live.mState.mTarget, actorKeyValue).second)
            {
                const ESM::FormKey group = live.mState.mCompanionGroup.isNull()
                    ? live.mState.mTarget
                    : live.mState.mCompanionGroup;
                const std::int32_t formation = live.mState.mFormationIndex < 0
                    ? derivedFormation[actorKeyValue]
                    : live.mState.mFormationIndex;
                const ESM::FormKey sideWith = live.mState.mCompanionSideWith == live.mState.mTarget
                    ? ESM::FormKey{}
                    : live.mState.mCompanionSideWith;
                state.mCompanions.push_back({ live.mState.mTarget, actorKeyValue, group, sideWith, formation });
            }
            ESM::FormKey owner;
            ESM::FormId ownerId;
            if (const ESM4::ActorCharacter* characterReference
                = mWorld.mStore.get<ESM4::ActorCharacter>().search(actorKeyValue))
            {
                ownerId = characterReference->mOwner;
            }
            else if (const ESM4::ActorCreature* creatureReference
                = mWorld.mStore.get<ESM4::ActorCreature>().search(actorKeyValue))
                ownerId = creatureReference->mOwner;
            if (!ownerId.isZeroOrUnset())
                owner = ESM::FormKeyResolver(mWorld.mContentFiles).toFormKey(ownerId);
            if (!live.mState.mMount.isNull() && mountPairs.emplace(live.mState.mMount, actorKeyValue).second)
                state.mMounts.push_back({ live.mState.mMount, actorKeyValue, owner,
                    live.mLastRiddenHorse, true });
            else if (!live.mLastRiddenHorse.isNull()
                && mountPairs.emplace(live.mLastRiddenHorse, actorKeyValue).second)
                state.mMounts.push_back({ live.mLastRiddenHorse, actorKeyValue, owner,
                    live.mLastRiddenHorse, false });
        }
        state.mPathPoints.clear();
        for (const ESM4::PathgridNodeKey& node : mWorld.mStore.getOblivionPathgridService().disabledNodes())
            state.mPathPoints.push_back({ node.mPathgrid, node.mNode, false });
    }

    void OblivionAiService::restore(const ESM4::RuntimeState& state)
    {
        mPendingPackageDone.restore(state.mPendingPackageDone);
        mActors.clear();
        mDetectionVectors.clear();
        flushDiagnosticCounters();
        for (const ESM4::RuntimeDetectionVector& vector : state.mDetectionVectors)
            mDetectionVectors.emplace(std::make_pair(vector.mObserver, vector.mTarget), vector);
        mNextEvaluationGeneration = std::max<std::uint64_t>(1, state.mAiRngState);
        for (const ESM4::RuntimeActorAiState& saved : state.mActorAi)
        {
            LiveActor live;
            live.mState = saved;
            if (!saved.mScriptPackage.isNull())
                live.mScriptPackage = packageCandidate(saved.mScriptPackage);
            live.mSelectedWindow = saved.mScheduleWindow;
            if (!saved.mPackage.isNull())
                if (const ESM4::AIPackage* selected = package(saved.mPackage))
                    if (!live.mSelectedWindow)
                        live.mSelectedWindow = selected->mScheduleData.activeWindow(now());
            if (saved.mHasDestination)
            {
                live.mDestination = osg::Vec3f(saved.mDestinationPosition.pos[0],
                    saved.mDestinationPosition.pos[1], saved.mDestinationPosition.pos[2]);
                live.mDestinationCell = saved.mDestinationCell;
            }
            // A low-process save contains an abstract position that may be
            // ahead of the resident reference's static position. Keep it
            // authoritative until promotion validates and reconciles it.
            live.mAbstractPositionDirty = saved.mTier == ESM4::ProcessTier::Low
                && !saved.mLastValidCell.isNull() && !saved.mCell.isNull()
                && saved.mLastValidCell == saved.mCell;
            if (!live.mScriptPackage && saved.mScriptPackage.isDynamic()
                && saved.mScriptPackage.mNamespace == "force-flee")
            {
                ESM4::PackageCandidate candidate;
                candidate.mKey = saved.mScriptPackage;
                candidate.mType = ESM4::AIPackageType::FleeNotCombat;
                candidate.mSchedule.mStartHour = -1;
                candidate.mSchedule.mDuration = static_cast<std::int32_t>(
                    std::max(1.f, saved.mDurationRemaining));
                live.mScriptPackage = std::move(candidate);
                ESM4::AIPackage transient;
                transient.mFormKey = saved.mScriptPackage;
                transient.mPackageType = ESM4::AIPackageType::FleeNotCombat;
                transient.mScheduleData.mStartHour = -1;
                transient.mScheduleData.mDuration = static_cast<std::int32_t>(
                    std::max(1.f, saved.mDurationRemaining));
                transient.mTargetData.mKind = ESM4::PackageTargetKind::SpecificReference;
                transient.mTargetData.mReferenceKey = saved.mTarget;
                live.mTransientPackage = std::move(transient);
            }
            mActors.emplace(saved.mActor, std::move(live));
            mNextEvaluationGeneration = std::max(mNextEvaluationGeneration, saved.mSelectionGeneration + 1);
        }
        for (const ESM4::RuntimeCompanionRelation& relation : state.mCompanions)
        {
            const auto member = mActors.find(relation.mMember);
            if (member == mActors.end())
                continue;
            member->second.mState.mTarget = relation.mLeader;
            member->second.mState.mTargetBase = {};
            member->second.mState.mCompanionGroup = relation.mGroup;
            member->second.mState.mCompanionSideWith = relation.mSideWith;
            member->second.mState.mFormationIndex = relation.mFormationIndex;
            if (const auto leaderActor = mActors.find(relation.mLeader); leaderActor != mActors.end())
            {
                member->second.mState.mTargetBase = leaderActor->second.mState.mBase;
                if (leaderActor->second.mState.mCompanionGroup.isNull())
                    leaderActor->second.mState.mCompanionGroup = relation.mGroup;
            }
            else if (const MWWorld::Ptr leaderPtr = ptrFor(relation.mLeader); !leaderPtr.isEmpty())
                member->second.mState.mTargetBase = baseKey(leaderPtr);
        }
        // Mount relations are authoritative for current attachment state;
        // non-mounted relations retain only the last-ridden horse identity.
        // Applying them after actor records also restores the reciprocal
        // rider/horse view without relying on load-order enumeration.
        for (const ESM4::RuntimeMountRelation& relation : state.mMounts)
        {
            auto found = mActors.find(relation.mRider);
            if (found == mActors.end())
                continue;
            found->second.mLastRiddenHorse = relation.mLastRidden;
            if (relation.mMounted)
            {
                found->second.mState.mMount = relation.mHorse;
                found->second.mState.mRider = {};
                if (auto horse = mActors.find(relation.mHorse); horse != mActors.end())
                    horse->second.mState.mRider = relation.mRider;
            }
            else
                found->second.mState.mMount = {};
        }
        for (const ESM4::RuntimePathPointState& point : state.mPathPoints)
        {
            try
            {
                mWorld.mStore.getOblivionPathgridService().applyOverlay(
                    { point.mPathgrid, point.mNode }, point.mEnabled);
            }
            catch (const std::out_of_range&)
            {
                Log(Debug::Warning) << "TES4 runtime AI overlay references missing pathgrid "
                                    << point.mPathgrid.serialize();
            }
        }
        // Older saves and saves written before an actor was promoted to high
        // process contain no per-actor native state. Seed the remaining
        // winning ACHR/ACRE records after applying saved state so those actors
        // still receive low-process ticks without overwriting restored intent.
        seedActors();
    }
}
