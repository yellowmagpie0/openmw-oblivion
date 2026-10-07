#ifndef OPENMW_COMPONENTS_ESM4_RUNTIMESTATE_H
#define OPENMW_COMPONENTS_ESM4_RUNTIMESTATE_H

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formkey.hpp>
#include <components/esm/gameprofile.hpp>
#include <components/esm/position.hpp>

#include "aiphase.hpp"
#include "actionledger.hpp"
#include "crimecontracts.hpp"
#include "actorvalues.hpp"
#include "physicalcombat.hpp"
#include "projectilerules.hpp"
#include "physicalblendsettings.hpp"
#include "physicalvelocitycontroller.hpp"
#include "physicalsceneworld.hpp"

namespace ESM
{
    class ESMReader;
    class ESMWriter;
}

namespace ESM4
{
    // v45 retains v44's payload layout and requires owned shared bounty views
    // to agree at whole-save admission. Earlier v44 saves may have stale views.
    inline constexpr std::uint32_t CurrentRuntimeStateVersion = 45;

    struct RuntimeContentIdentity
    {
        std::string mPlugin;
        std::string mFingerprint;

        friend bool operator==(const RuntimeContentIdentity&, const RuntimeContentIdentity&) = default;
    };

    using RuntimeValue = std::variant<bool, std::int64_t, double, std::string>;

    struct RuntimeInventoryItem
    {
        ESM::FormKey mBase;
        std::int32_t mCount = 0;
        // -1 identifies an item category without condition/charge. A value of
        // zero is a valid broken or discharged item.
        double mCondition = -1; // Envelope: legacy int32 exact; version24 stores native binary32.
        float mCharge = -1.f;
        // TES4 biped bits occupy the low 16 bits. M13 reserves the next three
        // bits for the weapon, ammunition, and portable-light pseudo-slots.
        std::uint32_t mEquippedSlots = 0;
        // Oblivion exposes eight quick keys. -1 means that no key is bound.
        std::int8_t mHotkey = -1;
        // A non-null owner survives theft/transfer until the item is sold to
        // a legitimate merchant. M15 consumes this identity for crime.
        ESM::FormKey mOwner;
        // Portable lights keep a fractional burn duration independently from
        // durability and enchantment charge. -1 selects the base duration.
        float mRemainingUsageTime = -1.f;
        // v41: native ownership extras of this exact inventory instance.
        std::optional<std::int32_t> mOwnershipRank = std::nullopt;
        ESM::FormKey mOwnershipGlobal = {};

        friend bool operator==(const RuntimeInventoryItem&, const RuntimeInventoryItem&) = default;
    };

    // Logical equipment presentation, not native animation-group numbers.
    enum class ActorDrawState : std::uint8_t { Nothing, Weapon, Spell };

    struct RuntimeReferenceState
    {
        ESM::FormKey mKey;
        ESM::FormKey mBase;
        ESM::FormKey mCell;
        bool mEnabled = true;
        bool mDeleted = false;
        ESM::Position mPosition{};
        std::optional<ESM::FormKey> mOwner;
        std::int32_t mLockLevel = 0;
        std::vector<RuntimeInventoryItem> mInventory;
        std::map<std::string, RuntimeValue, std::less<>> mCustomState;
        // v29: absent in old saves, rather than inferred from combat or AI.
        // The native non-Player actor owns this view independently of input.
        std::optional<ActorDrawState> mActorDrawState = std::nullopt;
        // v40: exact extras of a loose native item instance. Absent extras
        // select the winning base defaults; zero remains broken/discharged.
        std::optional<float> mItemCondition;
        std::optional<float> mItemCharge;
        // v41: absent rank is distinct from every signed explicit rank.
        std::optional<std::int32_t> mOwnershipRank = std::nullopt;
        ESM::FormKey mOwnershipGlobal = {};

        friend bool operator==(const RuntimeReferenceState&, const RuntimeReferenceState&) = default;
    };

    struct RuntimeClockState
    {
        std::int32_t mYear = 1;
        std::int32_t mMonth = 0;
        std::int32_t mDay = 1;
        double mHour = 0;
        double mTimeScale = 30;

        friend bool operator==(const RuntimeClockState&, const RuntimeClockState&) = default;
    };

