#include "physicalblendsettings.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include <boost/multiprecision/cpp_int.hpp>

namespace ESM4
{
    namespace
    {
        void validate(float value)
        {
            if (!std::isfinite(value))
                throw std::invalid_argument("nonfinite native physical blend duration");
        }

        using boost::multiprecision::cpp_int;

        cpp_int decimalPower10(unsigned exponent)
        {
            cpp_int result = 1;
            while (exponent--)
                result *= 10;
            return result;
        }

        cpp_int roundDecimalWithTiesTowardZero(cpp_int numerator, cpp_int denominator, int shift)
        {
            if (shift >= 0)
                numerator <<= shift;
            else
                denominator <<= -shift;
            cpp_int quotient = numerator / denominator;
            const cpp_int remainder = numerator % denominator;
            if ((remainder << 1) > denominator)
                ++quotient;
            return quotient;
        }

        std::string formatPhysicalBlendDefault(float previous)
        {
            const auto bits = std::bit_cast<std::uint32_t>(previous);
            const int rawExponent = (bits >> 23) & 255u;
            const std::uint32_t significand = (bits & 0x7fffffu) | (rawExponent ? (1u << 23) : 0u);
            const int exponent = rawExponent ? rawExponent - 150 : -149;
            cpp_int numerator = cpp_int(significand) * 1000000;
            cpp_int denominator = 1;
            if (exponent >= 0)
                numerator <<= exponent;
            else
                denominator <<= -exponent;
            cpp_int units = numerator / denominator;
            if (((numerator % denominator) << 1) >= denominator)
                ++units;
            // Original98208B's fixed-six default rounds decimal halfway values
            // away from zero. Preserve the sign even when all six digits are0.
            std::string result = units.convert_to<std::string>();
            if (result.size() < 7)
                result.insert(0, 7 - result.size(), '0');
            result.insert(result.size() - 6, 1, '.');
            if (bits & 0x80000000u)
                result.insert(result.begin(), '-');
            return result;
        }

