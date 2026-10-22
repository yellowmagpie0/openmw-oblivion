/*
 * OpenMW - The completely unofficial reimplementation of Morrowind
 *
 * This file (character.cpp) is part of the OpenMW package.
 *
 * OpenMW is distributed as free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * version 3, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 3 along with this program. If not, see
 * https://www.gnu.org/licenses/ .
 */

#include "character.hpp"

#include <components/esm4/loadarmo.hpp>

#include "../mwsound/nativeaudioutils.hpp"
#include <components/esm4/loadsoun.hpp>
#include <components/esm4/physicalcombat.hpp>
#include <components/esm4/animationblend.hpp>

#include <array>
#include <bit>
#include <unordered_set>

#include <components/esm/records.hpp>
#include <components/misc/mathutil.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/misc/rng.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/conversion.hpp>

#include <components/settings/values.hpp>

#include <components/sceneutil/positionattitudetransform.hpp>
#include <components/sceneutil/keyframe.hpp>

#include "../mwrender/animation.hpp"
#include "../mwrender/oblivionmeleeprojection.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/player.hpp"
#include "../mwworld/spellcaststate.hpp"

#include "actorutil.hpp"
#include "oblivioncombat.hpp"
#include "oblivionmelee.hpp"
#include <set>
#include "oblivionai.hpp"
#include "../mwworld/worldimp.hpp"
#include "../mwworld/oblivionprofileservices.hpp"
#include "../mwworld/oblivionactorstats.hpp"
#include "../mwlua/localscripts.hpp"
#include <components/esm4/combatsettings.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/loadcrea.hpp>
#include "aicombataction.hpp"
#include "creaturestats.hpp"
#include "movement.hpp"
#include "npcstats.hpp"
#include "security.hpp"
#include "spellcasting.hpp"
#include "weapontype.hpp"

namespace
{
    struct NativeMeleePlayback
    {
        float mBegin, mEnd, mFrequency, mTimelineBegin, mTimelineEnd;
        std::array<float, 4> mKeys;
        std::uint8_t mBlendFrames;
        std::array<float, 3> mPowerKeys{};

        float retirementDuration() const
        {
            const auto frames = std::bit_cast<std::int8_t>(mBlendFrames);
            return frames == 0 ? .1f : static_cast<float>(double(frames) / 30.);
        }

        float rendererTime(float output) const
        {
            if (output <= mBegin)
                return mTimelineBegin;
            if (output >= mEnd)
                return mTimelineEnd;
            const float fraction = (output - mBegin) / (mEnd - mBegin);
            return mTimelineBegin + fraction * (mTimelineEnd - mTimelineBegin);
        }
    };

    std::vector<ESM4::MeleeTextKey> nativeRawKeys(const SceneUtil::ControllerSequenceMetadata& metadata)
    {
        std::vector<ESM4::MeleeTextKey> keys;
        keys.reserve(metadata.mTextKeys.size());
        for (const auto& [time, text] : metadata.mTextKeys)
            keys.push_back({time, text});
        return keys;
    }

    std::optional<NativeMeleePlayback> nativeMeleePlayback(MWRender::Animation& animation,
        std::string_view group, bool power, bool counter = false)
    {
        const auto* metadata = animation.getControllerSequenceMetadata(group);
        if (!metadata || metadata->mCycleType != 2)
            return {};
        for (float value : {metadata->mStartTime, metadata->mStopTime, metadata->mFrequency,
                 metadata->mTimelineStart, metadata->mTimelineStop})
            if (!std::isfinite(value))
                return {};
        if (metadata->mStopTime <= metadata->mStartTime || metadata->mFrequency <= 0
            || metadata->mTimelineStart < 0 || metadata->mTimelineStop <= metadata->mTimelineStart
            || !std::isfinite(metadata->mStopTime - metadata->mStartTime)
            || !std::isfinite(metadata->mTimelineStop - metadata->mTimelineStart))
            return {};
        try
        {
            const auto raw = nativeRawKeys(*metadata);
            if (power)
            {
                const auto keys = counter ? ESM4::counterMeleeKeyTimes(raw) : ESM4::powerMeleeKeyTimes(raw);
                if (keys.mMatchedCount != 3) return {};
                (void)ESM4::advancePowerMeleePhase(ESM4::PowerMeleePhase::Start, 0, 0, keys.mTimes);
                return NativeMeleePlayback{metadata->mStartTime, metadata->mStopTime, metadata->mFrequency,
                    metadata->mTimelineStart, metadata->mTimelineStop, {}, ESM4::meleeBlendFrames(raw), keys.mTimes};
            }
            const auto keys = ESM4::ordinaryMeleeKeyTimes(raw);
            if (keys.mMatchedCount != 4)
                return {};
            (void)ESM4::advanceOrdinaryMeleePhase(ESM4::OrdinaryMeleePhase::Start, 0, 0, keys.mTimes);
            return NativeMeleePlayback{metadata->mStartTime, metadata->mStopTime, metadata->mFrequency,
                metadata->mTimelineStart, metadata->mTimelineStop, keys.mTimes, ESM4::meleeBlendFrames(raw)};
        }
        catch (const std::invalid_argument&)
        {
            return {};
        }
    }

    const ESM::RefId wolfRun = ESM::RefId::stringRefId("WolfRun");

    std::string_view getBestAttack(const ESM::Weapon* weapon)
    {
        int slash = weapon->mData.mSlash[0] + weapon->mData.mSlash[1];
        int chop = weapon->mData.mChop[0] + weapon->mData.mChop[1];
        int thrust = weapon->mData.mThrust[0] + weapon->mData.mThrust[1];
        if (slash == chop && slash == thrust)
            return "slash";
        else if (thrust >= chop && thrust >= slash)
            return "thrust";
        else if (slash >= chop && slash >= thrust)
            return "slash";
        else
            return "chop";
    }

    // Converts a movement Run state to its equivalent Walk state, if there is one.
    MWMechanics::CharacterState runStateToWalkState(MWMechanics::CharacterState state)
    {
        using namespace MWMechanics;
        switch (state)
        {
            case CharState_RunForward:
                return CharState_WalkForward;
            case CharState_RunBack:
                return CharState_WalkBack;
            case CharState_RunLeft:
                return CharState_WalkLeft;
            case CharState_RunRight:
                return CharState_WalkRight;
            case CharState_SwimRunForward:
                return CharState_SwimWalkForward;
            case CharState_SwimRunBack:
                return CharState_SwimWalkBack;
            case CharState_SwimRunLeft:
                return CharState_SwimWalkLeft;
            case CharState_SwimRunRight:
                return CharState_SwimWalkRight;
            default:
                return state;
        }
    }

    // Converts a Hit state to its equivalent Death state.
    MWMechanics::CharacterState hitStateToDeathState(MWMechanics::CharacterState state)
    {
        using namespace MWMechanics;
        switch (state)
        {
            case CharState_SwimKnockDown:
                return CharState_SwimDeathKnockDown;
            case CharState_SwimKnockOut:
                return CharState_SwimDeathKnockOut;
            case CharState_KnockDown:
                return CharState_DeathKnockDown;
            case CharState_KnockOut:
                return CharState_DeathKnockOut;
            default:
                return CharState_None;
        }
    }

    // Converts a movement state to its equivalent base animation group as long as it is a movement state.
    std::string_view movementStateToAnimGroup(MWMechanics::CharacterState state)
    {
        using namespace MWMechanics;
        switch (state)
        {
            case CharState_WalkForward:
                return "walkforward";
            case CharState_WalkBack:
                return "walkback";
            case CharState_WalkLeft:
                return "walkleft";
            case CharState_WalkRight:
                return "walkright";

            case CharState_SwimWalkForward:
                return "swimwalkforward";
            case CharState_SwimWalkBack:
                return "swimwalkback";
            case CharState_SwimWalkLeft:
                return "swimwalkleft";
            case CharState_SwimWalkRight:
                return "swimwalkright";

            case CharState_RunForward:
                return "runforward";
            case CharState_RunBack:
                return "runback";
            case CharState_RunLeft:
                return "runleft";
            case CharState_RunRight:
                return "runright";

            case CharState_SwimRunForward:
                return "swimrunforward";
            case CharState_SwimRunBack:
                return "swimrunback";
            case CharState_SwimRunLeft:
                return "swimrunleft";
            case CharState_SwimRunRight:
                return "swimrunright";

            case CharState_SneakForward:
                return "sneakforward";
            case CharState_SneakBack:
                return "sneakback";
            case CharState_SneakLeft:
                return "sneakleft";
            case CharState_SneakRight:
                return "sneakright";

            case CharState_TurnLeft:
                return "turnleft";
            case CharState_TurnRight:
                return "turnright";
            case CharState_SwimTurnLeft:
                return "swimturnleft";
            case CharState_SwimTurnRight:
                return "swimturnright";
            default:
                return {};
        }
    }

    // Converts a death state to its equivalent animation group as long as it is a death state.
    std::string_view deathStateToAnimGroup(MWMechanics::CharacterState state)
    {
        using namespace MWMechanics;
        switch (state)
        {
            case CharState_SwimDeath:
                return "swimdeath";
            case CharState_SwimDeathKnockDown:
                return "swimdeathknockdown";
            case CharState_SwimDeathKnockOut:
                return "swimdeathknockout";
            case CharState_DeathKnockDown:
                return "deathknockdown";
            case CharState_DeathKnockOut:
                return "deathknockout";
            case CharState_Death1:
                return "death1";
            case CharState_Death2:
                return "death2";
            case CharState_Death3:
                return "death3";
            case CharState_Death4:
                return "death4";
            case CharState_Death5:
                return "death5";
            default:
                return {};
        }
    }

    // Converts a hit state to its equivalent animation group as long as it is a hit state.
    std::string hitStateToAnimGroup(MWMechanics::CharacterState state)
    {
        using namespace MWMechanics;
        switch (state)
        {
            case CharState_SwimHit:
                return "swimhit";
            case CharState_SwimKnockDown:
                return "swimknockdown";
            case CharState_SwimKnockOut:
                return "swimknockout";

            case CharState_Hit:
                return "hit";
            case CharState_KnockDown:
                return "knockdown";
            case CharState_KnockOut:
                return "knockout";

            case CharState_Block:
                return "shield";

            default:
                return {};
        }
    }

    // Converts an idle state to its equivalent animation group.
    std::string idleStateToAnimGroup(MWMechanics::CharacterState state)
    {
        using namespace MWMechanics;
        switch (state)
        {
            case CharState_IdleSwim:
                return "idleswim";
            case CharState_IdleSneak:
                return "idlesneak";
            case CharState_Idle:
            case CharState_SpecialIdle:
                return "idle";
            default:
                return {};
        }
    }

    MWRender::Animation::AnimPriority getIdlePriority(MWMechanics::CharacterState state)
    {
        using namespace MWMechanics;
        MWRender::Animation::AnimPriority priority(Priority_Default);
        switch (state)
        {
            case CharState_IdleSwim:
                return Priority_SwimIdle;
            case CharState_IdleSneak:
                priority[MWRender::BoneGroup_LowerBody] = Priority_SneakIdleLowerBody;
                [[fallthrough]];
            default:
                return priority;
        }
    }

    float getFallDamage(const MWWorld::Ptr& ptr, float fallHeight)
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();
        const MWWorld::Store<ESM::GameSetting>& store = world->getStore().get<ESM::GameSetting>();

        const float fallDistanceMin = store.find("fFallDamageDistanceMin")->mValue.getFloat();

        if (fallHeight >= fallDistanceMin)
        {
            const float acrobaticsSkill = static_cast<float>(ptr.getClass().getSkill(ptr, ESM::Skill::Acrobatics));
            const float jumpSpellBonus = ptr.getClass()
                                             .getCreatureStats(ptr)
                                             .getMagicEffects()
                                             .getOrDefault(ESM::MagicEffect::Jump)
                                             .getMagnitude();
            const float fallAcroBase = store.find("fFallAcroBase")->mValue.getFloat();
            const float fallAcroMult = store.find("fFallAcroMult")->mValue.getFloat();
            const float fallDistanceBase = store.find("fFallDistanceBase")->mValue.getFloat();
            const float fallDistanceMult = store.find("fFallDistanceMult")->mValue.getFloat();

            float x = fallHeight - fallDistanceMin;
            x -= (1.5f * acrobaticsSkill) + jumpSpellBonus;
            x = std::max(0.0f, x);

            float a = fallAcroBase + fallAcroMult * (100 - acrobaticsSkill);
            x = fallDistanceBase + fallDistanceMult * x;
            x *= a;

            return x;
        }
        return 0.f;
    }

    bool isRealWeapon(int weaponType)
    {
        return weaponType != ESM::Weapon::HandToHand && weaponType != ESM::Weapon::Spell
            && weaponType != ESM::Weapon::None;
    }

}

namespace MWMechanics
{

    std::string CharacterController::chooseRandomGroup(const std::string& prefix, int* num) const
    {
        auto& prng = MWBase::Environment::get().getWorld()->getPrng();

        int numAnims = 0;
        while (mAnimation->hasAnimation(prefix + std::to_string(numAnims + 1)))
            ++numAnims;

        int roll = Misc::Rng::rollDice(numAnims, prng) + 1; // [1, numAnims]
        if (num)
            *num = roll;
        return prefix + std::to_string(roll);
    }

    void CharacterController::clearStateAnimation(std::string& anim) const
    {
        if (anim.empty())
            return;
        if (mAnimation)
            mAnimation->disable(anim);
        anim.clear();
    }

    void CharacterController::resetCurrentJumpState()
    {
        clearStateAnimation(mCurrentJump);
        mJumpState = JumpState_None;
    }

    void CharacterController::resetCurrentMovementState()
    {
        clearStateAnimation(mCurrentMovement);
        mMovementState = CharState_None;
        mMovementAnimationHasMovement = false;
    }

    void CharacterController::resetCurrentIdleState()
    {
        clearStateAnimation(mCurrentIdle);
        mIdleState = CharState_None;
    }

    void CharacterController::resetCurrentHitState()
    {
        clearStateAnimation(mCurrentHit);
        mHitState = CharState_None;
    }

    void CharacterController::resetCurrentWeaponState()
    {
        clearStateAnimation(mCurrentWeapon);
        mUpperBodyState = UpperBodyState::None;
    }

    void CharacterController::resetCurrentDeathState()
    {
        clearStateAnimation(mCurrentDeath);
        mDeathState = CharState_None;
    }

    void CharacterController::refreshHitRecoilAnims()
    {
        auto& charClass = mPtr.getClass();
        if (!charClass.isActor())
            return;
        const auto world = MWBase::Environment::get().getWorld();
        auto& stats = charClass.getCreatureStats(mPtr);
        bool knockout = stats.isFatigueKnockedOut();
        bool recovery = stats.getHitRecovery();
        bool knockdown = stats.getKnockedDown();
        bool block = stats.getBlock() && !knockout && !recovery && !knockdown;
        bool isSwimming = world->isSwimming(mPtr);

        stats.setBlock(false);

        if (mPtr == getPlayer() && mHitState == CharState_Block && block)
        {
            mHitState = CharState_None;
            resetCurrentIdleState();
        }

        if (!mPtr.getClass().isNpc() && mUpperBodyState > UpperBodyState::WeaponEquipped)
        {
            recovery = false;
            stats.setHitRecovery(false);
        }

        if (mHitState != CharState_None)
        {
            if (!mAnimation->isPlaying(mCurrentHit))
            {
                if (isKnockedOut() && mCurrentHit.empty() && knockout)
                    return;

                mHitState = CharState_None;
                mCurrentHit.clear();
                stats.setKnockedDown(false);
                stats.setHitRecovery(false);
                resetCurrentIdleState();
            }
            else if (isKnockedOut())
                mAnimation->setLoopingEnabled(mCurrentHit, knockout);
            return;
        }

        if (!knockout && !knockdown && !recovery && !block)
            return;

        MWRender::Animation::AnimPriority priority(Priority_Knockdown);
        std::string_view startKey = "start";
        std::string_view stopKey = "stop";
        if (knockout)
        {
            mHitState = isSwimming ? CharState_SwimKnockOut : CharState_KnockOut;
            stats.setKnockedDown(true);
        }
        else if (knockdown)
        {
            mHitState = isSwimming ? CharState_SwimKnockDown : CharState_KnockDown;
        }
        else if (recovery)
        {
            mHitState = isSwimming ? CharState_SwimHit : CharState_Hit;
            priority = Priority_Hit;
        }
        else if (block)
        {
            mHitState = CharState_Block;
            priority = Priority_Hit;
            priority[MWRender::BoneGroup_LeftArm] = Priority_Block;
            priority[MWRender::BoneGroup_LowerBody] = Priority_WeaponLowerBody;
            startKey = "block start";
            stopKey = "block stop";
        }

        mCurrentHit = hitStateToAnimGroup(mHitState);

        if (isRecovery())
        {
            mCurrentHit = chooseRandomGroup(mCurrentHit);
            if (mHitState == CharState_SwimHit && !mAnimation->hasAnimation(mCurrentHit))
                mCurrentHit = chooseRandomGroup(hitStateToAnimGroup(CharState_Hit));
        }

        // Cancel upper body animations
        if (isKnockedOut() || isKnockedDown())
        {
            if (!mCurrentWeapon.empty())
                mAnimation->disable(mCurrentWeapon);
            if (mUpperBodyState > UpperBodyState::WeaponEquipped)
            {
                mUpperBodyState = UpperBodyState::WeaponEquipped;
                if (mWeaponType > ESM::Weapon::None)
                    mAnimation->showWeapons(true);
            }
            else if (mUpperBodyState < UpperBodyState::WeaponEquipped)
            {
                mUpperBodyState = UpperBodyState::None;
            }
        }

        if (!mAnimation->hasAnimation(mCurrentHit))
        {
            mCurrentHit.clear();
            return;
        }

        playBlendedAnimation(mCurrentHit, priority, MWRender::BlendMask_All, true, 1, startKey, stopKey, 0.0f,
            std::numeric_limits<uint32_t>::max());
    }

    void CharacterController::refreshJumpAnims(JumpingState jump, bool force)
    {
        if (!force && jump == mJumpState)
            return;

        if (jump == JumpState_None)
        {
            if (!mCurrentJump.empty())
                resetCurrentIdleState();
            resetCurrentJumpState();
            return;
        }

        std::string_view weapShortGroup = getWeaponShortGroup(mWeaponType);
        std::string jumpAnimName = "jump";
        jumpAnimName += weapShortGroup;
        MWRender::Animation::BlendMask jumpmask = MWRender::BlendMask_All;
        if (!weapShortGroup.empty() && !mAnimation->hasAnimation(jumpAnimName))
            jumpAnimName = fallbackShortWeaponGroup("jump", &jumpmask);

        if (!mAnimation->hasAnimation(jumpAnimName))
        {
            if (!mCurrentJump.empty())
                resetCurrentIdleState();
            resetCurrentJumpState();
            return;
        }

        bool startAtLoop = (jump == mJumpState);
        mJumpState = jump;
        clearStateAnimation(mCurrentJump);

        mCurrentJump = jumpAnimName;
        if (mJumpState == JumpState_InAir)
            playBlendedAnimation(jumpAnimName, Priority_Jump, jumpmask, false, 1.0f,
                startAtLoop ? "loop start" : "start", "stop", 0.f, std::numeric_limits<uint32_t>::max());
        else if (mJumpState == JumpState_Landing)
            playBlendedAnimation(jumpAnimName, Priority_Jump, jumpmask, true, 1.0f, "loop stop", "stop", 0.0f, 0);
    }

    bool CharacterController::onOpen() const
    {
        if (mPtr.getType() == ESM::Container::sRecordId && mAnimation)
        {
            if (!mAnimation->hasAnimation("containeropen"))
                return true;

            if (mAnimation->isPlaying("containeropen"))
                return false;

            if (mAnimation->isPlaying("containerclose"))
                return false;

            mAnimation->play(
                "containeropen", Priority_Scripted, MWRender::BlendMask_All, false, 1.0f, "start", "stop", 0.f, 0);
            if (mAnimation->isPlaying("containeropen"))
                return false;
        }

        return true;
    }

    void CharacterController::onClose() const
    {
        if (mPtr.getType() == ESM::Container::sRecordId)
        {
            if (!mAnimation || !mAnimation->hasAnimation("containerclose"))
                return;

            float complete, startPoint = 0.f;
            bool animPlaying = mAnimation->getInfo("containeropen", &complete);
            if (animPlaying)
                startPoint = 1.f - complete;

            mAnimation->play("containerclose", Priority_Scripted, MWRender::BlendMask_All, false, 1.0f, "start", "stop",
                startPoint, 0);
        }
    }

    std::string_view CharacterController::getWeaponAnimation(int weaponType) const
    {
        std::string_view weaponGroup = getWeaponType(weaponType)->mLongGroup;
        if (isRealWeapon(weaponType) && !mAnimation->hasAnimation(weaponGroup))
        {
            static const std::string_view oneHandFallback = getWeaponType(ESM::Weapon::LongBladeOneHand)->mLongGroup;
            static const std::string_view twoHandFallback = getWeaponType(ESM::Weapon::LongBladeTwoHand)->mLongGroup;

            const ESM::WeaponType* weapInfo = getWeaponType(weaponType);

            // For real two-handed melee weapons use 2h swords animations as fallback, otherwise use the 1h ones
            if (weapInfo->mFlags & ESM::WeaponType::TwoHanded && weapInfo->mWeaponClass == ESM::WeaponType::Melee)
                weaponGroup = twoHandFallback;
            else
                weaponGroup = oneHandFallback;
        }
        else if (weaponType == ESM::Weapon::HandToHand && !mPtr.getClass().isBipedal(mPtr))
            return "attack1";

        return weaponGroup;
    }

    std::string_view CharacterController::getWeaponShortGroup(int weaponType) const
    {
        if (weaponType == ESM::Weapon::HandToHand && !mPtr.getClass().isBipedal(mPtr))
            return {};
        return getWeaponType(weaponType)->mShortGroup;
    }

    std::string CharacterController::fallbackShortWeaponGroup(
        const std::string& baseGroupName, MWRender::Animation::BlendMask* blendMask) const
    {
        if (!isRealWeapon(mWeaponType))
        {
            if (blendMask != nullptr)
                *blendMask = MWRender::BlendMask_LowerBody;

            return baseGroupName;
        }

        static const std::string_view oneHandFallback = getWeaponShortGroup(ESM::Weapon::LongBladeOneHand);
        static const std::string_view twoHandFallback = getWeaponShortGroup(ESM::Weapon::LongBladeTwoHand);

        std::string groupName = baseGroupName;
        const ESM::WeaponType* weapInfo = getWeaponType(mWeaponType);

        // For real two-handed melee weapons use 2h swords animations as fallback, otherwise use the 1h ones
        if (weapInfo->mFlags & ESM::WeaponType::TwoHanded && weapInfo->mWeaponClass == ESM::WeaponType::Melee)
            groupName += twoHandFallback;
        else
            groupName += oneHandFallback;

        // Special case for crossbows - we should apply 1h animations a fallback only for lower body
        if (mWeaponType == ESM::Weapon::MarksmanCrossbow && blendMask != nullptr)
            *blendMask = MWRender::BlendMask_LowerBody;

        if (!mAnimation->hasAnimation(groupName))
        {
            groupName = baseGroupName;
            if (blendMask != nullptr)
                *blendMask = MWRender::BlendMask_LowerBody;
        }

        return groupName;
    }

    void CharacterController::refreshMovementAnims(CharacterState movement, bool force)
    {
        if (movement == mMovementState && !force)
            return;

        std::string_view movementAnimGroup = movementStateToAnimGroup(movement);

        if (movementAnimGroup.empty())
        {
            if (!mCurrentMovement.empty())
                resetCurrentIdleState();
            resetCurrentMovementState();
            return;
        }
        std::string movementAnimName{ movementAnimGroup };

        mMovementState = movement;
        std::string::size_type swimpos = movementAnimName.find("swim");
        if (!mAnimation->hasAnimation(movementAnimName))
        {
            if (swimpos != std::string::npos)
            {
                movementAnimName.erase(swimpos, 4);
                swimpos = std::string::npos;
            }
        }

        MWRender::Animation::BlendMask movemask = MWRender::BlendMask_All;

        std::string_view weapShortGroup = getWeaponShortGroup(mWeaponType);

        // Non-biped creatures don't use spellcasting-specific movement animations.
        if (!isRealWeapon(mWeaponType) && !mPtr.getClass().isBipedal(mPtr))
            weapShortGroup = {};

        if (swimpos == std::string::npos && !weapShortGroup.empty())
        {
            std::string weapMovementAnimName;
            // Spellcasting stance turning is a special case
            if (mWeaponType == ESM::Weapon::Spell && isTurning())
            {
                weapMovementAnimName = weapShortGroup;
                weapMovementAnimName += movementAnimName;
            }
            else
            {
                weapMovementAnimName = movementAnimName;
                weapMovementAnimName += weapShortGroup;
            }

            if (!mAnimation->hasAnimation(weapMovementAnimName))
                weapMovementAnimName = fallbackShortWeaponGroup(movementAnimName, &movemask);

            movementAnimName = std::move(weapMovementAnimName);
        }

        if (!mAnimation->hasAnimation(movementAnimName))
        {
            std::string::size_type runpos = movementAnimName.find("run");
            if (runpos != std::string::npos)
                movementAnimName.replace(runpos, 3, "walk");

            if (!mAnimation->hasAnimation(movementAnimName))
            {
                if (!mCurrentMovement.empty())
                    resetCurrentIdleState();
                resetCurrentMovementState();
                return;
            }
        }

        // If we're playing the same animation, start it from the point it ended
        float startpoint = 0.f;
        if (!mCurrentMovement.empty() && movementAnimName == mCurrentMovement)
            mAnimation->getInfo(mCurrentMovement, &startpoint);

        mMovementAnimationHasMovement = true;

        clearStateAnimation(mCurrentMovement);
        mCurrentMovement = std::move(movementAnimName);

        // For non-flying creatures, MW uses the Walk animation to calculate the animation velocity
        // even if we are running. This must be replicated, otherwise the observed speed would differ drastically.
        mAdjustMovementAnimSpeed = true;
        if (mPtr.getClass().getType() == ESM::Creature::sRecordId
            && !(mPtr.get<ESM::Creature>()->mBase->mFlags & ESM::Creature::Flies))
        {
            CharacterState walkState = runStateToWalkState(mMovementState);
            std::string_view anim = movementStateToAnimGroup(walkState);

            mMovementAnimSpeed = mAnimation->getVelocity(anim);
            if (mMovementAnimSpeed <= 1.0f)
            {
                // Another bug: when using a fallback animation (e.g. RunForward as fallback to SwimRunForward),
                // then the equivalent Walk animation will not use a fallback, and if that animation doesn't exist
                // we will play without any scaling.
                // Makes the speed attribute of most water creatures totally useless.
                // And again, this can not be fixed without patching game data.
                mAdjustMovementAnimSpeed = false;
                mMovementAnimSpeed = 1.f;
            }
        }
        else
        {
            mMovementAnimSpeed = mAnimation->getVelocity(mCurrentMovement);

            if (mMovementAnimSpeed <= 1.0f)
            {
                // The first person anims don't have any velocity to calculate a speed multiplier from.
                // We use the third person velocities instead.
                // FIXME: should be pulled from the actual animation, but it is not presently loaded.
                bool sneaking = mMovementState == CharState_SneakForward || mMovementState == CharState_SneakBack
                    || mMovementState == CharState_SneakLeft || mMovementState == CharState_SneakRight;
                mMovementAnimSpeed = (sneaking ? 33.5452f : (isRunning() ? 222.857f : 154.064f));
                mMovementAnimationHasMovement = false;
            }
        }

        playBlendedAnimation(mCurrentMovement, Priority_Movement, movemask, false, 1.f, "start", "stop", startpoint,
            std::numeric_limits<uint32_t>::max(), true);
    }

