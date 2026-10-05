#ifndef OPENMW_MWPHYSICS_MTPHYSICS_H
#define OPENMW_MWPHYSICS_MTPHYSICS_H

#include <atomic>
#include <condition_variable>
#include <memory>
#include <map>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>

#include <osg/Timer>

#include "components/misc/budgetmeasurement.hpp"
#include <components/esm/formkey.hpp>
#include "physicssystem.hpp"
#include "ptrholder.hpp"

class btTransform;

namespace ESM4
{
    struct RuntimeActorRagdoll;
}

namespace NifBullet
{
    struct ActorRagdollDefinition;
    struct RagdollInternalCollisionFilter;
    struct RagdollBodyState;
    struct RagdollNativePackedVelocityState;
    struct RagdollNativeWorldSceneRequest;
    struct RagdollNativeVelocityDrive;
    struct RagdollNativeMotionRequest;
    struct RagdollNativeScenePoseRequest;
    struct RagdollNativeBlendUpdate;
    struct RagdollNativeBlendPublication;
    struct RagdollNativeBlendControllerState;
    struct RagdollNativeVelocitySetupRequest;
    struct RagdollNativeHitVelocitySetupRequest;
    struct RagdollNativeVelocityControllerState;
    struct RagdollNativeControllerReference;
    struct RagdollNativeForceRequest;
    struct RagdollNativeKnockdownBlendRequest;
    struct RagdollNativeHitBlendSetupRequest;
    enum class RagdollNativeHitBlendDisposition;
    struct RagdollNativeKnockdownControllerSetupRequest;
    struct RagdollNativePassOutSettings;
    enum class RagdollNativeKnockdownBlendDisposition;
    struct RagdollNativeBlendControllerTarget;
    struct RagdollNativeBlendState;
    struct RagdollBoneWorldPose;
}

namespace Misc
{
    class Barrier;
}

namespace MWRender
{
    class DebugDrawer;
}

namespace MWPhysics
{
    struct NativeRagdollRestoreLifetime;
    enum class LockingPolicy
    {
        NoLocks,
        ExclusiveLocksOnly,
        AllowSharedLocks,
    };

    class PhysicsTaskScheduler
    {
    public:
        PhysicsTaskScheduler(float physicsDt, btCollisionWorld* collisionWorld, MWRender::DebugDrawer* debugDrawer);
        ~PhysicsTaskScheduler();

        /// @brief move actors taking into account desired movements and collisions
        /// @param numSteps how much simulation step to run
        /// @param timeAccum accumulated time from previous run to interpolate movements
        /// @param actorsData per actor data needed to compute new positions
        /// @return new position of each actor
        void applyQueuedMovements(float& timeAccum, std::vector<Simulation>& simulations, osg::Timer_t frameStart,
            unsigned int frameNumber, osg::Stats& stats, const WorldFrameData& worldData);

        void resetSimulation(const ActorMap& actors);
        void suspendActorCollision(Actor& actor, bool suspended);

        class PreparedObjectRemoval
        {
            struct Data;
            std::unique_ptr<Data> mData;
            explicit PreparedObjectRemoval(std::unique_ptr<Data> data);
            friend class PhysicsTaskScheduler;
        public:
            ~PreparedObjectRemoval();
            PreparedObjectRemoval(const PreparedObjectRemoval&) = delete;
            PreparedObjectRemoval& operator=(const PreparedObjectRemoval&) = delete;
            bool isValid() const;
            bool commit();
            std::span<btCollisionObject* const> collisionObjects() const;
        };
        // Holds the collision lock after waiting for workers. The scheduler,
        // reference and optional static collision object must outlive the plan.
        std::unique_ptr<PreparedObjectRemoval> prepareObjectRemoval(
            const MWWorld::Ptr& ptr, btCollisionObject* staticObject);

