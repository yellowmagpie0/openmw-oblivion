/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#include "aipackagedata.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "formidfields.hpp"

namespace ESM4
{
    namespace
    {
        constexpr std::uint32_t semanticPackageFlags =
            static_cast<std::uint32_t>(PackageFlag::OffersServices)
            | static_cast<std::uint32_t>(PackageFlag::MustReachLocation)
            | static_cast<std::uint32_t>(PackageFlag::MustComplete)
            | static_cast<std::uint32_t>(PackageFlag::LockDoorsAtStart)
            | static_cast<std::uint32_t>(PackageFlag::LockDoorsAtEnd)
            | static_cast<std::uint32_t>(PackageFlag::LockDoorsAtLocation)
            | static_cast<std::uint32_t>(PackageFlag::UnlockDoorsAtStart)
            | static_cast<std::uint32_t>(PackageFlag::UnlockDoorsAtEnd)
            | static_cast<std::uint32_t>(PackageFlag::UnlockDoorsAtLocation)
            | static_cast<std::uint32_t>(PackageFlag::ContinueIfPlayerNear)
            | static_cast<std::uint32_t>(PackageFlag::OncePerDay)
            | static_cast<std::uint32_t>(PackageFlag::SkipFalloutBehaviour)
            | static_cast<std::uint32_t>(PackageFlag::AlwaysRun)
            | static_cast<std::uint32_t>(PackageFlag::AlwaysSneak)
            | static_cast<std::uint32_t>(PackageFlag::AllowSwimming)
            | static_cast<std::uint32_t>(PackageFlag::AllowFalls)
            | static_cast<std::uint32_t>(PackageFlag::ArmourUnequipped)
            | static_cast<std::uint32_t>(PackageFlag::WeaponsUnequipped)
            | static_cast<std::uint32_t>(PackageFlag::DefensiveCombat)
            | static_cast<std::uint32_t>(PackageFlag::UseHorse)
            | static_cast<std::uint32_t>(PackageFlag::NoIdleAnimations);

        constexpr std::array<std::int32_t, 12> packageTypes = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        };

        constexpr std::int64_t daysFromCivil(std::int32_t year, std::int32_t month, std::int32_t day)
        {
            // Howard Hinnant's proleptic Gregorian conversion.  The offset is
            // immaterial; only differences and weekday modulo seven are used.
            year -= month <= 2;
            const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
            const std::uint32_t yearOfEra = static_cast<std::uint32_t>(year - era * 400);
            const std::uint32_t monthPrime = static_cast<std::uint32_t>(month + (month > 2 ? -3 : 9));
            const std::uint32_t dayOfYear = (153 * monthPrime + 2) / 5 + static_cast<std::uint32_t>(day) - 1;
            const std::uint32_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
            return era * 146097 + static_cast<std::int64_t>(dayOfEra);
        }

        constexpr bool isLeapYear(std::int32_t year)
        {
            return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        }

        constexpr std::int32_t daysInMonth(std::int32_t year, std::int32_t month)
        {
            constexpr std::array<std::int32_t, 12> days = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
            if (month < 0 || month >= static_cast<std::int32_t>(days.size()))
                return 0;
            return days[month] + (month == 1 && isLeapYear(year) ? 1 : 0);
        }

        CalendarInstant fromDayAndHour(std::int64_t day, double hour)
        {
            // Invert daysFromCivil by a bounded search around the current
            // game-year.  Game clocks are small and this avoids a second
            // non-obvious calendar algorithm in a data-only component.
            const std::int32_t approximateYear = static_cast<std::int32_t>(day / 365) + 1;
            std::int32_t year = approximateYear;
            while (daysFromCivil(year + 1, 1, 1) <= day)
                ++year;
            while (daysFromCivil(year, 1, 1) > day)
                --year;

            std::int32_t month = 1;
            while (month < 12 && daysFromCivil(year, month + 1, 1) <= day)
                ++month;
            const std::int32_t date = static_cast<std::int32_t>(day - daysFromCivil(year, month, 1)) + 1;
            return { year, month - 1, date, hour };
        }

        CalendarInstant normalize(CalendarInstant value)
        {
            if (!std::isfinite(value.mHour))
                throw std::invalid_argument("TES4 calendar hour must be finite");
            const double wholeDays = std::floor(value.mHour / 24.0);
            value.mHour -= wholeDays * 24.0;
            const std::int64_t day = daysFromCivil(value.mYear, value.mMonth + 1, value.mDay)
                + static_cast<std::int64_t>(wholeDays);
            return fromDayAndHour(day, value.mHour);
        }

