#ifndef OPENMW_ESM4_MASTERYRULES_H
#define OPENMW_ESM4_MASTERYRULES_H

#include "physicalcombat.hpp"

namespace ESM4
{
    struct MasteryProcSettings
    {
        std::int32_t mAttackDisarm;
        std::int32_t mBlockDisarm;
        std::int32_t mBlockStagger;
        std::int32_t mKnockdown;
        std::int32_t mParalysis;
        std::int32_t mHandBlockRecoil;
    };
    enum class MasteryAttack { Normal, Standing, Forward, Backward, Left, Right, Arrow };
    struct AttackMasteryInput
    {
        std::int32_t mBaseSkill;
        MasteryAttack mAttack;
        std::int32_t mTargetBaseSpeed;
        bool mDamageKnockdown;
    };
    struct AttackMasteryResult
    {
        bool mConsumesDraw;
        bool mKnockdown;
        bool mParalysis;
    };
    struct MasteryProcResult
    {
        bool mConsumesDraw;
        bool mTriggered;
    };
    struct BlockMasteryInput
    {
        std::int32_t mBlock;
        std::int32_t mHandToHand;
        bool mHasWeapon;
        bool mHasShield;
    };
    struct UnarmedBlockRecoilInput
    {
        std::int32_t mBaseBlock;
        std::int32_t mBaseHandToHand;
        float mAbsorbedFraction;
        bool mUnarmed;
        bool mActiveBlock;
        bool mProjectile;
        bool mAttackerHasWeapon;
    };
    // The zero-absorption contact branch uses Block mastery for this roll.
    // Journeyman Hand to Hand exits this branch before the roll. Callers own
    // facing, positive-damage selection and actual opponent recoil admission.
    MasteryProcResult unarmedBlockRecoil(const UnarmedBlockRecoilInput& input, unsigned draw,
        const MasteryProcSettings& settings, const CombatMasterySettings& mastery);

    struct BowZoomSettings
    {
        float mZoomFov;
        float mTimeChange;
        float mTimeStart;
    };
    struct BowZoomInput
    {
        std::int32_t mBaseMarksman;
        std::int32_t mProcessAction;
        float mElapsed;
        float mCurrentFov;
        float mNormalFov;
        float mSceneFov;
        float mDuration;
        bool mEnabled;
        bool mThirdPerson;
        bool mCameraRestricted;
        bool mBlockHeld;
        bool mAiming;
    };
    // Full native FOV update decision. Null means no camera setter invocation,
    // including failed mastery/delay guards; camera/input ownership is external.
    std::optional<float> bowZoomFov(const BowZoomInput& input,
        const BowZoomSettings& settings, const CombatMasterySettings& mastery);
    void validateBowZoomSettings(const BowZoomSettings& settings);

    struct BlockingDodgeInput
    {
        std::int32_t mBaseAcrobatics;
        std::optional<std::uint16_t> mResolvedAnimationGroup;
        bool mBlockHeld;
        bool mInterfacePreventsDodge;
        bool mAnimationBusy;
        bool mAnimationDataPresent;
        bool mProcessPresent;
    };
    // Jump admission precedes this decision. Native low-byte movement bits
    // select forward/back/left/right in that priority order; the resolved
    // animation, rather than the requested group, determines start success.
    std::uint8_t requestedDodgeGroup(std::uint32_t movementFlags);
    bool blockingDodgeAllowed(const BlockingDodgeInput& input, const CombatMasterySettings& mastery);
    // Decisions only: callers own contact eligibility, actual RNG advancement,
    // real inventory drops, effect application, controller state and saves.
    AttackMasteryResult attackMasteryReaction(const AttackMasteryInput& input, unsigned draw,
        const MasteryProcSettings& settings, const CombatMasterySettings& mastery);
    MasteryProcResult sidePowerDisarm(std::int32_t baseSkill, MasteryAttack attack, bool isNpc,
        bool targetHasWeapon, bool targetWeaponQuestItem, bool attackerWeaponDrawn, unsigned draw,
        const MasteryProcSettings& settings, const CombatMasterySettings& mastery);
    MasteryProcResult blockMasteryStagger(const BlockMasteryInput& input, unsigned draw,
        const MasteryProcSettings& settings, const CombatMasterySettings& mastery);
    MasteryProcResult blockMasteryDisarm(const BlockMasteryInput& input, bool isNpc,
        bool targetHasWeapon, bool targetWeaponQuestItem, bool defenderWeaponDrawn, unsigned draw,
        const MasteryProcSettings& settings, const CombatMasterySettings& mastery);
    void validateMasteryProcSettings(const MasteryProcSettings& settings);
}

#endif