        class PreparedLooseObject
        {
            struct Impl;
            std::unique_ptr<Impl> mImpl;
            explicit PreparedLooseObject(std::unique_ptr<Impl> impl);
            friend class PhysicsTaskScheduler;
        public:
            ~PreparedLooseObject();
            PreparedLooseObject(PreparedLooseObject&&) noexcept;
            PreparedLooseObject& operator=(PreparedLooseObject&&) noexcept;
            PreparedLooseObject(const PreparedLooseObject&) = delete;
            PreparedLooseObject& operator=(const PreparedLooseObject&) = delete;
        };

        // One non-actor body, independently of actor life/ragdoll ownership.
        // The caller keeps the borrowed reference alive through preparation
        // and registration, then removes physics before deleting that reference.
        // Construction/cancellation do not register collision or consume state.
        std::unique_ptr<PreparedLooseObject> prepareLooseObject(const MWWorld::Ptr& ptr,
            const NifBullet::ActorRagdollDefinition& definition, float lengthScale,
            std::span<const btTransform> poses, int collisionGroup, int collisionMask);
        bool validatePreparedLooseObject(const PreparedLooseObject& object);
        bool commitLooseObject(PreparedLooseObject& object);
        bool hasLooseObject(const MWWorld::Ptr& ptr);
        std::vector<MWWorld::Ptr> looseObjectOwners();
        std::vector<NifBullet::RagdollBodyState> captureLooseObject(const MWWorld::Ptr& ptr);
        // Reverse the retained authored body/bind transforms for a native
        // one-body model. Read-only; waits for the prior physical frame.
        // Requires the native length scale, independently of actor ragdolls.
        osg::Matrixf captureLooseObjectRootPose(const MWWorld::Ptr& ptr);
        std::vector<NifBullet::RagdollNativePackedVelocityState> captureLooseObjectPackedVelocities(
            const MWWorld::Ptr& ptr);
        void restoreLooseObject(const MWWorld::Ptr& ptr,
            std::span<const NifBullet::RagdollBodyState> states,
            std::span<const NifBullet::RagdollNativePackedVelocityState> velocities);
        void removeLooseObject(const MWWorld::Ptr& ptr);
        void updateLooseObjectPtr(const MWWorld::Ptr& old, const MWWorld::Ptr& updated);
        btCollisionObject* looseObjectCollisionObject(const MWWorld::ConstPtr& ptr);