    struct RuntimePlayerState
    {
        ESM::FormKey mReference;
        ESM::FormKey mCell;
        ESM::Position mPosition{};
        std::map<std::string, double, std::less<>> mActorValues;
        std::vector<RuntimeInventoryItem> mInventory;
        std::string mName;
        ESM::FormKey mRace;
        ESM::FormKey mClass;
        ESM::FormKey mBirthSign;
        bool mFemale = false;
        std::uint8_t mCharacterGenerationFlags = 0;

        friend bool operator==(const RuntimePlayerState&, const RuntimePlayerState&) = default;
    };

    // Script locals use a distinct value type so reference variables remain
    // distinguishable from strings across a save/reload boundary.
    using RuntimeScriptValue = std::variant<std::monostate, std::int64_t, double, std::string, ESM::FormKey>;

    struct RuntimeScriptInstance
    {
        std::string mUnit;
        ESM::FormKey mContext;
        std::vector<RuntimeScriptValue> mLocals;
        bool mOnLoadFired = false;

        friend bool operator==(const RuntimeScriptInstance&, const RuntimeScriptInstance&) = default;
    };

    struct RuntimeQuestState
    {
        ESM::FormKey mQuest;
        std::int32_t mStage = 0;
        bool mRunning = false;
        std::vector<std::int32_t> mCompletedStages;

        friend bool operator==(const RuntimeQuestState&, const RuntimeQuestState&) = default;
    };

    struct RuntimeActorAiState
    {
        ESM::FormKey mActor;
        ESM::FormKey mBase;
        ESM::FormKey mPackage;
        // Persist the transient script-package slot independently from the
        // currently selected package.
        ESM::FormKey mScriptPackage;
        ESM::FormKey mTarget;
        ESM::FormKey mTargetBase;
        ESM::FormKey mCell;
        ESM::FormKey mPathgrid;
        ESM::FormKey mDoor;
        ESM::FormKey mDestinationCell;
        ESM::Position mDestinationPosition{};
        ESM::FormKey mLastValidCell;
        ESM::Position mLastValidPosition{};
        ESM::FormKey mActionItem;
        ESM::FormKey mLastTransitionDoor;
        ESM::FormKey mCompanionGroup;
        ESM::FormKey mCompanionSideWith;
        ESM::FormKey mMount;
        ESM::FormKey mRider;
        std::optional<ScheduleWindow> mScheduleWindow;
        ConditionResult mConditionResult = ConditionResult::False;
        PackageSource mSource = PackageSource::None;
        AIPackageType mPackageType = AIPackageType::Unknown;
        PackageProcedure mProcedure = PackageProcedure::None;
        PackagePhase mPhase = PackagePhase::Select;
        ProcessTier mTier = ProcessTier::High;
        PhaseBoundary mBoundary = PhaseBoundary::None;
        std::uint32_t mListIndex = 0;
        std::uint32_t mPathNode = 0;
        std::uint32_t mRepathAttempts = 0;
        std::int32_t mFormationIndex = -1;
        std::uint64_t mSelectionGeneration = 0;
        std::uint64_t mRouteGeneration = 0;
        std::uint64_t mTransitionGeneration = 0;
        float mActionTimer = 0.0f;
        float mDurationRemaining = 0.0f;
        float mNoProgressSeconds = 0.0f;
        float mDoorCooldown = 0.0f;
        float mLowProcessTimer = 0.0f;
        // Countdown to the next abstract low-process update.  The route
        // progress timer above and this cadence timer are intentionally
        // separate: one measures simulated travel, the other prevents a
        // loaded actor from being processed once per render frame.
        float mNextLowProcessTick = 0.0f;
        bool mRestrained = false;
        bool mActionReserved = false;
        bool mHasDestination = false;
        bool mDoorAnimationStarted = false;
        std::string mInterruptionReason;

        friend bool operator==(const RuntimeActorAiState&, const RuntimeActorAiState&) = default;
    };

    struct RuntimePathPointState
    {
        ESM::FormKey mPathgrid;
        std::uint32_t mNode = 0;
        bool mEnabled = true;

        friend bool operator==(const RuntimePathPointState&, const RuntimePathPointState&) = default;
    };

    struct RuntimeCompanionRelation
    {
        ESM::FormKey mLeader;
        ESM::FormKey mMember;
        ESM::FormKey mGroup;
        ESM::FormKey mSideWith;
        std::int32_t mFormationIndex = -1;

        friend bool operator==(const RuntimeCompanionRelation&, const RuntimeCompanionRelation&) = default;
    };