    void CharacterController::refreshIdleAnims(CharacterState idle, bool force)
    {
        // FIXME: if one of the below states is close to their last animation frame (i.e. will be disabled in the coming
        // update), the idle animation should be displayed
        if (((mUpperBodyState != UpperBodyState::None && mUpperBodyState != UpperBodyState::WeaponEquipped)
                || mMovementState != CharState_None || !mCurrentHit.empty())
            && !mPtr.getClass().isBipedal(mPtr))
        {
            resetCurrentIdleState();
            return;
        }

        std::string nativeIdle;
        if (idle == CharState_Idle && mPtr.getClass().isNpc()
            && MWBase::Environment::get().getWorld()->getGameProfile() == ESM::GameProfile::Oblivion)
        {
            switch (mWeaponType)
            {
                case ESM::Weapon::HandToHand: nativeIdle = "handtohandidle"; break;
                case ESM::Weapon::LongBladeOneHand: nativeIdle = "onehandidle"; break;
                case ESM::Weapon::LongBladeTwoHand: nativeIdle = "twohandidle"; break;
                default: nativeIdle = "idle"; break;
            }
            const auto* metadata = mAnimation->getControllerSequenceMetadata(nativeIdle);
            if (!metadata || metadata->mCycleType != 0 || !mAnimation->hasAnimation(nativeIdle))
                nativeIdle.clear();
        }
        if (!force && idle == mIdleState && (mAnimation->isPlaying(mCurrentIdle) || !mAnimQueue.empty())
            && (nativeIdle.empty() || nativeIdle == mCurrentIdle))
            return;

        mIdleState = idle;

        std::string idleGroup = nativeIdle.empty() ? idleStateToAnimGroup(mIdleState) : nativeIdle;
        if (idleGroup.empty())
        {
            resetCurrentIdleState();
            return;
        }

        MWRender::Animation::AnimPriority priority = getIdlePriority(mIdleState);
        size_t numLoops = std::numeric_limits<uint32_t>::max();

        // Only play "idleswim" or "idlesneak" if they exist. Otherwise, fallback to
        // "idle"+weapon or "idle".
        bool fallback = mIdleState != CharState_Idle && !mAnimation->hasAnimation(idleGroup);
        if (fallback)
        {
            priority = getIdlePriority(CharState_Idle);
            idleGroup = idleStateToAnimGroup(CharState_Idle);
        }

        if (nativeIdle.empty() && (fallback || mIdleState == CharState_Idle || mIdleState == CharState_SpecialIdle))
        {
            std::string_view weapShortGroup = getWeaponShortGroup(mWeaponType);
            if (!weapShortGroup.empty())
            {
                std::string weapIdleGroup = idleGroup;
                weapIdleGroup += weapShortGroup;
                if (!mAnimation->hasAnimation(weapIdleGroup))
                    weapIdleGroup = fallbackShortWeaponGroup(idleGroup);
                idleGroup = std::move(weapIdleGroup);

                // play until the Loop Stop key 2 to 5 times, then play until the Stop key
                // this replicates original engine behavior for the "Idle1h" 1st-person animation
                auto& prng = MWBase::Environment::get().getWorld()->getPrng();
                numLoops = 1 + Misc::Rng::rollDice(4, prng);
            }
        }

        if (!mAnimation->hasAnimation(idleGroup))
        {
            resetCurrentIdleState();
            return;
        }

        float startPoint = 0.f;
        // There is no need to restart anim if the new and old anims are the same.
        // Just update the number of loops.
        if (mCurrentIdle == idleGroup)
            mAnimation->getInfo(mCurrentIdle, &startPoint);

        clearStateAnimation(mCurrentIdle);
        mCurrentIdle = std::move(idleGroup);
        playBlendedAnimation(mCurrentIdle, priority, MWRender::BlendMask_All, false, 1.0f, "start", "stop", startPoint,
            static_cast<uint32_t>(numLoops), true);
    }

    void CharacterController::refreshCurrentAnims(
        CharacterState idle, CharacterState movement, JumpingState jump, bool force)
    {
        // If the current animation is scripted, do not touch it
        if (isScriptedAnimPlaying())
            return;

        refreshHitRecoilAnims();
        refreshJumpAnims(jump, force);
        refreshMovementAnims(movement, force);

        // idle handled last as it can depend on the other states
        refreshIdleAnims(idle, force);
    }

    void CharacterController::playDeath(float startpoint, CharacterState death)
    {
        mDeathState = death;
        mCurrentDeath = deathStateToAnimGroup(mDeathState);
        mPtr.getClass().getCreatureStats(mPtr).setDeathAnimation(
            static_cast<signed char>(mDeathState - CharState_Death1));

        // For dead actors, refreshCurrentAnims is no longer called, so we need to disable the movement state manually.
        // Note that these animations wouldn't actually be visible (due to the Death animation's priority being higher).
        // However, they could still trigger text keys, such as Hit events, or sounds.
        resetCurrentMovementState();
        resetCurrentWeaponState();
        resetCurrentHitState();
        resetCurrentIdleState();
        resetCurrentJumpState();

        playBlendedAnimation(
            mCurrentDeath, Priority_Death, MWRender::BlendMask_All, false, 1.0f, "start", "stop", startpoint, 0);
    }

    CharacterState CharacterController::chooseRandomDeathState() const
    {
        int selected = 0;
        chooseRandomGroup("death", &selected);
        return static_cast<CharacterState>(CharState_Death1 + (selected - 1));
    }

    void CharacterController::playRandomDeath(float startpoint)
    {
        if (mPtr == getPlayer())
        {
            // The first-person animations do not include death, so we need to
            // force-switch to third person before playing the death animation.
            MWBase::Environment::get().getWorld()->useDeathCamera();
        }

        mDeathState = hitStateToDeathState(mHitState);
        if (mDeathState == CharState_None && MWBase::Environment::get().getWorld()->isSwimming(mPtr))
            mDeathState = CharState_SwimDeath;

        if (mDeathState == CharState_None
            || (mAnimation && !mAnimation->hasAnimation(deathStateToAnimGroup(mDeathState))))
            mDeathState = chooseRandomDeathState();

        // Do not interrupt scripted animation by death
        if (!mAnimation || isScriptedAnimPlaying())
            return;

        playDeath(startpoint, mDeathState);
    }

    std::string CharacterController::chooseRandomAttackAnimation() const
    {
        std::string result;
        bool isSwimming = MWBase::Environment::get().getWorld()->isSwimming(mPtr);

        if (isSwimming)
            result = chooseRandomGroup("swimattack");

        if (!isSwimming || !mAnimation->hasAnimation(result))
            result = chooseRandomGroup("attack");

        return result;
    }

    CharacterController::CharacterController(const MWWorld::Ptr& ptr, MWRender::Animation& anim)
        : mPtr(ptr)
        , mAnimation(&anim)
    {
        mAnimation->setTextKeyListener(this);

        const MWWorld::Class& cls = mPtr.getClass();
        if (cls.isActor())
        {
            /* Accumulate along X/Y only for now, until we can figure out how we should
             * handle knockout and death which moves the character down. */
            mAnimation->setAccumulation(osg::Vec3f(1.0f, 1.0f, 0.0f));

            if (cls.hasInventoryStore(mPtr))
            {
                getActiveWeapon(mPtr, &mWeaponType);
                if (mWeaponType != ESM::Weapon::None)
                {
                    mUpperBodyState = UpperBodyState::WeaponEquipped;
                    mCurrentWeapon = getWeaponAnimation(mWeaponType);
                }

                if (mWeaponType != ESM::Weapon::None && mWeaponType != ESM::Weapon::Spell
                    && mWeaponType != ESM::Weapon::HandToHand)
                {
                    mAnimation->showWeapons(true);
                    // Note: controllers for ranged weapon should use time for beginning of animation to play shooting
                    // properly, for other weapons they should use absolute time. Some mods rely on this behaviour (to
                    // rotate throwing projectiles, for example)
                    ESM::WeaponType::Class weaponClass = getWeaponType(mWeaponType)->mWeaponClass;
                    bool useRelativeDuration = weaponClass == ESM::WeaponType::Ranged;
                    mAnimation->setWeaponGroup(mCurrentWeapon, useRelativeDuration);
                }

                mAnimation->showCarriedLeft(updateCarriedLeftVisible(mWeaponType));
            }

            if (!cls.getCreatureStats(mPtr).isDead())
            {
                mIdleState = CharState_Idle;
                if (cls.getCreatureStats(mPtr).getFallHeight() > 0)
                    mJumpState = JumpState_InAir;
            }
            else
            {
                const MWMechanics::CreatureStats& cStats = mPtr.getClass().getCreatureStats(mPtr);
                if (cStats.isDeathAnimationFinished())
                {
                    // Set the death state, but don't play it yet
                    // We will play it in the first frame, but only if no script set the skipAnim flag
                    signed char deathanim = cStats.getDeathAnimation();
                    if (deathanim == -1)
                        mDeathState = chooseRandomDeathState();
                    else
                        mDeathState = static_cast<CharacterState>(CharState_Death1 + deathanim);

                    mFloatToSurface = cStats.getHealth().getBase() != 0;
                }
                // else: nothing to do, will detect death in the next frame and start playing death animation
            }
        }
        else
        {
            /* Don't accumulate with non-actors. */
            mAnimation->setAccumulation(osg::Vec3f(0.f, 0.f, 0.f));

            mIdleState = CharState_Idle;
        }

        // Do not update animation status for dead actors
        if (mDeathState == CharState_None && (!cls.isActor() || !cls.getCreatureStats(mPtr).isDead()))
            refreshCurrentAnims(mIdleState, mMovementState, mJumpState, true);

        mAnimation->runAnimation(0.f);

        unpersistAnimationState();
    }

    CharacterController::~CharacterController()
    {
        detachAnimation();
    }

    void CharacterController::detachAnimation()
    {
        if (mAnimation)
        {
            persistAnimationState();
            mAnimation->clearTextKeyListener(this);
            mAnimation = nullptr;
        }
    }

    void CharacterController::abandonAnimation() noexcept
    {
        if (mAnimation) mAnimation->clearTextKeyListener(this);
        mAnimation = nullptr;
    }

    void CharacterController::handleTextKey(
        std::string_view groupname, SceneUtil::TextKeyMap::ConstIterator key, const SceneUtil::TextKeyMap& map)
    {
        if (!mAnimation)
            return;
        std::string_view evt = key->second;

        persistOblivionMeleeProgress();
        MWBase::Environment::get().getLuaManager()->animationTextKey(mPtr, key->second);

        if (MWBase::Environment::get().getWorld()->getGameProfile() == ESM::GameProfile::Oblivion
            && (evt == "hit" || evt.starts_with("a:") || evt == std::string(groupname) + ": hit"))
        {
            // Native contact dispatch is implemented separately from playback.
            // These stock keys must not invoke TES3 evaluateHit/hit or wind-up.
            return;
        }

        if (MWBase::Environment::get().getWorld()->getGameProfile() == ESM::GameProfile::Oblivion
            && Misc::StringUtils::ciStartsWith(evt, "sound: "))
        {
            const auto* sound = MWSound::resolveNativeAnimationSound(
                MWBase::Environment::get().getWorld()->getStore(), evt.substr(7));
            if (sound)
                MWBase::Environment::get().getSoundManager()->playSound3D(mPtr, sound->mId, 1.f, 1.f);
            return;
        }

        if (evt.substr(0, 7) == "sound: ")
        {
            MWBase::SoundManager* sndMgr = MWBase::Environment::get().getSoundManager();
            sndMgr->playSound3D(mPtr, ESM::RefId::stringRefId(evt.substr(7)), 1.0f, 1.0f);
            return;
        }

        auto& charClass = mPtr.getClass();
        if (evt.substr(0, 10) == "soundgen: ")
        {
            std::string_view soundgen = evt.substr(10);

            // The event can optionally contain volume and pitch modifiers
            float volume = 1.0f;
            float pitch = 1.0f;

            if (soundgen.find(' ') != std::string::npos)
            {
                std::vector<std::string_view> tokens;
                Misc::StringUtils::split(soundgen, tokens);
                soundgen = tokens[0];

                if (tokens.size() >= 2)
                {
                    volume = Misc::StringUtils::toNumeric<float>(tokens[1], volume);
                }

                if (tokens.size() >= 3)
                {
                    pitch = Misc::StringUtils::toNumeric<float>(tokens[2], pitch);
                }
            }

            const ESM::RefId sound = charClass.getSoundIdFromSndGen(mPtr, soundgen);
            if (!sound.empty())
            {
                MWBase::SoundManager* sndMgr = MWBase::Environment::get().getSoundManager();
                if (soundgen == "left" || soundgen == "right")
                {
                    if (!sndMgr->getSoundPlaying(mPtr, wolfRun))
                        sndMgr->playSound3D(
                            mPtr, sound, volume, pitch, MWSound::Type::Foot, MWSound::PlayMode::NoPlayerLocal);
                }
                else
                {
                    sndMgr->playSound3D(mPtr, sound, volume, pitch);
                }
            }
            return;
        }

        if (evt.substr(0, groupname.size()) != groupname || evt.substr(groupname.size(), 2) != ": ")
        {
            // Not ours, skip it
            return;
        }

        std::string_view action = evt.substr(groupname.size() + 2);
        if (action == "equip attach")
        {
            if (groupname == "shield")
                mAnimation->showCarriedLeft(true);
            else if (mUpperBodyState == UpperBodyState::Equipping)
                mAnimation->showWeapons(true);
        }
        else if (action == "unequip detach")
        {
            if (groupname == "shield")
                mAnimation->showCarriedLeft(false);
            else if (mUpperBodyState == UpperBodyState::Unequipping)
                mAnimation->showWeapons(false);
        }
        else if (action == "chop hit" || action == "slash hit" || action == "thrust hit" || action == "hit")
        {
            int attackType = -1;
            if (action == "hit")
            {
                if (groupname == "attack1" || groupname == "swimattack1")
                    attackType = ESM::Weapon::AT_Chop;
                else if (groupname == "attack2" || groupname == "swimattack2")
                    attackType = ESM::Weapon::AT_Slash;
                else if (groupname == "attack3" || groupname == "swimattack3")
                    attackType = ESM::Weapon::AT_Thrust;
            }
            else if (action == "chop hit")
                attackType = ESM::Weapon::AT_Chop;
            else if (action == "slash hit")
                attackType = ESM::Weapon::AT_Slash;
            else if (action == "thrust hit")
                attackType = ESM::Weapon::AT_Thrust;
            // We want to avoid hit keys that come out of nowhere (e.g. in the follow animation)
            // and processing multiple hit keys for a single attack
            if (mReadyToHit)
            {
                charClass.hit(
                    mPtr, mAttackStrength, mAttackWindUp, attackType, mAttackVictim, mAttackHitPos, mAttackSuccess);
                mReadyToHit = false;
            }
        }
        else if (isRandomAttackAnimation(groupname) && action == "start")
        {
            std::multimap<float, std::string>::const_iterator hitKey = key;

            // Not all animations have a hit key defined. If there is none, the hit happens with the start key.
            bool hasHitKey = false;
            while (hitKey != map.end())
            {
                if (hitKey->second.starts_with(groupname))
                {
                    std::string_view suffix = std::string_view(hitKey->second).substr(groupname.size());
                    if (suffix == ": hit")
                    {
                        hasHitKey = true;
                        break;
                    }
                    if (suffix == ": stop")
                        break;
                }
                ++hitKey;
            }
            if (!hasHitKey)
            {
                // State update doesn't expect the start key to be the hit key,
                // so we have to do this early.
                prepareHit();

                if (groupname == "attack1" || groupname == "swimattack1")
                    charClass.hit(mPtr, mAttackStrength, mAttackWindUp, ESM::Weapon::AT_Chop, mAttackVictim,
                        mAttackHitPos, mAttackSuccess);
                else if (groupname == "attack2" || groupname == "swimattack2")
                    charClass.hit(mPtr, mAttackStrength, mAttackWindUp, ESM::Weapon::AT_Slash, mAttackVictim,
                        mAttackHitPos, mAttackSuccess);
                else if (groupname == "attack3" || groupname == "swimattack3")
                    charClass.hit(mPtr, mAttackStrength, mAttackWindUp, ESM::Weapon::AT_Thrust, mAttackVictim,
                        mAttackHitPos, mAttackSuccess);
            }
        }
        else if (action == "shoot attach")
            mAnimation->attachArrow();
        else if (action == "shoot release")
        {
            // See notes for melee release above
            if (mReadyToHit)
            {
                mAnimation->releaseArrow(mAttackStrength, mAttackWindUp);
                mReadyToHit = false;
            }
        }
        else if (action == "shoot follow attach")
            mAnimation->attachArrow();
        // Make sure this key is actually for the RangeType we are casting. The flame atronach has
        // the same animation for all range types, so there are 3 "release" keys on the same time, one for each range
        // type.
        else if (groupname == "spellcast" && action == mAttackType + " release")
        {
            if (mCanCast)
                MWBase::Environment::get().getWorld()->castSpell(mPtr, mCastingScriptedSpell);
            mCastingScriptedSpell = false;
            mCanCast = false;
        }
        else if (groupname == "containeropen" && action == "loot")
            MWBase::Environment::get().getWindowManager()->pushGuiMode(MWGui::GM_Container, mPtr);
    }

    void CharacterController::updatePtr(const MWWorld::Ptr& ptr)
    {
        mPtr = ptr;
    }

    void CharacterController::updateIdleStormState(bool inwater) const
    {
        if (!mAnimation->hasAnimation("idlestorm"))
            return;

        bool animPlaying = mAnimation->isPlaying("idlestorm");
        if (mUpperBodyState != UpperBodyState::None || inwater)
        {
            if (animPlaying)
                mAnimation->disable("idlestorm");
            return;
        }

        const auto world = MWBase::Environment::get().getWorld();
        if (world->isInStorm())
        {
            osg::Vec3f stormDirection = world->getStormDirection();
            osg::Vec3f characterDirection = mPtr.getRefData().getBaseNode()->getAttitude() * osg::Vec3f(0, 1, 0);
            stormDirection.normalize();
            characterDirection.normalize();
            if (stormDirection * characterDirection < -0.5f)
            {
                if (!animPlaying)
                {
                    int mask = MWRender::BlendMask_Torso | MWRender::BlendMask_RightArm;
                    playBlendedAnimation("idlestorm", Priority_Storm, mask, true, 1.0f, "start", "stop", 0.0f,
                        std::numeric_limits<uint32_t>::max(), true);
                }
                else
                {
                    mAnimation->setLoopingEnabled("idlestorm", true);
                }
                return;
            }
        }

        if (animPlaying)
        {
            mAnimation->setLoopingEnabled("idlestorm", false);
        }
    }

    bool CharacterController::updateCarriedLeftVisible(const int weaptype) const
    {
        // Shields/torches shouldn't be visible during any operation involving two hands
        // There seems to be no text keys for this purpose, except maybe for "[un]equip start/stop",
        // but they are also present in weapon drawing animation.
        return mAnimation->updateCarriedLeftVisible(weaptype);
    }

    float CharacterController::calculateWindUp() const
    {
        if (!mAnimation || mCurrentWeapon.empty() || mWeaponType == ESM::Weapon::PickProbe
            || isRandomAttackAnimation(mCurrentWeapon))
            return -1.f;

        float minAttackTime = mAnimation->getTextKeyTime(mCurrentWeapon + ": " + mAttackType + " min attack");
        float maxAttackTime = mAnimation->getTextKeyTime(mCurrentWeapon + ": " + mAttackType + " max attack");
        if (minAttackTime == -1.f || minAttackTime >= maxAttackTime)
            return -1.f;

        return std::clamp(
            (mAnimation->getCurrentTime(mCurrentWeapon) - minAttackTime) / (maxAttackTime - minAttackTime), 0.f, 1.f);
    }

    void CharacterController::prepareHit()
    {
        if (mReadyToHit)
            return;

        auto& prng = MWBase::Environment::get().getWorld()->getPrng();
        mAttackStrength = calculateWindUp();
        mAttackWindUp = mAttackStrength;
        if (mAttackStrength == -1.f)
            mAttackStrength = std::min(1.f, 0.1f + Misc::Rng::rollClosedProbability(prng));
        ESM::WeaponType::Class weapclass = getWeaponType(mWeaponType)->mWeaponClass;
        if (weapclass != ESM::WeaponType::Ranged)
        {
            if (weapclass != ESM::WeaponType::Thrown)
            {
                mAttackSuccess = mPtr.getClass().evaluateHit(mPtr, mAttackVictim, mAttackHitPos);
                if (!mAttackSuccess)
                    mAttackStrength = 0.f;
            }
            playSwishSound();
        }

        mReadyToHit = true;
    }

    void CharacterController::persistOblivionMeleeProgress() const
    {
        auto* world = dynamic_cast<MWWorld::World*>(MWBase::Environment::get().getWorld().operator MWBase::World*());
        auto* service = world && world->getGameProfile() == ESM::GameProfile::Oblivion
            ? world->getOblivionCombatService() : nullptr;
        if (!service || !mAnimation || !mOblivionRenderedStrike)
            return;
        const auto actor = mPtr == world->getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
            : mPtr.getCellRef().getFormKey();
        const auto* state = service->findMeleeState(actor);
        if (state && state->mStrike && state->mStrike->mActionId == mOblivionRenderedStrike)
        {
            // Native timing publishes its final frame target before callbacks.
            // Intermediate shared text-key positions are not its authority.
            if (state->mStrike->mSequenceTiming)
                return;
            const float time = mAnimation->getCurrentTime(state->mStrike->mAnimationGroup);
            if (std::isfinite(time) && time >= state->mStrike->mAnimationTime)
                service->updateMeleeAnimation(mOblivionRenderedStrike, actor, time);
        }
    }

