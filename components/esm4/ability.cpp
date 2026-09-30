#include "ability.hpp"

#include "loadmgef.hpp"
#include "loadspel.hpp"

#include <bit>

namespace ESM4
{
    LoadedEffectSetting mergeLoadedEffectSetting(LoadedEffectSetting previous, const EffectSettingData& authored)
    {
        // Original editable flag tables B03418/B034C4, followed by the
        // preserved static ActorValue branch and projectile-bit clearing.
        constexpr std::uint32_t editable = 0x0fe03c00;
        auto flags = (previous.mFlags & ~editable) | (authored.mFlags & editable);
        auto data = authored.mAssociatedData;
        if (previous.mFlags & 0x01000000)
        {
            flags |= 0x01000000;
            data = previous.mData;
        }
        flags &= ~0x00200000;
        return {flags, data};
    }

    ValueModifierEffectInputs resolveValueModifierEffectInputs(const SpellEffect& effect, LoadedEffectSetting setting)
    {
        return {setting.mFlags & 0x01000000 ? setting.mData : effect.mActorValue,
            setting.mFlags & 0x100 ? 1.f : static_cast<float>(std::bit_cast<std::int32_t>(effect.mMagnitude)),
            setting.mFlags & 0x80 ? 0.f : static_cast<float>(std::bit_cast<std::int32_t>(effect.mDuration))};
    }
}
