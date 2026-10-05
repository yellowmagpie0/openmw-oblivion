#ifndef OPENMW_COMPONENTS_NIFBULLET_ACTORRAGDOLLPHYSICS_HPP
#define OPENMW_COMPONENTS_NIFBULLET_ACTORRAGDOLLPHYSICS_HPP

#include "actorragdoll.hpp"
#include "ragdollcollisionfilter.hpp"
#include "ragdollvelocity.hpp"

#include <components/esm4/physicalblendsettings.hpp>
#include <components/esm4/physicalvelocitycontroller.hpp>
#include <components/esm4/physicalsceneworld.hpp>

#include <functional>
#include <memory>
#include <span>
#include <utility>

#include <LinearMath/btTransform.h>

class btDynamicsWorld;
class btCollisionObject;

namespace NifBullet
{
    // Prepare linked collision gains/flags from explicit resolved native body
    // filters before physical admission; preserve the authored source graph.
    ActorRagdollDefinition ragdollDefinitionWithNativeLinkedBlendState(
        const ActorRagdollDefinition& authored, std::span<const std::uint32_t> resolvedPackedFilters,
        const ESM4::PhysicalBlendGainTable& resolvedGains);

    struct RagdollNativeKeyframedStepResult
    {
        RagdollNativeTargetPose mBodyPose;
        osg::Vec3f mCenterOfMass;
        RagdollNativeVelocities mVelocities;
        // Original motion+A0: twice the capped half-angle vector, followed
        // by sqrt(stored angular fraction squared) * native pi.
        std::array<float, 4> mAngularDelta;
        float mLinearW = 0.f, mAngularW = 0.f;
    };

    // Original keyframed motion virtual+10 8EA4B0. Uses physical current COM,
    // rotation and raw local COM; no damping/gravity or scene/bodyT conversion.
    // Computes a new physical pose, not a World step or contact publication.
    // Packed fourth velocity lanes use the same native XYZ-derived cap factors;
    // they do not contribute to pose integration or squared-length reductions.
    RagdollNativeKeyframedStepResult ragdollNativeKeyframedMotionStep(const osg::Vec3f& currentCenterOfMass,
        const std::array<float, 4>& currentRotation, const osg::Vec3f& localCenterOfMass,
        const RagdollNativeVelocities& velocities, float frameSeconds,
        float maximumLinearVelocity, float angularLimit, float linearW = 0.f, float angularW = 0.f);

    // Original bhk position adapters store binary32 after multiplying by
    // these separately stored constants. The reverse is not computed as 1/k.
    inline constexpr float RagdollNativeLengthScale = 6.999040126800537f;
    osg::Vec3f ragdollWorldToNativePosition(const osg::Vec3f& position);
    osg::Vec3f ragdollNativeToWorldPosition(const osg::Vec3f& position);

    struct RagdollBoneWorldPose
    {
        std::uint32_t mNodeRecord;
        osg::Matrixf mPose;
    };

