#ifndef OPENMW_ESM4_PROJECTILERULES_H
#define OPENMW_ESM4_PROJECTILERULES_H

#include "physicalcombat.hpp"

#include <optional>
#include <span>

namespace ESM4
{
    struct BowAnimationKeys
    {
        std::array<float, 5> mTimes{};
        std::uint8_t mMatchedCount = 0;
    };

    // Native AttackBow phases, in authored traversal order: Start, Attach,
    // Hold, Release, End. Sound keys do not advance phases. This does not
    // validate playback or collect sound events. Non-sound text after a
    // completed five-slot sequence is explicitly outside the supported domain.
    BowAnimationKeys bowAnimationKeyTimes(std::span<const MeleeTextKey> textKeys);

    enum class BowAnimationPhase : std::uint8_t { Start, Attach, Hold, Release, End };
    struct PlayerBowHoldInput
    {
        bool mHeld;
        bool mPressed;
        bool mReady;
        bool mCrossbow;
        bool mBlocked;
        std::uint32_t mAnimationCategory;
        std::uint32_t mInputGate;
        BowAnimationPhase mPhase;
        bool mLatched;
    };
    struct PlayerBowHoldResult
    {
        bool mPaused;
        bool mLatched;
        friend bool operator==(const PlayerBowHoldResult&, const PlayerBowHoldResult&) = default;
    };
    // Original Player input tail for a present bow. The caller supplies the
    // process readiness query and animation category (AttackBow is 7; casts
    // are 5), and applies pause to both views. Earlier attack admission owns
    // latch creation; this tail only preserves or clears it.
    PlayerBowHoldResult playerBowHold(const PlayerBowHoldInput& input);

    struct BowAnimationProgress
    {
        BowAnimationPhase mPhase = BowAnimationPhase::Start;
        float mSequenceOffset = 0;
        friend bool operator==(const BowAnimationProgress&, const BowAnimationProgress&) = default;
    };
    // Original five-phase advancement prefix. The supplied animation clock
    // has already advanced. At most one phase advances, strictly past its key.
    // Entering Hold in the upper-body slot also adjusts the sequence offset.
    // End/queued-animation dispatch and input routing remain caller operations.
    BowAnimationProgress advanceBowAnimation(const BowAnimationProgress& progress,
        float animationClock, const std::array<float, 5>& keyTimes, bool upperBody = true);

    // Per-slot running-sequence prefix, with the animation clock already
    // advanced. A paused upper-body slot subtracts the frame duration from
    // its offset through native float stores and skips phase advancement.
    // Unpaused playback compensates for the supplied native speed through
    // the original sequence-start-relative float stores before advancing.
    // Caller selects pause/release policy and dispatches End/queued playback.
    BowAnimationProgress advanceBowPlayback(const BowAnimationProgress& progress,
        float animationClock, float duration, float sequenceStart,
        const std::array<float, 5>& keyTimes, bool upperBodyPaused, float playbackRate = 1);

    // Player held-bow input prefix, after bow/input eligibility. Actions4/5
    // accumulate through phase Release; other actions or End reset the timer.
    float advancePlayerBowTimer(float current, float duration,
        std::int32_t processAction, BowAnimationPhase phase);

    // Player input selection preceding the hold tail. A new press can reset
    // a completed/inactive draw; only an eligible held frame accumulates.
    // Ineligible input leaves the timer untouched, including unused operands.
    float playerBowTimerAfterInput(float current, float duration, std::int32_t processAction,
        BowAnimationPhase phase, bool held, bool pressed, bool ready, bool blocked);

    enum class BowActionEvent : std::uint8_t { None, Attach, Release };
    // Actor action dispatch after a present, running sequence has been resolved.
    BowActionEvent bowActionEvent(std::int32_t processAction, BowAnimationPhase phase,
        bool sequencePresent, bool sequenceRunning);

    struct ArrowCleanupSettings
    {
        std::int32_t mMaximumReferences;
    };

