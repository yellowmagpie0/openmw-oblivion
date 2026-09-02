/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#ifndef OPENMW_COMPONENTS_ESM4_AIPACKAGEDATA_H
#define OPENMW_COMPONENTS_ESM4_AIPACKAGEDATA_H

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>
#include <components/esm/formkey.hpp>

namespace ESM4
{
    // TES4's package type is deliberately separate from the similarly named
    // TES3/FO3/TES5 enums.  In particular, values 12 and above are not
    // silently interpreted as Sandbox/Patrol/Guard packages.
    enum class AIPackageType : std::uint8_t
    {
        Find = 0,
        Follow = 1,
        Escort = 2,
        Eat = 3,
        Sleep = 4,
        Wander = 5,
        Travel = 6,
        Accompany = 7,
        UseItemAt = 8,
        Ambush = 9,
        FleeNotCombat = 10,
        CastMagic = 11,
        // Pursue is a runtime-only intent installed by AI/script control;
        // stock PACK records do not encode raw type 12.
        Pursue = 12,
        Unknown = 0xff,
    };

    using PackageType = AIPackageType;

    std::optional<AIPackageType> packageTypeFromRaw(std::int32_t value);
    std::string_view packageTypeName(AIPackageType value);

    // Match a TES4 package's PTDT object-type filter against a native record
    // type.  The numeric values are the Oblivion/Construction Set order and
    // are intentionally kept in the pure data layer so the parser, audit,
    // and runtime cannot drift apart.
    bool packageObjectTypeMatches(std::uint32_t objectType, ESM::RecNameInts recordType);
    bool isKnownPackageObjectType(std::uint32_t objectType);

    // These are the TES4 names used by the Construction Set/xEdit definition.
    // Reserved bits are named rather than discarded; their raw values remain
    // available to diagnostics even though they do not alter M14 routing.
    enum class PackageFlag : std::uint32_t
    {
        OffersServices = 0x00000001,
        MustReachLocation = 0x00000002,
        MustComplete = 0x00000004,
        LockDoorsAtStart = 0x00000008,
        LockDoorsAtEnd = 0x00000010,
        LockDoorsAtLocation = 0x00000020,
        UnlockDoorsAtStart = 0x00000040,
        UnlockDoorsAtEnd = 0x00000080,
        UnlockDoorsAtLocation = 0x00000100,
        ContinueIfPlayerNear = 0x00000200,
        OncePerDay = 0x00000400,
        Reserved00000800 = 0x00000800,
        SkipFalloutBehaviour = 0x00001000,
        AlwaysRun = 0x00002000,
        Reserved00004000 = 0x00004000,
        Reserved00008000 = 0x00008000,
        Reserved00010000 = 0x00010000,
        AlwaysSneak = 0x00020000,
        AllowSwimming = 0x00040000,
        AllowFalls = 0x00080000,
        ArmourUnequipped = 0x00100000,
        WeaponsUnequipped = 0x00200000,
        DefensiveCombat = 0x00400000,
        UseHorse = 0x00800000,
        NoIdleAnimations = 0x01000000,
        Reserved02000000 = 0x02000000,
        Reserved04000000 = 0x04000000,
        Reserved08000000 = 0x08000000,
        Reserved10000000 = 0x10000000,
        Reserved20000000 = 0x20000000,
        Reserved40000000 = 0x40000000,
        Reserved80000000 = 0x80000000,
    };

    struct PackageFlags
    {
        std::uint32_t mRaw = 0;
        std::uint32_t mKnown = 0;
        std::uint32_t mReserved = 0;

        bool has(PackageFlag flag) const
        {
            return (mRaw & static_cast<std::uint32_t>(flag)) != 0;
        }
    };

    PackageFlags decodePackageFlags(std::uint32_t value);

    struct CalendarInstant
    {
        std::int32_t mYear = 1;
        std::int32_t mMonth = 0;
        std::int32_t mDay = 1;
        double mHour = 0.0;

        friend bool operator==(const CalendarInstant&, const CalendarInstant&) = default;
    };

    struct ScheduleWindow
    {
        CalendarInstant mStart;
        CalendarInstant mEnd;
        double mDurationHours = 0.0;

        friend bool operator==(const ScheduleWindow&, const ScheduleWindow&) = default;
    };

    struct PackageSchedule
    {
        // TES4 uses -1 for month/day/time wildcards and 0 for a wildcard date.
        std::int8_t mMonth = -1;
        std::int8_t mDayOfWeek = -1;
        std::uint8_t mDate = 0;
        std::int8_t mStartHour = -1;
        std::int32_t mDuration = 0;

        bool isValid(std::string* reason = nullptr) const;
        bool matchesDate(const CalendarInstant& instant) const;
        bool isEligible(const CalendarInstant& instant) const;
        std::optional<ScheduleWindow> activeWindow(const CalendarInstant& instant) const;