    // Current scene bone world pose at the bhk scene-sync boundary.
    // Input already includes actor/world placement and must be rigid.
    // The single pose uses native lengths; the graph adapter returns world
    // lengths for ActorRagdollPhysics with RagdollNativeLengthScale.
    // Native scene rotation extraction and position conversion before the
    // Bullet transform representation. Matrix must already be rigid.
    RagdollNativeTargetPose ragdollNativeSceneTargetPose(const osg::Matrixf& worldPose);
    // Ordinary scene sync plus the native bhkRigidBodyT local offset, when
    // present. Local translation is already in native lengths and includes
    // any graph property scaling.
    RagdollNativeTargetPose ragdollNativeSceneBodyTargetPose(
        const osg::Matrixf& worldPose, const RagdollBodyDefinition& body);
    // Native8B8FB0/8B9150 reverse bodyT offset, in native length units.
    // Ordinary bodies retain their native origin/quaternion unchanged.
    // Matrix/world-length publication remains a separate operation.
    RagdollNativeTargetPose ragdollNativeSceneTargetFromBodyPose(
        const RagdollNativeTargetPose& bodyPose, const RagdollBodyDefinition& body);
    // Native8B9050 COM getter uses the physical motion basis, independently
    // of reverse scene-origin projection. All positions use native lengths.
    osg::Vec3f ragdollNativeSceneCenterOfMass(const osg::Vec3f& bodyCenter,
        const std::array<float, 4>& bodyRotation, const RagdollBodyDefinition& body);
    // Original mixed blend target:539850 matrix layout then8B1B40 quaternion.
    // Keep its unrounded intermediates and raw quaternion for later Slerp;
    // unlike keyframed scene sync, this boundary does not normalize.
    RagdollNativeTargetPose ragdollNativeBlendSceneTargetPose(const osg::Matrixf& worldPose);
    btTransform ragdollNativePoseFromBoneWorld(const osg::Matrixf& worldPose);
    // Native motion origin/XYZW quaternion returned through bhk scene adapters.
    // Output uses world lengths; input quaternion must already be unit length.
    osg::Matrixf ragdollBoneWorldFromNativePose(const RagdollNativeTargetPose& pose);
    // Original mixed renderer uses8B1DD0 instead of ordinary47C600 stores.
    osg::Matrixf ragdollBoneWorldFromNativeBlendPose(const RagdollNativeTargetPose& pose);

    struct RagdollNativeBlendPoseTargets
    {
        RagdollNativeTargetPose mDriveTarget;
        osg::Matrixf mSceneTarget;
    };

    // For an already selected PoseAndVelocity route. Drive target is mixed;
    // scene position uses the animated target unless flag0x100 selects mixed.
    // Both targets are prepared before any caller-owned scene/body publication.
    RagdollNativeBlendPoseTargets ragdollNativeBlendPoseTargets(const RagdollNativeTargetPose& physical,
        const osg::Matrixf& animatedWorld, float hierarchyGain, std::uint16_t collisionFlags);
    std::vector<btTransform> ragdollBodyWorldPoses(const ActorRagdollDefinition& definition,
        std::span<const RagdollBoneWorldPose> bones);

    // Original torque*dt binary32 store, then torque-unit conversion to the
    // caller's world units (mass is unchanged, lengths use lengthScale).
    btScalar ragdollFrictionImpulse(float torque, float frameSeconds, float lengthScale);

    struct RagdollBodyState
    {
        std::uint32_t mRecord;
        btTransform mPose;
        btVector3 mLinearVelocity;
        btVector3 mAngularVelocity;
    };

    enum class RagdollNativeMotion : std::uint8_t
    {
        Dynamic = 1,
        Keyframed = 6,
    };

    struct RagdollNativeMotionRequest
    {
        std::uint32_t mRecord;
        RagdollNativeMotion mMotion;
        friend bool operator==(const RagdollNativeMotionRequest&, const RagdollNativeMotionRequest&) = default;
    };

    struct RagdollNativeScenePoseRequest
    {
        std::uint32_t mRecord;
        osg::Matrixf mWorldPose;
    };

    struct RagdollNativePackedVelocityState
    {
        std::uint32_t mRecord;
        ESM4::PhysicalWorldSceneVelocities mVelocities;
    };

    struct RagdollNativeWorldSceneRequest
    {
        std::uint32_t mRecord;
        // Caller-resolved original getter snapshot, converted target and frame.
        // A borrowed Bullet World alone does not resolve native authority.
        ESM4::PhysicalWorldSceneInput mInput;
    };

    struct RagdollNativeVelocityDrive
    {
        std::uint32_t mRecord;
        RagdollNativeTargetPose mTarget;
        float mVelocityGain;
    };

    struct RagdollNativeForceRequest
    {
        std::uint32_t mRecord;
        osg::Vec3f mForce;
        float mFrameSeconds;
        float mForceW = 0.f;
    };

    struct RagdollNativeBlendUpdate
    {
        std::uint32_t mRecord;
        osg::Matrixf mAnimatedWorld;
        float mHierarchyGain;
        float mVelocityGain;
        std::uint16_t mCollisionFlags;
    };

