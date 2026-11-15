# Windows live-session fixes — 2026-11-15

## Current handoff

Target: responsive tutorial play using the player's existing saved game, with
working armor UI and native skill progression. The bounded routing/reference
lookup performance chunk is verified. The saved game remains slow; this is not
a completed performance course. Next: repair the recurring armor-interface and
legacy skill-award errors, then address the remaining saved-world effect scans
and measured CPU work. Broader tutorial gates remain in
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