    void CharacterController::dispatchOblivionMeleeContact()
    {
        auto* world = dynamic_cast<MWWorld::World*>(MWBase::Environment::get().getWorld().operator MWBase::World*());
        auto* service = world && world->getGameProfile() == ESM::GameProfile::Oblivion
            ? world->getOblivionCombatService() : nullptr;
        if (!service || !mOblivionRenderedStrike || mSkipAnim || !mAnimation)
            return;
        const auto actor = mPtr == world->getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
            : mPtr.getCellRef().getFormKey();
        if (!service->isMeleeContactPending(mOblivionRenderedStrike, actor))
            return;
        const auto strike = *service->findMeleeState(actor)->mStrike;
        float weaponWeight = 0;
        const ESM4::Weapon* weapon = nullptr;
        if (!mWeapon.isEmpty())
        {
            const auto id = MWWorld::OblivionProfileServices::nativeItemId(
                world->getStore(), mWeapon.getCellRef().getRefId());
            const auto* form = id.getIf<ESM::FormId>();
            weapon = form ? world->getStore().get<ESM4::Weapon>().search(*form) : nullptr;
            if (!weapon || weapon->mData.type > 3
                || ESM::FormKeyResolver(world->getContentFiles()).toFormKey(*form) != strike.mWeaponBase)
                return;
            weaponWeight = weapon->mData.weight;
        }
        else if (!strike.mWeaponBase.isNull())
            return;
        std::vector<const ESM4::GameSetting*> settings;
        std::set<ESM::FormId> seen;
        for (const auto& record : world->getStore().get<ESM4::GameSetting>())
            if (seen.insert(record.mId).second)
                settings.push_back(world->getStore().get<ESM4::GameSetting>().search(record.mId));
        const auto reachSettings = ESM4::buildMeleeReachSettings(settings);
        const float placed = mPtr.getCellRef().getScale();
        osg::Vec3f scale(placed, placed, placed);
        mPtr.getClass().adjustScale(mPtr, scale, true);
        const bool creature = mPtr.getType() == ESM::REC_CREA4;
        if (creature && weapon)
            return; // Armed Creature contact has a separate native multiplier path.
        const auto* natural = creature ? mPtr.get<ESM4::Creature>()->mBase : nullptr;
        if (natural && !natural->mAttackReach)
            return;
        const bool counter = strike.mKind == ESM4::MeleeStrikeKind::Counter;
        const float reach = counter ? ESM4::weaponMeleeReach(1.f, scale.z(), reachSettings)
            : weapon ? ESM4::weaponMeleeReach(weapon->mData.reach, scale.z(), reachSettings)
            : natural ? ESM4::creatureMeleeReach(*natural->mAttackReach,
                natural->mData.creatureType, scale.z(), reachSettings)
            : ESM4::unarmedMeleeReach(scale.z(), reachSettings);
        const float cost = counter ? 0.f : ESM4::attackFatigueCost(weaponWeight,
            !creature && strike.mKind >= ESM4::MeleeStrikeKind::StandingPower,
            ESM4::buildAttackFatigueSettings(settings));
        const auto fatigue = [&] { return mPtr == world->getPlayerPtr()
            ? service->getPlayerValue(10) : service->getNonPlayerValue(mPtr, 10); };
        const float before = fatigue();
        MWWorld::Ptr selectedTarget;
        if (const auto* melee = service->findMeleeState(actor); !counter && melee && melee->mAiIntent)
        {
            const auto key = melee->mAiIntent->mTarget;
            auto* ai = world->getOblivionAiService();
            selectedTarget = key == ESM::FormKey::dynamic("player", 1) ? world->getPlayerPtr()
                : ai ? ai->resolveReference(key) : MWWorld::Ptr{};
            if (selectedTarget.isEmpty())
                return; // An unavailable owned target must never enumerate substitutes.
        }
        const auto contact = commitOblivionMeleeContact(*world, strike.mActionId, mPtr, selectedTarget,
            reach, weaponWeight, oblivionNormalizedDifficulty(Settings::game().mDifficulty), isSneaking());
        if (contact && !contact->mVictim.isEmpty() && mPtr == world->getPlayerPtr())
        {
            if (auto* window = MWBase::Environment::get().getWindowManagerOrNull())
                window->setEnemy(contact->mVictim);
        }
        if (contact && contact->mVictim.isEmpty() && !counter)
        {
            // Stock TES4 melee KFs have no sound key. Emit after the contact
            // commits so repeated frames and restored completed strikes cannot replay it.
            // This uses the original Audio INI defaults; custom INI import is separate.
            const auto editorId = ESM4::nativeMeleeSwishSound(weapon
                ? std::optional<ESM4::WeaponSwishInput>{{ weapon->mData.weight, weapon->mData.speed }}
                : std::nullopt);
            if (const auto* sound = MWSound::resolveNativeAnimationSound(world->getStore(), editorId))
            {
                MWBase::Environment::get().getSoundManager()->playSound3D(mPtr, sound->mId, 1.f, 1.f);
                Log(Debug::Verbose) << "M15 melee swish requested: actor=" << actor.serialize()
                                    << " id=" << strike.mActionId << " sound=" << editorId;
            }
        }
        if (contact && contact->mHitSounds)
        {
            for (const auto editorId : *contact->mHitSounds)
            {
                if (editorId.empty())
                    continue;
                if (const auto* sound = MWSound::resolveNativeAnimationSound(world->getStore(), editorId))
                {
                    const auto soundManager = MWBase::Environment::get().getSoundManager();
                    const bool played = contact->mHitSoundPosition
                        ? soundManager->playSound3D(osg::Vec3f((*contact->mHitSoundPosition)[0],
                            (*contact->mHitSoundPosition)[1], (*contact->mHitSoundPosition)[2]), sound->mId, 1.f, 1.f)
                            != nullptr
                        : soundManager->playSound3D(contact->mVictim, sound->mId, 1.f, 1.f) != nullptr;
                    Log(Debug::Verbose) << "M15 melee hit sound requested: actor=" << actor.serialize()
                                        << " id=" << strike.mActionId << " sound=" << editorId
                                        << " played=" << played;
                }
            }
        }
        if (contact)
            Log(Debug::Verbose) << (contact->mVictim.isEmpty() ? "M15 melee miss committed: actor="
                : "M15 melee hit committed: actor=") << actor.serialize()
                                << " id=" << strike.mActionId << " fatigue_cost=" << cost
                                << " before=" << before << " after=" << fatigue()
                                << " action=" << service->getProcessAction(actor)
                                << " health_damage=" << contact->mDamage.mHealth
                                << " fatigue_damage=" << contact->mDamage.mFatigue
                                << " block_fraction=" << contact->mBlockAbsorbedFraction
                                << " block_fatigue=" << contact->mBlockFatigueDebit
                                << " weapon_condition_write=" << contact->mWeaponConditionAfterWear.value_or(-1.f)
                                << " victim=" << (contact->mVictim.isEmpty() ? "none"
                                    : contact->mVictim == world->getPlayerPtr() ? ESM::FormKey::dynamic("player", 1).serialize()
                                    : contact->mVictim.getCellRef().getFormKey().serialize())
                                << " victim_action=" << (contact->mVictim.isEmpty() ? -1
                                    : service->getProcessAction(contact->mVictim == world->getPlayerPtr()
                                        ? ESM::FormKey::dynamic("player", 1) : contact->mVictim.getCellRef().getFormKey()))
                                << " armor_condition_writes=" << contact->mArmorConditionWrites
                                << " combat_random_draws=" << contact->mRandomDraws;
    }

    bool CharacterController::bindOblivionMeleeReaction()
    {
        auto* world = dynamic_cast<MWWorld::World*>(MWBase::Environment::get().getWorld().operator MWBase::World*());
        auto* service = world && world->getGameProfile() == ESM::GameProfile::Oblivion
            ? world->getOblivionCombatService() : nullptr;
        if (!service || !mAnimation) return false;
        const auto actor = mPtr == world->getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
            : mPtr.getCellRef().getFormKey();
        const auto* state = service->findMeleeState(actor);
        const auto bodyReaction = state ? state->mReaction : std::nullopt;
        const auto firstReaction = state && state->mFirstPerson ? state->mFirstPerson->mReaction : std::nullopt;
        auto* peer = mAnimation->getPlaybackPeer();
        const auto firstStrike = state && state->mFirstPerson ? state->mFirstPerson->mStrike : std::nullopt;
        const bool firstFinished = firstStrike && (firstStrike->mKind == ESM4::MeleeStrikeKind::Counter
            ? firstStrike->mInactive : firstStrike->mKind >= ESM4::MeleeStrikeKind::StandingPower
                ? firstStrike->mPowerPhase == ESM4::PowerMeleePhase::End
                : firstStrike->mOrdinaryPhase == ESM4::OrdinaryMeleePhase::End);
        if (!peer && (firstStrike || (state && state->mFirstPerson
                && (state->mFirstPerson->mPassiveSequence || !state->mFirstPerson->mRetiringSequences.empty()))))
            throw std::invalid_argument("native first-person strike has no renderer view");
        if (peer && mOblivionFirstPersonRenderedStrike && (!firstStrike || firstFinished
                || firstStrike->mActionId != mOblivionFirstPersonRenderedStrike
                || firstStrike->mAnimationGroup != mOblivionFirstPersonStrikeGroup))
        {
            peer->disable(mOblivionFirstPersonStrikeGroup, MWRender::Animation::PlaybackScope::ThisView);
            mOblivionFirstPersonRenderedStrike = 0;
            mOblivionFirstPersonStrikeGroup.clear();
        }
        if (firstStrike && !firstFinished && (mOblivionFirstPersonRenderedStrike != firstStrike->mActionId
                || !peer->getInfo(firstStrike->mAnimationGroup)))
        {
            const auto playback = nativeMeleePlayback(*peer, firstStrike->mAnimationGroup,
                firstStrike->mKind >= ESM4::MeleeStrikeKind::StandingPower,
                firstStrike->mKind == ESM4::MeleeStrikeKind::Counter);
            if (!playback || firstStrike->mAnimationTime < playback->mTimelineBegin
                || firstStrike->mAnimationTime > playback->mTimelineEnd)
                throw std::invalid_argument("native first-person strike cannot bind saved metadata/time");
            const float fraction = (firstStrike->mAnimationTime - playback->mTimelineBegin)
                / (playback->mTimelineEnd - playback->mTimelineBegin);
            peer->play(firstStrike->mAnimationGroup, Priority_Weapon, MWRender::BlendMask_All,
                false, 0.f, "start", "stop", fraction, 0, false, MWRender::Animation::PlaybackScope::ThisView,
                firstStrike->mKind == ESM4::MeleeStrikeKind::Counter ? std::string_view(mOblivionBlockGroup)
                    : std::string_view{});
            mOblivionFirstPersonRenderedStrike = firstStrike->mActionId;
            mOblivionFirstPersonStrikeGroup = firstStrike->mAnimationGroup;
            Log(Debug::Verbose) << "M15 first-person strike bind: actor=" << actor.serialize()
                << " id=" << firstStrike->mActionId << " time=" << firstStrike->mAnimationTime;
        }
        if (firstReaction && !peer)
            throw std::invalid_argument("native first-person reaction has no renderer view");
        const std::string oldStrike = mOblivionRenderedStrike ? mCurrentWeapon : std::string{};
        const auto bind = [&](MWRender::Animation& view, const auto& reaction, std::uint64_t& rendered,
                              std::string& group, auto& pose, bool firstPerson) {
            if (rendered && (!reaction || reaction->mContactId != rendered || reaction->mAnimationGroup != group))
            {
                const auto old = std::exchange(group, {});
                rendered = 0;
                pose.reset();
                if (!old.empty()) view.disable(old, MWRender::Animation::PlaybackScope::ThisView);
            }
            if (!reaction || reaction->mInactive) return false;
            if (rendered == reaction->mContactId && view.getInfo(reaction->mAnimationGroup)) return true;
            (void)captureOblivionMeleeSequenceMetadata(view, reaction->mAnimationGroup, true);
            const float start = view.getTextKeyTimeInGroup(reaction->mAnimationGroup,
                reaction->mAnimationGroup + ": start");
            const float stop = view.getTextKeyTimeInGroup(reaction->mAnimationGroup,
                reaction->mAnimationGroup + ": stop");
            if (start < 0 || stop <= start || reaction->mAnimationTime > stop)
                throw std::invalid_argument("native reaction cannot bind its saved renderer time");
            if (!firstPerson)
            {
                mOblivionRenderedStrike = 0;
                mCurrentWeapon.clear();
                if (reaction->mKind != ESM4::MeleeReactionKind::BlockHit)
                {
                    const auto block = std::exchange(mOblivionBlockGroup, {});
                    if (!block.empty()) mAnimation->disable(block);
                }
            }
            if (!oldStrike.empty()) view.disable(oldStrike, MWRender::Animation::PlaybackScope::ThisView);
            rendered = reaction->mContactId;
            group = reaction->mAnimationGroup;
            pose = reaction->mPoseSnapshot
                ? std::make_shared<const ESM4::RuntimeMeleePoseSnapshot>(*reaction->mPoseSnapshot) : nullptr;
            const float fraction = (std::max(start, reaction->mAnimationTime) - start) / (stop - start);
            // Ordinary BlockIdle stays resident; native per-bone priorities
            // project each reaction from its own resource and captured pose.
            if (peer)
                view.play(group, Priority_Hit, MWRender::BlendMask_All,
                    false, 0.f, "start", "stop", fraction, 0, false, MWRender::Animation::PlaybackScope::ThisView);
            else
                playBlendedAnimation(group, Priority_Hit, MWRender::BlendMask_All,
                    false, 0.f, "start", "stop", fraction, 0);
            Log(Debug::Verbose) << "M15 reaction bind: actor=" << actor.serialize()
                                << " cause=" << reaction->mContactId << " group=" << reaction->mAnimationGroup
                                << " pose_tracks=" << (reaction->mPoseSnapshot ? reaction->mPoseSnapshot->mTracks.size() : 0)
                                << " view=" << (firstPerson ? "first" : "body");
            return true;
        };
        const bool body = bind(*mAnimation, bodyReaction, mOblivionRenderedReaction,
            mOblivionReactionGroup, mOblivionPoseSnapshot, false);
        const bool first = peer && bind(*peer, firstReaction, mOblivionFirstPersonRenderedReaction,
            mOblivionFirstPersonReactionGroup, mOblivionFirstPersonPoseSnapshot, true);
        return body || first || (firstStrike && !firstFinished);
    }

    void CharacterController::bindOblivionHeldBlock()
    {
        auto* world = dynamic_cast<MWWorld::World*>(MWBase::Environment::get().getWorld().operator MWBase::World*());
        auto* service = world ? world->getOblivionCombatService() : nullptr;
        if (!service || !mAnimation) return;
        const bool player = mPtr == world->getPlayerPtr();
        const auto actor = player ? ESM::FormKey::dynamic("player", 1) : mPtr.getCellRef().getFormKey();
        const auto* state = service->findMeleeState(actor);
        if (!state) return;
        const auto bind = [&](MWRender::Animation& view, const auto& held) {
            for (const auto& entry : held)
            {
                if (!entry.resident() || view.getInfo(entry.mAnimationGroup)) continue;
                const auto source = captureOblivionMeleeSequenceMetadata(view, entry.mAnimationGroup, false);
                if (source.mCycle != ESM4::MeleeSequenceCycle::Loop)
                    throw std::invalid_argument("held-block binding lost its cyclic source");
                const float fraction = (entry.mAnimationTime - source.mTimelineBegin)
                    / (source.mTimelineEnd - source.mTimelineBegin);
                if (!std::isfinite(fraction) || fraction < 0 || fraction > 1)
                    throw std::invalid_argument("held-block binding cannot map its saved sample");
                // The event track follows native clock projection. Its legacy
                // priority must not evict the independent slot3 event track.
                view.play(entry.mAnimationGroup, Priority_Block, MWRender::BlendMask_All, false,
                    0.f, "start", "stop", fraction, std::numeric_limits<std::uint32_t>::max(), true,
                    MWRender::Animation::PlaybackScope::ThisView);
                Log(Debug::Verbose) << "M15 held binding: actor=" << actor.serialize()
                    << " group=" << entry.mAnimationGroup << " time=" << entry.mAnimationTime;
            }
        };
        bind(*mAnimation, state->mHeldBlocks);
        if (state->mFirstPerson)
        {
            auto* peer = mAnimation->getPlaybackPeer();
            if (!peer && std::any_of(state->mFirstPerson->mHeldBlocks.begin(), state->mFirstPerson->mHeldBlocks.end(),
                    [](const auto& held) { return !held.mInactive; }))
                throw std::invalid_argument("held first-person cache has no renderer view");
            if (peer) bind(*peer, state->mFirstPerson->mHeldBlocks);
        }
        const auto& active = player && world->isFirstPerson() && state->mFirstPerson
            ? state->mFirstPerson->mHeldBlocks : state->mHeldBlocks;
        const auto resident = std::find_if(active.begin(), active.end(), [](const auto& entry) { return entry.resident(); });
        if (resident != active.end()) mOblivionBlockGroup = resident->mAnimationGroup;
    }

    bool CharacterController::advanceOblivionMeleeReactionFrame(float duration)
    {
        auto* world = dynamic_cast<MWWorld::World*>(MWBase::Environment::get().getWorld().operator MWBase::World*());
        auto* service = world && world->getGameProfile() == ESM::GameProfile::Oblivion
            ? world->getOblivionCombatService() : nullptr;
        if (!service || !mAnimation) return false;
        const auto actor = mPtr == world->getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
            : mPtr.getCellRef().getFormKey();
        const auto active = [](const auto* state) {
            return state && (state->mReaction || state->mPassiveSequence
                || (state->mStrike && (state->mStrike->mPoseSnapshot
                    || state->mStrike->mKind == ESM4::MeleeStrikeKind::Counter)) || !state->mRetiringSequences.empty()
                || std::any_of(state->mHeldBlocks.begin(), state->mHeldBlocks.end(), [](const auto& h) { return !h.mInactive; })
                || (state->mFirstPerson && (state->mFirstPerson->mReaction || state->mFirstPerson->mStrike
                    || state->mFirstPerson->mPassiveSequence || !state->mFirstPerson->mRetiringSequences.empty()
                    || std::any_of(state->mFirstPerson->mHeldBlocks.begin(), state->mFirstPerson->mHeldBlocks.end(),
                        [](const auto& h) { return !h.mInactive; }))));
        };
        const auto* state = service->findMeleeState(actor);
        auto* peer = mAnimation->getPlaybackPeer();
        if (!active(state))
        {
            if (mOblivionMeleeProjection) mAnimation->setNativeSequenceLayers({});
            if (peer && mOblivionFirstPersonMeleeProjection) peer->setNativeSequenceLayers({});
            mOblivionMeleeProjection = mOblivionFirstPersonMeleeProjection = false;
            mOblivionPassivePoseSnapshot.reset();
            mOblivionFirstPersonPassivePoseSnapshot.reset();
            mOblivionHeldPoseBindings.clear();
            mOblivionFirstPersonHeldPoseBindings.clear();
            return false;
        }
        bindOblivionMeleeReaction();
        bindOblivionHeldBlock();
        state = service->findMeleeState(actor);
        if (!active(state)) return false;
        std::array<std::set<std::string_view>, 2> groups;
        if (state->mReaction) groups[0].insert(state->mReaction->mAnimationGroup);
        if (state->mStrike) groups[0].insert(state->mStrike->mAnimationGroup);
        for (const auto& sequence : state->mRetiringSequences) groups[0].insert(sequence.mAnimationGroup);
        if (state->mFirstPerson && state->mFirstPerson->mReaction)
            groups[1].insert(state->mFirstPerson->mReaction->mAnimationGroup);
        if (state->mFirstPerson && state->mFirstPerson->mStrike)
        {
            groups[1].insert(state->mFirstPerson->mStrike->mAnimationGroup);
            if (state->mStrike) groups[0].insert(state->mStrike->mAnimationGroup);
        }
        if (state->mFirstPerson)
            for (const auto& sequence : state->mFirstPerson->mRetiringSequences)
                groups[1].insert(sequence.mAnimationGroup);
        if (state->mPassiveSequence) groups[0].insert(state->mPassiveSequence->group());
        if (state->mFirstPerson && state->mFirstPerson->mPassiveSequence)
            groups[1].insert(state->mFirstPerson->mPassiveSequence->group());
        for (const auto& held : state->mHeldBlocks) if (!held.mInactive) groups[0].insert(held.mAnimationGroup);
        if (state->mFirstPerson)
            for (const auto& held : state->mFirstPerson->mHeldBlocks)
                if (!held.mInactive) groups[1].insert(held.mAnimationGroup);
        std::vector<OblivionMeleeSequenceMetadata> metadata;
        for (unsigned view = 0; view < groups.size(); ++view)
            for (const auto group : groups[view])
            {
                const auto& reaction = view ? state->mFirstPerson->mReaction : state->mReaction;
                const auto& passive = view ? state->mFirstPerson->mPassiveSequence : state->mPassiveSequence;
                const auto* passiveReaction = passive
                    ? std::get_if<ESM4::RuntimeMeleeReaction>(&passive->mSequence) : nullptr;
                auto source = captureOblivionMeleeSequenceMetadata(view ? *peer : *mAnimation, group,
                    (reaction && reaction->mAnimationGroup == group)
                        || (passiveReaction && passiveReaction->mAnimationGroup == group));
                source.mFirstPerson = view != 0;
                const auto& strike = view ? state->mFirstPerson->mStrike : state->mStrike;
                const auto* passiveStrike = passive
                    ? std::get_if<ESM4::RuntimeMeleeStrike>(&passive->mSequence) : nullptr;
                const auto* advancingStrike = strike && strike->mAnimationGroup == group
                    && ((state->mFirstPerson && state->mFirstPerson->mStrike)
                        || strike->mKind == ESM4::MeleeStrikeKind::Counter) ? &*strike : passiveStrike;
                if (advancingStrike && advancingStrike->mAnimationGroup == group)
                {
                    const bool power = advancingStrike->mKind >= ESM4::MeleeStrikeKind::StandingPower;
                    const auto playback = nativeMeleePlayback(view ? *peer : *mAnimation, group, power,
                        advancingStrike->mKind == ESM4::MeleeStrikeKind::Counter);
                    if (!playback) throw std::invalid_argument("native owned strike metadata is missing");
                    source.mRetirementDuration = playback->retirementDuration();
                    if (power) source.mPowerKeys = playback->mPowerKeys;
                    else source.mOrdinaryKeys = playback->mKeys;
                }
                metadata.push_back(std::move(source));
            }
        auto prepared = service->prepareMeleeReactionFrame(actor, duration, metadata);
        if (!prepared) return false;
        const auto& next = prepared->state();
        const auto project = [&](MWRender::Animation& view, const auto& reaction,
                                 std::span<const ESM4::RuntimeRetiringMeleeSequence> retiring,
                                 const ESM4::RuntimeMeleeStrike* strike, float clock,
                                 const auto& pose, const auto& excludedGroups,
                                 const ESM4::RuntimePassiveMeleeSequence* passive, auto& passivePose,
                                 const auto& held, auto& heldPoses) {
            std::vector<MWRender::Animation::NativeSequenceLayer> layers;
            if ((!reaction || reaction->mInactive) && retiring.empty() && !strike && !passive
                && std::none_of(held.begin(), held.end(), [](const auto& h) { return !h.mInactive; }))
            {
                passivePose.reset();
                heldPoses.clear();
                return layers;
            }
            std::vector<std::string_view> excluded(excludedGroups.begin(), excludedGroups.end());
            if (strike) excluded.push_back(strike->mAnimationGroup);
            for (const auto& sequence : held) excluded.push_back(sequence.mAnimationGroup);
            const bool ownsSlot0 = reaction && reaction->mKind == ESM4::MeleeReactionKind::Stagger
                && !reaction->mRetirement && !reaction->mInactive;
            layers = view.nativePlayingSequenceLayers(excluded, ownsSlot0);
            const auto retirementEase = [&](const auto& span) {
                const float end = static_cast<float>(double(*span.mStartClock) + span.mDuration);
                return ESM4::sequenceBlendEase(clock, *span.mStartClock, end, true);
            };
            const auto activationEase = [&](const ESM4::MeleeSequenceTiming& timing) {
                return timing.mEasing ? ESM4::sequenceBlendEase(clock, *timing.mEaseStart, timing.mEaseEnd, false) : 1.f;
            };
            const auto append = [&](std::string_view group, float time, float ease) {
                const auto* source = view.getControllerSequenceMetadata(group);
                if (!source) throw std::invalid_argument("native melee layer lost its sequence metadata");
                layers.push_back({std::string(group), MWRender::BlendMask_All, time, source->mWeight, ease});
            };
            for (const auto& sequence : retiring)
                append(sequence.mAnimationGroup, sequence.mAnimationTime, retirementEase(sequence.mRetirement));
            auto passiveLayers = MWRender::oblivionPassiveMeleeLayers(view, passive, clock, passivePose);
            layers.insert(layers.end(), std::make_move_iterator(passiveLayers.begin()),
                std::make_move_iterator(passiveLayers.end()));
            auto heldLayers = MWRender::oblivionHeldBlockLayers(view, held, clock, heldPoses);
            layers.insert(layers.end(), std::make_move_iterator(heldLayers.begin()),
                std::make_move_iterator(heldLayers.end()));
            if (reaction && !reaction->mInactive)
            {
                if (reaction->mPoseSnapshot)
                {
                    if (!pose) throw std::invalid_argument("native reaction lost its captured pose binding");
                    const auto& timing = reaction->mSequenceTiming;
                    layers.push_back({reaction->mAnimationGroup, MWRender::BlendMask_All, 0, 1,
                        ESM4::sequenceBlendEase(clock, *timing.mEaseStart, timing.mEaseEnd, true), false, pose});
                }
                append(reaction->mAnimationGroup, reaction->mAnimationTime, reaction->mRetirement
                    ? retirementEase(*reaction->mRetirement) : activationEase(reaction->mSequenceTiming));
            }
            if (strike && !strike->mInactive)
            {
                if (strike->mPoseSnapshot)
                {
                    const auto& timing = *strike->mSequenceTiming;
                    auto captured = std::make_shared<const ESM4::RuntimeMeleePoseSnapshot>(*strike->mPoseSnapshot);
                    layers.push_back({strike->mAnimationGroup, MWRender::BlendMask_All, 0, 1,
                        ESM4::sequenceBlendEase(clock, *timing.mEaseStart, timing.mEaseEnd, true), false, captured});
                }
                append(strike->mAnimationGroup, strike->mAnimationTime,
                    strike->mRetirement ? retirementEase(*strike->mRetirement)
                        : strike->mSequenceTiming ? activationEase(*strike->mSequenceTiming) : 1.f);
            }
            return layers;
        };
        auto bodyLayers = project(*mAnimation, next.mReaction, next.mRetiringSequences,
            next.mStrike ? &*next.mStrike : nullptr, prepared->clock(), mOblivionPoseSnapshot, groups[0],
            next.mPassiveSequence ? &*next.mPassiveSequence : nullptr, mOblivionPassivePoseSnapshot,
            next.mHeldBlocks, mOblivionHeldPoseBindings);
        const auto firstReaction = next.mFirstPerson ? next.mFirstPerson->mReaction : std::nullopt;
        std::vector<MWRender::Animation::NativeSequenceLayer> firstLayers;
        const auto* firstStrike = next.mFirstPerson && next.mFirstPerson->mStrike ? &*next.mFirstPerson->mStrike : nullptr;
        if (firstStrike && (firstStrike->mKind == ESM4::MeleeStrikeKind::Counter ? firstStrike->mInactive
                : firstStrike->mKind >= ESM4::MeleeStrikeKind::StandingPower
                    ? firstStrike->mPowerPhase == ESM4::PowerMeleePhase::End
                    : firstStrike->mOrdinaryPhase == ESM4::OrdinaryMeleePhase::End)) firstStrike = nullptr;
        const bool firstRetiring = next.mFirstPerson && !next.mFirstPerson->mRetiringSequences.empty();
        const auto* firstPassive = next.mFirstPerson && next.mFirstPerson->mPassiveSequence
            ? &*next.mFirstPerson->mPassiveSequence : nullptr;
        const bool firstHeld = next.mFirstPerson && std::any_of(next.mFirstPerson->mHeldBlocks.begin(),
            next.mFirstPerson->mHeldBlocks.end(), [](const auto& held) { return !held.mInactive; });
        if (firstReaction || firstStrike || firstRetiring || firstPassive || firstHeld)
            firstLayers = project(*peer, firstReaction, next.mFirstPerson->mRetiringSequences, firstStrike, next.mFirstPerson->mClock,
                mOblivionFirstPersonPoseSnapshot, groups[1], firstPassive, mOblivionFirstPersonPassivePoseSnapshot,
                next.mFirstPerson->mHeldBlocks, mOblivionFirstPersonHeldPoseBindings);
        else
        {
            mOblivionFirstPersonPassivePoseSnapshot.reset();
            mOblivionFirstPersonHeldPoseBindings.clear();
        }
        const bool paired = peer && (firstReaction || firstStrike || firstRetiring || firstPassive
            || firstHeld || mOblivionFirstPersonMeleeProjection);
        if (!(paired ? mAnimation->setNativeSequenceLayersForViews(bodyLayers, firstLayers)
                     : mAnimation->setNativeSequenceLayers(bodyLayers)))
            throw std::invalid_argument("native melee sequence projection could not bind");
        mOblivionMeleeProjection = !bodyLayers.empty();
        mOblivionFirstPersonMeleeProjection = !firstLayers.empty();
        const auto target = [](MWRender::Animation& view, const auto& reaction) {
            if (reaction && !reaction->mInactive
                && !view.setAnimationFrameTime(reaction->mAnimationGroup, reaction->mAnimationTime,
                    MWRender::Animation::PlaybackScope::ThisView))
                throw std::invalid_argument("native reaction event track is missing");
        };
        target(*mAnimation, next.mReaction);
        if (firstReaction) target(*peer, firstReaction);
        if (next.mFirstPerson && next.mFirstPerson->mStrike)
        {
            if (next.mStrike && !mAnimation->setAnimationFrameTime(next.mStrike->mAnimationGroup,
                    next.mStrike->mAnimationTime, MWRender::Animation::PlaybackScope::ThisView))
                throw std::invalid_argument("native body strike event track is missing");
            if (firstStrike && !peer->setAnimationFrameTime(firstStrike->mAnimationGroup,
                    firstStrike->mAnimationTime, MWRender::Animation::PlaybackScope::ThisView))
                throw std::invalid_argument("native first-person strike event track is missing");
        }
        const bool bodyInactive = next.mReaction && next.mReaction->mInactive;
        const bool firstInactive = firstReaction && firstReaction->mInactive;
        struct RetiredGroup { std::string mGroup; std::uint64_t mCause; bool mFirstPerson; };
        std::vector<RetiredGroup> retiredGroups;
        const auto collectRetired = [&](const auto& before, const auto& after, bool first) {
            const auto remains = [&](std::string_view group) {
                return (after.mStrike && after.mStrike->mAnimationGroup == group)
                    || (after.mReaction && after.mReaction->mAnimationGroup == group)
                    || (after.mPassiveSequence && after.mPassiveSequence->group() == group)
                    || std::any_of(after.mHeldBlocks.begin(), after.mHeldBlocks.end(),
                        [&](const auto& held) { return !held.mInactive && Misc::StringUtils::ciEqual(held.mAnimationGroup, group); })
                    || std::any_of(after.mRetiringSequences.begin(), after.mRetiringSequences.end(),
                        [&](const auto& sequence) { return sequence.mAnimationGroup == group; });
            };
            for (const auto& held : before.mHeldBlocks)
                if (!held.mInactive && !remains(held.mAnimationGroup))
                    retiredGroups.push_back({held.mAnimationGroup, 0, first});
            for (const auto& sequence : before.mRetiringSequences)
                if (!remains(sequence.mAnimationGroup))
                    retiredGroups.push_back({sequence.mAnimationGroup, sequence.mContactId, first});
            if (before.mPassiveSequence && !remains(before.mPassiveSequence->group()))
                retiredGroups.push_back({std::string(before.mPassiveSequence->group()),
                    before.mPassiveSequence->cause(), first});
        };
        collectRetired(*state, next, false);
        if (state->mFirstPerson && next.mFirstPerson)
            collectRetired(*state->mFirstPerson, *next.mFirstPerson, true);
        if (!service->commitMeleeReactionFrame(*prepared))
            throw std::invalid_argument("native reaction frame lost actor ownership");
        if (bodyInactive && !mOblivionReactionGroup.empty())
            mAnimation->disable(mOblivionReactionGroup, MWRender::Animation::PlaybackScope::ThisView);
        if (firstInactive && !mOblivionFirstPersonReactionGroup.empty())
            peer->disable(mOblivionFirstPersonReactionGroup, MWRender::Animation::PlaybackScope::ThisView);
        for (const auto& retired : retiredGroups)
        {
            auto& view = retired.mFirstPerson ? *peer : *mAnimation;
            view.disable(retired.mGroup, MWRender::Animation::PlaybackScope::ThisView);
            Log(Debug::Verbose) << "M15 native sequence retired: actor=" << actor.serialize()
                << " cause=" << retired.mCause << " group=" << retired.mGroup
                << " view=" << (retired.mFirstPerson ? "first" : "body");
        }
        return true;
    }

