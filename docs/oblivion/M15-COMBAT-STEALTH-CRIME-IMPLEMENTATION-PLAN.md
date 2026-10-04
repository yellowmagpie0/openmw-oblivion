# M15 combat, stealth, crime, and jail: staged implementation plan

Status: **planned; implementation and acceptance have not been performed**.
Prepared on 2026-09-11 against source revision
`95479d424786ff2d51be540b1121b99da65ea154`, after the M14 acceptance
declaration. This is an executable handoff for a subsequent implementation
agent, not evidence that M15 works.

The authoritative scope is [M15 in the roadmap](../OBLIVION-COMPATIBILITY-ROADMAP.md):
melee, ranged combat, blocking, armor/damage, stagger, knockdown, weapon
condition, projectiles, creature attacks, death/ragdoll/loot, sneak,
pickpocket, trespass, theft, assault, murder, bounty, guards, surrender, and
jail. Acceptance requires **tutorial combat, a representative dungeon, an
Arena match, and every crime-resolution path through normal gameplay, with
correct persistence**.

## 1. How to execute this plan

Read sections 1-7 before editing. Work through stages S0-S14 in order; each
stage has a bounded deliverable and its own stopping point. Do not implement
the whole milestone in one change. A stage may have several small commits,
but its closing commit must include its tests and current handoff record.

At the start of each stage:

1. Read this plan, `AGENTS.md`, the current milestone report, and the preceding
   stage's evidence. Inspect `git status --short`; preserve unrelated changes.
2. Confirm the current revision and recheck the named code boundaries.
   Paths below are navigation aids, not permission to trust obsolete behavior.
3. Enumerate the stage's requirements as individually identified test cases.
   Every requirement needs an implementation owner, test level, oracle,
   expected result, and generated evidence path.
4. Write the smallest failing test/reproducer for the behavior before fixing
   it. For wholly new behavior, demonstrate that the unimplemented baseline
   fails the new assertion, rather than silently skipping the test.
5. Implement the stage, run its gates, investigate every failure, and review
   the actual state/images. A runnable binary alone is not a stage pass.
6. Commit the source, hermetic fixtures, tests, and report update together.
   Record the tested implementation revision or exact pre-commit tree/diff
   hash, then record the resulting commit in the next handoff update.

Create `docs/oblivion/M15-COMBAT-STEALTH-CRIME.md` in S0 as the durable report.
Keep an explicit stage table with `pending`, `in-progress`, `blocked`, and
`passed`, implementation revision, commands, case counts, failures, evidence
paths, and the next bounded task. Do not erase failed attempts or relabel
diagnostic evidence as acceptance evidence. Generated logs, game data, saves,
captures, and reports belong under
`build/oblivion-compat/m15/<stage>/<run-id>/`, never in Git.

**Stopping rule:** do not close a stage while any required case is missing,
skipped, unsupported, flaky, or failing. If a later milestone blocks an
acceptance flow, record the blocker and implement only its justified narrow
prerequisite; never bypass the flow to call M15 complete. Completed earlier
stages remain useful and independently reviewable.

### 1.1 Scope boundaries and decisions

| Boundary | Required decision |
| --- | --- |
| M14 | Reuse native navigation, package scheduling, detection, companions, horses, and stable actor identity. Add combat/pursuit interruptions without starting a competing TES3 AI sequence. |
| M16 | Full magic, active effects, poisons, diseases, enchantment execution, skill-use progression, and leveling remain M16. Define typed damage/effect integration points now. Never silently count an enchantment or spell as executed. Combat mastery abilities at existing skill values, including any necessary narrow effect prerequisite, belong to M15; earning those values through general progression remains M16. Implement the small jail skill-penalty operation against existing actor values in M15; it is not a general leveling system. |
| M17 | The current roadmap assigns dialogue, voice, persuasion, and service integration to M17. Some old M14 prose incorrectly assigns dialogue to M18. Use the roadmap numbering. M15 still needs a working, normal-input arrest/surrender interface and the minimal real dialogue/result paths required by its campaigns. |
| M18 | General quest/objective/journal work remains M18. Existing ObScript quest state is reusable, but manually setting quest stages is not a substitute for Arena or tutorial progression. |
| M19 | Full Oblivion UI parity remains M19. A functional, accessible combat HUD, sneak indication, pickpocket interaction, arrest choices, jail/rest choice, and death/load flow are necessary now. Shared presentation is acceptable only if it invokes real native actions and exposes their results. |
| Mounted attacks | Target original Oblivion 1.2.0416, not Skyrim or a mounted-combat mod. Establish and test vanilla attack restrictions while riding. The M14 statement that M15 owns mounted combat means M15 owns this policy, incoming damage, dismount interruption, and horse/rider death handling; it is not a mandate to invent player attacks from horseback. |
| Crime geography | Determine native bounty/identity/faction rules from TES4 data and probes. Do not import Skyrim's per-hold bounties or Morrowind's crime semantics merely because similarly named APIs exist. |
| Original-game oracle | The roadmap's universal verification requires independent original-game behavioral/visual probes. This plan follows that requirement for M15; M14's local prohibition on Proton workflows is not copied forward. Original-game probes are isolated reference runs, never a replacement for OpenMW E2E. If unavailable, record an open oracle gate, not an automatic waiver. |

Do not require full later milestones just to begin M15. Nonmagical duels,
synthetic crime courses, and staged jail interactions provide useful early
deliverables. However, select final official-content campaigns in S0, discover
their dependencies early, and keep their full success criteria intact.

## 2. Current source baseline: inspect these boundaries first

The following facts were checked in the source revision above. Older M14
documents contain superseded findings and provisional test counts. The M14
acceptance ledger is context, not proof that an M15-specific path works.

| Surface | Existing implementation and M15 consequence |
| --- | --- |
| Native actor classes | `apps/openmw/mwclass/esm4npc.hpp/.cpp` now derives through `Actor`, exposes `CreatureStats`, `NpcStats`, movement, inventory, armor rating, essential status, and corpse activation. It does not declare native `hit`/`onHit` overrides. Trace inherited behavior before connecting damage. Locate the creature implementation from its registered class, not an assumed `esm4creature.cpp` filename. |
| Actor update | `apps/openmw/mwmechanics/actors.cpp` routes native NPCs to `OblivionAiService` and excludes them from the old engagement, crime pursuit, and `AiSequence::execute` block. Other nearby code still reads `AiSequence::isInCombat`. Replace native decisions at reviewed boundaries; do not simply remove the isolation guard. |
| Native AI | `apps/openmw/mwmechanics/oblivionai.hpp/.cpp` owns packages, routes, high/low processing, detection, mounts, and capture/restore. `Ambush` currently ends at the explicit M15 action boundary. Combat needs a real consumer, and ordinary schedules must resume after combat. |
| Shared combat | `apps/openmw/mwmechanics/combat.cpp`, `character.cpp`, `mechanicsmanagerimp.cpp`, and `pickpocket.cpp` contain existing TES3-shaped formulas and behavior. Reuse motion/physics primitives where valid, not Morrowind hit-chance, blocking, hand-to-hand, pickpocket, or crime rules by accident. |
| Equipment | `apps/openmw/mwworld/oblivionprofileservices.hpp/.cpp` provides native item definitions and a narrow shared-item projection. `components/esm4/inventorymechanics.*`, shared inventory/container stores, and M13 interactions already own stacks, equipment, condition, charge, and stolen ownership. Extend that bridge instead of introducing another mutable inventory. |
| Projectile chain | `apps/openmw/mwworld/projectilemanager.cpp`, `apps/openmw/mwphysics/projectile.cpp`, and projectile collision callbacks lead into shared hit handling. Native projectile identity, damage dispatch, collision, and save restoration must be designed together. |
| Records | NPC/creature records retain combat-style references, faction data, flags, and death items. `CSTY` is a `RawRecord` in `apps/esmtool/tes4.cpp`; there is no typed `components/esm4/loadcsty.cpp` at this baseline. Lossless parsing is not semantic combat-style support. |
| Script placeholders | In `apps/openmw/mwworld/oblivionscriptmanager.cpp`, `StartCombat`, `StopCombat`, `SetCrimeGold`, `SetUnconscious`, and `SetEssential` are deferred. `IsInCombat` and `IsPCAMurderer` return zero. `Kill`/`Resurrect` and `GetDead` use `obscript.dead` custom state, not a demonstrated physical death lifecycle. |
| Related script state | `IsPlayerInJail` reads a world flag; infamy/faction crime queries use player actor-value entries. Ownership/faction commands are also deferred. Audit each relevant command and condition, including aliases and event contexts, rather than implementing only this list. |
| Persistence | `components/esm4/runtimestate.hpp` has `CurrentRuntimeStateVersion = 7`, not 5. The state contains references, inventory, player values, AI, mounts, detection, and pending package callbacks, but no complete typed M15 combat/crime/jail ledger. Extend binary, canonical JSON, world restore, and `scripts/tes4_runtime_state.py` in lockstep. |
| Legacy jail | `apps/openmw/mwgui/jailscreen.cpp` uses the old prison-marker/rest path. Its existence is not proof of TES4 arrest, confiscation, jail geography, skill penalties, or release. Trace and profile-gate its actions. |
| Existing tests | Component tests are in `apps/components_tests/esm4/`; native engine tests include `apps/openmw_tests/mwworld/testoblivionai.cpp` and `testoblivionprofileservices.cpp`. There is no existing `apps/openmw_tests/mwmechanics/` test directory to assume. |

Useful existing suites include `ESM4RuntimeState.*`,
`ESM4PlayerMechanics.*`, `ESM4InventoryMechanics.*`, `ESM4Detection.*`,
`OblivionAiTest.*`, and `OblivionProfileServicesTest.*`. List the binaries'
actual tests before relying on any filter. Some engine-target M14 tests test
pure helpers; their location does not make them world/physics integration
tests.

