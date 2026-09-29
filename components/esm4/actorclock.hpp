#ifndef OPENMW_ESM4_ACTORCLOCK_H
#define OPENMW_ESM4_ACTORCLOCK_H

namespace ESM4
{
    // Native manager setter: reset nonfinite values and times strictly above
    // 100000 seconds to +0. This is not a modulo operation. Finite negative
    // values (including -0) survive the original setter.
    float normalizeActorManagerTime(float time) noexcept;
    // Both native callers store the elapsed/addition results as binary32
    // before invoking the setter. Zero time scale consequently resets to0.
    float advanceActorManagerTime(float time, float elapsed) noexcept;
    float actorManagerTimeAfterHour(float time, float timeScale) noexcept;

    // Clock arithmetic at the start of the common actor update. The caller
    // owns update eligibility, effects/resources and committing the new time
    // after the update. A rewind/uninitialized negative previous time normally
    // produces zero elapsed, except a new time strictly inside (0, .3f).
    // Both inputs must be finite; no native actor/process pointers are retained.
    float actorUpdateDuration(float time, float previousTime);
}

#endif
