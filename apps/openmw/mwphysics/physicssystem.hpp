#ifndef OPENMW_MWPHYSICS_PHYSICSSYSTEM_H
#define OPENMW_MWPHYSICS_PHYSICSSYSTEM_H

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include <osg/BoundingBox>
#include <osg/Quat>
#include <osg/Timer>
#include <osg/ref_ptr>

#include <components/vfs/pathutil.hpp>
#include <components/esm4/physicalcombat.hpp>
#include <components/esm4/physicalblendsettings.hpp>
#include <components/esm/formkey.hpp>
#include <components/nif/niftypes.hpp>

#include "../mwworld/ptr.hpp"

#include "collisiontype.hpp"
#include "raycasting.hpp"

namespace osg
{
    class Group;
    class Object;
    class Stats;
}

namespace MWRender
{
    class DebugDrawer;
    class LooseObjectSceneBinding;
}

namespace Resource
{
    class BulletShapeManager;
    class ResourceSystem;
}

class btCollisionWorld;
class btSequentialImpulseConstraintSolver;
class btBroadphaseInterface;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btCollisionObject;
class btCollisionShape;
class btVector3;
class btTransform;

namespace NifBullet
{
    class ActorRagdollPhysics;
    struct ActorRagdollDefinition;
    struct RagdollInternalCollisionFilter;
    struct RagdollBodyState;
    struct RagdollWorldRegistration;
    struct RagdollBonePoseWriteback;
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
    struct RagdollGetUpPlan;
    struct RagdollNativeGetUpNodeResult;
    struct RagdollNativePassOutSettings;
    enum class RagdollNativeKnockdownBlendDisposition;
    struct RagdollNativeBlendControllerTarget;
    struct RagdollNativeBlendState;
    struct RagdollBoneWorldPose;
}

namespace ESM4
{
    struct RuntimeActorRagdoll;
    struct RuntimeLooseWeapon;
    struct PhysicalBlendTimeCache;
    struct PhysicalFrameResult;
}

namespace MWPhysics
{
    // SuppliedPhysicalPose retains existing admission flags. PendingReference
    // registers pending40 and selected dynamic-root0xC separately from the
    // immutable authored graph. Caller schedules first sync before stepping.
    enum class LooseObjectSceneInitialization { SuppliedPhysicalPose, PendingReference };
    // Metadata for one admitted physical target, without reading its motion.
    // The weak identity rejects replacement bodies even on the same REFR/model.
    struct LooseObjectSceneTarget
    {
        std::uint32_t mBodyRecord = 0;
        std::uint32_t mNodeRecord = 0;
        std::optional<std::uint32_t> mReferenceRoot;
        std::uint16_t mCollisionFlags = 0;
    private:
        std::weak_ptr<const char> mIdentity;
        friend class PhysicsTaskScheduler;
    };

    // Owns detached collision geometry and a prepared map node. Destruction
    // before commit has no world effect and does not consume a projectile ID.
    class PreparedProjectile
    {
        struct Data;
        std::unique_ptr<Data> mData;
        explicit PreparedProjectile(std::unique_ptr<Data> data);
        friend class PhysicsSystem;

    public:
        ~PreparedProjectile();
        PreparedProjectile(const PreparedProjectile&) = delete;
        PreparedProjectile& operator=(const PreparedProjectile&) = delete;
    };

    class PreparedLooseObject
    {
        struct Data;
        std::unique_ptr<Data> mData;
        explicit PreparedLooseObject(std::unique_ptr<Data> data);
        friend class PhysicsSystem;
    public:
        ~PreparedLooseObject();
        bool hasLiveOwner() const noexcept;
        PreparedLooseObject(const PreparedLooseObject&) = delete;
        PreparedLooseObject& operator=(const PreparedLooseObject&) = delete;
    };

    enum class ActorRagdollCapsulePolicy { Suspend, Retain };