    struct RuntimeMountRelation
    {
        ESM::FormKey mHorse;
        ESM::FormKey mRider;
        ESM::FormKey mOwner;
        ESM::FormKey mLastRidden;
        bool mMounted = false;

        friend bool operator==(const RuntimeMountRelation&, const RuntimeMountRelation&) = default;
    };

    struct RuntimeDetectionVector
    {
        ESM::FormKey mObserver;
        ESM::FormKey mTarget;
        double mScore = 0.0;
        bool mDetected = false;
        bool mLineOfSight = false;

        friend bool operator==(const RuntimeDetectionVector&, const RuntimeDetectionVector&) = default;
    };

    struct RuntimePackageDoneEvent
    {
        ESM::FormKey mActor;
        ESM::FormKey mPackage;

        friend bool operator==(const RuntimePackageDoneEvent&, const RuntimePackageDoneEvent&) = default;
    };

    struct RuntimePassiveValueModifier
    {
        std::uint32_t mEffectIndex = 0;
        std::uint32_t mCode = 0;
        std::uint32_t mActorValue = 0;
        // Post-sign/clamp magnitude actually dispatched during application.
        // Removal uses this saved value, never a re-resolved winning EFIT.
        float mStoredMagnitude = 0;
        // v19: quantity before detrimental sign/clamp and base application.
        // Native list insertion compares this to older applied quantities.
        // Absence on older snapshots is unknown, never an inferred magnitude.
        std::optional<float> mInitialMagnitude = std::nullopt;
        friend bool operator==(const RuntimePassiveValueModifier&, const RuntimePassiveValueModifier&) = default;
    };

    struct RuntimePassiveAbility
    {
        ESM::FormKey mSpell;
        // Preserve application order; do not sort effects for serialization.
        std::vector<RuntimePassiveValueModifier> mEffects;
        void validate() const;
        friend bool operator==(const RuntimePassiveAbility&, const RuntimePassiveAbility&) = default;
    };

    struct RuntimeActorValues
    {
        ESM::FormKey mActor;
        ESM::FormKey mBase;
        ActorValueOwner mOwner = ActorValueOwner::NonPlayer;
        ActorValueProcess mProcess = ActorValueProcess::Active;
        // TES4 AV0..71. Preserve each modifier's presence independently from
        // its value. Bases are resolved native values, never shared UI views.
        std::array<ActorValueState, 72> mValues{};
        // v44: reference-owned crime gold; AV37 has no base-form field.
        // Absence preserves legacy scalar storage; readers never infer a realm.
        std::optional<CrimeBountyState> mBounty;
        bool mPlayerInShiveringIsles = false;
        // v10: raw integer base-form contributions for player AV8..11.
        // Never infer these from the resolved dynamic bases above. Absence
        // denotes legacy/uninitialized player authority, not four zeroes.
        std::optional<std::array<std::int32_t, 4>> mPlayerFormValues;
        // v17: resolved signed Health form input before the first float store.
        // Older snapshots retain their float-only interpretation; do not infer
        // a lost integer from a rounded legacy base or current shared record.
        std::optional<std::int32_t> mNonPlayerFormHealth;
        // v18: narrow self ability ownership, including applied magnitudes.
        // Null means unknown legacy ownership. An empty vector means known
        // no applied abilities; neither state permits guessing from AV bases.
        std::optional<std::vector<RuntimePassiveAbility>> mPassiveAbilities;

        // v25: signed native High/MiddleHigh process knocked byte. Absence
        // retains unknown legacy Active-process state; Low queries return zero.
        std::optional<std::int8_t> mProcessKnockedState;
        // v26: signed High-process action code. Unknown legacy state remains
        // absent. Low queries return -1; action6 is the native blocking posture.
        std::optional<std::int16_t> mProcessAction;

        void validate() const;
        friend bool operator==(const RuntimeActorValues&, const RuntimeActorValues&) = default;
    };

    struct RuntimeActorBaseOverride
    {
        ESM::FormKey mBase;
        ActorBaseKind mKind = ActorBaseKind::Npc;
        std::vector<ActorBaseValueSet> mValues;

        void validate() const;
        friend bool operator==(const RuntimeActorBaseOverride&, const RuntimeActorBaseOverride&) = default;
    };

