#include "ability.hpp"

#include "loadmgef.hpp"
#include "loadspel.hpp"

#include <bit>
#include <array>
#include <cmath>
#include <stdexcept>

namespace ESM4
{
    std::optional<LoadedEffectSetting> compiledPassiveValueModifierDefinition(std::uint32_t code)
    {
        // Reviewed initializer arguments; editable facts/provenance are in
        // docs/oblivion/M15-PASSIVE-ABILITY-DEFAULTS.json.
        static constexpr std::array definitions{
            std::pair{ESM::fourCC("WABR"), LoadedEffectSetting{0x1000172, 55}},
            std::pair{ESM::fourCC("WKFI"), LoadedEffectSetting{0x100007f, 61}},
            std::pair{ESM::fourCC("WKFR"), LoadedEffectSetting{0x100007f, 62}},
            std::pair{ESM::fourCC("WKSH"), LoadedEffectSetting{0x100007f, 68}},
            std::pair{ESM::fourCC("WKMA"), LoadedEffectSetting{0x100007f, 64}},
            std::pair{ESM::fourCC("STMA"), LoadedEffectSetting{0x1000112, 57}},
            std::pair{ESM::fourCC("SABS"), LoadedEffectSetting{0x1000072, 52}},
            std::pair{ESM::fourCC("FOAT"), LoadedEffectSetting{0x100072, 0}},
            std::pair{ESM::fourCC("FOSP"), LoadedEffectSetting{0x1000072, 9}},
            std::pair{ESM::fourCC("RSFI"), LoadedEffectSetting{0x100007a, 61}},
            std::pair{ESM::fourCC("RSFR"), LoadedEffectSetting{0x100007a, 62}},
            std::pair{ESM::fourCC("RSMA"), LoadedEffectSetting{0x100007a, 64}},
            std::pair{ESM::fourCC("RSDI"), LoadedEffectSetting{0x100007a, 63}},
            std::pair{ESM::fourCC("RSPO"), LoadedEffectSetting{0x100007a, 67}}
        };
        for (const auto& [candidate, definition] : definitions)
            if (candidate == code)
                return definition;
        return std::nullopt;
    }

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

    namespace
    {
        float storedDelta(double value)
        {
            const float result = static_cast<float>(value);
            if (!std::isfinite(result))
                throw std::invalid_argument("native value-modifier arithmetic overflow");
            return result;
        }
    }

    bool valueModifierRequiresCurrent(std::uint32_t actorValue, std::uint32_t code, float delta)
    {
        if (actorValue >= 72 || !std::isfinite(delta))
            throw std::invalid_argument("invalid native value-modifier clamp input");
        return actorValue <= 32 && actorValue != 10 && code != ESM::fourCC("ABHE") && delta <= 0.f;
    }

    float clampValueModifierDelta(std::uint32_t actorValue, std::uint32_t code,
        float delta, std::optional<float> current)
    {
        if (!valueModifierRequiresCurrent(actorValue, code, delta))
            return delta;
        if (!current || !std::isfinite(*current))
            throw std::invalid_argument("native value-modifier clamp requires finite current value");
        const float sum = storedDelta(double(*current) + double(delta));
        return sum < 0.f ? storedDelta(double(delta) - double(sum)) : delta;
    }

    std::optional<float> initialValueModifierRemovalDamage(std::uint32_t actorValue,
        std::uint32_t code, float storedMagnitude, std::optional<float> current)
    {
        // Validate the owned AV and finite magnitude even on the no-write path.
        valueModifierRequiresCurrent(actorValue, code, storedMagnitude);
        if (storedMagnitude <= 0.f)
            return std::nullopt;
        return storedDelta(double(clampValueModifierDelta(actorValue, code, -storedMagnitude, current))
            + double(storedMagnitude));
    }

}
