# M14 navigation, detection, and AI packages: implementation plan

Status: **in progress**. Commit
`d9c68c8ed69bd34f1c35fc7aa7837832ea6e56a9` contains a large candidate
implementation based on `a9acf26cc1e94c97c6ef4c0c53c880b08f82845f`, after
accepted M13. Its previous acceptance declaration was withdrawn because the
implementation and evidence do not satisfy this plan. The provisional report
is `docs/oblivion/M14-NAVIGATION-DETECTION-AI.md`; generated diagnostic
artifacts remain below `build/oblivion-compat/`.

The candidate was committed as one aggregate change rather than the required
phase checkpoints. Resume from the earliest open gate, make subsequent repairs
in the phase-sized commits required by section 14, and do not mark M14 accepted
until every item in section 19 is independently demonstrated. The roadmap
definition remains authoritative:

- Interpret TES4 pathgrids and package data.
- Implement high- and low-level actor processing, schedules, wander, travel,
  follow, escort, eat, sleep, use-item, flee, pursue, dialogue approach,
  companions, and horses.
- Verify synthetic pathgrid maps, door/cell transitions, a 24-hour NPC schedule
  replay, obstruction recovery, companion travel, and deterministic detection.
- Demonstrate that tutorial escorts and representative city populations finish
  their schedules and transitions without stalls, teleport loops, or package
  ordering errors.

## 1. Scope and completion policy

M14 owns navigation data, non-combat perception, package selection and
execution, actor processing tiers, package-driven interaction, companion and
mount locomotion, related script commands, and persistence of those systems.
It may add the minimum TES4 actor-runtime bridge needed to make the M11 visual
actors participate in mechanics. That bridge is a prerequisite, not permission
to implement later gameplay systems.

Do not include:

- M15 melee/ranged combat, armor and damage resolution, death/ragdolls, sneak
  attacks, pickpocketing, crime, guards, arrest, or jail.
- M16 spell/effect execution. A package may reach a clearly represented
  `waiting-for-M16-action` boundary only if such a package actually occurs in
  official TES4 content; it must never report the unavailable effect as cast.
- M17 alchemy/enchanting behavior, M18 dialogue content/UI, or M19 UI work.
- Any original Oblivion executable, Wine, or Proton workflow. Runtime
  acceptance uses this project's `openmw` binary with real Oblivion content.
- Scenario-only teleportation or direct state mutation as a substitute for AI.
  Test control may set initial conditions, advance the game clock, inject a
  documented obstruction, and observe state; ordinary packages must cause all
  subsequent movement and interaction.

M14 is not complete merely because an NPC moves. Completion requires native
record interpretation, correct priority/schedule selection, deterministic
state transitions, high/low processing continuity, save/load, real quest and
city coverage, and Morrowind isolation.

## 2. Baseline findings that shape the implementation

Recheck these facts at the start of implementation and update this section if
the base revision changes.

### 2.1 Records exist but are not in the game store

- `components/esm4/loadpack.hpp/.cpp` parses `EDID`, `PKDT`, `PSDT`, `PLDT`,
  `PTDT`, and 24-byte `CTDA`. It skips 20-byte TES4 `CTDT` and many later-game
  fields. The `PTDT` path incorrectly checks `mLocation.type` when deciding
  whether the target payload is a FormID; it must check `mTarget.type`.
- `components/esm4/loadpgrd.hpp/.cpp` parses `DATA`, `PGRP`, `PGRR`, `PGRI`,
  and `PGRL`, skips `PGAG`, and fabricates an editor ID from the load-order
  `FormId`. It also rewrites the point priority byte from odd/even `z`, which is
  not an acceptable data interpretation.
- Both types are reachable in `components/esm4/records.hpp` and can be dumped
  by `esmtool`, but neither `Store<ESM4::AIPackage>` nor
  `Store<ESM4::Pathgrid>` is present in `MWWorld::ESMStore::StoreTuple`.
  Consequently the runtime does not retain either winning record.
- `ESM4::Npc::mAIPackages` and `ESM4::Creature::mAIPackages` already retain
  package FormIDs in declared priority order, with index zero highest.
- `ESM::FormRecordMetadata` already retains the stable winning record key and
  the first non-null stable group-parent key. A cell-child `PGRD` can therefore
  be associated with its cell through `ESMStore::getFormKeyIndex()`. Do not infer
  a parent from file order, coordinates, `currCell`, or the fake editor ID.
- `components/esm4/formidfields.cpp` already owns the xEdit-derived CTDA
  parameter FormID masks. Reuse and extend that single table rather than
  introducing a second condition-parameter registry.

### 2.2 Existing navigation is TES3-shaped

- `Store<ESM::Pathgrid>::search(const MWWorld::Cell&)` returns no pathgrid for a
  TES4 cell.
- `MWWorld::Scene` registers and removes pathgrids only for TES3 cells.
- `DetourNavigator::Navigator` accepts an `ESM::Cell` and `ESM::Pathgrid` and
  converts their links to off-mesh connections.
- `MWMechanics::AiPackage` obtains an `ESM::Pathgrid`, caches a
  `PathgridGraph` by raw pointer, and otherwise falls back to Recast/Detour.
- The existing path follower, obstacle checks, door opening, `AiWander`,
  `AiTravel`, `AiFollow`, `AiEscort`, `AiPursue`, and `AiActivate` are valuable
  motion primitives, but their TES3 package objects are not a sufficient data
  model for TES4 schedules and conditions.

### 2.3 TES4 NPCs and creatures are not mechanics actors yet

- `MWClass::ESM4Npc` is a plain `RegisteredClass`, does not derive through
  `MWClass::Actor`, has no active actor collision object, and exposes no
  `CreatureStats`, `NpcStats`, `Movement`, or `InventoryStore` through the
  normal class interface.
- `MWClass::ESM4Creature` is currently an `ESM4InteractiveBase`; activation
  treats it like an interaction/container placeholder rather than a live actor.
- M11 supplies TES4 rendering/animation and M13 supplies native item/equipment
  state. M14 must connect those existing authorities to a real actor custom-data
  object; it must not create a second inventory or visual-equipment authority.
- `MWMechanics::Actors` currently performs full AI only inside
  `mActorsProcessingRange`. Outside it, `AiSequence::execute(..., true)` freezes
  most packages. `fastForwardAi()` advances only loaded actors during rest.
  This is not the required low-level process system.

### 2.4 Current placeholders belong to M14

At baseline `OblivionScriptManager` hard-codes `GetCurrentAIProcedure` and
`IsRidingHorse` to zero. It defers `EvaluatePackage`/`EVP`,
`AddScriptPackage`, `PathPointEnable`, `PathPointDisable`, `ForceFlee`, and
`SetRestrained`. The implementation must also audit and provide live behavior
for the condition/query forms of `GetCurrentAIPackage`, `GetIsCurrentPackage`,
`GetDetected`, `GetDetectionLevel`, and `GetLOS` if present in the compiler's
supported command set.

`StartCombat`/`StopCombat`, damage, crime pursuit, and arrest remain M15 unless
the only M14 action is an inert, explicit package-state transition. Never claim
combat happened. `SetUnconscious` should remain with the actor/combat owner
unless real official M14 reachability proves it is required for a schedule.

### 2.5 Preliminary content census

An offline `esmtool dump` census on the canonical 11-file official profile saw
the following raw records. These include overrides and are useful only as a
baseline; acceptance must count winning records after load-order resolution.

