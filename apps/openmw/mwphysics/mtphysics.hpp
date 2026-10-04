#ifndef OPENMW_MWPHYSICS_MTPHYSICS_H
#define OPENMW_MWPHYSICS_MTPHYSICS_H

#include <atomic>
#include <condition_variable>
#include <memory>
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
    struct RagdollNativeVelocityControllerState;
    struct RagdollNativeControllerReference;
    struct RagdollNativeForceRequest;
    struct RagdollNativeKnockdownBlendRequest;
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

        // Main-thread ownership operations wait for the previous worker frame.
        void addActorRagdoll(const MWWorld::Ptr& ptr, const NifBullet::ActorRagdollDefinition& definition,
            float lengthScale, std::span<const btTransform> poses, int collisionGroup, int collisionMask,
            const NifBullet::RagdollInternalCollisionFilter* internalFilter = nullptr);
        void removeActorRagdoll(const MWWorld::Ptr& ptr);
        bool hasActorRagdoll(const MWWorld::Ptr& ptr);
        NifBullet::ActorRagdollDefinition actorRagdollDefinition(const MWWorld::Ptr& ptr);
        ESM4::RuntimeActorRagdoll captureActorRagdollSnapshot(
            const MWWorld::Ptr& ptr, const ESM::FormKey& base, std::string_view model);
        void restoreActorRagdollSnapshot(const MWWorld::Ptr& ptr,
            const ESM4::RuntimeActorRagdoll& snapshot, const ESM::FormKey& base, std::string_view model);
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
        std::vector<NifBullet::RagdollNativeVelocityControllerState> captureActorRagdollVelocityControllers(const MWWorld::Ptr& ptr);
        void restoreActorRagdollVelocityControllers(const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollNativeVelocityControllerState> controllers);
        std::vector<NifBullet::RagdollNativeControllerReference> captureActorRagdollControllerOrder(const MWWorld::Ptr& ptr, std::span<const std::uint32_t> nodeOrder);
        void advanceActorRagdollPhysicalControllers(const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollNativeControllerReference> controllerOrder, float inputTime);
        void applyActorRagdollNativeForces(const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollNativeForceRequest> requests);
        std::vector<NifBullet::RagdollNativeBlendControllerState> captureActorRagdollBlendControllers(const MWWorld::Ptr& ptr);
        std::vector<NifBullet::RagdollNativeBlendState> captureActorRagdollBlendStates(const MWWorld::Ptr& ptr);
        ESM4::PhysicalBlendTimeCache captureNativeBlendTimeCache();
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
        void prepareWork(float& timeAccum, std::vector<Simulation>& simulations, osg::Timer_t frameStart,
            unsigned int frameNumber, osg::Stats& stats, const WorldFrameData& worldData);

        std::unique_ptr<WorldFrameData> mWorldFrameData;
        std::vector<Simulation>* mSimulations = nullptr;
        std::unordered_set<const btCollisionObject*> mCollisionObjects;
        class NativeSceneBinding;
        std::unique_ptr<NativeSceneBinding> mNativeSceneBinding;
        std::unordered_map<const MWWorld::LiveCellRefBase*, std::unique_ptr<ActorRagdoll>> mActorRagdolls;
        // Shared by owned native physical controllers across actors. Access
        // only after the worker barrier under the collision-world lock.
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
