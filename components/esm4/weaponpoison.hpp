#ifndef OPENMW_COMPONENTS_ESM4_WEAPONPOISON_H
#define OPENMW_COMPONENTS_ESM4_WEAPONPOISON_H

#include "loadalch.hpp"
#include <components/esm3/loadalch.hpp>
#include <components/esm3/loadmgef.hpp>
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace ESM4
{
    inline bool weaponPoisonIdentity(const ESM::FormKey& key)
    {
        return key.isContent() || (key.isDynamic() && key.mNamespace == "shared-item" && key.mValue > 0);
    }

    // The generated save facade carries its own marker, not TES4 ALCH flags.
    // Admit only the already supported novice recipe's complete effect.
    inline bool craftedWeaponPoison(const ESM::Potion& potion)
    {
        if (!potion.mId.is<ESM::GeneratedRefId>() || !potion.mScript.empty()
            || potion.mData.mFlags != ESM::Potion::OblivionPoison || potion.mEffects.mList.size() != 1)
            return false;
        const auto& effect = potion.mEffects.mList.front().mData;
        return effect.mEffectID == ESM::MagicEffect::DamageHealth
            && effect.mSkill.empty() && effect.mAttribute.empty()
            && effect.mRange == 0 && effect.mArea == 0 && effect.mDuration == 5
            && effect.mMagnMin == 1 && effect.mMagnMax == 1;
    }

    // One landed application belongs to the recipient, independently of the
    // bottle and the weapon. Elapsed time is simulation time, never wall time.
    struct WeaponPoisonState
    {
        ESM::FormKey mPoison;
        ESM::FormKey mAttacker;
        float mMagnitude = 1;
        float mDuration = 5;
        float mElapsed = 0;

        void validate() const
        {
            if (!mPoison.isDynamic() || mPoison.mNamespace != "shared-item" || !mPoison.mValue
                || ESM::FormKey::deserialize(mPoison.serialize()) != mPoison
                || mAttacker != ESM::FormKey::dynamic("player", 1)
                || mMagnitude != 1 || mDuration != 5
                || !std::isfinite(mElapsed) || mElapsed < 0 || mElapsed >= mDuration)
                throw std::invalid_argument("invalid crafted weapon poison ownership or lifetime");
        }

        friend bool operator==(const WeaponPoisonState&, const WeaponPoisonState&) = default;
    };

    struct WeaponPoisonStep
    {
        float mElapsed;
        float mDamage;
        bool mExpired;
    };

    inline WeaponPoisonStep advanceWeaponPoison(const WeaponPoisonState& effect, float duration)
    {
        effect.validate();
        if (!std::isfinite(duration) || duration < 0)
            throw std::invalid_argument("invalid weapon poison frame duration");
        const float elapsed = static_cast<float>(std::min(double(effect.mDuration), double(effect.mElapsed) + duration));
        return {elapsed, (elapsed - effect.mElapsed) * effect.mMagnitude, elapsed >= effect.mDuration};
    }

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