| Content file | Raw `PACK` | Raw `PGRD` |
| --- | ---: | ---: |
| `Oblivion.esm` | 7,209 | 8,228 |
| `DLCShiveringIsles.esp` | 0 | 0 |
| `DLCBattlehornCastle.esp` | 100 | 25 |
| `DLCFrostcrag.esp` | 7 | 7 |
| `DLCHorseArmor.esp` | 0 | 0 |
| `DLCMehrunesRazor.esp` | 49 | 34 |
| `DLCOrrery.esp` | 4 | 1 |
| `DLCSpellTomes.esp` | 1 | 0 |
| `DLCThievesDen.esp` | 29 | 8 |
| `DLCVileLair.esp` | 15 | 6 |
| `Knights.esp` | 303 | 61 |
| **Raw total** | **7,717** | **8,370** |

The local Shivering Isles file used for this census is an 85-byte stub. Record
that fact in audit output rather than presenting zero as coverage of the real
expansion. Base-game M14 acceptance must not depend on that stub.

## 3. Target architecture

Use a native TES4 definition/state layer with adapters at the existing engine
boundaries. Do not translate every `PACK` into an `ESM::AIPackageList`, do not
copy TES3 records into the store, and do not scatter game-profile branches
through every old package class.

The intended flow is:

```text
winning PACK/PGRD + stable FormKey metadata
                  |
                  +--> immutable TES4 package/pathgrid definitions
                  |          |
NPC/CREA package list -------+--> schedule + condition selector
                                         |
actor reference + saved AI state --> active package phase machine
                                         |
                     high process: path/physics/animation/action
                      low process: clock/cell-route/abstract progress
                                         |
                         stable promotion/demotion + save/load
```

Separate pure decisions from runtime side effects:

- `components/esm4`: decoded records, package enums and flags, schedule math,
  canonical condition structures, deterministic detection calculation,
  immutable pathgrid graph/adaptation, serializable state, and validation.
- `apps/openmw/mwworld`: winning record lookup, stable reference/cell identity,
  world/cell/door resolution, actor state ownership, save capture/restore, and
  profile service wiring.
- `apps/openmw/mwmechanics`: process-tier coordinator, package selector and
  phase executors, path following, obstacle recovery, companion/mount groups,
  and animation/action requests.
- `OblivionScriptManager`: thin command/query bindings to the same AI service.
  It must not contain a parallel scheduler, detector, or package state machine.

## 4. Data model and parser work

### 4.1 Canonical PACK model

Replace raw interpretation at call sites with typed values while preserving raw
bytes needed for diagnostics:

- Add stable `mFormKey` to `ESM4::AIPackage` and `ESM4::Pathgrid` so the generic
  loader writes the resolved winning identity into the records.
- Define a TES4-only package-type enum. Start with the known TES4 family
  (`Find`, `Follow`, `Escort`, `Eat`, `Sleep`, `Wander`, `Travel`, `Accompany`,
  `UseItemAt`, `Ambush`, and `FleeNotCombat`), then confirm the actual numeric
  domain by a base-master/DLC census. Version-gate values belonging only to
  FO3/TES5; do not copy their `Sandbox`, `Patrol`, `Guard`, or other values into
  TES4 simply because the current shared parser sees later games.
- Decode all observed `PKDT` flags into named masks while preserving unknown
  bits. Audit flag combinations per type and add a reviewed explanation for
  every observed unknown bit before acceptance.
- Decode `PSDT` as an immutable schedule with explicit wildcards and validated
  month, weekday, date, start-hour, and duration. Define exact behavior for
  zero duration, 24-hour duration, durations crossing midnight/month/year, and
  impossible month/date combinations.
- Represent `PLDT` and `PTDT` as tagged unions. A numeric object type is not a
  FormID. Fix the current `PTDT` location/target mix-up and apply FormID
  adjustment only to variants documented and verified as references.
- Parse both 24-byte `CTDA` and 20-byte TES4 `CTDT` into one canonical
  condition. Preserve comparison operator, OR/grouping bit, global-comparison
  bit, run-on context, function index, comparison value/global, and correctly
  typed parameters. Use `formidfields.cpp` to identify FormID slots.
- Reject malformed subrecord sizes and invalid discriminants with record key,
  plugin, subrecord, and offset in the diagnostic. A later-game layout should
  be intentionally version-gated and skipped with a count, not partially read
  as TES4.

Parser tests must build binary fixtures for every valid layout, every tagged
location/target variant, parameter typing, schedule wildcard/wrap behavior,
unknown-bit preservation, truncation, overlong payloads, and the existing
`PTDT` regression. Add load-order tests proving an override wins by stable key
and a deletion removes the store entry.

### 4.2 Canonical PGRD model

- Preserve the encoded point fields exactly. Delete the odd/even-`z` priority
  rewrite. Name a point flag/priority only after a fixture or real-data audit
  demonstrates it.
- Validate `DATA` against `PGRP` count without assuming subrecord order. Validate
  `PGRP` alignment, the total expected `PGRR` endpoint count, every local and
  foreign node index, and `PGRL` minimum/aligned size.
- Preserve `-1` sentinels only where the format permits them. Reject or report
  all other negative/out-of-range endpoints instead of indexing unchecked.
- Decode `PGAG` after auditing its sizes and correlation with point counts and
  disabled/preferred points. If it is proven non-routing data, retain it as a
  typed opaque field and document why ignoring it cannot affect M14. It may not
  remain an uncounted skip.
- Convert `PGRL.object` and every appropriate record reference to stable
  `FormKey` through the existing resolver. Retain encoded FormIDs only for
  diagnostics.
- Get the owning cell from the winning record's `FormRecordMetadata::mParent`.
  Validate that it resolves to a winning `CELL`, that record and metadata keys
  agree, and that no winning cell has an ambiguous winning pathgrid.
- Establish coordinate semantics from synthetic fixtures and real records.
  Preserve encoded coordinates; expose a documented world-space conversion
  from the adapter. Do not assume all points are cell-local or all points are
  world-space without testing interiors, ordinary exterior cells, and worldspace
  boundaries.

### 4.3 Store registration and audit

- Add the required forward declarations/includes and
  `Store<ESM4::AIPackage>`/`Store<ESM4::Pathgrid>` entries to `StoreTuple`.
- Exercise generic stable override/deletion behavior and editor-ID lookup where
  applicable. PGRD has no real `EDID`, so remove the fake ID and never insert it
  into the editor-ID map.
- Add an offline `m14-audit` command to `scripts/oblivion_compat.py`. It must
  report winning PACK/PGRD counts and fingerprints, package type/flag/schedule/
  location/target distributions, condition functions and unsupported forms,
  pathgrid node/local-edge/foreign-edge/object-link/PGAG distributions, invalid
  references, actors without resolvable packages, and cells without a usable
  graph.
- Check in a reviewed count-lock JSON generated from the canonical content
  profile. Raw counts above are not the lock. Changed counts or fingerprints
  fail until intentionally reviewed.

  Verification correction (2026-09-07): comparison values/global identities
  and mixed CTDA/CTDT order are now retained in the audit fingerprint. The old
  lock failed only on the package fingerprint; after reviewing that isolated
  change, the official-content audit passes (`m14-audit-comparisons/`). Numeric
  threshold changes can no longer pass unnoticed. Live acceptance remains open.

Suggested files:

- `components/esm4/loadpack.hpp/.cpp`
- `components/esm4/loadpgrd.hpp/.cpp`
- `components/esm4/aipackagedata.hpp/.cpp`
- `components/esm4/pathgriddata.hpp/.cpp`
- `apps/openmw/mwworld/esmstore.hpp/.cpp`
- `apps/components_tests/esm4/testloadpack.cpp`
- `apps/components_tests/esm4/testloadpgrd.cpp`
- `scripts/data/oblivion_compat/oblivion_m14_data_counts.json`

## 5. Native pathgrid service

Create an immutable TES4 pathgrid service keyed by stable cell and PGRD
`FormKey`. It should retain native node identities while exposing the graph
operations required by both `PathgridGraph` and the navigator.

