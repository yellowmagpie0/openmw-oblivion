#include "ability.hpp"

#include "loadmgef.hpp"
#include "loadspel.hpp"

#include <bit>
#include <array>
#include <cmath>
#include <charconv>
#include <string>
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

    namespace
    {
        std::string comparisonQuantity(float value)
        {
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native passive comparison quantity");
            const double absolute = std::abs(double(value));
            std::string result;
            if (absolute < 16777216.)
            {
                const auto ticks = static_cast<std::uint64_t>(std::floor(absolute * 10. + .5));
                result = std::to_string(ticks / 10) + "." + char('0' + ticks % 10);
            }
            else
            {
                std::array<char, 80> buffer{};
                if (absolute < 1.e17)
                {
                    const auto conversion = std::to_chars(buffer.data(), buffer.data() + buffer.size(),
                        absolute, std::chars_format::fixed, 0);
                    if (conversion.ec != std::errc{})
                        throw std::invalid_argument("native passive comparison conversion failed");
                    result.assign(buffer.data(), conversion.ptr);
                }
                else
                {
                    // Original CRT floating conversion retains seventeen significant
                    // decimal digits even when formatting a much larger integer.
                    const auto conversion = std::to_chars(buffer.data(), buffer.data() + buffer.size(),
                        absolute, std::chars_format::scientific, 16);
                    if (conversion.ec != std::errc{})
                        throw std::invalid_argument("native passive comparison conversion failed");
                    const std::string_view scientific(buffer.data(), conversion.ptr - buffer.data());
                    const auto exponentAt = scientific.find('e');
                    int exponent = 0;
                    auto exponentStart = scientific.data() + exponentAt + 1;
                    if (*exponentStart == '+')
                        ++exponentStart;
                    const auto parsed = std::from_chars(exponentStart, scientific.data() + scientific.size(), exponent);
                    if (parsed.ec != std::errc{} || exponent < 16)
                        throw std::invalid_argument("native passive comparison exponent conversion failed");
                    for (char digit : scientific.substr(0, exponentAt))
                        if (digit != '.')
                            result += digit;
                    result.append(static_cast<std::size_t>(exponent + 1) - result.size(), '0');
                }
                result += ".0";
            }
            if (std::signbit(value))
                result.insert(result.begin(), '-');
            return result;
        }
    }

    std::string passiveEffectComparisonKey(std::uint32_t flags, std::uint32_t school,
        std::string_view name, float magnitude, float duration)
    {
        constexpr std::string_view schools = "ceadfb";
        std::string result(1, school < schools.size() ? schools[school] : 'z');
        for (unsigned char character : name.substr(0, 30))
        {
            if (character == 0)
                break;
            if (character > 127)
                throw std::invalid_argument("unsupported non-ASCII native passive comparison name");
            result += character >= 'A' && character <= 'Z' ? char(character + ('a' - 'A')) : char(character);
        }
        result += comparisonQuantity(flags & 0x100 ? 1000.f : magnitude);
        result += comparisonQuantity(flags & 0x80 ? 1000.f : duration);
        return result;
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
