# Windows live-session fixes — 2026-11-15

## Current handoff

Target: responsive tutorial play using the player's existing saved game, with
working armor UI and native skill progression. The bounded routing/reference
lookup performance chunk and both recurring error repairs are verified. The
saved game remains slow; this is not a completed performance course. The
effect-owner and recovered-arrow scan chunks are also verified: final autosave
performance is 17.9–18.6 FPS, versus the original 7.3–7.4 FPS. Remaining cost
includes low-process AI routing and physics-worker waits; smooth gameplay is
still an acceptance gap. Next: a bounded AI-routing/physics investigation,
using the final CPU sample below. Broader tutorial gates remain in
[the tutorial handoff](TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md).

## Performance chunk

Street reachability is shared between mutually reachable directed cells and
invalidated with pathgrid topology/overlays. Generation checks are batched for
the callback-free low-process pass. Door plans reuse query-local door values,
stop longer reverse branches after a validated exit, and reject rooms with no
reachable exit before searching the world. Door runtime IDs avoid repeated
record resolution. No AI cadence or gameplay subsystem was disabled.

Saved-reference queries memoize offsets, validating the current key on every
hit; they retain neither saved values nor borrowed record pointers. Growth,
reordering, removal, key edits and replacement snapshots fall back to a current
lookup. Script update batches share a validated borrowed index for read queries;
vector replacement/growth falls back safely. Effect-field parsing uses sorted
prefix ranges. Scene validation reuses its attachment-tag string without
removing hierarchy checks or the equipment-loot repair.

MSVC RelWithDebInfo, Intel Graphics, 1280x720, sound on, uncapped/no VSync.
Two independent loads of the unchanged player autosave, 10-second warmup and
30-second samples, plus two fresh starts each at prison/city with 5-second
warmup and 20-second samples:

| Case | Control FPS | Fixed FPS | Mechanics ms/frame, control → fixed |
| --- | ---: | ---: | ---: |
| Player autosave | 7.30–7.38 | 14.24–14.73 | 91.96–94.39 → 36.26–37.79 |
| Fresh prison | 25.30–27.88 | 32.29–33.03 | 24.65–27.26 → 20.78–21.05 |
| Fresh city | 23.77–25.46 | 26.30–26.63 | 30.08–32.23 → 28.04–28.52 |

All six final benchmark processes exited cleanly. These short stationary
measurements do not establish traversal/combat/visual acceptance or eliminate
stutter. The autosave contains 52,625 reference states and 3,663 AI states;
physics still takes about 21 ms/frame and scripts about 7 ms/frame. Later CPU
samples identify environmental-fire/Charm metadata scans among the remaining
work. The larger performance backlog remains open.

Evidence under `build-windows/`:

- Full components **3,303/3,303** and engine **2,298/2,298**, exact inventories,
  no failures/skips: `checks-5c0f07c159714d2fa57252a40a1c8ad6`,
  `live-fix-perf-full.log`. Tested dirty-source fingerprint
  `46afe09b854ab5c0a26cca18badb52ba46ca9ad220d514ff07eb1d545f446edc`
  at HEAD `910ab912823fb00b72596658dac3d7cbbacd50c2`.
- Focused AI, ownership, doors, one-way street/overlay variants, saved-reference
  mutation, script, magic, traversal and physical-pose checks:
  `live-fix-perf-final-focused.log` and its referenced inventory receipt.
- Game build: `live-fix-perf-final-runtime-build.log`; executable SHA256
  `568f71872b78a0e9ce06b1fa8acffce281b04f1de9088962589be8eb0e5e48e7`.
- Original/control executable SHA256
  `f49a47970533dd9ae9dc4f1017faea9a05b0b83a6c09be2ba56b3d0658fd4e79`.
  Controls: `performance/live-fix-control-01/results.json` and
  `performance/live-save-control-02/results.json`. Final results:
  `performance/live-save-perf-final-02/results.json` and
  `performance/live-fix-final-fresh/results.json`, with raw traces/logs.
- Pristine autosave SHA256
  `c8f9993e12bba00f65aa21a5b492f3a72c5228aa855fbbf0cd16246ddb7f3eb0`.
  All probes use private profiles and leave that input unchanged.
- Intermediate attempts remain in `live-fix-perf-*` logs and
  `performance/live-save-optimized-*`. A discarded headless regression entered
  unsupported resident-renderer replanning; the replacement blocked-exit
  save/load regression passes. Failed launch/link-overlap attempts contain no
  accepted timings. An isolated diagnostic process was force-stopped after
  stack sampling; it is not clean-exit/runtime acceptance evidence.

