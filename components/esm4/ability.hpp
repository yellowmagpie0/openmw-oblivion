#ifndef OPENMW_ESM4_ABILITY_H
#define OPENMW_ESM4_ABILITY_H

#include <cstdint>
#include <optional>
#include <vector>

#include <components/esm/formkey.hpp>

namespace ESM4
{
    struct EffectSettingData;
    struct SpellEffect;

    struct LoadedEffectSetting
    {
        std::uint32_t mFlags;
        std::uint32_t mData;
    };

    // Verified compiled inputs for the fourteen racial/birthsign VMOD codes.
    // Other codes remain unadmitted; this is not a complete magic registry.
    std::optional<LoadedEffectSetting> compiledPassiveValueModifierDefinition(std::uint32_t code);

    // Original TES4 DATA loading preserves compiled flags and static AV data.
    // The caller supplies the prior ready definition, including on overrides.
    LoadedEffectSetting mergeLoadedEffectSetting(
        LoadedEffectSetting previous, const EffectSettingData& authored);

    struct ValueModifierEffectInputs
    {
        std::uint32_t mActorValue;
        float mMagnitude;
        float mDuration;
    };

    struct PassiveValueModifierInput
    {
        std::uint32_t mEffectIndex;
        std::uint32_t mCode;
        std::uint32_t mFlags;
        ValueModifierEffectInputs mValues;
    };

    // Resolved self Ability4 constructor inputs. These are not saved applied
    // magnitudes. The winning-record adapter owns class/range admission.
    struct PassiveAbilityInput
    {
        ESM::FormKey mSpell;
        std::vector<PassiveValueModifierInput> mEffects;
    };

    // Constructor inputs only. Caller verifies native effect class/admission;
    // detrimental sign, resistance, application and removal happen later.
    ValueModifierEffectInputs resolveValueModifierEffectInputs(
        const SpellEffect& effect, LoadedEffectSetting setting);
    // Query only when the original clamp reaches actor current-value dispatch.
    bool valueModifierRequiresCurrent(std::uint32_t actorValue, std::uint32_t code, float delta);
    float clampValueModifierDelta(std::uint32_t actorValue, std::uint32_t code,
        float delta, std::optional<float> current);

    // Initial Damage write during recoverable removal, before the separate
    // Health-specific correction and essential-health checks. Null omits the
    // write; an engaged zero still dispatches it. Base removal always uses the
    // negated stored magnitude, not the clamped value returned here.
    std::optional<float> initialValueModifierRemovalDamage(std::uint32_t actorValue,
        std::uint32_t code, float storedMagnitude, std::optional<float> current);

}
#endif