        bool matchesWeekday(std::int8_t value, std::int32_t weekday)
        {
            if (value == -1)
                return true;
            if (value >= 0 && value <= 6)
                return value == weekday;
            switch (value)
            {
                case 7: // Morndas to Fredas
                    return weekday >= 1 && weekday <= 5;
                case 8: // Loredas, Sundas
                    return weekday == 0 || weekday == 6;
                case 9: // Morndas, Middas, Fredas
                    return weekday == 1 || weekday == 3 || weekday == 5;
                case 10: // Tirdas, Turdas
                    return weekday == 2 || weekday == 4;
                default:
                    return false;
            }
        }

        std::int32_t weekday(CalendarInstant instant)
        {
            // 2000-01-02 was a Sunday.  Modulo is normalized for years before
            // that point so save fixtures can use any signed year.
            const std::int64_t days = daysFromCivil(instant.mYear, instant.mMonth + 1, instant.mDay);
            // daysFromCivil's epoch is one day after the weekday anchor used
            // by the TES4 convention.  2000-01-02 is Sundas (zero).
            const std::int64_t value = (days + 3) % 7;
            return static_cast<std::int32_t>((value + 7) % 7);
        }

        std::uint32_t readU32(std::span<const std::uint8_t> data, std::size_t offset)
        {
            if (offset + 4 > data.size())
                throw std::invalid_argument("TES4 condition payload is truncated");
            return static_cast<std::uint32_t>(data[offset])
                | static_cast<std::uint32_t>(data[offset + 1]) << 8
                | static_cast<std::uint32_t>(data[offset + 2]) << 16
                | static_cast<std::uint32_t>(data[offset + 3]) << 24;
        }

        float readFloat(std::span<const std::uint8_t> data, std::size_t offset)
        {
            const std::uint32_t value = readU32(data, offset);
            float result;
            static_assert(sizeof(result) == sizeof(value));
            std::memcpy(&result, &value, sizeof(result));
            return result;
        }

