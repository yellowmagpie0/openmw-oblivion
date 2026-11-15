# Windows merge and performance handoff — 2026-11-14

## Current result and remaining limits

Merged GitHub `origin/master` at `c8cd395f9e` into
`codex/windows-native-dev`, merge commit `0d0f09d3d6`. The conflict in native
combat retains both upstream spell-cast cleanup and the Windows-safe action
swap. The pre-existing removal of two GetItemCount trace statements was preserved.
Nothing was pushed. The performance changes remain in the working tree.

All configured Windows targets build. The bounded performance chunk is complete:
a parentless enable-query optimization is implemented, regression-tested and
measured against a fresh control build. Observed FPS gains are approximately
9–23%, with mechanics time reduced by 10–19%. The optimized source and executable
are restored as the final working state; the final prison confirmation reached
31.6 FPS and exited cleanly.

Smart App Control initially blocked the game with WinError 4551 and Code
Integrity events 3033/3077/3118. Execution succeeded after the user resolved the
block and requested a retry. The agent did not change Windows security settings.

Startup/load improvement is not established. The city remains slow at about
26 FPS, and long-frame spikes remain. Old Bridge has a pre-existing startup
frame-error problem in both builds, so that location's timing is provisional.
Offscreen package execution and active-actor mechanics remain candidates for a
separate performance chunk. Traversal/combat, visual acceptance and cross-platform
gates remain outside these stationary Windows probes.

## Fresh controlled comparison after retry

`optimized-02` ran two samples per location, followed by `control-02` with the
pre-optimization enable-query implementation restored temporarily. The runner,
CMake cache, revision, data path, random seed, warmup and sample settings matched.
The comparison verifies the metadata, clean exits and raw frame sample counts.
FPS and quantiles below pool the two twenty-second samples at each location.
Startup columns show mean process-to-buffered-gameplay-statistics latency.

| Location | Control FPS | Optimized FPS | Gain | p95 ms, control → optimized | Startup s, control → optimized |
| --- | ---: | ---: | ---: | ---: | ---: |
| Imperial prison | 27.9 | 30.9 | 10.9% | 59.9 → 56.9 | 17.9 → 18.7 |
| Market District | 23.7 | 26.0 | 9.5% | 67.6 → 63.3 | 19.4 → 19.7 |
| Old Bridge (provisional) | 32.3 | 35.4 | 9.4% | 55.5 → 53.4 | 22.3 → 22.0 |
| Goblin Jim's Cave | 27.8 | 34.1 | 22.8% | 61.7 → 55.0 | 16.9 → 16.6 |

Mean mechanics time fell from 26.4 to 22.9 ms in the prison, 32.7 to 29.3 ms
in the city, 21.7 to 19.0 ms at the bridge, and 22.7 to 18.5 ms in the cave.
Old Bridge's p99 remained approximately 76 ms; the change does not eliminate
stutter. These are short, sequential runs, not guarantees for other viewpoints,
power conditions, content, combat or travel.

Log audit found 5–6 `pointer replacement has a duplicate reference identity`
startup frame errors per Old Bridge process in the original baseline, fresh
control and optimized runs. No frame-error messages occurred in the other three
locations' control/optimized logs. The bridge issue is an adjacent correctness
discovery, not an optimization regression or a closed gameplay gate.

## Baseline runtime measurements

Native MSVC RelWithDebInfo, Intel(R) Graphics, 1280x720 windowed, sound enabled,
VSync and frame cap disabled. Four stationary inspection starts, two fresh
processes per location, fixed random seed, five-second warmup and twenty-second
samples. Ranges below show the two observations, not confidence intervals.

| Location | FPS | p95 frame time (ms) | Startup latency (s) |
| --- | ---: | ---: | ---: |
| Imperial prison | 22.6–28.5 | 58.2–74.6 | 18.4–28.0 |
| Imperial City Market District | 21.0–22.0 | 70.4–74.4 | 20.1–22.1 |
| Old Bridge | 26.9–29.6 | 55.7–60.7 | 24.3–27.2 |
| Goblin Jim's Cave | 24.6–25.0 | 65.8–68.5 | 17.5–18.1 |

Startup is process launch to the first observed buffered gameplay statistics,
including the profiler's three-frame delay. Filesystem caches were not flushed;
these are not cold-disk or door-transition loading times. Instrumentation adds
overhead. The run-to-run variation requires repeated comparisons.

All eight baseline processes exited cleanly. Logs retain existing missing GMST,
local-script warnings and Old Bridge startup frame errors; clean process exit
does not imply a warning-free runtime. Screenshot inspection was unavailable because the computer-use app
approval timed out. These probes do not establish visual or normal-input
gameplay acceptance, travel/combat performance, or Linux/sanitizer coverage.