    class PreparedActorRagdoll
    {
        struct Data;
        std::unique_ptr<Data> mData;
        explicit PreparedActorRagdoll(std::unique_ptr<Data> data);
        friend class PhysicsSystem;
    public:
        ~PreparedActorRagdoll();
        PreparedActorRagdoll(const PreparedActorRagdoll&) = delete;
        PreparedActorRagdoll& operator=(const PreparedActorRagdoll&) = delete;
        // Borrowed detached graph. Stage controller/motion changes here;
        // commitActorRagdoll owns World publication and capsule handoff.
        NifBullet::ActorRagdollPhysics& physics();
        bool isValid() const;
        bool commit();
    };

    struct NativeRagdollSnapshotGroup;
    class PreparedNativeRagdollSnapshotRestore
    {
        struct Data;
        std::unique_ptr<Data> mData;
        explicit PreparedNativeRagdollSnapshotRestore(std::unique_ptr<Data> data);
        friend class PhysicsTaskScheduler;

    public:
        ~PreparedNativeRagdollSnapshotRestore();
        PreparedNativeRagdollSnapshotRestore(const PreparedNativeRagdollSnapshotRestore&) = delete;
        PreparedNativeRagdollSnapshotRestore& operator=(const PreparedNativeRagdollSnapshotRestore&) = delete;
    };

    struct NativeRagdollSnapshotBinding
    {
        MWWorld::Ptr mPtr;
        ESM::FormKey mActor;
        ESM::FormKey mBase;
        std::string_view mModel;
    };

    class HeightField;
    class Object;
    class Actor;
    struct ActorMovementSnapshot;
    class PhysicsTaskScheduler;
    class Projectile;
    enum ScriptedCollisionType : char;

    using ActorMap = std::unordered_map<const MWWorld::LiveCellRefBase*, std::shared_ptr<Actor>>;

    struct ContactPoint
    {
        MWWorld::Ptr mObject;
        osg::Vec3f mPoint;
        osg::Vec3f mNormal;
    };

    struct LOSRequest
    {
        LOSRequest(const std::weak_ptr<Actor>& a1, const std::weak_ptr<Actor>& a2);
        std::array<std::weak_ptr<Actor>, 2> mActors;
        std::array<const Actor*, 2> mRawActors;
        bool mResult;
        bool mStale;
        int mAge;
    };
    bool operator==(const LOSRequest& lhs, const LOSRequest& rhs) noexcept;

    struct ActorFrameData
    {
        ActorFrameData(Actor& actor, bool inert, bool waterCollision, float slowFall, float waterlevel, bool isPlayer);
        osg::Vec3f mPosition;
        osg::Vec3f mInertia;
        const btCollisionObject* mStandingOn;
        bool mIsOnGround;
        bool mIsOnSlope;
        bool mWalkingOnWater;
        const bool mInert;
        btCollisionObject* mCollisionObject;
        const float mSwimLevel;
        const float mSlowFall;
        osg::Vec2f mRotation;
        osg::Vec3f mMovement;
        std::optional<ESM4::TimedKnockbackState> mNativeKnockback;
        std::optional<ESM4::TimedKnockbackState> mInitialNativeKnockback;
        osg::Vec3f mLastStuckPosition;
        const float mWaterlevel;
        const float mHalfExtentsZ;
        float mOldHeight;
        unsigned int mStuckFrames;
        const bool mFlying;
        const bool mWasOnGround;
        const bool mIsAquatic;
        const bool mWaterCollision;
        const bool mSkipCollisionDetection;
        const bool mIsPlayer;
    };

    struct ProjectileFrameData
    {
        explicit ProjectileFrameData(Projectile& projectile);
        osg::Vec3f mPosition;
        osg::Vec3f mMovement;
        const btCollisionObject* mCaster;
        const btCollisionObject* mCollisionObject;
        Projectile* mProjectile;
    };