### 5.1 Adapter responsibilities

- Resolve one winning PGRD per owning cell, its encoded-to-world transform, and
  a stable `{pgrd key, node index}` identity.
- Build deterministic adjacency from local `PGRR` connections. Sort/deduplicate
  duplicate edges only in the derived graph; preserve raw data for audit.
- Resolve `PGRI` cross-cell connections by matching the foreign coordinate to a
  neighboring winning pathgrid with an explicit tolerance and deterministic
  tie-break. Record the source node and resolved destination cell/node.
- Resolve every `PGRL` object link to the winning reference, validate linked
  nodes, and classify doors/furniture/other relevant references. Door links are
  authoritative hints for cell-transition routing but still require an actual
  usable door and destination.
- Apply runtime node enable/disable overlays without mutating immutable content.
  Graph generation/revision must change when an overlay changes so path caches
  cannot retain disabled nodes.
- Expose nearest enabled node, reachable component, route, path cost, and
  point/edge iteration in a profile-neutral interface. Return typed failure
  reasons (`no-grid`, `no-near-point`, `different-component`, `disabled`,
  `unresolved-foreign-link`) instead of a bare empty path.

### 5.2 Navigator integration

Prefer one of these designs, in order:

1. Extend the navigator's pathgrid input to a small profile-neutral immutable
   view containing world-space points and edges.
2. If that is disproportionately invasive, create an owned `ESM::Pathgrid`
   compatibility view with stable lifetime and documented coordinate mapping.

In either design, native `ESM4::Pathgrid` remains the source of truth. Do not
insert a fake TES3 record into `ESMStore`.

Update `MWWorld::Scene` so TES4 cell load/unload registers/removes the adapted
graph exactly once. Ensure asynchronous navmesh updates cannot retain pointers
to destroyed adapters. Rebuild only the affected off-mesh connections when a
path point overlay changes.

Change AI path access to request the current cell's profile-neutral graph from
the service. Remove the process-global `static map<const ESM::Pathgrid*, ...>`
cache or put generation-aware caching behind the service. Recast remains the
primary continuous-space path solver; pathgrid points, object links, and
cross-cell links supplement it and provide fallback/intent, not a replacement
for collision-aware navigation.

### 5.3 Synthetic pathgrid gate

Unit/integration fixtures must cover:

- empty, one-node, linear, branching, cyclic, disconnected, and duplicate-edge
  graphs;
- nearest-node ties and unreachable destinations;
- malformed point/link counts and every invalid index class;
- enabled/disabled nodes, cache invalidation, and save/reload of overlays;
- interior and exterior coordinate transforms;
- paired adjacent exterior cells with `PGRI` in both directions;
- a missing or ambiguous foreign destination;
- `PGRL` links to a normal door, teleport door, non-door object, deleted object,
  and missing object;
- cell add/remove/re-add without duplicate navigator edges or stale pointers.

## 6. Make TES4 NPCs and creatures real actors

This phase must land before package execution. Rendering an NPC while updating
AI state in an unrelated shadow object will create irreconcilable save and
interaction bugs.

### 6.1 Class/custom-data bridge

- Introduce shared TES4 actor custom data (or a carefully templated TES4 actor
  base) containing `CreatureStats`, `Movement`, and `InventoryStore`; add
  `NpcStats` only for NPCs. Copy/clone semantics must match `RefData` ownership.
- Derive/register `ESM4Npc` and `ESM4Creature` through `MWClass::Actor`, while
  preserving the M11 model, FaceGen, skeleton, animation, scale, tooltip, and
  activation behavior.
- Restore actor collision via the common `Actor::insertObject` path with the
  correct native model/bounds. Test initial insertion, cell move, disable/
  enable, death placeholder state, and removal for leaks or duplicate bodies.
- Implement the class virtuals required by mechanics: creature/NPC stats,
  movement settings, container/inventory store, actor type flags, walking/
  swimming/flying capability, speed calculations, skill lookup, faction,
  essential/persistent flags, and base fight/confidence data.
- Initialize native attributes, 21 skills, health/magicka/fatigue, level,
  aggression, confidence, energy, responsibility, factions, base scale, and
  movement capabilities from TES4 records. Reuse the M12 native formula layer;
  never run TES3 autocalculation or read TES3-only GMSTs.
- Populate the actor's one authoritative `InventoryStore` through the M13
  native definition/instance mapping. Equipment rendering, encumbrance,
  package item use, scripts, transfer, and persistence must observe the same
  store. Remove the old static armor/clothing vectors only after parity tests.
- Resolve leveled actors/templates only through already-supported stable record
  rules. If M14 uncovers unsupported official template semantics affecting AI,
  implement and test the minimum inheritance needed for packages/stats rather
  than silently creating an empty actor.

### 6.2 Profile isolation

The common `MWMechanics::Actors` loop currently performs TES3 aggression,
crime pursuit, dialogue greeting, and magic maintenance. Route TES4 actors to
the native M14 process coordinator before those TES3-only decisions. Shared
locomotion/controller updates are fine; TES3 combat engagement and crime must
not start for an Oblivion actor before M15.

Actor bridge tests must demonstrate `isActor()`, physics collision, stats,
movement, inventory identity, equipped visuals, copy/move between cells, and
save/reload for an NPC and a creature. Rerun M11 face/body/animation and M13
equipment checks so the structural change does not regress accepted work.

## 7. Pure schedule, condition, and detection layer

Build this layer without world mutation so it receives exhaustive component
tests.

### 7.1 Schedule evaluation

Represent game time as a normalized calendar instant plus monotonically
increasing evaluation generation. Given an actor's ordered package list and an
instant, return all schedule-eligible candidates in declared order. Test every
wildcard, weekday/month boundary, leap/non-leap policy used by the current game
clock, duration edge, midnight wrap, timescale jump, wait/rest jump, and clock
rewind/load.

Selection order is:

1. valid transient script package, if any;
2. first base package in the actor record's declared priority order whose
   schedule and conditions pass;
3. a typed idle/no-package state, never an invented Wander package.

Tie-breaking must not depend on unordered-container iteration or load-order
FormIDs. Use package list position, then stable `FormKey` only as a final
deterministic diagnostic order. `EvaluatePackage` forces selection immediately;
ordinary selection occurs at schedule/condition invalidation and bounded fixed
ticks, not every render frame.

### 7.2 Conditions

Create one typed TES4 condition evaluator shared by package selection and
ObScript query behavior. An evaluation context should explicitly supply
subject, target, reference/run-on object, player, cell/worldspace, clock,
globals, quest stages, actor values, inventory, faction, detection, and random
source. This avoids hidden global lookups in pure tests.

- Implement the comparison operators and AND/OR grouping exactly.
- Distinguish numeric parameters from FormKeys and distinguish a numeric
  comparison value from a global-backed comparison.
- Resolve subject/target/reference contexts exactly; missing context is a
  typed result, not an implicit player substitution.
- Inventory/global/quest/actor-value queries must call the M7/M12/M13
  authorities. LOS/detection queries call the M14 detector.
- Census every condition function reachable from winning official packages.
  Every reachable function must be implemented or the milestone remains open.
  Unsupported unreachable functions may be listed in a reviewed lock with
  count zero. Never turn an unknown function into unconditional true or false.
- Log a given unsupported condition signature once with actor/package/function
  identity, while the audit treats any reachable occurrence as a failure.

### 7.3 Detection

Add an explicit pure `ESM4::DetectionInput` and result type. Keep a continuous
score for `GetDetectionLevel` and a threshold/roll result for `GetDetected`.
Inputs should include, when applicable:

