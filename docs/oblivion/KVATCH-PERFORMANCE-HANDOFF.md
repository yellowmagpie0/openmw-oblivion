# Kvatch performance checkpoint — 2026-11-16

## Current handoff

Target: responsive play in the first Oblivion plane outside Kvatch. The identified
offscreen NPC routing loop and Windows legacy geometry submission bottleneck are
repaired. The matched plane replay now reaches **20.96–20.97 FPS**, with drawing
around **9.5–9.7 ms/frame**, down from 57 ms. Overall responsiveness remains open:
mechanics is still 21.7–21.8 ms and physics about 10 ms. Next: profile the
remaining low-process scheduler and native-physics CPU cost with vertex buffers
enabled; do not describe this stationary replay as smooth gameplay acceptance.
Broader gameplay acceptance remains in [the tutorial handoff](TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md).

GitHub's default branch is `master` (there is no `main` branch). It was fetched
and fast-forwarded into `codex/windows-native-dev` at
`7f4c95fba1ea9d1c12c3206898ee7533fb6063e5` before this repair. Changes remain local.

## Repair

CELL-only door planning invented through-routes between disconnected parts of a
room. Live traces show a guard repeatedly entering/leaving Dynari Amnis' house
while trying to reach the Southwest Watch Tower. The planner now searches strongly
connected regions of enabled native pathgrid nodes, directed walking links and
the actual XTEL landing nodes. It checks current door availability, locks, keys
and ownership per query, then builds the normal pathgrid approach to the chosen
door. Equal-hop exits across cells compete by actual approach cost.

Topology is shared between plans and invalidated on overlays, graph replacement
and runtime restoration. Replacing a winning pathgrid now advances its generation,
including replacements with the same key/node count. No AI cadence, script
execution, collision checks or saved-state fields were removed.

The regression covers reciprocal false routes through disconnected rooms,
forward/reverse/bidirectional links, shifted content indices, save/reload and
topology replacement. Existing ownership, lock, disabled-door, street-link and
handoff tests also pass.

## Matched runtime evidence

MSVC RelWithDebInfo, Intel Graphics, 1280×720, sound on, uncapped/no VSync.
Both builds used the player's graphics settings, including FOV 75 and the water
shader. Two independent loads per build; 10-second warmup and 30-second samples;
no simultaneous builds or game instances. All four processes exited cleanly.

| Plane-entry replay | Merged control | Routing repair |
| --- | ---: | ---: |
| FPS | 1.106 / 1.121 | 13.102 / 13.583 |
| Mechanics ms/frame | 854.98 / 841.50 | 31.35 / 29.42 |
| Frame p95 ms | 1727.25 / 1642.50 | 96.22 / 90.56 |
| Drawing ms/frame | 63.98 / 67.58 | 57.88 / 57.06 |
| Guard `016416` door transitions per run | 18 / 18 | 1 / 1 |

The last manual save is at Weynon Priory, not in the observed live plane session.
The private plane fixture preserves all 3,683 AI states, relocates the player to
the original plane entrance and retains only the relocated player's movement
snapshot to avoid cross-world movement footprints. It is a staged diagnostic
fixture, not an organically saved Kvatch course. A separate clean load of the
unchanged original manual save reaches 27.75 FPS with the repaired executable.
The original save's SHA256 remains
`d55ccdbf3757456664d47355fdb377f18e6f05cc80a27ea62aeee0055f6c3f34`.

Evidence under `build-windows/`:

- `performance/kvatch-performance-verification.json`: hashes, inputs, complete
  timing results, fixture recipe and test/source identities.
- `performance/kvatch-plane-{control,fixed}-user-01/`: accepted matched runs.
- `performance/kvatch-existing-save-fixed-01/`: original-save load check.
- `checks-600cc03441304fe88813942578413274/`: full engine **2,311/2,311** and
  components **3,308/3,308**, exact inventories, zero failures/skips.
  Tested source fingerprint:
  `a1018bdfaec92cdda744e2b727db43abd3a14897942bc247f603b07d03f91c43`.
- `kvatch-regions-focused-03.log`: **57/57** focused door/AI tests.
- Game SHA256:
  `01d612ec75f931f5e804893a53d3f4e81bb888cab4490edaed4e521fe1cee41b`.

The AI documentation was added after its verification; tested AI C++ is unchanged.
Unrelated recording-tool files created during the task were preserved. OBS was
blocked at startup, rather than actively recording; it was left untouched.

## Rendering follow-up

The Intel OpenGL draw thread spent most of its time submitting legacy display
lists. A private `OSG_VERTEX_BUFFER_HINT=VERTEX_BUFFER_OBJECT` experiment reduced
drawing from about 55–58 ms to 9.6 ms, without changing scene content. The Windows
`RuntimeViewer` now chooses vertex buffers before context realization when no
backend preference was supplied. Explicit environment and VBO/VAO choices remain
authoritative; other platforms retain their defaults.

Native scene traversal also revalidated the entire rig repeatedly between internal
operations. It now avoids duplicate checks and checks on nodes that dispatched no
controllers. Callback mutation, ownership, placement and traversal ordering checks
remain. A regression verifies collision/flag/projection callbacks cannot detach
children and continue into their controllers. Existing traversal tests also pass.

Same staged save, settings, seed, sampling and process isolation as above:

