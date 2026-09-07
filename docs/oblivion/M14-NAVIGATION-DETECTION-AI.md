# M14 navigation, detection, and AI packages

Status: **in progress**. Commit
`d9c68c8ed69bd34f1c35fc7aa7837832ea6e56a9` is a candidate implementation,
not an accepted milestone. It was delivered as one aggregate change based on
`a9acf26cc1e94c97c6ef4c0c53c880b08f82845f`, without the required phase
checkpoints.

The earlier acceptance declaration was withdrawn after review of the plan,
changes, and textual evidence. The existing audit, scenario, save, and test
artifacts are retained as diagnostic inputs, but must not be cited as M14
acceptance evidence without satisfying the reopened gates below.

## Reopened acceptance gates

- Migrate every scenario to the actor-scoped event and saved-state requirements
  now supported by the harness, then add actor-specific comparisons for package
  order, route, cell transitions, arrival, and recovery. The companion manifest
  now scopes selection/phase/route, persisted targets and group relationships,
  destinations, interruptions, stalled phases, and near/separated/near distance
  outcomes. The city manifest now scopes all nine named NPCs and rejects their
  route failures, locked-door failures, and excessive reselection. The door
  manifest requires its named actor to route, encounter a bounded obstruction,
  recover, and perform a door transition without excessive reselection or a
  missing navmesh. It also compares the named actor across open, obstructed,
  and recovered checkpoints, requiring route-generation changes and recovery
  movement; the recorded checkpoints have neither. The detection manifest now
  scopes outcomes to the named player/Valen Dreth pair and requires both
  occluded non-detection and visible positive detection; the recorded evidence
  contains only the former. The horse
  manifest now requires exact player/horse mount, reciprocal rider, and dismount
  events plus reciprocal saved-state relationships at both checkpoints.
  The tutorial manifest now scopes Uriel Septim, Baurus, Glenroy, and Captain
  Renault and requires each to leave `CGBladesWaitToMove`, select the correct
  first movement package, route, and cross a door without routing failures.
  Its checkpoint also requires the four exact actor/base pairs to have left the
  wait package with active destinations and no terminal interruption. The
  course now quickloads that save, writes a post-load save, requires a logged
  load, and repeats the four named-state checks. The recorded run predates this
  boundary and does none of the required escort work; exact target checks and
  the remaining contextual reload sequence are still open.
- Finish companion recovery. Earlier stationary/repath-exhausted evidence is
  superseded by `m14-companion-live-target/`: the actors move and cross doors,
  but the final 863.54-unit gap still fails the 512-unit limit. Obstruction
  causality, formation/cycles, and transition reload coverage remain open.
- Complete the city matrix. `m14-city-logical-position/` passes the current
  named-actor and calendar checks, but does not establish high/low equivalence
  or reload continuation at every required clock boundary. The population
  stream still has 640 blocked-route occurrences within its explicit budget;
  a passing fixture is not evidence that every ambient route is repaired.
- Replace the tutorial smoke run with assertions for the real Emperor/Blades
  actors, declared packages, targets, door sequence, arrival, and reload
  continuation.
- Expand the door, detection, horse, and Morrowind runs to the complete matrices
  in the implementation plan. The current runs cover only small smoke subsets.
- Add the engine integration coverage required by section 15.2 of the plan.
  M14 engine-target tests now cover rule helpers, moving-target baselines,
  binary persistence, gait selection, and logical-position retention. They
  still do not establish the actor/world/navigation/inventory/script/companion/
  mount integration coverage claimed by the former report.
- Produce durable full-build, full-test, ASan, and UBSan logs from a clean tree
  at the final implementation revision.
- Restore phase-sized implementation commits for subsequent repair work and use
  a separate final acceptance commit after every definition-of-done item passes.

M14's candidate promotes native TES4 navigation and AI data that was previously
parsed but discarded into a profile-owned runtime. Oblivion NPCs and
creatures now share the existing M11 render/animation and M12/M13 actor state,
while a stable-keyed coordinator owns package selection, schedule evaluation,
path execution, process tiers, detection, doors, companions, and horses.

## Candidate implementation present