    struct ArrowCleanupCandidate
    {
        float mAge;
        bool mSettled; // Original arrow state 2; flying/impact/fading states do not qualify.
        bool mPreferredPool; // Native process-list query 1; fallback is query 0.
    };

    // Count is after creating the new arrow. Preserve traversal order within
    // each pool. Caller supplies only resolved, eligible arrow references and
    // starts fading the returned one; this is not a hard cap on flying arrows.
    std::optional<std::size_t> selectArrowForCleanup(std::int32_t referenceCount,
        std::span<const ArrowCleanupCandidate> candidates, const ArrowCleanupSettings& settings);
    void validateArrowCleanupSettings(const ArrowCleanupSettings& settings);

    struct ArrowRecoverySettings
    {
        std::int32_t mInventoryChance;
    };

    struct ArrowInventoryRecoveryResult
    {
        bool mConsumesDraw = false;
        bool mRecover = false;
    };

    // Final inventory-recovery roll after the actor-impact eligibility path.
    // This tests the arrow's enchantment, independently of a bow enchantment.
    // The caller draws only for an unenchanted arrow; draw is in [0,99].
    ArrowInventoryRecoveryResult arrowInventoryRecovery(bool arrowEnchanted, unsigned draw,
        const ArrowRecoverySettings& settings);
    void validateArrowRecoverySettings(const ArrowRecoverySettings& settings);

    struct ArrowLifetimeSettings
    {
        float mMaximumAge;
    };

    struct ArrowLifetimeState
    {
        float mAge = 0;
        float mOpacity = 1;
        bool mFading = false;
    };

    struct ArrowLifetimeChange
    {
        ArrowLifetimeState mState;
        bool mRemove;
    };

    // Applies to a live, non-deleted arrow. Collision/count-budget decisions may
    // begin fading earlier. The world owns removal and must not tick it again.
    ArrowLifetimeChange advanceArrowLifetime(const ArrowLifetimeState& state, float duration,
        const ArrowLifetimeSettings& settings);
    void validateArrowLifetimeSettings(const ArrowLifetimeSettings& settings);

    struct BowFatigueSettings
    {
        float mHoldPerSecond;
        float mPerShot;
    };
    // Hold debit is player-only, during the bow-hold action. Shot debit also
    // applies to NPC novices, after launch damage has been captured.
    float bowHoldFatigue(std::int32_t marksman, bool player, bool holding, float duration,
        const BowFatigueSettings& settings, const CombatMasterySettings& mastery);
    float bowShotFatigue(std::int32_t marksman, const BowFatigueSettings& settings,
        const CombatMasterySettings& mastery);
    void validateBowFatigueSettings(const BowFatigueSettings& settings);

    struct ProjectileSettings
    {
        float mBowTimerBase;
        float mBowTimerMultiplier;
        float mSpeedMultiplier;
        float mWeakSpeed;
        float mGravityBase;
        float mGravityMultiplier;
        float mWeakGravity;
    };

    struct ArrowDamageInput
    {
        std::int32_t mMarksman;
        std::int32_t mLuck;
        std::int32_t mAgility;
        std::uint16_t mBowDamage;
        std::uint16_t mAmmoDamage;
        float mBowConditionRatio;
        float mFatigueRatio;
        float mDrawFraction;
        std::int32_t mAttackBonus = 0; // Caller-resolved AV; no effect execution here.
    };

    // Original launch arithmetic only. The controller owns timer accumulation,
    // release eligibility, ammo consumption, launch transform and collisions.
    float bowDrawFraction(float timer, const ProjectileSettings& settings);
    float arrowLaunchDamage(const ArrowDamageInput& input, const PhysicalCombatSettings& settings);
    float arrowLaunchSpeed(float ammoSpeed, float drawFraction, const ProjectileSettings& settings);
    float arrowGravityFactor(std::int32_t marksman, std::int32_t luck, float drawFraction,
        const ProjectileSettings& settings, const PhysicalCombatSettings& physical);
    void validateProjectileSettings(const ProjectileSettings& settings);
}

#endif