    void CharacterController::advanceOblivionMeleePlayback(float duration)
    {
        auto* world = dynamic_cast<MWWorld::World*>(MWBase::Environment::get().getWorld().operator MWBase::World*());
        auto* service = world && world->getGameProfile() == ESM::GameProfile::Oblivion
            ? world->getOblivionCombatService() : nullptr;
        if (!service || !mAnimation || !mPtr.getClass().isActor())
            return;
        const auto actor = mPtr == world->getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
            : mPtr.getCellRef().getFormKey();
        const auto* life = service->findActorLife(actor);
        if (!life || !service->findActorValues(actor))
            return;
        const bool frozen = mSkipAnim && !isScriptedAnimPlaying();
        const auto cancel = [&] {
            std::string group;
            if (const auto* state = service->findMeleeState(actor); state && state->mStrike)
            {
                group = state->mStrike->mAnimationGroup;
                service->cancelMeleeStrike(state->mStrike->mActionId, actor);
            }
            else if (mOblivionRenderedStrike)
                group = mCurrentWeapon;
            mOblivionRenderedStrike = 0;
            mCurrentWeapon.clear();
            mUpperBodyState = UpperBodyState::WeaponEquipped;
            if (life->mPhase == ESM4::ActorLifePhase::Alive && service->findMeleeState(actor))
                service->setMeleeInput(actor, {});
            if (!group.empty())
                mAnimation->disable(group);
        };
        const auto* state = service->findMeleeState(actor);
        if (frozen || life->mPhase != ESM4::ActorLifePhase::Alive)
        {
            cancel();
            cancelOblivionCombatInput();
            service->advanceAnimationClock(actor, frozen ? 0.f : duration);
            return;
        }
        // Contact dispatch can replace this actor's strike after its weapon
        // prefix already ran. Bind the newly published reaction on this pass.
        try
        {
            bindOblivionMeleeReaction();
            state = service->findMeleeState(actor);
            if (state && ((state->mFirstPerson && state->mFirstPerson->mStrike)
                    || (state->mStrike && state->mStrike->mKind == ESM4::MeleeStrikeKind::Counter)))
            {
                if (!advanceOblivionMeleeReactionFrame(duration)) service->advanceAnimationClock(actor, duration);
                return;
            }
            if (state && !state->mStrike && (state->mReaction || state->mPassiveSequence || !state->mRetiringSequences.empty()
                    || std::any_of(state->mHeldBlocks.begin(), state->mHeldBlocks.end(), [](const auto& h) { return !h.mInactive; })
                    || (state->mFirstPerson && (state->mFirstPerson->mReaction
                        || state->mFirstPerson->mPassiveSequence || !state->mFirstPerson->mRetiringSequences.empty()
                        || std::any_of(state->mFirstPerson->mHeldBlocks.begin(), state->mFirstPerson->mHeldBlocks.end(),
                            [](const auto& h) { return !h.mInactive; })))))
            {
                if (!advanceOblivionMeleeReactionFrame(duration)) service->advanceAnimationClock(actor, duration);
                return;
            }
        }
        catch (const std::invalid_argument& error)
        {
            Log(Debug::Verbose) << "M15 reaction frame rejected: actor=" << actor.serialize() << " reason=" << error.what();
            cancelOblivionCombatInput();
            service->advanceAnimationClock(actor, duration);
            return;
        }
        if (mOblivionMeleeProjection && (!state || state->mRetiringSequences.empty()))
        {
            mAnimation->setNativeSequenceLayers({});
            mOblivionMeleeProjection = false;
        }
        if (mOblivionFirstPersonMeleeProjection
            && (!state || !state->mFirstPerson || !state->mFirstPerson->mReaction))
        {
            if (auto* peer = mAnimation->getPlaybackPeer()) peer->setNativeSequenceLayers({});
            mOblivionFirstPersonMeleeProjection = false;
        }
        if (!state || !state->mStrike)
        {
            if (mOblivionRenderedStrike)
                cancel();
            service->advanceAnimationClock(actor, duration);
            return;
        }
        const auto strike = *state->mStrike;
        const bool power = strike.mKind >= ESM4::MeleeStrikeKind::StandingPower;
        const auto metadata = nativeMeleePlayback(*mAnimation, strike.mAnimationGroup, power,
            strike.mKind == ESM4::MeleeStrikeKind::Counter);
        if (!metadata || mOblivionRenderedStrike != strike.mActionId)
        {
            Log(Debug::Verbose) << "M15 melee timing rejected: actor=" << actor.serialize()
                                << " id=" << strike.mActionId << " reason=sequence-metadata";
            cancel();
            service->advanceAnimationClock(actor, duration);
            return;
        }
        if (!strike.mSequenceTiming)
        {
            // Explicit legacy migration: preserve displayed raw time and start
            // active history on the durable actor clock, without replaying keys.
            const float clock = service->advanceAnimationClock(actor, 0);
            const float fraction = std::clamp((strike.mAnimationTime - metadata->mTimelineBegin)
                / (metadata->mTimelineEnd - metadata->mTimelineBegin), 0.f, 1.f);
            const float raw = metadata->mBegin + fraction * (metadata->mEnd - metadata->mBegin);
            ESM4::MeleeSequenceTiming timing;
            timing.mOffset = static_cast<float>(double(raw) / metadata->mFrequency - clock);
            timing.mEaseStart = raw;
            timing.mEaseEnd = clock;
            timing.mLastInput = static_cast<float>(double(clock) + *timing.mOffset);
            timing.mWeightedTime = raw;
            timing.mOutputTime = raw;
            service->setMeleeSequenceTiming(strike.mActionId, actor, timing);
        }
        const auto& current = *service->findMeleeState(actor)->mStrike;
        struct FrameTarget
        {
            float mClock;
            unsigned mPhase;
            ESM4::MeleeSequenceTiming mTiming;
        };
        const auto next = [&]() -> FrameTarget {
            if (power)
            {
                const ESM4::PowerMeleeFrame previous{service->animationClock(actor),
                    current.mPowerPhase.value_or(ESM4::PowerMeleePhase::Start), *current.mSequenceTiming};
                const auto transition = current.mPoseSnapshot
                    ? std::optional(current.mPoseSnapshot->mDestinationFrame) : std::nullopt;
                if (current.mKind == ESM4::MeleeStrikeKind::Counter)
                {
                    const auto counter = ESM4::advanceCounterMeleeFrame({previous.mClock, previous.mPhase,
                        previous.mTiming, current.mRetirement, current.mInactive}, duration, metadata->mFrequency,
                        metadata->mBegin, metadata->mEnd, metadata->mPowerKeys, false, transition,
                        metadata->retirementDuration());
                    return {counter.mClock, unsigned(counter.mPhase), counter.mTiming};
                }
                const auto frame = ESM4::advancePowerMeleeFrame(previous, duration, current.mPlaybackSpeed,
                    metadata->mFrequency, metadata->mBegin, metadata->mEnd, metadata->mPowerKeys, false, transition);
                return {frame.mClock, unsigned(frame.mPhase), frame.mTiming};
            }
            const auto frame = ESM4::advanceOrdinaryMeleeFrame(
                {service->animationClock(actor), current.mOrdinaryPhase, *current.mSequenceTiming},
                duration, current.mPlaybackSpeed, metadata->mFrequency, metadata->mBegin,
                metadata->mEnd, metadata->mKeys, false,
                current.mPoseSnapshot ? std::optional(current.mPoseSnapshot->mDestinationFrame) : std::nullopt);
            return {frame.mClock, unsigned(frame.mPhase), frame.mTiming};
        }();
        const float target = metadata->rendererTime(next.mTiming.mOutputTime);
        // Validate the renderer target before publishing the durable frame.
        // The shared bridge currently admits forward playback only.
        if (target < mAnimation->getCurrentTime(strike.mAnimationGroup))
        {
            Log(Debug::Verbose) << "M15 melee timing rejected: actor=" << actor.serialize()
                                << " id=" << strike.mActionId << " reason=renderer-rewind";
            cancel();
            service->advanceAnimationClock(actor, duration);
            return;
        }
        if (!mAnimation->setAnimationFrameTime(strike.mAnimationGroup, target))
        {
            cancel();
            service->advanceAnimationClock(actor, duration);
            return;
        }
        const bool initialized = !current.mSequenceTiming->mOffset;
        const auto priorPhase = power ? unsigned(current.mPowerPhase.value_or(ESM4::PowerMeleePhase::Start))
            : unsigned(current.mOrdinaryPhase);
        if (power)
            service->advancePowerMeleeSequence(strike.mActionId, actor, duration, metadata->mFrequency,
                metadata->mBegin, metadata->mEnd, metadata->mPowerKeys, false, metadata->retirementDuration());
        else
            service->advanceOrdinaryMeleeSequence(strike.mActionId, actor, duration, metadata->mFrequency,
                metadata->mBegin, metadata->mEnd, metadata->mKeys);
        service->updateMeleeAnimation(strike.mActionId, actor, target);
        // The strike manager advanced the shared clock. Detached layers use
        // that same clock without a second frame-duration increment.
        try { advanceOblivionMeleeReactionFrame(0); }
        catch (const std::invalid_argument& error)
        {
            Log(Debug::Verbose) << "M15 outgoing frame rejected: actor=" << actor.serialize() << " reason=" << error.what();
            cancelOblivionCombatInput();
        }
        if (initialized || priorPhase != next.mPhase)
            Log(Debug::Verbose) << "M15 melee frame: actor=" << actor.serialize()
                                << " id=" << strike.mActionId << " phase=" << unsigned(next.mPhase)
                                << " clock=" << next.mClock << " output=" << next.mTiming.mOutputTime;
    }

    void CharacterController::finishOblivionMeleePlayback()
    {
        auto* world = dynamic_cast<MWWorld::World*>(MWBase::Environment::get().getWorld().operator MWBase::World*());
        auto* service = world && world->getGameProfile() == ESM::GameProfile::Oblivion
            ? world->getOblivionCombatService() : nullptr;
        if (!service || !mAnimation || !mOblivionRenderedStrike)
            return;
        const auto actor = mPtr == world->getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
            : mPtr.getCellRef().getFormKey();
        const auto* state = service->findMeleeState(actor);
        float complete = 0;
        if (!state || !state->mStrike || state->mStrike->mActionId != mOblivionRenderedStrike
            || !mAnimation->getInfo(state->mStrike->mAnimationGroup, &complete)
            || (state->mStrike->mSequenceTiming
                ? (state->mStrike->mKind == ESM4::MeleeStrikeKind::Counter ? !state->mStrike->mInactive
                    : state->mStrike->mKind >= ESM4::MeleeStrikeKind::StandingPower
                        ? state->mStrike->mPowerPhase != ESM4::PowerMeleePhase::End
                        : state->mStrike->mOrdinaryPhase != ESM4::OrdinaryMeleePhase::End) : complete < 1))
            return;
        const auto strike = *state->mStrike;
        if (strike.mKind == ESM4::MeleeStrikeKind::Counter) return; // Actor prefix owns state0 recovery.
        service->finishMeleeStrike(strike.mActionId, actor);
        Log(Debug::Verbose) << "M15 melee finished: actor=" << actor.serialize() << " id=" << strike.mActionId
                            << " action=" << service->getProcessAction(actor);
        mOblivionRenderedStrike = 0;
        mCurrentWeapon.clear();
        mUpperBodyState = UpperBodyState::WeaponEquipped;
        // runAnimation has returned; disabling here cannot invalidate its
        // text-key iterator. Publish cancellation before the Lua end callback.
        mAnimation->disable(strike.mAnimationGroup, state->mFirstPerson && state->mFirstPerson->mStrike
            ? MWRender::Animation::PlaybackScope::ThisView : MWRender::Animation::PlaybackScope::AllViews);
    }

    void CharacterController::cancelOblivionCombatInput()
    {
        auto* world = dynamic_cast<MWWorld::World*>(MWBase::Environment::get().getWorld().operator MWBase::World*());
        auto* service = world && world->getGameProfile() == ESM::GameProfile::Oblivion
            ? world->getOblivionCombatService() : nullptr;
        if (!service || !mAnimation)
            return;
        const auto actor = mPtr == world->getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
            : mPtr.getCellRef().getFormKey();
        if (!world->cancelOblivionHeldBlock(mPtr)) return;
        std::string strikeGroup;
        if (const auto* state = service->findMeleeState(actor); state && state->mStrike)
        {
            const auto strike = *state->mStrike;
            strikeGroup = strike.mAnimationGroup;
            service->cancelMeleeStrike(strike.mActionId, actor);
            Log(Debug::Verbose) << "M15 melee interrupt: actor=" << actor.serialize()
                                << " id=" << strike.mActionId << " group=" << strikeGroup;
        }
        if (const auto* state = service->findMeleeState(actor); state && state->mReaction)
            service->cancelMeleeReaction(state->mReaction->mContactId, actor);
        if (const auto* state = service->findMeleeState(actor);
            state && state->mFirstPerson && state->mFirstPerson->mReaction)
            service->cancelMeleeReaction(state->mFirstPerson->mReaction->mContactId, actor);
        if (const auto* state = service->findMeleeState(actor);
            state && state->mFirstPerson && state->mFirstPerson->mStrike)
            service->cancelMeleeStrike(state->mFirstPerson->mStrike->mActionId, actor);
        if (auto* peer = mAnimation->getPlaybackPeer(); peer && !mOblivionFirstPersonStrikeGroup.empty())
            peer->disable(mOblivionFirstPersonStrikeGroup, MWRender::Animation::PlaybackScope::ThisView);
        mOblivionFirstPersonRenderedStrike = 0;
        mOblivionFirstPersonStrikeGroup.clear();
        service->clearMeleeVisualHistory(actor);
        const auto reactionGroup = std::exchange(mOblivionReactionGroup, {});
        const auto firstReactionGroup = std::exchange(mOblivionFirstPersonReactionGroup, {});
        mOblivionRenderedReaction = 0;
        mOblivionFirstPersonRenderedReaction = 0;
        mOblivionPoseSnapshot.reset();
        mOblivionFirstPersonPoseSnapshot.reset();
        mOblivionPassivePoseSnapshot.reset();
        mOblivionFirstPersonPassivePoseSnapshot.reset();
        mOblivionHeldPoseBindings.clear();
        mOblivionFirstPersonHeldPoseBindings.clear();
        if (mOblivionMeleeProjection) mAnimation->setNativeSequenceLayers({});
        auto* peer = mAnimation->getPlaybackPeer();
        if (peer && mOblivionFirstPersonMeleeProjection) peer->setNativeSequenceLayers({});
        mOblivionMeleeProjection = false;
        mOblivionFirstPersonMeleeProjection = false;
        service->endBlocking(actor);
        service->clearMeleeInput(actor);
        const std::string blockGroup = std::exchange(mOblivionBlockGroup, {});
        mOblivionRenderedStrike = 0;
        mCurrentWeapon.clear();
        const bool drawn = mPtr.getClass().getCreatureStats(mPtr).getDrawState() == DrawState::Weapon
            || mPtr.getType() == ESM::REC_CREA4;
        mUpperBodyState = drawn ? UpperBodyState::WeaponEquipped : UpperBodyState::None;
        // All authority and renderer identities are cleared before either
        // disable can emit an animation-end observer.
        if (!strikeGroup.empty())
            mAnimation->disable(strikeGroup);
        if (!reactionGroup.empty()) mAnimation->disable(reactionGroup, MWRender::Animation::PlaybackScope::ThisView);
        if (peer && !firstReactionGroup.empty())
            peer->disable(firstReactionGroup, MWRender::Animation::PlaybackScope::ThisView);
        if (!blockGroup.empty())
        {
            Log(Debug::Verbose) << "M15 block release: actor=" << actor.serialize() << " group=" << blockGroup;
            mAnimation->disable(blockGroup);
        }
    }