- `PACK` and `PGRD` are native stable-keyed stores. `CTDA` and TES4 `CTDT`
  conditions share one typed representation; `PTDT` location/target typing and
  FormID adjustment are corrected. Pathgrid point bytes are preserved, `PGAG`
  is retained and audited, PGRR/PGRI/PGRL indices are validated, and pathgrid
  ownership comes from winning cell metadata rather than file order.
- `ESM4::PathgridService` exposes deterministic nearest-node, component, route,
  foreign-link, object-link, overlay, generation, and navigator-adapter APIs.
  Interior/local and exterior/world-space coordinates are handled at the
  adapter boundary, with no fake TES3 pathgrid inserted into the store.
- TES4 NPC and creature classes expose native actor stats, movement, collision,
  inventory/equipment, factions, capabilities, and stable runtime identity.
  The `OblivionAiService` is the sole package/phase authority; TES4 actors do
  not create a concurrent TES3 `AiSequence`.
- Ordered schedule/condition selection, transient script packages, fixed-step
  high processing, deterministic low processing, route recovery, restraint,
  and version-5 persistence are implemented. Low-process movement retains
  waypoints and crosses only resolved door edges; it never substitutes a final
  destination teleport.
- Wander, Travel, Find, Follow, Accompany, Escort, Eat, Sleep, Use Item At,
  Flee Not Combat, Pursue, dialogue approach, companion grouping, and horse
  mount/dismount state are implemented. The five observed `CastMagic` records
  stop at the explicit `waiting-for-M16-action` boundary, and `Ambush` stops at
  `ready-for-M15-combat`; neither reports unavailable combat or magic effects
  as completed.
- M14 ObScript commands and queries use the same native service: package
  evaluation and insertion, current package/procedure, path-point overlays,
  LOS/detection, flee, restraint, and horse state. Invalid native targets are
  typed runtime errors, not silent no-ops or deferred placeholders.
- The scenario harness provides actor observation, clock control, obstruction
  control, navigation debug capture, structured AI events, bounded progress
  checks, and forbidden-action checks. It does not move actors or force package
  success after initial setup.

## Official-content audit

The canonical profile is the eleven installed official files listed by
`m14-audit`: `Oblivion.esm`, the local 85-byte `DLCShiveringIsles.esp` stub,
and the nine installed DLC/Knights files. The count lock is
[`oblivion_m14_data_counts.json`](../../scripts/data/oblivion_compat/oblivion_m14_data_counts.json).
The provisional audit artifact is
[`m14-audit.json`](../../build/oblivion-compat/m14-audit-final6/m14-audit.json)
with the inspectable HTML report beside it. The lossless parser census is
[`baseline.json`](../../build/oblivion-compat/m14-lossless-final3/baseline.json):
11/11 installed plugins and 17/17 archives were enumerated, with zero
unsupported record families and zero unallowlisted semantic skips.

| Audit value | Result |
| --- | ---: |
| Winning records | 1,187,866 |
| Winning `PACK` / `PGRD` / `CELL` | 7,668 / 8,288 / 35,565 |
| Actor records / unresolved package lists | 3,636 / 0 |
| Conditions / invalid references | 6,830 / 0 |
| Pathgrid nodes | 675,161 |
| Local / foreign pathgrid edges | 2,915,191 / 291,637 |
| Resolved `PGRL` links / linked nodes | 510 / 1,577 |
| Linked base types | `ACTI` 405, `DOOR` 99, `STAT` 6 |
| PGAG bytes | 88,046 |
| Ambiguous pathgrid cells | 0 |

The package fingerprint is
`sha256:04b4e35cd39d55e277084afd18b06bb70477f9a7a36c12b654201d69cec481c9`;
the pathgrid fingerprint is
`sha256:19780f08b7491950df0eb6f4b32e7b91634d636cc3a440986926a354f2d45883`.
Observed package types are Accompany 40, Ambush 90, CastMagic 5, Eat 855,
Escort 75, Find 768, FleeNotCombat 11, Follow 230, Sleep 762, Travel 2,132,
UseItemAt 819, and Wander 1,881. The 180 actors without packages are explicit
no-package records, not unresolved package references. The 27,277 cells without
a usable graph are report-only cells with no usable pathgrid; no winning cell
has an ambiguous graph.

Three reserved package flag bits (`0x00000800`, `0x00004000`, and
`0x00008000`) and one stock out-of-range Find target are retained in the audit
as reviewed data, with no unreviewed fallback. All reachable condition
functions and run-on contexts are supported (`0:Subject` accounts for all
6,830 audited conditions).

