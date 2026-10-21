#ifndef OPENMW_ESM4_PHYSICALCOMBAT_H
#define OPENMW_ESM4_PHYSICALCOMBAT_H

#include <cstdint>
#include <array>
#include <optional>
#include <span>
#include <cstddef>
#include <string_view>

namespace ESM4
{
    struct WeaponSwishInput
    {
        float mWeight;
        float mSpeed;
    };
    // Original Audio INI defaults, not GMST values. Both native policies
    // remain explicit inputs; importing a user's Oblivion.ini is separate.
    struct WeaponSwishSettings
    {
        bool mUseSpeed = true;
        float mMediumWeightMinimum = 8.f;
        float mLargeWeightMinimum = 25.f;
        float mMediumSpeedMaximum = 1.1f;
        float mLargeSpeedMaximum = .95f;
    };
    // Null weapon selects the unarmed SOUN without reading weapon/settings.
    // Equality at either threshold selects Large in both original policies.
    std::string_view nativeMeleeSwishSound(std::optional<WeaponSwishInput> weapon,
        const WeaponSwishSettings& settings = {});

    // Original6AFB48 hit palette for an NPC source and NPC target. Classes
    // are caller-resolved native arguments: negative means no material layer,
    // armor zero/light and positive/heavy; shield accepts only zero/one.
    // These flags describe the selector branches, not inferred actor states.
    struct NpcMeleeHitSoundInput
    {
        std::int32_t mWeaponType = -1;
        std::int32_t mArmorClass = -1;
        std::int32_t mShieldClass = -1;
        bool mSuppressBodySound = false;
        bool mEnchantedWeaponSound = false;
    };
    // Material, body and weapon layers in native lookup order; absent layers
    // are empty. Creature sources/targets require their separate native paths.
    std::array<std::string_view, 3> nativeNpcMeleeHitSounds(const NpcMeleeHitSoundInput& input);

    // Same dispatcher with an optional Creature victim. This is the victim
    // Actor byte104, not the TESCreature base's creatureType: exactly2 selects
    // Bone; other byte values select Fur. Empty selects the NPC Flesh branch.
    // Creature source custom Weapon9 selection remains a separate caller.
    std::array<std::string_view, 3> nativeMeleeHitSounds(
        const NpcMeleeHitSoundInput& input, std::optional<std::uint8_t> creatureActorByte);

    // Original6AFFF0 weapon/unarmed block palette in attacker, defender order.
    // The enchantment selector affects only the attacker layer.
    std::array<std::string_view, 2> nativeMeleeBlockSounds(std::int32_t attackerWeaponType,
        std::int32_t defenderWeaponType, bool enchantedWeaponSound = false);

    enum class CombatMastery { Novice, Apprentice, Journeyman, Expert, Master };
    struct CombatMasterySettings
    {
        std::array<std::int32_t, 4> mMinimumSkill;
    };
    struct MeleeInputSettings
    {
        float mPowerAttackDelay;
        CombatMasterySettings mMastery;
    };
    void validateMeleeInputSettings(const MeleeInputSettings& settings);
    // airborne is the caller-resolved native5EC180 predicate, not an inferred
    // collision or shared TES3 animation state.
    bool airborneMeleeStartAllowed(std::int32_t baseAcrobatics, bool airborne,
        const MeleeInputSettings& settings);
    bool heldPowerAttackAllowed(std::int32_t baseAcrobatics, bool swimming, bool airborne,
        const MeleeInputSettings& settings);
    // Original5EC180 reads the low-byte group in locomotion slot0 first,
    // then resolved character context state2. Missing objects stay missing.
    bool nativeAttackAirborne(std::optional<std::uint8_t> animationGroup,
        std::optional<std::uint32_t> characterState);

    struct MeleeTextKey
    {
        float mTime;
        std::string_view mText;
    };
    template <std::size_t Size> struct MeleeKeys
    {
        std::array<float, Size> mTimes{};
        std::uint8_t mMatchedCount = 0;
        friend bool operator==(const MeleeKeys&, const MeleeKeys&) = default;
    };
    using OrdinaryMeleeKeys = MeleeKeys<4>;
    using PowerMeleeKeys = MeleeKeys<3>;
    using ReactionKeys = MeleeKeys<2>;
    // Original51B688: consume Start/Hit/a:/End in authored order, rather
    // than collecting independently named keys or sorting a renderer map.
    // Missing/unwritten slots retain the original zero initialization.
    OrdinaryMeleeKeys ordinaryMeleeKeyTimes(std::span<const MeleeTextKey> textKeys);
    PowerMeleeKeys powerMeleeKeyTimes(std::span<const MeleeTextKey> textKeys);
    // Native subtype0/1 slots consume Start/End without an attack Hit key.
    ReactionKeys reactionKeyTimes(std::span<const MeleeTextKey> textKeys);
    // Original436DA0/51B490: internal name before its first underscore is
    // the gameplay group; the resource basename supplies movement/family.
    // This bounded parser recognizes BlockHit28, Recoil30 and Stagger31.
    std::optional<std::uint16_t> nativeReactionSequenceGroup(
        std::string_view sequenceName, std::string_view resourceBasename);
    // Slot0 locomotion names from the native table, excluding SpecialIdle2/slot4.
    std::optional<std::uint16_t> nativeLocomotionSequenceGroup(
        std::string_view sequenceName, std::string_view resourceBasename);
    struct NativeNpcBlockHitGroupInput
    {
        std::optional<std::uint8_t> mWeaponType;
        bool mWeaponDrawn115 = false;
        // The second native process byte is kept explicit: its full gameplay
        // meaning is not inferred from its use in this selector.
        bool mFlag114 = false;
        bool mShield = false;
        std::uint16_t mMovementFlags = 0;
    };
    // Original NPC5E5690, ordinary BlockHit request with no Player override
    // or forced-unarmed argument. Returns the packed pre-fallback group.
    std::uint16_t nativeNpcBlockHitGroup(const NativeNpcBlockHitGroupInput& input);
    // Original470D20 family/movement fallback for those three groups over
    // nonempty cached collections. Missing reactions return no selection;
    // the original's eventual Idle fallback is rejected by reaction callers.
    // Variant choice and its RNG draw belong to the selected collection.
    std::optional<std::uint16_t> resolveNativeReactionSequenceGroup(
        std::uint16_t requested, std::span<const std::uint16_t> available);
    // Original47DA70/47F7B0: sphere(x,y,z,r), planes(nx,ny,nz,d)
    // using normal.dot(point)-d. A sphere tangent on the negative side is
    // outside, including a zero-radius point exactly on a plane.
    bool nativeReactionSphereInFrustum(const std::array<float, 4>& sphere,
        const std::array<std::array<float, 4>, 6>& planes);
    // Original raw Blend: decimal parsing stores the low byte; last match wins.
    // Integer overflow and malformed leading CR without LF are unsupported.
    std::uint8_t meleeBlendFrames(std::span<const MeleeTextKey> textKeys);
    float meleeBlendDuration(std::optional<std::uint8_t> priorFrames,
        std::uint8_t frames, float defaultDuration);