## 3. Required architecture and state ownership

### 3.1 One simulation authority, multiple adapters

Use a native profile-owned combat/crime service or a small coordinated set of
services. Suggested new files are
`apps/openmw/mwmechanics/oblivioncombat.*` and `oblivioncrime.*`; these are
**proposed**, not existing APIs. Keep pure record interpretation and rules in
`components/esm4`, following the current player/inventory/AI conventions.
Do not turn `OblivionScriptManager` into a second mechanics implementation.

The required flow is:

```text
normal input / native AI / legitimate ObScript command
    -> validated action intent and stable action ID
    -> native actor/controller animation or interaction
    -> authoritative contact/release/transfer/arrest transition
    -> native rule evaluation and single committed state change
    -> ordered script events, audio, UI, and telemetry
    -> capture/restore of that same authoritative state
```

Define interfaces before expanding call sites:

- Combat intent: source, target if known, equipped instance, attack mode,
  directional power attack, aim, input time, cancellation, and action ID.
- Hit request: attacker, victim, source item/projectile, contact position,
  physical damage type, blocked/sneak context, and causative attack ID.
- Hit outcome: accepted/rejected reason, pre/post health and fatigue, armor
  and block contributions, condition changes, stagger/knockdown/death result.
- Crime request: perpetrator, victim/owner, affected item/reference, offense,
  value/count, location, legality context, and causative gameplay action.
- Crime outcome: witness decisions, report state, bounty/infamy/faction deltas,
  lawful-combat exceptions, guard response, and persistent incident ID.
- Arrest/jail transaction: chosen resolution, authority, assessed fine,
  confiscation, sentence, destination, release/escape status, and commit ID.

Use explicit invalid-target/unsupported-data errors consistent with the
existing typed diagnostics. Do not return success after failing to resolve a
FormKey or silently fall back to TES3 formulas.

### 3.2 Authoritative state and save contract

Inventory/condition stays in the M13 inventory authority. Health, fatigue,
skills, death, and essential state must have one live authority with a
documented native save projection. Bounty must not diverge between `NpcStats`,
player actor-value maps, world flags, Lua, UI, and the new service. Define
read-through adapters and a single mutation entry point.

Persist, or explicitly prove reconstructible, all of the following:

| State group | Required contents and invariant |
| --- | --- |
| Actor combat | Stable actor/target identities, engagement/legal context, attack phase and remaining timing where resumable, pending hit identity, block, health/fatigue, stagger/knockdown/unconscious recovery, essential/dead state, killer/cause. No raw pointers, animation handles, or load-order indices. |
| Projectiles | Stable instance, shooter, source equipment/ammo, launch properties, transform/velocity, lifetime, collision/impact state, damage context. An already spent arrow cannot hit again after load. |
| Death/corpse | Terminal versus essential knockout, death event consumption, death-item generation, inventory/loot provenance, corpse pose or deterministic restoration contract, activation eligibility. |
| Crime | Monotonic incident identity, action deduplication, ownership/legal context, witness/report state, bounty and relevant faction/infamy/murder state. Define bounded retention without losing unresolved offenses. |
| Arrest | Pursuing authority and incident, dialogue/choice transaction, fine/confiscation committed flags, pending transition, valid cancellation state. |
| Jail | Prison/evidence/belongings/release references, sentence start and remaining time, original property metadata, retained quest items, served/escaped state, applied skill/time penalties, release transaction state. |
| Event/RNG | Dedicated reproducible RNG state or specified deterministic draws; pending callbacks with FIFO order and consumed IDs. Saving inside a callback must not replay it or discard the remaining queue. |

Declare exact save boundaries. Prefer a committed simulation tick, flushing
pending work consistently, over capturing half a transfer or half an impact.
If saving is temporarily prohibited during an uninterruptible transition, the
normal UI must expose that restriction, and a test must prove no partial save
is written. Do not ban all combat/jail saves as a shortcut.

Bump the current runtime schema from the actual version at implementation
time. Preserve migration from every supported old version, including the
baseline version 7. Reconcile legacy `obscript.dead` state deliberately;
conflicting old/custom/native fields require a documented rule and fixture.
Validate sizes, finite numbers, enums, nonnegative bounded quantities,
duplicate identities, dangling references, and transaction consistency before
mutating the world. A failed load must leave the prior world intact.

### 3.3 Native rules and independent expected values

Do not write formulas by guessing or copying `combat.cpp`. Create a compact,
source-controlled rule/probe table with provenance, units, rounding, clamp
order, applicable game version, and independent expected values. Keep
proprietary scripts/data/captures outside Git.

Required rule families:

- Weapon/hand-to-hand/creature base damage, skills/attributes, fatigue,
  difficulty, normal versus power attacks, directional attacks, weapon reach
  and timing, condition (including above-normal repaired condition).
- Shield/weapon/unarmed blocking, angle and timing, block skill, armor rating
  and caps, armor skill/condition, durability distribution, fatigue costs.
- Combat mastery abilities for Blade, Blunt, Hand to Hand, Block, Marksman,
  Sneak, and armor skills, including native unlock thresholds and equipment/
  posture requirements. Audit disarm, knockdown/paralysis, defensive abilities,
  and sneak armor bypass rather than assuming directional attacks alone cover
  mastery. Existing Armorer/condition behavior remains connected to M13.
- Stagger, knockdown, fatigue knockout, essential unconscious recovery, and
  normal death. Explicitly distinguish Oblivion hand-to-hand from Morrowind.
- Bow draw/release, ammo damage, speed/gravity/range, recovery, and collision.
- Sneak attack eligibility and multipliers; visibility, sound, motion, light,
  distance, skill, armor/footwear, observer awareness, and relevant existing
  modifiers. Do not implement a second detector.
- Pickpocket attempt/exit/failure and prohibited items; ownership/rank/public
  access; trespass warning/report timing; theft value; assault/murder legality;
  witnessed/unwitnessed reporting; fines, surrender, confiscation, sentence,
  skill penalties, release, escape, and identity-specific crime exceptions.

For each continuous rule test zero, one, near-threshold values, threshold
minus/at/plus one representable step, normal values, and extremes. For each
probabilistic rule test deterministic forced samples on both sides of the
threshold plus a fixed-seed distribution check with a predeclared tolerance.
Do not use a statistical test alone to establish a specific branch.

Expected values must be hand-derived from reviewed rules, captured from an
independent probe, or generated by an independent reference implementation.
Calling production code to generate its own expected output is forbidden.
Unknown formula semantics are an open case, not a magic constant to tune
until one screenshot looks plausible.

Give every audited mastery ability a row: skill, threshold, prerequisites,
normal input, probability/draw, consequence, duration if any, and save fields.
Test just below/at/above the unlock value and deterministic proc/non-proc
samples. Test disarmed items as real dropped/recoverable inventory, not a
boolean animation flag. If an ability requires paralysis or another M16-shaped
effect, implement its narrow real state/restore/controller semantics now or
keep the ability's gate blocked; relabeling it as stagger or omitting it because
leveling is later is not acceptable. Do not implement unrelated spells.

## 4. Verification contract: no success-shaped shortcuts

### 4.1 Test levels

| Level | What must really execute | What it cannot prove alone |
| --- | --- | --- |
| L0: data/parser | Synthetic TES4 records, overrides/deletions, malformed variants, real winning-record audit. | A parsed combat style does not make an actor fight. |
| L1: pure rules | Explicit input/output tables, state-machine transitions, conservation properties, deterministic RNG. | A damage helper does not prove input, animation, contact, or death. |
| L2: service/world integration | Real native stores, actor instances, inventory, world/service adapters, event dispatch, and serialization. Use fakes only at external boundaries, with recorded calls. | A helper in `openmw-tests` is not automatically integration coverage. |
| L3: engine/render/physics | Actual `openmw`, input/controller, animation keys, physics contacts/projectiles, menus, sound, and save files. Hermetic authored fixtures allowed. | Direct service calls or a pre-dead target do not prove a normal-input duel. |
| L4: real-content E2E | Original installed TES4 content, normal actions and real scripts/doors/dialogue, explicit campaign outcomes, fresh-process continuation. | A boot, timed walk, generic log match, or existence of a save does not establish gameplay success. |
| L5: cross-cutting | Full suites, compilers/configurations, sanitizers, lossless official census, Morrowind regression, performance, visual/audio review. | Passing the two aggregate CTest entries does not imply OpenCS or external integration coverage. |

Every gameplay stage requires L1/L2 plus an L3 vertical slice before closure.
L0 applies whenever records/save formats change. Early L3 slices can be
synthetic and narrowly scoped; L4 is not deferred discovery, because S0 and
S12 must expose official-content dependencies before final campaigns.

### 4.2 Setup versus acceptance actions

Give manifests explicit phases: `setup`, `exercise`, `observe`, and
`restart-continuation` (new harness contract to implement in S1).
Setup may define a synthetic fixture, deterministic seed/clock/weather,
starting location/equipment/stats, and a documented initial save.

Once `exercise` starts, permit only ordinary input and read-only observation.
The engine may perform its normal transitions, including jail transfer;
the harness may not cause the result directly.

Forbidden during exercise:

- Console `Kill`, `Resurrect`, `SetAV`, `ModAV`, `SetCrimeGold`, `SetStage`,
  faction/ownership mutation, `StartCombat`, `StopCombat`, `coc`, position
  writes, item grants/removals, forced animation completion, or AI phase writes.
- Runtime-state `mutate`, patched saves/plugins, generated event logs, synthetic
  script-event injection, or a subprocess that writes expected outcome files.
- Hidden mutation through generic `command`, `type`, environment-driven event
  files, or an observer endpoint. An action-name denylist alone is insufficient.
- Immortal opponents, disabled AI, infinite resources, no collision, or forced
  favorable RNG used to make an official campaign pass.