    struct WorldFrameData
    {
        WorldFrameData();
        WorldFrameData(bool isInStorm, const osg::Vec3f& stormDirection);
        bool mIsInStorm;
        osg::Vec3f mStormDirection;
    };

    template <class Ptr, class FrameData>
    class SimulationImpl
    {
    public:
        explicit SimulationImpl(const std::weak_ptr<Ptr>& ptr, FrameData&& data)
            : mPtr(ptr)
            , mData(data)
        {
        }

        std::optional<std::pair<std::shared_ptr<Ptr>, std::reference_wrapper<FrameData>>> lock()
        {
            if (auto locked = mPtr.lock())
                return { { std::move(locked), std::ref(mData) } };
            return std::nullopt;
        }

    private:
        std::weak_ptr<Ptr> mPtr;
        FrameData mData;
    };

    using ActorSimulation = SimulationImpl<Actor, ActorFrameData>;
    using ProjectileSimulation = SimulationImpl<Projectile, ProjectileFrameData>;
    using Simulation = std::variant<ActorSimulation, ProjectileSimulation>;

    class PhysicsSystem : public RayCastingInterface
    {
    public:
        PhysicsSystem(Resource::ResourceSystem* resourceSystem, osg::ref_ptr<osg::Group> parentNode);
        virtual ~PhysicsSystem();

        Resource::BulletShapeManager* getShapeManager();

        void enableWater(float height);
        void setWaterHeight(float height);
        void disableWater();

        void addObject(const MWWorld::Ptr& ptr, VFS::Path::NormalizedView mesh, osg::Quat rotation,
            int collisionType = CollisionType_World, bool respectVisualCollisionType = true);
        void addActor(const MWWorld::Ptr& ptr, VFS::Path::NormalizedView mesh);

        class PreparedObjectRemoval
        {
            struct Data;
            std::unique_ptr<Data> mData;
            explicit PreparedObjectRemoval(std::unique_ptr<Data> data);
            friend class PhysicsSystem;
        public:
            ~PreparedObjectRemoval();
            PreparedObjectRemoval(const PreparedObjectRemoval&) = delete;
            PreparedObjectRemoval& operator=(const PreparedObjectRemoval&) = delete;
            bool isValid() const;
            bool commit();
        };
        // Main-thread non-actor removal. Owns the scheduler collision lock;
        // destroy before callbacks. Physics, reference and cell outlive it.
        std::unique_ptr<PreparedObjectRemoval> prepareObjectRemoval(const MWWorld::Ptr& ptr);