        std::optional<float> parsePhysicalBlendDecimal(
            std::string_view text, std::size_t* parsedLength = nullptr)
        {
            std::size_t position = 0;
            while (position < text.size()
                && (text[position] == ' ' || (text[position] >= '\t' && text[position] <= '\r')))
                ++position;
            std::uint32_t sign = 0;
            if (position < text.size() && (text[position] == '+' || text[position] == '-'))
            {
                if (text[position] == '-')
                    sign = 0x80000000u;
                ++position;
            }
            std::string digits;
            bool hasPoint = false;
            bool hasDigit = false;
            bool significant = false;
            int exponent = 0;
            while (position < text.size())
            {
                const char character = text[position];
                if (character == '.' && !hasPoint)
                {
                    hasPoint = true;
                    ++position;
                    continue;
                }
                if (character < '0' || character > '9')
                    break;
                hasDigit = true;
                significant |= character != '0';
                if (significant)
                    digits += character;
                if (hasPoint)
                    --exponent;
                ++position;
            }
            if (!hasDigit)
                return std::nullopt;
            if (position < text.size() && (text[position] == 'e' || text[position] == 'E'))
            {
                ++position;
                int direction = 1;
                if (position < text.size() && (text[position] == '+' || text[position] == '-'))
                {
                    if (text[position] == '-')
                        direction = -1;
                    ++position;
                }
                int inputExponent = 0;
                while (position < text.size() && text[position] >= '0' && text[position] <= '9')
                {
                    inputExponent = std::min(5201, inputExponent * 10 + text[position] - '0');
                    ++position;
                }
                exponent += direction * inputExponent;
            }
            if (parsedLength)
                *parsedLength = position;
            if (digits.empty())
                return std::bit_cast<float>(sign);
            if (digits.size() > 24)
            {
                // Original994ECE's 25-digit collector increments digit24 when
                // digit24>=5, then drops digit25. Preserve a resulting digit10
                // as an integer coefficient rather than changing its position.
                if (digits[23] >= '5')
                    ++digits[23];
                exponent += static_cast<int>(digits.size()) - 24;
                digits.resize(24);
            }
            // Bound integer work; these exponents cannot yield a finite nonzero
            // float for a nonzero coefficient of at most24 decimal digits.
            if (exponent > 100)
                throw std::invalid_argument("nonfinite native physical blend INI value");
            if (exponent < -400)
                return std::bit_cast<float>(sign);
            cpp_int numerator = 0;
            for (char digit : digits)
                numerator = numerator * 10 + (digit - '0');

            // Original995258 discards the low16 bits of the integer collector
            // before decimal exponent multiplication.
            const int integerExponent = static_cast<int>(boost::multiprecision::msb(numerator));
            numerator = roundDecimalWithTiesTowardZero(numerator, cpp_int(1), 79 - integerExponent);
            numerator = (numerator >> 16) << 16;
            cpp_int denominator = 1;
            if (integerExponent >= 79)
                numerator <<= integerExponent - 79;
            else
                denominator <<= 79 - integerExponent;
            if (exponent >= 0)
                numerator *= decimalPower10(exponent);
            else
                denominator *= decimalPower10(-exponent);
            int binaryExponent = static_cast<int>(boost::multiprecision::msb(numerator))
                - static_cast<int>(boost::multiprecision::msb(denominator));
            if (binaryExponent >= 0 ? numerator < (denominator << binaryExponent)
                                    : (numerator << -binaryExponent) < denominator)
                --binaryExponent;

            // Use integer arithmetic to make native significand stores and
            // halfway rounding independent of the host's floating-point mode.
            const cpp_int mantissa
                = roundDecimalWithTiesTowardZero(numerator, denominator, 79 - binaryExponent);
            cpp_int normal = roundDecimalWithTiesTowardZero(mantissa, cpp_int(1), -56);
            if (normal == (cpp_int(1) << 24))
            {
                ++binaryExponent;
                normal >>= 1;
            }
            std::uint32_t result = 0;
            if (binaryExponent >= 128)
                throw std::invalid_argument("nonfinite native physical blend INI value");
            if (binaryExponent < -151)
                result = 0;
            else if (binaryExponent <= -127)
            {
                // Original99F66D restores the pre-rounded significand for
                // subnormals, rounds half a subnormal unit, then truncates it.
                result = (roundDecimalWithTiesTowardZero(mantissa, cpp_int(1), binaryExponent + 71) >> 1)
                             .convert_to<std::uint32_t>();
            }
            else
                result = (static_cast<std::uint32_t>(binaryExponent + 127) << 23)
                    | (normal.convert_to<std::uint32_t>() - (1u << 23));
            return std::bit_cast<float>(sign | result);
        }
        template <std::size_t Size>
        void readPhysicalBlendProfileStrings(std::array<std::string, Size>& current,
            const std::array<std::optional<std::string_view>, Size>& processedValues)
        {
            for (std::size_t i = 0; i < Size; ++i)
            {
                const std::string_view text = processedValues[i].value_or(current[i]);
                current[i] = std::string(text.substr(0, std::min<std::size_t>(255, text.find('\0'))));
            }
        }

        PhysicalBlendGains parsePhysicalHitBlendGains(std::string_view text)
        {
            text = text.substr(0, text.find('\0'));
            PhysicalBlendGains result{1.f, 1.f};
            std::size_t consumed = 0;
            if (const auto first = parsePhysicalBlendDecimal(text, &consumed))
            {
                result.mHierarchy = *first;
                // The native format is "%f, %f": a space before the literal
                // comma prevents the second conversion; spaces after it don't.
                if (consumed < text.size() && text[consumed] == ',')
                    if (const auto second = parsePhysicalBlendDecimal(text.substr(consumed + 1)))
                        result.mVelocity = *second;
            }
            return result;
        }

    }

    PhysicalBlendFloatSettingResult readPhysicalBlendFloatSetting(
        float previous, std::optional<std::string_view> processedProfileValue)
    {
        validate(previous);
        const std::string formattedDefault
            = processedProfileValue ? std::string{} : formatPhysicalBlendDefault(previous);
        std::string_view text = processedProfileValue.value_or(formattedDefault);
        text = text.substr(0, std::min<std::size_t>(255, text.find('\0')));
        const auto value = parsePhysicalBlendDecimal(text);
        return {value.value_or(previous), value.has_value()};
    }