- Starting the Arena with its match already awarded, starting jail with a
  fabricated served sentence, or using `Kill` to manufacture a corpse.

An isolated ObScript integration test may call `StartCombat` or `Kill` to test
those commands; it must be labeled as such and cannot stand in for the
normal-input combat campaign. Authored original scripts legitimately calling
commands during gameplay are allowed, with script/unit provenance recorded.
Do not reject a real quest's `SetStage` merely because harness stage writes
are forbidden.

Campaign starting saves must have reproducible provenance from normal
gameplay and no pre-satisfied target outcome. A subsystem fixture save can
arrange initial conditions but is not campaign progression evidence.

### 4.3 Causal assertions and independent observations

Extend the existing M14 event/checkpoint design; do not just add regexes.
Every relevant event must carry a run ID, process/reload epoch, monotonic
sequence, simulation tick, stable actor/target keys, attack/projectile/incident
or transaction ID, cause, and result. Include witness and item-instance
identity where applicable.

Examples of required causal chains:

```text
input attack -> attack phase -> contact with named victim
  -> damage/condition deltas -> stagger or death -> script event -> loot

input release -> one ammo debit -> projectile launch -> geometric contact
  -> one victim outcome -> spent/recoverable arrow -> no duplicate after load

input take -> ownership/legality -> named observer detects offense
  -> incident reported -> exact bounty delta -> guard pursues
  -> ordinary surrender choice -> fine/jail transaction -> resumed world
```

Cross-check events against independently captured live values and parsed
saves. Telemetry claiming `damage=10` cannot pass if health is unchanged.
Compare the actual named victim; another actor's death must not satisfy it.
Use actor-relative deltas, not global event totals.

Fatal events and transaction boundaries must be lossless. If routine telemetry
is sampled, retain occurrence counters and a closing summary; validate counts
across reload epochs without double counting. Sampled aggregate counts cannot
satisfy an ordered causal-chain assertion. Missing files, missing fields,
empty match sets, wrong actors, duplicates, stale files, incomplete queues,
and truncated streams must fail closed.

### 4.4 Mandatory negative controls

S1 must run these against the verifier, and each feature stage adds its own:

1. No events, wrong actor/target/incident, wrong order, duplicate hit, and
   identical events from a previous run all fail the intended assertion.
2. Correct telemetry with unchanged live health/bounty/inventory fails.
3. A replaced or edited save, stale screenshot, missing final summary, and
   early clean exit fail.
4. A scripted state write hidden in a generic input/subprocess action fails
   campaign validation; legitimate original-script execution remains allowed.
5. An engine error, unsupported reachable command, sanitizer diagnostic, missing
   required capture, or no-sound invocation in an audio case fails.
6. A deliberately missing test filter and skipped/disabled required test cannot
   be reported as a pass.
7. The harmless counterpart succeeds with no damage/crime: miss, blocked
   projectile, authorized transfer, unseen theft where no report is expected,
   or ordinary lawful interaction as appropriate.

Use temporary copies of evidence for corruptions; never alter the original
successful run or acceptance expectations to manufacture a pass.

### 4.5 Timing, retries, randomness, and tolerances

Freeze resolution, camera, graphics, seed, clock, weather, initial actor/item
state, input bindings, and simulated step policy per case. Wait for explicit
state/event acknowledgements with bounded deadlines, not unexplained long
sleeps. Derive deadlines from route length, animation duration, and measured
baseline plus documented slack.

Run deterministic synthetic courses with at least three fixed seeds and
30/60/120 FPS caps for timing-sensitive combat. Compare outcome semantics,
not renderer timestamps or bitwise ragdoll trajectories. Run final campaigns
twice from independent clean outputs with the same seed and once with a second
declared seed; all must meet the same success criteria.

Never retry until green. Preserve every run and classify any nondeterministic
failure. A retry after a demonstrated external infrastructure failure is a new
run with the original failure retained. Never increase a timeout/failure
budget, lower a damage threshold, shrink the actor set, or change a golden
merely to accept the candidate.

## 5. Stages and dependencies

The default implementation order is linear to keep a less capable agent from
building incompatible authorities in parallel. S8's detector/rule work could
be developed independently after S3, but integration still follows this order.

| Stage | Deliverable | Depends on | Runnable checkpoint |
| --- | --- | --- | --- |
| S0 | Baseline, case inventory, campaign dependency audit | Accepted M14 source | Existing native and Morrowind baselines reproducible |
| S1 | Fail-closed M15 harness and evidence model | S0 | Synthetic no-op/mutation negative-control course |
| S2 | Native data and reviewed physical/crime rule inputs | S1 | Content audit and rule matrix |
| S3 | Services, actor bridge, versioned persistent state | S2 | Save/restart of idle native services without regression |
| S4 | Melee, block, mitigation, condition, reactions, minimal opponent attack policy | S3 | Normal-input nonlethal melee course |
| S5 | Bow/projectile lifecycle | S4 | Normal-input ranged course with in-flight restart |
| S6 | Death, essential knockout, ragdoll, loot | S5 | Kill/loot and essential-recovery courses |
| S7 | Native combat AI and interruption/resumption | S6 | Autonomous duel, ambush, flee, companion course |
| S8 | Sneak attacks and pickpocket | S7 | Paired hidden/detected stealth course |
| S9 | Ownership, offenses, witnesses, bounty | S8 | Real actions produce exact lawful/criminal outcomes |
| S10 | Guards, surrender, fines, resisting arrest | S9 | Guard reaches player; each available choice works |
| S11 | Jail, confiscation, sentence, release, escape | S10 | Serve and escape branches survive restart |
| S12 | Complete script bindings and campaign prerequisites | S11 | Reachability audit and normal entry into each campaign |
| S13 | Official tutorial/dungeon/Arena/crime campaigns | S12 | Every L4 outcome and continuation demonstrated |
| S14 | Full acceptance and durable report | S13 | Universal gates and final acceptance ledger |

### S0 - Establish a trustworthy baseline and inventory

**Changes:** documentation/test metadata only, except narrowly justified
baseline observability needed to identify cases.

1. Record exact source revision, clean/dirty state, build cache/compiler
   identities, enabled targets, graphics/audio backend, data paths and content
   fingerprints. The checked baseline uses a RelWithDebInfo `build/` and
   bundled double-precision Bullet; do not overwrite a user's build setup.
2. Run the existing focused native suites, Python suite, M14 audit, and selected
   M14/M13/Morrowind scenarios with fresh outputs. Inventory full-suite
   availability and establish final acceptance build directories.
3. Inspect winning `CSTY`, actor/equipment/GMST/faction/ownership records,
   prison/evidence/release markers, tutorial actors, candidate dungeon, Arena
   scripts and dialogue. Use TES4 readers/audits, not the TES3-only MCP author.
4. Select exact official campaign start/end points and named actor/reference
   FormKeys. A practical dungeon candidate is Vilverin, but inspect its actual
   dependency chain before locking it. Do not invent IDs from memory.
5. Trace the entire Arena entry -> equipment/rules -> match activation ->
   opponent engagement -> victory -> return/reward chain. Trace tutorial
   progression to combat and jail entry/release. List reachable missing
   commands, dialogue choices, magic dependencies, and quest events.
6. Lock requirement IDs and the branch matrix in sections 8-9. Start the
   independent formula/original-game probe inventory. Record the local
   Shivering Isles stub limitation if still present; zero records in a stub
   is not expansion coverage.

**Tests/gate:** fresh baseline logs and test inventory exist; each final flow
has exact targets and success predicates; environmental and preexisting
failures are classified. A preexisting failure is not silently ignored:
retain its reproduction and use a matched baseline/candidate comparison.

**Stop:** do not start combat implementation with an unspecified Arena/dungeon
or assume future dialogue work will automatically make acceptance possible.
Suggested commit: `Document M15 baseline and acceptance case inventory`.

### S1 - Build the evidence harness before feature success

**Changes:** extend `scripts/oblivion_compat.py`,
`scripts/tes4_runtime_state.py`, `scripts/tests/`, and
`scripts/data/oblivion_compat/scenario.schema.json`; add minimal read-only
engine observation plumbing. Preserve existing M14 manifest behavior.

1. Add a typed M15 manifest section and actor/target/incident-scoped assertions
   implementing section 4. Keep schema and actual runner validation aligned.
   Validate nested fields/discriminants as well as the top-level JSON shape.
2. Add setup/exercise separation, run/epoch freshness, checksummed initial
   inputs, read-only snapshots, and explicit action provenance.
3. In M15 exercise mode, reject generic subprocess execution and console input;
   expose allowlisted read-only operations instead. Prevent scenario-provided
   event files from injecting script results. Do not merely rename M14
   controls or rely on manifest authors promising not to cheat.
4. Add canonical state-subset comparisons with explicit field paths and
   allowed tolerances. Missing required values fail, even when a default
   would happen to satisfy the expected result.
5. Add evidence aggregation with one result per case, required artifact list,
   actual test counts, unsupported/error counts, and a nonzero failure exit.
   Missing or skipped cases block overall acceptance.
6. Provide a two-process save/restart driver using existing scenario
   launches and the normal load interface, with separate user-data outputs.
   Record PID/process epochs, save digest, load acknowledgment, and resave.

**Tests:** Python unit tests cover every negative control in 4.4, schema/runner
agreement, path containment, actor scoping, counter/epoch boundaries, and
comparison exclusions. Launch a tiny engine fixture to prove observation does
not change authoritative state. Run a bad manifest and confirm nonzero exit.

**Gate:** an empty world, fake success log, or scripted outcome cannot pass the
new M15 course. Existing harness tests and chosen M14 scenarios still pass.
Suggested commit: `Add fail-closed M15 scenario evidence and controls`.

### S2 - Decode native data and lock rule inputs

**Changes:** typed combat-style reader/store and rule definitions; extend
existing native records only where inspection proves missing semantics.

