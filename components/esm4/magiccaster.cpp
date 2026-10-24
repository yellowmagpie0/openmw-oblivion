#include "magiccaster.hpp"
#include <bit>
#include <cmath>
#include <stdexcept>
namespace ESM4
{
    namespace
    {
        float integerRatio24(std::int32_t numerator, std::int32_t denominator)
        {
            const bool negative = (numerator < 0) != (denominator < 0);
            const auto magnitude = [](std::int32_t value) {
                return static_cast<std::uint64_t>(value < 0 ? -std::int64_t(value) : std::int64_t(value));
            };
            const auto n = magnitude(numerator), d = magnitude(denominator);
            if (n == 0) return negative ? -0.f : 0.f;
            int exponent = int(std::bit_width(n)) - int(std::bit_width(d));
            if (exponent >= 0 ? n < (d << exponent) : (n << -exponent) < d) --exponent;
            const int shift = 23 - exponent;
            const auto scaledNumerator = shift >= 0 ? n << shift : n;
            const auto scaledDenominator = shift < 0 ? d << -shift : d;
            auto significand = scaledNumerator / scaledDenominator;
            const auto remainder = scaledNumerator % scaledDenominator;
            if (remainder * 2 > scaledDenominator ||
                (remainder * 2 == scaledDenominator && significand % 2)) ++significand;
            const float result = static_cast<float>(std::ldexp(double(significand), exponent - 23));
            return negative ? -result : result;
        }
        // factor is the nonnegative normal binary32 coverage fraction, or
        // zero. Keep the exact integer product before PC24 rounding: its up
        // to 55 significant bits can otherwise double-round through double.
        float integerProduct24(std::int32_t integer, float factor)
        {
            const auto magnitude = static_cast<std::uint64_t>(
                integer < 0 ? -std::int64_t(integer) : std::int64_t(integer));
            if (magnitude == 0 || factor == 0)
                return integer < 0 ? -0.f : 0.f;
            const auto encoded = std::bit_cast<std::uint32_t>(factor);
            const auto mantissa = (encoded & 0x7fffffu) | 0x800000u;
            const auto product = magnitude * mantissa;
            const int excess = int(std::bit_width(product)) - 24;
            const int shift = excess > 0 ? excess : 0;
            auto significand = product >> shift;
            if (shift != 0)
            {
                const auto remainder = product & ((std::uint64_t{1} << shift) - 1);
                const auto half = std::uint64_t{1} << (shift - 1);
                if (remainder > half || (remainder == half && significand % 2))
                    ++significand;
            }
            const int exponent = int((encoded >> 23) & 0xffu) - 127 - 23;
            const float result = static_cast<float>(std::ldexp(double(significand), exponent + shift));
            return integer < 0 ? -result : result;
        }
        // Original x87 PC24 rounds precision at arithmetic instructions but
        // retains its extended exponent until explicit binary32 stores.
        double rounded24(double value)
        {
            if (value == 0 || !std::isfinite(value)) return value;
            int exponent;
            const double scaled = std::ldexp(std::frexp(std::abs(value), &exponent), 24);
            const double lower = std::floor(scaled);
            const double fraction = scaled - lower;
            const double mantissa = lower + (fraction > .5 || (fraction == .5 && static_cast<std::uint32_t>(lower) % 2));
            return std::copysign(std::ldexp(mantissa, exponent - 24), value);
        }
    }

    void validateCombatSpellPowerSettings(const CombatSpellPowerSettings& settings)
    {
        for (float value : {settings.mFatigueBase, settings.mFatigueMultiplier,
                 settings.mArmorPenaltyMinimum, settings.mArmorPenaltyMaximum})
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native combat spell power setting");
    }

    float playerCombatSpellPower(const PlayerCombatSpellPower& input, const CombatSpellPowerSettings& settings)
    {
        for (const auto coverage : {input.mLightCoverage, input.mHeavyCoverage})
            if (coverage < 0 || coverage > 100)
                throw std::invalid_argument("invalid native casting armor coverage");
        validateCombatSpellPowerSettings(settings);
        float armor = 1.f;
        if (input.mLightCoverage + input.mHeavyCoverage > 0)
        {
            const auto term = [&](std::int32_t skill, std::int32_t coverage) {
                if (skill >= settings.mJourneymanMinimum) return 0.f;
                if (skill >= 100) skill = 100;
                // Original integer SUB/ADD wrap before FILD; retain their
                // int32 result even for negative/extreme current skills.
                const auto twice = std::bit_cast<std::int32_t>((std::uint32_t{50} - static_cast<std::uint32_t>(skill)) * 2u);
                const float fraction = static_cast<float>(rounded24(double(coverage) / 100.));
                return integerProduct24(twice, fraction);
            };
            const float light = term(input.mLightArmor, input.mLightCoverage);
            const float heavy = term(input.mHeavyArmor, input.mHeavyCoverage);
            const double sum = rounded24(double(light) + heavy);
            const double range = rounded24(double(settings.mArmorPenaltyMaximum) - settings.mArmorPenaltyMinimum);
            const double weighted = rounded24(sum * range);
            const double scaled = rounded24(weighted / 100.);
            const double remaining = rounded24(1. - scaled);
            armor = static_cast<float>(rounded24(remaining - settings.mArmorPenaltyMinimum));
        }
        float fatigue = 1.f;
        if (input.mBaseFatigue != 0 && input.mCurrentFatigue != input.mBaseFatigue)
        {
            const float ratio = integerRatio24(input.mCurrentFatigue, input.mBaseFatigue);
            const double contribution = rounded24(double(settings.mFatigueMultiplier) * ratio);
            fatigue = static_cast<float>(rounded24(double(settings.mFatigueBase) + contribution));
        }
        const float result = static_cast<float>(rounded24(double(armor) * fatigue));
        if (!std::isfinite(result)) throw std::invalid_argument("native combat spell power overflow");
        return result;
    }
}