    enum class OrdinaryMeleePhase : std::uint8_t { Start, Contact, Queue, End };
    enum class PowerMeleePhase : std::uint8_t { Start, Contact, End };
    // Original ordinary subtype4 slots: Start, Hit, a:, End. Advance at most
    // one phase per native animation update, strictly after its next key.
    OrdinaryMeleePhase advanceOrdinaryMeleePhase(OrdinaryMeleePhase phase,
        float sequenceOffset, float animationClock, const std::array<float, 4>& keyTimes);
    PowerMeleePhase advancePowerMeleePhase(PowerMeleePhase phase,
        float sequenceOffset, float animationClock, const std::array<float, 3>& keyTimes);
    struct ReactionSlotProgress
    {
        bool mStarted;
        bool mRetire;
        friend bool operator==(const ReactionSlotProgress&, const ReactionSlotProgress&) = default;
    };
    // Original477414..4774BD: an active, clamp-cycle subtype0/1 slot
    // (BlockHit/Recoil/Stagger), with no queued replacement. Start is strict;
    // End is inclusive and can retire on the same update that crosses Start.
    // The caller owns the state1/easing/freeze guards, blend deactivation and
    // process-action recovery. mRetire is a request, not an inactive sequence.
    ReactionSlotProgress advanceReactionSlot(bool started, float sequenceOffset,
        float animationClock, const std::array<float, 2>& keyTimes);
    float advanceMeleeAnimationClock(float animationClock, float frameDuration);
    // The caller has resolved a newly activated sequence's sentinel offset.
    // Original6CA950 initializes it from the current native animation clock.
    float initialMeleeSequenceOffset(float animationClock);
    // Original477086 speed correction, including the offset-minus-begin store
    // and PC24 register arithmetic observed in actual Player frames. Caller
    // owns group/state/freeze eligibility.
    float correctMeleeSequenceOffset(float sequenceOffset, float sequenceBegin,
        float playbackSpeed, float frameDuration);


    enum class MeleeSequenceCycle : std::uint8_t { Loop = 0, Clamp = 2 };
    // Timing for an unsynchronized loop/clamp sequence (native states1/2/5).
    // Optional values represent the original uninitialized sentinel, without
    // importing a magic float into supported runtime data.
    struct MeleeSequenceTiming
    {
        bool mEasing = false;
        std::optional<float> mOffset;
        std::optional<float> mEaseStart;
        float mEaseEnd = 0;
        std::optional<float> mLastInput;
        float mWeightedTime = 0;
        float mOutputTime = 0;
        friend bool operator==(const MeleeSequenceTiming&, const MeleeSequenceTiming&) = default;
    };
    // Original6CA950/6C5FC0. Caller supplies the global animation clock and
    // owns activation, synchronization/cycle selection, phase ordering and
    // actual controlled transforms. PC24 operation rounding matches the live
    // body/FP manager entry. Does not change its input state.
    MeleeSequenceTiming updateMeleeSequenceTiming(const MeleeSequenceTiming& state,
        float animationClock, float frequency, float begin, float end,
        std::optional<float> transitionFrame = std::nullopt,
        MeleeSequenceCycle cycle = MeleeSequenceCycle::Clamp);