The computer-use helper could not activate the private game window after one
binding refresh. No normal-input UI or screenshot acceptance is claimed.
No Windows sanitizer configuration was available for this run. This document
was written after the listed tests; the verified C++ sources are unchanged.
Earlier Windows provenance is linked in [the performance handoff](../WINDOWS-PERFORMANCE.md).

## Effect-owner scan repair

Fire and Charm updates now keep a derived list of owner keys, rather than
scanning all 52,625 saved references every frame. Timers, damage and caster
ownership are read from the current reference on each tick. Casts invalidate
membership; expiry removes membership without further allocation. Snapshot or
reference-vector changes rebuild the index, and load/simulation switch/reset
boundaries discard it. Disposition queries use the validated sparse reference
lookup. No saved schema, effect formula, AI cadence or collision validation
was changed.

The new regression covers initially empty lists, both effect types, in-place
casts, live timer changes, expiry, vector growth, snapshot replacement and
failed malformed-state rebuilds. Existing World tests cover recasts after
expiry, resistance/god mode, lethal damage without replay and partially elapsed
effects after save/load. Charm timer/ownership checks are engine tests, not a
rendered Charm-casting course.

- Focused **21/21**: `build-windows/checks-591bd24acc3441f9a195c263530b6051`,
  `live-fix-effects-focused.log`.
- Final complete engine inventory **2,300/2,300**, no failures/skips:
  `build-windows/checks-176f354483104e06abeba3de44f60430`,
  `live-fix-effects-full.log`. Tested source fingerprint at HEAD
  `773e11deab7688213d913548024465dc8442ce46`:
  `641df9736de3ffb69a98386b89c8b3f17539a4a833427608d4311e59c01c6bca`.
  Component sources are unchanged; the 3,303-test evidence above is reused.
- Final game SHA256:
  `bc03073a03441bb9097eec0e5728e0ecf19a4f67731fae4e7f41b46dc6e0b120`.
  Build: `build-windows/live-fix-effects-runtime-build.log`.
- Same stationary autosave protocol, two clean loads:
  `build-windows/performance/live-save-effects-final-01/results.json`.
  **17.87/18.08 FPS**; mechanics **32.71/32.32 ms**, physics **12.79/12.55 ms**,
  scripts **6.39/6.43 ms**. Frame p95 remains **84.8 ms**, p99 **103–106 ms**.
  The original input hash was checked again after both runs and is unchanged.
- Fresh prison **33.10/33.26 FPS**, city **28.35/27.58 FPS**, four clean exits:
  `build-windows/performance/live-fix-effects-final-fresh/results.json`.
- Rendered final-binary skill/error probe:
  `build-windows/live-skill-effects-final-01/result.json`: real movement,
  native Athletics progress, **zero skill/focus/frame errors**, clean exit,
  unchanged executable/input hashes. Original control evidence above is reused.

This documentation was updated after the checks; tested C++ sources are
unchanged. These results verify the identified repairs, not smooth 60 FPS,
normal-input loot/visual acceptance, or a warning-free tutorial course.

## Recovered-arrow scan follow-up

The final isolated 60-stack CPU sample found no recurring fire/Charm scan, but
five stacks still parsed recovered-arrow metadata. That parser now examines
only the sorted `native.arrow.` range, preserving all partial/unknown-field
rejections and leaving unrelated custom state alone. Existing regressions cover
empty, valid, partial, unknown, malformed, noncanonical and fading ownership.

- Complete final engine suite **2,300/2,300**, exact inventory, no failures/skips:
  `build-windows/checks-78f6a31fd0a640d4b56b9c9952370e8a`,
  `live-fix-arrow-scan-full.log`. Tested source fingerprint at HEAD
  `b80a693a154dd413f17b4bbb705605cefd1d7f41`:
  `032557dfe78be4b959ca782d59a2b54a69e8bf931be14df87a44d7e91c9acfe6`.
- Final game SHA256:
  `0b8486c5e737611fe30fc7979cf359c61280f51bc3aa305dbeebee7cf0921751`.
  Build: `build-windows/live-fix-arrow-scan-runtime-build.log`.
