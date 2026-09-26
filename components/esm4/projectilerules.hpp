#ifndef OPENMW_ESM4_PROJECTILERULES_H
#define OPENMW_ESM4_PROJECTILERULES_H

#include "physicalcombat.hpp"

#include <optional>
#include <span>

namespace ESM4
{
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