    struct OrdinaryMeleeFrame
    {
        float mClock;
        OrdinaryMeleePhase mPhase;
        MeleeSequenceTiming mTiming;
        friend bool operator==(const OrdinaryMeleeFrame&, const OrdinaryMeleeFrame&) = default;
    };
    // Common ordinary slot frame prefix, then unsynchronized clamp manager
    // update. freezeClock models original mode5, not slot freezes3/6.
    OrdinaryMeleeFrame advanceOrdinaryMeleeFrame(const OrdinaryMeleeFrame& frame,
        float duration, float speed, float frequency, float begin, float end,
        const std::array<float, 4>& keyTimes, bool freezeClock = false,
        std::optional<float> transitionFrame = std::nullopt);
    struct PowerMeleeFrame
    {
        float mClock;
        PowerMeleePhase mPhase;
        MeleeSequenceTiming mTiming;
        friend bool operator==(const PowerMeleeFrame&, const PowerMeleeFrame&) = default;
    };
    // Native subtype5 uses Start/Hit/End, without the ordinary a: queue slot.
    PowerMeleeFrame advancePowerMeleeFrame(const PowerMeleeFrame& frame,
        float duration, float speed, float frequency, float begin, float end,
        const std::array<float, 3>& keyTimes, bool freezeClock = false,
        std::optional<float> transitionFrame = std::nullopt);

    enum class PowerAttackDirection { Standing, Forward, Backward, Left, Right };
    struct PowerAttackSettings
    {
        float mBaseMultiplier;
        float mStandingMultiplier;
        float mSideMultiplier;
        float mBackwardMultiplier;
        float mForwardMultiplier;
    };
    CombatMastery combatMastery(std::int32_t skill, const CombatMasterySettings& settings);
    float powerAttackMultiplier(std::int32_t skill, PowerAttackDirection direction,
        const PowerAttackSettings& settings, const CombatMasterySettings& mastery);
    void validateCombatMasterySettings(const CombatMasterySettings& settings);
    void validatePowerAttackSettings(const PowerAttackSettings& settings);

    struct CombatConeResult
    {
        float mDegrees;
        bool mInside;
    };
    // Bearing is the separately computed horizontal direction to the target.
    CombatConeResult combatHitCone(float facingRadians, float bearingRadians, float coneDegrees);

    struct MeleeReachSettings
    {
        float mCombatDistance;
        float mHandMultiplier;
        float mGiantMultiplier;
    };
    // Base reach is rounded before applying resolved actor scale (including NPC race
    // height). Creature RNAM is already a distance; weapon reach is a
    // combat-distance factor.
    float weaponMeleeReach(float weaponReach, float scale, const MeleeReachSettings& settings);
    float unarmedMeleeReach(float scale, const MeleeReachSettings& settings);
    float creatureMeleeReach(std::uint8_t reach, std::uint8_t creatureType, float scale,
        const MeleeReachSettings& settings);
    void validateMeleeReachSettings(const MeleeReachSettings& settings);

    struct MeleeDistanceActor
    {
        std::array<float, 3> mPosition{};
        float mMinimumZ = 0;
        float mMaximumZ = 0;
        float mMaximumY = 0;
        float mScale = 1;
        bool mIsActor = true;
        bool mSwimming = false;
    };
    // Bounds are outputs of the native reference bounds getters, before the
    // separate scale multiplication for radius. The caller owns reference
    // validity, same-space checks and the initial reference distance.
    float meleeContactDistance(float referenceDistance, const MeleeDistanceActor& attacker,
        const MeleeDistanceActor& target, bool selectedTarget, float slopeDifference);

    struct MeleeContactCandidate
    {
        float mDistance;
        float mFacingDegrees;
        bool mInsideCone;
        bool mEligible;
    };
    // A selected target is exclusive, even when it fails. Otherwise native
    // acquisition chooses the smallest angle; equal angles prefer the later
    // candidate in the caller's resident actor order. Reach is inclusive.
    // Eligibility applies to enumeration only; the original selected branch
    // does not read death/residency flags. Application eligibility is separate.
    std::optional<std::size_t> selectMeleeContact(std::span<const MeleeContactCandidate> candidates,
        float reach, std::optional<std::size_t> selectedTarget = {});

    struct PhysicalCombatSettings
    {
        std::int32_t mLuckSkillBase;
        float mLuckSkillMultiplier;
        float mFatigueBase;
        float mFatigueMultiplier;
        float mWeaponMultiplier;
        float mSkillBase;
        float mSkillMultiplier;
        float mConditionBase;
        float mConditionMultiplier;
        float mAttributeBase;
        float mAttributeMultiplier;
    };

    struct EssentialRecoverySettings
    {
        float mDelay;
        float mHealthFraction;
    };
    enum class NativeDeathDecision : std::uint8_t { Skip, Die, EnterEssentialUnconscious, RefreshEssentialHealth };
    struct NativeDeathInput
    {
        std::uint32_t mRawLifeState;
        bool mEssentialHandlingEnabled;
        bool mBaseEssential;
    };
    // Negative-Health callback gate; explicit Kill bypasses the Health check.
    bool nativeHealthCallbackEligible(std::uint32_t rawLifeState, float currentHealth);
    // Already admitted original6005F0 transition. Base flags are resolved for
    // NPCs, creatures and Player; this returns no world-side effects.
    NativeDeathDecision nativeDeathDecision(const NativeDeathInput& input);

    struct EssentialRecoveryHealth
    {
        float mTarget;
        float mAdjustment;
    };
    // Shared by entry into essential unconsciousness and its recovery. The
    // controller owns eligibility, the countdown and the actor-value mutation.
    EssentialRecoveryHealth essentialRecoveryHealth(std::int32_t baseHealth, float currentHealth,
        const EssentialRecoverySettings& settings);
    void validateEssentialRecoverySettings(const EssentialRecoverySettings& settings);