## Provisional verification record

The aggregate implementation recorded the following commands as passing. These
results establish a buildable baseline, not milestone acceptance:

```sh
cmake --build build --target esmtool components-tests openmw-tests openmw -j2
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s scripts/tests -p 'test_*.py'
git diff --check
```

| Gate | Result |
| --- | ---: |
| Component GoogleTest inventory | 1,580 / 1,580 |
| Engine GoogleTest inventory | 516 / 516 |
| Python compatibility/state tests | 40 / 40 |
| Focused M14 component tests | 50 / 50 |
| Focused M14 engine tests | 5 / 5 |
| Full CTest targets | 2 / 2 |
| Sanitized component suite | 1,580 / 1,580 |
| Sanitized engine suite | 516 / 516 |
| Count-lock audit | 1 / 1 |
| Lossless official-content census | 11 / 11 plugins; 17 / 17 archives |

The focused suites cover binary PACK/PGRD fixtures, malformed data, stable
overrides/deletions, schedules and conditions, detection vectors, pathgrid
graphs/foreign links/object links/overlays, phase machines, persistence
migration/validation, actor bridges, process tiers, doors, companions, and
mount reciprocity. The Python verifier covers schema 5, v1--v4 migration,
corruption, content identity, chronology, routes, overlays, detection, and
mount/companion relationships.

## Provisional real-content runs

All six Oblivion M14 scenario commands and the Morrowind isolation command
completed according to the current harness. Review found that the harness did
not assert the required scenario outcomes, and some passing streams contain
stalls and repeated route failures. These directories are therefore diagnostic
evidence only:

| Scenario | Evidence |
| --- | --- |
| Tutorial escort | `build/oblivion-compat/m14-scenario-tutorial-final5/` |
| 24-hour Imperial City population | `build/oblivion-compat/m14-scenario-city-city6/` |
| Doors and obstruction recovery | `build/oblivion-compat/m14-scenario-doors-final3/` |
| Companion travel | `build/oblivion-compat/m14-scenario-companion-final3/` |
| Detection matrix | `build/oblivion-compat/m14-scenario-detection-final5/` |
| Horses | `build/oblivion-compat/m14-scenario-horse-final21/` |
| Morrowind isolation | `build/oblivion-compat/morrowind-m14-final3/` |

A focused navigation repair was verified in
`build/oblivion-compat/m14-navmesh-agent-bounds-fix/`. Multipart TES4 NPC
models no longer register zero-sized agents, convex and multi-sphere collision
input no longer aborts navmesh tile jobs, and Valen Dreth produced 53 successful
continuous routes with no `navmesh is not found` event. The scenario remains a
failure because its separate obstruction invalidation, door-transition, and
package-reselection gates are still open.

The scheduler repair in `build/oblivion-compat/m14-scheduler-fix/` reduced the
door-course event stream from 229,223 to 22,718 events and Valen Dreth from 54
selections to one. Terminal packages now retain their completed or interrupted
state while the same schedule winner remains active, and retry only after a
real pathgrid generation change. The course still fails its independent
obstruction and door-transition requirements.

The city run records five calendar checkpoints, but does not prove the required
named-NPC histories, high/low equivalence, or reload boundaries. The companion
run does not demonstrate separation and recovery. The tutorial, door,
detection, horse, and Morrowind runs cover only subsets of their planned
matrices. Historical M7, M12, and M13 runs remain useful regression signals but
cannot substitute for the open M14 gates.

The previous report recorded visual review of the tutorial interior, city, door
course, horse mount, detection scene, companion scene, and Morrowind frame.
Those captures remain useful for later comparison, but they do not prove the
missing behavioral and persistence gates. The existing magenta fallback
HUD/resource markers visible in the captures are also present in accepted M13
captures and are not attributed to the M14 candidate.

## Boundaries and known limitations

### Diagnostic accounting repair (2026-09-05)

Repeated failure events were sampled by the engine, but the harness counted
lines rather than occurrences. Failure budgets now include cumulative samples
and closing summary totals without double counting. Clear, restore, and
shutdown flush their counters; final verification rejects missing or
inconsistent summaries. Aggregate counts cannot satisfy ordered-action gates.
Regression tests cover suppressed repeats, actor isolation, reload epochs,
legacy summaries, malformed counters, and incomplete streams.