        // Detached native body admission. Caller owns reference lifetime and
        // selects the profile/model; ordinary static collision stays separate.
        std::unique_ptr<PreparedLooseObject> prepareLooseObject(const MWWorld::Ptr& ptr,
            const NifBullet::ActorRagdollDefinition& definition, float lengthScale,
            std::span<const btTransform> poses, int collisionGroup, int collisionMask,
            std::shared_ptr<const MWRender::LooseObjectSceneBinding> sceneBinding = {},
            LooseObjectSceneInitialization sceneInitialization = LooseObjectSceneInitialization::SuppliedPhysicalPose);
        // Restore an asset-validated native snapshot while both body and
        // scene binding are still detached. Definition properties are unscaled.
        std::unique_ptr<PreparedLooseObject> prepareLooseObjectSnapshot(const MWWorld::Ptr& ptr,
            const NifBullet::ActorRagdollDefinition& definition, const ESM4::RuntimeLooseWeapon& snapshot,
            const ESM::FormKey& base, std::string_view model, int collisionGroup, int collisionMask,
            std::shared_ptr<const MWRender::LooseObjectSceneBinding> sceneBinding = {});
        ESM4::RuntimeLooseWeapon captureLooseObjectSnapshot(
            const MWWorld::Ptr& ptr, const ESM::FormKey& base, std::string_view model);
        bool validatePreparedLooseObject(const PreparedLooseObject& prepared);
        bool commitLooseObject(PreparedLooseObject& prepared);
        bool hasLooseObject(const MWWorld::Ptr& ptr);
        // Null for a deliberately renderer-free body. A linked retired or
        // replaced model rejects; it must never become an unbound fallback.
        std::shared_ptr<const MWRender::LooseObjectSceneBinding> captureLooseObjectSceneBinding(const MWWorld::Ptr& ptr);
        LooseObjectSceneTarget captureLooseObjectSceneTarget(const MWWorld::Ptr& ptr);
        // Native NIF drawing/world units, rigid scale1, before inverse bind.
        // Inactive targets return null without reading physical input. Pending
        // first-sync flags reject. Active conversion requires native length units.
        std::optional<Nif::NiTransform> captureLooseObjectTargetPose(
            const MWWorld::Ptr& ptr, const LooseObjectSceneTarget& target);
        // Exact current descriptor, native units and pending40 required. Reset
        // velocities/force scene-to-body with live precision, then clear40 and
        // return fresh metadata. World rotation/T are consumed; scale is unused.
        // Stale/invalid inputs leave motion and flags unchanged. No REFR/node writes.
        LooseObjectSceneTarget synchronizeLooseObjectFirstScenePose(const MWWorld::Ptr& ptr,
            const LooseObjectSceneTarget& target, const Nif::NiTransform& world);
        std::vector<MWWorld::Ptr> looseObjectOwners();
        std::vector<NifBullet::RagdollBodyState> captureLooseObject(const MWWorld::Ptr& ptr);
        // Reverse the retained authored body/bind transforms for a native
        // one-body model. Read-only; waits for the prior physical frame.
        // Requires the native length scale, independently of actor ragdolls.
        osg::Matrixf captureLooseObjectRootPose(const MWWorld::Ptr& ptr);
        // Prepare one native root scene traversal without advancing its local
        // state. Commit only after validating the complete World frame.
        NifBullet::RagdollBonePoseWriteback captureLooseObjectRootFrame(
            const MWWorld::Ptr& ptr, const osg::Matrixf& initialRoot);
        void commitLooseObjectRootFrame(const MWWorld::Ptr& ptr,
            const NifBullet::RagdollBonePoseWriteback& frame);
        std::vector<NifBullet::RagdollNativePackedVelocityState> captureLooseObjectPackedVelocities(
            const MWWorld::Ptr& ptr);
        void restoreLooseObject(const MWWorld::Ptr& ptr,
            std::span<const NifBullet::RagdollBodyState> states,
            std::span<const NifBullet::RagdollNativePackedVelocityState> velocities);
        void removeLooseObject(const MWWorld::Ptr& ptr);