    struct EssentialRecoveryTick
    {
        float mRemaining;
        bool mRecover;
    };
    // Native process knocked-state byte, not TES3 hit-animation state. Only
    // raw states 1/3 advance an already-unconscious essential actor's timer.
    // The original timer may overshoot below zero; the lifecycle adapter owns
    // clearing its persisted countdown when committing recovery.
    EssentialRecoveryTick advanceEssentialRecovery(float remaining, float frameSeconds,
        bool essentialUnconscious, std::int8_t knockedState);

    // Sustained incapacitation requirement, not a random hit knockdown or an
    // animation-completion signal. Essential means already unconscious, not
    // simply flagged essential. Recovery may begin when this becomes false.
    bool requiresIncapacitation(float currentFatigue, bool paralyzed, bool essentialUnconscious);

    enum class PhysicalReactionInitializationAction : std::uint8_t
    {
        SkipDuplicatePlayerAnimation,
        ClearActorLifeReaction,
        DispatchExistingState,
        BeginParalysis,
        BeginFatigue,
    };

    struct PhysicalReactionInitialization
    {
        std::int8_t mKnockedState;
        bool mClearFlag40;
        PhysicalReactionInitializationAction mAction;
    };

    // Original6545E0 initialization prefix before physical side effects.
    // Life states1/2 clear the process byte;6 requires incapacitation. The
    // caller resolves Player animation identity and current AV10/IntegerAV48.
    // Existing raw bytes stay intact; this does not execute body/magic/mount
    // cleanup, animation, blend clocks or the later raw-state dispatcher.
    PhysicalReactionInitialization resolvePhysicalReactionInitialization(
        std::uint32_t rawLifeState, std::int8_t knockedState, float currentFatigue,
        std::int32_t paralysisInteger, bool duplicatePlayerAnimation);

    // Original raw2/4 entry prefix: root absence skips the lookup; an absent
    // blend object yields gain1. This is a gate only, not entry cleanup or gain
    // evolution. IEEE nonfinite comparison behavior follows the original.
    bool knockdownBlendEntryReady(bool hasRoot, std::optional<float> hierarchyGain);

    struct PhysicalBlendGains
    {
        float mHierarchy;
        float mVelocity;
        friend bool operator==(const PhysicalBlendGains&, const PhysicalBlendGains&) = default;
    };

    struct PhysicalBlendKey
    {
        float mTime;
        PhysicalBlendGains mGains;
        friend bool operator==(const PhysicalBlendKey&, const PhysicalBlendKey&) = default;
    };

    // The native one-shot transition creates at most two keys. Empty keys
    // leave gains unchanged; one key is constant. Two-key evaluation requires
    // a caller-clamped time and keeps the native float-store boundaries.
    std::optional<PhysicalBlendGains> evaluatePhysicalBlend(
        std::span<const PhysicalBlendKey> keys, float time);

    struct PhysicalBlendEvaluation
    {
        std::optional<PhysicalBlendGains> mGains;
        std::uint32_t mCursor;
    };

    // Original8AA990 general authored-key evaluation with an explicit cached
    // lower segment cursor. Return the next cursor without mutating input.
    // Empty/single-key paths ignore unused time/cursor fields; multi-key input
    // requires finite ordered times, finite gains and caller-clamped time.
    PhysicalBlendEvaluation evaluatePhysicalBlendKeys(
        std::span<const PhysicalBlendKey> keys, float time, std::uint32_t cursor);

    struct PhysicalBlendClock
    {
        // Original NiTimeController sentinel (-FLT_MAX), not an actor timer.
        float mStartTime = -3.40282346638528859812e+38F;
        float mPreviousTime = -3.40282346638528859812e+38F;
        float mElapsed = 0.f;
        friend bool operator==(const PhysicalBlendClock&, const PhysicalBlendClock&) = default;
    };

    // Absolute, speed1, phase0, one-shot controller clock (flags0xc5).
    // The returned key time is clamped; stored elapsed time is not. The first
    // update establishes the origin. This is not native controller attachment.
    float advancePhysicalBlendClock(PhysicalBlendClock& clock, float absoluteTime, float duration);

    struct PhysicalBlendTiming
    {
        std::uint16_t mFlags;
        float mFrequency, mPhase, mStartKey, mStopKey;
        friend bool operator==(const PhysicalBlendTiming&, const PhysicalBlendTiming&) = default;
    };

    // Original7155A0 uses a shared last-result cache. Its identity includes
    // cycle mode, bounds and uncycled key time, but excludes the reverse flag.
    // The runtime authority owns this cache; it is not static helper state.
    struct PhysicalBlendTimeCache
    {
        std::uint32_t mCycle = 0xffffffffu;
        float mStopKey = 0.f, mStartKey = 0.f, mKeyTime = 0.f, mResult = 0.f;
        friend bool operator==(const PhysicalBlendTimeCache&, const PhysicalBlendTimeCache&) = default;
    };

    // General finite ordered-bound controller clock. Stage clock/cache writes
    // together; malformed inputs or arithmetic overflow leave both unchanged.
    // Controller active/attachment/update eligibility belongs to the caller.
    float advancePhysicalBlendClock(PhysicalBlendClock& clock, PhysicalBlendTimeCache& cache,
        const PhysicalBlendTiming& timing, float inputTime);