        // Main-thread ownership operations wait for the previous worker frame.
        void addActorRagdoll(const MWWorld::Ptr& ptr, const NifBullet::ActorRagdollDefinition& definition,
            float lengthScale, std::span<const btTransform> poses, int collisionGroup, int collisionMask,
            const NifBullet::RagdollInternalCollisionFilter* internalFilter = nullptr);
        void removeActorRagdoll(const MWWorld::Ptr& ptr);
        bool hasActorRagdoll(const MWWorld::Ptr& ptr);
        // Main-thread borrowed references; the returned vector owns no actors.
        // Waits for queued physics before reading the complete current owner set.
        std::vector<MWWorld::Ptr> actorRagdollOwners();
        NifBullet::ActorRagdollDefinition actorRagdollDefinition(const MWWorld::Ptr& ptr);
        ESM4::RuntimeActorRagdoll captureActorRagdollSnapshot(
            const MWWorld::Ptr& ptr, const ESM::FormKey& base, std::string_view model);
        void restoreActorRagdollSnapshot(const MWWorld::Ptr& ptr,
            const ESM4::RuntimeActorRagdoll& snapshot, const ESM::FormKey& base, std::string_view model);
        // Complete owner bindings; capture/prepare all under one worker barrier.
        NativeRagdollSnapshotGroup captureActorRagdollSnapshots(std::span<const NativeRagdollSnapshotBinding> bindings);
        // Own staged buffers without publishing. Commit rejects consumed,
        // foreign or removed/replaced owners before the first publication.
        std::unique_ptr<PreparedNativeRagdollSnapshotRestore> prepareActorRagdollSnapshots(
            const NativeRagdollSnapshotGroup& snapshot, std::span<const NativeRagdollSnapshotBinding> bindings);
        void commitActorRagdollSnapshots(PreparedNativeRagdollSnapshotRestore& prepared);
        void restoreActorRagdollSnapshots(const NativeRagdollSnapshotGroup& snapshot,
            std::span<const NativeRagdollSnapshotBinding> bindings);
        void updateActorRagdollPtr(const MWWorld::Ptr& old, const MWWorld::Ptr& updated);
        std::vector<NifBullet::RagdollBodyState> captureActorRagdoll(const MWWorld::Ptr& ptr);
        std::vector<NifBullet::RagdollNativePackedVelocityState> captureActorRagdollNativePackedVelocities(
            const MWWorld::Ptr& ptr);
        void restoreActorRagdollNativePackedVelocities(const MWWorld::Ptr& ptr,
            std::span<const NifBullet::RagdollNativePackedVelocityState> states);
        // World guard bits come from this scheduler's owned wrapper binding.
        // Caller supplies prepared native getter/target/frame fields. Stage all
        // writes before the optional atomic hook; no physics reentry in the hook.
        // This does not infer getter clock lanes or automatic collision ordering.
        std::vector<std::uint32_t> synchronizeActorRagdollWorldScenes(const MWWorld::Ptr& ptr,
            std::span<const NifBullet::RagdollNativeWorldSceneRequest> requests,
            const std::function<void(std::span<const std::uint32_t>)>& beforePublish = {});
        void restoreActorRagdoll(const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollBodyState> states);
        // Queries/publication share the worker barrier and owned record identity.
        // Capture returns the actual logical modes saved by current snapshots.
        std::vector<NifBullet::RagdollNativeMotionRequest> captureActorRagdollNativeMotionModes(const MWWorld::Ptr& ptr);
        void setActorRagdollNativeMotionModes(const MWWorld::Ptr& ptr,
            std::span<const NifBullet::RagdollNativeMotionRequest> requests);
        // Sparse scene targets publish only after the movement worker barrier.
        void synchronizeActorRagdollKeyframedPoses(const MWWorld::Ptr& ptr,
            std::span<const NifBullet::RagdollNativeScenePoseRequest> poses);
        // Atomic controller batch after queued movement workers finish.
        // Prepared native clock/selector remain caller-owned.
        std::vector<NifBullet::RagdollNativeBlendPublication> updateActorRagdollBlends(
            const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollNativeBlendUpdate> updates,
            float preparedFrameSeconds, std::uint32_t rawUpdateSelector);
        // Selected-controller curve setup after the movement-worker barrier;
        // duration/filter and whole reaction lifecycle remain caller-owned.
        std::vector<NifBullet::RagdollNativeKnockdownBlendDisposition> prepareActorRagdollKnockdownBlends(
            const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollNativeKnockdownBlendRequest> requests);
        // Atomic normal Down setup; caller resolves configuration and motion synchronization.
        std::vector<NifBullet::RagdollNativeKnockdownBlendDisposition> prepareActorRagdollKnockdownControllerSetup(
            const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollNativeKnockdownControllerSetupRequest> requests,
            NifBullet::RagdollNativePassOutSettings settings);
        // Generated controller ownership and immediate force/controller phase
        // share the worker/world barrier and scheduler physical clock cache.
        void prepareActorRagdollVelocityControllers(const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollNativeVelocitySetupRequest> requests);
        std::vector<NifBullet::RagdollNativeHitBlendDisposition> prepareActorRagdollHitBlends(const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollNativeHitBlendSetupRequest> requests);
        std::vector<NifBullet::RagdollNativeHitBlendDisposition> prepareActorRagdollHitControllerSetup(
            const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollNativeHitBlendSetupRequest> blends,
            std::span<const NifBullet::RagdollNativeHitVelocitySetupRequest> velocities);
        void prepareActorRagdollHitVelocityControllers(const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollNativeHitVelocitySetupRequest> requests);
        std::vector<NifBullet::RagdollNativeVelocityControllerState> captureActorRagdollVelocityControllers(const MWWorld::Ptr& ptr);
        void restoreActorRagdollVelocityControllers(const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollNativeVelocityControllerState> controllers);
        std::vector<NifBullet::RagdollNativeControllerReference> captureActorRagdollControllerOrder(const MWWorld::Ptr& ptr, std::span<const std::uint32_t> nodeOrder);
        void advanceActorRagdollPhysicalControllers(const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollNativeControllerReference> controllerOrder, float inputTime);
        void applyActorRagdollNativeForces(const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollNativeForceRequest> requests);
        std::vector<NifBullet::RagdollNativeBlendControllerState> captureActorRagdollBlendControllers(const MWWorld::Ptr& ptr);
        std::vector<NifBullet::RagdollNativeBlendState> captureActorRagdollBlendStates(const MWWorld::Ptr& ptr);
        ESM4::PhysicalBlendTimeCache captureNativeBlendTimeCache();
        void restoreNativeBlendTimeCache(const ESM4::PhysicalBlendTimeCache& cache);
        std::vector<NifBullet::RagdollNativeBlendPublication> updateActorRagdollBlendControllers(
            const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollNativeBlendControllerTarget> targets,
            float inputTime, float preparedFrameSeconds, std::uint32_t rawUpdateSelector);
        // Called on the renderer update thread. Scene publication must be
        // atomic and must not reenter physics while the worker/world lock is held.
        std::vector<NifBullet::RagdollNativeBlendPublication> updateActorRagdollBlendFrame(
            const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollBoneWorldPose> bones,
            std::span<const std::uint32_t> controllerOrder, float inputTime,
            float preparedFrameSeconds, std::uint32_t rawUpdateSelector,
            const std::function<void(std::span<const NifBullet::RagdollNativeBlendPublication>)>& publishScene = {});
        // Serialized with movement workers; controller supplies resolved inverse time.
        void driveActorRagdollPoseVelocities(const MWWorld::Ptr& ptr,
            std::span<const NifBullet::RagdollNativeVelocityDrive> drives, float inverseFrameSeconds);
        void applyActorRagdollImpulse(const MWWorld::Ptr& ptr, std::size_t body,
            const btVector3& impulse, const btVector3& worldPoint);