- observer and target Sneak, Agility, Luck, and relevant native actor state;
- distance, view angle/facing, unobstructed LOS, interior/exterior light, target
  illumination, movement mode/speed, footwear/armor noise, sleeping state, and
  invisibility/chameleon/sound modifiers;
- released TES4 GMST coefficients read per profile, not static TES3 GMST caches;
- a deterministic random sample supplied by the caller.

M16 may later provide real effect magnitudes; until then the detector accepts
zero or an explicit value from existing state and does not simulate spells.
LOS false must be a hard non-detection result unless a verified native rule
says otherwise. Add property tests for bounds and monotonicity in distance,
view cone, observer ability, target Sneak, light, movement/noise, and effect
magnitudes, plus exact fixed-seed vectors. Save either the AI RNG state or all
information needed to reproduce its next sample.

## 8. Package scheduler and process tiers

Create a TES4 process coordinator owned by the mechanics/world profile service,
keyed by stable actor reference `FormKey`. It owns runtime package state for
both loaded and unloaded actors and is the only authority that changes active
package/phase.

### 8.1 Explicit runtime state machine

At minimum track:

- actor reference and base record stable keys;
- selected package stable key and source (`base` or `script`), list priority,
  selection generation, schedule window, and condition result;
- package procedure and phase (`select`, `resolve`, `path`, `door`, `arrive`,
  `act`, `wait`, `complete`, `interrupted`, or a more precise typed enum);
- stable target reference/base/cell/pathgrid-node keys, resolved destination,
  last valid position/cell, and intended door edge;
- action timer, package duration remaining, repath attempts, no-progress timer,
  door cooldown/visited transition generation, and interruption reason;
- process tier, next low-process tick, restrained state, companion group and
  mount/rider relationship, and deterministic RNG state.

The navigation path itself, raw pointers, `Ptr`, controller handles, and navmesh
polygons are ephemeral and must be rebuilt after load/promotion.

Use a two-stage executor: package-specific logic chooses intent/phase; shared
movement code realizes a destination, opens/uses a door, detects obstruction,
and reports arrival/failure. That prevents each package from inventing its own
door and recovery semantics.

### 8.2 High process

Loaded, active actors within the native processing range receive fixed-step
package decisions, full collision-aware navigation, controller movement,
animation/action requests, detection, door use, and head/facing updates.
Render FPS must not change schedule selection or total simulated travel. Bound
catch-up steps after a long frame and record dropped/backlogged time.

Do not execute TES3 `AiSequence` concurrently for a TES4 actor. Existing AI
classes may be composed as tested motion helpers, but the TES4 package and
phase remain visible and authoritative.

### 8.3 Low process

Actors outside high range, in inactive cells, or unloaded but eligible for
processing advance at a fixed game-time cadence. Low processing must:

- reevaluate schedules and conditions at exact relevant boundaries;
- resolve travel over a coarse graph of cells, linked doors, exterior
  neighbors, and native pathgrid foreign links;
- advance distance/time deterministically using native movement speed;
- run non-visual action timers and commit package side effects only at the same
  phase boundary as high process;
- retain the last valid waypoint and intended transition rather than directly
  snapping to the final schedule destination;
- prioritize persistent, quest-referenced, companion, escorted, mounted, and
  explicitly scripted actors, with a documented budget for ordinary population;
- remain independent of whether a cell happened to be preloaded for rendering.

Promotion validates the saved low-process cell/position against the current
graph/navmesh and snaps only to the nearest reachable point within a bounded,
logged tolerance. On failure, keep the last valid waypoint and request a
recovery path; never teleport through walls. Demotion captures stable intent
and progress before destroying ephemeral controller/path state.

Test high-to-low-to-high continuity, unload/reload, save/reload in each phase,
wait/rest acceleration, timescale changes, and deterministic results at 30,
60, and 144 FPS render pacing.

## 9. Shared movement, doors, and obstruction recovery

- Represent a cross-cell route as ordered segments with stable source/dest cell,
  pathgrid nodes, door reference, and destination marker. Re-resolve each key
  at use time so content overrides and cell reloads cannot leave stale pointers.
- Normal doors use the existing collision/animation activation path. A locked
  door may open only if M13 ownership/key rules allow it; AI does not silently
  unlock or bypass it. Trapped-door consequences remain bounded by existing
  M13 behavior and later M15/M16 effects.
- Teleport doors transition exactly once per route edge. Record a transition
  generation, source door, destination cell, and cooldown to prevent immediate
  reverse selection/ping-pong. Place the actor at the real destination marker
  with a small collision-validated forward offset.
- Followers, escorts, companions, and mounts reference targets by stable key and
  can follow their target through a door after resolving the corresponding
  destination. Reuse useful `ActionTeleport` concepts, but do not depend solely
  on its loaded-actor, within-800-unit follower scan.
- Detect no progress using distance-along-route and elapsed fixed steps. Recovery
  proceeds through bounded local steering, door re-evaluation, navmesh/pathgrid
  repath, alternate reachable pathgrid node, and finally a typed stalled state
  with one diagnostic. Never loop unboundedly or silently teleport.
- Restraint stops translation/package action while still allowing time and
  package reevaluation as verified. Releasing restraint resumes from a rebuilt
  path without resetting schedule priority.

Obstruction integration tests need an unlocked door, a locked door with key, a
locked door without key, a temporary movable blocker, an opening door, an
unreachable target, and a blocker removed after a stall. Assert bounded repath
counts and successful recovery where a route becomes available.

## 10. Required package behavior

For every behavior below, define selection, target/location resolution, high
process, low process, completion/repetition, interruption, save/load, and query
procedure. A package cannot be considered implemented if only its movement
portion works.

| Behavior | Required semantics and evidence |
| --- | --- |
| Wander | Choose enabled, reachable destinations inside the package radius/location; use pathgrid connectivity and deterministic RNG; honor idle/action timing and schedule end; never choose through a closed unreachable component. |
| Travel | Resolve the exact location/reference/object-type destination; route across cells and doors; complete only within a documented native tolerance; remain complete until reevaluation rather than restarting each tick. |
| Follow / Accompany | Resolve a stable moving target; maintain near/far spacing, facing, deterministic formation order, lost-target recovery, and door/cell following. `Follow` remains active; `Accompany` obeys its duration/location completion rules as verified from content. |
| Escort | Keep the target/group together while moving to the package destination; pause/recover when the target is separated; complete only when escort conditions and destination are satisfied. Exercise the real tutorial escort chain. |
| Eat | Resolve food/container/location, reserve exactly one M13 inventory item, travel/use appropriate furniture or idle, consume only on successful action completion, and release the reservation on interruption. No M17 alchemy effect is applied. |
| Sleep | Resolve an available bed/furniture marker, honor ownership/availability, travel and enter/leave the sleep idle for the scheduled duration, then wake/reselect. Do not add player rest benefits or spell recovery. |
| Use Item At | Resolve item and target/location, reserve the item, travel, and invoke the normal M13 activation/use path once. Preserve the item if interrupted before the commit point; never synthesize M16 magic effects. |
| Find | Resolve the requested reference/base/object type and search/travel until within package distance; define deterministic candidate order and behavior when no candidate exists. |
| Flee / Flee Not Combat | Track the threat, choose a reachable point that increases path distance and breaks LOS, reevaluate detection/safe distance, and complete/continue per the package. This is locomotion only and must not start M15 combat. `ForceFlee` installs the same explicit state. |
| Pursue | Follow a stable target across cells into interaction range and expose a pursue/arrival state. Crime pursuit and attack/arrest decisions remain M15. |
| Dialogue approach | Approach, face, and emit a typed `ready-for-dialogue` event/state at the native greeting distance. M18 consumes it; M14 must not fabricate topic selection, subtitles, or dialogue completion. |
| Companion travel | Build stable, cycle-safe follow groups; preserve formation indices and side-with relationships; cross doors/cells; recover a separated member; and persist group membership. |
| Horses | Parse/store `ACHR.XHRS` instead of skipping it; model stable horse/rider/owner/last-ridden relationships; make horse creatures real actors; attach/detach rider transforms and collision safely; support follow/travel and door-boundary dismount/recovery; make `IsRidingHorse` truthful. Combat while mounted remains M15. |