    struct KnockdownSettings
    {
        float mAgilityBase;
        float mAgilityMultiplier;
        float mDamageBase;
        float mDamageMultiplier;
        float mMaximumChance;
    };
    // Caller supplies truncated pre-armor contact damage and a draw in [0,99].
    // This is the random damage rule, not unconsciousness or mastery policy.
    bool damageKnockdown(std::int32_t agility, std::int32_t luck, float fatigueRatio,
        std::int32_t damage, unsigned draw, const KnockdownSettings& settings,
        const PhysicalCombatSettings& physical);
    void validateKnockdownSettings(const KnockdownSettings& settings);

    struct KnockbackSettings
    {
        float mAgilityBase;
        float mAgilityMultiplier;
        float mDamageBase;
        float mDamageMultiplier;
        float mMaximumForce;
        float mDuration;
    };
    // Signed force before caller direction/physics; only its upper bound is capped.
    float damageKnockback(std::int32_t agility, std::int32_t luck, float fatigueRatio,
        std::int32_t damage, const KnockbackSettings& settings, const PhysicalCombatSettings& physical);
    void validateKnockbackSettings(const KnockbackSettings& settings);

    // Original Havok character storage, in its native units. This pulse is
    // separate from gravity/inertia and is replaced only by a stronger pulse.
    struct TimedKnockbackState
    {
        std::array<float, 3> mAcceleration{};
        float mRemaining = 0;
        friend bool operator==(const TimedKnockbackState&, const TimedKnockbackState&) = default;
    };
    std::array<float, 3> nativeKnockbackVector(const std::array<float, 3>& delta, float force);
    TimedKnockbackState replaceNativeKnockback(const TimedKnockbackState& previous,
        const std::array<float, 3>& worldVector, float duration);
    // Composition and expiry are separate operations. The movement adapter
    // must resolve their ordering from the original dispatcher.
    std::array<float, 3> nativeKnockbackVelocity(TimedKnockbackState& state,
        const std::array<float, 3>& baseVelocity, std::uint32_t flags);
    void advanceNativeKnockback(TimedKnockbackState& state, float elapsed);
    // Mastery uses the base actor value, floored before rank lookup, not the
    // luck/effect-adjusted combat value. Fatigue divides current by this base.
    std::int32_t combatBaseValue(float value);
    float combatFatigueRatio(float current, std::int32_t base);

    struct BlockCostSettings
    {
        float mBase;
        float mMultiplier;
        float mSkillBase;
        float mSkillMultiplier;
    };
    struct BlockContactCosts
    {
        float mFatigueDebit;
        float mBlockingItemWear;
    };
    // Current integer Block enters fatigue arithmetic; base Block selects
    // mastery. Item wear still passes through the item condition mutation.
    BlockContactCosts blockContactCosts(std::int32_t baseBlock, std::int32_t currentBlock,
        float damage, float absorbedFraction, bool hasBlockingItem,
        const BlockCostSettings& settings, const CombatMasterySettings& mastery);
    void validateBlockCostSettings(const BlockCostSettings& settings);
    // Original5FF8F3 rounds subtraction and multiplication to x87 PC24
    // before publishing the binary32 remaining Health damage.
    float blockedHealthDamage(float damage, float absorbedFraction);
    // Native accepted block adjusts the previously stored body-armor wear.
    // Preserve the intermediate remainder store; an unarmed block leaves it
    // unchanged, while complete equipped absorption suppresses this wear.
    float blockedArmorWear(float wear, float absorbedFraction, bool unarmedBlock);


    struct WeaponDamageInput
    {
        std::int32_t mSkill;
        std::int32_t mLuck;
        std::int32_t mAttribute; // Governing attribute supplied by weapon policy.
        std::uint16_t mBaseDamage;
        float mConditionRatio; // Current / original maximum; may exceed one.
        float mFatigueRatio; // Supplied actor ratio; this helper does not clamp it.
        float mAttackMultiplier = 1.f;
        bool mIgnoreFatigue = false;
    };

    struct HandToHandSettings
    {
        float mSkillBase;
        float mSkillMultiplier;
        float mStrengthBase;
        float mStrengthMultiplier;
        float mHealthMinimum;
        float mHealthMaximum;
        float mFatigueBase;
        float mFatigueMultiplier;
    };

    struct AttackFatigueSettings
    {
        float mBase;
        float mWeightMultiplier;
        float mPowerMultiplier;
    };
    // Caller excludes staff/bow actions and supplies zero weight for unarmed.
    float attackFatigueCost(float weaponWeight, bool powerAttack, const AttackFatigueSettings& settings);
    void validateAttackFatigueSettings(const AttackFatigueSettings& settings);

    struct MovementFatigueSettings
    {
        float mStrengthCapacityMultiplier;
        float mRunBase;
        float mRunMultiplier;
        std::array<float, 5> mAthleticsMultipliers;
        float mJumpBase;
        float mJumpMultiplier;
        float mExpertJumpMultiplier;
    };
    struct MovementFatigueInput
    {
        float mCurrentFatigue;
        float mCurrentStrength;
        std::int32_t mCurrentEncumbrance;
        std::int32_t mBaseSkill; // Athletics for running, Acrobatics for jumping.
    };
    // Positive debit, limited to positive current fatigue. The caller owns
    // movement acceptance, actor expenditure eligibility, god mode and mutation.
    // Zero capacity follows native transient IEEE arithmetic; no NaN/Inf escapes.
    float runningFatigueDebit(const MovementFatigueInput& input, float duration,
        const MovementFatigueSettings& settings, const CombatMasterySettings& mastery);
    float jumpingFatigueDebit(const MovementFatigueInput& input,
        const MovementFatigueSettings& settings, const CombatMasterySettings& mastery);
    void validateMovementFatigueSettings(const MovementFatigueSettings& settings);