Recounting `m14-city-final` yields 885 route-blocked occurrences, below that
manifest's explicit 1,000 budget; its event checks still pass. The historical
`m14-tutorial-named-save` stream has inconsistent totals across quickload and
must be regenerated. Its save checkpoints prove Autosave/Quicksave discovery
and native state parsing, not escort completion: the four actors remain in
`CGBladesWaitToMove`. The uncommitted `SetStage` experiment was withdrawn
because skipping dialogue is not a permitted dialogue-independent wait control.
No tutorial acceptance gate has been closed.

Fresh verification in `build/oblivion-compat/m14-tutorial-counter-repair/`
confirms valid totals across quickload: 23,477 occurrences in 23,369 lines,
including 540 route-blocked occurrences against the unchanged 500 budget.
The scenario exits normally with no unreviewed error logs but fails that
budget and the four escorts' package/door progression checks. Both saves and
the quickload are logged; counter integrity is fixed, escort behavior is not.
The rebuilt engine/component suites pass 516/516 and 1,582/1,582 tests; Python
passes 62/62. Logs are `m14-counter-engine-tests.log`,
`m14-counter-component-tests.log`, and `m14-counter-test-build.log` under
`build/oblivion-compat/`. The count-locked content audit passes in
`m14-audit-counter-repair/`. Per-actor phase-repeat checks now remain effective
despite interleaved actors or diagnostic events.

M15 still owns combat, damage, crime, arrest, and mounted combat. M16 owns real
magic-effect execution; observed CastMagic packages expose the typed boundary
instead of faking a cast. M18 owns dialogue content/topic selection, and M19
owns the complete Oblivion UI. The local Shivering Isles file is an 85-byte
stub, so all current audit and runtime evidence is limited to the installed
canonical profile and does not cover expansion data that is not present.

### Moving-target destination repair (2026-09-05)

Loaded and unloaded followers previously updated the waiting destination every
tick, even below the 96-unit reroute threshold. Slow target movement therefore
never accumulated enough displacement to restart following. Both paths now
retain the last routed intent until cumulative movement or a cell change
requires another route. Shared-helper tests cover sub-threshold samples and
preservation through binary runtime-state serialization and tier changes.
The build and all 518 engine tests pass (`m14-follow-destination-build.log`,
`m14-follow-persistence-test-build.log`, `m14-follow-engine-tests.log`).

`m14-companion-destination-repair/` exits normally and passes its event gates,
but the full course still fails. Its fixed 80-second startup delay captures
the initial actors in Resolve without targets/destinations, and its final
member/leader distance is 6,801.61 with different cells. Synchronize the initial
checkpoint to real route events, then investigate the recovery failure without
relaxing the 512-unit distance requirement. This is not a companion acceptance
pass and does not establish full formation or process-tier equivalence.

The companion manifest now waits for all three named actors' first route
events instead of sleeping for 80 seconds from process launch. Its overall
220-second budget accommodates content startup and the unchanged course steps.
`m14-companion-observed-start/` passes the initial checkpoint and all event
checks, exits normally, and fails only final recovery: distance 7,366.22,
different cells. This isolates an actual follow/recovery problem from startup
timing. The 512-unit near-distance limit remains unchanged. Python: 62/62.

### Follow catch-up gait repair (2026-09-06)

Follow/Accompany now run at ordinary native speed when farther than package
spacing plus a 96-unit margin, or when following across a cell boundary. This
is an explicit runtime spacing policy, not an original-engine oracle result.
AlwaysSneak takes precedence and AlwaysRun is retained. All resident route
segments and unloaded movement use the same policy. Resident movement applies
the gait before querying native speed, preserving modified stats, inventory
weight, swimming, and immobilization checks. Player targets use live position
even if mount bookkeeping has created a cached AI entry.

Build, 520 engine tests, and 62 Python tests pass. Logs:
`m14-follow-gait-build.log` and `m14-follow-gait-engine-tests.log`.
`m14-companion-native-gait/` passes initial spacing (290.35), separation
(1,519.07), and event gates. Final recovery still fails: 1,923.29, same cell,
against the unchanged 512 limit. Inspect stale route targets and low-process
position authority next; no companion acceptance claim follows from this run.

