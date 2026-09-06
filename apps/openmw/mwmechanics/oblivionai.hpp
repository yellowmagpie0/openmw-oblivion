/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#ifndef OPENMW_MWMECHANICS_OBLIVIONAI_H
#define OPENMW_MWMECHANICS_OBLIVIONAI_H

#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <osg/Vec3f>

#include <components/esm4/aiselection.hpp>
#include <components/esm4/aiphase.hpp>
#include <components/esm4/detection.hpp>
#include <components/esm4/loadpack.hpp>
#include <components/esm4/pathgriddata.hpp>
#include <components/esm4/runtimestate.hpp>

#include "../mwworld/ptr.hpp"

namespace MWWorld
{
    class World;
}

namespace MWMechanics
{
    /// Native TES4 package/process coordinator.
    ///
    /// This is intentionally the only runtime owner of an Oblivion actor's
    /// active package and phase.  Ptrs, controller paths, and navmesh handles
    /// remain ephemeral; the maps below contain only stable state and the
    /// small amount of high-process intent needed for the current frame.
    class OblivionAiService
    {
    public:
        explicit OblivionAiService(MWWorld::World& world);
        ~OblivionAiService();

        void clear();

        bool handles(const MWWorld::ConstPtr& actor) const;
        void update(const MWWorld::Ptr& actor, float duration, bool highProcess);
        // Advance actors whose CellStore is not resident.  This is a state-
        // only low-process pass; promotion to high process resumes the same
        // stable package/route intent through update().
        void updateUnloaded(float duration);
        // Door activation can move an actor between CellStores.  The actor
        // list commits those transitions after its iteration has finished.
        void commitPendingTransitions();
        void fastForward(float gameHours);

        bool evaluatePackage(const MWWorld::Ptr& actor);
        bool addScriptPackage(const MWWorld::Ptr& actor, const ESM::FormKey& package);
        bool forceFlee(const MWWorld::Ptr& actor, const ESM::FormKey& threat, float durationHours);
        bool setRestrained(const MWWorld::Ptr& actor, bool restrained);

        // Horse interaction is kept in the native coordinator so mounted
        // state, rider reciprocity, attachment, and persistence share one
        // authority. The class layer only creates the interaction action.
        bool isHorse(const MWWorld::ConstPtr& actor) const;
        bool mountHorse(const MWWorld::Ptr& rider, const MWWorld::Ptr& horse);
        bool dismountHorse(const MWWorld::Ptr& rider);

        bool setPathPoint(const ESM::FormKey& pathgrid, std::uint32_t node, bool enabled);
        bool hasLinkedPathPoints(const ESM::FormKey& object) const;
        bool setLinkedPathPoints(const ESM::FormKey& object, bool enabled);

        const ESM4::RuntimeActorAiState* state(const MWWorld::ConstPtr& actor) const;
        const ESM4::RuntimeActorAiState* state(const ESM::FormKey& actor) const;
        std::optional<ESM::FormKey> currentPackage(const MWWorld::ConstPtr& actor) const;
        ESM4::PackageProcedure currentProcedure(const MWWorld::ConstPtr& actor) const;
        bool isCurrentPackage(const MWWorld::ConstPtr& actor, const ESM::FormKey& package) const;
        bool isRestrained(const MWWorld::ConstPtr& actor) const;
        bool isRidingHorse(const MWWorld::ConstPtr& actor) const;

        bool getLOS(const MWWorld::ConstPtr& observer, const MWWorld::ConstPtr& target) const;
        ESM4::DetectionResult detection(const MWWorld::ConstPtr& observer, const MWWorld::ConstPtr& target) const;

        // Resolve a stable TES4 reference through the native actor/reference
        // registry. ObScript queries and AI must use the same bridge so that
        // loaded ESM4 actors are addressable by their FormKey.
        MWWorld::Ptr resolveReference(const ESM::FormKey& key) const;

        void capture(ESM4::RuntimeState& state) const;
        void restore(const ESM4::RuntimeState& state);

