#ifndef OPENMW_COMPONENTS_ESM4_WEAPONPOISON_H
#define OPENMW_COMPONENTS_ESM4_WEAPONPOISON_H

#include "loadalch.hpp"
#include <optional>

namespace ESM4
{
    // Bounded original Poison of Illness profile. Timed/player-crafted effects,
    // scripted mixtures and other poison families need their own execution policy.
    inline std::optional<float> instantWeaponPoisonDamage(const Potion& potion)
    {
        if ((potion.mFlags & Rec_Deleted) || !potion.mScriptId.isZeroOrUnset()
            || potion.mEffects.size() != 1) return {};
        const auto& effect = potion.mEffects.front();
        if (effect.mId != ESM::fourCC("DGHE") || effect.mMagnitude != 10
            || effect.mArea || effect.mDuration || effect.mRange || effect.mActorValue != 8
            || effect.mScriptEffect) return {};
        return 10.f;
    }
}
#endif