### Low-process position authority repair (2026-09-06)

Resident low processing now resolves destinations, builds paths, and advances
movement from logical positions using the same routines as unloaded actors.
Resident native speed still supplies inventory/stat/immobilization constraints.
Logical progress, rather than a stationary rendered position, resets the
no-progress timer. Synchronization retains logical cell crossings while the
observed resident position/cell is unchanged, but recognizes external moves.
Cross-cell realization is queued for the post-update transition pass; promotion
validates the logical route position and rebuilds a resident Recast corridor.
The next fixed step resolves the newly registered Ptr after a cell transfer.

Build and 521 engine tests pass (`m14-low-position-build.log`,
`m14-low-position-engine-tests.log`); Python remains 62/62.
`m14-companion-logical-position/` has clean exit/error and event checks but
still fails final recovery at 1,919.13 units. That run predates the final
logical-progress timer adjustment. The moving-target route refresh and full
high/low/reload equivalence gates remain open.

`m14-city-logical-position/`, with the final timer adjustment, passes all
current checks: five calendar checkpoints, named-actor gates, save checks,
and clean exit with no unreviewed errors. Its occurrence totals are 30,915
events, including 640 route-blocked occurrences (1,000 allowed), 580 door
transitions, and 13 tier events. This does not replace the still-missing paired
high/low runs and reloads at every required schedule boundary.

### Active moving-target route refresh (2026-09-06)

Follow-like Path processing now refreshes a cumulatively displaced destination
while moving, rather than waiting to consume the old route. Resident high,
resident low, and unloaded paths use the same persisted displacement threshold.
Active boundary crossings finish before rerouting; target movement does not
reset no-progress/retry budgets. Resident target resolution respects dirty
logical positions/cells while player targets remain live.

Build and 521 engine tests pass (`m14-follow-live-target-build.log`,
`m14-follow-live-target-engine-tests.log`). The fresh native
`m14-companion-live-target/` run has clean exit/error and event checks, including
70 member target-route refreshes and a door transition for both member and
leader. Initial spacing is 328.81, separation is 742.34, and final recovery is
863.54 units in the same cell. Recovery still fails the unchanged 512-unit
limit, so companion acceptance remains open. Next investigate remaining
pathgrid/gait lag and prove obstruction causality, formation/cycle handling,
and the required high/low and save/reload transition matrix. This course is
runtime evidence for refresh activity, not exhaustive integration coverage.

### Named-actor adverse-event budgets (2026-09-06)

Named actors now default to zero door failures, action-commit failures,
low-process reconciliation failures, and bounded fast-forward failures, in
addition to the existing zero blocked-route budget. Deliberate adverse cases
must declare an explicit per-event allowance. Regression tests check each
event, suppressed occurrences in summaries, exact allowance boundaries, and
isolation from unrelated actors; all 63 Python tests pass
(`m14-adverse-budgets-tests.log`). Revalidating existing city-logical-position
and companion-logical-position streams passes the stricter event checks;
this is not a new runtime run or a companion distance pass. The tutorial
counter-repair stream still fails its previously recorded progression and
blocked-route checks. No acceptance gate is closed by this harness change.

### Quicksave no-game guard repair (2026-09-06)

The TES4 CharacterGen saving exception had moved profile/global queries ahead
of the running-game check. Quicksave now short-circuits those queries when no
game is running, retaining the TES3 CharacterGen restriction and TES4 exception.
Build and 521 engine tests pass (`m14-quicksave-guard-build.log`,
`m14-quicksave-guard-engine-tests.log`). `m14-morrowind-quicksave-guard/` passes
the native Balmora smoke course, ordinary quicksave, absence of TES4 save
markers, and clean exit/error checks. This is not a direct no-game quicksave
regression test or the full pre-M14 AI behavior comparison; those remain open.

### Reviewed content-gap count locks (2026-09-06)

The official-content lock now pins the previously report-only counts of 180
actors without packages and 27,277 cells without usable graphs. These are
content inventory counts, not permission to ignore a missing route for a
scenario actor. `m14-count-gap-lock/m14-audit.json` passes against the unchanged
official content fingerprints. Python coverage rejects growth in either count;
all 64 tests pass (`m14-count-gap-tests.log`).

### Harness controls and durable failures (2026-09-06)