    // Versioned, load-order-independent state owned by the Oblivion profile.
    // Logical lifecycle phases, not the original executable's animation-state
    // numbers. A terminal actor is dead while its death animation may continue.
    enum class ActorLifePhase : std::uint8_t { Alive, Dead, EssentialUnconscious };

    // Shape-world transforms and velocities in game world units. Record IDs
    // belong to the identified NIF asset, not the content load order.
    // Logical owned-body mode, not a raw Havok motion class/getter type.
    enum class RuntimeRagdollMotion : std::uint8_t { Dynamic = 1, Keyframed = 6 };

    struct RuntimeRagdollBody
    {
        std::uint32_t mRecord = 0;
        std::uint32_t mNodeRecord = 0;
        std::array<float, 9> mRotation{1, 0, 0, 0, 1, 0, 0, 0, 1};
        std::array<float, 3> mPosition{};
        std::array<float, 3> mLinearVelocity{};
        std::array<float, 3> mAngularVelocity{};
        // v32: exact native binary32 velocity lanes, separate from the world
        // spatial projection. Legacy snapshots leave this absent.
        std::optional<PhysicalWorldSceneVelocities> mNativePackedVelocity;
        // v33: legacy snapshots do not invent an original motion mode.
        std::optional<RuntimeRagdollMotion> mNativeMotion = std::nullopt;
        friend bool operator==(const RuntimeRagdollBody&, const RuntimeRagdollBody&) = default;
    };

    // v34: stored requests are independent of the current logical body mode.
    struct RuntimeRagdollBlendState
    {
        std::uint32_t mBodyRecord = 0;
        std::uint16_t mCollisionFlags = 0;
        std::uint32_t mRequestedMotion = 0;
        float mHierarchyGain = 0;
        float mVelocityGain = 0;
        friend bool operator==(const RuntimeRagdollBlendState&, const RuntimeRagdollBlendState&) = default;
    };

    struct RuntimeRagdollBlendController
    {
        std::uint32_t mRecord = 0;
        std::uint32_t mAttachedNode = 0;
        std::optional<std::uint32_t> mTargetNode;
        PhysicalBlendControllerState mState;
        friend bool operator==(const RuntimeRagdollBlendController&, const RuntimeRagdollBlendController&) = default;
    };

    struct RuntimeRagdollVelocityController
    {
        std::uint32_t mAttachedNode = 0;
        std::optional<std::uint32_t> mTargetNode;
        bool mPrecedesBlend = true;
        PhysicalVelocityControllerState mState;
        friend bool operator==(const RuntimeRagdollVelocityController&, const RuntimeRagdollVelocityController&) = default;
    };

    struct RuntimeRagdollControllers
    {
        // Canonical increasing authored-record and generated-attachment order.
        std::vector<RuntimeRagdollBlendController> mBlends;
        std::vector<RuntimeRagdollVelocityController> mVelocities;
        friend bool operator==(const RuntimeRagdollControllers&, const RuntimeRagdollControllers&) = default;
    };

    struct RuntimeActorRagdoll
    {
        ESM::FormKey mBase;
        std::string mModel;
        // Lowercase hexadecimal encoding of Nif::FileView's 16 opaque hash
        // bytes. This is the renderer's asset identity, not a SHA-256 digest.
        std::string mAssetHash;
        // Canonical increasing body-record order, independent of hierarchy.
        std::vector<RuntimeRagdollBody> mBodies;
        // Absent means a legacy snapshot; present empty means no blend targets.
        // Present entries are the complete target set in increasing body order.
        std::optional<std::vector<RuntimeRagdollBlendState>> mNativeBlends = std::nullopt;
        // v35: absent legacy data differs from current complete empty lists.
        // Shared clocks are runtime-authority state, not per-actor fields.
        std::optional<RuntimeRagdollControllers> mNativeControllers = std::nullopt;
        void validate() const;
        friend bool operator==(const RuntimeActorRagdoll&, const RuntimeActorRagdoll&) = default;
    };
    struct RuntimeActorLife
    {
        ESM::FormKey mActor;
        ESM::FormKey mBase;
        ActorLifePhase mPhase = ActorLifePhase::Alive;
        float mRecoveryRemaining = 0;
        ESM::FormKey mKiller;

        void validate() const;
        friend bool operator==(const RuntimeActorLife&, const RuntimeActorLife&) = default;
    };