    private:
        struct LiveActor
        {
            ESM4::RuntimeActorAiState mState;
            std::optional<ESM4::PackageCandidate> mScriptPackage;
            std::optional<ESM4::AIPackage> mTransientPackage;
            std::vector<ESM4::PathgridNodeKey> mRoute;
            // Ephemeral Recast route for continuous same-cell movement. The
            // native pathgrid route remains alongside it for stable node and
            // foreign-door intent, but this route is the primary motion path
            // whenever the resident navmesh can solve the segment.
            std::vector<osg::Vec3f> mContinuousRoute;
            // Native route cursor to resume at after the collision-aware
            // resident-cell segment has been consumed.
            std::optional<std::size_t> mContinuousRouteEndCursor;
            std::optional<osg::Vec3f> mDestination;
            ESM::FormKey mDestinationCell;
            // ACHR.XHRS identifies the actor's last-ridden horse. It is not
            // a mounted-state bit; current mounting is represented by the
            // persisted mState.mMount relationship.
            ESM::FormKey mLastRiddenHorse;
            std::optional<ESM4::ScheduleWindow> mSelectedWindow;
            osg::Vec3f mLastObservedPosition;
            ESM::FormKey mLastObservedCell;
            bool mHasObservedPosition = false;
            std::size_t mRouteCursor = 0;
            std::size_t mContinuousRouteCursor = 0;
            // A resolved PGRI edge is a walkable cell boundary, not a door.
            // Keep its destination while a resident actor physically crosses
            // the boundary; otherwise the next fixed step would see the
            // actor still in the source cell and steer it back to the source
            // node.
            std::optional<ESM4::PathgridNodeKey> mForeignRouteTarget;
            // A cell-to-cell segment may be represented by paired XTEL doors
            // instead of a native PGRI edge. The source-side route ends at
            // the door; this ephemeral marker carries the exact source-door
            // position until the boundary is reached. The stable door key is
            // kept in RuntimeState::mDoor once the segment is prepared.
            std::optional<ESM::FormKey> mRouteDoor;
            osg::Vec3f mRouteDoorPosition;
            // Kept out of RuntimeState because it is a diagnostic for the
            // current route build, not gameplay intent.  The next blocked
            // event uses it to distinguish a disconnected/missing graph from
            // an explicit PGRL edge that needs a door phase.
            std::string mLastRouteFailure;
            float mFixedRemainder = 0.0f;
            float mSelectionCheckTimer = 0.0f;
            bool mPendingDoor = false;
            bool mPendingRealization = false;
            bool mNeedsSelection = false;
            // Low-process movement advances the persisted last-valid position
            // while the resident actor remains at its old render position.
            // This bit makes that abstract position authoritative until the
            // actor is promoted or an external move invalidates it.
            bool mAbstractPositionDirty = false;
        };

        struct UnloadedLocation
        {
            ESM::FormKey mReference;
            ESM::FormKey mBase;
            ESM::FormKey mCell;
            osg::Vec3f mPosition;
            ESM::RecNameInts mType = ESM::REC_REFR4;
            bool mEnabled = true;
            bool mLocked = false;
            ESM::FormKey mOwner;
            ESM::RefId mKey;
            // Stable XTEL destination data for placed doors. The destination
            // marker's cell is resolved through mUnloadedLocationByReference
            // when a route is built, so no CellStore needs to be loaded just
            // to plan a low-process transition.
            ESM::FormKey mTeleportDoor;
            osg::Vec3f mTeleportPosition;
        };

        MWWorld::World& mWorld;
        std::map<ESM::FormKey, LiveActor> mActors;
        std::vector<UnloadedLocation> mUnloadedLocations;
        std::map<ESM::FormKey, std::size_t> mUnloadedLocationByReference;
        std::map<ESM::FormKey, std::vector<std::size_t>> mUnloadedLocationsByBase;
        std::map<std::uint32_t, std::vector<std::size_t>> mUnloadedLocationsByType;
        // Directed coarse XTEL edges. The reverse index lets a route build
        // find a shortest sequence of real door transitions without scanning
        // every placed reference for every actor.
        std::map<ESM::FormKey, std::vector<std::size_t>> mUnloadedDoorsByCell;
        std::map<ESM::FormKey, std::vector<std::size_t>> mUnloadedDoorsByDestinationCell;
        mutable std::map<std::pair<ESM::FormKey, ESM::FormKey>, ESM4::RuntimeDetectionVector> mDetectionVectors;
        mutable std::map<std::string, std::uint64_t> mDiagnosticCounters;
        mutable std::uint64_t mNextEvaluationGeneration = 1;
        mutable std::ofstream mEventStream;
        std::optional<ESM4::CalendarInstant> mSimulationNow;

        ESM::FormKey actorKey(const MWWorld::ConstPtr& actor) const;
        ESM::FormKey baseKey(const MWWorld::ConstPtr& actor) const;
        ESM::FormKey cellKey(const MWWorld::ConstPtr& actor) const;

        LiveActor& ensure(const MWWorld::ConstPtr& actor);
        void synchronizeIdentity(LiveActor& live, const MWWorld::ConstPtr& actor);
        const ESM4::AIPackage* package(const ESM::FormKey& key) const;
        std::vector<ESM4::PackageCandidate> basePackages(const MWWorld::ConstPtr& actor) const;
        std::optional<ESM4::PackageCandidate> packageCandidate(const ESM::FormKey& key) const;
        ESM4::CalendarInstant now() const;
        ESM4::ConditionEvaluationContext conditionContext(
            const MWWorld::Ptr& actor, const LiveActor& live) const;
        ESM4::PackageSelection select(const MWWorld::Ptr& actor, LiveActor& live, bool restart = true);