    struct RagdollNativeBlendPublication
    {
        std::uint32_t mRecord;
        std::uint16_t mCollisionFlags;
        // Absent for scene-to-physics or suppressed mixed scene publication.
        std::optional<osg::Matrixf> mSceneTarget;
    };

    struct RagdollNativeBlendControllerState
    {
        std::uint32_t mRecord;
        std::optional<std::uint32_t> mTargetNode;
        ESM4::PhysicalBlendControllerState mState;
        std::uint32_t mAttachedNode = 0;
    };

    struct RagdollNativeVelocityControllerState
    {
        // Generated controllers have no authored NIF record identity.
        std::uint32_t mAttachedNode;
        std::optional<std::uint32_t> mTargetNode;
        ESM4::PhysicalVelocityControllerState mState;
        bool mPrecedesBlend = true;
    };

    struct RagdollNativeVelocitySetupRequest
    {
        std::uint32_t mNodeRecord;
        std::array<float, 4> mSourceVector;
        float mDuration;
    };

    struct RagdollNativeHitVelocitySetupRequest
    {
        std::uint32_t mNodeRecord;
        std::array<float, 4> mSourceVector;
        float mResolvedMassMultiplier;
    };

    enum class RagdollNativeControllerKind { Blend, Velocity };
    struct RagdollNativeControllerReference
    {
        RagdollNativeControllerKind mKind;
        // Blend uses its authored record; velocity uses its attachment node.
        std::uint32_t mIdentity;
        friend bool operator==(const RagdollNativeControllerReference&,
            const RagdollNativeControllerReference&) = default;
    };

    struct RagdollNativeKnockdownBlendRequest
    {
        std::uint32_t mNodeRecord;
        float mDuration;
    };

    // Caller-resolved DEFAULT settings from BlendSettings.ini; no cached PE defaults.
    struct RagdollNativePassOutSettings
    {
        float mForce;
        float mTime;
    };

    struct RagdollNativeKnockdownControllerSetupRequest
    {
        std::uint32_t mNodeRecord;
        osg::Vec3f mWorldVector;
        float mDuration;
    };

    enum class RagdollNativeKnockdownBlendDisposition
    {
        Started, MissingBlend, MissingController, Disabled,
    };

    struct RagdollNativeHitBlendSetupRequest
    {
        std::uint32_t mNodeRecord;
        ESM4::PhysicalBlendGains mConfiguredGains;
    };

    enum class RagdollNativeHitBlendDisposition
    {
        Started, MissingBlend, MissingController, StrongerSetup,
    };

    struct RagdollNativeBlendControllerTarget
    {
        std::uint32_t mControllerRecord;
        osg::Matrixf mAnimatedWorld;
    };

    struct RagdollNativeBlendState
    {
        std::uint32_t mBodyRecord;
        std::uint16_t mCollisionFlags;
        ESM4::PhysicalBlendGains mGains;
        // Original collision+1C, independent of actual body motion.
        std::uint32_t mRequestedMotion = 8;
    };

    // Owns collision shapes, rigid bodies and constraints. The borrowed world
    // must outlive the instance for ordinary Bullet worlds. Native worlds use
    // lifetime identities; retained graphs safely discard after World destruction.
    // Destruction removes every registration while its World remains alive.
    // Poses and velocities use the caller's world units. Native shape/inertia
    // lengths are converted once with the supplied positive length scale.
    // Optional internal filtering consumes caller-resolved group/masks during
    // construction only; it does not assign actor groups or filter other owners.
    class ActorRagdollPhysics
    {
    public:
        enum class Publication { Immediate, Deferred };

        ActorRagdollPhysics(const ActorRagdollDefinition& definition, btDynamicsWorld& world,
            float lengthScale, std::span<const btTransform> bodyPoses, int collisionGroup, int collisionMask,
            void* userPointer = nullptr, const RagdollInternalCollisionFilter* internalFilter = nullptr,
            Publication publication = Publication::Immediate);
        ~ActorRagdollPhysics();
        ActorRagdollPhysics(const ActorRagdollPhysics&) = delete;
        ActorRagdollPhysics& operator=(const ActorRagdollPhysics&) = delete;