    // Logical strike kinds, with stock native group selection owned by the
    // controller adapter. These values are save enums, not raw TES4 group IDs.
    enum class MeleeStrikeKind : std::uint8_t
    { Left, Right, StandingPower, ForwardPower, BackwardPower, LeftPower, RightPower };
    enum class MeleeQueuedStrike : std::uint8_t { None, Ordinary, Power };
    // Owned draw/attachment/release protocol. No renderer handles or projectile
    // pointers are serialized; release acknowledgment is distinct from spawning.
    struct RuntimeBowState
    {
        std::uint64_t mActionId = 0;
        ESM::FormKey mBowBase, mAmmoBase;
        std::string mAnimationGroup;
        float mPlaybackRate = 1;
        BowAnimationProgress mProgress;
        std::array<float, 5> mKeyTimes{};
        std::int16_t mAction = 4;
        bool mReleaseCommitted = false;
        // v39: accepted Player input latch, retained through held playback.
        bool mPlayerHoldLatched = false;
        void validate() const;
        friend bool operator==(const RuntimeBowState&, const RuntimeBowState&) = default;
    };

    struct RuntimeMeleeInput
    {
        float mHeldSeconds = 0;
        bool mInputHeld = false;
        bool mPreferLeft = true;
        MeleeQueuedStrike mQueued = MeleeQueuedStrike::None;
        void validate() const;
        friend bool operator==(const RuntimeMeleeInput&, const RuntimeMeleeInput&) = default;
    };
    struct RuntimeMeleeStrike
    {
        std::uint64_t mActionId = 0;
        MeleeStrikeKind mKind = MeleeStrikeKind::Left;
        ESM::FormKey mWeaponBase; // Null for hand-to-hand or a natural attack.
        std::string mAnimationGroup; // The selected variant, not just its strike kind.
        float mPlaybackSpeed = 1;
        float mAnimationTime = 0;
        bool mContactCommitted = false;
        // v22: authoritative ordinary phase; v21 migrates to Start without
        // inferring it from a renderer time or changing contact consumption.
        OrdinaryMeleePhase mOrdinaryPhase = OrdinaryMeleePhase::Start;
        std::optional<MeleeSequenceTiming> mSequenceTiming = std::nullopt;
        void validate() const;
        friend bool operator==(const RuntimeMeleeStrike&, const RuntimeMeleeStrike&) = default;
    };
    struct RuntimeMeleeAiIntent
    {
        ESM::FormKey mTarget;
        ESM::FormKey mStyle;
        void validate() const;
        friend bool operator==(const RuntimeMeleeAiIntent&, const RuntimeMeleeAiIntent&) = default;
    };
    struct RuntimeMeleeState
    {
        RuntimeMeleeInput mInput;
        std::optional<RuntimeMeleeStrike> mStrike;
        // v28: combat service owns the NPC input and its selected target.
        std::optional<RuntimeMeleeAiIntent> mAiIntent = std::nullopt;
        void validate() const;
        friend bool operator==(const RuntimeMeleeState&, const RuntimeMeleeState&) = default;
    };

    struct RuntimeActorDeathEvent
    {
        std::uint64_t mId = 0;
        ESM::FormKey mActor;
        ESM::FormKey mKiller;

        friend bool operator==(const RuntimeActorDeathEvent&, const RuntimeActorDeathEvent&) = default;
    };

    // The binary representation is private to OpenMW saves and deliberately
    // does not reuse raw load-order indices from Bethesda plugins.
    struct RuntimeState
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_T4ST;