        ConditionParameter decodeParameter(std::span<const std::uint8_t> data, std::size_t offset,
            bool isFormId, const std::function<ESM::FormKey(ESM::FormId)>& resolver)
        {
            ConditionParameter result;
            result.mRaw = readU32(data, offset);
            result.mIsFormId = isFormId;
            result.mNumber = static_cast<std::int32_t>(result.mRaw);
            if (isFormId)
            {
                result.mReference = ESM::FormId::fromUint32(result.mRaw);
                result.mReferenceKey = resolver(result.mReference);
            }
            return result;
        }
    }

    std::optional<AIPackageType> packageTypeFromRaw(std::int32_t value)
    {
        if (value < packageTypes.front() || value > packageTypes.back())
            return std::nullopt;
        return static_cast<AIPackageType>(value);
    }

    std::string_view packageTypeName(AIPackageType value)
    {
        switch (value)
        {
            case AIPackageType::Find:
                return "Find";
            case AIPackageType::Follow:
                return "Follow";
            case AIPackageType::Escort:
                return "Escort";
            case AIPackageType::Eat:
                return "Eat";
            case AIPackageType::Sleep:
                return "Sleep";
            case AIPackageType::Wander:
                return "Wander";
            case AIPackageType::Travel:
                return "Travel";
            case AIPackageType::Accompany:
                return "Accompany";
            case AIPackageType::UseItemAt:
                return "UseItemAt";
            case AIPackageType::Ambush:
                return "Ambush";
            case AIPackageType::FleeNotCombat:
                return "FleeNotCombat";
            case AIPackageType::CastMagic:
                return "CastMagic";
            case AIPackageType::Pursue:
                return "Pursue";
            case AIPackageType::Unknown:
                return "Unknown";
        }
        return "Unknown";
    }

    bool isKnownPackageObjectType(std::uint32_t objectType)
    {
        // Zero is the explicit "none" value used when a target/location has
        // no object filter.  1..35 are the Construction Set filters.
        return objectType <= 35;
    }

    bool packageObjectTypeMatches(std::uint32_t objectType, ESM::RecNameInts recordType)
    {
        // These values are the native TES4 package object-type filters.  Do
        // not replace this table with the later Fallout/TES5 ordering.
        switch (objectType)
        {
            case 1: // activator
                return recordType == ESM::REC_ACTI4;
            case 2: // apparatus
                return recordType == ESM::REC_APPA4;
            case 3: // armor
                return recordType == ESM::REC_ARMO4;
            case 4: // book
                return recordType == ESM::REC_BOOK4;
            case 5: // clothing
                return recordType == ESM::REC_CLOT4;
            case 6: // container
                return recordType == ESM::REC_CONT4;
            case 7: // door
                return recordType == ESM::REC_DOOR4;
            case 8: // ingredient
                return recordType == ESM::REC_INGR4;
            case 9: // light
                return recordType == ESM::REC_LIGH4;
            case 10: // miscellaneous object
                return recordType == ESM::REC_MISC4;
            case 11: // flora
                return recordType == ESM::REC_FLOR4;
            case 12: // furniture
                return recordType == ESM::REC_FURN4;
            case 13: // weapon, any subtype
            case 23: // weapon, no subtype
            case 24: // melee weapon
            case 25: // ranged weapon
                return recordType == ESM::REC_WEAP4;
            case 14: // ammunition
                return recordType == ESM::REC_AMMO4;
            case 15: // NPC
                return recordType == ESM::REC_NPC_4;
            case 16: // creature
                return recordType == ESM::REC_CREA4;
            case 17: // soul gem
                return recordType == ESM::REC_SLGM4;
            case 18: // key
                return recordType == ESM::REC_KEYM4;
            case 19: // alchemy
            case 20: // food (food is an ALCH record in TES4)
                return recordType == ESM::REC_ALCH4;
            case 21: // combat wearable
            case 22: // wearable
                return recordType == ESM::REC_ARMO4 || recordType == ESM::REC_CLOT4;
            case 26: // spell, any
            case 27: // spell, ranged target
            case 28: // spell, touch
            case 29: // spell, self
            case 30: // spell, Alteration school
            case 31: // spell, Conjuration school
            case 32: // spell, Destruction school
            case 33: // spell, Illusion school
            case 34: // spell, Mysticism school
            case 35: // spell, Restoration school
                return recordType == ESM::REC_SPEL4;
            default:
                return false;
        }
    }

    PackageFlags decodePackageFlags(std::uint32_t value)
    {
        return { value, value & semanticPackageFlags, value & ~semanticPackageFlags };
    }

    bool PackageSchedule::isValid(std::string* reason) const
    {
        auto invalid = [reason](std::string message) {
            if (reason != nullptr)
                *reason = std::move(message);
            return false;
        };
        if (mMonth < -1 || mMonth > 11)
            return invalid("month must be -1 or in [0, 11]");
        if (mDayOfWeek < -1 || mDayOfWeek > 10)
            return invalid("day of week must be -1 or in [0, 10]");
        if (mDate > 31)
            return invalid("date must be 0 or in [1, 31]");
        // The CK stores a wildcard date as zero.  A fixed date must also be
        // a date that can occur in the selected month.  February 29 is kept
        // valid because the schedule repeats on leap years; using a leap
        // year here gives the validation the least surprising result.
        if (mMonth != -1 && mDate != 0 && mDate > daysInMonth(2000, mMonth))
            return invalid("date is outside the selected month");
        if (mStartHour < -1 || mStartHour > 23)
            return invalid("start hour must be -1 or in [0, 23]");
        if (mDuration < 0)
            return invalid("duration must not be negative");
        if (reason != nullptr)
            reason->clear();
        return true;
    }

    bool PackageSchedule::matchesDate(const CalendarInstant& instant) const
    {
        if (!isValid() || instant.mMonth < 0 || instant.mMonth > 11
            || instant.mDay < 1 || instant.mDay > daysInMonth(instant.mYear, instant.mMonth))
            return false;
        if (mMonth != -1 && mMonth != instant.mMonth)
            return false;
        if (mDate != 0 && mDate != instant.mDay)
            return false;
        return matchesWeekday(mDayOfWeek, weekday(instant));
    }

    double PackageSchedule::effectiveDurationHours() const
    {
        if (mDuration == 0)
            return 24.0;
        return static_cast<double>(mDuration);
    }

    std::int32_t calendarDayOfWeek(const CalendarInstant& instant)
    {
        return weekday(instant);
    }

    bool isValidCalendarInstant(const CalendarInstant& instant)
    {
        return instant.mMonth >= 0 && instant.mMonth < 12 && instant.mDay >= 1
            && instant.mDay <= daysInMonth(instant.mYear, instant.mMonth) && std::isfinite(instant.mHour)
            && instant.mHour >= 0.0 && instant.mHour < 24.0;
    }

    double calendarHoursUntil(const CalendarInstant& from, const CalendarInstant& to)
    {
        const CalendarInstant normalizedFrom = normalize(from);
        const CalendarInstant normalizedTo = normalize(to);
        return static_cast<double>(daysFromCivil(normalizedTo.mYear, normalizedTo.mMonth + 1, normalizedTo.mDay)
                   - daysFromCivil(normalizedFrom.mYear, normalizedFrom.mMonth + 1, normalizedFrom.mDay))
                * 24.0
            + normalizedTo.mHour - normalizedFrom.mHour;
    }

    CalendarInstant calendarAddHours(const CalendarInstant& instant, double hours)
    {
        if (!std::isfinite(hours))
            throw std::invalid_argument("TES4 calendar addition must be finite");
        CalendarInstant result = instant;
        result.mHour += hours;
        return normalize(result);
    }

    std::optional<ScheduleWindow> PackageSchedule::activeWindow(const CalendarInstant& value) const
    {
        if (!isValid())
            return std::nullopt;
        const CalendarInstant instant = normalize(value);
        const double duration = effectiveDurationHours();
        const std::int64_t instantDay = daysFromCivil(instant.mYear, instant.mMonth + 1, instant.mDay);
        const auto inWindow = [duration](const CalendarInstant& now, const CalendarInstant& start) {
            const double elapsed = static_cast<double>(daysFromCivil(now.mYear, now.mMonth + 1, now.mDay)
                    - daysFromCivil(start.mYear, start.mMonth + 1, start.mDay))
                * 24.0 + now.mHour - start.mHour;
            return elapsed >= 0.0 && elapsed < duration;
        };

        // "Any" is a real TES4 start-time value, not midnight.  It means
        // that the package may begin whenever the date filters match.  Use
        // the current instant as the start of the window; otherwise a short
        // wildcard package would only be eligible during the first hours of
        // the day.  A previous matching day is still searched below so a
        // long wildcard window can cross midnight.
        if (mStartHour == -1 && matchesDate(instant))
        {
            const CalendarInstant end
                = normalize({ instant.mYear, instant.mMonth, instant.mDay, instant.mHour + duration });
            return ScheduleWindow{ instant, end, duration };
        }

        // A package's date filters describe the start day, not every day of
        // its active interval. Searching back from the current day handles
        // duration across midnight, month/year boundaries, wildcard time,
        // and sparse filters such as Feb 29 plus a weekday. The Gregorian
        // 400-year cycle bounds the search for any repeating filter.
        const std::int64_t durationDays = static_cast<std::int64_t>(std::ceil(duration / 24.0));
        constexpr std::int64_t maxCalendarCycleDays = 146097;
        const std::int64_t lookback
            = std::min(maxCalendarCycleDays + 1, std::max<std::int64_t>(1, durationDays + 1));
        for (std::int64_t offset = 0; offset <= lookback; ++offset)
        {
            const CalendarInstant start = fromDayAndHour(instantDay - offset,
                mStartHour == -1 ? 0.0 : static_cast<double>(mStartHour));
            if (matchesDate(start) && inWindow(instant, start))
            {
                const CalendarInstant end = normalize({ start.mYear, start.mMonth, start.mDay,
                    start.mHour + duration });
                return ScheduleWindow{ start, end, duration };
            }
        }
        return std::nullopt;
    }

    bool PackageSchedule::isEligible(const CalendarInstant& instant) const
    {
        return activeWindow(instant).has_value();
    }

    bool PackageLocation::hasReference() const
    {
        return mKind == PackageLocationKind::NearReference || mKind == PackageLocationKind::InCell
            || mKind == PackageLocationKind::EditorLocation || mKind == PackageLocationKind::ObjectId;
    }

    bool PackageTarget::hasReference() const
    {
        return mKind == PackageTargetKind::SpecificReference || mKind == PackageTargetKind::ObjectId
            || mKind == PackageTargetKind::LinkedReference;
    }

    ConditionOperator conditionOperator(std::uint8_t flags)
    {
        switch (flags & 0xf0)
        {
            case 0x00:
                return ConditionOperator::Equal;
            case 0x20:
                return ConditionOperator::NotEqual;
            case 0x40:
                return ConditionOperator::Greater;
            case 0x60:
                return ConditionOperator::GreaterOrEqual;
            case 0x80:
                return ConditionOperator::Less;
            case 0xa0:
                return ConditionOperator::LessOrEqual;
            default:
                return ConditionOperator::Unknown;
        }
    }

    std::string_view conditionOperatorName(ConditionOperator value)
    {
        switch (value)
        {
            case ConditionOperator::Equal:
                return "==";
            case ConditionOperator::NotEqual:
                return "!=";
            case ConditionOperator::Greater:
                return ">";
            case ConditionOperator::GreaterOrEqual:
                return ">=";
            case ConditionOperator::Less:
                return "<";
            case ConditionOperator::LessOrEqual:
                return "<=";
            case ConditionOperator::Unknown:
                return "?";
        }
        return "?";
    }

    ConditionRunOn conditionRunOnFromRaw(std::uint32_t value)
    {
        switch (value)
        {
            case 0: return ConditionRunOn::Subject;
            case 1: return ConditionRunOn::Target;
            case 2: return ConditionRunOn::Reference;
            case 3: return ConditionRunOn::CombatTarget;
            case 4: return ConditionRunOn::LinkedReference;
            default: return ConditionRunOn::Unknown;
        }
    }

    std::string_view conditionRunOnName(ConditionRunOn value)
    {
        switch (value)
        {
            case ConditionRunOn::Subject: return "Subject";
            case ConditionRunOn::Target: return "Target";
            case ConditionRunOn::Reference: return "Reference";
            case ConditionRunOn::CombatTarget: return "CombatTarget";
            case ConditionRunOn::LinkedReference: return "LinkedReference";
            case ConditionRunOn::Unknown: return "Unknown";
        }
        return "Unknown";
    }

    PackageCondition decodePackageCondition(std::span<const std::uint8_t> data,
        const std::function<ESM::FormKey(ESM::FormId)>& resolver)
    {
        if (data.size() != 20 && data.size() != 24)
            throw std::invalid_argument("TES4 package condition must be 20 or 24 bytes");

        PackageCondition result;
        result.mEncodedSize = static_cast<std::uint8_t>(data.size());
        result.mFlags = data[0];
        result.mOperator = conditionOperator(result.mFlags);
        result.mOr = (result.mFlags & 0x01) != 0;
        result.mRunOnTarget = (result.mFlags & 0x02) != 0;
        result.mUseGlobal = (result.mFlags & 0x04) != 0;
        result.mComparisonValue = readFloat(data, 4);
        result.mFunction = static_cast<std::int32_t>(readU32(data, 8));

        if (result.mUseGlobal)
        {
            result.mComparisonGlobal = ESM::FormId::fromUint32(readU32(data, 4));
            result.mComparisonGlobalKey = resolver(result.mComparisonGlobal);
        }

        const std::uint8_t formIdMask = conditionParameterFormIdMask(result.mFunction);
        result.mParameter1 = decodeParameter(data, 12, (formIdMask & 1) != 0, resolver);
        result.mParameter2 = decodeParameter(data, 16, (formIdMask & 2) != 0, resolver);
        if (data.size() == 24)
        {
            result.mUnknownTail = readU32(data, 20);
            result.mRunOn = conditionRunOnFromRaw(result.mUnknownTail);
        }
        return result;
    }

    bool conditionIsTrue(double actual, double expected, ConditionOperator operation)
    {
        if (!std::isfinite(actual) || !std::isfinite(expected))
            return false;
        switch (operation)
        {
            case ConditionOperator::Equal:
                return actual == expected;
            case ConditionOperator::NotEqual:
                return actual != expected;
            case ConditionOperator::Greater:
                return actual > expected;
            case ConditionOperator::GreaterOrEqual:
                return actual >= expected;
            case ConditionOperator::Less:
                return actual < expected;
            case ConditionOperator::LessOrEqual:
                return actual <= expected;
            case ConditionOperator::Unknown:
                return false;
        }
        return false;
    }
}
