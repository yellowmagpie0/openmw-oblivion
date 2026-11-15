# Windows live-session fixes — 2026-11-15

## Current handoff

Target: responsive tutorial play using the player's existing saved game, with
working armor UI and native skill progression. The bounded routing/reference
lookup performance chunk and both recurring error repairs are verified. The
saved game remains slow; this is not a completed performance course. Current
chunk: remove repeated full-world effect-owner scans while preserving live
casts, expiration, save/load and runtime ownership. Broader tutorial gates remain in
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