        // Deferred construction requires NativeDynamicsWorld and creates no
        // broadphase bodies, constraints or native motion registration.
        // Explicit publication is once-only; foreign/replaced World identities
        // reject. Any throwing admission removes every partial registration.
        // Caller publishes its logical owner only after this succeeds.
        bool publish(btDynamicsWorld& world);
        bool isPublished() const noexcept;

        // Borrowed identities for engine collision routing; ownership stays here.
        std::span<btCollisionObject* const> collisionObjects() const;
        std::vector<RagdollBodyState> capture() const;
        void restore(std::span<const RagdollBodyState> states);
        // Complete ordered pose and native packed velocity snapshot. Native
        // velocities are authoritative over the spatial projection. Validate
        // both snapshots before mutation; retain restore's force/activation
        // and interpolation behavior. Motion modes are not changed.
        void restore(std::span<const RagdollBodyState> states,
            std::span<const RagdollNativePackedVelocityState> packedVelocities);
        // Complete ordered logical motion modes, pose and native velocities.
        // Stage all validation before handoffs/publication; retain object,
        // shape and constraint identities. Does not restore activation/contacts.
        void restore(std::span<const RagdollBodyState> states,
            std::span<const RagdollNativePackedVelocityState> packedVelocities,
            std::span<const RagdollNativeMotionRequest> motions);
        // Include the complete blend-target snapshot, independent of actual
        // modes. Stage raw flags/request/gains before any physical publication.
        void restore(std::span<const RagdollBodyState> states,
            std::span<const RagdollNativePackedVelocityState> packedVelocities,
            std::span<const RagdollNativeMotionRequest> motions,
            std::span<const RagdollNativeBlendState> blends);
        // Complete physical and owned controller state; shared clock caches
        // remain with the runtime authority that supplies each controller phase.
        void restore(std::span<const RagdollBodyState> states,
            std::span<const RagdollNativePackedVelocityState> packedVelocities,
            std::span<const RagdollNativeMotionRequest> motions,
            std::span<const RagdollNativeBlendState> blends,
            std::span<const RagdollNativeBlendControllerState> blendControllers,
            std::span<const RagdollNativeVelocityControllerState> velocityControllers);
        class PreparedRestore
        {
        public:
            ~PreparedRestore();
            PreparedRestore(const PreparedRestore&) = delete;
            PreparedRestore& operator=(const PreparedRestore&) = delete;
        private:
            friend class ActorRagdollPhysics;
            struct Data;
            explicit PreparedRestore(std::unique_ptr<Data> data);
            std::unique_ptr<Data> mData;
        };
        // Caller retains the original owner and synchronization barrier until
        // commit. Stage complete owned buffers without publishing body state.
        // Legacy projections retain absent metadata instead of inventing it.
        std::unique_ptr<PreparedRestore> prepareRestore(std::span<const RagdollBodyState> states) const;
        std::unique_ptr<PreparedRestore> prepareRestore(std::span<const RagdollBodyState> states,
            std::span<const RagdollNativePackedVelocityState> packedVelocities) const;
        std::unique_ptr<PreparedRestore> prepareRestore(std::span<const RagdollBodyState> states,
            std::span<const RagdollNativePackedVelocityState> packedVelocities,
            std::span<const RagdollNativeMotionRequest> motions) const;
        std::unique_ptr<PreparedRestore> prepareRestore(std::span<const RagdollBodyState> states,
            std::span<const RagdollNativePackedVelocityState> packedVelocities,
            std::span<const RagdollNativeMotionRequest> motions,
            std::span<const RagdollNativeBlendState> blends) const;
        std::unique_ptr<PreparedRestore> prepareRestore(std::span<const RagdollBodyState> states,
            std::span<const RagdollNativePackedVelocityState> packedVelocities,
            std::span<const RagdollNativeMotionRequest> motions,
            std::span<const RagdollNativeBlendState> blends,
            std::span<const RagdollNativeBlendControllerState> blendControllers,
            std::span<const RagdollNativeVelocityControllerState> velocityControllers) const;
        // Original owner only; successful publication consumes the token.
        void commitRestore(PreparedRestore& prepared);
        // Native units, including both packed fourth lanes. XYZ comes from the
        // current body; fourth lanes have separate owned binary32 storage.
        std::vector<RagdollNativePackedVelocityState> captureNativePackedVelocities() const;
        // Complete ordered snapshot, staged before all writes. Preserve pose,
        // forces, mode and activation; this is velocity state restoration only.
        void restoreNativePackedVelocities(std::span<const RagdollNativePackedVelocityState> states);
        // Sparse8A3900 publication from explicitly resolved prepared snapshots.
        // Prepare the entire batch, invoke the optional atomic caller hook, then
        // publish all eight lanes and wake groups for actual writes only. A
        // throwing hook leaves this owner unchanged; no owner mutation/reentry
        // or retained spans are permitted inside the hook. Return written IDs.
        // No inferred World authority, direct pose setter or motion step.
        std::vector<std::uint32_t> synchronizeNativeWorldScenes(
            std::span<const RagdollNativeWorldSceneRequest> requests,
            const std::function<void(std::span<const std::uint32_t>)>& beforePublish = {});
        // Requested blend-controller motion, not a serialized body snapshot.
        // Dynamic restores the original mass/principal inertia while retaining
        // the current pose and velocities, as the native motion archive does.
        // Caller owns scene synchronization and any transition velocity reset.
        std::vector<RagdollNativeMotionRequest> captureNativeMotionModes() const;
        void setNativeMotionModes(std::span<const RagdollNativeMotionRequest> requests);
        // Sparse keyframed scene publication using native scene target
        // preparation. Stage the whole batch before changing any body. Preserve
        // velocities/forces and refresh current/previous poses and AABBs.
        void synchronizeNativeKeyframedPoses(std::span<const RagdollNativeScenePoseRequest> poses);
        // Stage an entire sparse controller batch before publishing modes,
        // scene synchronization or velocities. Return renderer targets and
        // updated native collision flags; caller owns node writes and clock.
        // No Bullet step, actor root resolution or lifecycle transition.
        std::vector<RagdollNativeBlendPublication> updateNativeBlends(
            std::span<const RagdollNativeBlendUpdate> updates, float preparedFrameSeconds,
            std::uint32_t rawUpdateSelector, float nativeGravityZ);
        // Normal selected-controller setup only. Duration is resolved by the
        // caller from the native body filter. Stage the complete request batch;
        // read attachment-node gains independently of controller target identity.
        // Synchronize stored requested motion before missing/disabled handling.
        // Generated velocity setup, immediate branches, recursive traversal and
        // shared frame clocks remain separate.
        std::vector<RagdollNativeKnockdownBlendDisposition> prepareNativeKnockdownBlends(
            std::span<const RagdollNativeKnockdownBlendRequest> requests);
        // Atomic owned setup for already admitted body nodes and Down durations.
        // Read current damping and archived dynamic mass, create at the head or
        // retain existing target/list position. No body force, wake or pose write.
        void prepareNativeVelocityControllers(std::span<const RagdollNativeVelocitySetupRequest> requests);
        // Original HIT setter: compiled duration, caller-resolved multiplier,
        // archived mass/current damping. Replace existing vector/timing while
        // retaining target, elapsed/delta and list position; stage whole batch.
        void prepareNativeHitVelocityControllers(std::span<const RagdollNativeHitVelocitySetupRequest> requests);
        std::vector<RagdollNativeVelocityControllerState> captureNativeVelocityControllers() const;
        // Replace generated controller state atomically, with owned identity,
        // finite clock/vector and ordered timing admission. No body mutation.
        void restoreNativeVelocityControllers(std::span<const RagdollNativeVelocityControllerState> controllers);
        // Selected physical controllers in caller-supplied node traversal order.
        // This excludes unrelated renderer controllers and does not infer scene
        // traversal from the body graph. Fresh velocity controllers prepend.
        std::vector<RagdollNativeControllerReference> captureNativeControllerOrder(
            std::span<const std::uint32_t> nodeOrder) const;
        // Selected physical-controller phase in caller-supplied traversal order.
        // Stage all clocks, gains, velocity removals and immediate linear forces
        // together. This phase does not publish collision/scene poses or infer
        // recursive scene traversal; those remain separate caller boundaries.
        void advanceNativePhysicalControllers(std::span<const RagdollNativeControllerReference> controllerOrder,
            float inputTime, ESM4::PhysicalBlendTimeCache& sharedTimeCache);
        // Original normal8AB440 selected blend-controller/velocity setup.
        // Caller resolves per-body Down duration and stored requested-motion
        // synchronization first. A new velocity uses the resolved pass-out duration
        // and native source-vector preparation; an existing one is untouched.
        // Missing/disabled bodies skip unused inputs. Stage the complete batch.
        // Immediate/nonblend/recursive entry, forces and World remain separate.
        std::vector<RagdollNativeKnockdownBlendDisposition> prepareNativeKnockdownControllerSetup(
            std::span<const RagdollNativeKnockdownControllerSetupRequest> requests,
            RagdollNativePassOutSettings settings);
        // Original selected HIT blend leaf: read attachment-node gains,
        // retain independent controller target and stage the entire batch.
        // No requested-motion synchronization, body writes or wake.
        std::vector<RagdollNativeHitBlendDisposition> prepareNativeHitBlends(
            std::span<const RagdollNativeHitBlendSetupRequest> requests);
        // Stage both selected HIT controller kinds as a single reaction.
        // A late failure in either batch must leave both owned lists unchanged.
        std::vector<RagdollNativeHitBlendDisposition> prepareNativeHitControllerSetup(
            std::span<const RagdollNativeHitBlendSetupRequest> blends,
            std::span<const RagdollNativeHitVelocitySetupRequest> velocities);
        std::vector<RagdollNativeBlendControllerState> captureNativeBlendControllers() const;
        std::vector<RagdollNativeBlendState> captureNativeBlendStates() const;
        // Complete collision metadata snapshot; no body/controller conversion.
        void restoreNativeBlendStates(std::span<const RagdollNativeBlendState> states);