Audit every winning official TES4 package type. If an observed type is not named
above, either implement its complete navigation/scheduling semantics when they
are within M14, or document a precise later-milestone action boundary while
still selecting, traveling, waiting, interrupting, and reporting it correctly.
An observed package must never silently become Wander or success.

For `Ambush`, for example, M14 can navigate to and wait at the ambush location
and expose `ready-for-M15-combat`; it cannot pretend an attack happened. Apply
the same rule to any official effect/combat-specific type found by the census.

## 11. ObScript integration

Expose an `OblivionAiService` (name may vary) to the script host and route all
commands/queries through it:

- `EvaluatePackage`/`EVP`: increment the actor's evaluation generation and
  select immediately after the current script statement, with deterministic
  interruption cleanup.
- `AddScriptPackage`: resolve a stable package key, push/replace the documented
  transient package slot, and persist it. Invalid/non-PACK input is a typed
  runtime error rather than a no-op.
- `GetCurrentAIPackage` and `GetIsCurrentPackage`: return stable native package
  identity comparisons, including transient source rules.
- `GetCurrentAIProcedure`: return the procedure corresponding to the real phase,
  using verified TES4 numeric values. Do not reuse TES3 values without an
  explicit mapping test.
- `PathPointEnable`/`PathPointDisable` and linked-pathpoint variants: mutate the
  runtime graph overlay for the addressed `PGRD` node/object link, bump graph
  generation, invalidate affected routes, and persist.
- `GetLOS`, `GetDetected`, and `GetDetectionLevel`: query the same live detector
  and raycast used by package conditions. The score query must not consume RNG.
- `ForceFlee`: install or update the native flee intent with its real target and
  duration; do not start combat.
- `SetRestrained`: update native actor process state and locomotion immediately.
- `IsRidingHorse`: inspect the stable mount/rider relationship.

Remove these names from the deferred set and delete their hard-coded results.
Add compiler/runtime tests for direct, reference-qualified, condition, quest,
and object-script contexts; invalid targets; save/load; and command ordering in
one frame. The M7 official script corpus must compile without regression, and
runtime logs must contain no `deferred command=` entry for an M14-owned command.

## 12. Persistence schema versions 5 and 6

Bump `ESM4::CurrentRuntimeStateVersion` from 4 to 5 and use typed structures,
not ad-hoc `mCustomState` strings, for AI state. A suggested layout is:

2026-09-06 repair extension: version 6 appends a typed FIFO of pending native
`OnPackageDone` actor/package identities. Completion callbacks can save or
change actors, so delivery must occur outside actor iteration and consume the
current event before invocation while preserving outstanding events in saves.
Version 5 remains readable and must not synthesize callbacks for already
completed packages. The FIFO is intentionally ordered, not sorted by FormKey.
Both C++ and Python codecs support the extension; runtime delivery and native
tutorial evidence must be verified separately from codec round trips.

- `RuntimeActorAiState` per actor reference with the fields in section 8.1;
- `RuntimePathPointState` keyed by PGRD FormKey/node index (or object-link key)
  for enable overlays;
- stable companion/mount relations represented once with validated reciprocal
  references;
- a deterministic AI RNG state/stream separate from unrelated gameplay RNG.

Requirements:

- Canonical ordering by stable key and canonical JSON output.
- Binary and Python codec parity, strict bounds, finite coordinates/timers,
  valid enum values, unique actor and point keys, valid content ownership, and
  no dangling package/target/mount reference after content validation.
- Versions 1--4 migrate with no active native package, default graph overlays,
  no companion/mount relationship, and a deterministic derived RNG seed.
  Migration must preserve all earlier player, script, quest, reference, and M13
  inventory state byte-for-byte at the logical level.
- Active package, action reservation, low-process route progress, restrained
  state, disabled path points, companion group, and mount state survive
  quicksave/reload. Ephemeral paths/controllers are rebuilt exactly once.
- Content reordering and changed numeric load-order indices resolve through
  `FormKey`; missing/deleted content produces an actionable validation error or
  a documented safe package interruption, never retargeting another record.

Add `python3 scripts/oblivion_compat.py runtime-state m14-verify SAVE --report
REPORT`. It should validate schema, stable identities, package/phase chronology,
route/door generations, schedule checkpoint states, graph overlays, detection
vectors, companion and mount relationships, and absence of direct-teleport test
markers.

## 13. Observability and deterministic test control

Extend the existing scenario harness with M14 controls that are unavailable in
normal gameplay only when they are clearly marked test observation/control:

- select an actor by stable reference/editor ID and observe package, procedure,
  phase, cell, position, target, route/repath count, process tier, and schedule;
- advance the game clock to an exact instant or by an exact duration;
- place/remove a simple collision obstruction without moving the tested actor;
- set a fixed AI/detection seed;
- capture actor state checkpoints and a compact transition event stream;
- toggle path visualization/debug labels for screenshots without changing AI;
- fail on a bounded no-progress timeout, repeated door-edge transition, invalid
  package reordering, or forbidden fallback.

Test controls must not select packages, advance package phases, move actors,
consume items, open doors, mount horses, or force success. Keep production
behavior independent of whether event reporting is enabled.

Use structured events such as:

```text
ai actor=<FormKey> package=<FormKey> phase=<old>-><new> reason=<reason>
ai-route actor=<FormKey> cell=<FormKey> door=<FormKey> generation=<n>
ai-tier actor=<FormKey> high->low position=<...> game_time=<...>
detection observer=<FormKey> target=<FormKey> score=<...> detected=<0|1>
```

Rate-limit repeated diagnostics but keep counters in the final report.
Failure budgets must count occurrences, not sampled log lines. Diagnostic
summaries close each counter epoch before clear, restore, and shutdown; live
samples carry cumulative counts. Final verification rejects missing or
inconsistent totals. Aggregates are not evidence for ordered actions.

## 14. Implementation phases and commit checkpoints

Do not combine all M14 work into one unreviewable change. Each checkpoint below
must build, run its focused tests, pass `git diff --check`, and be committed
before proceeding. Suggested commit subjects are illustrative.

### Current checkpoint after acceptance review

2026-09-08: the Telepe companion preflight proves all three named actors'
unobstructed XTEL transitions and saved interior cells, but **fails** on an
unsupported moth path interpolator and invalid `exit()` console input. It does
not establish obstruction/recovery or reload behavior. Repair those findings
without log waivers. Path-interpolator flags require constant velocity and
orientation-following, not translation-only playback. Also audit dynamic door
availability: immutable initially-disabled flags must not override enabled
resident/saved state, nor may disabled/deleted doors remain usable. Sanitized
engine tests pass 526/526; M14 remains in progress.

The subsequent door-state repair removes immutable enabled-flag gates from
XTEL indexing and runtime access, uses resident/saved lock and ownership state,
and prevents removed inventory keys from reappearing through base-data fallback.
Engine tests pass 527/527. Native disabled-door interruption is observed in the
in-progress Telepe obstruction run; recovery, both-side reload and the full
lock/key/ownership matrix are still open. The first fixture's expected
`route-blocked` event is incorrect for a `door-unavailable` phase interruption.

The aggregate candidate contains code corresponding to every phase, but code
presence is not a passed phase gate. Use this table as the resume point and
update it only when the named gate has fresh, actor-specific evidence.