        std::unique_ptr<PreparedActorRagdoll> prepareActorRagdoll(const MWWorld::Ptr& ptr,
            const NifBullet::ActorRagdollDefinition& definition, float lengthScale,
            std::span<const btTransform> poses, int collisionGroup, int collisionMask,
            const NifBullet::RagdollInternalCollisionFilter* internalFilter = nullptr,
            ActorRagdollCapsulePolicy capsulePolicy = ActorRagdollCapsulePolicy::Suspend);
        bool validatePreparedActorRagdoll(const PreparedActorRagdoll& prepared);
        bool commitActorRagdoll(PreparedActorRagdoll& prepared);
        void addActorRagdoll(const MWWorld::Ptr& ptr, const NifBullet::ActorRagdollDefinition& definition,
            float lengthScale, std::span<const btTransform> poses, int collisionGroup, int collisionMask,
            const NifBullet::RagdollInternalCollisionFilter* internalFilter = nullptr);
        void removeActorRagdoll(const MWWorld::Ptr& ptr);
        bool hasActorRagdoll(const MWWorld::Ptr& ptr);
        // Retain the physical owner and movement capsule while changing only
        // its caller-resolved body/constraint World registration selection.
        NifBullet::RagdollWorldRegistration captureActorRagdollWorldRegistrationSelection(const MWWorld::Ptr& ptr);
        void setActorRagdollWorldRegistrationSelection(
            const MWWorld::Ptr& ptr, const NifBullet::RagdollWorldRegistration& selection);
        // Main-thread borrowed references; the returned vector owns no actors.
        // Waits for queued physics before reading the complete current owner set.
        std::vector<MWWorld::Ptr> actorRagdollOwners();
        NifBullet::ActorRagdollDefinition actorRagdollDefinition(const MWWorld::Ptr& ptr);
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
        ESM4::RuntimeActorRagdoll captureActorRagdollSnapshot(
            const MWWorld::Ptr& ptr, const ESM::FormKey& base, std::string_view model);
        void restoreActorRagdollSnapshot(const MWWorld::Ptr& ptr,
            const ESM4::RuntimeActorRagdoll& snapshot, const ESM::FormKey& base, std::string_view model);
        // Queries/publication share the worker barrier and owned record identity.
        // These requested modes are not yet part of the physical save projection.
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
        // Owned controller state and publication share the movement-worker barrier.
        // Selected-controller curve setup after the movement-worker barrier;
        // duration/filter and whole reaction lifecycle remain caller-owned.
        std::vector<NifBullet::RagdollNativeGetUpNodeResult> setupActorRagdollGetUp(const MWWorld::Ptr& ptr,
            const NifBullet::RagdollGetUpPlan& plan, const ESM4::PhysicalBlendDurationTables& durations,
            const ESM4::PhysicalBlendGainTable& gains, float startKey);
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
        // Actual scene-node phases share the worker barrier and existing native
        // clock cache. Advance own controllers once, then supply the composed
        // animated world after the parent's physical projection. No World step
        // or automatic scene/controller scheduling is inferred here.
        void advanceActorRagdollPhysicalNodeControllers(const MWWorld::Ptr& ptr,
            std::uint32_t nodeRecord, float inputTime);
        std::optional<NifBullet::RagdollNativeBlendPublication> updateActorRagdollBlendNode(
            const MWWorld::Ptr& ptr, std::uint32_t nodeRecord, const osg::Matrixf& animatedWorld,
            float preparedFrameSeconds, std::uint32_t rawUpdateSelector,
            const std::function<void(std::span<const NifBullet::RagdollNativeBlendPublication>)>& publishScene = {});
        // Apply outer scene gates, pending first synchronization or the inner
        // blend handler. The scene hook runs before body/flag publication under
        // the World lock and must not reenter physics.
        std::optional<NifBullet::RagdollNativeBlendPublication> updateActorRagdollBlendSceneNode(
            const MWWorld::Ptr& ptr, std::uint32_t nodeRecord, const osg::Matrixf& animatedWorld,
            float preparedFrameSeconds, std::uint32_t rawUpdateSelector,
            const std::function<void(std::span<const NifBullet::RagdollNativeBlendPublication>)>& publishScene = {});
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
        std::vector<NifBullet::RagdollNativeBlendPublication> updateActorRagdollPhysicalFrame(
            const MWWorld::Ptr& ptr, std::span<const NifBullet::RagdollBoneWorldPose> bones,
            std::span<const NifBullet::RagdollNativeControllerReference> controllerOrder, float inputTime,
            float preparedFrameSeconds, std::uint32_t rawUpdateSelector,
            const std::function<void(std::span<const NifBullet::RagdollNativeBlendPublication>)>& publishScene = {});
        // Serialized with movement workers; controller supplies resolved inverse time.
        void driveActorRagdollPoseVelocities(const MWWorld::Ptr& ptr,
            std::span<const NifBullet::RagdollNativeVelocityDrive> drives, float inverseFrameSeconds);
        void applyActorRagdollImpulse(const MWWorld::Ptr& ptr, std::size_t body,
            const btVector3& impulse, const btVector3& worldPoint);