        std::uint32_t mVersion = CurrentRuntimeStateVersion;
        ESM::GameProfile mProfile = ESM::GameProfile::Oblivion;
        std::uint64_t mNextDynamicSerial = 1;
        std::vector<RuntimeContentIdentity> mContent;
        RuntimeClockState mClock;
        RuntimePlayerState mPlayer;
        std::map<ESM::FormKey, RuntimeValue> mGlobals;
        std::vector<RuntimeReferenceState> mReferences;
        std::uint64_t mScriptEventSequence = 0;
        std::vector<RuntimeScriptInstance> mScriptInstances;
        std::vector<RuntimeQuestState> mQuests;
        std::uint64_t mAiRngState = 1;
        // v27: profile-owned combat stream, independent of AI/world scheduling.
        // Zero is a valid CRT seed. Older saves initialize this stream at one.
        std::uint32_t mCombatRngState = 1;
        std::vector<RuntimeActorAiState> mActorAi;
        std::vector<RuntimePathPointState> mPathPoints;
        std::vector<RuntimeCompanionRelation> mCompanions;
        std::vector<RuntimeMountRelation> mMounts;
        std::vector<RuntimeDetectionVector> mDetectionVectors;
        // FIFO, not sorted: callbacks can affect subsequent callbacks. A save
        // inside one callback must retain the rest without replaying that one.
        std::vector<RuntimePackageDoneEvent> mPendingPackageDone;
        // v8: M15 action identity ownership. Older versions start a new ledger;
        // script/AI event IDs belong to separate namespaces and are not reused.
        ActionLedgerState mPhysicalActions;
        // v20: pending physical intents bind to stable attacker identity. Older
        // anonymous IDs remain anonymous; no actor or contact is inferred.
        std::map<std::uint64_t, ESM::FormKey> mPhysicalActionOwners;
        // v21: input queue and animation/contact continuation, including the
        // follow-through of an already consumed strike. Older IDs do not imply
        // an animation; old snapshots deliberately start with no melee state.
        std::map<ESM::FormKey, RuntimeMeleeState> mNativeMeleeStates;
        // v23: AnimData clock survives strike cancellation/incapacitation.
        std::map<ESM::FormKey, float> mNativeAnimationClocks;
        // v30: timed native Havok pulse, independent of attack continuation.
        // Older saves start without inferred physical forces.
        std::map<ESM::FormKey, TimedKnockbackState> mNativeActorKnockback;
        // v31: persistent physical snapshots; old saves have no invented pose.
        std::map<ESM::FormKey, RuntimeActorRagdoll> mNativeActorRagdolls;
        // v36: scheduler-wide physical clock cache, independent of actor ownership.
        // Renderer-global Ni controller caching belongs to a different authority.
        std::optional<PhysicalBlendTimeCache> mNativePhysicalBlendTimeCache = std::nullopt;
        // v37: original Player bow timer, independent of the animation clock.
        // Absence in an older snapshot starts at zero, without inferring a draw.
        std::optional<float> mNativePlayerBowTimer = std::nullopt;
        // v38: owned bow playback, retained independently of loaded renderers.
        std::map<ESM::FormKey, RuntimeBowState> mNativeBowStates;
        // v9: native actor-value authority, including retained unloaded actors.
        std::vector<RuntimeActorValues> mNativeActorValues;
        // v11: shared base-record overrides, including bases with no loaded actors.
        std::vector<RuntimeActorBaseOverride> mNativeActorBases;

        // v12: native life authority and FIFO death callbacks. Remove a callback
        // before dispatch; a save in that callback retains only remaining work.
        std::vector<RuntimeActorLife> mNativeActorLife;
        std::uint64_t mNextDeathEvent = 1;
        std::vector<RuntimeActorDeathEvent> mPendingDeathEvents;
        // Original storage is a wrapping 16-bit per-base counter, queried as signed.
        std::map<ESM::FormKey, std::uint16_t> mNativeDeathCounts;

        // v14: native process breath timer, retained for unloaded actors too.
        // Missing older-save entries remain absent until native process adoption.
        std::map<ESM::FormKey, float> mNativeActorBreath;
        // v15: symmetric opponent membership, stored once per canonical pair.
        // Attack targets, phases and legal hit context are separate contracts.
        // Both endpoints require native values/life and may not be terminal.
        std::set<std::pair<ESM::FormKey, ESM::FormKey>> mNativeCombatEngagements;

        // v16: manager seconds and the last completed common update per actor.
        // An absent actor timestamp denotes an uninitialized clock, not time0.
        float mNativeActorManagerTime = 0;
        std::map<ESM::FormKey, float> mNativeActorUpdateTimes;
        // v42: validated crime/custody contracts; older saves invent no incidents.
        CrimeStateContracts mNativeCrime;

        void validate() const;
        std::vector<std::uint8_t> serializeBinary() const;
        static RuntimeState deserializeBinary(const std::vector<std::uint8_t>& data);

        void save(ESM::ESMWriter& writer) const;
        void load(ESM::ESMReader& reader);

        std::vector<std::string> getMissingContentFiles(const std::vector<std::string>& currentContent) const;
        void validateContent(const std::vector<RuntimeContentIdentity>& currentContent) const;
        std::string canonicalJson() const;

        friend bool operator==(const RuntimeState&, const RuntimeState&) = default;
    };
}

#endif
