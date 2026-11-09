#ifndef OPENMW_COMPONENTS_ESM4_POTIONATTRIBUTES_H
#define OPENMW_COMPONENTS_ESM4_POTIONATTRIBUTES_H

#include <cstdint>
#include <vector>

#include <components/esm/formkey.hpp>

namespace ESM4
{
    inline constexpr float MeadAttributeDrain = 1.f;
    inline constexpr float MeadDuration = 60.f;
    inline constexpr float MeadFatigueRestoration = 2.f;

    struct PotionAttributeModifier
    {
        std::uint8_t mAttribute = 0;
        float mDelta = 0.f;

        friend bool operator==(const PotionAttributeModifier&, const PotionAttributeModifier&) = default;
    };

    struct PotionAttributeStep
    {
        float mElapsed;
        bool mExpired;
    };

    PotionAttributeStep advancePotionAttributes(float elapsed, float lifetime, float duration);

    // One consumed application's resolved, recoverable Maximum contributions.
    // Instant restoration is committed at use and is never replayed from this owner.
    struct PotionAttributeState
    {
        ESM::FormKey mPotion;
        float mDuration = 0.f;
        float mElapsed = 0.f;
        std::vector<PotionAttributeModifier> mModifiers;

        void validate() const;

        friend bool operator==(const PotionAttributeState&, const PotionAttributeState&) = default;
    };
}

#endif