Current verification repair: historical quickload streams may have lost
diagnostic totals when the service reset its counters. Rerun those courses with
the counter-preservation fix; a prior `passed` result is not sufficient. The
tutorial currently saves and reloads native CharacterGen state but remains at
the initial dialogue-dependent wait. Do not use `SetStage` or a package override
to claim escort progression. Section 16.1 permits dialogue-independent waiting
controls only; the real escort/arrival/reload gate remains open.

2026-09-06 repair checkpoint: affected-route overlay invalidation is repaired;
the city course now synchronizes startup to a named route and passes its
current five-checkpoint course (`m14-city-overlay-ready/`). Engine tests pass
522/522, Python tests 67/67, and ASan/UBSan component tests 1,582/1,582. These
are repair-stage results, not final clean-revision acceptance. Reviewed
content-gap counts are now locked and pass the official audit.

The user approved the minimum real dialogue prerequisite on 2026-09-06. Native
`Say`/`SayTo` now select conditioned INFOs, resolve native voice links and ordered
responses, and dispatch the selected result against the speaker. The real
`m14-tutorial-native-dialogue/` run progresses CharacterGen through stages
5, 6, 9 and 10 with ordinary player movement. Both save/reload checkpoints pass,
but the tutorial still FAILS: Baurus does not select 032aeb, none of the four
escorts records the required door transition, and 530 blocked/no-progress
events exceed the unchanged 500 budget. Continue investigating actual native
package/script progression and door intent; do not accept initial arrivals as
the complete escort course. M18 owns dialogue under section 1; this is only the
authorized prerequisite, not M18 acceptance. A
scheduled test dialogue-result event or `SetStage` is not equivalent evidence.
Do not mark the tutorial or M14 complete before that behavior is verified.
Even after the prerequisite is implemented, companion recovery, valid door
fixtures, the full process/reload/detection/horse/Morrowind matrices, engine
integration coverage, and final clean-revision verification remain required.

2026-09-07 completion-event repair: the coordinator now dispatches real
`OnPackageDone` outside actor iteration with a persisted schema-6 FIFO. The
latest tutorial (`m14-tutorial-package-done/`) reaches stage 12 through Renault's
native callback, and both save checkpoints pass. It still fails on unsupported
`Look` in native result scripts, missing Baurus/door progression, and 530
blocked/no-progress events against the unchanged 500 budget. Repair `Look` and
inspect subsequent real triggers before claiming the tutorial complete. The
run's resource-version mismatch also requires matching-build verification.

Subsequent `m14-tutorial-scripted-look/` repairs Look/StopLook, has matching
resources, reaches native stage 13 and passes both schema-6 checkpoints. It
still FAILS: `PickIdle` is unsupported in INFO 00bf0d, native `OnTrigger` is
not wired to the stage-14 phantom volume, and the Baurus/door/budget gates above
remain unmet. Real idle and trigger semantics are required, not no-op command
handlers. Dedicated live head-tracking/StopLook/reload coverage is also open.
Native variable conditions now resolve SLSD IDs by SCVR name, with sparse-ID
regressions; they no longer read unrelated source-order locals. The unchanged
`m14-tutorial-native-variables/` still fails at PickIdle and the same Baurus/door
gates, with 545 blocked/no-progress events against 500. Neither this correction
nor passing helper tests closes the tutorial gate.
IDLE parser, runtime-store registration and a validated, non-recursive authored
hierarchy index now have component/sanitizer and engine-store coverage.
PickIdle command wiring, skeleton-family selection, playback sections and
native animation/reload evidence are still outstanding; do not mark this
prerequisite complete from the store/index tests.
The Travel-gait correction keeps the companion within 512 units in
`m14-companion-authored-gait/`, but that course fails to establish separation
(347.39 < 512), so recovery is unproven. Its enable/disable controls supply a
discarded TES3 argument instead of selecting the native reference, and their
errors are absent from text evidence. Repair the control/acknowledgement and
actual obstruction assertions before accepting companion or door courses.

| Phase | Candidate state | Gate state |
| --- | --- | --- |
| 0: baseline | Audit and ordinary test artifacts exist; the harness supports actor-scoped ordered events, counts, failure reasons, exact saved-state fields, named-actor checkpoint deltas, pair-distance outcomes, and named detection-pair outcomes; all six core scenarios use named-identity gates and tutorial has a four-actor before/after quickload pair | Open: add exact tutorial targets and the remaining before-door, after-door, waiting, and package-transition reload boundaries; expand detection and horse beyond their smoke gates; reproduce from the candidate revision and retain exact logs |
| 1: records/audit | PACK/PGRD stores, typed data, audit tooling, and a count lock are present | Open: independently review parsing/identity claims and rerun full sanitizer coverage |
| 2: pathgrids | Graph, overlay, foreign/object-link, Scene, and navigator code plus focused component tests are present; the Imperial Prison live-cell run now builds convex collision input without tile-job failures and resolves Valen's continuous navmesh | Open: complete obstruction invalidation, lifecycle/door integration, and real-cell gates under sanitizers |
| 3: actor bridge | NPC/creature actor bridge code is present; multipart TES4 NPCs now derive collision/agent bounds from their model shape, with configured bounds as a last resort, instead of registering zero-sized navigator agents | Open: add the remaining physics, stats, inventory-identity, movement, reload, and TES3-isolation integration tests |
| 4: pure AI rules | Schedule, condition, selection, phase, and detection helpers have focused tests | Open: prove complete reachable-condition coverage, calendar/FPS properties, and the full detection matrix |
| 5: high process | Selector and high-process locomotion code is present | Open: prove a synthetic population, real high-process movement, phase reloads, and absence of TES3 AI conversion |
| 6: doors/low process/state | Door, low-process, obstruction, and schema-5 code is present | Open: resolve recorded stalls/locked-door cycles and prove deterministic promotion, reload, and door behavior |
| 7: package behaviors | Candidate behavior branches exist for the required package families, companions, and horses | Open: add behavior-specific integration, interruption, persistence, and real-content evidence |
| 8: ObScript/audit | Candidate M14 command/query bindings exist | Open: test every context and command against the live service and retain official-corpus/deferred-trace evidence |
| 9: acceptance | A report and scenario artifacts were produced | Invalidated: strengthen the harness, rerun every section 16 scenario, then perform a new acceptance review and commit |

### Phase 0: reproduce the baseline and lock the worklist

1. Build `esmtool`, `components-tests`, `openmw-tests`, and `openmw`.
2. Run current full component, engine, and Python compatibility suites.
3. Run the preliminary PACK/PGRD audit against the canonical content profile.
4. Record content file identity, size/hash, winning counts, all package values,
   condition functions, and pathgrid irregularities.
5. Turn every observed unknown into a named work item. Do not add an exception
   merely to make the audit green.

No acceptance-status change occurs here.

### Phase 1: harden records, stable stores, and count locks

Implement section 4, including `CTDT`, `PTDT` fix, preserved PGRD bytes, stable
keys/parents, typed stores, malformed fixtures, override/deletion tests, and the
audit command. Suggested commit: `Load and audit native Oblivion AI records`.

Gate: all winning PACK/PGRD records load, all references are classified, and
there are no unreviewed skips or unknown observed types.

### Phase 2: pathgrid graph and navigator integration

Implement section 5, all synthetic fixtures, Scene add/remove, graph overlays,
foreign and object links, and cache invalidation. Suggested commit:
`Integrate Oblivion pathgrids with navigation`.

Gate: synthetic maps and paired cell/door tests pass under ASan/UBSan, and a
real-cell diagnostic view shows points/links registered at correct positions.

### Phase 3: native actor mechanics bridge