`m14_console` is restricted to single-line player `coc <cell-editor-id>`
placement. Raw text controls in M14 manifests permit only `exit()`; quest and
actor mutations must not bypass the typed-control restrictions. This is a
manifest validation guard, not a security sandbox for arbitrary keyboard input.
Regression checks reject quest-stage forcing and newline/command injection at
validation and console execution boundaries.

Action exceptions now produce failed `scenario.json` evidence instead of
escaping without a report, and a clean process exit cannot pass a course with
unexecuted actions. All 67 Python tests pass (`m14-harness-failure-tests.log`),
including missing-save failure retention and clean early-exit rejection.

### Overlay invalidation scope and regression evidence (2026-09-06)

Single-node and linked-object overlays now share affected-route filtering,
including routes through foreign graphs. They clear stale continuous-corridor
cursors and door intent only for affected actors in Path. Waiting, acting,
door, and terminal phases are not forced into Path; unrelated routes and
no-progress/retry budgets are preserved. A focused helper regression covers
phase isolation and foreign-graph dependencies. Build and 522 engine tests
pass (`m14-overlay-scope-build.log`, `m14-overlay-scope-engine-tests.log`).

`m14-city-overlay-scope/` failed because its fixed startup delay attempted a
save before the game was ready. The old harness escaped without scenario.json;
the retained process log and `m14-city-overlay-scope.log` record that failure.
The city manifest now waits for named actor 01d15d's first native route event
and has a 260-second total budget. `m14-city-overlay-ready/` passes all current
checks and five calendar checkpoints, with clean exit/error gates. This is a
city regression, not a direct overlay integration test or the full paired
high/low/reload matrix.

ASan/UBSan components: 1,582/1,582 pass with leak detection and halt-on-UB
enabled (`m14-repair-sanitizer-build.log`, `m14-repair-sanitizer-tests.log`).
These are repair-stage component results, not final clean-tree engine/runtime
sanitizer acceptance. Full overlay lifecycle integration remains open.

### Authorized dialogue prerequisite: retained INFO data (2026-09-06)

The user authorized the minimum real dialogue prerequisite for the tutorial;
this does not accept or complete M18. INFO now retains all native conditions
and ordered responses instead of only the final entries, reads the native
three-byte flags, and retains the previous-INFO link. Stable condition keys
use the existing PACK decoder. Binary reader fixtures cover both condition
layouts, response order/notes isolation, flags, predecessor, and malformed
CTDT rejection. All 1,584 component tests pass
(`m14-dialogue-data-component-tests.log`). The initial build failed on an
incorrect test member name, corrected before this pass; its log is retained.
Runtime selection/result dispatch and tutorial verification remain open.

### Dialogue voice resources and legacy INFO layouts (2026-09-06)

Voice helpers now follow sex-specific native RACE.VNAM links, reject missing
or cyclic links, and require all numbered responses for the chosen INFO and
voice race/sex. Response numbers use the low byte of TRDT, retaining nonzero
padding in parsed data. Released Valen Dreth is a Dark Elf whose voice links
resolve to High Elf; the matching recordings are in that archive directory.
No arbitrary other-race or other-INFO fallback is used by these helpers.

The reader rejects wrong native condition/response/flag sizes but accepts
legacy two-byte DATA with absent flags. The first strict-DATA runtime probe
(`m14-tutorial-native-voice-race/`) failed loading native INFO 0253d4. A census
then identified it and 0253d3 as the base master's two legacy DATA records;
all 11 local official-profile plugins use the now-supported layouts
(`m14-dialogue-info-layout-census.log`). This is a layout census, not full
dialogue semantic acceptance.

All 1,588 component tests pass (`m14-dialogue-legacy-info-component-tests.log`),
including ordered voice matching, missing response/race/sex rejection, voice
link chains/cycles, native malformed layouts, and legacy DATA. The first
voice-helper build failed on a FormId null-check API mistake; corrected builds
and the original failure log are retained. Runtime tutorial verification is
still in progress.

### Conditioned scripted speech and real tutorial progression (2026-09-06)