    // Original water probe stores height*ratio to float, then compares the
    // unrounded position+offset sum strictly below the water plane.
    bool actorWaterProbe(float positionZ, float height, float ratio, float waterLevel);
    bool actorNeedsAir(bool pureAquatic, bool deeplySubmerged, bool swimming);

    struct SwimBreathSettings
    {
        float mBase;
        float mEnduranceMultiplier;
        float mDamageMultiplier;
    };
    struct SwimBreathUpdate
    {
        float mRemaining;
        bool mDrowning;
    };
    // Caller supplies native integer Endurance/base Health and owns water,
    // process and actor eligibility. Damage has separate rate/frame float stores.
    float swimBreathMaximum(std::int32_t endurance, const SwimBreathSettings& settings);
    float drowningDamage(std::int32_t baseHealth, float duration, const SwimBreathSettings& settings);
    SwimBreathUpdate updateSwimBreath(float remaining, float maximum, float duration,
        std::int32_t waterBreathing);
    void validateSwimBreathSettings(const SwimBreathSettings& settings);

    struct FatigueRegenerationSettings
    {
        float mBase;
        float mEnduranceMultiplier;
    };
    struct FatigueRegenerationInput
    {
        float mCurrent;
        float mBase;
        float mMaximumModifier; // Native maximum modifier, excluding script/damage modifiers.
        std::int32_t mEndurance; // Current integer Endurance AV, not luck-adjusted.
        float mDuration;
    };
    // Requested positive restoration only. The actor-value mutation authority
    // owns Damage-channel mutation and applying the change once.
    float fatigueRegeneration(const FatigueRegenerationInput& input, const FatigueRegenerationSettings& settings);
    void validateFatigueRegenerationSettings(const FatigueRegenerationSettings& settings);

    // Requested restoration, before Damage-channel mutation. Maximum modifier
    // eligibility and actor/death/rest policy belong to the caller.
    float healthRestoration(float current, std::int32_t base, float maximumModifier);

    struct MagickaRegenerationSettings
    {
        float mBase;
        float mWillpowerMultiplier;
    };
    struct MagickaRegenerationInput
    {
        float mCurrent;
        std::int32_t mBase;
        float mMaximumModifier;
        std::int32_t mWillpower;
        std::int32_t mStuntedMagicka;
        float mDuration;
        bool mHasActiveMagicItem;
        bool mCheckActiveMagicItem;
    };
    float magickaRegeneration(const MagickaRegenerationInput& input, const MagickaRegenerationSettings& settings);
    void validateMagickaRegenerationSettings(const MagickaRegenerationSettings& settings);

    struct DurabilitySettings
    {
        float mWeaponDamageMultiplier;
        float mArmorDamageMultiplier;
    };
    // Pure wear amounts. Contact eligibility, armor-piece selection and
    // condition mutation are caller responsibilities. Signed armor fractions can
    // produce nonpositive wear, which must not repair an item.
    float weaponWear(std::uint16_t baseDamage, const DurabilitySettings& settings);
    float armorWear(float incomingDamage, float absorbedFraction, const DurabilitySettings& settings);
    void validateDurabilitySettings(const DurabilitySettings& settings);

    enum class ArmorWeight { Light, Heavy };
    struct ArmorWearMasterySettings
    {
        float mLightNoviceMultiplier;
        float mHeavyNoviceMultiplier;
        float mLightJourneymanMultiplier;
        float mHeavyJourneymanMultiplier;
    };
    float armorWearMasteryMultiplier(std::int32_t skill, ArmorWeight weight,
        const ArmorWearMasterySettings& settings, const CombatMasterySettings& mastery);
    void validateArmorWearMasterySettings(const ArmorWearMasterySettings& settings);
    // Caller supplies final wear after mastery/block policy. Positive wear
    // snaps remaining condition below one to zero; repaired excess is retained.
    // Nonpositive wear preserves the current condition exactly.
    float conditionAfterWear(float current, float wear);
    // Native item readers retain an unsigned maximum or a stored float in
    // double until subtraction. No value means no condition publication;
    // nonpositive wear must preserve an absent/fractional condition unchanged.
    std::optional<float> nativeConditionAfterWear(double current, float wear);
    // Admission precedes the armor mastery multiplier. Positive incoming wear
    // still publishes the native float store when the multiplier is zero.
    // The caller supplies the original floored base skill, not current AV.
    std::optional<float> nativeArmorConditionAfterWear(double current, float wear,
        std::int32_t baseSkill, ArmorWeight weight, const ArmorWearMasterySettings& settings,
        const CombatMasterySettings& mastery, bool bypassMastery = false);


    struct ArmorMitigation
    {
        float mHealthDamage;
        float mAbsorbedFraction;
    };
    // Total rating already includes the actor's aggregation/mastery/cap policy.
    // Rating includes signed Defense; negative values amplify damage.
    // The caller resolves armor bypass eligibility; this helper only applies it.
    ArmorMitigation mitigateArmor(float damage, float rating, float maximumFraction, bool bypass);

    struct CombatRandomDraw
    {
        std::uint32_t mNextState;
        std::uint16_t mValue;
    };
    // Original initialized CRT stream: explicit state in/out, no clock seed.
    // Percentile callers use mValue % 100, preserving the native modulo bias.
    CombatRandomDraw combatRandomDraw(std::uint32_t state) noexcept;