Implement section 6 and rerun M11/M12/M13 actor, movement, inventory, and visual
checks. Suggested commit: `Promote Oblivion NPCs and creatures to live actors`.

Gate: a real NPC and creature collide, animate, move through the controller,
expose native stats and one inventory, and reload without visual regression;
no TES3 aggression/crime path runs.

### Phase 4: pure schedule, conditions, and detection

Implement section 7 with exhaustive table/property tests and real-data
condition coverage. Suggested commit: `Add deterministic Oblivion AI rules`.

Gate: every condition reachable from official winning packages has a tested
handler, schedule replay is frame-rate independent, and fixed detection vectors
are exact and monotonic.

### Phase 5: selector, high processing, and basic locomotion

Implement the scheduler/state machine plus high-process Wander, Travel, Find,
and any verified patrol-like behavior. Integrate common path following and
package queries. Suggested commit: `Execute native Oblivion schedules`.

Gate: a synthetic 24-hour population selects packages in exact declared order,
moves without TES3 package conversion, and reloads in every phase.

### Phase 6: doors, low processing, and persistence v5

Implement cross-cell segments, door cooldown, obstruction recovery, promotion/
demotion, low-process ticks, and the version-5 codecs/verifier. Suggested
commits: `Add Oblivion actor process tiers` and `Persist Oblivion AI state`.

Gate: unloaded travel and paired-door loops are deterministic across save/load,
render FPS, wait/rest, and cell load order; old state versions still load.

### Phase 7: remaining package behaviors

Land small, behavior-focused commits in this order:

1. Follow, Accompany, Escort, companion groups, and tutorial door following.
2. Eat, Sleep, and Use Item At using M13 transactions/furniture.
3. Flee, Pursue, and dialogue approach without M15/M18 side effects.
4. Horse record/state, rider attachment, follow/travel, and dismount recovery.

Gate each behavior with pure phase-machine, integration, real-record, interruption,
and save/load tests before starting the next.

### Phase 8: script surface and complete audit

Implement section 11, remove only M14 placeholders, rerun the official script
corpus, and make `m14-audit` fail on any reachable unsupported package,
condition, pathgrid feature, or deferred M14 command. Suggested commit:
`Connect ObScript to native Oblivion AI`.

### Phase 9: end-to-end iteration and acceptance

Run every scenario in section 16, inspect images and state, fix the causes of
all stalls/loops/fallbacks, rerun the full test matrix, and only then write
`docs/oblivion/M14-NAVIGATION-DETECTION-AI.md`, update
`docs/oblivion/README.md`, the roadmap ledger, and
`docs/oblivion/IMPLEMENTATION-STATUS.json`. The acceptance commit should contain
evidence references, not generated build artifacts.

## 15. Automated test matrix

### 15.1 Component tests

Add focused suites (names can follow local conventions) under
`apps/components_tests/esm4/`:

- PACK and PGRD binary decoding, malformed input, stable FormID fields;
- schedule calendar/window evaluation and priority ordering;
- condition grouping, operators, run-on contexts, parameter typing, and every
  official reachable condition function;
- detection exact vectors, seeded random outcomes, properties, and invalid
  inputs;
- pathgrid graph building, cross-cell/object links, overlays, and routes;
- package phase machines and interruption cleanup without a renderer;
- runtime-state v5 binary/JSON round trips, v1--v4 migration, corruption,
  duplicate identities, non-finite values, truncation, and content reorder.

Run new record decoders and state codecs under ASan/UBSan. Add fuzz/regression
seeds for every malformed payload found during the official-content audit.

### 15.2 Engine tests

Add `apps/openmw_tests/mwworld/testoblivionai.cpp` and smaller mechanics tests as
needed. Use a fake clock, stable resolver, world/cell graph, navigator stub, and
runtime host to cover:

- actor creation/physics/stats/inventory for NPC and creature;
- selection/reselection, script-package precedence, and no-package state;
- high/low promotion and unloaded actor advancement;
- pathgraph registration lifecycle and overlay invalidation;
- door permissions, teleport edge cooldown, follower/escort transition;
- obstruction escalation and bounded failure;
- M13 item reservation commit/rollback for Eat and Use Item At;
- sleep/furniture availability;
- companion cycle rejection and formation order;
- horse rider reciprocal state and detach cleanup;
- every M14 ObScript command/query and a guarantee that no TES3 AI sequence is
  created for a TES4 actor.

### 15.3 Python/tooling tests

Extend the existing test suite for:

- deterministic `m14-audit` output and count/fingerprint mismatch failures;
- scenario schema validation and forbidden direct-mutation actions;
- structured event parsing, timeout/loop/order detection, and path summaries;
- `runtime-state m14-verify` for good saves, migrations, corruption, missing
  content, duplicated actors/nodes, invalid reciprocal mount state, and bad
  chronology;
- manifest variables and clear errors when real Oblivion data is absent.

### 15.4 Full regression commands

Use the actual configured build directory, but the final run should be
equivalent to:

```sh
cmake --build build --target esmtool components-tests openmw-tests openmw -j2
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s scripts/tests -p 'test_*.py'
git diff --check
```

Also run focused filters during iteration and record exact totals in the final
report. Never substitute focused filters for the final full suites.

## 16. End-to-end manifests and real-content acceptance

Create checked-in manifests under `scripts/data/oblivion_compat/`. Every run
must write `scenario.json`, `process.log`, structured AI events, checkpoint
state JSON, screenshots where visual behavior matters, and a quicksave that
passes `m14-verify`.

### 16.1 Tutorial escort

Suggested manifest: `oblivion_m14_tutorial_escort.json`.

- Start before the real Imperial Prison Emperor/Blades escort segment using
  released quest, actor, package, door, and cell records.
- Let the ordinary quest scripts and native packages select behavior. Test
  control may advance dialogue-independent waiting only after recording why.
- Assert each escort's declared package order, target, path phases, door/cell
  edge sequence, arrival tolerances, and bounded repaths.
- Save/reload before a door, after a door, while waiting, and after a package
  transition. The escort must continue without a duplicate transition,
  vanished actor, or package restart loop.
- Visually inspect formation, facing, walking/idle animation, collision, door
  use, and absence of sliding/teleport pops. State evidence remains decisive.

### 16.2 Representative 24-hour city population

Suggested manifest: `oblivion_m14_city_schedule.json`.

- Select 8--12 named base-game NPC references across at least three Imperial
  City districts/interiors. Preflight must resolve their real stable actor and
  package keys; a missing actor is a failed fixture, not silently replaced.
- Include examples whose audited package lists exercise sleep, eat, work/use,
  travel, wander, and at least one cross-interior/exterior transition.
- Replay a full 24 game hours at a controlled timescale. Record at every package
  boundary and at regular checkpoints: clock, active package, list priority,
  condition outcomes, phase, tier, cell, position, target, and inventory action.
- Run once with actors kept high process where practical and once with normal
  load/unload/low processing. End states and package histories must agree within
  documented movement tolerances.
- Save/reload at morning, midday, evening, and midnight wrap. No NPC may stall,
  invert package priority, consume twice, sleep indefinitely, or oscillate
  through a door.

### 16.3 Door and obstruction course

Suggested manifest: `oblivion_m14_navigation_doors.json`.

Use real interior/exterior doors plus controlled blockers. Exercise normal,
teleport, locked-with-key, locked-without-key, temporarily obstructed, and
unreachable routes. Assert the recovery ladder, retry bounds, exact one-way
transition counts, and a clean typed failure where no route exists.

### 16.4 Companion travel

Suggested manifest: `oblivion_m14_companion_travel.json`.