        // The game stores an integer number of game hours.  A zero duration is
        // the native all-day/default-package value; it is not an empty window.
        double effectiveDurationHours() const;
    };

    // Sunday is zero, matching the TES4 condition-function convention.
    bool isValidCalendarInstant(const CalendarInstant& instant);
    std::int32_t calendarDayOfWeek(const CalendarInstant& instant);

    // Return the exact in-game hours between two normalized calendar instants.
    // Keeping this in the pure package-data layer prevents the runtime selector
    // from treating a package's full schedule duration as its remaining time
    // when the actor is first evaluated halfway through the active window.
    double calendarHoursUntil(const CalendarInstant& from, const CalendarInstant& to);

    // Add game hours using the same proleptic calendar and month/date rules as
    // schedule evaluation. This never mutates the world's authoritative clock.
    CalendarInstant calendarAddHours(const CalendarInstant& instant, double hours);

    enum class PackageLocationKind : std::uint8_t
    {
        None,
        NearReference,
        InCell,
        CurrentLocation,
        EditorLocation,
        ObjectId,
        ObjectType,
        NearLinkedReference,
        AtPackageLocation,
        Unknown,
    };

    struct PackageLocation
    {
        PackageLocationKind mKind = PackageLocationKind::None;
        std::int32_t mRawKind = 0xff;
        ESM::FormId mReference{}; // adjusted load-order ID, for diagnostics
        ESM::FormKey mReferenceKey; // stable ID used by runtime systems
        std::uint32_t mObjectType = 0;
        std::int32_t mRadius = 0;

        bool hasReference() const;
        bool isObjectType() const { return mKind == PackageLocationKind::ObjectType; }
    };

    enum class PackageTargetKind : std::uint8_t
    {
        None,
        SpecificReference,
        ObjectId,
        ObjectType,
        LinkedReference,
        Unknown,
    };

    struct PackageTarget
    {
        PackageTargetKind mKind = PackageTargetKind::None;
        std::int32_t mRawKind = 0xff;
        ESM::FormId mReference{};
        ESM::FormKey mReferenceKey;
        std::uint32_t mObjectType = 0;
        std::int32_t mDistance = 0;

        bool hasReference() const;
        bool isObjectType() const { return mKind == PackageTargetKind::ObjectType; }
    };

    enum class ConditionOperator : std::uint8_t
    {
        Equal,
        NotEqual,
        Greater,
        GreaterOrEqual,
        Less,
        LessOrEqual,
        Unknown,
    };

    ConditionOperator conditionOperator(std::uint8_t flags);
    std::string_view conditionOperatorName(ConditionOperator value);

    // CTDA's final DWORD is the native run-on selector.  Keep the raw value
    // in PackageCondition as well: values not defined by TES4 must remain
    // observable and must never be evaluated against the actor itself.
    enum class ConditionRunOn : std::uint8_t
    {
        Subject = 0,
        Target = 1,
        Reference = 2,
        CombatTarget = 3,
        LinkedReference = 4,
        Unknown = 0xff,
    };

    ConditionRunOn conditionRunOnFromRaw(std::uint32_t value);
    std::string_view conditionRunOnName(ConditionRunOn value);

    struct ConditionParameter
    {
        std::uint32_t mRaw = 0;
        bool mIsFormId = false;
        std::int32_t mNumber = 0;
        ESM::FormId mReference{};
        ESM::FormKey mReferenceKey;

        friend bool operator==(const ConditionParameter&, const ConditionParameter&) = default;
    };

    struct PackageCondition
    {
        std::uint8_t mFlags = 0;
        ConditionOperator mOperator = ConditionOperator::Unknown;
        bool mOr = false;
        bool mRunOnTarget = false;
        bool mUseGlobal = false;
        float mComparisonValue = 0.f;
        ESM::FormId mComparisonGlobal{};
        ESM::FormKey mComparisonGlobalKey;
        std::int32_t mFunction = 0;
        ConditionParameter mParameter1;
        ConditionParameter mParameter2;
        ESM::FormId mRunOnReference{};
        ESM::FormKey mRunOnReferenceKey;
        ConditionRunOn mRunOn = ConditionRunOn::Subject;
        // Retain the encoded DWORD losslessly for diagnostics and future
        // extended layouts.  In a native 24-byte CTDA this is the run-on
        // selector; 20-byte CTDT has no such field and stays at Subject.
        std::uint32_t mUnknownTail = 0;
        std::uint8_t mEncodedSize = 0;

        friend bool operator==(const PackageCondition&, const PackageCondition&) = default;
    };

    // Decode either the native 20-byte CTDT or the 24-byte CTDA layout.  The
    // resolver receives the raw on-disk FormID and must return the stable key
    // for it (or a null key for a null/unresolved reference).
    PackageCondition decodePackageCondition(std::span<const std::uint8_t> data,
        const std::function<ESM::FormKey(ESM::FormId)>& resolver);

    bool conditionIsTrue(double actual, double expected, ConditionOperator operation);
}

#endif