    PhysicalHitBlendConfiguration resolvePhysicalHitBlendConfiguration(
        const PhysicalBlendGainTable& previous, const PhysicalHitBlendSettings& settings)
    {
        validate(settings.mMinimumHierarchy);
        validate(settings.mMinimumVelocity);
        for (const auto gain : previous)
        {
            validate(gain.mHierarchy);
            validate(gain.mVelocity);
        }
        const std::array<std::pair<std::size_t, PhysicalBlendGains>, 10> configured{{
            {1, settings.mHead}, {2, settings.mBody}, {3, settings.mSpine1}, {4, settings.mSpine2},
            {5, settings.mLUpperArm}, {6, settings.mLForeArm}, {7, settings.mLHand},
            {11, settings.mRUpperArm}, {12, settings.mRForeArm}, {13, settings.mRHand}}};
        auto result = PhysicalHitBlendConfiguration{previous, settings.mMinimumHierarchy, settings.mMinimumVelocity};
        for (const auto& [id, gain] : configured)
        {
            validate(gain.mHierarchy);
            validate(gain.mVelocity);
            result.mGains[id] = gain;
        }
        return result;
    }

    PhysicalBlendCollisionState resolvePhysicalBlendCollisionAfterLink(
        PhysicalBlendCollisionState loaded, bool hasResolvedBody, std::uint32_t packedFilter,
        const PhysicalBlendGainTable& resolvedGains)
    {
        const auto index = hasResolvedBody ? (packedFilter >> 8) & 31u : 0u;
        const auto gains = resolvedGains[index];
        validate(gains.mHierarchy);
        validate(gains.mVelocity);
        loaded.mFlags |= 0x8;
        loaded.mGains = gains;
        return loaded;
    }

    PhysicalBlendDurationTables resolvePhysicalBlendDurationTables(
        const PhysicalBlendDurationTables& previous, const PhysicalBlendDurationSettings& settings)
    {
        validate(settings.mGetUpTime);
        validate(settings.mKnockdownTime);
        auto result = previous;
        for (std::size_t i = 0; i < result.mGetUp.size(); ++i)
        {
            validate(previous.mGetUp[i]);
            validate(previous.mKnockdown[i]);
            if (previous.mGetUp[i] >= 0.f)
                result.mGetUp[i] = settings.mGetUpTime;
            if (previous.mKnockdown[i] >= 0.f)
                result.mKnockdown[i] = settings.mKnockdownTime;
        }
        return result;
    }

    PhysicalHitBlendProfileConfiguration loadPhysicalHitBlendProfile(
        const PhysicalHitBlendProfile& previous, const PhysicalBlendGainTable& previousGains,
        const PhysicalBlendDurationTables& previousDurations, std::uint32_t version,
        const PhysicalHitBlendProfileValues& processedValues)
    {
        auto next = previous;
        if (version >= 14)
        {
            for (std::size_t i = 0; i < next.mGains.size(); ++i)
            {
                const std::string_view text = processedValues.mGains[i].value_or(next.mGains[i]);
                // Original4A8800 always stores the returned string, even when
                // empty. Its unchanged false return does not reject that write.
                next.mGains[i] = std::string(text.substr(0, std::min<std::size_t>(255, text.find('\0'))));
            }
            next.mDurations.mGetUpTime
                = readPhysicalBlendFloatSetting(previous.mDurations.mGetUpTime, processedValues.mGetUpTime).mValue;
            next.mDurations.mKnockdownTime
                = readPhysicalBlendFloatSetting(previous.mDurations.mKnockdownTime, processedValues.mKnockdownTime).mValue;
            next.mMinimumHierarchy
                = readPhysicalBlendFloatSetting(previous.mMinimumHierarchy, processedValues.mMinimumHierarchy).mValue;
            next.mMinimumVelocity
                = readPhysicalBlendFloatSetting(previous.mMinimumVelocity, processedValues.mMinimumVelocity).mValue;
        }

        PhysicalHitBlendSettings settings;
        const std::array<PhysicalBlendGains*, 10> gains{{
            &settings.mRHand, &settings.mRForeArm, &settings.mRUpperArm,
            &settings.mLHand, &settings.mLForeArm, &settings.mLUpperArm,
            &settings.mSpine2, &settings.mSpine1, &settings.mBody, &settings.mHead}};
        for (std::size_t i = 0; i < gains.size(); ++i)
            *gains[i] = parsePhysicalHitBlendGains(next.mGains[i]);
        settings.mMinimumHierarchy = next.mMinimumHierarchy;
        settings.mMinimumVelocity = next.mMinimumVelocity;
        auto hit = resolvePhysicalHitBlendConfiguration(previousGains, settings);
        auto durations = resolvePhysicalBlendDurationTables(previousDurations, next.mDurations);
        return {std::move(next), std::move(hit), std::move(durations)};
    }