    enum class NativeSequenceCollection : std::uint8_t { Single, Multiple };
    struct NativeSequenceSelection
    {
        std::uint32_t mIndex;
        std::uint32_t mNextState;
        unsigned mDraws;
        friend bool operator==(const NativeSequenceSelection&, const NativeSequenceSelection&) = default;
    };
    // Original471710/470BF0. Single ignores the requested byte. Multiple
    // interprets it as signed: negative or out-of-range requests draw once,
    // then use unsigned remainder. Selection does not exclude the last item.
    // The caller owns collection identity/order and atomically commits RNG.
    NativeSequenceSelection selectNativeSequenceVariant(NativeSequenceCollection collection,
        std::uint32_t count, std::uint8_t requested, std::uint32_t randomState);

    struct NativeSequenceResources
    {
        NativeSequenceCollection mCollection;
        std::span<const std::string_view> mPaths;
    };
    struct NativePlayerReactionSelection
    {
        std::uint32_t mPreflightIndex;
        std::uint32_t mBodyIndex;
        std::uint32_t mFirstPersonIndex;
        bool mFirstPersonMatched;
        std::uint32_t mNextState;
        unsigned mDraws;
        friend bool operator==(const NativePlayerReactionSelection&, const NativePlayerReactionSelection&) = default;
    };
    // Original Player group preflight uses the active camera's collection;
    // playback uses body, then65D790/470C40 matches a Windows basename in a
    // Multiple first-person collection or falls back to another selection.
    // Both group resolutions and ordered resources must already be available.
    // Missing-group/family fallback and playback activation belong to callers.
    NativePlayerReactionSelection selectNativePlayerReactionResources(bool firstPersonCamera,
        const NativeSequenceResources& body, const NativeSequenceResources& firstPerson,
        std::uint32_t randomState);

    enum class ArmorWearSlot { Head, Hair, UpperBody, LowerBody, Hands, Feet, Shield };
    struct ArmorWearSelectionSettings
    {
        std::int32_t mHeadChance;
        std::int32_t mUpperBodyChance;
        std::int32_t mLowerBodyChance;
        std::int32_t mHandsChance;
        std::int32_t mFeetChance;
    };
    // One original selection attempt, using a supplied [0,99] draw. Missing
    // candidates fall forward through thresholds; feet do not fall to shield.
    // The world retries at most seven draws, stopping on the first selection.
    inline constexpr unsigned ArmorWearSelectionAttempts = 7;
    std::optional<ArmorWearSlot> selectArmorWearSlot(unsigned draw,
        const std::array<bool, 7>& available, const ArmorWearSelectionSettings& settings);
    void validateArmorWearSelectionSettings(const ArmorWearSelectionSettings& settings);
    struct ArmorWearSelection
    {
        std::optional<ArmorWearSlot> mSlot;
        std::uint32_t mNextState;
        unsigned mDraws;
    };
    // Prepare up to seven native draws without publishing stream state. A
    // caller commits mNextState alongside the contact, even if no slot wins.
    ArmorWearSelection selectArmorWear(std::uint32_t state,
        const std::array<bool, 7>& available, const ArmorWearSelectionSettings& settings);

    struct HandToHandInput
    {
        std::int32_t mSkill;
        std::int32_t mLuck;
        std::int32_t mStrength;
        float mFatigueRatio;
        bool mSuppressFatigueDamage = false;
    };

    struct HandToHandDamage
    {
        float mHealth;
        float mFatigue;
    };

    struct HandToHandContactInput
    {
        std::int32_t mSkill;
        std::int32_t mLuck;
        std::int32_t mStrength;
        float mCurrentFatigue;
        std::int32_t mBaseFatigue;
        // Result of the victim's native process knocked-state getter, not a
        // TES3 animation enum. Low processes return zero; no process is nullopt.
        std::optional<std::int8_t> mVictimKnockedState;
    };

    // Common native predicate queries process action +2D0 and compares exactly6.
    // A missing process or another signed action code is not blocking.
    bool nativeBlockingPosture(std::optional<std::int16_t> processAction);
    // GetKnockedState command/CTDA4F76B0 translates only raw3/4 to1.
    // This is distinct from the signed process byte used by contact rules.
    bool nativeScriptKnockedState(std::int8_t processState) noexcept;

    enum class BlockEquipment { Shield, Weapon, Unarmed };
    struct BlockContactInput
    {
        bool mBlocking;
        bool mParalyzed;
        bool mBypassBlock;
        bool mInsideCone;
        BlockEquipment mEquipment;
        bool mWeaponAttack;
        bool mProjectile;
    };
    enum class BlockContactDisposition { None, ReactionOnly, Absorb };
    // ReactionOnly retains block reaction handling but has zero absorbed fraction.
    BlockContactDisposition blockContactDisposition(const BlockContactInput& input);

    struct BlockSettings
    {
        float mSkillBase;
        float mSkillMultiplier;
        float mMaximum;
        float mWeaponMultiplier;
        float mUnarmedMultiplier;
    };
    struct BlockInput
    {
        std::int32_t mSkill;
        std::int32_t mLuck;
        float mFatigueRatio;
        BlockEquipment mEquipment;
    };