        // Thread safe wrappers
        void rayTest(const btVector3& rayFromWorld, const btVector3& rayToWorld,
            btCollisionWorld::RayResultCallback& resultCallback) const;
        void convexSweepTest(const btConvexShape* castShape, const btTransform& from, const btTransform& to,
            btCollisionWorld::ConvexResultCallback& resultCallback) const;
        void contactTest(btCollisionObject* colObj, btCollisionWorld::ContactResultCallback& resultCallback);
        std::optional<btVector3> getHitPoint(const btTransform& from, btCollisionObject* target);
        void aabbTest(const btVector3& aabbMin, const btVector3& aabbMax, btBroadphaseAabbCallback& callback);
        void getAabb(const btCollisionObject* obj, btVector3& min, btVector3& max);
        void setCollisionFilterMask(btCollisionObject* collisionObject, int collisionFilterMask);
        void addCollisionObject(btCollisionObject* collisionObject, int collisionFilterGroup, int collisionFilterMask);
        void removeCollisionObject(btCollisionObject* collisionObject);
        void updateSingleAabb(const std::shared_ptr<PtrHolder>& ptr, bool immediate = false);
        bool getLineOfSight(const std::shared_ptr<Actor>& actor1, const std::shared_ptr<Actor>& actor2);
        void debugDraw();
        void* getUserPointer(const btCollisionObject* object) const;
        void releaseSharedStates(); // destroy all objects whose destructor can't be safely called from
                                    // ~PhysicsTaskScheduler()

    private:
        class WorkersSync;
        class ActorRagdoll;
        class LooseObject;
        void clearLooseObjects();
        bool validatePreparedLooseObjectLocked(const PreparedLooseObject& object) const;
        void clearActorRagdolls();
        ActorRagdoll& actorRagdoll(const MWWorld::Ptr& ptr);

