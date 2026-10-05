#ifndef OPENMW_MWWORLD_OBLIVIONCOMBATDATA_H
#define OPENMW_MWWORLD_OBLIVIONCOMBATDATA_H

#include <components/esm/formkey.hpp>
#include <components/esm/gameprofile.hpp>
#include <components/esm4/combatsettings.hpp>

namespace MWWorld
{
    class ESMStore;

    struct OblivionCombatPolicy
    {
        ESM::FormKey mActorBase;
        // Null identifies the native default style, not an unresolved record.
        ESM::FormKey mStyle;
        ESM4::CombatStyleStandard mStandard;
        ESM4::CombatStyleAdvanced mAdvanced;
    };

    struct OblivionArrowLaunchInput
    {
        ESM::FormKey mBow;
        ESM::FormKey mAmmunition;
        bool mPlayer;
        float mBowTimer;
        std::int32_t mMarksman; // Damage/gravity: truncated native float query.
        std::int32_t mLuck;
        std::int32_t mAgility;
        float mBowConditionRatio;
        float mFatigueRatio;
        std::int32_t mMasteryMarksman; // Separate native integer AV query.
        std::int32_t mAttackBonus = 0;
    };

    struct OblivionArrowLaunch
    {
        ESM::FormKey mBow;
        ESM::FormKey mAmmunition;
        float mDrawFraction;
        float mDamage;
        float mSpeed;
        float mGravityFactor;
        float mShotFatigueDebit;
    };

    // Read-only launch snapshot from winning TES4 records and caller-resolved
    // native AVs/condition. Sample before the per-shot fatigue debit. NPCs
    // supply full draw and do not read the player timer. This neither admits
    // release nor consumes ammunition, creates a projectile or applies damage.
    OblivionArrowLaunch resolveOblivionArrowLaunch(const ESMStore& store,
        ESM::GameProfile profile, const OblivionArrowLaunchInput& input);

    ESM4::CombatStyleDefaults buildOblivionCombatDefaults(const ESMStore& store);
    OblivionCombatPolicy resolveOblivionCombatPolicy(const ESMStore& store,
        const ESM::FormKey& actorBase, const ESM4::CombatStyleDefaults& defaults);
    ESM4::DropExtraOwnerSelection resolveOblivionDropExtraOwner(const ESMStore& store,
        ESM::GameProfile profile, std::uint32_t nativeActorState, bool hasExistingOwner,
        bool cellHasOwner, std::int32_t basePrice);

}

#endif