- Two clean autosave loads, same protocol:
  `build-windows/performance/live-save-arrow-final-01/results.json`.
  **18.60/17.86 FPS**, mechanics **32.12/33.62 ms**, physics **11.29/11.88 ms**,
  scripts **6.31/6.48 ms**; p95 **84.8/87.7 ms**, p99 **102.7/109.4 ms**.
  Short-run variance does not establish a separate FPS improvement from this
  small parser change. Fresh-scene evidence above uses the preceding executable;
  it is not relabeled as final-binary evidence.
- Final rendered movement/skill probe:
  `build-windows/live-skill-arrow-final-01/result.json`: passed, clean exit,
  **zero skill/focus/frame errors**, unchanged input/executable hashes.
- Diagnostic sample before this small parser follow-up:
  `build-windows/live_stack_sample_16048.json`. Of 60 stacks, 26 include
  low-process AI, 14 door planning, and 12 physics-worker waits during loose-item
  synchronization. These overlapping inclusive counts are diagnostic, not a
  timing measurement. Its private process exited cleanly; sampled timing in
  `performance/live-save-effects-diagnostic-01` is excluded from benchmarks.

The documentation update follows verification; the tested C++ is unchanged.
The final executable is ready for a human loot/traversal check. Original save,
full component evidence and limitations remain as recorded above.

## Armor UI repair

Native TES4 armor classification and item armor-rating UI queries now use the
winning native record before consulting Morrowind Lua interfaces. The legacy
Lua customization path remains available to ordinary Morrowind armor.

The regression attaches a real Lua Combat interface that misclassifies the
native item, checks native heavy classification/condition-aware rating and the
actual tooltip text, and verifies that a legacy item still uses the override.
It covers shifted content indices and explicit incoming-store queries.
`live-fix-errors-before.log` reproduces the wrong classification before repair.

Both error fixes together pass **19 focused tests**, then the complete engine
inventory **2,299/2,299**, exact inventories with no failures/skips:
`checks-2c5c231eeeba4d169f4b3627ab7c1e90` / `live-fix-errors-after-focused-02.log`
and `checks-1d7b8e0921be4d23addad7916c9b8f46` / `live-fix-errors-full.log`.
Combined tested fingerprint at HEAD `b9d69469098f3cd5d16c8c7eecb4410396dc622e`:
`242fda4e09bc0ef4a11b8ffee36658c966e1b809349aa20eba4fde010668e5c7`.
Component evidence above is reused; those component sources are unchanged.
The armor and skill changes are committed separately after this verification.
This section was added afterward; the verified C++ sources are unchanged.

## Native skill-award repair

Engine skill-use requests in the Oblivion profile now publish through the
existing native skill-training authority rather than enqueueing Morrowind Lua
facade writes. Running/swimming retain their fractional native awards and
tutorial skill-use tracking, without a second once-per-second award. Morrowind
skill events retain their existing route.

The before-fix regression shows a missing native level/experience award. The
fixed regression checks earned levels, fractional experience, counters,
serialization, repeated delayed-action draining and rejected invalid input.
The full/filtered evidence for the combined fixes is recorded above.

Editable rendered probe:
`scripts/validate_windows_live_skill.py`, with the manifest and Lua controls in
`scripts/data/oblivion_compat/windows_live_skill/`. It uses a private profile,
checks real movement and earned Athletics, clean exit, unchanged executable and
input save, and the expected presence/absence of the reported errors. This is
automated Lua control, not normal human input or a visual loot acceptance course.

- `build-windows/live-skill-control-02/result.json`: original executable,
  six simulated seconds of movement, **5 rejected skill-update errors**.
- `build-windows/live-skill-fixed-02/result.json`: fixed executable, six
  simulated seconds of movement, **0 skill/focus/frame errors**, earned native
  Athletics and clean exit. Both preserve the pristine input save hash above.
- Both runs advanced Athletics at approximately 0.01008 normalized progress per
  simulated second; the repair does not double the existing movement award.
- Fixed executable SHA256:
  `d6eaba90c90503405d7a57d838a65bcadc2e116b092c3e5cdb28271b22292b30`.
  Build log: `build-windows/live-fix-errors-runtime-build-02.log`.
- First rendered probe retained a failure because normal input overwrote its
  driver. The corrected fixture uses the supported Controls interface override;
  movement/experience assertions were retained. The first focused build also
  caught a pointer-wrapper conversion error, corrected before the passing run.

Reproduce with a new output directory:

```powershell
python scripts/validate_windows_live_skill.py --save PATH_TO_SAVE --output build-windows/live-skill-new
```

Startup missing-GMST/script compatibility warnings are separate existing issues;
these receipts do not claim an entirely warning-free game or sanitizer coverage.