        std::unique_ptr<PreparedProjectile> prepareProjectile(
            const MWWorld::Ptr& caster, const osg::Vec3f& position, float radius);
        std::unique_ptr<PreparedProjectile> prepareProjectile(const MWWorld::Ptr& caster,
            const osg::Vec3f& position, VFS::Path::NormalizedView mesh, bool computeRadius);
        // Main-thread publication. Foreign/consumed tokens fail before writes;
        // collision registration completes before publishing the prepared map
        // node and consuming the next ID. Resource writers can prepare first.
        int commitProjectile(PreparedProjectile& prepared);

        int addProjectile(
            const MWWorld::Ptr& caster, const osg::Vec3f& position, VFS::Path::NormalizedView mesh, bool computeRadius);
        void setCaster(int projectileId, const MWWorld::Ptr& caster);
        void removeProjectile(const int projectileId);

        NativeRagdollSnapshotGroup captureActorRagdollSnapshots(std::span<const NativeRagdollSnapshotBinding> bindings);
        // Own staged buffers without publishing. Commit rejects consumed,
        // foreign or removed/replaced owners before the first publication.
        std::unique_ptr<PreparedNativeRagdollSnapshotRestore> prepareActorRagdollSnapshots(
            const NativeRagdollSnapshotGroup& snapshot, std::span<const NativeRagdollSnapshotBinding> bindings);
        void commitActorRagdollSnapshots(PreparedNativeRagdollSnapshotRestore& prepared);
        void restoreActorRagdollSnapshots(const NativeRagdollSnapshotGroup& snapshot,
            std::span<const NativeRagdollSnapshotBinding> bindings);
        void updatePtr(const MWWorld::Ptr& old, const MWWorld::Ptr& updated);

        Actor* getActor(const MWWorld::Ptr& ptr);
        const Actor* getActor(const MWWorld::ConstPtr& ptr) const;

        const Object* getObject(const MWWorld::ConstPtr& ptr) const;

        Projectile* getProjectile(int projectileId) const;

        void setIgnoreCollision(const MWWorld::Ptr& first, const MWWorld::Ptr& second, bool ignore);

        // Object or Actor
        void remove(const MWWorld::Ptr& ptr);

        void updateScale(const MWWorld::Ptr& ptr);
        void updateRotation(const MWWorld::Ptr& ptr, osg::Quat rotate);
        void updatePosition(const MWWorld::Ptr& ptr);

        void addHeightField(const float* heights, int x, int y, int size, int verts, float minH, float maxH,
            const osg::Object* holdObject);

        void removeHeightField(int x, int y);

        const HeightField* getHeightField(int x, int y) const;

        bool toggleCollisionMode();

        /// Determine new position based on all queued movements, then clear the list.
        void stepSimulation(
            float dt, bool skipSimulation, osg::Timer_t frameStart, unsigned int frameNumber, osg::Stats& stats);

        // One already prepared native process frame. Validate before changing
        // collision inputs or consuming queued movement; use the shared worker
        // world pass without adding time to the legacy accumulator.
        void stepPreparedNativeFrame(const ESM4::PhysicalFrameResult& frame, float remaining,
            bool skipSimulation, osg::Timer_t frameStart, unsigned int frameNumber, osg::Stats& stats);

        /// Apply new positions to actors
        void synchronizeCompletedMovements();
        std::vector<ActorMovementSnapshot> captureActorMovements();
        void restoreActorMovements(std::span<const ActorMovementSnapshot> snapshots);
        void moveActors();
        void debugDraw();

        std::vector<MWWorld::Ptr> getCollisions(const MWWorld::ConstPtr& ptr, int collisionGroup,
            int collisionMask) const; ///< get handles this object collides with
        std::vector<ContactPoint> getCollisionsPoints(
            const MWWorld::ConstPtr& ptr, int collisionGroup, int collisionMask) const;
        /// Actors overlapping authored non-solid phantom geometry. No AABB-only approximation.
        std::vector<MWWorld::Ptr> getTriggerActors(const MWWorld::ConstPtr& ptr) const;
        osg::Vec3f traceDown(const MWWorld::Ptr& ptr, const osg::Vec3f& position, float maxHeight);

