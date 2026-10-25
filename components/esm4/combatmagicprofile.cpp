#include "combatmagicprofile.hpp"

#include "ability.hpp"
#include "common.hpp"
#include "loadmgef.hpp"
#include "loadspel.hpp"

#include <cmath>

namespace ESM4
{
    bool supportsDefaultCombatParalysisProfile(const Spell* spell, const EffectSetting* paralysis)
    {
        if (!paralysis || (paralysis->mFlags & Rec_Deleted)
            || paralysis->mEffectCode != ESM::fourCC("PARA") || !paralysis->mData)
            return false;
        const auto& authored = *paralysis->mData;
        if (authored.mSchool != 3 || authored.mResistanceActorValue != 66
            || authored.mAssociatedForm || !std::isfinite(authored.mBaseCost))
            return false;
        // Original417420 initializer arguments: flags01000173, static AV48.
        // Full native self-cast matrices verify both compiled and vanilla
        // loaded01000D73 profiles. General authored flag variants stay guarded.
        const auto ready = mergeLoadedEffectSetting({0x01000173, 48}, authored);
        if (ready.mData != 48 || (ready.mFlags != 0x01000173 && ready.mFlags != 0x01000d73))
            return false;
        if (!spell)
            return true; // Original41D590 fallback: fixed cost0, one self PARA10.
        if ((spell->mFlags & Rec_Deleted) || !spell->mData || spell->mData->mType != 0 || spell->mData->mCost != 0
            || spell->mData->mLevel != 0 || spell->mData->mFlags != 1 || spell->mEffects.size() != 1)
            return false;
        // NoMagnitude supplies1 and compiled static data supplies AV48;
        // authored EFIT magnitude/actor-value fields do not select those values.
        const auto& effect = spell->mEffects.front();
        return effect.mId == ESM::fourCC("PARA") && effect.mArea == 0 && effect.mDuration == 10
            && effect.mRange == 0 && !effect.mScriptEffect;
    }
}
