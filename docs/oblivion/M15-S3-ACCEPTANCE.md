# M15S3 acceptance — 2026-10-15

S3 is delivered against the five changes, tests and gate in
[the implementation plan](M15-COMBAT-STEALTH-CRIME-IMPLEMENTATION-PLAN.md#s3---wire-services-actor-state-and-persistence).
The implementation checkpoint is `2a97dce87a42a23e74821ab7c3aa6ce067b4c9c5`.
This accepts services, actor authority and persistence; full M15 gameplay remains
outside this acceptance. The [authority map](M15-S3-AUTHORITY.md) traces the
adapters. Earlier incomplete statements in its append-only history and the
[milestone ledger](M15-COMBAT-STEALTH-CRIME.md) describe their respective checkpoints.

## Requirement audit

| Requirement | Implementation and verification |
| --- | --- |
| Authority map and actual actor paths | `OblivionCombatService` owns native values, life and legal state. World adapters publish changes and guarded shared stats supply class/Lua/GUI reads. The map traces Player, native NPC/creature, dynamic actor, unloaded reference and projected equipment. Actual actor tests include `nativePlayerRecomputesFromRawInputsAndRestoresIntoActualPlayer`, `nativeServiceOwnsNpcValuesAcrossMutationAndReload`, `liveCreatureClassUsesCanonicalNativeSkillGroups` and `resourceBatchRestoresResidentPlayerAndUnloadedActorsAtomically`. Dynamic NPC/creature reconstruction checks both process modes and all 72 value channels. `StatsWindow::onFrame` reads the same shared reputation/bounty getters; runtime Lua/native-command/save comparisons exercise those adapters. |
| Profile and lifetime | `World` creates native services only for Oblivion. New game and clear reset owned state; profile teardown destroys it. Player reference aliases normalize through `runtimeReferenceKey`. Actual registration/profile, repeated clear/reconstruction, scope/move/owner-lifetime tests pass. Failed incoming loads discard the candidate and restore the prior Running world. Canonical loaded-world replacement and engine shutdown run under ASan/UBSan. |
| Versioned state and strict validation | `RuntimeState` schema 46 is the binary/JSON/Python contract. Supported native envelopes are 1–46; saves without T4ST have a separate deliberate legacy path. Tests migrate empty state, populated inventories, native actor values/life, script/AI queues and later-added crime contracts from every version that can represent each group. Unsupported earlier fields remain absent rather than invented. Native values, shared stat projections and saved state are checked together. |
| Ordering and load transaction | Action IDs, pending actions, FIFO death callbacks, RNG and clocks are captured from live services. Death events leave the queue before callbacks can save. Prepared restore handles reject stale/foreign/replayed publication. `NativeLoadTransaction` retains outgoing World/backends and all participating managers while actual saved data installs into an incoming runtime. Candidate failure restores original owners; successful commit retires old users before their dependencies. Precommit and postcommit fault courses verify both boundaries. |
| Public queries | Service methods expose combat membership/opponents, values/life, action state, RNG, crime contracts and bounty. World provides script/controller/Lua adapters and jail queries. `scriptCombatCommandsPublishNativeMembershipAndStopOwnedActions` and Player-alias tests exercise the actual host. Native combat membership queries use service authority rather than `AiSequence::isInCombat`. |

The explicit negative-test gate is covered at parser, whole-save admission and
World/service layers: truncation/corruption/trailing bytes, foreign profile,
unsupported schema, duplicate/dangling identities, nonfinite/out-of-domain
values, conflicting shared/native authority, missing or fingerprint-changed
content, and plugin reorder. `obscript.dead` tests cover legacy adoption,
conflict rejection, deferred consumption and rollback. Stable FormKeys and
weather catalog identities survive reorder, including actual two-plugin runtime
courses recorded in the ledger. Missing/changed content fails before native
publication; the real late-failure transaction separately proves prior-world
preservation after scene construction.

No new authoritative state exists only in telemetry: service capture writes the
versioned T4ST contract, and restore reconstructs those services from that
contract. Crime/arrest/jail producers belong to later stages; their validated,
versioned state and query contracts are present in S3.

## Automated evidence

All paths below are relative to `build/oblivion-compat/m15/S3/`.

| Evidence | Result and provenance |
| --- | --- |
| `root-load-transaction-normal-05`, `root-load-transaction-sanitized-04` | All 1,477 engine cases / 121 suites pass normally and under ASan/UBSan. Tested dirty fingerprint: `d6421374ccfeebcfa064b6c862da40c59a8febc72bc9e838fe72a18211ae453f`, subsequently committed in the implementation checkpoint. |
| `input-facade-retention-normal-02`, `input-facade-retention-sanitized-02` | All 2,616 component cases / 252 suites pass normally and under ASan/UBSan. Component implementation/tests are unchanged between that checkpoint (`75b58a4241e6ee82b00c5ba9f37a8a65ff2844f1`) and the accepted implementation. |
| `completion-audit-python-01` | All 298 Python cases pass at the accepted implementation checkpoint, without skips. |
| `completion-audit-01` | Revalidates the four exact C++ inventory/XML pairs, links 27 named engine cases to S3 requirements, and checks the existing runtime certificates. Its scripts and JSON are reviewable local evidence, not another C++ execution. |

## Actual engine acceptance

Canonical `root-load-transaction-runtime-normal-06` / `-sanitized-07` load an
actual native save from NoGame, F5, F9-load while Running, wait for five Player
frames in the second domain, render, F5 and quit. Their certificates include
source/binary/MyGUI/scenario/capture/save hashes, two committed loads, two saves,
selected persisted contracts and all 2,588 nonempty native reference inventories.

`root-load-transaction-failure-{normal,sanitized}-04` inject a failure after
candidate scene construction and `LuaManager::gameLoaded`. They verify exact
original Player/cell/node/Scene/renderer/current-character identities, native
bytes and PRNG, then render/save/quit. The postcommit normal/sanitized courses
instead throw after publication and retain the newly committed world. These
fault probes use disclosed source copies. They complement the canonical binary.

`root-load-transaction-restart-{normal,sanitized}-01` quit and start distinct
fresh processes, load the preceding real saves, render and resave. SHA links
prove the continuation inputs. The precommit/restart courses use core fingerprint
`970306f2e426d42025881f0c85bb43b85891c864a8eb8d7041dda17c41e115ad`;
the final change adds protection after commit and does not change those paths.

`completion-audit-legacy-{normal,sanitized}-03` additionally load an untouched,
previously accepted schema-21 save with real Oblivion content using the final
implementation. PIDs 425189/425188 render, save schema 46 and quit. Independent
decoding preserves the Player inventory and all 2,021 original nonempty reference
inventories, normalizing only later-added absent ownership fields to null.
Action/base/death/engagement fields remain equal; absent crime state becomes the
empty contract. The images were inspected. Additional cell discoveries are
allowed; total reference counts are not asserted equal.

The original S1 synthetic schema-7 fixture lacks a winning TES4 Player form.
Loading it alone rejects that missing dependency; adding the base game creates
ambiguous synthetic GMSTs. Both failed recipes and cancellations remain in
`completion-audit-legacy-*-01/02`; they are excluded from passing evidence.
Schema-7 format migration is covered by the automated version matrix. Historical
format support does not bypass current content/input validation.

`root-load-transaction-morrowind-normal-01` runs an actual Morrowind new game,
renders, saves and quits. Its parsed 102-record save has no GPRO/T4VR/T4ST or
OMW4STATE marker. This is profile isolation, not full Morrowind gameplay testing.

ASan leak detection is disabled. Null-driver OpenAL courses verify source and
decoder ownership, not listening-device output. Arbitrary externally retained
World handles still require dependent controllers/widgets to retire before World
destruction, as ordinary engine ownership does. The local load transaction is
resolved before those owners die. No audible combat, every rendered stat field,
or later-stage hit/crime/jail gameplay acceptance is inferred from S3 evidence.