        /// @param ignore Optional, a list of Ptr to ignore in the list of results. targets are actors to filter for,
        /// ignoring all other actors.
        RayCastingResult castRay(const osg::Vec3f& from, const osg::Vec3f& to,
            const std::vector<MWWorld::ConstPtr>& ignore = {}, const std::vector<MWWorld::Ptr>& targets = {},
            int mask = CollisionType_Default, int group = 0xff) const override;
        using RayCastingInterface::castRay;

        RayCastingResult castSphere(const osg::Vec3f& from, const osg::Vec3f& to, float radius,
            int mask = CollisionType_Default, int group = 0xff) const override;

        /// Return true if actor1 can see actor2.
        bool getLineOfSight(const MWWorld::ConstPtr& actor1, const MWWorld::ConstPtr& actor2) const override;

        bool isOnGround(const MWWorld::Ptr& actor);

        bool canMoveToWaterSurface(const MWWorld::ConstPtr& actor, const float waterlevel);

        /// Get physical half extents (scaled) of the given actor.
        osg::Vec3f getHalfExtents(const MWWorld::ConstPtr& actor) const;

        /// Get physical half extents (not scaled) of the given actor.
        osg::Vec3f getOriginalHalfExtents(const MWWorld::ConstPtr& actor) const;

        /// @see MWPhysics::Actor::getRenderingHalfExtents
        osg::Vec3f getRenderingHalfExtents(const MWWorld::ConstPtr& actor) const;

        /// Get the position of the collision shape for the actor. Use together with getHalfExtents() to get the
        /// collision bounds in world space.
        /// @note The collision shape's origin is in its center, so the position returned can be described as center of
        /// the actor collision box in world space.
        osg::Vec3f getCollisionObjectPosition(const MWWorld::ConstPtr& actor) const;

        /// Get bounding box in world space of the given object.
        osg::BoundingBox getBoundingBox(const MWWorld::ConstPtr& object) const;

        /// Queues velocity movement for a Ptr. If a Ptr is already queued, its velocity will
        /// be overwritten. Valid until the next call to stepSimulation
        void queueObjectMovement(const MWWorld::Ptr& ptr, const osg::Vec3f& velocity);

        /// Clear the queued movements list without applying.
        void clearQueuedMovement();

        /// Return true if \a actor has been standing on \a object in this frame
        /// This will trigger whenever the object is directly below the actor.
        /// It doesn't matter if the actor is stationary or moving.
        bool isActorStandingOn(const MWWorld::Ptr& actor, const MWWorld::ConstPtr& object) const;

        /// Get the handle of all actors standing on \a object in this frame.
        void getActorsStandingOn(const MWWorld::ConstPtr& object, std::vector<MWWorld::Ptr>& out) const;

        /// Return true if an object of the given type has collided with this object
        bool isObjectCollidingWith(const MWWorld::ConstPtr& object, ScriptedCollisionType type) const;

        /// Get the handle of all actors colliding with \a object in this frame.
        void getActorsCollidingWith(const MWWorld::ConstPtr& object, std::vector<MWWorld::Ptr>& out) const;

        bool toggleDebugRendering();

        /// Mark the given object as a 'non-solid' object. A non-solid object means that
        /// \a isOnSolidGround will return false for actors standing on that object.
        void markAsNonSolid(const MWWorld::ConstPtr& ptr);

        bool isOnSolidGround(const MWWorld::Ptr& actor) const;

        void updateAnimatedCollisionShape(const MWWorld::Ptr& object);

        template <class Function>
        void forEachAnimatedObject(Function&& function) const
        {
            std::for_each(mAnimatedObjects.begin(), mAnimatedObjects.end(), function);
        }