    struct ArmorMasterySettings
    {
        // Head, hair, upper body, lower body, hands, feet, active shield.
        std::array<std::int32_t, 7> mCoverage;
        std::int32_t mLightMasterMinimum;
        float mLightMasterRatingMultiplier;
        float mHeavyExpertWeightMultiplier;
        float mHeavyMasterWeightMultiplier;
        float mLightExpertWeightMultiplier;
    };
    std::int32_t armorCoverage(const std::array<bool, 7>& matchingSlots,
        const ArmorMasterySettings& settings);
    float masteryArmorRating(float itemRating, float otherRating, std::int32_t baseLightArmor,
        std::int32_t lightCoverage, std::int32_t heavyCoverage, float maximum,
        const ArmorMasterySettings& settings, const CombatMasterySettings& mastery);
    // Reduction applies to one worn instance, not other items in its stack.
    float wornArmorWeight(float weight, bool heavy, std::int32_t baseSkill, bool worn,
        const ArmorMasterySettings& settings, const CombatMasterySettings& mastery);
    void validateArmorMasterySettings(const ArmorMasterySettings& settings);

    struct ArmorRatingSettings
    {
        float mSkillBase;
        float mSkillMaximum;
        float mConditionBase;
        float mConditionMultiplier;
    };
    struct ArmorRatingInput
    {
        std::uint16_t mBaseHundredths;
        std::int32_t mSkill;
        std::int32_t mLuck;
        float mConditionRatio;
    };
    float armorRating(const ArmorRatingInput& input, const ArmorRatingSettings& settings,
        const PhysicalCombatSettings& physical);

    // Original488CB0 rounds each equipped entry after547370, before aggregation.
    float nativeEquippedArmorRating(const ArmorRatingInput& input, const ArmorRatingSettings& settings,
        const PhysicalCombatSettings& physical);
    float capArmorRating(float total, float maximum);
    void validateArmorRatingSettings(const ArmorRatingSettings& settings);

    // The caller resolves identities first. Self-inflicted damage follows the
    // original victim-player branch; unknown sources select Unaffected.
    enum class PlayerDamageRole { Unaffected, Attacker, Victim };

    struct PhysicalContactDamage
    {
        float mHealth;
        float mFatigue;
    };
    // Resource amounts selected for native damage writers after armor/block.
    // Rescale Fatigue with the pre-difficulty Health ratio, then apply difficulty
    // to Health only. A zero/zero ratio produces no Fatigue writer in the native
    // sink. Nonfinite writer amounts remain outside finite runtime storage.
    PhysicalContactDamage physicalContactDamage(const PhysicalContactDamage& incoming,
        float remainingHealth, float difficulty, float difficultyMultiplier, PlayerDamageRole role);

    HandToHandDamage handToHandDamage(const HandToHandInput& input,
        const HandToHandSettings& settings, const PhysicalCombatSettings& physical);
    HandToHandDamage handToHandContactDamage(const HandToHandContactInput& input,
        const HandToHandSettings& settings, const PhysicalCombatSettings& physical);
    // Original contact rescales Fatigue by the stored post-armor/block Health
    // ratio before difficulty affects Health. Caller supplies both Health stores.
    // Nonzero incoming Fatigue requires a nonzero Health denominator; singular
    // or overflowing native intermediates are diagnosed rather than published.
    float mitigateContactFatigue(float incomingFatigue, float remainingHealthDamage,
        float unmitigatedHealthDamage);
    float blockFraction(const BlockInput& input, const BlockSettings& settings,
        const PhysicalCombatSettings& physical);
    float difficultyDamage(float damage, float difficulty, float multiplier, PlayerDamageRole role);
    void validateHandToHandSettings(const HandToHandSettings& settings);
    void validateBlockSettings(const BlockSettings& settings);

    // Pure original TES4 pre-mitigation arithmetic. No contact, mastery,
    // difficulty, armor, block, enchantment or actor mutation occurs here.
    float effectiveCombatSkill(std::int32_t skill, std::int32_t luck, const PhysicalCombatSettings& settings);
    float combatFatigueMultiplier(float ratio, const PhysicalCombatSettings& settings);
    float weaponDamage(const WeaponDamageInput& input, const PhysicalCombatSettings& settings);
    // Original484F80 adds integer AV42 after the547070 float result, using
    // PC24 FILD/FADD and another explicit float store.
    float addWeaponAttackBonus(float damage, std::int32_t bonus);
    // Creature natural attacks use base attack damage and fatigue, followed by
    // truncation to signed integer in the original actor virtual method.
    std::int32_t creatureNaturalDamage(std::uint16_t baseDamage, float fatigueRatio,
        const PhysicalCombatSettings& settings);
    void validatePhysicalCombatSettings(const PhysicalCombatSettings& settings);
    // Original held NiAVObject world rotation layout (nine float values).
    // Return the dropped reference XYZ rotation, including native atan stores
    // and singular branches. This does not decompose an OSG scene matrix.
    std::array<float, 3> nativeDroppedReferenceRotation(const std::array<float, 9>& rotation);

    enum class DropExtraOwnerSelection : std::uint8_t { KeepExisting, ClearExisting, PlayerBase };

    // Original drop prelude only. This selects the item's ExtraOwnership
    // update, not the complete dropped-reference ownership or publication.
    // Actor state is the original uint32 actor+B0 slot, not a TES3 animation
    // enum. Cell ownership is the result of its native ExtraOwnership query.
    DropExtraOwnerSelection selectDropExtraOwner(std::uint32_t nativeActorState,
        bool hasExistingOwner, bool cellHasOwner, std::int32_t basePrice, float threshold);

}

#endif