## Diagnosis and bounded change

Mechanics consumed approximately 24–37 ms/frame in the baseline. Temporary
timers in a fresh prison process found only 12 active actors but 3,663 actors
in the offscreen AI pass. After its initial startup window, the pass consumed
about 17 ms/frame: approximately 3 ms for resident lookups, 6.7 ms for enable
queries, and 6–7 ms for package execution and associated work.

The change in `apps/openmw/mwworld/oblivionenablestate.cpp` handles authored
parentless ACHR/ACRE/REFR queries directly using the record's resolved runtime
ID. It avoids rebuilding a load-order resolver and allocating a cycle-detection
set for these roots. Resident flags still override saved flags, which override
authored flags; all are read on every query. Stale borrowed indexes are still
rejected. Parented, unknown and dynamic references retain the existing traversal.
No AI cadence, deadline, save schema or package behavior was intentionally changed.
Temporary timing code was removed; its patches and traces are retained locally.

## Build and test evidence

- Merged baseline: all configured targets build; **3,303 component tests** and
  **2,294 engine tests** pass, exact inventory agreement, no failures/skips.
  Evidence: `build-windows/checks-d84044a5a47a445a80d6a7c50e4ded24`.
  Source fingerprint: `c43edcb36f826de5cb4c79ecbba789c41ee67410b6de461f124bcd130c3e72c5`.
- Portable Windows Python selection: **204 tests** pass. This is not the full
  Linux-oriented Python gate. Log: `build-windows/performance-python-tests-unrestricted.log`.
- Optimized source: **25 focused tests** pass, followed by **all 2,295 engine
  tests**, exact inventories, no failures/skips and unchanged before/after
  source fingerprints. All configured targets build. Component/Python evidence
  above is reused with its original provenance; those sources were not changed.
  Focused evidence: `build-windows/checks-a93e609701894431be8071f12e12ad01`.
  Full engine evidence: `build-windows/checks-f0960d42e0ad447eac64ef4cb14da59d`.
  Tested fingerprint: `b99c79be31d6f4cc5d1648624a1280394ffd1a15d4ea35142626c882e5fcc3ea`.
  This handoff was added afterward; final C++ sources are unchanged from that run.
- After the control experiment, the optimized source was restored byte-for-byte
  (SHA256 `acbff0c6f440366a4b5530c27ac1c5fe16f82f00604c970b931492681911f974`).
  Its timestamp was refreshed so Ninja actually recompiled it, then all configured
  targets rebuilt and all **25 focused tests** passed again. Evidence:
  `build-windows/checks-200e2b6ff18442f3bf899e02a3439869`, fingerprint
  `d80dfe328d6a064cf41806d8a9f1118faac098ecc7abcb1f60b0bea9874ff8cc`.
  Final build log: `build-windows/performance-restored-build-02.log`.
  This report was updated afterward; no C++ changes followed these checks.
- Build logs retain MSVC conversion and existing declaration warnings. No
  warning-free build or ASan/UBSan run is claimed.

Local evidence under `build-windows/performance/`:

- `baseline-01/results.json`: raw run summaries, revision, diff, cache and runner
  hashes, profiles, logs and OSG traces. Baseline executable SHA256:
  `39d83923d270921ed00f0b3c4f629c1e114b47b506b6a60afac6cbe373a34305`.
- `mechanics-diagnostic-01` and `low-diagnostic-01`: isolated temporary-timer
  probes; their executable identities and diffs are recorded separately.
- `diagnostic-actors.patch` and `diagnostic-low.patch`: removed instrumentation.
- `optimized-01`: failed launch only; **no optimized FPS result**. Launch log:
  `build-windows/performance-optimized-01.log`. Windows policy diagnostics:
  `windows-control-block.json`. Blocked executable SHA256:
  `12b51b70bab5047fc6604541ad97cee104bb716374c352f8793fd6acd355734b`.
- `optimized-02` and `control-02`: eight clean benchmark processes each, after
  execution was permitted. `comparison.json` pools their raw traces;
  `compare.py` retains the aggregation and metadata/sample-count checks.
  Control executable SHA256:
  `97749751ed08efc964e9e30a0f78073c8c3c9cb2be0d036f5a45881b757a5957`.
- `restored-optimized-03`: final restored executable's clean prison run,
  31.6 FPS, p95 56.3 ms, mean mechanics 22.4 ms, startup 18.3 s.
  Final executable SHA256:
  `ec346a2725d5e5eac846a337bcdfe48efc563a94e35d29532f3f9f8d33863602`.

Runner usage and measurement limitations are documented in
[`scripts/HOWTO-benchmark.md`](../scripts/HOWTO-benchmark.md).
