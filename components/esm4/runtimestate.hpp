#ifndef OPENMW_COMPONENTS_ESM4_RUNTIMESTATE_H
#define OPENMW_COMPONENTS_ESM4_RUNTIMESTATE_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formkey.hpp>
#include <components/esm/gameprofile.hpp>
#include <components/esm/position.hpp>

#include "aiphase.hpp"

namespace ESM
{
    class ESMReader;
    class ESMWriter;
}

namespace ESM4
{
    inline constexpr std::uint32_t CurrentRuntimeStateVersion = 7;

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
        std::int32_t mCondition = -1;
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

        friend bool operator==(const RuntimeInventoryItem&, const RuntimeInventoryItem&) = default;
    };

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

    // Versioned, load-order-independent state owned by the Oblivion profile.
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
        std::vector<RuntimeActorAiState> mActorAi;
        std::vector<RuntimePathPointState> mPathPoints;
        std::vector<RuntimeCompanionRelation> mCompanions;
        std::vector<RuntimeMountRelation> mMounts;
        std::vector<RuntimeDetectionVector> mDetectionVectors;
        // FIFO, not sorted: callbacks can affect subsequent callbacks. A save
        // inside one callback must retain the rest without replaying that one.
        std::vector<RuntimePackageDoneEvent> mPendingPackageDone;

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