        void doSimulation();
        void worker();
        void updateActorsPositions();
        bool hasLineOfSight(const Actor* actor1, const Actor* actor2);
        void refreshLOSCache();
        void updateAabbs();
        void updatePtrAabb(const std::shared_ptr<PtrHolder>& ptr);
        void updateStats(osg::Timer_t frameStart, unsigned int frameNumber, osg::Stats& stats);
        std::tuple<unsigned, float> calculateStepConfig(float timeAccum) const;
        void afterPreStep();
        void afterPostStep();
        void afterPostSim();
        void syncWithMainThread();
        void waitForWorkers();
        void validateActorRagdollBindings(std::span<const NativeRagdollSnapshotBinding> bindings) const;
        std::unique_ptr<PreparedNativeRagdollSnapshotRestore> prepareActorRagdollSnapshotsLocked(
            const NativeRagdollSnapshotGroup& snapshot, std::span<const NativeRagdollSnapshotBinding> bindings);
        void commitActorRagdollSnapshotsLocked(PreparedNativeRagdollSnapshotRestore& prepared);
        void prepareWork(float& timeAccum, std::vector<Simulation>& simulations, osg::Timer_t frameStart,
            unsigned int frameNumber, osg::Stats& stats, const WorldFrameData& worldData);

        std::unique_ptr<WorldFrameData> mWorldFrameData;
        std::vector<Simulation>* mSimulations = nullptr;
        std::unordered_set<const btCollisionObject*> mCollisionObjects;
        class NativeSceneBinding;
        std::unique_ptr<NativeSceneBinding> mNativeSceneBinding;
        std::unordered_map<const MWWorld::LiveCellRefBase*, std::unique_ptr<ActorRagdoll>> mActorRagdolls;
        using LooseObjectMap = std::map<const MWWorld::LiveCellRefBase*, std::unique_ptr<LooseObject>>;
        LooseObjectMap mLooseObjects;
        std::shared_ptr<const char> mLoosePreparationIdentity;

        // Shared by owned native physical controllers across actors. Access
        // only after the worker barrier under the collision-world lock.
        std::shared_ptr<const NativeRagdollRestoreLifetime> mNativeRagdollRestoreLifetime;
        std::unique_ptr<ESM4::PhysicalBlendTimeCache> mNativeBlendTimeCache;
        float mDefaultPhysicsDt;
        float mPhysicsDt;
        float mTimeAccum;
        btCollisionWorld* mCollisionWorld;
        MWRender::DebugDrawer* mDebugDrawer;
        std::vector<LOSRequest> mLOSCache;
        std::set<std::weak_ptr<PtrHolder>, std::owner_less<std::weak_ptr<PtrHolder>>> mUpdateAabb;

        // TODO: use std::experimental::flex_barrier or std::barrier once it becomes a thing
        std::unique_ptr<Misc::Barrier> mPreStepBarrier;
        std::unique_ptr<Misc::Barrier> mPostStepBarrier;
        std::unique_ptr<Misc::Barrier> mPostSimBarrier;

        LockingPolicy mLockingPolicy;
        unsigned mNumThreads;
        int mNumJobs;
        unsigned mRemainingSteps;
        int mLOSCacheExpiry;
        bool mAdvanceSimulation;
        std::atomic<int> mNextJob;
        std::atomic<int> mNextLOS;
        std::vector<std::thread> mThreads;

        mutable std::shared_mutex mSimulationMutex;
        mutable std::shared_mutex mCollisionWorldMutex;
        mutable std::shared_mutex mLOSCacheMutex;
        mutable std::mutex mUpdateAabbMutex;

        unsigned int mFrameNumber;
        const osg::Timer* mTimer;

        unsigned mPrevStepCount;
        Misc::BudgetMeasurement mBudget;
        Misc::BudgetMeasurement mAsyncBudget;
        unsigned int mBudgetCursor;
        osg::Timer_t mAsyncStartTime;
        osg::Timer_t mTimeBegin;
        osg::Timer_t mTimeEnd;
        osg::Timer_t mFrameStart;

        std::unique_ptr<WorkersSync> mWorkersSync;
    };

}
#endif