    bool CharacterController::updateOblivionWeaponState(float duration)
    {
        auto* world = dynamic_cast<MWWorld::World*>(MWBase::Environment::get().getWorld().operator MWBase::World*());
        auto* service = world ? world->getOblivionCombatService() : nullptr;
        if (!service || !mAnimation)
            return false;
        const bool player = mPtr == world->getPlayerPtr();
        const auto actor = player ? ESM::FormKey::dynamic("player", 1) : mPtr.getCellRef().getFormKey();
        const auto* life = service->findActorLife(actor);
        const auto& cls = mPtr.getClass();
        const auto& stats = cls.getCreatureStats(mPtr);
        const bool drawn = stats.getDrawState() == DrawState::Weapon || mPtr.getType() == ESM::REC_CREA4;
        const auto hasResidentHeld = [](const auto* state) {
            const auto resident = [](const auto& cache) {
                return std::any_of(cache.begin(), cache.end(), [](const auto& entry) { return entry.resident(); });
            };
            return state && (resident(state->mHeldBlocks)
                || (state->mFirstPerson && resident(state->mFirstPerson->mHeldBlocks)));
        };
        const auto releaseBlock = [&] {
            const auto* heldState = service->findMeleeState(actor);
            if (hasResidentHeld(heldState))
            {
                const bool counter = heldState->mStrike && heldState->mStrike->mKind == ESM4::MeleeStrikeKind::Counter;
                const auto cause = counter ? heldState->mStrike->mActionId : 0;
                if (service->getProcessAction(actor) == 2 && counter)
                {
                    Log(Debug::Verbose) << "M15 Counter block release deferred: actor=" << actor.serialize()
                                        << " id=" << cause << " action=2";
                    return false;
                }
                if (!world->releaseOblivionHeldBlock(mPtr)) return false;
                // Slot3 now belongs to the passive cache; clear only its owner
                // bindings so the next pass does not disable its event tracks.
                mOblivionRenderedStrike = mOblivionFirstPersonRenderedStrike = 0;
                mCurrentWeapon.clear(); mOblivionFirstPersonStrikeGroup.clear();
                mOblivionRenderedReaction = mOblivionFirstPersonRenderedReaction = 0;
                mOblivionReactionGroup.clear(); mOblivionFirstPersonReactionGroup.clear();
                mOblivionPoseSnapshot.reset(); mOblivionFirstPersonPoseSnapshot.reset();
                mUpperBodyState = UpperBodyState::WeaponEquipped;
                const auto group = std::exchange(mOblivionBlockGroup, {});
                Log(Debug::Verbose) << "M15 block release: actor=" << actor.serialize() << " group=" << group;
                if (counter) Log(Debug::Verbose) << "M15 Counter block released: actor=" << actor.serialize()
                    << " id=" << cause << " action=" << service->getProcessAction(actor);
                return true;
            }
            if (const auto* state = service->findMeleeState(actor); state && state->mStrike
                && state->mStrike->mKind == ESM4::MeleeStrikeKind::Counter)
            {
                const auto cause = state->mStrike->mActionId;
                if (service->getProcessAction(actor) == 2)
                {
                    Log(Debug::Verbose) << "M15 Counter block release deferred: actor=" << actor.serialize()
                                        << " id=" << cause << " action=2";
                    return false;
                }
                if (!world->releaseOblivionCounterBlock(mPtr, mOblivionBlockGroup)) return false;
                // The cache now owns Counter's remaining visual lifetime.
                // Keep its renderer state until the native frame retires it.
                mOblivionRenderedStrike = 0;
                mCurrentWeapon.clear();
                mOblivionFirstPersonRenderedStrike = 0;
                mOblivionFirstPersonStrikeGroup.clear();
                mUpperBodyState = UpperBodyState::WeaponEquipped;
                Log(Debug::Verbose) << "M15 Counter block released: actor=" << actor.serialize()
                                    << " id=" << cause << " action=" << service->getProcessAction(actor);
            }
            else
                service->endBlocking(actor);
            // Clear posture before animation-end observers can query/save it.
            const std::string group = std::exchange(mOblivionBlockGroup, {});
            if (!group.empty())
            {
                Log(Debug::Verbose) << "M15 block release: actor=" << actor.serialize() << " group=" << group;
                mAnimation->disable(group);
            }
            return true;
        };
        const auto interrupt = [&] { cancelOblivionCombatInput(); };
        if (!life || life->mPhase != ESM4::ActorLifePhase::Alive)
        {
            interrupt();
            return false;
        }
        try { bindOblivionHeldBlock(); }
        catch (const std::invalid_argument& error)
        {
            Log(Debug::Verbose) << "M15 held restore binding rejected: actor=" << actor.serialize() << " reason=" << error.what();
            interrupt();
            return false;
        }
        // Scene construction and the first mechanics frame precede Lua's
        // queued OnActive event. Bind saved ownership until local scripts have
        // activated; interpreting their default controls would manufacture a
        // release on load. Zero-duration renderer refreshes also supply no input.
        const auto* localScripts = mPtr.getRefData().getLuaScripts();
        if ((duration == 0 || (localScripts && !localScripts->isActive()))
            && hasResidentHeld(service->findMeleeState(actor)))
        {
            Log(Debug::Verbose) << "M15 held initialization: actor=" << actor.serialize() << " input deferred";
            return false;
        }
        const auto* earlyControls = MWBase::Environment::get().getLuaManager()->getActorControls(mPtr);
        if ((!earlyControls || !earlyControls->mBlock) && service->getProcessAction(actor) == 6
            && hasResidentHeld(service->findMeleeState(actor)))
            if (!releaseBlock()) return false;
        // BlockHit keeps the held BlockIdle channel resident. A fresh renderer
        // has neither layer yet, so reconstruct the held NPC layer before
        // recovery decides whether channel1 is present. This projects existing
        // action6; beginBlocking would reject an owned reaction and must not be
        // used to manufacture another authority transition here.
        if (const auto* state = service->findMeleeState(actor);
            state && state->mReaction && state->mReaction->mKind == ESM4::MeleeReactionKind::BlockHit
            && mOblivionBlockGroup.empty() && !player && mPtr.getType() == ESM::REC_NPC_4 && drawn
            && service->getProcessAction(actor) == 6
            && !isScriptedAnimPlaying() && !isKnockedOut() && !isKnockedDown() && !isRecovery()
            && oblivionKnockedState(*world, mPtr) == 0 && !oblivionParalyzed(*world, mPtr) && !mSkipAnim)
        {
            const auto* controls = MWBase::Environment::get().getLuaManager()->getActorControls(mPtr);
            if (controls && controls->mBlock)
                try
                {
                    const auto cause = state->mReaction->mContactId;
                    auto& inventory = cls.getInventoryStore(mPtr);
                    const auto right = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
                    const MWWorld::Ptr weapon = right == inventory.end() ? MWWorld::Ptr{} : *right;
                    std::string family = "handtohand";
                    int weaponType = ESM::Weapon::HandToHand;
                    if (!weapon.isEmpty())
                    {
                        const auto id = MWWorld::OblivionProfileServices::nativeItemId(
                            world->getStore(), weapon.getCellRef().getRefId());
                        const auto* form = id.getIf<ESM::FormId>();
                        const auto* record = form ? world->getStore().get<ESM4::Weapon>().search(*form) : nullptr;
                        if (!record || record->mData.type > 3)
                            throw std::invalid_argument("unsupported held BlockHit weapon binding");
                        family = record->mData.type % 2 ? "twohand" : "onehand";
                        weaponType = record->mData.type % 2
                            ? ESM::Weapon::LongBladeTwoHand : ESM::Weapon::LongBladeOneHand;
                    }
                    bool shield = false;
                    if (const auto left = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedLeft);
                        left != inventory.end() && left->getType() == ESM::REC_ARMO)
                    {
                        const auto id = MWWorld::OblivionProfileServices::nativeItemId(
                            world->getStore(), left->getCellRef().getRefId());
                        const auto* form = id.getIf<ESM::FormId>();
                        const auto* armor = form ? world->getStore().get<ESM4::Armor>().search(*form) : nullptr;
                        if (!armor || !(armor->mArmorFlags & ESM4::Armor::TES4_Shield))
                            throw std::invalid_argument("unsupported held BlockHit shield binding");
                        shield = true;
                    }
                    const std::string group = shield ? "blockidle" : family + "blockidle";
                    const auto* metadata = mAnimation->getControllerSequenceMetadata(group);
                    const float start = mAnimation->getTextKeyTimeInGroup(group, group + ": start");
                    const float stop = mAnimation->getTextKeyTimeInGroup(group, group + ": stop");
                    if (!metadata || metadata->mCycleType != 0 || start < 0 || stop <= start)
                        throw std::invalid_argument("held BlockHit requires a cyclic BlockIdle resource");
                    mWeapon = weapon;
                    mWeaponType = weaponType;
                    mUpperBodyState = UpperBodyState::WeaponEquipped;
                    mAnimation->showWeapons(true);
                    mOblivionBlockGroup = group;
                    playBlendedAnimation(group, Priority_Weapon, MWRender::BlendMask_UpperBody,
                        false, 1.f, "start", "stop", 0.f, std::numeric_limits<std::uint32_t>::max(), true);
                    Log(Debug::Verbose) << "M15 block begin: actor=" << actor.serialize() << " group=" << group
                                        << " restored_reaction=" << cause;
                }
                catch (const std::exception& error)
                {
                    Log(Debug::Verbose) << "M15 reaction binding rejected: actor=" << actor.serialize()
                                        << " reason=" << error.what();
                    interrupt();
                    return false;
                }
        }
        const auto* heldState = service->findMeleeState(actor);
        const bool typedHeld = heldState && (!heldState->mHeldBlocks.empty()
            || (heldState->mFirstPerson && !heldState->mFirstPerson->mHeldBlocks.empty()));
        const bool blockIdlePresent = typedHeld ? [&] {
            const bool firstActive = player && world->isFirstPerson();
            const auto& cache = firstActive && heldState->mFirstPerson
                ? heldState->mFirstPerson->mHeldBlocks : heldState->mHeldBlocks;
            return std::any_of(cache.begin(), cache.end(), [](const auto& entry) { return entry.resident(); });
        }() : !mOblivionBlockGroup.empty() && mAnimation->getInfo(mOblivionBlockGroup);
        if (const auto* state = service->findMeleeState(actor); state && state->mStrike
            && state->mStrike->mKind == ESM4::MeleeStrikeKind::Counter && state->mStrike->mInactive)
        {
            const auto strike = *state->mStrike;
            if (service->finishMeleeStrike(strike.mActionId, actor, blockIdlePresent))
            {
                mOblivionRenderedStrike = 0;
                mCurrentWeapon.clear(); mUpperBodyState = UpperBodyState::WeaponEquipped;
                mAnimation->disable(strike.mAnimationGroup, state->mFirstPerson && state->mFirstPerson->mStrike
                    ? MWRender::Animation::PlaybackScope::ThisView : MWRender::Animation::PlaybackScope::AllViews);
                Log(Debug::Verbose) << "M15 counter recovered: actor=" << actor.serialize()
                                    << " id=" << strike.mActionId << " action=" << service->getProcessAction(actor);
            }
        }
        service->finishFirstPersonMeleeStrike(actor);
        const auto* recovering = service->findMeleeState(actor);
        const auto recoveryCause = recovering && recovering->mReaction ? recovering->mReaction->mContactId : 0;
        if (service->recoverInactiveMeleeReaction(actor, blockIdlePresent))
            Log(Debug::Verbose) << "M15 reaction recovered: actor=" << actor.serialize()
                                << " cause=" << recoveryCause << " action=" << service->getProcessAction(actor);
        if (const auto* state = service->findMeleeState(actor);
            state && (state->mReaction || (state->mFirstPerson && state->mFirstPerson->mReaction)))
        {
            if (isScriptedAnimPlaying() || isKnockedOut() || isKnockedDown() || isRecovery()
                || oblivionKnockedState(*world, mPtr) != 0 || oblivionParalyzed(*world, mPtr) || mSkipAnim)
            {
                interrupt();
                return false;
            }
            try { bindOblivionMeleeReaction(); }
            catch (const std::invalid_argument& error)
            {
                Log(Debug::Verbose) << "M15 reaction binding rejected: actor=" << actor.serialize()
                                    << " reason=" << error.what();
                interrupt();
            }
            return false;
        }
        if (!drawn || isScriptedAnimPlaying() || isKnockedOut() || isKnockedDown() || isRecovery()
            || oblivionKnockedState(*world, mPtr) != 0 || oblivionParalyzed(*world, mPtr)
            || mSkipAnim || (player && world->getOblivionAiService() && world->getOblivionAiService()->isRidingHorse(mPtr)))
        {
            interrupt();
            if (!drawn && mWeaponType != ESM::Weapon::None)
            {
                mAnimation->showWeapons(false);
                mWeaponType = ESM::Weapon::None;
                mWeapon = {};
            }
            return false;
        }
        MWWorld::Ptr weapon;
        const bool creature = mPtr.getType() == ESM::REC_CREA4;
        std::string family = "handtohand";
        bool shield = false;
        ESM::FormKey weaponBase;
        float speed = 1;
        int weaponType = ESM::Weapon::HandToHand;
        if (cls.hasInventoryStore(mPtr))
        {
            auto& inventory = cls.getInventoryStore(mPtr);
            const auto left = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedLeft);
            if (left != inventory.end() && left->getType() == ESM::REC_ARMO)
            {
                const auto id = MWWorld::OblivionProfileServices::nativeItemId(
                    world->getStore(), left->getCellRef().getRefId());
                const auto* form = id.getIf<ESM::FormId>();
                const auto* armor = form ? world->getStore().get<ESM4::Armor>().search(*form) : nullptr;
                if (!armor || !(armor->mArmorFlags & ESM4::Armor::TES4_Shield))
                {
                    interrupt();
                    return false;
                }
                shield = true;
            }
            auto equipped = inventory.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
            if (equipped != inventory.end())
            {
                if (creature)
                {
                    interrupt();
                    return false;
                }
                weapon = *equipped;
                const auto id = MWWorld::OblivionProfileServices::nativeItemId(
                    world->getStore(), weapon.getCellRef().getRefId());
                const auto* form = id.getIf<ESM::FormId>();
                const auto* record = form ? world->getStore().get<ESM4::Weapon>().search(*form) : nullptr;
                if (!record || record->mData.type > 3 || !std::isfinite(record->mData.speed)
                    || record->mData.speed <= 0)
                {
                    interrupt();
                    return false; // Bow/staff have their own native controller path.
                }
                family = record->mData.type % 2 ? "twohand" : "onehand";
                speed = record->mData.speed;
                weaponBase = ESM::FormKeyResolver(world->getContentFiles()).toFormKey(*form);
                weaponType = record->mData.type % 2 ? ESM::Weapon::LongBladeTwoHand : ESM::Weapon::LongBladeOneHand;
            }
        }
        if (const auto* state = service->findMeleeState(actor); state && state->mStrike
            && (state->mStrike->mWeaponBase != weaponBase || (mOblivionRenderedStrike && weapon != mWeapon)))
            interrupt();
        if (const auto* state = service->findMeleeState(actor);
            state && state->mFirstPerson && state->mFirstPerson->mStrike
                && state->mFirstPerson->mStrike->mWeaponBase != weaponBase)
            interrupt();
        const bool hadEquippedState = mUpperBodyState != UpperBodyState::None;
        const bool equipmentChanged = weapon != mWeapon || weaponType != mWeaponType || !hadEquippedState;
        mWeapon = weapon;
        mWeaponType = weaponType;
        mUpperBodyState = UpperBodyState::WeaponEquipped;
        // Hiding a player's weapon clears shared attack intent. Drawn unarmed
        // combat must retain that intent, and an unchanged weapon needs no rebuild.
        if (equipmentChanged)
            mAnimation->showWeapons(true);
        const auto* controls = MWBase::Environment::get().getLuaManager()->getActorControls(mPtr);
        // Original5E5690 forces prefix0 for worn-shield groups27/28.
        const std::string blockGroup = shield ? "blockidle" : family + "blockidle";
        if ((equipmentChanged && hadEquippedState)
            || (!mOblivionBlockGroup.empty() && mOblivionBlockGroup != blockGroup))
        {
            if (hasResidentHeld(service->findMeleeState(actor))) interrupt();
            else releaseBlock();
        }
        const auto* blockState = service->findMeleeState(actor);
        const bool counterActive = blockState && blockState->mStrike
            && blockState->mStrike->mKind == ESM4::MeleeStrikeKind::Counter;
        bool blockAccepted = false;
        if (!counterActive && controls && controls->mBlock && mPtr.getType() != ESM::REC_CREA4
            && mAnimation->hasAnimation(blockGroup))
        {
            const bool resident = hasResidentHeld(service->findMeleeState(actor));
            if (resident && service->getProcessAction(actor) == 6)
            {
                bindOblivionHeldBlock();
                return false;
            }
            // Legacy action6 has no recoverable held timing. Keep its existing
            // playback until release; fresh activations use the native cache.
            const bool legacy = !resident && service->getProcessAction(actor) == 6;
            blockAccepted = legacy ? service->beginBlocking(actor) : world->beginOblivionHeldBlock(mPtr, blockGroup);
            if (blockAccepted)
            {
                mOblivionBlockGroup = blockGroup;
                if (legacy)
                {
                    if (!mAnimation->isPlaying(blockGroup))
                        playBlendedAnimation(blockGroup, Priority_Weapon, MWRender::BlendMask_UpperBody,
                            false, 1.f, "start", "stop", 0.f, std::numeric_limits<std::uint32_t>::max(), true);
                }
                else
                {
                    bindOblivionHeldBlock();
                    // Anchor manager state before projection, matching its
                    // ordinary frame path without consuming another delta.
                    advanceOblivionMeleeReactionFrame(0);
                    Log(Debug::Verbose) << "M15 block begin: actor=" << actor.serialize() << " group=" << blockGroup;
                }
                return false;
            }
        }
        // Counter action2 defers release. After contact, action6 can release
        // posture while cached slot3 and held-slot retirement keep advancing.
        if (!counterActive || !controls || !controls->mBlock) releaseBlock();
        persistOblivionMeleeProgress();
        const auto* saved = service->findMeleeState(actor);
        auto input = saved ? saved->mInput : ESM4::RuntimeMeleeInput{};
        bool active = saved && saved->mStrike;
        bool power = active && saved->mStrike->mKind >= ESM4::MeleeStrikeKind::StandingPower;
        bool queueWindow = !active;
        if (active)
        {
            const auto strike = *saved->mStrike;
            mCurrentWeapon = strike.mAnimationGroup;
            const float start = mAnimation->getTextKeyTimeInGroup(strike.mAnimationGroup, strike.mAnimationGroup + ": start");
            const float stop = mAnimation->getTextKeyTimeInGroup(strike.mAnimationGroup, strike.mAnimationGroup + ": stop");
            if (start < 0 || stop <= start || strike.mAnimationTime > stop
                || !service->bindMeleePlayback(strike.mActionId, actor))
            {
                interrupt();
                active = false;
            }
            else
            {
                if (mOblivionRenderedStrike != strike.mActionId || !mAnimation->getInfo(strike.mAnimationGroup))
                {
                    mOblivionRenderedStrike = strike.mActionId;
                    Log(Debug::Verbose) << "M15 melee restore: actor=" << actor.serialize()
                                        << " id=" << strike.mActionId << " time=" << strike.mAnimationTime;
                    const float fraction = (std::max(start, strike.mAnimationTime) - start) / (stop - start);
                    try
                    {
                        if ((saved->mFirstPerson && saved->mFirstPerson->mStrike)
                            || strike.mKind == ESM4::MeleeStrikeKind::Counter)
                            mAnimation->play(strike.mAnimationGroup, Priority_Weapon, MWRender::BlendMask_All,
                                false, 0.f, "start", "stop", fraction, 0, false,
                                MWRender::Animation::PlaybackScope::ThisView,
                                strike.mKind == ESM4::MeleeStrikeKind::Counter ? std::string_view(mOblivionBlockGroup)
                                    : std::string_view{});
                        else
                            playBlendedAnimation(strike.mAnimationGroup, Priority_Weapon, MWRender::BlendMask_All,
                                false, strike.mPlaybackSpeed, "start", "stop", fraction, 0);
                    }
                    catch (...)
                    {
                        interrupt();
                        throw;
                    }
                }
                const float window = mAnimation->getTextKeyTimeInGroup(strike.mAnimationGroup, "a:");
                queueWindow = !power && (strike.mSequenceTiming
                    ? strike.mOrdinaryPhase >= ESM4::OrdinaryMeleePhase::Queue
                    : window >= 0 && strike.mAnimationTime > window);
                float complete = 0;
                if (!mAnimation->getInfo(strike.mAnimationGroup, &complete)
                    || (!strike.mSequenceTiming && complete >= 1))
                {
                    interrupt();
                    active = false;
                    queueWindow = true;
                }
            }
        }
        std::optional<ESM4::MeleeInputSettings> inputSettings;
        const auto settingsForInput = [&]() -> const ESM4::MeleeInputSettings& {
            if (!inputSettings)
                inputSettings = MWWorld::resolveOblivionMeleeInputSettings(world->getStore());
            return *inputSettings;
        };
        const auto airborneForInput = [&] {
            // The shared jump group spans native JumpStart/JumpLoop/JumpLand.
            // Keep that animation predicate ahead of physical state, including
            // landing playback on an already grounded body.
            const std::optional<std::uint8_t> jumpGroup = !mCurrentJump.empty()
                    && mAnimation->isPlaying(mCurrentJump)
                ? std::optional<std::uint8_t>{40} : std::nullopt;
            // Bridge common live motion states; this does not establish every
            // original Havok context transition (climbing, noclip, jump windup).
            const std::uint32_t motion = world->isSwimming(mPtr) ? 5
                : world->isFlying(mPtr) ? 4 : world->isOnGround(mPtr) ? 0 : 2;
            return ESM4::nativeAttackAirborne(jumpGroup, motion);
        };
        const bool held = getAttackingOrSpell();
        const bool pressed = held && !input.mInputHeld;
        std::optional<ESM4::MeleeStrikeKind> selected;
        if (input.mQueued != ESM4::MeleeQueuedStrike::None && queueWindow)
            selected = input.mQueued == ESM4::MeleeQueuedStrike::Power ? ESM4::MeleeStrikeKind::StandingPower
                : input.mPreferLeft ? ESM4::MeleeStrikeKind::Left : ESM4::MeleeStrikeKind::Right;
        else if (pressed)
        {
            input.mHeldSeconds = 0;
            if (!active)
                selected = input.mPreferLeft ? ESM4::MeleeStrikeKind::Left : ESM4::MeleeStrikeKind::Right;
            else if (!stats.getStance(CreatureStats::Stance_Sneak) || world->isSwimming(mPtr))
                input.mQueued = ESM4::MeleeQueuedStrike::Ordinary;
        }
        else if (held)
        {
            if (!power && input.mQueued != ESM4::MeleeQueuedStrike::Power)
                input.mHeldSeconds = static_cast<float>(double(input.mHeldSeconds) + duration);
            const auto& settings = settingsForInput();
            if (input.mHeldSeconds > settings.mPowerAttackDelay)
            {
                input.mHeldSeconds = 0;
                const int acrobatics = player ? service->getPlayerBaseValue(26)
                    : service->getNonPlayerBaseValue(actor, 26, world->getStore());
                if (!ESM4::heldPowerAttackAllowed(acrobatics, world->isSwimming(mPtr),
                        airborneForInput(), settings))
                    input.mQueued = ESM4::MeleeQueuedStrike::Ordinary;
                else if (active && saved->mStrike->mAnimationTime > 0)
                    input.mQueued = ESM4::MeleeQueuedStrike::Power;
                else
                    selected = ESM4::MeleeStrikeKind::StandingPower;
            }
        }
        input.mInputHeld = held;
        if (selected)
        {
            const int acrobatics = player ? service->getPlayerBaseValue(26)
                : service->getNonPlayerBaseValue(actor, 26, world->getStore());
            if (!ESM4::airborneMeleeStartAllowed(acrobatics, airborneForInput(), settingsForInput()))
            {
                Log(Debug::Verbose) << "M15 melee start rejected: actor=" << actor.serialize()
                                    << " reason=airborne-acrobatics base=" << acrobatics;
                input.mQueued = ESM4::MeleeQueuedStrike::None;
                selected.reset();
            }
        }
        if (selected)
        {
            // Exact sneak/directional/mastery variant selection is the next adapter step.
            constexpr std::array suffixes = {"attackleft", "attackright", "attackpower", "attackforwardpower",
                "attackbackpower", "attackleftpower", "attackrightpower"};
            const std::string group = family + suffixes[static_cast<unsigned>(*selected)];
            Log(Debug::Verbose) << "M15 melee selection: actor=" << actor.serialize()
                                << " group=" << group << " available=" << mAnimation->hasAnimation(group);
            input.mQueued = ESM4::MeleeQueuedStrike::None;
            if (mAnimation->hasAnimation(group))
            {
                const float start = mAnimation->getTextKeyTimeInGroup(group, group + ": start");
                const float stop = mAnimation->getTextKeyTimeInGroup(group, group + ": stop");
                bool nativeKeysValid = true;
                std::optional<NativeMeleePlayback> nativePlayback;
                std::optional<NativeMeleePlayback> firstPlayback;
                std::optional<OblivionMeleeStrikeViews> views;
                float nativeEase = 0;
                {
                    nativePlayback = nativeMeleePlayback(*mAnimation, group,
                        *selected >= ESM4::MeleeStrikeKind::StandingPower);
                    nativeKeysValid = nativePlayback.has_value();
                    if (nativePlayback)
                    {
                        try
                        {
                            std::optional<std::uint8_t> priorFrames;
                            const std::string prior(mAnimation->getActiveAnimationGroup(MWRender::BoneGroup_RightArm));
                            if (!prior.empty())
                                if (const auto* metadata = mAnimation->getControllerSequenceMetadata(prior))
                                    priorFrames = ESM4::meleeBlendFrames(nativeRawKeys(*metadata));
                            nativeEase = ESM4::meleeBlendDuration(priorFrames, nativePlayback->mBlendFrames, .1f);
                            const auto* current = service->findMeleeState(actor);
                            const auto prepareView = [&](MWRender::Animation& view, const NativeMeleePlayback& playback,
                                    const OblivionMeleeSlot3Source& old,
                                    std::optional<ESM4::RuntimeRetiringMeleeSequence>& outgoing) {
                                ESM4::RuntimeMeleeStrike incoming{1, *selected, weaponBase, group, speed,
                                    playback.mTimelineBegin, false};
                                if (*selected >= ESM4::MeleeStrikeKind::StandingPower)
                                    incoming.mPowerPhase = ESM4::PowerMeleePhase::Start;
                                incoming.mSequenceTiming = ESM4::MeleeSequenceTiming{};
                                std::optional<std::uint8_t> oldFrames;
                                if (old && !old.mInactive)
                                    if (const auto* metadata = view.getControllerSequenceMetadata(old.mGroup))
                                        oldFrames = ESM4::meleeBlendFrames(nativeRawKeys(*metadata));
                                const float ease = ESM4::meleeBlendDuration(oldFrames, playback.mBlendFrames, .1f);
                                incoming.mSequenceTiming->mEasing = ease > 0;
                                incoming.mSequenceTiming->mEaseEnd = ease;
                                if (ease > 0)
                                {
                                    if (old && !old.mInactive && old.mTiming && old.mTiming->mOffset
                                        && !old.mTiming->mEasing && !old.mPose && old.mGroup != group)
                                        outgoing = ESM4::RuntimeRetiringMeleeSequence{old.mCause,
                                            std::string(old.mGroup), old.mAnimationTime, *old.mTiming, {ease, {}}};
                                    else
                                    {
                                        incoming.mPoseSnapshot = view.captureNativeSequencePose(group, 0);
                                        if (!incoming.mPoseSnapshot) throw std::invalid_argument("missing native strike pose");
                                        incoming.mSequenceTiming->mOffset = 0.f;
                                    }
                                }
                                incoming.validate();
                                return incoming;
                            };
                            if (player || oblivionMeleeSlot3Source(current))
                            {
                                views.emplace();
                                views->mBody = prepareView(*mAnimation, *nativePlayback,
                                    oblivionMeleeSlot3Source(current), views->mBodyOutgoing);
                            }
                            if (player)
                            {
                                auto* first = mAnimation->getPlaybackPeer();
                                firstPlayback = first ? nativeMeleePlayback(*first, group,
                                    *selected >= ESM4::MeleeStrikeKind::StandingPower) : std::nullopt;
                                if (!firstPlayback) throw std::invalid_argument("missing paired native strike resource");
                                const auto firstClock = current && current->mFirstPerson
                                    ? current->mFirstPerson->mClock : service->animationClock(actor);
                                views->mFirstPerson = ESM4::RuntimeFirstPersonMeleeState{firstClock, {},
                                    prepareView(*first, *firstPlayback,
                                        oblivionMeleeSlot3Source(current && current->mFirstPerson
                                            ? &*current->mFirstPerson : nullptr), views->mFirstPersonOutgoing)};
                            }
                        }
                        catch (const std::invalid_argument&)
                        {
                            nativeKeysValid = false;
                        }
                    }
                    if (!nativeKeysValid)
                        Log(Debug::Verbose) << "M15 melee start rejected: actor=" << actor.serialize()
                                            << " reason=native-melee-keys group=" << group;
                }
                if (start >= 0 && stop > start && nativeKeysValid)
                {
                    // A rejected replacement must not cancel the current strike.
                    const std::string oldGroup = active ? saved->mStrike->mAnimationGroup : std::string{};
                    if (active && !views) interrupt();
                    const bool nativeActivation = views.has_value();
                    std::uint64_t id;
                    try { id = service->beginMeleeStrike(actor, *selected, group, speed, weaponBase, std::move(views)); }
                    catch (const std::invalid_argument& error)
                    {
                        Log(Debug::Verbose) << "M15 melee start rejected: actor=" << actor.serialize()
                                            << " reason=" << error.what();
                        service->setMeleeInput(actor, input);
                        return false;
                    }
                    service->setMeleeInput(actor, input); // Publish input before text-key/Lua callbacks.
                    mCurrentWeapon = group;
                    mOblivionRenderedStrike = id;
                    try
                    {
                        if (nativePlayback && !firstPlayback && !nativeActivation)
                        {
                            service->advanceAnimationClock(actor, 0);
                            ESM4::MeleeSequenceTiming timing;
                            timing.mEasing = nativeEase > 0;
                            timing.mEaseEnd = nativeEase;
                            service->setMeleeSequenceTiming(id, actor, timing);
                            service->updateMeleeAnimation(id, actor, nativePlayback->mTimelineBegin);
                        }
                        if (!service->bindMeleePlayback(id, actor))
                        {
                            service->cancelMeleeStrike(id, actor);
                            mOblivionRenderedStrike = 0;
                            mCurrentWeapon.clear();
                            return false;
                        }
                        if (firstPlayback)
                        {
                            service->advanceAnimationClock(actor, 0);
                            if (!oldGroup.empty() && oldGroup != group)
                                mAnimation->disable(oldGroup, MWRender::Animation::PlaybackScope::ThisView);
                            mAnimation->play(group, Priority_Weapon, MWRender::BlendMask_All,
                                false, 0.f, "start", "stop", 0, 0, false, MWRender::Animation::PlaybackScope::ThisView);
                            bindOblivionMeleeReaction();
                        }
                        else
                            playBlendedAnimation(group, Priority_Weapon, MWRender::BlendMask_All,
                                false, speed, "start", "stop", 0, 0);
                    }
                    catch (...)
                    {
                        service->cancelMeleeStrike(id, actor);
                        mOblivionRenderedStrike = 0;
                        mCurrentWeapon.clear();
                        mAnimation->disable(group);
                        throw;
                    }
                    if (*selected == ESM4::MeleeStrikeKind::Left || *selected == ESM4::MeleeStrikeKind::Right)
                        input.mPreferLeft = *selected != ESM4::MeleeStrikeKind::Left;
                }
            }
        }
        life = service->findActorLife(actor);
        if (life && life->mPhase == ESM4::ActorLifePhase::Alive)
        {
            service->setMeleeInput(actor, input);
            if (const auto* current = service->findMeleeState(actor); current && current->mStrike)
                mUpperBodyState = UpperBodyState::AttackEnd;
        }
        return false;
    }

    bool CharacterController::updateWeaponState(float duration)
    {
        if (MWBase::Environment::get().getWorld()->getGameProfile() == ESM::GameProfile::Oblivion)
            return updateOblivionWeaponState(duration);

        // If the current animation is scripted, we can't do anything here.
        if (isScriptedAnimPlaying())
            return false;

        const auto world = MWBase::Environment::get().getWorld();
        auto& prng = world->getPrng();
        MWBase::SoundManager* sndMgr = MWBase::Environment::get().getSoundManager();

        const MWWorld::Class& cls = mPtr.getClass();
        CreatureStats& stats = cls.getCreatureStats(mPtr);
        int weaptype = ESM::Weapon::None;
        if (stats.getDrawState() == DrawState::Weapon)
            weaptype = ESM::Weapon::HandToHand;
        else if (stats.getDrawState() == DrawState::Spell)
            weaptype = ESM::Weapon::Spell;

        const bool isWerewolf = cls.isNpc() && cls.getNpcStats(mPtr).isWerewolf();

        const ESM::RefId* downSoundId = nullptr;
        bool weaponChanged = false;
        bool ammunition = true;
        float weapSpeed = 1.f;
        if (cls.hasInventoryStore(mPtr))
        {
            MWWorld::InventoryStore& inv = cls.getInventoryStore(mPtr);
            MWWorld::ContainerStoreIterator weapon = getActiveWeapon(mPtr, &weaptype);
            if (stats.getDrawState() == DrawState::Spell)
                weapon = inv.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);

            MWWorld::Ptr newWeapon;
            if (weapon != inv.end())
            {
                newWeapon = *weapon;
                if (isRealWeapon(mWeaponType))
                    downSoundId = &newWeapon.getClass().getDownSoundId(newWeapon);
            }
            // weapon->HtH switch: weapon is empty already, so we need to take sound from previous weapon
            else if (!mWeapon.isEmpty() && weaptype == ESM::Weapon::HandToHand && mWeaponType != ESM::Weapon::Spell)
                downSoundId = &mWeapon.getClass().getDownSoundId(mWeapon);

            if (mWeapon != newWeapon)
            {
                mWeapon = newWeapon;
                weaponChanged = true;
            }

            if (stats.getDrawState() == DrawState::Weapon && !mWeapon.isEmpty()
                && mWeapon.getType() == ESM::Weapon::sRecordId)
            {
                weapSpeed = mWeapon.get<ESM::Weapon>()->mBase->mData.mSpeed;
                MWWorld::ConstContainerStoreIterator ammo = inv.getSlot(MWWorld::InventoryStore::Slot_Ammunition);
                int ammotype = getWeaponType(mWeapon.get<ESM::Weapon>()->mBase->mData.mType)->mAmmoType;
                if (ammotype != ESM::Weapon::None)
                    ammunition = ammo != inv.end() && ammo->get<ESM::Weapon>()->mBase->mData.mType == ammotype;
                // Cancel attack if we no longer have ammunition
                if (!ammunition)
                    setAttackingOrSpell(false);
            }

            MWWorld::ConstContainerStoreIterator torch = inv.getSlot(MWWorld::InventoryStore::Slot_CarriedLeft);
            if (torch != inv.end() && torch->getType() == ESM::Light::sRecordId
                && updateCarriedLeftVisible(mWeaponType))
            {
                if (mAnimation->isPlaying("shield"))
                    mAnimation->disable("shield");

                playBlendedAnimation("torch", Priority_Torch, MWRender::BlendMask_LeftArm, false, 1.0f, "start", "stop",
                    0.0f, std::numeric_limits<uint32_t>::max(), true);
            }
            else if (mAnimation->isPlaying("torch"))
            {
                mAnimation->disable("torch");
            }
        }

        MWRender::Animation::AnimPriority priorityWeapon(Priority_Default);
        if (cls.isBipedal(mPtr))
        {
            // For bipeds, blend weapon animations with lower body animations with higher priority
            // For non-bipeds, movement takes priority
            priorityWeapon = Priority_Weapon;
            priorityWeapon[MWRender::BoneGroup_LowerBody] = Priority_WeaponLowerBody;
        }

        bool forcestateupdate = false;

        // We should not play equipping animation and sound during weapon->weapon transition
        const bool isStillWeapon = isRealWeapon(mWeaponType) && isRealWeapon(weaptype);

        // If the current weapon type was changed in the middle of attack (e.g. by Equip console command or when bound
        // spell expires), we should force actor to the "weapon equipped" state, interrupt attack and update animations.
        // Morrowind does this at the end of the attack (see #4646 and PR 1972).
        // If we decide to cope with the resulting problems, the thrown weapon->H2H case below should be extended.
        if (isStillWeapon && mWeaponType != weaptype && mUpperBodyState > UpperBodyState::WeaponEquipped)
        {
            forcestateupdate = true;
            if (!mCurrentWeapon.empty())
                mAnimation->disable(mCurrentWeapon);
            mUpperBodyState = UpperBodyState::WeaponEquipped;
            setAttackingOrSpell(false);
            mAnimation->showWeapons(true);
        }

        if (!isKnockedOut() && !isKnockedDown() && !isRecovery())
        {
            std::string weapgroup;
            if (((!isWerewolf && cls.isBipedal(mPtr)) || mWeaponType != ESM::Weapon::Spell) && weaptype != mWeaponType
                && mUpperBodyState <= UpperBodyState::AttackWindUp && mUpperBodyState != UpperBodyState::Unequipping
                && !isStillWeapon)
            {
                // We can not play un-equip animation if weapon changed since last update
                if (!weaponChanged)
                {
                    // Note: we do not disable the weapon unequipping animation automatically to avoid body desync
                    weapgroup = getWeaponAnimation(mWeaponType);
                    int unequipMask = MWRender::BlendMask_All;
                    mUpperBodyState = UpperBodyState::Unequipping;
                    bool useShieldAnims = mAnimation->useShieldAnimations();
                    if (useShieldAnims && mWeaponType != ESM::Weapon::HandToHand && mWeaponType != ESM::Weapon::Spell
                        && !(mWeaponType == ESM::Weapon::None && weaptype == ESM::Weapon::Spell))
                    {
                        unequipMask = unequipMask | ~MWRender::BlendMask_LeftArm;
                        mAnimation->disable("shield");
                        playBlendedAnimation("shield", Priority_Block, MWRender::BlendMask_LeftArm, true, 1.0f,
                            "unequip start", "unequip stop", 0.0f, 0);
                    }
                    else if (mWeaponType == ESM::Weapon::HandToHand)
                        mAnimation->showCarriedLeft(false);

                    mAnimation->disable(weapgroup);
                    playBlendedAnimation(
                        weapgroup, priorityWeapon, unequipMask, false, 1.0f, "unequip start", "unequip stop", 0.0f, 0);

                    mAnimation->detachArrow();

                    // If we do not have the "unequip detach" key, hide weapon manually.
                    if (mAnimation->getTextKeyTime(weapgroup + ": unequip detach") < 0)
                        mAnimation->showWeapons(false);
                }

                if (downSoundId && !downSoundId->empty())
                {
                    sndMgr->playSound3D(mPtr, *downSoundId, 1.0f, 1.0f);
                }
            }

            float complete;
            bool animPlaying = mAnimation->getInfo(mCurrentWeapon, &complete);
            if (!animPlaying || complete >= 1.0f)
            {
                // Weapon is changed, no current animation (e.g. unequipping or attack).
                // Start equipping animation now.
                if (weaptype != mWeaponType && mUpperBodyState <= UpperBodyState::WeaponEquipped)
                {
                    forcestateupdate = true;
                    bool useShieldAnims = mAnimation->useShieldAnimations();
                    if (!useShieldAnims)
                        mAnimation->showCarriedLeft(updateCarriedLeftVisible(weaptype));

                    weapgroup = getWeaponAnimation(weaptype);

                    // Note: controllers for ranged weapon should use time for beginning of animation to play shooting
                    // properly, for other weapons they should use absolute time. Some mods rely on this behaviour (to
                    // rotate throwing projectiles, for example)
                    ESM::WeaponType::Class weaponClass = getWeaponType(weaptype)->mWeaponClass;
                    bool useRelativeDuration = weaponClass == ESM::WeaponType::Ranged;
                    mAnimation->setWeaponGroup(weapgroup, useRelativeDuration);

                    if (!isStillWeapon)
                    {
                        if (animPlaying)
                            mAnimation->disable(mCurrentWeapon);
                        if (weaptype != ESM::Weapon::None)
                        {
                            mAnimation->showWeapons(false);
                            int equipMask = MWRender::BlendMask_All;
                            mUpperBodyState = UpperBodyState::Equipping;
                            mResetIdleOnAttackEnd = true;
                            if (useShieldAnims && weaptype != ESM::Weapon::Spell)
                            {
                                equipMask = equipMask | ~MWRender::BlendMask_LeftArm;
                                mAnimation->disable("shield");
                                playBlendedAnimation("shield", Priority_Block, MWRender::BlendMask_LeftArm, true, 1.0f,
                                    "equip start", "equip stop", 0.0f, 0);
                            }

                            if (weaptype != ESM::Weapon::Spell || cls.isBipedal(mPtr))
                            {
                                playBlendedAnimation(weapgroup, priorityWeapon, equipMask, false, 1.0f, "equip start",
                                    "equip stop", 0.0f, 0);
                            }

                            // If we do not have the "equip attach" key, show weapon manually.
                            if (weaptype != ESM::Weapon::Spell
                                && mAnimation->getTextKeyTime(weapgroup + ": equip attach") < 0)
                            {
                                mAnimation->showWeapons(true);
                            }

                            if (!mWeapon.isEmpty() && mWeaponType != ESM::Weapon::HandToHand && isRealWeapon(weaptype))
                            {
                                const ESM::RefId& upSoundId = mWeapon.getClass().getUpSoundId(mWeapon);
                                if (!upSoundId.empty())
                                    sndMgr->playSound3D(mPtr, upSoundId, 1.0f, 1.0f);
                            }
                        }
                    }

                    if (isWerewolf && weaptype != ESM::Weapon::None)
                    {
                        const MWWorld::ESMStore& store = world->getStore();
                        const ESM::Sound* sound = store.get<ESM::Sound>().searchRandom("WolfEquip", prng);
                        if (sound)
                        {
                            sndMgr->playSound3D(mPtr, sound->mId, 1.0f, 1.0f);
                        }
                    }

                    mWeaponType = weaptype;
                    mCurrentWeapon = weapgroup;
                }

                // Make sure that we disabled unequipping animation
                if (mUpperBodyState == UpperBodyState::Unequipping)
                {
                    resetCurrentWeaponState();
                    mWeaponType = ESM::Weapon::None;
                }
            }
        }

        float complete = 0.f;
        bool animPlaying = false;
        ESM::WeaponType::Class weapclass = getWeaponType(mWeaponType)->mWeaponClass;
        if (getAttackingOrSpell())
        {
            mResetIdleOnAttackEnd = true;
            if (mUpperBodyState == UpperBodyState::WeaponEquipped
                && (mHitState == CharState_None || mHitState == CharState_Block))
            {
                mAttackStrength = -1.f;
                mReadyToHit = false;

                // Randomize attacks for non-bipedal creatures
                if (!cls.isBipedal(mPtr)
                    && (!mAnimation->hasAnimation(mCurrentWeapon) || isRandomAttackAnimation(mCurrentWeapon)))
                {
                    mCurrentWeapon = chooseRandomAttackAnimation();
                }

                if (mWeaponType == ESM::Weapon::Spell)
                {
                    // Unset casting flag, otherwise pressing the mouse button down would
                    // continue casting every frame if there is no animation
                    setAttackingOrSpell(false);
                    if (mPtr == getPlayer())
                    {
                        // For the player, set the spell we want to cast
                        // This has to be done at the start of the casting animation,
                        // *not* when selecting a spell in the GUI (otherwise you could change the spell mid-animation)
                        const ESM::RefId& selectedSpell
                            = MWBase::Environment::get().getWindowManager()->getSelectedSpell();
                        stats.getSpells().setSelectedSpell(selectedSpell);
                    }
                    ESM::RefId spellid = stats.getSpells().getSelectedSpell();
                    bool isMagicItem = false;

                    // Play hand VFX and allow castSpell use (assuming an animation is going to be played) if
                    // spellcasting is successful. Scripted spellcasting bypasses restrictions.
                    MWWorld::SpellCastState spellCastResult = MWWorld::SpellCastState::Success;
                    if (!mCastingScriptedSpell || world->getGameProfile() == ESM::GameProfile::Oblivion)
                        spellCastResult = world->startSpellCast(mPtr);
                    mCanCast = spellCastResult == MWWorld::SpellCastState::Success;

                    if (spellCastResult == MWWorld::SpellCastState::Unsupported)
                    {
                        spellid = {};
                        mCastingScriptedSpell = false;
                    }

                    if (spellid.empty() && spellCastResult != MWWorld::SpellCastState::Unsupported)
                    {
                        const MWWorld::ContainerStore& inv = cls.getContainerStore(mPtr);
                        if (inv.getSelectedEnchantItem() != inv.end())
                        {
                            const MWWorld::ConstPtr& enchantItem = *inv.getSelectedEnchantItem();
                            spellid = enchantItem.getClass().getEnchantment(enchantItem);
                            isMagicItem = true;
                        }
                    }

                    if (isMagicItem && !Settings::game().mUseMagicItemAnimations)
                    {
                        world->breakInvisibility(mPtr);
                        // Enchanted items by default do not use casting animations
                        world->castSpell(mPtr);
                        // Spellcasting animation needs to "play" for at least one frame to reset the aiming factor
                        animPlaying = true;
                        mUpperBodyState = UpperBodyState::Casting;
                        // But idle should not be reset
                        mResetIdleOnAttackEnd = false;
                    }
                    // Play the spellcasting animation/VFX if the spellcasting was successful or failed due to
                    // insufficient magicka. Used up powers are exempt from this from some reason.
                    else if (!spellid.empty() && spellCastResult != MWWorld::SpellCastState::PowerAlreadyUsed)
                    {
                        world->breakInvisibility(mPtr);
                        MWMechanics::CastSpell cast(mPtr, {}, false, mCastingScriptedSpell);

                        const std::vector<ESM::IndexedENAMstruct>* effects{ nullptr };
                        const MWWorld::ESMStore& store = world->getStore();
                        if (isMagicItem)
                        {
                            const ESM::Enchantment* enchantment = store.get<ESM::Enchantment>().find(spellid);
                            effects = &enchantment->mEffects.mList;
                            cast.playSpellCastingEffects(enchantment);
                        }
                        else
                        {
                            const ESM::Spell* spell = store.get<ESM::Spell>().find(spellid);
                            effects = &spell->mEffects.mList;
                            cast.playSpellCastingEffects(spell);
                        }
                        if (!effects->empty())
                        {
                            if (mCanCast)
                            {
                                const ESM::MagicEffect* effect = store.get<ESM::MagicEffect>().find(
                                    effects->back().mData.mEffectID); // use last effect of list for color of VFX_Hands

                                const ESM::Static* castStatic
                                    = world->getStore().get<ESM::Static>().find(ESM::RefId::stringRefId("VFX_Hands"));

                                const VFS::Path::Normalized castStaticModel
                                    = Misc::ResourceHelpers::correctMeshPath(castStatic->mModel.getNormalized());

                                if (mAnimation->getNode("Bip01 L Hand"))
                                    mAnimation->addEffect(castStaticModel.value(), "", false, "Bip01 L Hand",
                                        effect->mParticle.getOriginal());

                                if (mAnimation->getNode("Bip01 R Hand"))
                                    mAnimation->addEffect(castStaticModel.value(), "", false, "Bip01 R Hand",
                                        effect->mParticle.getOriginal());
                            }
                            // first effect used for casting animation
                            const ESM::ENAMstruct& firstEffect = effects->front().mData;

                            std::string startKey;
                            std::string stopKey;
                            if (isRandomAttackAnimation(mCurrentWeapon))
                            {
                                startKey = "start";
                                stopKey = "stop";
                                if (mCanCast)
                                    world->castSpell(mPtr,
                                        mCastingScriptedSpell); // No "release" text key to use, so cast immediately
                                mCastingScriptedSpell = false;
                                mCanCast = false;
                            }
                            else
                            {
                                switch (firstEffect.mRange)
                                {
                                    case 0:
                                        mAttackType = "self";
                                        break;
                                    case 1:
                                        mAttackType = "touch";
                                        break;
                                    case 2:
                                        mAttackType = "target";
                                        break;
                                }

                                startKey = mAttackType + " start";
                                stopKey = mAttackType + " stop";
                            }
                            playBlendedAnimation(mCurrentWeapon, priorityWeapon, MWRender::BlendMask_All, false, 1,
                                startKey, stopKey, 0.0f, 0);
                            mUpperBodyState = UpperBodyState::Casting;
                        }
                    }
                }
                else
                {
                    std::string startKey = "start";
                    std::string stopKey = "stop";

                    MWBase::LuaManager::ActorControls* actorControls
                        = MWBase::Environment::get().getLuaManager()->getActorControls(mPtr);
                    const bool aiInactive
                        = actorControls->mDisableAI || !MWBase::Environment::get().getMechanicsManager()->isAIActive();
                    if (mWeaponType != ESM::Weapon::PickProbe && !isRandomAttackAnimation(mCurrentWeapon))
                    {
                        if (weapclass == ESM::WeaponType::Ranged || weapclass == ESM::WeaponType::Thrown)
                            mAttackType = "shoot";
                        else if (mPtr == getPlayer())
                        {
                            if (Settings::game().mBestAttack)
                            {
                                if (!mWeapon.isEmpty() && mWeapon.getType() == ESM::Weapon::sRecordId)
                                {
                                    mAttackType = getBestAttack(mWeapon.get<ESM::Weapon>()->mBase);
                                }
                                else
                                {
                                    // There is no "best attack" for Hand-to-Hand
                                    mAttackType = getRandomAttackType();
                                }
                            }
                            else
                            {
                                mAttackType = getMovementBasedAttackType();
                            }
                        }
                        else if (aiInactive)
                        {
                            mAttackType = getDesiredAttackType();
                            if (mAttackType == "")
                                mAttackType = getRandomAttackType();
                        }

                        // else if (mPtr != getPlayer()) use mAttackType set by AiCombat
                        startKey = mAttackType + ' ' + startKey;
                        stopKey = mAttackType + " max attack";
                    }

                    mUpperBodyState = UpperBodyState::AttackWindUp;

                    // Reset the attack results when the attack starts.
                    // Strictly speaking this should probably be done when the attack ends,
                    // but the attack animation might be cancelled in a myriad different ways.
                    mAttackSuccess = false;
                    mAttackVictim = MWWorld::Ptr();
                    mAttackHitPos = osg::Vec3f();

                    playBlendedAnimation(mCurrentWeapon, priorityWeapon, MWRender::BlendMask_All, false, weapSpeed,
                        startKey, stopKey, 0.0f, 0);
                }
            }
        }

        // Random attack and pick/probe animations never have wind up and are played to their end.
        // Other animations must be released when the attack state is unset.
        if (mUpperBodyState == UpperBodyState::AttackWindUp
            && (mWeaponType == ESM::Weapon::PickProbe || isRandomAttackAnimation(mCurrentWeapon)
                || !getAttackingOrSpell()))
        {
            mUpperBodyState = UpperBodyState::AttackRelease;
            world->breakInvisibility(mPtr);
            if (mWeaponType == ESM::Weapon::PickProbe)
            {
                // TODO: this will only work for the player, and needs to be fixed if NPCs should ever use
                // lockpicks/probes.
                MWWorld::Ptr target = world->getFocusObject();

                if (!target.isEmpty())
                {
                    std::string_view resultMessage, resultSound;
                    if (mWeapon.getType() == ESM::Lockpick::sRecordId)
                        Security(mPtr).pickLock(target, mWeapon, resultMessage, resultSound);
                    else if (mWeapon.getType() == ESM::Probe::sRecordId)
                        Security(mPtr).probeTrap(target, mWeapon, resultMessage, resultSound);
                    if (!resultMessage.empty())
                        MWBase::Environment::get().getWindowManager()->messageBox(resultMessage);
                    if (!resultSound.empty())
                        sndMgr->playSound3D(target, ESM::RefId::stringRefId(resultSound), 1.0f, 1.0f);
                }
            }
            // Evaluate the attack results and play the swish sound.
            // Attack animations with no hit key do this earlier.
            else
            {
                prepareHit();
            }

            if (mWeaponType == ESM::Weapon::PickProbe || isRandomAttackAnimation(mCurrentWeapon))
                mUpperBodyState = UpperBodyState::AttackEnd;
        }

        if (mUpperBodyState == UpperBodyState::AttackRelease)
        {
            // The release state might have been reached before reaching the wind-up section. We'll play the new section
            // only when the wind-up section is reached.
            float currentTime = mAnimation->getCurrentTime(mCurrentWeapon);
            float minAttackTime = mAnimation->getTextKeyTime(mCurrentWeapon + ": " + mAttackType + " min attack");
            float maxAttackTime = mAnimation->getTextKeyTime(mCurrentWeapon + ": " + mAttackType + " max attack");
            if (minAttackTime <= currentTime && currentTime <= maxAttackTime)
            {
                std::string hit = mAttackType != "shoot" ? "hit" : "release";

                float startPoint = 0.f;

                // Skip a bit of the pre-hit section based on the attack strength
                if (minAttackTime != -1.f && minAttackTime < maxAttackTime)
                {
                    startPoint = 1.f - mAttackStrength;
                    float minHitTime = mAnimation->getTextKeyTime(mCurrentWeapon + ": " + mAttackType + " min hit");
                    float hitTime = mAnimation->getTextKeyTime(mCurrentWeapon + ": " + mAttackType + ' ' + hit);
                    if (maxAttackTime <= minHitTime && minHitTime < hitTime)
                        startPoint *= (minHitTime - maxAttackTime) / (hitTime - maxAttackTime);
                }

                mAnimation->disable(mCurrentWeapon);
                playBlendedAnimation(mCurrentWeapon, priorityWeapon, MWRender::BlendMask_All, false, weapSpeed,
                    mAttackType + " max attack", mAttackType + ' ' + hit, startPoint, 0);
            }

            animPlaying = mAnimation->getInfo(mCurrentWeapon, &complete);

            // Try playing the "follow" section if the attack animation ended naturally or didn't play at all.
            if (!animPlaying || (currentTime >= maxAttackTime && complete >= 1.f))
            {
                std::string start = "follow start";
                std::string stop = "follow stop";

                if (mAttackType != "shoot")
                {
                    std::string strength = mAttackStrength < 0.33f ? "small"
                        : mAttackStrength < 0.66f                  ? "medium"
                                                                   : "large";
                    start = strength + ' ' + start;
                    stop = strength + ' ' + stop;
                }

                mReadyToHit = false;

                if (animPlaying)
                    mAnimation->disable(mCurrentWeapon);
                playBlendedAnimation(mCurrentWeapon, priorityWeapon, MWRender::BlendMask_All, false, weapSpeed,
                    mAttackType + ' ' + start, mAttackType + ' ' + stop, 0.0f, 0);
                mUpperBodyState = UpperBodyState::AttackEnd;

                animPlaying = mAnimation->getInfo(mCurrentWeapon, &complete);
            }
        }

        if (!animPlaying)
            animPlaying = mAnimation->getInfo(mCurrentWeapon, &complete);

        if (!animPlaying || complete >= 1.f)
        {
            if (mUpperBodyState == UpperBodyState::Equipping || mUpperBodyState == UpperBodyState::AttackEnd
                || mUpperBodyState == UpperBodyState::Casting)
            {
                if (ammunition && mWeaponType == ESM::Weapon::MarksmanCrossbow)
                    mAnimation->attachArrow();

                // Cancel stagger animation at the end of an attack to avoid abrupt transitions
                // in favor of a different abrupt transition, like Morrowind
                if (mUpperBodyState != UpperBodyState::Equipping && isRecovery())
                    mAnimation->disable(mCurrentHit);

                if (animPlaying)
                    mAnimation->disable(mCurrentWeapon);

                // Skip Thrown->H2H idle transition (e.g., if we've run out of ammo)
                // In Morrowind this isn't actually specific to this transition
                // See the weapon->weapon mid-attack skip logic above
                if (mUpperBodyState == UpperBodyState::AttackEnd)
                {
                    if (weapclass == ESM::WeaponType::Thrown && weaptype == ESM::Weapon::HandToHand)
                    {
                        forcestateupdate = true;
                        mWeaponType = weaptype;
                        mCurrentWeapon = getWeaponAnimation(mWeaponType);
                        mAnimation->showWeapons(false);
                    }
                }
                // We should not break swim and sneak animations
                if (mResetIdleOnAttackEnd && mIdleState != CharState_IdleSneak && mIdleState != CharState_IdleSwim)
                {
                    resetCurrentIdleState();
                }

                mUpperBodyState = UpperBodyState::WeaponEquipped;
            }
            else if (mUpperBodyState == UpperBodyState::Unequipping)
            {
                if (animPlaying)
                    mAnimation->disable(mCurrentWeapon);
                mUpperBodyState = UpperBodyState::None;
                mWeaponType = ESM::Weapon::None;
            }
        }

        mAnimation->setPitchFactor(0.f);
        if (mUpperBodyState > UpperBodyState::WeaponEquipped
            && (weapclass == ESM::WeaponType::Ranged || weapclass == ESM::WeaponType::Thrown))
        {
            mAnimation->setPitchFactor(1.f);

            // A smooth transition can be provided if a pre-wind-up section is defined. Random attack animations never
            // have one.
            if (mUpperBodyState == UpperBodyState::AttackWindUp && !isRandomAttackAnimation(mCurrentWeapon))
            {
                float currentTime = mAnimation->getCurrentTime(mCurrentWeapon);
                float minAttackTime = mAnimation->getTextKeyTime(mCurrentWeapon + ": " + mAttackType + " min attack");
                float startTime = mAnimation->getTextKeyTime(mCurrentWeapon + ": " + mAttackType + " start");
                if (startTime <= currentTime && currentTime < minAttackTime)
                    mAnimation->setPitchFactor((currentTime - startTime) / (minAttackTime - startTime));
            }
            else if (mUpperBodyState == UpperBodyState::AttackEnd)
            {
                // technically we do not need a pitch for crossbow reload animation,
                // but we should avoid abrupt repositioning
                if (mWeaponType == ESM::Weapon::MarksmanCrossbow)
                    mAnimation->setPitchFactor(std::max(0.f, 1.f - complete * 10.f));
                else
                    mAnimation->setPitchFactor(1.f - complete);
            }
        }

        mAnimation->setAccurateAiming(mUpperBodyState > UpperBodyState::WeaponEquipped);

        return forcestateupdate;
    }

    void CharacterController::updateAnimQueue()
    {
        if (mAnimQueue.empty())
            return;

        if (!mAnimation->isPlaying(mAnimQueue.front().mGroup))
        {
            // Playing animations through mwscript is weird. If an animation is
            // a looping animation (idle or other cyclical animations), then they
            // will end as expected. However, if they are non-looping animations, they
            // will stick around forever or until another animation appears in the queue.
            bool shouldPlayOrRestart = mAnimQueue.size() > 1;
            if (shouldPlayOrRestart || !mAnimQueue.front().mScripted
                || (mAnimQueue.front().mLoopCount == 0 && mAnimQueue.front().mLooping))
            {
                mAnimation->setPlayScriptedOnly(false);
                mAnimation->disable(mAnimQueue.front().mGroup);
                mAnimQueue.pop_front();
                shouldPlayOrRestart = true;
            }
            else
                // A non-looping animation will stick around forever, so only restart if the animation
                // actually was removed for some reason.
                shouldPlayOrRestart = !mAnimation->getInfo(mAnimQueue.front().mGroup)
                    && mAnimation->hasAnimation(mAnimQueue.front().mGroup);

            if (shouldPlayOrRestart)
            {
                // Move on to the remaining items of the queue
                playAnimQueue();
            }
        }
        else
        {
            float complete;
            uint32_t loopcount;
            mAnimation->getInfo(mAnimQueue.front().mGroup, &complete, nullptr, &loopcount);
            mAnimQueue.front().mLoopCount = loopcount;
            mAnimQueue.front().mTime = complete;
        }

        if (!mAnimQueue.empty())
            mAnimation->setLoopingEnabled(mAnimQueue.front().mGroup, mAnimQueue.size() <= 1);
    }

    void CharacterController::playAnimQueue(bool loopStart)
    {
        if (!mAnimQueue.empty())
        {
            clearStateAnimation(mCurrentIdle);
            mIdleState = CharState_SpecialIdle;
            auto priority = mAnimQueue.front().mScripted ? Priority_Scripted : Priority_Default;
            mAnimation->setPlayScriptedOnly(mAnimQueue.front().mScripted);
            if (mAnimQueue.front().mScripted)
                mAnimation->play(mAnimQueue.front().mGroup, priority, MWRender::BlendMask_All, false,
                    mAnimQueue.front().mSpeed, (loopStart ? "loop start" : mAnimQueue.front().mStartKey),
                    mAnimQueue.front().mStopKey, mAnimQueue.front().mTime, mAnimQueue.front().mLoopCount,
                    mAnimQueue.front().mLooping);
            else
                playBlendedAnimation(mAnimQueue.front().mGroup, priority, MWRender::BlendMask_All, false,
                    mAnimQueue.front().mSpeed, (loopStart ? "loop start" : mAnimQueue.front().mStartKey),
                    mAnimQueue.front().mStopKey, mAnimQueue.front().mTime, mAnimQueue.front().mLoopCount,
                    mAnimQueue.front().mLooping);
        }
    }

    void CharacterController::update(float duration)
    {
        if (!mAnimation)
            return;
        MWBase::World* world = MWBase::Environment::get().getWorld();
        MWBase::SoundManager* sndMgr = MWBase::Environment::get().getSoundManager();
        const MWWorld::Class& cls = mPtr.getClass();
        osg::Vec3f movement(0.f, 0.f, 0.f);
        bool isMoving = false;
        float speed = 0.f;

        updateMagicEffects();

        bool isPlayer = mPtr == MWMechanics::getPlayer();
        bool isFirstPersonPlayer = isPlayer && MWBase::Environment::get().getWorld()->isFirstPerson();
        bool godmode = isPlayer && MWBase::Environment::get().getWorld()->getGodModeState();

        float scale = mPtr.getCellRef().getScale();

        if (!Settings::game().mNormaliseRaceSpeed && cls.isNpc() && mPtr.getType() == ESM::NPC::sRecordId)
        {
            const ESM::NPC* npc = mPtr.get<ESM::NPC>()->mBase;
            const ESM::Race* race = world->getStore().get<ESM::Race>().find(npc->mRace);
            float weight = npc->isMale() ? race->mData.mMaleWeight : race->mData.mFemaleWeight;
            scale *= weight;
        }

        if (cls.isActor() && cls.getCreatureStats(mPtr).wasTeleported())
        {
            mSmoothedSpeed = osg::Vec2f();
            cls.getCreatureStats(mPtr).setTeleported(false);
        }

        if (!cls.isActor())
            updateAnimQueue();
        else if (!cls.getCreatureStats(mPtr).isDead())
        {
            bool onground = world->isOnGround(mPtr);
            bool inwater = world->isSwimming(mPtr);
            bool flying = world->isFlying(mPtr);
            bool solid = world->isActorCollisionEnabled(mPtr);
            // Can't run and sneak while flying (see speed formula in Npc/Creature::getSpeed)
            bool sneak
                = cls.getCreatureStats(mPtr).getStance(MWMechanics::CreatureStats::Stance_Sneak) && !flying && !inwater;
            bool isrunning = cls.getCreatureStats(mPtr).getStance(MWMechanics::CreatureStats::Stance_Run) && !flying;
            CreatureStats& stats = cls.getCreatureStats(mPtr);
            Movement& movementSettings = cls.getMovementSettings(mPtr);

            // Oblivion input queues jump until this point so the standard Lua player-control update cannot clear the
            // one-frame request before physics consumes it.
            if (isPlayer && world->getPlayer().consumeOblivionJumpRequest())
                movementSettings.mPosition[2] = 1.f;

            // Force Jump Logic

            isMoving = (std::abs(movementSettings.mPosition[0]) > .5 || std::abs(movementSettings.mPosition[1]) > .5);
            if (!inwater && !flying)
            {
                // Force Jump
                if (stats.getMovementFlag(MWMechanics::CreatureStats::Flag_ForceJump))
                    movementSettings.mPosition[2] = onground ? 1.f : 0.f;
                // Force Move Jump, only jump if they're otherwise moving
                if (stats.getMovementFlag(MWMechanics::CreatureStats::Flag_ForceMoveJump) && isMoving)
                    movementSettings.mPosition[2] = onground ? 1.f : 0.f;
            }

            osg::Vec3f rot = cls.getRotationVector(mPtr);
            osg::Vec3f vec(movementSettings.asVec3());
            movementSettings.mSpeedFactor = std::min(vec.length(), 1.f);
            vec.normalize();

            const bool smoothMovement = Settings::game().mSmoothMovement;
            if (smoothMovement)
            {
                float angle = mPtr.getRefData().getPosition().rot[2];
                osg::Vec2f targetSpeed
                    = Misc::rotateVec2f(osg::Vec2f(vec.x(), vec.y()), -angle) * movementSettings.mSpeedFactor;
                osg::Vec2f delta = targetSpeed - mSmoothedSpeed;
                float speedDelta = movementSettings.mSpeedFactor - mSmoothedSpeed.length();
                float deltaLen = delta.length();

                float maxDelta;
                if (isFirstPersonPlayer)
                    maxDelta = 1;
                else if (std::abs(speedDelta) < deltaLen / 2)
                    // Turning is smooth for player and less smooth for NPCs (otherwise NPC can miss a path point).
                    maxDelta = duration * (isPlayer ? 1.0f / Settings::game().mSmoothMovementPlayerTurningDelay : 6.f);
                else if (isPlayer && speedDelta < -deltaLen / 2)
                    // As soon as controls are released, mwinput switches player from running to walking.
                    // So stopping should be instant for player, otherwise it causes a small twitch.
                    maxDelta = 1;
                else // In all other cases speeding up and stopping are smooth.
                    maxDelta = duration * 3.f;

                if (deltaLen > maxDelta)
                    delta *= maxDelta / deltaLen;
                mSmoothedSpeed += delta;

                osg::Vec2f newSpeed = Misc::rotateVec2f(mSmoothedSpeed, angle);
                movementSettings.mSpeedFactor = newSpeed.normalize();
                vec.x() = newSpeed.x();
                vec.y() = newSpeed.y();

                const float eps = 0.001f;
                if (movementSettings.mSpeedFactor < eps)
                {
                    movementSettings.mSpeedFactor = 0;
                    vec.x() = 0;
                    vec.y() = 1;
                }
                else if ((vec.y() < 0) != mIsMovingBackward)
                {
                    if (targetSpeed.length() < eps || (movementSettings.mPosition[1] < 0) == mIsMovingBackward)
                        vec.y() = mIsMovingBackward ? -eps : eps;
                }
                vec.normalize();
            }

            float effectiveRotation = rot.z();
            bool canMove = cls.getMaxSpeed(mPtr) > 0;
            const bool turnToMovementDirection = Settings::game().mTurnToMovementDirection;
            const bool isBiped = mPtr.getClass().isBipedal(mPtr);
            if (!isBiped || !turnToMovementDirection || isFirstPersonPlayer)
            {
                movementSettings.mIsStrafing = std::abs(vec.x()) > std::abs(vec.y()) * 2;
                stats.setSideMovementAngle(0);
            }
            else if (canMove)
            {
                float targetMovementAngle
                    = vec.y() >= 0 ? std::atan2(-vec.x(), vec.y()) : std::atan2(vec.x(), -vec.y());
                movementSettings.mIsStrafing = (stats.getDrawState() != MWMechanics::DrawState::Nothing || inwater)
                    && std::abs(targetMovementAngle) > osg::DegreesToRadians(60.0f);
                if (movementSettings.mIsStrafing)
                    targetMovementAngle = 0;
                float delta = targetMovementAngle - stats.getSideMovementAngle();
                float cosDelta = cosf(delta);

                if ((vec.y() < 0) == mIsMovingBackward)
                    movementSettings.mSpeedFactor
                        *= std::min(std::max(cosDelta, 0.f) + 0.3f, 1.f); // slow down when turn
                if (std::abs(delta) < osg::DegreesToRadians(20.0f))
                    mIsMovingBackward = vec.y() < 0;

                float maxDelta = osg::PIf * duration * (2.5f - cosDelta);
                delta = std::clamp(delta, -maxDelta, maxDelta);
                stats.setSideMovementAngle(stats.getSideMovementAngle() + delta);
                effectiveRotation += delta;
            }

            mAnimation->setLegsYawRadians(stats.getSideMovementAngle());
            if (stats.getDrawState() == MWMechanics::DrawState::Nothing || inwater)
                mAnimation->setUpperBodyYawRadians(stats.getSideMovementAngle() / 2);
            else
                mAnimation->setUpperBodyYawRadians(stats.getSideMovementAngle() / 4);
            if (smoothMovement && !isPlayer && !inwater)
                mAnimation->setUpperBodyYawRadians(mAnimation->getUpperBodyYawRadians() + mAnimation->getHeadYaw() / 2);

            speed = cls.getCurrentSpeed(mPtr);
            vec.x() *= speed;
            vec.y() *= speed;

            if (isKnockedOut() || isKnockedDown() || isRecovery() || isScriptedAnimPlaying())
                vec = osg::Vec3f();

            CharacterState movestate = CharState_None;
            CharacterState idlestate = CharState_None;
            JumpingState jumpstate = JumpState_None;

            const MWWorld::Store<ESM::GameSetting>& gmst = world->getStore().get<ESM::GameSetting>();
            if (vec.x() != 0.f || vec.y() != 0.f)
            {
                // advance athletics
                if (isPlayer)
                {
                    if (inwater)
                    {
                        mSecondsOfSwimming += duration;
                        while (mSecondsOfSwimming > 1)
                        {
                            cls.skillUsageSucceeded(mPtr, ESM::Skill::Athletics, ESM::Skill::Athletics_SwimOneSecond);
                            mSecondsOfSwimming -= 1;
                        }
                    }
                    else if (isrunning && !sneak)
                    {
                        mSecondsOfRunning += duration;
                        while (mSecondsOfRunning > 1)
                        {
                            cls.skillUsageSucceeded(mPtr, ESM::Skill::Athletics, ESM::Skill::Athletics_RunOneSecond);
                            mSecondsOfRunning -= 1;
                        }
                    }
                }

                if (!godmode && !stats.getFatigue().isNativeProjection())
                {
                    // Native running expenditure is committed with regeneration
                    // in the actor update. This is the legacy movement writer.
                    // reduce fatigue
                    float fatigueLoss = 0.f;
                    static const float fFatigueRunBase = gmst.find("fFatigueRunBase")->mValue.getFloat();
                    static const float fFatigueRunMult = gmst.find("fFatigueRunMult")->mValue.getFloat();
                    static const float fFatigueSwimWalkBase = gmst.find("fFatigueSwimWalkBase")->mValue.getFloat();
                    static const float fFatigueSwimRunBase = gmst.find("fFatigueSwimRunBase")->mValue.getFloat();
                    static const float fFatigueSwimWalkMult = gmst.find("fFatigueSwimWalkMult")->mValue.getFloat();
                    static const float fFatigueSwimRunMult = gmst.find("fFatigueSwimRunMult")->mValue.getFloat();
                    static const float fFatigueSneakBase = gmst.find("fFatigueSneakBase")->mValue.getFloat();
                    static const float fFatigueSneakMult = gmst.find("fFatigueSneakMult")->mValue.getFloat();

                    if (cls.getEncumbrance(mPtr) <= cls.getCapacity(mPtr))
                    {
                        const float encumbrance = cls.getNormalizedEncumbrance(mPtr);
                        if (sneak)
                            fatigueLoss = fFatigueSneakBase + encumbrance * fFatigueSneakMult;
                        else
                        {
                            if (inwater)
                            {
                                if (!isrunning)
                                    fatigueLoss = fFatigueSwimWalkBase + encumbrance * fFatigueSwimWalkMult;
                                else
                                    fatigueLoss = fFatigueSwimRunBase + encumbrance * fFatigueSwimRunMult;
                            }
                            else if (isrunning)
                                fatigueLoss = fFatigueRunBase + encumbrance * fFatigueRunMult;
                        }
                    }
                    fatigueLoss *= duration;
                    fatigueLoss *= movementSettings.mSpeedFactor;
                    DynamicStat<float> fatigue = cls.getCreatureStats(mPtr).getFatigue();
                    fatigue.setCurrent(fatigue.getCurrent() - fatigueLoss, fatigue.getCurrent() < 0);
                    cls.getCreatureStats(mPtr).setFatigue(fatigue);
                }
            }

            bool wasInJump = mInJump;
            mInJump = false;
            const float jumpHeight = cls.getJump(mPtr);
            if (jumpHeight <= 0.f || sneak || inwater || flying || !solid)
            {
                vec.z() = 0.f;
                // Following code might assign some vertical movement regardless, need to reset this manually
                // This is used for jumping detection
                movementSettings.mPosition[2] = 0;
            }

            if (!inwater && !flying && solid)
            {
                // In the air (either getting up —ascending part of jump— or falling).
                if (!onground)
                {
                    mInJump = true;
                    jumpstate = JumpState_InAir;

                    static const float fJumpMoveBase = gmst.find("fJumpMoveBase")->mValue.getFloat();
                    static const float fJumpMoveMult = gmst.find("fJumpMoveMult")->mValue.getFloat();
                    float factor = fJumpMoveBase
                        + fJumpMoveMult * mPtr.getClass().getSkill(mPtr, ESM::Skill::Acrobatics) / 100.f;
                    factor = std::min(1.f, factor);
                    vec.x() *= factor;
                    vec.y() *= factor;
                    vec.z() = 0.0f;
                }
                // Started a jump.
                else if (mJumpState != JumpState_InAir && vec.z() > 0.f)
                {
                    mInJump = true;
                    if (vec.x() == 0 && vec.y() == 0)
                        vec.z() = jumpHeight;
                    else
                    {
                        osg::Vec3f lat(vec.x(), vec.y(), 0.0f);
                        lat.normalize();
                        vec = osg::Vec3f(lat.x(), lat.y(), 1.0f) * jumpHeight * 0.707f;
                    }
                }
            }

            if (!mInJump)
            {
                if (mJumpState == JumpState_InAir && !flying && solid && wasInJump)
                {
                    float height = cls.getCreatureStats(mPtr).land(isPlayer);
                    float healthLost = 0.f;
                    if (!inwater)
                        healthLost = getFallDamage(mPtr, height);

                    if (healthLost > 0.0f)
                    {
                        const float fatigueTerm = cls.getCreatureStats(mPtr).getFatigueTerm();

                        // inflict fall damages
                        if (!godmode)
                        {
                            DynamicStat<float> health = cls.getCreatureStats(mPtr).getHealth();
                            float realHealthLost = healthLost * (1.0f - 0.25f * fatigueTerm);
                            health.setCurrent(health.getCurrent() - realHealthLost);
                            cls.getCreatureStats(mPtr).setHealth(health);
                            sndMgr->playSound3D(mPtr, ESM::RefId::stringRefId("Health Damage"), 1.0f, 1.0f);
                            if (isPlayer)
                                MWBase::Environment::get().getWindowManager()->activateHitOverlay();
                        }

                        const float acrobaticsSkill = cls.getSkill(mPtr, ESM::Skill::Acrobatics);
                        if (healthLost > (acrobaticsSkill * fatigueTerm))
                        {
                            if (!godmode)
                                cls.getCreatureStats(mPtr).setKnockedDown(true);
                        }
                        else
                        {
                            // report acrobatics progression
                            if (isPlayer)
                                cls.skillUsageSucceeded(mPtr, ESM::Skill::Acrobatics, ESM::Skill::Acrobatics_Fall);
                        }
                    }

                    if (mPtr.getClass().isNpc())
                    {
                        std::string_view sound;
                        osg::Vec3f pos(mPtr.getRefData().getPosition().asVec3());
                        if (world->isUnderwater(mPtr.getCell(), pos) || world->isWalkingOnWater(mPtr))
                            sound = "DefaultLandWater";
                        else if (onground)
                            sound = "DefaultLand";

                        if (!sound.empty())
                            sndMgr->playSound3D(mPtr, ESM::RefId::stringRefId(sound), 1.f, 1.f, MWSound::Type::Foot,
                                MWSound::PlayMode::NoPlayerLocal);
                    }
                }

                if (mAnimation->isPlaying(mCurrentJump))
                    jumpstate = JumpState_Landing;

                vec.z() = 0.0f;

                if (movementSettings.mIsStrafing)
                {
                    if (vec.x() > 0.0f)
                        movestate = (inwater ? (isrunning ? CharState_SwimRunRight : CharState_SwimWalkRight)
                                             : (sneak ? CharState_SneakRight
                                                      : (isrunning ? CharState_RunRight : CharState_WalkRight)));
                    else if (vec.x() < 0.0f)
                        movestate = (inwater
                                ? (isrunning ? CharState_SwimRunLeft : CharState_SwimWalkLeft)
                                : (sneak ? CharState_SneakLeft : (isrunning ? CharState_RunLeft : CharState_WalkLeft)));
                }
                else if (vec.length2() > 0.0f)
                {
                    if (vec.y() >= 0.0f)
                        movestate = (inwater ? (isrunning ? CharState_SwimRunForward : CharState_SwimWalkForward)
                                             : (sneak ? CharState_SneakForward
                                                      : (isrunning ? CharState_RunForward : CharState_WalkForward)));
                    else
                        movestate = (inwater
                                ? (isrunning ? CharState_SwimRunBack : CharState_SwimWalkBack)
                                : (sneak ? CharState_SneakBack : (isrunning ? CharState_RunBack : CharState_WalkBack)));
                }
                else
                {
                    // It seems only bipedal actors use turning animations.
                    // Also do not use turning animations in the first-person view and when sneaking.
                    if (!sneak && !isFirstPersonPlayer && isBiped)
                    {
                        if (effectiveRotation > 0.f)
                            movestate = inwater ? CharState_SwimTurnRight : CharState_TurnRight;
                        else if (effectiveRotation < 0.f)
                            movestate = inwater ? CharState_SwimTurnLeft : CharState_TurnLeft;
                    }
                }
            }

            if (turnToMovementDirection && !isFirstPersonPlayer && isBiped
                && (movestate == CharState_SwimRunForward || movestate == CharState_SwimWalkForward
                    || movestate == CharState_SwimRunBack || movestate == CharState_SwimWalkBack))
            {
                float swimmingPitch = mAnimation->getBodyPitchRadians();
                float targetSwimmingPitch = -mPtr.getRefData().getPosition().rot[0];
                float maxSwimPitchDelta = 3.0f * duration;
                swimmingPitch += std::clamp(targetSwimmingPitch - swimmingPitch, -maxSwimPitchDelta, maxSwimPitchDelta);
                mAnimation->setBodyPitchRadians(swimmingPitch);
            }
            else
                mAnimation->setBodyPitchRadians(0);

            if (inwater && isPlayer && !isFirstPersonPlayer && Settings::game().mSwimUpwardCorrection)
            {
                const float swimUpwardCoef = Settings::game().mSwimUpwardCoef;
                vec.z() = std::abs(vec.y()) * swimUpwardCoef;
                vec.y() *= std::sqrt(1.0f - swimUpwardCoef * swimUpwardCoef);
            }

            if (isBiped)
            {
                if (mTurnAnimationThreshold > 0)
                    mTurnAnimationThreshold -= duration;

                if (movestate == CharState_TurnRight || movestate == CharState_TurnLeft
                    || movestate == CharState_SwimTurnRight || movestate == CharState_SwimTurnLeft)
                {
                    mTurnAnimationThreshold = 0.05f;
                }
                else if (movestate == CharState_None && isTurning() && mTurnAnimationThreshold > 0)
                {
                    movestate = mMovementState;
                }
            }

            if (movestate != CharState_None)
            {
                clearAnimQueue();
                jumpstate = JumpState_None;
            }

            updateAnimQueue();
            if (!mAnimQueue.empty())
                idlestate = CharState_SpecialIdle;
            else if (sneak && !mInJump)
                idlestate = CharState_IdleSneak;
            else
                idlestate = CharState_Idle;

            if (inwater)
                idlestate = CharState_IdleSwim;

            if (!mSkipAnim)
            {
                refreshCurrentAnims(idlestate, movestate, jumpstate, updateWeaponState(duration));
                updateIdleStormState(inwater);
            }

            if (isTurning())
            {
                if (duration > 0)
                {
                    float turnSpeed = std::min(1.5f, std::abs(rot.z()) / duration / static_cast<float>(osg::PI));
                    mAnimation->adjustSpeedMult(mCurrentMovement, turnSpeed);
                }
            }
            else if (mMovementState != CharState_None && mAdjustMovementAnimSpeed)
            {
                // Vanilla caps the played animation speed.
                const float maxSpeedMult = 10.f;
                const float speedMult = speed / mMovementAnimSpeed;
                mAnimation->adjustSpeedMult(mCurrentMovement, std::min(maxSpeedMult, speedMult));
                // Make sure the actual speed is the "expected" speed even though the animation is slower
                if (isMovementAnimationControlled())
                    scale *= std::max(1.f, speedMult / maxSpeedMult);
            }

            if (!mSkipAnim)
            {
                if (!isKnockedDown() && !isKnockedOut())
                {
                    if (rot != osg::Vec3f())
                        world->rotateObject(mPtr, rot, true);
                }
                else // avoid z-rotating for knockdown
                {
                    if (rot.x() != 0 && rot.y() != 0)
                    {
                        rot.z() = 0.0f;
                        world->rotateObject(mPtr, rot, true);
                    }
                }

                updateHeadTracking(duration);
            }

            movement = vec;
            movementSettings.mPosition[0] = movementSettings.mPosition[1] = 0;

            // Can't reset jump state (mPosition[2]) here in full; we don't know for sure whether the PhysicsSystem will
            // actually handle it in this frame due to the fixed minimum timestep used for the physics update. It will
            // be reset in PhysicsSystem::move once the jump is handled.
            if (movement.z() == 0.f)
                movementSettings.mPosition[2] = 0;
        }
        else if (cls.getCreatureStats(mPtr).isDead())
        {
            // initial start of death animation for actors that started the game as dead
            // not done in constructor since we need to give scripts a chance to set the mSkipAnim flag
            if (!mSkipAnim && mDeathState != CharState_None && mCurrentDeath.empty())
            {
                // Fast-forward death animation to end for persisting corpses or corpses after end of death animation
                if (cls.isPersistent(mPtr) || cls.getCreatureStats(mPtr).isDeathAnimationFinished())
                    playDeath(1.f, mDeathState);
            }
        }

        // Original actor contact dispatch observes the prior Contact phase.
        dispatchOblivionMeleeContact();
        advanceOblivionMeleePlayback(duration);
        osg::Vec3f movementFromAnimation
            = mAnimation->runAnimation(mSkipAnim && !isScriptedAnimPlaying() ? 0.f : duration);
        persistOblivionMeleeProgress();
        finishOblivionMeleePlayback();

        if (mPtr.getClass().isActor() && !isScriptedAnimPlaying())
        {
            if (isMovementAnimationControlled())
            {
                if (duration != 0.f && movementFromAnimation != osg::Vec3f())
                {
                    movementFromAnimation /= duration;

                    // Ensure we're moving in the right general direction.
                    // In vanilla, all horizontal movement is taken from animations, even when moving diagonally (which
                    // doesn't have a corresponding animation). So to achieve diagonal movement, we have to rotate the
                    // movement taken from the animation to the intended direction.
                    //
                    // Note that while a complete movement animation cycle will have a well defined direction, no
                    // individual frame will, and therefore we have to determine the direction based on the currently
                    // playing cycle instead.
                    if (speed > 0.f)
                    {
                        float animMovementAngle = getAnimationMovementDirection();
                        float targetMovementAngle = std::atan2(-movement.x(), movement.y());
                        float diff = targetMovementAngle - animMovementAngle;
                        movementFromAnimation = osg::Quat(diff, osg::Vec3f(0, 0, 1)) * movementFromAnimation;
                    }

                    movement = movementFromAnimation;
                }
                else
                {
                    movement = osg::Vec3f();
                }
            }
            else if (mSkipAnim)
            {
                movement = osg::Vec3f();
            }

            if (mFloatToSurface && world->isSwimming(mPtr))
            {
                if (cls.getCreatureStats(mPtr).isDead()
                    || (!godmode
                        && cls.getCreatureStats(mPtr)
                                .getMagicEffects()
                                .getOrDefault(ESM::MagicEffect::Paralyze)
                                .getModifier()
                            > 0))
                {
                    movement.z() = 1.0;
                }
            }

            movement.x() *= scale;
            movement.y() *= scale;
            world->queueMovement(mPtr, movement);
        }

        mSkipAnim = false;

        mAnimation->enableHeadAnimation(cls.isActor() && !cls.getCreatureStats(mPtr).isDead());

        // Werewolf running sound logic
        if (isPlayer)
        {
            bool playWolfRun = false;
            if (movement != osg::Vec3f() && isMoving && mHitState == CharState_None)
            {
                if (mWeaponType == ESM::Weapon::None || mUpperBodyState == UpperBodyState::Unequipping)
                {
                    const NpcStats& stats = cls.getNpcStats(mPtr);
                    if (stats.isWerewolf() && !stats.isDead())
                    {
                        const bool sneaking = stats.getStance(MWMechanics::CreatureStats::Stance_Sneak);
                        const bool running = stats.getStance(MWMechanics::CreatureStats::Stance_Run);
                        playWolfRun = running && !sneaking && !world->isSwimming(mPtr);
                    }
                }
            }

            if (playWolfRun)
            {
                if (!sndMgr->getSoundPlaying(mPtr, wolfRun))
                    sndMgr->playSound3D(mPtr, wolfRun, 1.0f, 1.0f, MWSound::Type::Foot, MWSound::PlayMode::Loop);
            }
            else
            {
                if (sndMgr->getSoundPlaying(mPtr, wolfRun))
                    sndMgr->stopSound3D(mPtr, wolfRun);
            }
        }
    }

    void CharacterController::persistAnimationState() const
    {
        persistOblivionMeleeProgress();
        ESM::AnimationState& state = mPtr.getRefData().getAnimationState();

        state.mScriptedAnims.clear();
        for (AnimationQueue::const_iterator iter = mAnimQueue.begin(); iter != mAnimQueue.end(); ++iter)
        {
            // TODO: Probably want to presist lua animations too
            if (!iter->mScripted)
                continue;

            ESM::AnimationState::ScriptedAnimation anim;
            anim.mGroup = iter->mGroup;

            if (iter == mAnimQueue.begin() && mAnimation)
            {
                float complete;
                uint32_t loopcount;
                mAnimation->getInfo(anim.mGroup, &complete, nullptr, &loopcount);
                anim.mTime = complete;
                anim.mLoopCount = loopcount;
            }
            else
            {
                anim.mLoopCount = iter->mLoopCount;
                anim.mTime = 0.f;
            }

            state.mScriptedAnims.push_back(std::move(anim));
        }
    }

    void CharacterController::unpersistAnimationState()
    {
        const ESM::AnimationState& state = mPtr.getRefData().getAnimationState();

        if (!state.mScriptedAnims.empty())
        {
            clearAnimQueue();
            for (const ESM::AnimationState::ScriptedAnimation& animation : state.mScriptedAnims)
            {
                AnimationQueueEntry entry;
                entry.mGroup = animation.mGroup;
                entry.mLoopCount = static_cast<uint32_t>(
                    std::min<uint64_t>(animation.mLoopCount, std::numeric_limits<uint32_t>::max()));
                entry.mLooping = mAnimation->isLoopingAnimation(entry.mGroup);
                entry.mScripted = true;
                entry.mStartKey = "start";
                entry.mStopKey = "stop";
                entry.mSpeed = 1.f;
                entry.mTime = animation.mTime;
                if (animation.mAbsolute)
                {
                    float start = mAnimation->getTextKeyTime(animation.mGroup + ": start");
                    float stop = mAnimation->getTextKeyTime(animation.mGroup + ": stop");
                    float time = std::clamp(animation.mTime, start, stop);
                    entry.mTime = (time - start) / (stop - start);
                }

                mAnimQueue.push_back(std::move(entry));
            }

            playAnimQueue();
        }
    }

    void CharacterController::playBlendedAnimation(const std::string& groupname, const MWRender::AnimPriority& priority,
        int blendMask, bool autodisable, float speedmult, std::string_view start, std::string_view stop,
        float startpoint, uint32_t loops, bool loopfallback) const
    {
        if (mLuaAnimations)
            MWBase::Environment::get().getLuaManager()->playAnimation(mPtr, groupname, priority, blendMask, autodisable,
                speedmult, start, stop, startpoint, loops, loopfallback);
        else
            mAnimation->play(
                groupname, priority, blendMask, autodisable, speedmult, start, stop, startpoint, loops, loopfallback);
    }

    bool CharacterController::playGroup(std::string_view groupname, int mode, uint32_t count, bool scripted)
    {
        if (!mAnimation || !mAnimation->hasAnimation(groupname))
            return false;

        // We should not interrupt scripted animations with non-scripted ones
        if (isScriptedAnimPlaying() && !scripted)
            return true;

        bool looping = mAnimation->isLoopingAnimation(groupname);

        // If this animation is a looped animation that is already playing
        // and has not yet reached the end of the loop, allow it to continue animating with its existing loop count
        // and remove any other animations that were queued.
        // This emulates observed behavior from the original allows the script "OutsideBanner" to animate banners
        // correctly.
        if (!mAnimQueue.empty() && mAnimQueue.front().mGroup == groupname && looping
            && mAnimation->isPlaying(groupname))
        {
            float endOfLoop = mAnimation->getTextKeyTime(mAnimQueue.front().mGroup + ": loop stop");

            if (endOfLoop < 0) // if no Loop Stop key was found, use the Stop key
                endOfLoop = mAnimation->getTextKeyTime(mAnimQueue.front().mGroup + ": stop");

            if (endOfLoop > 0 && (mAnimation->getCurrentTime(mAnimQueue.front().mGroup) < endOfLoop))
            {
                mAnimQueue.resize(1);
                return true;
            }
        }

        // The loop count in vanilla is weird.
        // if played with a count of 0, all objects play exactly once from start to stop.
        // But if the count is x > 0, actors and non-actors behave differently. actors will loop
        // exactly x times, while non-actors will loop x+1 instead.
        if (mPtr.getClass().isActor() && count > 0)
            count--;

        AnimationQueueEntry entry;
        entry.mGroup = groupname;
        entry.mLoopCount = count;
        entry.mTime = 0.f;
        // "PlayGroup idle" is a special case, used to remove to stop scripted animations playing
        entry.mScripted = (scripted && groupname != "idle");
        entry.mLooping = looping;
        entry.mSpeed = 1.f;
        entry.mStartKey = ((mode == 2) ? "loop start" : "start");
        entry.mStopKey = "stop";

        bool playImmediately = false;

        if (mode != 0 || mAnimQueue.empty() || !isAnimPlaying(mAnimQueue.front().mGroup))
        {
            clearAnimQueue(scripted);

            playImmediately = true;
        }
        else
        {
            mAnimQueue.resize(1);
        }

        mAnimQueue.push_back(std::move(entry));

        if (playImmediately)
            playAnimQueue(mode == 2);

        return true;
    }

    bool CharacterController::playGroupLua(std::string_view groupname, float speed, std::string_view startKey,
        std::string_view stopKey, uint32_t loops, bool forceLoop)
    {
        // Note: In mwscript, "idle" is a special case used to clear the anim queue.
        // In lua we offer an explicit clear method instead so this method does not treat "idle" special.

        if (!mAnimation || !mAnimation->hasAnimation(groupname))
            return false;

        AnimationQueueEntry entry;
        entry.mGroup = groupname;
        // Note: MWScript gives one less loop to actors than non-actors.
        // But this is the Lua version. We don't need to reproduce this weirdness here.
        entry.mLoopCount = loops;
        entry.mStartKey = startKey;
        entry.mStopKey = stopKey;
        entry.mLooping = mAnimation->isLoopingAnimation(groupname) || forceLoop;
        entry.mScripted = true;
        entry.mSpeed = speed;
        entry.mTime = 0;

        if (mAnimQueue.size() > 1)
            mAnimQueue.resize(1);
        mAnimQueue.push_back(std::move(entry));

        if (mAnimQueue.size() == 1)
            playAnimQueue();

        return true;
    }

    void CharacterController::enableLuaAnimations(bool enable)
    {
        mLuaAnimations = enable;
    }

    void CharacterController::skipAnim()
    {
        mSkipAnim = true;
    }

    bool CharacterController::isScriptedAnimPlaying() const
    {
        // If the front of the anim queue is scripted, morrowind treats it as if it's
        // still playing even if it's actually done.
        if (!mAnimQueue.empty())
            return mAnimQueue.front().mScripted;

        return false;
    }

    bool CharacterController::isAnimPlaying(std::string_view groupName) const
    {
        if (mAnimation == nullptr)
            return false;
        return mAnimation->isPlaying(groupName);
    }

    bool CharacterController::isMovementAnimationControlled() const
    {
        if (mHitState != CharState_None || mDeathState != CharState_None)
            return true;

        if (Settings::game().mPlayerMovementIgnoresAnimation && mPtr == getPlayer())
            return false;

        if (mInJump)
            return false;

        bool movementAnimationControlled = mIdleState != CharState_None;
        if (mMovementState != CharState_None)
            movementAnimationControlled = mMovementAnimationHasMovement;
        return movementAnimationControlled;
    }

    void CharacterController::clearAnimQueue(bool clearScriptedAnims)
    {
        // Do not interrupt scripted animations, if we want to keep them
        if (mAnimation && (!isScriptedAnimPlaying() || clearScriptedAnims) && !mAnimQueue.empty())
            mAnimation->disable(mAnimQueue.front().mGroup);

        if (clearScriptedAnims)
        {
            if (mAnimation)
                mAnimation->setPlayScriptedOnly(false);
            mAnimQueue.clear();
            return;
        }

        std::erase_if(mAnimQueue, [](const AnimationQueueEntry& entry) { return !entry.mScripted; });
    }

    void CharacterController::forceStateUpdate()
    {
        if (!mAnimation)
            return;
        clearAnimQueue();

        // Make sure we canceled the current attack or spellcasting,
        // because we disabled attack animations anyway.
        mCanCast = false;
        mCastingScriptedSpell = false;
        setAttackingOrSpell(false);
        if (mUpperBodyState != UpperBodyState::None)
            mUpperBodyState = UpperBodyState::WeaponEquipped;

        refreshCurrentAnims(mIdleState, mMovementState, mJumpState, true);

        if (mDeathState != CharState_None)
        {
            playRandomDeath();
        }

        updateAnimQueue();

        mAnimation->runAnimation(0.f);
    }

    CharacterController::KillResult CharacterController::kill()
    {
        if (mDeathState == CharState_None)
        {
            playRandomDeath();
            resetCurrentIdleState();
            return Result_DeathAnimStarted;
        }

        MWMechanics::CreatureStats& cStats = mPtr.getClass().getCreatureStats(mPtr);
        if (isAnimPlaying(mCurrentDeath))
            return Result_DeathAnimPlaying;
        if (!cStats.isDeathAnimationFinished())
        {
            cStats.setDeathAnimationFinished(true);
            return Result_DeathAnimJustFinished;
        }
        return Result_DeathAnimFinished;
    }

    void CharacterController::resurrect()
    {
        if (mDeathState == CharState_None)
            return;

        resetCurrentDeathState();
        mWeaponType = ESM::Weapon::None;
    }

    void CharacterController::updateContinuousVfx() const
    {
        // Keeping track of when to stop a continuous VFX seems to be very difficult to do inside the spells code,
        // as it's extremely spread out (ActiveSpells, Spells, InventoryStore effects, etc...) so we do it here.

        // Stop any effects that are no longer active
        std::vector<std::string_view> effects = mAnimation->getLoopingEffects();

        for (std::string_view effectStr : effects)
        {
            auto effectId = ESM::RefId::deserializeText(effectStr);

            if (MWBase::Environment::get().getESMStore()->get<ESM::MagicEffect>().search(effectId)
                && (mPtr.getClass().getCreatureStats(mPtr).isDeathAnimationFinished()
                    || mPtr.getClass()
                            .getCreatureStats(mPtr)
                            .getMagicEffects()
                            .getOrDefault(MWMechanics::EffectKey(effectId))
                            .getMagnitude()
                        <= 0))
                mAnimation->removeEffect(effectStr);
        }
    }

    void CharacterController::updateMagicEffects() const
    {
        if (!mPtr.getClass().isActor())
            return;

        float light = mPtr.getClass()
                          .getCreatureStats(mPtr)
                          .getMagicEffects()
                          .getOrDefault(ESM::MagicEffect::Light)
                          .getMagnitude();
        mAnimation->setLightEffect(light);

        // If you're dead you don't care about whether you've started/stopped being a vampire or not
        if (mPtr.getClass().getCreatureStats(mPtr).isDead())
            return;

        bool vampire = mPtr.getClass()
                           .getCreatureStats(mPtr)
                           .getMagicEffects()
                           .getOrDefault(ESM::MagicEffect::Vampirism)
                           .getMagnitude()
            > 0.0f;
        mAnimation->setVampire(vampire);
    }

    void CharacterController::setVisibility(float visibility) const
    {
        if (!mAnimation)
            return;
        // We should take actor's invisibility in account
        if (mPtr.getClass().isActor())
        {
            float alpha = 1.f;
            if (mPtr.getClass()
                    .getCreatureStats(mPtr)
                    .getMagicEffects()
                    .getOrDefault(ESM::MagicEffect::Invisibility)
                    .getModifier()) // Ignore base magnitude (see bug #3555).
            {
                if (mPtr == getPlayer())
                    alpha = 0.25f;
                else
                    alpha = 0.05f;
            }
            float chameleon = mPtr.getClass()
                                  .getCreatureStats(mPtr)
                                  .getMagicEffects()
                                  .getOrDefault(ESM::MagicEffect::Chameleon)
                                  .getMagnitude();
            if (chameleon)
            {
                alpha *= std::clamp(1.f - chameleon / 100.f, 0.25f, 0.75f);
            }

            visibility = std::min(visibility, alpha);
        }

        // TODO: implement a dithering shader rather than just change object transparency.
        mAnimation->setAlpha(visibility);
    }

    std::string_view CharacterController::getMovementBasedAttackType() const
    {
        float* move = mPtr.getClass().getMovementSettings(mPtr).mPosition;
        if (std::abs(move[1]) > std::abs(move[0]) + 0.2f) // forward-backward
            return "thrust";
        if (std::abs(move[0]) > std::abs(move[1]) + 0.2f) // sideway
            return "slash";
        return "chop";
    }

    bool CharacterController::isRandomAttackAnimation(std::string_view group)
    {
        return (group == "attack1" || group == "swimattack1" || group == "attack2" || group == "swimattack2"
            || group == "attack3" || group == "swimattack3");
    }

    bool CharacterController::isAttackPreparing() const
    {
        auto* world = dynamic_cast<MWWorld::World*>(
            MWBase::Environment::get().getWorld().operator MWBase::World*());
        auto* service = world && world->getGameProfile() == ESM::GameProfile::Oblivion
            ? world->getOblivionCombatService() : nullptr;
        if (service)
        {
            const auto actor = mPtr == world->getPlayerPtr() ? ESM::FormKey::dynamic("player", 1)
                : mPtr.getCellRef().getFormKey();
            const auto* state = service->findMeleeState(actor);
            if (state && state->mStrike
                && state->mStrike->mKind <= ESM4::MeleeStrikeKind::Right)
                return state->mStrike->mOrdinaryPhase == ESM4::OrdinaryMeleePhase::Start;
        }
        return mUpperBodyState == UpperBodyState::AttackWindUp;
    }

    bool CharacterController::isCastingSpell() const
    {
        return mCastingScriptedSpell || mUpperBodyState == UpperBodyState::Casting;
    }

    bool CharacterController::isReadyToBlock() const
    {
        return mAnimation && updateCarriedLeftVisible(mWeaponType);
    }

    bool CharacterController::isKnockedDown() const
    {
        return mHitState == CharState_KnockDown || mHitState == CharState_SwimKnockDown;
    }

    bool CharacterController::isKnockedOut() const
    {
        return mHitState == CharState_KnockOut || mHitState == CharState_SwimKnockOut;
    }

    bool CharacterController::isTurning() const
    {
        return mMovementState == CharState_TurnLeft || mMovementState == CharState_TurnRight
            || mMovementState == CharState_SwimTurnLeft || mMovementState == CharState_SwimTurnRight;
    }

    bool CharacterController::isRecovery() const
    {
        return mHitState == CharState_Hit || mHitState == CharState_SwimHit;
    }

    bool CharacterController::isAttackingOrSpell() const
    {
        return mUpperBodyState != UpperBodyState::None && mUpperBodyState != UpperBodyState::WeaponEquipped;
    }

    bool CharacterController::isSneaking() const
    {
        return mIdleState == CharState_IdleSneak || mMovementState == CharState_SneakForward
            || mMovementState == CharState_SneakBack || mMovementState == CharState_SneakLeft
            || mMovementState == CharState_SneakRight;
    }

    bool CharacterController::isRunning() const
    {
        return mMovementState == CharState_RunForward || mMovementState == CharState_RunBack
            || mMovementState == CharState_RunLeft || mMovementState == CharState_RunRight
            || mMovementState == CharState_SwimRunForward || mMovementState == CharState_SwimRunBack
            || mMovementState == CharState_SwimRunLeft || mMovementState == CharState_SwimRunRight;
    }

    void CharacterController::setAttackingOrSpell(bool attackingOrSpell) const
    {
        mPtr.getClass().getCreatureStats(mPtr).setAttackingOrSpell(attackingOrSpell);
    }

    void CharacterController::castSpell(const ESM::RefId& spellId, bool scriptedSpell)
    {
        setAttackingOrSpell(true);
        mCastingScriptedSpell = scriptedSpell;
        ActionSpell action = ActionSpell(spellId);
        action.prepare(mPtr);
    }

    void CharacterController::setAIAttackType(std::string_view attackType)
    {
        mAttackType = attackType;
    }

    std::string_view CharacterController::getRandomAttackType()
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();
        float random = Misc::Rng::rollProbability(world->getPrng());
        if (random >= 2 / 3.f)
            return "thrust";
        if (random >= 1 / 3.f)
            return "slash";
        return "chop";
    }

    bool CharacterController::readyToPrepareAttack() const
    {
        return (mHitState == CharState_None || mHitState == CharState_Block)
            && mUpperBodyState <= UpperBodyState::WeaponEquipped;
    }

    bool CharacterController::readyToStartAttack() const
    {
        if (mHitState != CharState_None && mHitState != CharState_Block)
            return false;

        return mUpperBodyState == UpperBodyState::WeaponEquipped;
    }

    float CharacterController::getAttackStrength() const
    {
        return mAttackStrength;
    }

    bool CharacterController::getAttackingOrSpell() const
    {
        return mPtr.getClass().getCreatureStats(mPtr).getAttackingOrSpell();
    }

    std::string_view CharacterController::getDesiredAttackType() const
    {
        return mPtr.getClass().getCreatureStats(mPtr).getAttackType();
    }

    void CharacterController::setActive(int active) const
    {
        if (mAnimation)
            mAnimation->setActive(active);
    }

    void CharacterController::setHeadTrackTarget(const MWWorld::ConstPtr& target)
    {
        mHeadTrackTarget = target;
    }

    void CharacterController::playSwishSound() const
    {
        static ESM::RefId weaponSwish = ESM::RefId::stringRefId("Weapon Swish");
        const ESM::RefId* soundId = &weaponSwish;
        float volume = 0.98f + mAttackStrength * 0.02f;
        float pitch = 0.75f + mAttackStrength * 0.4f;

        const MWWorld::Class& cls = mPtr.getClass();
        if (cls.isNpc() && cls.getNpcStats(mPtr).isWerewolf())
        {
            MWBase::World* world = MWBase::Environment::get().getWorld();
            const MWWorld::ESMStore& store = world->getStore();
            const ESM::Sound* sound = store.get<ESM::Sound>().searchRandom("WolfSwing", world->getPrng());
            if (sound)
                soundId = &sound->mId;
        }

        if (!soundId->empty())
            MWBase::Environment::get().getSoundManager()->playSound3D(mPtr, *soundId, volume, pitch);
    }

    float CharacterController::getAnimationMovementDirection() const
    {
        if (!mAnimation)
            return 0.f;
        switch (mMovementState)
        {
            case CharState_RunLeft:
            case CharState_SneakLeft:
            case CharState_SwimWalkLeft:
            case CharState_SwimRunLeft:
            case CharState_WalkLeft:
                return osg::PI_2f;
            case CharState_RunRight:
            case CharState_SneakRight:
            case CharState_SwimWalkRight:
            case CharState_SwimRunRight:
            case CharState_WalkRight:
                return -osg::PI_2f;
            case CharState_RunForward:
            case CharState_SneakForward:
            case CharState_SwimRunForward:
            case CharState_SwimWalkForward:
            case CharState_WalkForward:
                return mAnimation->getLegsYawRadians();
            case CharState_RunBack:
            case CharState_SneakBack:
            case CharState_SwimWalkBack:
            case CharState_SwimRunBack:
            case CharState_WalkBack:
                return mAnimation->getLegsYawRadians() - osg::PIf;
            default:
                return 0.0f;
        }
    }

    void CharacterController::updateHeadTracking(float duration)
    {
        const osg::Node* head = mAnimation->getNode("Bip01 Head");
        if (!head)
            return;

        float zAngleRadians = 0.f;
        float xAngleRadians = 0.f;

        if (!mHeadTrackTarget.isEmpty())
        {
            osg::NodePathList nodepaths = head->getParentalNodePaths();
            if (nodepaths.empty())
                return;
            osg::Matrixf mat = osg::computeLocalToWorld(nodepaths[0]);
            osg::Vec3f headPos = mat.getTrans();

            osg::Vec3f direction;
            if (const MWRender::Animation* anim = MWBase::Environment::get().getWorld()->getAnimation(mHeadTrackTarget))
            {
                const osg::Node* node = anim->getNode("Head");
                if (node == nullptr)
                    node = anim->getNode("Bip01 Head");
                if (node != nullptr)
                {
                    nodepaths = node->getParentalNodePaths();
                    if (!nodepaths.empty())
                        direction = osg::computeLocalToWorld(nodepaths[0]).getTrans() - headPos;
                }
                else
                    // no head node to look at, fall back to look at center of collision box
                    direction = MWBase::Environment::get().getWorld()->aimToTarget(mPtr, mHeadTrackTarget, false);
            }
            direction.normalize();

            if (!mPtr.getRefData().getBaseNode())
                return;
            const osg::Vec3f actorDirection = mPtr.getRefData().getBaseNode()->getAttitude() * osg::Vec3f(0, 1, 0);

            zAngleRadians
                = std::atan2(actorDirection.x(), actorDirection.y()) - std::atan2(direction.x(), direction.y());
            zAngleRadians = static_cast<float>(
                Misc::normalizeAngle(zAngleRadians - mAnimation->getHeadYaw()) + mAnimation->getHeadYaw());
            zAngleRadians *= (1 - direction.z() * direction.z());
            xAngleRadians = std::asin(direction.z());
        }

        const float xLimit = osg::DegreesToRadians(40.f);
        const float zLimit = osg::DegreesToRadians(30.f);
        float zLimitOffset = mAnimation->getUpperBodyYawRadians();
        xAngleRadians = std::clamp(xAngleRadians, -xLimit, xLimit);
        zAngleRadians = std::clamp(zAngleRadians, -zLimit + zLimitOffset, zLimit + zLimitOffset);

        float factor = duration * 5;
        factor = std::min(factor, 1.f);
        xAngleRadians = (1.f - factor) * mAnimation->getHeadPitch() + factor * xAngleRadians;
        zAngleRadians = (1.f - factor) * mAnimation->getHeadYaw() + factor * zAngleRadians;

        mAnimation->setHeadPitch(xAngleRadians);
        mAnimation->setHeadYaw(zAngleRadians);
    }

    MWWorld::MovementDirectionFlags CharacterController::getSupportedMovementDirections() const
    {
        if (!mAnimation)
            return 0;
        using namespace std::string_view_literals;
        // There are fallbacks in the CharacterController::refreshMovementAnims for certain animations. Arrays below
        // represent them.
        constexpr std::array all = { ""sv };
        constexpr std::array walk = { "walk"sv };
        constexpr std::array swimWalk = { "swimwalk"sv, "walk"sv };
        constexpr std::array sneak = { "sneak"sv };
        constexpr std::array run = { "run"sv, "walk"sv };
        constexpr std::array swimRun = { "swimrun"sv, "run"sv, "walk"sv };
        constexpr std::array swim = { "swim"sv };
        switch (mMovementState)
        {
            case CharState_None:
            case CharState_SpecialIdle:
            case CharState_Idle:
            case CharState_IdleSwim:
            case CharState_IdleSneak:
                return mAnimation->getSupportedMovementDirections(all);
            case CharState_WalkForward:
            case CharState_WalkBack:
            case CharState_WalkLeft:
            case CharState_WalkRight:
                return mAnimation->getSupportedMovementDirections(walk);
            case CharState_SwimWalkForward:
            case CharState_SwimWalkBack:
            case CharState_SwimWalkLeft:
            case CharState_SwimWalkRight:
                return mAnimation->getSupportedMovementDirections(swimWalk);
            case CharState_RunForward:
            case CharState_RunBack:
            case CharState_RunLeft:
            case CharState_RunRight:
                return mAnimation->getSupportedMovementDirections(run);
            case CharState_SwimRunForward:
            case CharState_SwimRunBack:
            case CharState_SwimRunLeft:
            case CharState_SwimRunRight:
                return mAnimation->getSupportedMovementDirections(swimRun);
            case CharState_SneakForward:
            case CharState_SneakBack:
            case CharState_SneakLeft:
            case CharState_SneakRight:
                return mAnimation->getSupportedMovementDirections(sneak);
            case CharState_TurnLeft:
            case CharState_TurnRight:
                return mAnimation->getSupportedMovementDirections(all);
            case CharState_SwimTurnLeft:
            case CharState_SwimTurnRight:
                return mAnimation->getSupportedMovementDirections(swim);
            case CharState_Death1:
            case CharState_Death2:
            case CharState_Death3:
            case CharState_Death4:
            case CharState_Death5:
            case CharState_SwimDeath:
            case CharState_SwimDeathKnockDown:
            case CharState_SwimDeathKnockOut:
            case CharState_DeathKnockDown:
            case CharState_DeathKnockOut:
            case CharState_Hit:
            case CharState_SwimHit:
            case CharState_KnockDown:
            case CharState_KnockOut:
            case CharState_SwimKnockDown:
            case CharState_SwimKnockOut:
            case CharState_Block:
                return mAnimation->getSupportedMovementDirections(all);
        }
        return 0;
    }

}