1. Add TES4 `CSTY` types/reader using the current raw payload as lossless
   reference. Validate exact TES4 subrecord lengths, optional/default fields,
   flag domains, and later-game version separation. Register the type through
   record includes, winning stores, loader dispatch, `esmtool`, FormKey
   resolution, and the semantic audit. Audit raw-record consumers before
   removing their CSTY path.
2. Inventory native GMSTs with value types, faction crime/relationship flags,
   actor aggression/confidence/responsibility, essential/respawn flags,
   creature attack data, equipment/ammunition, sound/animation families, and
   jail-related references. Resolve owners and references after overrides.
3. Implement the pure rules from section 3.3 incrementally, with immutable
   input/output structures; do not couple them to world singletons.
4. Define missing-CSTY/default behavior only from verified native semantics.
   Invalid references or missing required settings must diagnose explicitly.
5. Add a semantic M15 audit and reviewed count lock tied to content hashes.
   New `m15-audit` support is a proposed deliverable, not a current command.
   Explain each exception; raw preservation alone cannot classify behavior
   as implemented.

**Tests:** binary round trips where supported, truncated/oversized fields,
NaN/invalid enum/counts, compressed records, overrides/deletions, multiple
masters, missing referenced style, style variants, formula tables and boundary
properties. Run affected parsers under ASan/UBSan. Audit all installed official
plugins, including DLC, and independently inspect representative decoded
records against raw payloads.

**Gate:** all selected official actors have a resolved, explained physical
combat policy; formulas have reviewed expected values. Unresolved magic
semantics remain explicitly M16-owned, not neutralized.
Suggested commit: `Decode TES4 combat data and add native rule matrices`.

### S3 - Wire services, actor state, and persistence

**Changes:** native service lifecycle/profile wiring, state types, world
capture/restore, binary/JSON/Python readers, tests.

1. Document the authority map from section 3 and trace the actual player,
   native NPC, creature, projected equipment, and unloaded-reference paths.
2. Construct services only for Oblivion; clear them on new game, load failure,
   world clear, and profile teardown. Resolve stable actor keys, including the
   player reference alias, through the existing native bridge.
3. Add the versioned state skeleton and strict validators. Native stat writes,
   shared class reads, UI queries, and saves must agree immediately.
4. Define queue ordering and transaction boundaries before implementing hits.
   Load into validated temporary state, then commit; cancel or rebuild
   ephemeral handles without replaying committed actions.
5. Add public queries needed by controllers and scripts, so later stages do
   not inspect private maps or rely on `AiSequence::isInCombat`.

**Tests:** actual native actor instances read/write the same stats through
each adapter; create/clear/load cycles; migration of all supported versions;
old `obscript.dead` conflict cases; corrupt/truncated/foreign-profile saves;
missing/changed content; plugin reordering; duplicate/dangling state.
Perform an actual engine save -> quit -> fresh load -> resave of idle services.

**Gate:** old accepted native saves load deliberately, Morrowind saves contain
no M15/T4ST state, and no new state exists only in telemetry.
Suggested commit: `Add persistent native combat and crime service contracts`.

### S4 - Deliver a normal-input melee slice

**Changes:** input/controller/native class contact dispatch, block/damage,
condition, reactions, minimal live HUD/audio.

1. Trace attack/block input through control switches, Lua player-control
   integration, `CharacterController`, animation text keys, hit acquisition,
   and class/mechanics dispatch for player, NPC, and creature.
2. Implement blade/blunt/hand-to-hand/creature strikes, normal and directional
   power attacks, reach and contact validity. A geometric hit must not miss
   because a TES3 chance roll happened to fail.
3. Implement block intent, valid equipment/posture/angle/time, mitigation,
   armor/condition, fatigue, hit recovery, stagger and knockdown. Resolve one
   contact only once per intended hit window, including multiple frames.
4. Cancel safely on weapon change/breakage, unequip, menu/control lock,
   stagger/knockdown, death, load, and mount transitions. Do not leave a stuck
   attack or held block after release.
5. Emit hit/miss/block/swing sound through M10 sound paths and animate the
   appropriate M11 skeleton. Do not just set a hit-reaction flag.
6. Wire related command/query paths that now have real semantics; leave no
   parallel script-only damage state.
7. Add the smallest real native opponent policy needed for this stage: an
   in-range stationary opponent chooses a basic attack through the production
   intent/controller/contact path. It can use an authored deterministic
   synthetic combat style, but cannot inject damage, contacts or outcomes.
   This is the first slice of the S7 policy, not a separate test-only fighter.
   S7 adds movement, full style decisions, target acquisition and interruptions.
8. Implement the melee/block/armor mastery rows from S2, including dropped-item
   consequences and narrowly required combat-triggered effect semantics.

**Tests:** all melee rows in section 8; native integration verifies exact
pre/post health, fatigue, armor and weapon condition; L3 player attacks a
named nonlethal target using normal input, blocks its attack, misses outside
reach/behind a wall, and resumes after stagger. The opponent uses the minimal
production policy above, so this gate does not depend on completing S7.
Repeat first/third person,
keyboard/mouse and gamepad, plus FPS cases. Save/restart during windup,
blocking, and recovery; compare the continuation with uninterrupted runs.

**Gate:** input -> animation contact -> one native delta -> visible/audible
reaction is proven. A direct call to the rule function is not enough.
Suggested commit: `Implement native Oblivion melee and blocking`.

### S5 - Deliver ranged combat and projectile persistence

**Changes:** bow draw/release and ammunition, native projectile lifecycle,
collision/hit dispatch, arrow recovery.

1. Use existing projectile/physics/render plumbing with native identity and
   rule dispatch. Do not represent TES4 ammunition through an unreviewed
   TES3 crossbow/bolt rule.
2. Debit one correctly identified Player ammo instance at successful release,
   not at draw start. Preserve NPC/Creature ammunition and god-mode Player
   quantities according to the verified original actor dispatch (checkpoint270
   in the provenance report). Define cancel/no-ammo/broken-bow behavior and
   equipment changes.
3. Handle launch transforms, moving shooters/targets, gravity, collision with
   walls/doors/terrain/water as applicable, expiration, stuck/recoverable
   arrows, and shooter self-collision avoidance.
4. Use continuous/swept collision as required; prove fast arrows do not tunnel.
   A projectile hits at most once unless a separately verified behavior says
   otherwise. Armor, block, sneak context and crime receive the same impact.
5. Capture/restore in-flight and spent states without spawning duplicate
   arrows, debiting ammo again, or applying a previous hit again.
6. Complete the Marksman mastery rows, including unlock and proc/non-proc
   branches, real reactions/effects and their saved continuation.

**Tests:** analytic trajectory cases with declared units/tolerances; near/far,
stationary/moving, thin-wall, edge-contact, high-speed, blocked and missed
targets; repeated draw/cancel/release; exact ammo conservation; two arrows
crossing simultaneously; cell unload/reload and shooter removal.
L3 normal-input shots must hit the named target and fail to hit one behind a
solid wall. Restart with an arrow in flight and compare its terminal result
and inventory against an uninterrupted run.

**Gate:** collision and saved state agree with visible arrows and damage;
neither hit logs nor a missing arrow mesh alone establish impact.
Suggested commit: `Implement TES4 bow projectiles and restart-safe impacts`.

### S6 - Complete death, essential recovery, ragdoll, and loot

**Changes:** single terminal/essential lifecycle; physical corpse realization;
native death events, death items, activation and player death/load.

1. Distinguish health-zero death, fatigue knockout, scripted unconsciousness,
   and essential actor recovery using verified Oblivion semantics. Essential
   actors must not accidentally become ordinary corpses.
2. Cancel attacks/routes/mount attachments correctly, record killer/cause,
   dispatch death/hit callbacks in verified order, and generate death items
   exactly once. Reentrant script changes must not duplicate transitions.
3. Connect animation to newly simulated dynamic ragdolls. Existing authored
   ragdoll poses are not evidence of a death-to-physics transition.
4. Preserve inventory/equipment provenance, allow normal corpse loot, respect
   ownership/quest-item restrictions, and handle unloading/returning.
5. Bind `Kill`, `Resurrect`, `GetDead`, `GetDeadCount`, `SetEssential`, and
   `SetUnconscious` to the same lifecycle where supported; test base/reference
   scope and attribution instead of retaining `obscript.dead` as an authority.
6. Define deterministic respawn/reset and death-item regeneration at legitimate
   reset boundaries. Do not respawn an actor merely because its cell unloads.

**Tests:** NPC/creature/player death; essential recovery; multiple lethal
contacts in one tick; corpse on stairs/slope/doorway; finite ragdoll bounds;
mounted actor/horse death and reciprocal cleanup; script-kill versus normal
hit contexts; resurrection; loot stack conservation; zero duplicate events
or death items across save/restart before and after death/loot. Capture
alive -> impact -> death animation -> settled corpse and inspect every phase.

**Gate:** normal combat creates a physical, persistent, lootable corpse;
essential recovery does not create murder/death loot. Player death exposes a
working normal load flow.
Suggested commit: `Implement native death essential recovery and corpse loot`.

### S7 - Integrate native combat AI with M14

**Changes:** combat decisions over native styles; package interruption and
resumption; coordinated target/movement/block/attack intents.

1. Resolve hostility using native relationships, aggression, confidence,
   responsibility, companions, and combat context. Do not treat all actors as
   hostile or identify guards from display names.
2. Consume the M14 Ambush boundary with a real attack decision. Add acquire,
   approach, attack/block, reposition, flee, search/lost-target, and disengage
   states as justified by native styles.
3. Reuse M14 routes/obstruction/doors; do not move actors by setting their final
   position. Ensure only one service owns locomotion intent each tick.
4. Handle multiple attackers, allies/friendly fire, companions, target death,
   cell transitions, unreachable targets, and resumption of the correct
   interrupted schedule/script package.