| Replay | Routing repair / legacy drawing | Default VBO | VBO + traversal | All repairs |
| --- | ---: | ---: | ---: | ---: |
| FPS | 13.10 / 13.58 | 18.83 / 18.66 | 19.35 / 19.54 | 20.96 / 20.97 |
| Drawing ms/frame | 57.88 / 57.06 | 9.65 / 9.87 | 9.99 / 9.86 | 9.68 / 9.49 |
| Mechanics ms/frame | 31.35 / 29.42 | 24.92 / 24.99 | 24.97 / 24.68 | 21.77 / 21.70 |
| Physics ms/frame | 12.96 / 12.82 | 11.98 / 12.23 | 10.27 / 10.17 | 9.97 / 9.96 |
| Frame p95 ms | 96.22 / 90.56 | 64.16 / 65.13 | 63.08 / 61.78 | 58.27 / 58.05 |

The final two runtime processes exited cleanly without load/frame failures. The
renderer change does not reduce resolution, view distance, effects, NPC count or
AI cadence. A real OpenGL regression compares the complete 32×32 framebuffer for
display-list and VBO geometry and checks that a GPU buffer was created. This is a
small geometry equivalence check, not a pixel comparison of the entire game scene.

Evidence under `build-windows/`:

- `performance/kvatch-render-default-vbo-01/`: default-policy comparison (two loads).
- `performance/kvatch-render-traversal-01/`: combined repair (two loads).
- `performance/kvatch-render-cpu-fixed-01/`: final rendering + identity/lookup
  repairs (two loads, no VBO environment override).
- `performance/kvatch-render-performance-verification.json`: final executable,
  input and test identities, complete timing receipts and preserved startup errors.
- `performance/kvatch-render-vbo-01/`: initial buffer experiment.
- `render-vbo-focused-01.log`: six focused viewer/buffer tests.
- `render-traversal-focused-02.log`: 64 scene traversal tests, exact inventory,
  zero failures/skips. Its predecessor retains a fixture failure: successive
  projection owners required explicit retirement between cases; corrected.
- `render_stack_15748_*.json`: sampled draw/driver/main-thread stacks, retained
  as diagnostics only; instrumented runs are excluded from matched timings.

Final integration: `checks-e44bf61750ea43c8b0b5d425c0f861ef/`, **2,313 engine +
3,311 component tests**, exact inventories, zero failures/skips. Source fingerprint
`9af2119da4ab9815d3e3c149f6cdb45cbf91c3741a40e489abf4019749503dcb`.
Final game SHA256:
`a8b83be54657c9b66ad850a7283ace03dc3171562d5d44eb31dc4878f403c53d`.
This evidence paragraph was added after verification; production/test C++ and the
measured executable are unchanged. The original save and graphics settings hashes
were rechecked and match the original comparison inputs. Final runs retain exactly
the same 43 missing-GMST startup errors and no additional error-level messages.

## Remaining gaps and retained failures

### Additional CPU overhead repair

With VBO enabled, a fresh 40-sample main-thread profile found plugin-name
normalization in 7 samples and low-process AI in 16. Simple ASCII basenames now
skip `std::filesystem::path`'s Windows UTF-16 conversion round trip; paths,
drive-prefixed names, control characters and non-ASCII names retain the original
filesystem path. Canonical `FormKey` lookup tries the normalized load-order map
directly, with the original normalization fallback for externally assembled keys.
Disposition queries now reuse the existing saved-reference offset lookup rather
than linearly scanning all reference snapshots. The lookup validates every hit
against the current key and reads current values; no gameplay values are cached.

`plugin-name-focused-01.log` passes all 19 FormKey tests, including differential
checks against the platform filename parser and shifted load-order resolution.
`runtime-identity-focused-01.log` passes 16 engine and 35 component tests for
disposition, fight scores, saved-reference lookup and identity. The sampled replay
`performance/kvatch-vbo-cpu-diagnostic-01/` is instrumented and excluded from FPS
comparisons; stacks are in `render_stack_8656_*.json`.

Mechanics and physics now limit overall FPS. The original low-resolution GPU
counter reports did not establish GPU hardware saturation.
`Water::updateVisible` enables reflection rendering for visible native water
without consulting `WATR::Flag_Reflective`. The installed lava record
`OblivionLavaTest01` (`039d98`) has flags `01`, whereas DefaultWater has `02`.
Private water-shader-off, GL-error-checking-off and transparent-postpass-off
experiments reached only 13.88, 13.95 and 14.14 FPS respectively, with drawing
still 54–56 ms. Those hypotheses did not explain the large submission cost;
no reflection or water-appearance change is included here. Receipts are in
`performance/kvatch-render-{water-off,gl-check-off,postpass-off}-01/`.

These are stationary automated replays, not normal-input traversal/combat or a
smooth-gameplay acceptance course. Both builds retain the same 43 startup GMST
errors. No Windows sanitizer configuration was available. Earlier evidence is
retained in [the Windows history](WINDOWS-LIVE-RUNTIME-FIXES.md).

Retained unsuccessful attempts include the new regression's incorrect record
order, a rejected cross-world movement fixture (its menu FPS is excluded), and
an interrupted incremental link at a 2 GB ILK. A full link succeeded; the local
CMake cache now uses `/INCREMENTAL:NO`. Exact failure paths are in the receipt.