`Say`/`SayTo` now evaluate INFO conditions through the shared AI condition
context, including the live player's TES3 mechanics proxy for race/sex. They
require the selected INFO's complete native voice sequence, return its total
decoded duration, and execute its result synchronously against the speaker.
This timing is required by Valen's native script, which checks the resulting
quest stage immediately after `SayTo`. Playback queues contain audio only;
reload clears them without replaying quest effects. Audio resume, SayOnce/random
selection, predecessor ordering and interactive dialogue remain M18 limitations.

The tutorial uses ordinary backward player movement to satisfy the released
taunt-marker distance condition, then waits for real dialogue/stage evidence;
it does not inject stages, packages or dialogue results. Retained intermediate
runs (`m14-tutorial-conditioned-say/`, `m14-tutorial-dialogue-prerequisite/`,
`m14-tutorial-player-dialogue-context/`) failed while exposing the missing
movement, player condition context and voice-race redirect respectively.

Latest `m14-tutorial-native-dialogue/` is still **FAIL**, with clean engine exit
0, all actions completed, no timeout and no unreviewed error findings. Native
speech advances CharacterGen 5 -> 6 -> 9 -> 10. Emperor, Glenroy and Renault
reach their initial destinations; both v5 save/reload checkpoints pass. Baurus
remains on 032b46 rather than required 032aeb; all four required door transitions
are absent. The 530 blocked/no-progress events exceed the unchanged 500 budget.
These failures remain work items, not waived assertions. Subsequent script and
package progression and the fixture's actual native door intent need inspection.

Verification: 522 engine tests and 67 Python tests pass
(`m14-dialogue-verified-engine-tests.log`, `m14-dialogue-verified-python-tests.log`),
alongside the 1,588 component tests above. This is repair-stage evidence; M14
remains in progress and final clean-revision/sanitizer verification is pending.

### Durable native package-completion events (2026-09-06)

Inspection found a further tutorial blocker: Renault's native
`OnPackageDone CGRenoteToMarkerA` advances CharacterGen to stage 12, but the AI
coordinator was not dispatching package-completion events. Recording a Complete
phase is not equivalent to executing the native callback.

Runtime schema 6 appends a typed pending-completion FIFO (stable actor/package
keys), with matching C++ and Python codecs. Version 5 remains readable without
inventing callbacks for old completed packages. Tests cover FIFO preservation,
removal of the current event before a callback save, repeated event identities,
malformed/truncated payload rejection and the older-version boundary. All
1,591 component tests, 522 engine tests and 68 Python tests pass in
`m14-package-event-schema-{components,engine,python}.log`. These codec tests do
not yet establish native callback delivery or tutorial acceptance.

### Native completion delivery and next tutorial failure (2026-09-07)

Both resident and unloaded phase transitions now enqueue `OnPackageDone` only
when entering Complete. The coordinator delivers the saved FIFO after actor
iteration (also after fast-forward), with the exact actor and completed PACK
as the event argument. Queue tests cover duplicate suppression, interruption
exclusion, callback saves, nested dispatch, callback-created events, reload
during delivery and exception cleanup. All 524 engine tests pass
(`m14-package-done-runtime-engine.log`). ASan/UBSan with leak detection also
passes all 1,591 component and 524 engine tests
(`m14-package-done-sanitizer-{components,engine}.log`).

`m14-tutorial-package-done/` still FAILS, but now proves native Renault
(032a15, base 02349f) completes CGRenoteToMarkerA (032ae9), invokes the matching
script block and advances CharacterGen to 12. Both schema-6 save checkpoints
pass. The next failure is unsupported `Look`: it aborts the stage-12 result
and later INFO 032b0d before its progression effects, producing repeated speech.
The Baurus/door and 530 > 500 blocked/no-progress failures remain. The run also
reports a binary/resources revision mismatch after interleaved build/commit
work; future runs must rebuild matching resources before launch. Engine exit
is 0, no timeout, and all harness actions completed, but error findings are
not clean. No milestone acceptance is claimed.

### Scripted Look and stage-13 progression (2026-09-07)

Native `Look`/`StopLook` now set/clear an explicit head-tracking override without
changing AI package targets, position or movement. The ordinary character
controller consumes it for native actors. Script-owned presentation intent is
stored as a canonical serialized FormKey in the existing reference custom-state
field `obscript.look_target`; C++ and Python reject malformed/noncanonical
values. Unavailable, disabled or remote targets do not force a cell load or
silently select a different head-tracking target. StopLook restores automatic
tracking. Unit coverage checks persistence and preservation of package state;
dedicated live StopLook/target-loss/head-pose/reload coverage remains open.