5. Define high/low-process combat continuity. Never fabricate an unseen
   victory or silently reset health/targets at demotion. Implement a validated
   low-process policy or a deliberate promotion mechanism with bounded cost;
   cover offscreen encounters and save/return.
6. Enforce vanilla mounted restrictions and incoming-hit/dismount behavior.
   Do not implement M16 `CastMagic` as a fake physical success.

**Tests:** synthetic deterministic duels for melee/archer/creature styles;
ambush, retreat, fleeing opponent, cover/door obstruction, unreachable victim,
three-actor aggro, ally intervention, schedule interruption/resumption,
high/low transitions, target disable/delete/load. Native actors must never
start a parallel TES3 combat sequence. L3 requires AI-generated contact and
damage, not actors walking near each other.

**Gate:** actors can autonomously engage, fight, disengage and resume their
native schedules without teleporting, repeated target loops, or fabricated
package completion.
Suggested commit: `Connect TES4 combat AI to native package processing`.

### S8 - Implement stealth and pickpocket interaction

**Changes:** reuse M14 perception for combat/sneak/crime; native sneak attack
eligibility and pickpocket transactions/UI.

1. Feed live sneaking, motion/noise, light, distance, equipment and observer
   awareness into the existing detector. Characterize any shared detector
   change before editing it.
2. Decide sneak attack eligibility at the correct action/contact boundary;
   one observer detecting the player must not be conflated with every
   victim's awareness. Apply the native weapon/skill rule once.
3. Enter pickpocket through normal sneaking activation of a living actor;
   distinguish it from corpse looting and ordinary container interaction.
4. Implement permitted/forbidden items, stack quantities, value/weight/skill
   chance inputs, success/failure/exit checks, and reverse transfer if native
   behavior supports it. Use the M13 ownership-preserving transaction.
5. Produce a typed crime attempt/result for S9 without inventing bounty here.
   Before S9, the incomplete crime consequence must be explicit; do not label
   the whole stealth/crime scenario accepted.
6. Wire `IsSneaking`, detection and relevant pickpocket/combat queries to live
   state; render correct sneak/detected feedback.
7. Complete the Sneak mastery matrix, including armor-bypass eligibility and
   its interaction with damage calculation rather than only HUD feedback.

**Tests:** bright/dark, moving/still, LOS/occluded, close/far, noisy/quiet,
front/behind, one/multiple observers, asleep/unconscious/dead as applicable.
Paired attacks differ only in awareness and produce exact expected damage.
Pickpocket success, failed attempt, cancellation/exit, prohibited equipped or
quest items, insufficient counts, and save/restart conserve item metadata.
Use forced RNG only in subsystem tests; official L4 runs use declared seeds.

**Gate:** normal sneaking activation and attacks consume the same detector
state seen by scripts and UI. A HUD icon alone cannot prove stealth.
Suggested commit: `Implement native sneak attacks and pickpocket actions`.

### S9 - Implement crimes, witnesses, and persistent bounty

**Changes:** native legality/ownership service, offense transactions, reporting
and crime ledger; connect every relevant normal action.

1. Resolve reference/cell/faction ownership, rank/access rights, overrides,
   public spaces, and stolen-item provenance through existing native data.
   Audit activation, taking, container transfer, harvesting, lock interaction,
   pickpocket, horse taking, and selling/confiscating stolen goods.
2. Define offenses for trespass, theft, attempted/detected pickpocket, assault,
   murder, and native associated actions. Distinguish the attempted action,
   completed action, witnessed/reportable offense, and bounty change.
3. Evaluate named observers through native perception at the offense's actual
   time/place. Cover victims, bystanders, guards, allies, dead/unconscious or
   disabled observers, obstructed sight, and the verified reporting semantics.
   No omniscient map-wide guard notification unless a native rule proves it.
4. Bind assault/murder to the hit/death cause, including projectile delay,
   companion involvement and lawful combat. Arena opponents, hostile dungeon
   enemies, self-defense, and essential knockout require explicit legal cases.
5. Deduplicate per causative action without merging separate crimes. Determine
   actual bounty/infamy/faction effects, murder flags and special identity
   rules from probes. Preserve unpaid incidents across doors/load/restart.
6. Implement live ownership/faction/crime script commands needed by these
   rules; unsupported contexts must fail precisely rather than defer.

**Tests:** full witness matrix in section 8, exact bounty and item deltas,
multiple witnesses reporting one act, repeated separate thefts, lethal assault
ordering, unwitnessed crime, delayed projectiles, lawful combat, ownership
change between setup and action, and loading with unresolved offenses.
L3 normal inputs steal/trespass/attack; snapshots prove both positive crimes
and matched legal/no-report cases. S9 may end before arrest UI is implemented,
but the exact correct guard-response intent must be observable.

**Gate:** crime is a consequence of the real action, not a script-side counter.
Suggested commit: `Implement native offenses witnesses and bounty state`.

### S10 - Implement guard pursuit, surrender, and fine resolution

**Changes:** guard authority/AI integration, ordinary arrest interaction,
native fine/confiscation transaction, resisting and accepted/rejected yield.

1. Identify lawful guards using resolved records/factions and native rules.
   Connect a reported offense to a specific guard/incident; reuse M14 pursuit
   and door handling.
2. Implement guard approach, response range, player yield/sheathing or other
   verified normal surrender gesture, and normal-input choice UI. Include
   accepted and rejected surrender conditions and non-guard combat yield
   where native behavior supports it.
3. Expose all native applicable choices: pay fine, go to jail, resist arrest,
   and conditional alternatives established by the content audit.
4. Pay fine atomically: verify affordability, remove exact gold, confiscate
   qualifying stolen property, clear the proper bounty/incident state, calm
   the proper actors, and resume control/schedules. Keep other obligations.
5. Resist arrest transitions to real combat; fleeing cannot silently clear
   unpaid bounty. Avoid repeated arrest dialogs while an interaction is
   already committed or the player is unable to act.
6. Save/restart during pursuit, before a choice, and after payment. Reopening
   a dialog or reloading must not charge or confiscate twice.

**Tests:** guard sees/does not see offender, normal/cross-cell/blocked pursuit,
one/multiple guards, sufficient/exact/insufficient gold, cancel, yield accepted/
rejected, weapon drawn/sheath, resist then flee/reapproach, dead guard, target
death, identity changes, simultaneous crimes, and transaction interruption.
L3 must show the guard physically reaches the player and the chosen UI action
causes the exact state change; calling `payFine()` is only L2.

**Gate:** fine and resist branches work end to end; the jail choice enters an
explicit pending S11 path and is not falsely reported as a completed sentence.
Suggested commit: `Implement native guards surrender and fine transactions`.

### S11 - Implement jail, sentence, release, and escape

**Changes:** TES4 prison/evidence/belongings/release resolution, confiscation,
normal serve-time interaction, penalties, escape and restoration.

1. Resolve each selected jurisdiction's actual markers/containers/doors from
   TES4 data. Do not teleport to a string-matched TES3 `prisonmarker`.
2. Jail entry must preserve the correct property, confiscate the correct
   stolen items, handle quest items/keys/equipped/hotkey items, move the player
   through the normal arrest transition, and expose their incarcerated state.
3. Implement sentence duration and serving through normal bed/rest activation
   and confirmation. Advance the native calendar/AI using existing clock
   services, apply the exact skill penalties once, and release at the correct
   location with the correct inventory/equipment/bounty state.
4. Implement escape through actual locks/doors/movement. Leaving by escape is
   not serving time. Handle evidence/belongings recovery, detection and
   re-arrest with native rules, not an unconditional reset.
5. Provide the service/state operations for fines or special services that
   resolve outstanding crime after escape when original content exposes them.
   Cover conditional guild or identity-specific operations at L1/L2 now.
   Their original dialogue prerequisites and full interaction cases close in
   S12/S13, not this stage's serve/escape gate; keep them explicitly open in
   the report. Absence of prerequisites does not mean a branch never exists.
6. Make every transfer/time/penalty/release idempotent across save/load. Do
   not recover property by recreating base items and losing condition, charge,
   ownership, stack identity, or hotkey bindings.

**Tests:** multiple prison layouts; minimum/large sentence; no property/mixed
property; stolen/legitimate/quest/stacked/equipped items; skill boundaries;
serve versus cancel versus escape; escape discovered/undiscovered; recovery
and re-arrest; save before confiscation, inside jail, before/after rest,
immediately after release, and after escape.
L3 separately completes serving and escaping through normal inputs, with
fresh-process continuation from jail and no post-setup state writes.

**Gate:** all four inventories (player, belongings, evidence, recovered loot)
balance, time/penalties apply once, and served/escaped states remain distinct.
Suggested commit: `Implement TES4 jail sentences release and escape`.

### S12 - Close script and official-content integration gaps

**Changes:** complete the M15 command/event/condition matrix and only the
narrow campaign prerequisites identified in S0. Commands belonging to earlier
stages should already be implemented; do not postpone their basic wiring here.

1. Search the native command registry, condition IDs, compiled official corpus,
   and all runtime deferred traces. Include object, player, quest, dialogue
   result, and relevant callback contexts; include aliases and wrong targets.
2. Cover combat/death/crime/jail getters and mutations, hit/death/murder/alarm
   or other actually supported native event blocks, essential/unconscious
   state, ownership/faction changes, and attribution. Confirm event names,
   arguments and ordering from the actual registry/probes before adding them.
3. Test save/load from callbacks, reentrancy, target deletion, actor unload,
   no duplicate callbacks, and exactly one committed consequence. Preserve
   the M7 synchronous execution model.
4. Finish the minimum real dialogue/result and quest-event bridges needed to
   enter and finish the selected tutorial, Arena and crime branches. They must
   run original conditions/results, not scenario-specific fabricated topics.
   Include the conditional guild/identity resolution interactions left open
   by S11 and demonstrate that their actual prerequisites select the correct
   choices before closing this stage.