        // Own authored controllers and current target gains. Resolve each
        // controller's target node, stage clocks/gains and the complete physical
        // batch, then commit together. The caller owns the shared clock cache
        // across actors and supplies target animation poses in request order.
        // Velocity controllers, World lifecycle and persistence are not owned
        // by this interface; direct updateNativeBlends remains an explicit bridge.
        std::vector<RagdollNativeBlendPublication> updateNativeBlendControllers(
            std::span<const RagdollNativeBlendControllerTarget> targets, float inputTime,
            ESM4::PhysicalBlendTimeCache& sharedTimeCache, float preparedFrameSeconds,
            std::uint32_t rawUpdateSelector, float nativeGravityZ);
        // Complete renderer frame, keyed by owned body nodes. The caller
        // supplies each owned controller once in its traversal order. Advance
        // controllers before publishing every blend body, including bodies
        // without controllers. Preserve supplied bone order for publication.
        // Once all native computations and owned metadata are staged, invoke
        // optional atomic scene publication before changing bodies/clocks/cache.
        // A throwing callback leaves this owner unchanged. The callback must
        // not mutate this owner, reenter scheduler operations, or retain the
        // borrowed publication span.
        std::vector<RagdollNativeBlendPublication> updateNativeBlendFrame(
            std::span<const RagdollBoneWorldPose> bones, std::span<const std::uint32_t> controllerOrder,
            float inputTime, ESM4::PhysicalBlendTimeCache& sharedTimeCache, float preparedFrameSeconds,
            std::uint32_t rawUpdateSelector, float nativeGravityZ,
            const std::function<void(std::span<const RagdollNativeBlendPublication>)>& publishScene = {});
        // Immediate native linear force updates, staged as a complete sparse
        // batch. Resolve inverse mass from owned dynamic mass; keyframed motion
        // ignores unused force/time. Wake owned constraint-connected groups
        // only after validation. No Bullet force accumulation or World step.
        void applyNativeForces(std::span<const RagdollNativeForceRequest> requests);
        void applyImpulse(std::size_t body, const btVector3& impulse, const btVector3& worldPoint);
        // Original sphere-motion velocity damping; caller owns gravity/force
        // composition and the serial step boundary. Bullet damping stays zero.
        void applyNativeDamping(float frameSeconds);
        // Sparse explicit native pose drives, keyed by owned body record.
        // Stage all results before publishing velocities and waking selected
        // bodies and constraint-connected owned bones. Caller decides which
        // controllers require a drive; an empty
        // selection leaves sleeping bodies alone. No transform/world step.
        void driveNativePoseVelocities(std::span<const RagdollNativeVelocityDrive> drives,
            float inverseFrameSeconds, float nativeGravityZ);
        // Explicit native-unit per-body velocity deltas, before damping and
        // loaded-body caps. Stages every result before publishing active-body velocities;
        // does not apply Bullet forces or advance the world/transform clock.
        void applyNativeVelocityStep(float frameSeconds, std::span<const osg::Vec3f> nativeLinearDeltas);
        // Complete ordered native delta vectors. Skip KEY/sleeping unused
        // deltas; stage every active Dynamic result before velocity publication.
        // Preserve pose, mode, forces, contacts and activation.
        void applyNativePackedVelocityStep(float frameSeconds,
            std::span<const std::array<float, 4>> nativeLinearDeltas);
        // Advance active owned keyframed bodies in native units. Stage the
        // entire batch before publishing transforms/velocities/AABBs. The
        // NativeDynamicsWorld invokes this once per actual Bullet substep.
        // An ordinary borrowed Bullet world requires an explicit caller.
        void stepNativeKeyframedMotion(float frameSeconds);