The first build failed on an incorrect variant type and a NotNullPtr cast;
both failures are retained in `m14-look-build-tests.log`. Corrected verification
passes 1,592 component tests, 524 engine tests and 69 Python tests
(`m14-look-verified-{components,engine}.log`, `m14-look-python-tests.log`).
`m14-tutorial-scripted-look/` advances natively through stage 12 to **13**, with
no Look diagnostic or resource-version mismatch. Both schema-6 checkpoints
pass; the final checkpoint records Glenroy and Emperor Look targets as the
stable player identity. This course did not save after issuing Look and then
reload that same intent, so it is not dedicated live Look-resume acceptance.

The scenario still **FAILS**: native INFO 00bf0d invokes unsupported `PickIdle`,
aborting before clearing `CharacterGen.uniqueIdle`. Baurus/escort door gates
remain unmet, and blocked/no-progress counts remain 530 against 500. Source
inspection also confirms stage 14 depends on `CGTriggerZoneCellScript`'s real
`OnTrigger player`, for which no native dispatch path exists. Released trigger
097a3b uses ACTI 097a3c / `TrigZone02.NIF`, which contains a
`bhkSimpleShapePhantom`. Implement actual idle selection/playback and phantom
overlap events; do not bypass them with stage writes or no-op commands. M14
remains in progress.

### Native condition variable identities (2026-09-07)

`GetQuestVariable` and `GetScriptVariable` now resolve native SLSD IDs through
SCVR names into VM locals, rather than treating IDs as source declaration
offsets. Released CharacterGen's `uniqueIdle` has native ID 11 but is not
source local 11. Resolution rejects missing/ambiguous identities without
changing the saved local layout. Sparse IDs, case, reordered declarations,
and duplicate IDs have regression coverage. All 1,593 component and 524 engine
tests pass (`m14-native-variable-{components,engine}.log`).

The unchanged native course `m14-tutorial-native-variables/` still **FAILS**:
it reaches stage 13, completes all actions, exits 0 without timeout and passes
both save checkpoints, but reports unsupported PickIdle twice. Baurus's
required selection and all four required door transitions remain missing;
545 blocked/no-progress events exceed the unchanged 500 limit. This fixes
condition identity, not the remaining tutorial acceptance gates.

### Native idle record retention (2026-09-07)

The IDLE reader now retains native ANAM section flags, DATA parent/predecessor
links and ordered CTDA/legacy CTDT conditions with stable reference identities.
Malformed native sizes are rejected; later-game branches remain separate.
The installed base game's 650 IDLE records contain 650 one-byte ANAMs,
650 eight-byte DATAs, 1,627 CTDA24s and three CTDT20s. The layout and section
interpretation agree with the [xEdit TES4 definitions](https://raw.githubusercontent.com/TES5Edit/TES5Edit/dev-4.1.5/Core/wbDefinitionsTES4.pas).

The first test build failed because the test passed a runtime string to the
literal-only fourCC helper (`m14-idle-record-build.log`). The corrected build
and all 1,595 component tests pass (`m14-idle-record-build-2.log`,
`m14-idle-record-components.log`). This is parser coverage only: PickIdle
selection, section-aware playback and native verification remain unfinished.

## Reproduce

Build and run the audit:

```sh
python3 scripts/oblivion_compat.py m14-audit \
  --oblivion-data "/path/to/Oblivion/Data" \
  --output build/oblivion-compat/m14-audit \
  --count-lock scripts/data/oblivion_compat/oblivion_m14_data_counts.json
```

Run a real scenario with the native OpenMW binary:

```sh
python3 scripts/oblivion_compat.py scenario \
  scripts/data/oblivion_compat/oblivion_m14_city_schedule.json \
  --output build/oblivion-compat/m14-city \
  --variable "source=$PWD" \
  --variable "openmw=$PWD/build/openmw" \
  --variable "resources=$PWD/build/resources" \
  --variable "oblivion_data=/path/to/Oblivion/Data"
```

Validate a generated quicksave independently:

```sh
python3 scripts/oblivion_compat.py runtime-state m14-verify SAVE \
  --report build/oblivion-compat/m14-city/m14-state-verification.json
```
