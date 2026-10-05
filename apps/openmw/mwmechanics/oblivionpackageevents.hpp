/*
  Copyright (C) 2026 OpenMW contributors

  This file is part of OpenMW.

  OpenMW is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License version 3, or (at your option) any
  later version.
*/
#ifndef OPENMW_MWMECHANICS_OBLIVIONPACKAGEEVENTS_H
#define OPENMW_MWMECHANICS_OBLIVIONPACKAGEEVENTS_H

#include <deque>
#include <optional>
#include <span>
#include <vector>

#include <components/esm4/aiselection.hpp>
#include <components/esm4/runtimestate.hpp>

namespace MWMechanics
{
    inline bool oblivionPackageSelectionMatches(const ESM4::RuntimeActorAiState& state,
        const std::optional<ESM4::ScheduleWindow>& selectedWindow,
        const ESM4::PackageSelection& selection, const ESM4::CalendarInstant& now)
    {
        const bool sameWindow = selectedWindow.has_value() == selection.mWindow.has_value()
            && (!selectedWindow || *selectedWindow == *selection.mWindow
                || ESM4::calendarHoursUntil(now, selectedWindow->mEnd) > 0.0);
        return (!selection.hasPackage() && state.mPackage.isNull()
                   && state.mSource == ESM4::PackageSource::None)
            || (selection.hasPackage() && state.mSource == selection.mSource
                && state.mPackage == selection.mPackage && state.mPackageType == selection.mType
                && sameWindow);
    }

    // Dispatch outside mechanics iteration. Captures retain outstanding events
    // even when a native callback saves, changes packages, or replaces the game.
    class OblivionPackageDoneQueue
    {
        std::deque<ESM4::RuntimePackageDoneEvent> mPending;
        std::uint64_t mEpoch = 0;
        bool mDispatching = false;

    public:
        void record(ESM4::PackagePhase from, ESM4::PackagePhase to,
            const ESM::FormKey& actor, const ESM::FormKey& package)
        {
            if (from != ESM4::PackagePhase::Complete && to == ESM4::PackagePhase::Complete
                && !actor.isNull() && !package.isNull())
                mPending.push_back({ actor, package });
        }

        std::vector<ESM4::RuntimePackageDoneEvent> capture() const
        {
            return { mPending.begin(), mPending.end() };
        }

        void restore(std::span<const ESM4::RuntimePackageDoneEvent> events)
        {
            std::deque<ESM4::RuntimePackageDoneEvent> prepared(events.begin(), events.end());
            installPrepared(prepared);
        }

        // Preserve the dispatch guard while invalidating the old callback batch.
        void installPrepared(std::deque<ESM4::RuntimePackageDoneEvent>& events) noexcept
        {
            mPending.swap(events);
            ++mEpoch;
        }

        template <class Callback>
        void dispatch(Callback&& callback)
        {
            if (mDispatching)
                return;
            mDispatching = true;
            struct Reset
            {
                bool& mFlag;
                ~Reset() { mFlag = false; }
            } reset{ mDispatching };
            const auto epoch = mEpoch;
            const std::size_t count = mPending.size();
            for (std::size_t i = 0; i < count && epoch == mEpoch && !mPending.empty(); ++i)
            {
                const auto event = mPending.front();
                mPending.pop_front();
                callback(event);
            }
        }
    };
}

#endif