5. If a campaign reaches M16 magic, either implement a narrowly justified real
   prerequisite with tests, or leave the campaign/M15 blocked. Selecting a
   documented representative nonmagical dungeon is acceptable; deleting an
   essential official encounter or pretending a spell succeeded is not.
6. Remove M15-owned deferred/constant-result paths. Keep genuinely later
   unsupported actions explicit and fail any campaign that reaches them.

**Tests:** every binding gets valid/invalid target tests and live-state
agreement; full official ObScript frontend audit; original-content runtime
traces through campaign entry, legal Arena engagement and jail entry/release.
No M15-owned reachable unsupported command/condition or deferred success.

**Gate:** no remaining hidden dependency prevents a normal user from performing
the S13 courses. Suggested commit:
`Complete M15 ObScript bindings and campaign prerequisites`.

### S13 - Run the official-content acceptance campaigns

**Changes:** final manifests/assertions, tightly coupled bug fixes with their
regression tests, and report. The feature should already exist.

Implement the courses in section 9, run them with real installed data, and
retain every failure. Each course must prove the required outcome, compare
the named actors/items, exercise restart continuation, and capture its key
visual/audio phases. Do not end a course just before its difficult action.

For each failure: reproduce the smallest case; add an L1/L2/L3 regression;
fix the root cause; rerun that case and affected neighboring campaigns.
If a fix changes a shared authority, rerun all consumers of that authority.

**Gate:** all tutorial/dungeon/Arena/crime resolution requirements pass with
normal input, repeatability, persistent state, and direct inspection. The
report separates synthetic coverage from original-content coverage.
Suggested commit: `Add complete M15 real-content acceptance campaigns`.

### S14 - Run universal gates and accept only the verified revision

Freeze an implementation candidate. Execute section 10's full build/test/
sanitizer/content/regression/performance/oracle gates against that candidate.
Rebuild/retest affected gates if any source or fixture changes afterward.
Do not mix old binaries, newer manifests, and stale images in one report.

Complete section 11's checklist. Update the report, the roadmap ledger,
`docs/oblivion/README.md`, and `IMPLEMENTATION-STATUS.json` only after all
gates pass. Mark M15 accepted and M16 next in a separate acceptance-documentation
commit. Update the top-level README's capability/limitation text accurately.
Do not mark absent Shivering Isles data as tested or claim M16/M17/M18 parity.

Suggested commit: `Record verified M15 combat stealth crime and jail acceptance`.

## 6. Exact existing commands and proposed additions

All commands below run from the repository root, not its parent directory.
Use Bash with `set -euo pipefail` for logged command sequences. Every `tee`
pipeline must preserve the command's nonzero status. Use unique output paths;
do not delete another run or reuse files that can satisfy freshness checks.

### 6.1 Existing discovery and focused tests

```sh
git status --short
git rev-parse HEAD
python3 scripts/oblivion_compat.py --help
python3 scripts/oblivion_compat.py scenario --help
python3 scripts/oblivion_compat.py runtime-state --help
python3 scripts/integration_tests.py --help
ctest --test-dir build --show-only
./build/components-tests --gtest_list_tests
./build/openmw-tests --gtest_list_tests
```

Build only existing relevant targets during development:

```sh
cmake --build build --target components-tests openmw-tests openmw esmtool -j2
./build/components-tests \
  --gtest_filter='ESM4RuntimeState.*:ESM4PlayerMechanics.*:ESM4InventoryMechanics.*:ESM4Detection.*'
./build/openmw-tests \
  --gtest_filter='OblivionAiTest.*:OblivionProfileServicesTest.*'
python3 -m unittest discover -s scripts/tests -p 'test_*.py'
```

These are baseline filters, **not M15 acceptance filters**. Register new
component/engine sources in their existing CMake targets and give suites
discoverable names, for example `ESM4CombatRules`, `ESM4CrimeRules`,
`ESM4CombatData`, `OblivionCombatTest`, and `OblivionCrimeTest`. Names are
proposals until implemented. Record the resolved case list and require a
positive expected case count per suite. GoogleTest returning zero with zero
matching tests must be treated as failure by the acceptance driver.

For each feature stage, run its newly named suites and all affected baseline
suites in one runner invocation per binary. Capture GoogleTest XML and text
output in that stage's evidence directory. Counts must come from the executed
result, not copied from M14 prose.

### 6.2 Existing scenario/content/state commands

Set these variables to inspected absolute paths before running:

```sh
SOURCE="$PWD"
BUILD="$SOURCE/build"
OBLIVION_DATA="/absolute/path/to/Oblivion/Data"
OUT="$SOURCE/build/oblivion-compat/m15/S0/baseline-01"
mkdir -p "$OUT"
python3 scripts/oblivion_compat.py baseline \
  --build "$BUILD" --oblivion-data "$OBLIVION_DATA" \
  --census-all --require-lossless-tes4 --hash-archives \
  --output "$OUT/content"
python3 scripts/oblivion_compat.py m14-audit \
  --oblivion-data "$OBLIVION_DATA" \
  --count-lock scripts/data/oblivion_compat/oblivion_m14_data_counts.json \
  --output "$OUT/m14-audit"
python3 scripts/oblivion_compat.py scenario \
  scripts/data/oblivion_compat/oblivion_m14_city_schedule.json \
  --output "$OUT/m14-city" \
  --variable "source=$SOURCE" \
  --variable "openmw=$BUILD/openmw" \
  --variable "resources=$BUILD/resources" \
  --variable "oblivion_data=$OBLIVION_DATA"
```

The M15 scenario invocation will use this same existing `scenario` CLI with
new checked-in manifests, proposed as:

```text
oblivion_m15_melee_course.json
oblivion_m15_ranged_course.json
oblivion_m15_death_loot.json
oblivion_m15_combat_ai.json
oblivion_m15_stealth_pickpocket.json
oblivion_m15_witness_matrix.json
oblivion_m15_arrest_fine.json
oblivion_m15_jail_serve.json
oblivion_m15_jail_escape.json
oblivion_m15_tutorial_combat.json
oblivion_m15_dungeon_combat.json
oblivion_m15_arena_match.json
```

Place them under `scripts/data/oblivion_compat/`. Split parameterized matrices
into explicit cases in the result report, not one opaque "scenario passed".
The names above are not files that already exist.

Existing independent save tools:

```sh
python3 scripts/oblivion_compat.py runtime-state inspect "$SAVE" \
  --report "$OUT/state.json"
python3 scripts/oblivion_compat.py runtime-state compare "$SAVE" \
  --expected "$EXPECTED_JSON" --report "$OUT/state-comparison.json"
python3 scripts/oblivion_compat.py runtime-state m14-verify "$SAVE" \
  --report "$OUT/m14-state.json"
```

Choose `SAVE` by exact expected name/path plus freshness/digest, not the first
file matched by a glob. Inspect the current CLI's compare semantics before
using it: full canonical comparison and the proposed M15 subset/continuation
comparison are different operations. Expected JSON must not be regenerated
from the candidate after a discrepancy.

S1/S2/S3 must implement and document any new `m15-audit`, `m15-verify`,
typed observations, state-delta checks, or aggregate `m15-acceptance` commands
before subsequent stages invoke them. Add `--help`, schema, positive and
negative tests for each. No such M15 CLI exists at plan creation time.

### 6.3 Existing visual and diagnostic tools

```sh
python3 scripts/oblivion_compat.py inspect-image "$IMAGE" \
  --report "$OUT/image-inspection.json"
python3 scripts/oblivion_compat.py compare-image "$REFERENCE" "$IMAGE" \
  --minimum-ssim 0.995 --maximum-phash 4 \
  --report "$OUT/image-comparison.json"
python3 scripts/oblivion_compat.py check-log "$LOG" \
  --report "$OUT/log-check.json"
```

For synthetic/UI regression captures add
`--maximum-changed-ratio 0.001` to enforce the roadmap's 0.1% unmasked pixel
limit. Confirm masks/cameras and metric interpretation before comparing.
Do not use cross-engine pixel equality. Image inspection catches empty frames,
not incorrect gameplay or animation, so direct agent inspection is mandatory.

## 7. Persistence and failure-injection procedure for every feature

Use this procedure at the stage's listed boundaries, not just once at the end:

1. Start from the same hashed initial fixture/save in two clean user-data
   directories, with identical seed/configuration and no old output files.
2. Run control A uninterrupted through the action sequence; record checkpoints,
   final state, inventory, queued/consumed event IDs, and a normal final save.
3. Run B to the chosen boundary; create a normal in-game save and wait for
   completion. Record its path/hash and the authoritative pre-quit snapshot.
4. Quit the engine, confirm it exited, then launch a different process loading
   that exact save. No same-process quickload substitutes for this step.
5. Observe before continuing; compare committed state exactly and documented
   ephemeral/reconstructed state by explicit semantic invariants/tolerances.
6. Perform the same remaining normal actions, resave, and compare B with A.
   Require the same terminal health, inventory/condition/ownership, crime,
   sentence, actor relationships, and consumed transactions.
7. Repeat with an allowed plugin reorder; content FormKeys must remain stable.
   Run missing-plugin, altered-fingerprint, wrong-profile, truncated save,
   invalid enum, duplicate transaction, and dangling actor negative cases.
8. Confirm rejected loads produce precise diagnostics and leave the world
   unchanged. Inspect saves independently with the Python reader and ensure
   it rejects the same invalid states as the engine.

Do not demand bitwise-identical physics poses across compilers. Record exact
stable fields and explicit bounded pose/velocity/collision invariants.
Do not broadly exclude all combat/AI state from comparison to make reload pass.

Failure injection may use synthetic tests to interrupt a transaction before
commit, after commit, and before acknowledgment; recover without duplication.
Campaigns still require ordinary input and authentic saves.