        bool isAreaOccupiedByOtherActor(
            const MWWorld::LiveCellRefBase* actor, const osg::Vec3f& position, float radius) const;

        void reportStats(unsigned int frameNumber, osg::Stats& stats) const;
        void reportCollision(const btVector3& position, const btVector3& normal);

        float mPhysicsDt;

    private:
        using IgnoredCollisionPair = std::pair<btCollisionObject*, btCollisionObject*>;

        struct IgnoredCollisionPairHash
        {
            std::size_t operator()(const IgnoredCollisionPair& pair) const
            {
                const std::size_t first = std::hash<btCollisionObject*>{}(pair.first);
                const std::size_t second = std::hash<btCollisionObject*>{}(pair.second);
                return first ^ (second + 0x9e3779b9 + (first << 6) + (first >> 2));
            }
        };

        void clearIgnoredCollisionPairs(btCollisionObject* object);
        void updateWater();

        void prepareSimulation(bool willSimulate, std::vector<Simulation>& simulations);
        void stepSimulationImpl(float dt, bool skipSimulation, osg::Timer_t frameStart, unsigned int frameNumber,
            osg::Stats& stats, const ESM4::PhysicalFrameResult* nativeFrame, float remaining);

        std::unique_ptr<btBroadphaseInterface> mBroadphase;
        std::unique_ptr<btDefaultCollisionConfiguration> mCollisionConfiguration;
        std::unique_ptr<btCollisionDispatcher> mDispatcher;
        std::unique_ptr<btSequentialImpulseConstraintSolver> mConstraintSolver;
        std::unique_ptr<btCollisionWorld> mCollisionWorld;
        std::unique_ptr<PhysicsTaskScheduler> mTaskScheduler;

        std::unique_ptr<Resource::BulletShapeManager> mShapeManager;
        Resource::ResourceSystem* mResourceSystem;

        using ObjectMap = std::unordered_map<const MWWorld::LiveCellRefBase*, std::shared_ptr<Object>>;
        ObjectMap mObjects;
        // Query-only objects: excluded from solid, navigator, ray and movement lookups.
        ObjectMap mTriggers;

        std::map<Object*, bool> mAnimatedObjects; // stores pointers to elements in mObjects

        ActorMap mActors;
        std::unordered_set<IgnoredCollisionPair, IgnoredCollisionPairHash> mIgnoredCollisionPairs;

        using ProjectileMap = std::map<int, std::shared_ptr<Projectile>>;
        ProjectileMap mProjectiles;
        // Tokens retain identity without retaining or dereferencing the owner.
        // A new PhysicsSystem at the same address must not accept old tokens.
        std::shared_ptr<const char> mProjectilePreparationOwner = std::make_shared<const char>(0);
        std::shared_ptr<const char> mLoosePreparationOwner = std::make_shared<const char>(0);
        std::shared_ptr<const char> mActorPreparationOwner = std::make_shared<const char>(0);

        using HeightFieldMap = std::map<std::pair<int, int>, std::unique_ptr<HeightField>>;
        HeightFieldMap mHeightFields;

        bool mDebugDrawEnabled;

        float mTimeAccum;

        unsigned int mProjectileId;

        float mWaterHeight;
        bool mWaterEnabled;

        std::unique_ptr<btCollisionObject> mWaterCollisionObject;
        std::unique_ptr<btCollisionShape> mWaterCollisionShape;

        std::unique_ptr<MWRender::DebugDrawer> mDebugDrawer;

        osg::ref_ptr<osg::Group> mParentNode;

        std::size_t mSimulationsCounter = 0;
        std::array<std::vector<Simulation>, 2> mSimulations;
        std::vector<std::pair<MWWorld::Ptr, osg::Vec3f>> mActorsPositions;

        PhysicsSystem(const PhysicsSystem&);
        PhysicsSystem& operator=(const PhysicsSystem&);
    };
}

#endif