    PhysicalBlendProfilesConfiguration loadPhysicalBlendProfiles(
        const PhysicalBlendProfilesConfiguration& previous, std::uint32_t version,
        const PhysicalBlendProfilesValues& processedValues)
    {
        for (const auto* table : {&previous.mPostLink, &previous.mHit.mGains, &previous.mQuadHit})
            for (const auto gain : *table)
            {
                validate(gain.mHierarchy);
                validate(gain.mVelocity);
            }
        validate(previous.mHit.mMinimumHierarchy);
        validate(previous.mHit.mMinimumVelocity);
        auto hit = loadPhysicalHitBlendProfile(
            previous.mProfiles.mHit, previous.mHit.mGains, previous.mDurations, version, processedValues.mHit);
        auto next = previous;
        auto& defaults = next.mProfiles.mDefault;
        auto& quadruped = next.mProfiles.mQuadHit;
        if (version >= 14)
        {
            readPhysicalBlendProfileStrings(defaults.mGains, processedValues.mDefaultGains);
            readPhysicalBlendProfileStrings(quadruped.mGains, processedValues.mQuadHitGains);
            defaults.mHighTranslation = readPhysicalBlendFloatSetting(
                previous.mProfiles.mDefault.mHighTranslation, processedValues.mHighTranslation).mValue;
            defaults.mHighRotation = readPhysicalBlendFloatSetting(
                previous.mProfiles.mDefault.mHighRotation, processedValues.mHighRotation).mValue;
            defaults.mLowTranslation = readPhysicalBlendFloatSetting(
                previous.mProfiles.mDefault.mLowTranslation, processedValues.mLowTranslation).mValue;
            defaults.mLowRotation = readPhysicalBlendFloatSetting(
                previous.mProfiles.mDefault.mLowRotation, processedValues.mLowRotation).mValue;
            defaults.mPassOutTime = readPhysicalBlendFloatSetting(
                previous.mProfiles.mDefault.mPassOutTime, processedValues.mPassOutTime).mValue;
            defaults.mPassOutForce = readPhysicalBlendFloatSetting(
                previous.mProfiles.mDefault.mPassOutForce, processedValues.mPassOutForce).mValue;
        }
        for (float value : {defaults.mHighTranslation, defaults.mHighRotation,
                 defaults.mLowTranslation, defaults.mLowRotation, defaults.mPassOutTime, defaults.mPassOutForce})
            validate(value);

        for (std::size_t i = 0; i < defaults.mGains.size(); ++i)
        {
            const auto gain = parsePhysicalHitBlendGains(defaults.mGains[i]);
            validate(gain.mHierarchy);
            validate(gain.mVelocity);
            // IDs1-21 are contiguous; PonyTail is23, leaving22 unchanged.
            next.mPostLink[i == 21 ? 23 : i + 1] = gain;
        }
        constexpr std::array<std::size_t, 12> quadIds{15, 14, 12, 11, 9, 8, 6, 5, 4, 3, 2, 1};
        for (std::size_t i = 0; i < quadIds.size(); ++i)
        {
            const auto gain = parsePhysicalHitBlendGains(quadruped.mGains[i]);
            validate(gain.mHierarchy);
            validate(gain.mVelocity);
            next.mQuadHit[quadIds[i]] = gain;
        }
        next.mProfiles.mHit = std::move(hit.mProfile);
        next.mHit = std::move(hit.mHit);
        next.mDurations = std::move(hit.mDurations);
        return next;
    }