    private:
        void validateRestore(std::span<const RagdollBodyState> states) const;
        void validateNativeBlendStates(std::span<const RagdollNativeBlendState> states) const;
        std::vector<RagdollNativeBlendControllerState> prepareNativeBlendControllerRestore(
            std::span<const RagdollNativeBlendControllerState> controllers) const;
        std::vector<RagdollNativeVelocityControllerState> prepareNativeVelocityControllerRestore(
            std::span<const RagdollNativeVelocityControllerState> controllers) const;
        void publishRestore(std::span<const RagdollBodyState> states);
        std::vector<RagdollBodyState> preparePackedRestore(std::span<const RagdollBodyState> states,
            std::span<const RagdollNativePackedVelocityState> packedVelocities) const;
        void publishPackedRestore(std::span<const RagdollBodyState> states,
            std::span<const RagdollNativePackedVelocityState> packedVelocities);
        std::vector<RagdollNativeBlendPublication> updateNativeBlendsImpl(
            std::span<const RagdollNativeBlendUpdate> updates, float preparedFrameSeconds,
            std::uint32_t rawUpdateSelector, float nativeGravityZ,
            const std::function<void(std::span<const RagdollNativeBlendPublication>)>& publishScene);
        std::vector<RagdollNativeBlendPublication> updateNativeBlendControllersImpl(
            std::span<const RagdollNativeBlendControllerTarget> targets,
            std::span<const RagdollBoneWorldPose> completeBones, float inputTime,
            ESM4::PhysicalBlendTimeCache& sharedTimeCache, float preparedFrameSeconds,
            std::uint32_t rawUpdateSelector, float nativeGravityZ,
            const std::function<void(std::span<const RagdollNativeBlendPublication>)>& publishScene);
        std::vector<RagdollNativeKnockdownBlendDisposition> prepareNativeKnockdownControllerSetupImpl(
            std::span<const RagdollNativeKnockdownControllerSetupRequest> requests, bool includeVelocity,
            RagdollNativePassOutSettings settings);
        void applyNativeForcesImpl(std::span<const RagdollNativeForceRequest> requests, bool allowRepeatedBodies);
        std::pair<std::vector<RagdollNativeBlendControllerState>, std::vector<RagdollNativeHitBlendDisposition>>
            prepareNativeHitBlendsImpl(std::span<const RagdollNativeHitBlendSetupRequest> requests) const;
        std::vector<RagdollNativeVelocityControllerState> prepareNativeHitVelocityControllersImpl(
            std::span<const RagdollNativeHitVelocitySetupRequest> requests) const;
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}

#endif
