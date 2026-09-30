#ifndef OPENMW_ESM4_ABILITY_H
#define OPENMW_ESM4_ABILITY_H

#include <cstdint>

namespace ESM4
{
    struct EffectSettingData;
    struct SpellEffect;

    struct LoadedEffectSetting
    {
        std::uint32_t mFlags;
        std::uint32_t mData;
    };

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

    // Constructor inputs only. Caller verifies native effect class/admission;
    // detrimental sign, resistance, application and removal happen later.
    ValueModifierEffectInputs resolveValueModifierEffectInputs(
        const SpellEffect& effect, LoadedEffectSetting setting);
}
#endif