    PhysicalKnockdownBlend preparePhysicalKnockdownBlend(PhysicalBlendGains current,
        float duration, float startKey, std::uint16_t controllerFlags, PhysicalBlendClock previousClock)
    {
        validate(current.mHierarchy);
        validate(current.mVelocity);
        validate(duration);
        validate(startKey);
        validate(previousClock.mElapsed);
        if (duration < 0.f)
            throw std::invalid_argument("disabled native knockdown blend duration");
        constexpr float sentinel = -std::numeric_limits<float>::max();
        previousClock.mStartTime = sentinel;
        previousClock.mPreviousTime = sentinel;
        // Setup ORs0xc5; NiTimeController::Start adds active bit0x8.
        return {{{{0.f, current}, {duration, {0.f, 0.f}}}}, startKey, duration,
            static_cast<std::uint16_t>((controllerFlags & 0xfef5u) | 0xcdu), previousClock};
    }

    PhysicalBlendKeyBounds resolvePhysicalBlendKeyBounds(
        PhysicalBlendKeyBounds previous, std::span<const PhysicalBlendKey> keys)
    {
        if (keys.empty())
            return {0.f, 0.f};
        validate(previous.mStartKey);
        validate(previous.mStopKey);
        validate(keys.front().mTime);
        validate(keys.back().mTime);
        constexpr float sentinel = std::numeric_limits<float>::max();
        if (keys.front().mTime < previous.mStartKey || previous.mStartKey == sentinel)
            previous.mStartKey = keys.front().mTime;
        if (keys.back().mTime < previous.mStopKey || previous.mStopKey == -sentinel)
            previous.mStopKey = keys.back().mTime;
        return previous;
    }

    PhysicalBlendControllerUpdate advancePhysicalBlendController(const PhysicalBlendControllerState& controller,
        const PhysicalBlendTimeCache& timeCache, bool hasTarget, std::optional<PhysicalBlendGains> targetGains,
        bool hasVelocityController, float inputTime)
    {
        PhysicalBlendControllerUpdate result{controller, timeCache, targetGains, false};
        auto& next = result.mController;
        if (!hasTarget || !(next.mTiming.mFlags & 8) || next.mKeys.empty())
            return result;
        validate(inputTime);
        if (!targetGains)
        {
            next.mClock.mPreviousTime = inputTime;
            return result;
        }
        validate(targetGains->mHierarchy);
        validate(targetGains->mVelocity);
        validate(next.mCachedGains.mHierarchy);
        validate(next.mCachedGains.mVelocity);
        const float keyTime = advancePhysicalBlendClock(next.mClock, result.mTimeCache, next.mTiming, inputTime);
        const auto evaluated = evaluatePhysicalBlendKeys(next.mKeys, keyTime, next.mCursor);
        next.mCursor = evaluated.mCursor;
        if (evaluated.mGains)
        {
            if (next.mCachedGains.mHierarchy < 0.f)
                next.mCachedGains = *targetGains;
            result.mTargetGains = evaluated.mGains;
        }
        if (keyTime == next.mTiming.mStopKey && (next.mTiming.mFlags & 6) == 4)
        {
            // Reset precedes restoration in Original8AAD60. Retain the cached
            // segment cursor even when its key array becomes empty.
            if (next.mTiming.mFlags & 0x40)
            {
                next.mKeys.clear();
                next.mCachedGains = {-1.f, -1.f};
                next.mSetupState = 0;
            }
            result.mRemoveVelocityController = (next.mTiming.mFlags & 0x80) && hasVelocityController;
            if ((next.mTiming.mFlags & 0x100) && next.mCachedGains.mHierarchy >= 0.f)
            {
                result.mTargetGains = next.mCachedGains;
                next.mCachedGains = {-1.f, -1.f};
            }
            next.mTiming.mFlags &= 0xfff7u;
            constexpr float sentinel = -std::numeric_limits<float>::max();
            next.mClock.mPreviousTime = sentinel;
            if (next.mTiming.mFlags & 1)
                next.mClock.mStartTime = sentinel;
        }
        return result;
    }

    float physicalBlendDurationForFilter(
        const PhysicalBlendDurationTables& tables, std::uint32_t filter, bool getUp)
    {
        const auto index = (filter >> 8) & 31u;
        const float duration = getUp ? tables.mGetUp[index] : tables.mKnockdown[index];
        validate(duration);
        return duration;
    }
}