## 8. Required coverage matrices

Assign stable IDs (for example `M15-MELEE-001`) to every concrete case in S0.
The following rows are minimum coverage, not a claim that every combination
must be an expensive E2E run. Enumerate all discrete rule branches at L1;
use pairwise combinations plus identified high-risk full crosses at L2/L3;
run the actual flows in section 9 at L4. Never sample away an offense or
resolution branch.

| Family | Required variations | Required assertions |
| --- | --- | --- |
| Melee/contact | Blade, blunt, unarmed, creature; normal/power/directional; skill thresholds; fatigue zero/low/full; difficulty boundaries; reach inside/outside; target behind wall; first/third person. | Correct rule values, contact causality, one hit/window, no TES3 random miss, correct fatigue/condition/reaction. |
| Block/armor | Shield, weapon, no legal block; angle boundary; early/late input; light/heavy/mixed/no armor; broken/normal/over-repaired condition; cap boundaries. | Exact mitigation and wear, correct actor/slot, no negative health/condition or duplicate fatigue charge. |
| Mastery | Every audited combat/Sneak/armor ability; below/at/above threshold; valid/invalid posture/equipment; deterministic proc/non-proc; attack direction; disarm/knockdown/paralysis/armor bypass as applicable. | Real unlocked ability without requiring M16 leveling, correct dropped item/effect duration/mitigation, persistence and no duplicate proc after reload. |
| Reactions | Stagger, knockdown, fatigue knockout, essential unconsciousness; attack interrupted; recovery while input held; repeated hits. | Native durations/eligibility, no stuck controller, no incorrect death/crime, save continuity. |
| Ranged | Partial/full draw, cancel, no ammo, moving shooter/target, thin wall, long distance, high speed, simultaneous projectiles, cell change. | Analytic trajectory bounds, one ammo debit/impact, no tunneling or ghost damage, exact spent/recovered inventory. |
| Death/loot | NPC/creature/player/essential; melee/arrow/script; simultaneous lethal hits; slopes/stairs; corpse unload; resurrection/reset. | One death/event/death-item generation, valid physical corpse, correct killer, legal loot and no duplication on reload. |
| AI | Style variants, aggression/confidence/relationships, ambush, flee/search, ally interference, cover/doors, high/low process, target deletion. | Real attacks, bounded progress, native-only sequence, correct hostility and schedule resumption. |
| Horses | Mounted input restriction, incoming melee/arrow, horse crime/ownership, rider/horse essential/death, dismount under interruption. | Vanilla policy, correct damage target, no double crime/hit, reciprocal mount state and safe collision. |
| Detection/sneak | Light/dark, LOS/occlusion, near/far, motion/noise, armor/footwear, skills, asleep/unconscious/dead observer, multiple observers. | Actor-pair awareness agrees with UI/scripts, correct sneak multiplier, no omniscient detection. |
| Pickpocket | Successful/failed/exit/cancel attempt, legal/forbidden item types, one/many items, value/weight boundaries, victim awareness. | Exact attempt semantics, item conservation/metadata, correct crime intent, no transfer on prohibited/failing action unless native rule says otherwise. |
| Ownership | Unowned, reference owner, cell owner, faction/rank, authorized access, owner change, stolen item in mixed stack. | Single legality answer across actions, exact stolen provenance, no blanket confiscation of legitimate property. |
| Witnesses | Zero/one/many; victim/bystander/guard; LOS yes/no; responsibility/faction variants; alive/dead/unconscious/disabled; offender known/unknown; observer enters after act. | Native report decision and timing, exact incident/bounty, duplicate reports do not duplicate one offense. |
| Offenses | Trespass warning/escalation/leave; theft/pickpocket/horse taking; assault/murder; lawful enemy/Arena/self-defense; companion/delayed-arrow attribution. | Distinct actions and legal exceptions, exact fines/flags, persistent unpaid crime, correct victim/owner. |
| Arrest | One/many guards, cross-cell pursuit, blocked route, sufficient/exact/insufficient funds, accept/reject yield, pay/jail/resist, cancel, flee/reapproach. | Correct guard and choices, transactional money/property, no duplicate dialog, unpaid crime not forgotten. |
| Jail | Multiple locations, short/long sentence, no/mixed property, quest items, skill boundaries, serve/cancel/escape, property recovery, re-arrest. | Correct cell/containers/time/penalty/release, distinct escape state, inventory conservation and idempotency. |
| Special crime state | Every applicable audited faction/guild discount, immunity or identity-specific bounty path; prerequisites met and unmet. | Correct gating, persistent identity, no imported Skyrim/Morrowind assumptions. If unavailable in installed content, name the absent dependency and retain synthetic coverage. |
| Script integration | Each M15 command/condition/event, aliases, object/player/quest/dialogue contexts, bad targets, callback saves/reentrancy. | Live-state agreement, exact event attribution/order, no constant/deferred placeholder, typed failure on unsupported use. |
| Regression | M12 controls/stats, M13 stack/equipment/repair/economy, M14 routes/detection/mounts, TES3 combat/crime/jail/inventory/saves. | Preserved contracts, no native state leakage, no unexplained baseline divergence. |

High-risk crosses must include lethal sneak projectile + witness + restart,
essential victim + multiple attackers, stolen equipped stack + fine/jail +
restart, escape + re-arrest + property recovery, and guard pursuit + door
transition + interrupted save/load.

## 9. End-to-end courses and exact completion criteria

### E1 - Tutorial combat

Start at the audited legitimate tutorial boundary with normal progression
provenance, no target already dead, and no fabricated quest completion.
Acquire/equip the actual available weapon/ammo through normal interaction.
Follow the required door/escort progression, fight the named tutorial
opponents, take actual corpse loot, and reach the post-encounter progression
boundary through authored scripts/conditions.

Assert each intended attacker/victim, real attack/contact/damage/death,
essential escort survival/recovery, exact loot deltas, the authored progression
change, and no inappropriate bounty. Restart once during combat or its nearest
valid save boundary and once after loot; continue to the same endpoint.
Do not replace this with the M14 escort course or a prison screenshot.

### E2 - Representative dungeon

Use the S0-selected real dungeon (Vilverin is a candidate, not a hard-coded
assumption). Enter through its real exterior/interior door; complete a
documented representative route containing melee, ranged or creature threats,
at least one obstruction/cover or elevation challenge, corpse loot, and a
return/exit. Specify exact mandatory encounters and destination.

Assert opponent engagement and defeat, player survival/resources, no attacks
through walls, correct corpse inventories, correct cell transitions and
persisted cleared/looted state on return. Restart with an active encounter and
after leaving/re-entering the dungeon. Do not use a generic damage log from a
different cell or finish after killing only the first convenient enemy.

### E3 - Arena match

Enter via actual dialogue/registration/equipment rules and authored start
conditions. Begin one complete representative match through normal
interaction, fight the declared opponent(s), satisfy the actual victory
conditions, leave through the normal route, and obtain the legitimate
post-match acknowledgment/reward.

Assert proper enemy activation and boundaries, real AI combat and death,
lawful Arena combat (no spurious assault/murder bounty), correct victory
event/quest state, reward exactly once, and continuation into the next idle/
ready state. Restart before the fight and after victory before reward in
separate runs. Match-start console commands, dead opponents, direct quest
stage writes, or an asserted "Arena" cell name cannot satisfy this course.

### E4 - Stealth and crime permutation course

Perform paired legal/unlawful and unseen/seen actions with exact player,
victim/owner, item, and observer identities. Include sneak attack, pickpocket
success and detected failure, trespass leave/escalation, theft, assault, and
murder. Reset only between independent cases from their verified initial
fixtures/saves, never during the exercised offense.

For each case assert the complete legality -> action -> detection/report ->
bounty/state chain (or its explicitly absent counterpart). Include multiple
witnesses, essential knockouts, legal dungeon/Arena combat, and ordinary
noncriminal movement as false-positive controls.

### E5 - Every crime-resolution branch

Use separate clean runs for each branch. All start with a real, witnessed
offense unless testing a no-crime/false-arrest negative case.

| Branch | Normal actions | Terminal proof |
| --- | --- | --- |
| Pay fine | Guard pursuit -> surrender/interaction -> pay. | Correct gold and stolen-property deltas, correct cleared bounty, guards resume, no second charge after restart. |
| Insufficient money | Same path with insufficient funds. | Native unavailable/rejected payment response, unchanged money/property until another legitimate choice, no false debt clearance. |
| Serve sentence | Accept jail -> inspect actual incarceration -> activate bed/rest -> confirm -> release. | Correct prison, confiscation and retained items, sentence/time/skill effects exactly once, normal release location and restored belongings. |
| Escape | Accept jail -> use actual locks/doors/stealth -> leave without serving. | Escape differs from release; correct outstanding/new bounty, inventory/evidence recovery rules and lawful world reentry. |
| Resist/flee/re-arrest | Resist -> real guard combat or pursuit -> escape immediate reach -> later encounter guard -> resolve. | Real combat/detection, no unexplained bounty reset, correct renewed arrest and final chosen resolution. |
| Yield variants | In actual combat perform vanilla surrender gesture under accepted/rejected conditions. | Correct opponent reaction and combat state, no arbitrary universal pacification. |
| Conditional alternatives | Meet audited guild/faction/identity prerequisites through legitimate provenance; choose the real alternative, then test unmet prerequisite. | Correct availability, discount/identity-specific state and persistence; no hard-coded "not applicable" without content evidence. |

Exercise at least two distinct audited prison/guard environments, including an
interior/exterior pursuit transition. Each branch requires fresh-process
restart at its most consequential state boundary. A single "jail happened"
event does not prove all branches.

### E6 - Visual and audio acceptance