        MWWorld::Ptr ptrFor(const ESM::FormKey& key) const;
        MWWorld::Ptr loadedPtrFor(const ESM::FormKey& key) const;
        void buildUnloadedLocationIndex();
        void seedActors();
        std::vector<ESM4::PackageCandidate> basePackages(const ESM::FormKey& base) const;
        ESM4::ConditionEvaluationContext unloadedConditionContext(const LiveActor& live) const;
        ESM4::PackageSelection selectUnloaded(LiveActor& live, bool restart = true);
        std::optional<osg::Vec3f> resolveUnloadedDestination(const ESM4::AIPackage& package,
            LiveActor& live) const;
        bool prepareUnloadedRoute(LiveActor& live);
        bool prepareDoorRoute(LiveActor& live, const ESM4::PathgridNodeKey& start,
            const ESM::FormKey& destinationCell, const MWWorld::Ptr* actor = nullptr);
        float unloadedMovementSpeed(const LiveActor& live) const;
        bool shouldRunPackage(const LiveActor& live, const ESM4::AIPackage* current,
            const osg::Vec3f& position) const;
        bool advanceUnloadedMovement(LiveActor& live, float duration, bool& reached,
            std::optional<float> movementSpeed = std::nullopt);
        bool transitionUnloaded(LiveActor& live, const ESM4::PackagePhaseInput& input);
        void executeUnloadedFixedStep(LiveActor& live, float duration);
        std::optional<MWWorld::Ptr> findReference(const MWWorld::ConstPtr& actor,
            const ESM::FormKey& key, std::optional<std::uint32_t> objectType) const;
        std::optional<osg::Vec3f> resolveTarget(const MWWorld::Ptr& actor, const ESM4::AIPackage& package,
            LiveActor& live, ESM::FormKey& targetKey) const;
        std::optional<osg::Vec3f> resolveDestination(const MWWorld::Ptr& actor,
            const ESM4::AIPackage& package, LiveActor& live, ESM::FormKey& targetKey) const;
        bool prepareRoute(const MWWorld::Ptr& actor, LiveActor& live, const osg::Vec3f& destination,
            const ESM::FormKey& destinationCell);
        bool advanceMovement(const MWWorld::Ptr& actor, LiveActor& live, float duration, bool highProcess,
            bool& reached);
        bool reconcileAbstractPosition(const MWWorld::Ptr& actor, LiveActor& live, bool allowCellChange = false);
        bool updateMountedAttachment(const MWWorld::Ptr& rider, LiveActor& live, bool highProcess);
        bool updateDialogueApproach(const MWWorld::Ptr& actor, LiveActor& live);
        bool executeFixedStep(const MWWorld::Ptr& actor, LiveActor& live, float duration, bool highProcess);
        std::optional<ESM::FormKey> doorForEdge(const ESM4::PathgridNodeKey& source,
            const ESM4::PathgridNodeKey& destination) const;
        bool canUseDoor(const MWWorld::Ptr& actor, const LiveActor& live, bool& locked) const;
        bool hasDoorOwnershipPermission(const LiveActor& live, const ESM::FormKey& owner,
            const MWWorld::Ptr* actor = nullptr) const;
        bool hasCellOwnershipPermission(const LiveActor& live, const ESM::FormKey& cell,
            const MWWorld::Ptr* actor = nullptr) const;
        bool canUseUnloadedDoor(const LiveActor& live, const UnloadedLocation& door, bool& locked,
            const MWWorld::Ptr* actor = nullptr) const;
        bool reserveAction(const MWWorld::Ptr& actor, LiveActor& live, const ESM4::AIPackage& package);
        bool completeAction(const MWWorld::Ptr& actor, LiveActor& live);
        void stopMovement(const MWWorld::Ptr& actor) const;
        void faceAndMove(const MWWorld::Ptr& actor, const osg::Vec3f& destination, bool run, bool sneak = false) const;
        bool transitionPackage(const MWWorld::Ptr& actor, LiveActor& live, const ESM4::PackagePhaseInput& input);
        void logTransition(const LiveActor& live, ESM4::PackagePhase oldPhase, std::string_view reason) const;
        void logEvent(std::string_view event, const LiveActor& live, std::string_view reason = {}) const;
        void flushDiagnosticCounters();
        std::uint64_t stableChoice(const ESM::FormKey& actor, std::uint64_t generation) const;
        std::optional<ESM4::PathgridNodeKey> chooseWanderNode(const MWWorld::Ptr& actor,
            const ESM4::AIPackage& package, const LiveActor& live) const;
    };
}

#endif