Use a real follower/companion package reachable in released content (a Martin,
Jauffre, or other audited base-game route is acceptable only after preflight
confirms its package and quest prerequisites). Cross exterior, city, and
interior doors, separate the companion with an obstruction, reunite, and
save/reload on both sides of a transition. Verify target key, formation index,
cell history, distance bounds, and no recursive follower cycle.

Current repair evidence (2026-09-06): `m14-companion-live-target/` exercises
active target reroutes and member/leader door crossings with clean event/error
checks. Initial and separated distance checks pass, but final recovery is
863.54 units against the unchanged 512-unit maximum. This gate remains open:
fix remaining recovery lag, prove the obstruction causes the separation, and
complete formation/cycle, process-tier, and both-side save/reload coverage.

2026-09-07 correction: `m14-companion-verified-controls/` supersedes the old
obstruction-control evidence. The corrected Travel gait keeps final spacing
below 512, but separation never reaches 512. Fresh text acknowledgements and
actual reference-state checks prove the fixture's initial enable is a no-op;
the later disable really changes 0b5d5b and is retained in the save. Replace
the ineffective obstruction sequence with a causal, verified obstruction
before accepting separation/recovery. The broader coverage above remains open.

### 16.5 Detection matrix

Suggested manifest: `oblivion_m14_detection.json`.

Place released actors at fixed distances/orientations in a controlled real
interior and exterior. Sweep LOS obstruction, facing, light, stationary/walk/
run/sneak, observer/target skill, and fixed RNG seeds. Compare exact report
scores/results to the component vectors and prove the script queries and
package conditions return the same values. Capture representative visible and
occluded frames to validate scene placement.

### 16.6 Horses

Suggested manifest: `oblivion_m14_horse.json`.

Resolve a real base-game horse and rider/owner relationship. Exercise mount,
travel/follow, high/low transition, an exterior route, a door boundary requiring
dismount, remount where legal, `IsRidingHorse`, and save/reload in mounted and
dismounted states. Inspect rider/horse transforms and collision visually and
verify reciprocal stable keys in state.

### 16.7 Morrowind isolation

Suggested manifest: `morrowind_m14_regression.json`.

Run representative TES3 Wander, Travel, Follow, Escort, doors, rest/AI fast
forward, and detection/awareness behavior. Compare to a pre-M14 baseline. Its
save must contain no TES4 AI/pathpoint/mount markers, and M14 profile services
must not be constructed for the Morrowind profile.

## 17. Runtime log and evidence gates

All M14 scenario logs must fail on:

- assertion, abort, crash, `Error in frame`, uncaught script/runtime error;
- unresolved winning PACK/PGRD, actor package, target, parent cell, path node,
  foreign link, object link, door destination, companion, or mount key;
- unsupported reachable package type/flag/condition function;
- any M14 command logged as deferred or hard-coded placeholder behavior;
- TES3 package/autocalculation/GMST fallback for an Oblivion actor;
- `no path` or `stuck` without the expected bounded recovery/failure event;
- repeated identical door edge beyond the configured cooldown, transition
  generation regression, or a teleport-loop diagnostic;
- package selection that violates the declared list order;
- direct scenario movement/state mutation after initial setup;
- non-finite position/detection/timer, duplicate item consumption, duplicate
  actor state, or invalid mount reciprocity;
- missing required screenshot/state/save/evidence file.

Keep a small reviewed warning allowlist only for unrelated systems with owner
milestone, exact message/pattern, scenario, count, and removal milestone. No
M14-owned warning may be allowlisted for acceptance.

## 18. Visual inspection checklist

Automated position and state assertions are mandatory, but they cannot catch
all controller/attachment faults. Inspect before/after-reload frames and short
sequences for:

- feet following terrain rather than floating/sinking or sliding in place;
- correct walk/run/idle/sit/eat/sleep transitions and actor facing;
- no actor overlap pileups at doors, beds, or package destinations;
- doors visibly open before passage and do not rapidly reopen/reverse;
- escorts/companions maintain plausible spacing and do not pop across walls;
- high/low promotion does not produce a visible large snap;
- rider alignment, horse animation/collision, and clean dismount;
- M11 face, eyes, jaw, hair, body, and M13 equipped-item visuals remain intact.

Record who/what was inspected in the final report and point to exact images.
Do not treat a screenshot as proof of schedule order, item conservation,
detection score, or persistence; those require structured state evidence.

## 19. Definition of done

M14 is complete only when every item is true:

- [ ] Every winning official-profile TES4 PACK and PGRD is loaded by stable key,
      associated correctly, included in a reviewed count/fingerprint lock, and
      has no unreviewed routing/package skip.
- [ ] All package types and condition functions reachable from official winning
      actor lists have implemented, tested semantics or an honest typed later-
      milestone action boundary that does not fake completion.
- [ ] TES4 NPCs and creatures are real mechanics actors sharing the M11 render,
      M12 stats/formulas, and M13 inventory/equipment authorities.
- [ ] TES4 pathgrids, foreign links, object links, overlays, and Recast/Detour
      integration pass synthetic, lifecycle, and real-cell checks.
- [ ] Schedules and package priority are deterministic across FPS, timescale,
      wait/rest, cell loading, and save/load.
- [ ] High and low processing preserve the same package intent and actor route
      without stalls or wall-crossing teleports.
- [ ] Wander, Travel, Follow, Escort, Eat, Sleep, Use Item At, Flee, Pursue,
      dialogue approach, companion travel, and horse behavior pass unit,
      integration, interruption, persistence, and applicable real-data tests.
- [ ] Detection has exact fixed probes, monotonic property coverage, real LOS,
      and consistent script/condition/package results.
- [ ] Every M14-owned ObScript placeholder is removed and the official script
      corpus plus live runtime contains no M14 deferred trace.
- [ ] Runtime schema v5 has strict C++/Python parity, v1--v4 migration, content
      reorder behavior, corruption rejection, and save/reload coverage in each
      significant package phase.
- [ ] Tutorial escorts and the representative 24-hour city population complete
      their expected package/cell transitions with no package-order error,
      repeated door edge, unbounded repath, or teleport loop.
- [ ] Companion, obstruction, detection, horse, and Morrowind regression
      scenarios pass; required visuals have been inspected.
- [ ] Full component, engine, Python, sanitizer, audit, and end-to-end suites are
      green from a clean tree, and `git diff --check` passes.
- [ ] The final M14 report names implementation revision, exact content profile,
      test totals, durable evidence paths, known later-milestone boundaries, and
      interactive reproduction commands.
- [ ] Only after all preceding checks, M14 is marked `accepted` and
      `next_bounded_delivery` moves to M15.

No checkbox is retained merely because corresponding code exists or a weak
smoke scenario passed. Parser/audit code, native runtime services, persistence,
and focused pure tests provide a useful candidate baseline, but their complete
criteria must be reverified as the step-by-step implementation resumes.

## 20. Practical handoff notes

- Begin with Phase 0 and commit Phase 1 before attempting visible NPC movement.
  Otherwise parser/store uncertainty will be misdiagnosed as pathfinding bugs.
- Prefer pure component APIs and stable `FormKey` state. Any new use of a raw
  load-order FormID, `Ptr`, record pointer, or unordered iteration in saved AI
  decisions deserves immediate scrutiny.
- Keep a live unknowns table in the eventual M14 report. Resolve entries through
  binary fixtures, official-content distributions, existing engine behavior,
  and deterministic runtime observations; do not hide them with permissive
  fallbacks.
- When a real scenario fails, first inspect the package/phase/route event stream,
  then the saved actor state, then the nav debug view. Visual symptoms alone are
  usually downstream of an identity, selection, or transition error.
- Commit generated count-locks and scenario manifests, but not `build/` logs,
  saves, screenshots, or local content. The final report references those local
  evidence paths in the same manner as M10--M13.