For E1-E5 and synthetic courses capture named phases: attack windup/contact/
recovery, block, arrow release/flight/impact, stagger/knockdown, essential
recovery, death/settled ragdoll, loot, sneak/detected UI, guard approach/choices,
jail property state, serve/escape and release.

Use deterministic 1280x720 Xvfb captures and fixed cameras; add object-ID/depth
captures where needed to prove contact geometry/occlusion. If the existing
harness does not provide those buffers, implement that narrow capture path
and negative controls before claiming the corresponding visual gate.
Require expected forms and finite bounds; reject empty/black frames, missing
textures, invalid animation transforms, clipped choices, unreadable required
text and shader errors.

Run audio cases with sound enabled. Existing M14 `--no-sound=1` manifests
cannot demonstrate combat audio. Capture routed audio output plus timestamped
selected asset/event/actor IDs using the project's existing sound/media
infrastructure. There is no general audio waveform verifier in the current
compatibility CLI; add focused support if needed. Require non-silent audible
output in expected windows, correct swing/hit/block/arrow/creature/death
selection, spatial attenuation, and no duplicate or missing events. Inspect/
listen using available media tools; logs alone are insufficient audio evidence.

Run paired isolated original-game probes for formula/behavior/animation/audio
questions identified in S0. Record executable/content hashes, input procedure,
settings and uncertainty. Keep any original captures local. Compare behavior,
phase ordering, geometry and asset selection, not cross-engine pixel identity.

The implementing agent must open and inspect every required montage and
record per-capture observations, pass/fail and discrepancies in the report.
Do not ask the user to do the visual or interactive testing. If an inspection
tool cannot expose required media, retain an explicit blocked inspection gate.

## 10. Final build, test, sanitizer, and regression campaign

### 10.1 Fresh build matrix

Use separate build directories; never switch compilers inside one cache.
At minimum run Debug and RelWithDebInfo with GCC and Clang when available.
Record genuine compiler absence; an available compiler's failed build is not
"unavailable". Preserve relevant known-working dependency options, including
bundled double-precision Bullet and the CMake compatibility floor on this
workstation.

Example for one matrix member (repeat with the corresponding compiler/type):

```sh
cmake -S . -B build-m15-gcc-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DBUILD_TESTING=ON \
  -DBUILD_COMPONENTS_TESTS=ON -DBUILD_OPENMW_TESTS=ON \
  -DBUILD_OPENCS=ON -DBUILD_OPENCS_TESTS=ON \
  -DOPENMW_USE_SYSTEM_BULLET=OFF \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-m15-gcc-debug \
  --target openmw esmtool bsatool components-tests openmw-tests openmw-cs-tests -j2
ctest --test-dir build-m15-gcc-debug --output-on-failure
./build-m15-gcc-debug/openmw-cs-tests
```

The current CMake registers `components-tests` and `openmw-tests` in CTest,
but **does not register `openmw-cs-tests`**. It must be built/enabled and run
explicitly. The current local `build/` has OpenCS tests disabled; two passing
CTest entries are not the universal gate. Run the default build as well at
final acceptance to catch enabled targets not in the focused target list.

Run the complete Python suite, not only new M15 tests. Component Lua tests
are part of `components-tests`; the external Lua API/integration course is
separate:

```sh
python3 -m unittest discover -s scripts/tests -p 'test_*.py'
python3 scripts/integration_tests.py "$EXAMPLE_SUITE" \
  --omw "$BUILD/openmw" --workdir "$OUT/integration"
```

`EXAMPLE_SUITE` is an inspected local checkout of OpenMW's example-suite
content. Omitting `--tests` and `--test_filter` requests the full runner
inventory. Record executed Lua cases and skip counts. A missing external
suite is a blocker to that gate, not a successful zero-test run. Use the same
example-suite revision, renderer/backend and environment for the baseline.

### 10.2 ASan/UBSan and malformed input

Use dedicated Debug build directories following `.gitlab-ci.yml` sanitizer
patterns. For an affected runtime build, compile and link with
`-fsanitize=address,undefined -fno-omit-frame-pointer`, and ensure both
sanitizers are active; separate ASan/UBSan builds are also valid. Supply flags
through CMake cache options, not an unrecorded local compiler wrapper.

Run affected component and engine suites with:

```sh
ASAN_OPTIONS=halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
./build-m15-sanitized/components-tests --gtest_filter="$M15_COMPONENT_FILTER"
ASAN_OPTIONS=halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
./build-m15-sanitized/openmw-tests --gtest_filter="$M15_ENGINE_FILTER"
```

The directory and filter variables are **to be configured and resolved during
implementation**, not preexisting artifacts. Check instrumentation in build
commands/runtime dependencies and retain sanitizer logs. Do not globally
disable leak/error detection to suppress a crash. Any external-library
suppression must be narrowly matched, evidenced on baseline, and documented.

Run the affected malformed-record/save corpus and existing relevant fuzz
targets if present. If there is no appropriate fuzz target, add a bounded
deterministic mutation loop to existing test infrastructure for the new
decoder/deserializer, retaining seed and minimized reproducers; do not claim
an unrun fuzz campaign. Exercise count/size overflow, invalid values,
truncation, reference cycles/duplicates, and old-version migration.

Run the sanitized `openmw` binary through at least the synthetic melee,
projectile/death, and crime/jail restart courses; pure parser sanitizer passes
do not cover actor pointers, collision callbacks, or transaction lifetime.

### 10.3 Real content and regression

Rerun the full installed ESM/ESP/BSA fingerprinted census with lossless checks,
the M14 semantic audit, the new M15 semantic audit, and the official ObScript
frontend audit. Query each command's actual `--help`; retain its exact
arguments in the report. Add/remove count-lock exceptions only after reviewing
the underlying records; never update counts automatically to whatever passed.

Rerun M12 control/stat, M13 inventory/equipment/economy and M14 navigation/
detection/horse cases affected by the implementation. Run the full Morrowind
integration suite and deterministic real-content courses covering new game/
Seyda Neen, dialogue/quest progression, melee/ranged/magic, stealth/crime/jail,
inventory/UI, exterior travel, and historical/new save-load.

Create an isolated baseline worktree/build at the S0 revision for shared-code
characterization. Do not reset the main worktree. Compare candidate and
baseline using the same content hashes, initial saves, scripts and settings.
Include TES3 chance-to-hit, blocking, hand-to-hand fatigue damage, crime/
bounty, pickpocket and jail behavior so native rules do not leak through
shared classes. Verify no M15 services/state markers appear in TES3 saves.

### 10.4 Performance and resource safety

Use existing scenario timing/engine stats infrastructure. Measure at least
three matched baseline/candidate runs after the same warmup, with fixed
hardware/settings/content. Record frame-time median/p95, loading time and
memory/peak resident usage for idle city, crowd combat, projectile burst,
guard pursuit, cell travel and save/reload.

The roadmap forbids an unexplained Morrowind regression above 5% in median
frame time, p95 frame time, loading time or memory. Repeat noisy measurements
with all samples retained; explain variance rather than cherry-picking.
For new Oblivion combat workloads, set absolute budgets in S0/S1 from scene
size and the baseline idle cost, before tuning. Avoid all-actors-squared
witness checks and unbounded incident/projectile/event retention.

Run a bounded long-duration synthetic soak with repeated combat/loot/crime/
jail cycles and cell unload/reload. Declare cycle count/duration up front,
assert bounded memory and actor/projectile/incident counts after cleanup, and
include a fresh-process save continuation at the end.

## 11. Final definition of done and handoff

M15 is accepted only when every checkbox has case IDs and fresh evidence:

- [ ] S0-S14 each has a passed gate, bounded commit history and durable handoff.
- [ ] Native combat-style/data semantics are wired through winning stores,
  settings, runtime actors and audit; raw preservation is not called simulation.
- [ ] Normal player/NPC/creature melee, block, armor, condition, reactions and
  ranged projectiles work without accidental TES3 rules.
- [ ] Native combat AI, detection, companions, horses and package resumption
  share the intended authorities and do not run competing AI sequences.
- [ ] Death, essential recovery, dynamic ragdolls, player load flow, death
  events and corpse loot work and persist without duplication.
- [ ] Sneak/pickpocket/ownership and every offense/witness branch have positive
  and negative cases, exact state deltas and no false crime in lawful combat.
- [ ] Every available arrest/surrender/fine/jail/serve/escape/resist/conditional
  resolution path is demonstrated through normal interaction.
- [ ] Native script commands, conditions and events agree with live state;
  no M15-owned deferred or constant-success/constant-false placeholder remains.
- [ ] Save/restart/continuation, old versions, reordering, corrupt content/state,
  callback saves and interrupted transactions meet section 7.
- [ ] Tutorial, full selected dungeon route, complete Arena match and all crime
  courses pass their exact named outcomes; setup cannot pre-satisfy them.
- [ ] Harness negative controls prove wrong/missing/stale/faked evidence fails;
  no required suite/case is disabled, skipped, empty or hidden behind retries.
- [ ] Visual, audio, independent original-game probes, and direct agent
  inspection have explicit per-case results.
- [ ] Full compiler/build/test/OpenCS/Lua/integration, sanitizer/mutation,
  official-content and Morrowind regression gates are complete.
- [ ] Performance/resource budgets pass with retained matched measurements.
- [ ] Final report identifies the exact tested revision, binary/config/content
  hashes, command lines, case counts, initial saves, seeds, capture notes and
  artifact paths. Missing data and genuine future scope are stated accurately.
- [ ] Acceptance metadata changes occur only after the implementation
  candidate has passed; source or fixture changes invalidate affected evidence.

When pausing between stages, leave the report with: last passed stage and
commit; current failing case and exact reproducer; what remains unimplemented;
which commands have actually been run; where artifacts are stored; whether
any processes remain active; and the next smallest action. Keep later gates
open. Never replace that handoff with "mostly done" or "tests pass" without
named outcomes.
