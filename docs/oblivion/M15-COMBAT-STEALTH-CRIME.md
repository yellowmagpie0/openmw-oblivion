# M15 combat, stealth, crime, and jail

Status: **in progress; not implemented or accepted**. The execution contract is
[the staged plan](M15-COMBAT-STEALTH-CRIME-IMPLEMENTATION-PLAN.md). Baseline work
started on 2026-09-19 at `72515455b345ef8700a3c8ebf10feef89db737d1`, with a clean
worktree on `master`. No M15 gameplay capability is established by this report.

## Stage ledger

Evidence root: `build/oblivion-compat/m15/`. Generated proprietary content,
logs, saves, screenshots, and reports are deliberately not committed.

| Stage | Status | Implementation / evidence | Next required gate |
| --- | --- | --- | --- |
| S0 baseline and inventory | passed | Baseline above; `S0/baseline-01/`, `static-calls-01/`, `inventory-02/`, `inventory-03/`; inventory closure below | Carry classified baseline defects and campaign prerequisites into their owning stages |
| S1 evidence harness | passed | `3477e6a4b0` through `8ac15f0ca3`, simulation-tick closure below; contract/observation/replay/restart evidence | Extend causal telemetry and negative controls with each native feature |
| S2 native data/rules | in-progress | Typed CSTY/CREA/FACT, 3,636 resolved actor-style policies, locked audit and reviewed rules below | Remaining physical/crime/mastery rules, asset semantics and original behavioral probes |
| S3 services/persistence | in-progress | Action ledger, schema evolution, actual NPC/creature/player publication and idle restart evidence below | Live writer activation, migration reconciliation and active-actor continuation |
| S4 melee/block | pending | No implementation/evidence | Normal-input contact and reaction |
| S5 projectiles | pending | No implementation/evidence | Normal-input release/impact and in-flight restart |
| S6 death/essential/loot | pending | No implementation/evidence | Physical corpse, essential recovery, loot, restart |
| S7 combat AI | pending | No implementation/evidence | Autonomous combat and native schedule resumption |
| S8 stealth/pickpocket | pending | No implementation/evidence | Shared perception, normal interactions, mastery |
| S9 offenses/witnesses | pending | No implementation/evidence | Exact legal/criminal consequences and persistence |
| S10 arrest/fine | pending | No implementation/evidence | Physical pursuit and normal choices |
| S11 jail | pending | No implementation/evidence | Property, sentence, release/escape, restart |
| S12 scripts/prerequisites | pending | No implementation/evidence | Real dialogue/results and command/event coverage |
| S13 official campaigns | pending | No implementation/evidence | E1–E6 normal gameplay and media review |
| S14 universal acceptance | pending | No implementation/evidence | Full matrix, sanitizers, regression, performance, original-game probes |

## S0 requirements and execution

These IDs identify baseline requirements, not completed gameplay tests. Each
must be split into the concrete permutations in plan sections 8–9 before its
owning stage is implemented. Future rows in the stage ledger are not passes.

| Case | Owner / level | Oracle and expected result | Evidence / current result |
| --- | --- | --- | --- |
| M15-S0-01 | S0 / build metadata | Exact revision, cache, binaries, content identity | Clean source recorded; RelWithDebInfo, GCC, bundled Bullet; content hashes in `content/baseline.json` |
| M15-S0-02 | S0 / L1 baseline | Existing focused tests execute nonzero counts and pass after rebuild | `rebuilt-components.xml`: 39/39; `rebuilt-engine.xml`: 29/29; `python.log`: 76/76 |
| M15-S0-03 | S0 / L0 content | Existing reviewed count locks and lossless census pass | `m14-audit/m14-audit.json`, `content/baseline.json`: pass |
| M15-S0-04 | S0 / L3 regression characterization | Existing city, inventory and TES3 isolation manifests run unchanged in fresh outputs | `m14-city/scenario.json`, `m13-items/scenario.json`, `morrowind/scenario.json`: automated passes; visual limits below |
| M15-S0-05 | S0 / L0 campaign discovery | Exact forms, original entry/results and prerequisite gaps for E1–E5 | Arena and crime findings below; full winning-record/condition traversal still open |
| M15-S0-06 | S0 / L5 availability | Identify original-game oracle, external Lua suite, compilers, audio and final build matrix | GCC/Xvfb/xdotool present; Clang not in `/usr/bin` or PATH; Steam Proton installation present but original-game probe not run; external suite not yet resolved |

### Rebuilt baseline

`cmake --build build --target components-tests openmw-tests openmw esmtool -j2`
completed successfully (`build.log`). This reused the user's existing build
configuration without changing its CMake options. The initial binary runs are
also retained as `components.*` and `engine.*`; only `rebuilt-*` establish the
post-build focused baseline. No skipped tests appeared in these focused runs.

The existing CTest inventory has **two** entries. OpenCS tests are disabled in
the current cache. Neither the focused runs nor that inventory satisfies S14.
The full GCC/Clang Debug/RelWithDebInfo matrix, OpenCS, sanitizers, full CTest,
external Lua integration, original-game oracle, audio and performance/soak
campaigns remain unrun for M15.

Reproduce from the repository root, choosing a fresh output directory:

```sh
OUT="$PWD/build/oblivion-compat/m15/S0/baseline-02"
mkdir -p "$OUT"
cmake --build build --target components-tests openmw-tests openmw esmtool -j2
./build/components-tests \
  --gtest_filter='ESM4RuntimeState.*:ESM4PlayerMechanics.*:ESM4InventoryMechanics.*:ESM4Detection.*' \
  --gtest_output="xml:$OUT/components.xml"
./build/openmw-tests \
  --gtest_filter='OblivionAiTest.*:OblivionProfileServicesTest.*' \
  --gtest_output="xml:$OUT/engine.xml"
python3 -m unittest discover -s scripts/tests -p 'test_*.py'
DATA='/home/maciek/.local/share/Steam/steamapps/common/Oblivion/Data'
python3 scripts/oblivion_compat.py baseline --build "$PWD/build" \
  --oblivion-data "$DATA" --census-all --require-lossless-tes4 \
  --hash-archives --output "$OUT/content"
python3 scripts/oblivion_compat.py m14-audit --oblivion-data "$DATA" \
  --count-lock scripts/data/oblivion_compat/oblivion_m14_data_counts.json \
  --output "$OUT/m14-audit"
./build/esmtool obscript "$OUT/obscript.json" "$DATA/Oblivion.esm"
```

The census inspected 11/11 plugins and 17/17 archives: zero census failures,
unsupported record families, or unallowlisted skips. The semantic M14 audit
matched the checked-in count lock: 7,668 packages, 8,288 pathgrids, 35,565
cells, 3,636 actors and zero invalid references. These checks establish data
preservation and existing M14 semantics, **not** M15 combat behavior.

The master-only ObScript audit (`obscript.json`) compiled 9,992 units with
zero frontend failures (2,031 object, 265 quest, 97 effect, 5,718 dialogue
result, 1,881 quest result). This is not the full installed-plugin S14 frontend
audit and does not prove runtime command implementation.

Master SHA-256:
`a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`.
`DLCShiveringIsles.esp` is still an 85-byte, zero-record stub. The master does
contain SE-prefixed records; the stub's count must not be interpreted as
either expansion runtime coverage or proof that the master has no SE data.

### Rendered baseline and limitations

The unchanged `oblivion_m14_city_schedule.json` passed its actor/event/save
checks in `m14-city/`. Both `city-morning.png` and `city-navmesh.png` were
opened and inspected. Geometry is textured and nonempty, but the crosshair
and HUD contain magenta missing-texture regions. The camera looks down over
the player/body and ground; these frames do not prove visible NPC movement.
The second frame contains navigation lines. **Visual acceptance fails** for
the missing UI textures; do not inherit this as an M15 visual pass.

The unchanged `morrowind_m14_regression.json` passed in `morrowind/`, including
its no-T4ST save assertion. Its inspected capture shows sky, rock geometry,
a textured TES3 HUD and crosshair, with the camera against terrain. It is a
profile/save-isolation smoke test, not evidence of playable Balmora traversal,
TES3 combat, crime, or jail. Those regression courses remain required.

`oblivion_m13_item_matrix.json` passed in `m13-items/`, with variables
`scene=prison`, `start=ImperialDungeon01`. Its event-injected inventory setup
and same-process quickload are existing M13 characterization only; they cannot
satisfy M15 normal-action or fresh-process restart requirements. These runs
all use `--no-sound=1` and establish no audible-output behavior.

For the three scenario commands the shared arguments were:

```sh
python3 scripts/oblivion_compat.py scenario MANIFEST --output FRESH_OUTPUT \
  --variable "source=$PWD" --variable "openmw=$PWD/build/openmw" \
  --variable "resources=$PWD/build/resources" \
  --variable "oblivion_data=/home/maciek/.local/share/Steam/steamapps/common/Oblivion/Data"
```

Both M13 prison captures were subsequently opened and inspected: textured
stone room/window and skeletal debris are visible, with matching magenta HUD,
crosshair and minimap defects before and after quickload. Neither frame
visibly establishes the claimed equipment state. Keep the inventory/save
assertions separate from equipment visual acceptance.

M13 additionally used the two variables above. The Morrowind manifest used
`--variable 'morrowind_data=/home/maciek/.local/share/Steam/steamapps/common/Morrowind/Data Files'`.
Exact commands and substitutions are also recoverable from each scenario
report and its generated configuration. All three outputs were fresh.

## S0 observability: compiled call-site inventory

The baseline handoff was committed as `85616edb5c`. The next bounded change
adds `static_calls` to each unit in the existing `esmtool obscript` JSON
report. This is the narrow baseline observability allowed by S0: global
command counts could not identify which event/context in a selected campaign
uses an unavailable command. No gameplay, compiler lowering, canonical
program representation, or runtime command behavior changes.

Each call includes compiled command spelling (including aliases), entry-point
and instruction indices, event and runtime event arguments, source line and
column, argument count and member-call status. Calls are emitted in program
order without deduplicating separate sites. Conditions are included because
they also require real command implementations. `null` means compilation
did not produce a program; `[]` means a compiled program contains no calls.
Neither means the runtime successfully exercised a campaign. Conditional
calls, including unreachable branches, are deliberately retained for audit.

Evidence is in `S0/static-calls-01/`:

- `baseline-failure.log`: the old report fails the new assertion requiring
  per-unit calls for `ArenaAggressionScript`.
- `components.log`: first attempt, 58/59 passed. One new test incorrectly
  expected lower-case event spelling; inspection of `Compiler::compile`
  established that events preserve source spelling while runtime arguments
  are case-folded. The corrected test explicitly checks both contracts.
- `components-corrected.xml`: 59/59 affected cases, including four new
  `ObScriptStaticCalls` cases. Tests compile synthetic scripts, exclude
  comments/strings/member stores, preserve distinct calls and aliases, check
  source/event/argument context, prove observation does not change canonical
  programs, and execute an untaken branch to demonstrate that a static call
  is not runtime coverage.
- `all-components.xml`: 1,621/1,621 component tests passed.
- `all-engine.xml`: after rebuilding `openmw-tests`, 546/546 engine tests
  passed. Neither full suite reports disabled or skipped cases.
- `python.log`: all 76 Python tests passed.
- `obscript.json` / `export-validation.log`: the master has 9,992 units and
  32,747 static calls. Arena's four `StartCombat` sites and its filtered
  `OnHit Player`/`StopCombat` site were checked against the original scripts.
  Removing the new field yields exactly the baseline report, including all
  source/reference/AST/program fingerprints and diagnostics.
- `official-obscript.json`: all eleven installed official plugins produce
  11,098 units, 39,845 static calls and zero frontend failures.
  `official-command.json` retains the exact ordered command arguments.

`tested-implementation.json` records binary SHA-256 hashes and executed XML
counts. The tested implementation diff against `85616edb5c`, restricted to
`program.hpp`, `program.cpp`, `apps/esmtool/obscript.cpp` and
`apps/components_tests/obscript/frontend.cpp`, has SHA-256
`b2c7eb4f1d13ab446caa67b7842cb97c4e210f09489ef775d6c2d167eb18b920`.
The full test runs validate this audit change, not the unimplemented S1–S14
requirements. No M15 runtime acceptance is claimed.

The audit command is unchanged:

```sh
./build/esmtool obscript OUTPUT.json /absolute/path/Oblivion.esm [other-plugins...]
```

The calls are static dependencies, not a control-flow proof, resolved dynamic
receiver identities, winning-record filtering or a declaration of supported
runtime commands. These are still separate S0/S12 tasks. In particular, an
original script's legitimate `Kill` or `SetStage` is distinguishable from
harness mutation by its unit and event provenance; banning the command name
globally would break the tutorial.

## Campaign discovery (not yet a complete S0 gate)

All forms in this section belong to `Oblivion.esm`. Use stable keys prefixed
`content:oblivion.esm:`; never use these six-digit IDs as load-order indices.
The following observations are from the installed master and its freshly
compiled corpus. Official-DLC override/condition traversal remains required
before treating them as the final winning campaign specification.

### E3: first Pit Dog Arena match

Selected target: normal registration with Owyn, receive/equip Battle Raiment,
request the first match, defeat the first Yellow Team Pit Dog, return to Owyn,
receive the first-win reward once, and return to the next ready state.

| Role | Form / original unit | Required predicate or dependency |
| --- | --- | --- |
| Owyn reference | `127672`, base `0222b6`, Bloodworks `037868` | Actual dialogue/conditions/results |
| Arena quest | `02991f`, script `02a24a` | Gates and persistent quest variables |
| ReadyForAMatch first result | INFO `00c0d0` | Sets readiness, enables/moves the original opponent, starts Arena/Aggression/Announcer/Disqualification |
| Match gate | `127670` | Raiment eligibility via equip/unequip events |
| First opponent | reference `18ae5b`, base `02a2b1` | Initially held in `18ae56`; legitimate result moves it to match marker `091b7d` |
| Match cell | `091b46` | Do not confuse `ICArena` (`037867`) with this fight cell |
| Team gates | `091b7e`, `091b7f` | Original timed unlock/activation |
| Aggression script | `01e643` | Resolves `Combatant0ARef`; writes aggression and calls `StartCombat Player` |
| Combatant script | `02a2b8` | Physical `GetDead` increments killed count and sets `FightOver` once; blocks corpse looting |
| First reward | GREETING INFO `027668` | 50 gold, fame increment, first-win state, `ResetInterior`, quest stops, gate lock and Arena stage 10 |
| Idle reset | quest `02991f`, stage 10 entry 0 | Clears match variables and stops Arena |

Acceptance must assert named opponent death, `CombatantsKilled` advancing
from 0 to 1, lawful combat without assault/murder bounty, reward delta 50
exactly once, authored reset, and restart before fight/after victory before
reward. Arena corpse looting is explicitly forbidden by its original script;
do not reuse the dungeon corpse-loot expectation here.

Unclosed prerequisite cases: initial registration and raiment choice INFO
conditions; dialogue selection/result execution; faction-rank changes;
`StartCombat`/`StopCombat`/`StopCombatAlarmOnActor`; physical `GetDead`;
`ResetInterior`; event attribution; announcer dialogue/audio. Inspecting source
or compiling these commands is not an implementation result.

### E2: Vilverin candidate

Inspected first cell: `01663a`; further cells: `0463f5`, `0463f4`, `0463f3`.
The first cell contains these exact bandit references/bases:

| Reference | Base | Editor identity |
| --- | --- | --- |
| `06c356` | `06c355` | VilverinBanditAmbushNPC |
| `06c359` | `06c357` | VilverinBanditSittingNPC |
| `06c35b` | `06c35a` | VilverinBanditBoss |
| `06c35c` | `06c357` | VilverinBanditSittingNPC |
| `06c35e` | `06c35d` | VilverinBanditFurnitureKhajiit |

These are candidate targets, **not a locked route**. Required remaining work:
door/return graph, actor inventory and style, ranged/creature/cover encounter,
loot and reset dependencies. `VilverinNewBossScript` (`0c80fb`) enables a
replacement on leaving Vilverin04; a full route must account for that
trigger instead of assuming all cleared state is permanent.

### E1 and E4–E5

Tutorial cells inspected: `ImperialDungeon01` = `01fbb9`,
`ImperialDungeon02` = `022288`, `ImperialDungeon03` = `022282`,
`ImperialDungeon04` = `022ff6`. The existing M14 escort manifest alone cannot
establish tutorial combat provenance, loot or progression. Exact combat
targets and legitimate start/end saves remain to be locked.

The static inventory further identifies the first authored ambush in
`ImperialDungeon01`:

| Actor reference | Actor base | Script | Package-completion target |
| --- | --- | --- | --- |
| `014a30` | `014a27` | `014a22` | Glenroy |
| `014a2c` | `014a28` | `014a24` | Glenroy |
| `0178c3` | `017617` | `017616` | Baurus |
| `0178c4` | `017619` | `017612` | Renault (`RenoteRef`) |

The four original `OnDeath` callbacks advance CharacterGen to stage 26 once
`ambushCount` reaches four, then reset that counter. Their `OnDeath Player`
blocks separately update player-kill attribution. Activation of an eligible
dead actor sets MQ01's loot state/stage 18 when MQ01 stage 15 has been done.
The first two scripts intentionally call `RenoteRef.Kill` in `OnHit` and
advance CharacterGen to 24. A test that requires **all** escorts to survive
this ambush would contradict original content: protect the actually essential
escorts, but preserve Renault's authored death. Exact weapon acquisition,
quest-condition progression and the legitimate starting save still need
normal-interaction proof.

Two selected prison environments for further tracing are the Imperial prison
cell `02c17c` (player-cell door `0937ea`) and Chorrol Castle Dungeon `02898e`
(player-cell door `09502f`). The ordinary jail INFO `0281b7` calls `GoToJail`
and locks all eight city player-cell doors. Escape variant INFO `0281b8` also
clears `Crime.PCEscaped`. Pay-gold INFO `027fc8` calls `PayFine`.

Conditional branches demonstrably exist: `TGPayCrimeGold` INFO `0983a3`
calculates half the bounty; `TGPayFines` INFO `0983a6` removes the assessed
gold and clears crime gold. `PayFineThief` also appears in INFO `0281b9`.
Their exact eligibility conditions, guards, evidence/belongings/bed/release
references, identity-specific branches and formula probes remain open. Do
not mark guild/identity alternatives inapplicable.

## Confirmed implementation boundaries

- `components/esm4/runtimestate.hpp` still declares schema version 7.
- `OblivionScriptManager` returns constant zero for `IsInCombat` and
  `IsPCAMurderer`; `StartCombat`, `StopCombat`, `SetCrimeGold`, `SetEssential`,
  `SetUnconscious`, ownership and faction-rank writes remain deferred.
- `esmtool` raw CSTY preservation is not typed combat-style simulation.
- Existing inventory and AI tests exercise previous milestone contracts;
  their green results establish no combat/crime/jail service implementation.

## Handoff

### S0 inventory closure

The initial incomplete discovery below is superseded by the checked-in
[case inventory](M15-CASE-INVENTORY.json). It declares all 18 mandatory
feature families, eleven crime-resolution/identity cases and E1–E4 routes,
with owning stages, oracles, restart boundaries and evidence destinations.
These are pending gameplay cases, not inherited passes.

`S0/inventory-02/inspect_winners.py` inspected the eleven fingerprint-matched
official plugins in their audited load order. `identity-validation.json`
checked 60 initial declared form occurrences against nondeleted winners;
`inventory-03/` additionally retains actor flags/factions, equipment/leveled
lists, GMST input bytes and all 129 winning CSTY records. The missile list's
ten level-1 variants have no SPLO and use `NPCBanditMissile` (`0ca0d7`), whose
CSTD payload was inspected directly. Fixed bandits and the selected Arena
opponent omit ZNAM; S2 must establish verified native default-style semantics.

Vilverin is now a locked **entrance-floor circuit**, including all five fixed
actors and three leveled origins (`04bb72`, `04bb73`, `06bfba`), two of which
are archers. Enter through `066ac8`/`016b61`, complete the entire circuit with
cover/elevation and corpse loot, traverse `049424`/`049b4a` into Vilverin02,
return and exit, then reenter and verify all eight cleared/looted states. The
eight required encounters cannot be satisfied by the five melee actors alone.
The declaration deliberately does not claim the full four-level boss clear.

Arena registration INFO `029ca6` sets combatant rank 0 and ArenaDialogue stage
20 and unlocks the raiment cabinets. Heavy/light results `00c1b7`/`00c220`
grant the corresponding raiment. Their original conditions and linked topic
IDs are retained in the winning record inventory; the original first-match
conditions include Owyn identity, armor state, combatant rank, quest variables
and time. S12 must execute these conditions/results, not reproduce their
effects from harness commands.

Imperial/Chorrol prison pairs resolve to markers `09316e`/`028ce5`, release
links `03f288`/`028ce6`, evidence chests `08526f`/`02b7b2`, and the declared
cell doors. Player-owned bed candidates nearest the markers are
`00a947`/`02b05a`; normal activation and geometry must verify these at S11.
No second belongings container is invented: determine actual native property
storage behavior in the jail probes. E4 uses Rindir `01d15e`, his owned placed
staff `0a6cea`, shop `02c16d` and private quarters `049af4`. Each independent
branch resets from legitimate initial provenance; no crime is pre-satisfied.
The cowl identity dependency is real `TGGrayCowlScript` (`03a82b`), in addition
to the audited guild half-fine dialogue.

Original-game probe families are inventoried, with executable/content hashes,
procedure, independent expected values, rounding/clamps, state and media
required. `Oblivion.exe` and Proton Experimental's Wine are installed; no
original-game probe is claimed yet. The former `/tmp/m14-example-suite.gJaDUq`
checkout is absent; reacquire pinned revision
`a41b44d9403ff3f8a1505c1b4bc152c4dd623b64` for the external Lua gate. GCC is
available; Clang was not found in PATH or `/usr/bin`. Reserve separate
`build-m15-{gcc,clang}-{debug,relwithdebinfo}` and `build-m15-sanitized`
directories. These are explicit later acceptance gates, not prerequisites
silently waived by S0 closure.

S0's baseline reproductions and inventories are complete. Visual baseline
defects remain classified and must be fixed/compared before media acceptance.
S1's next bounded task is the typed fail-closed evidence contract and normal
action isolation, starting with tests that reject the current permissive
handling of an unknown `m15` manifest section.

Commits completed so far:

- `85616edb5c`: initial baseline, visual discrepancies and campaign findings.
- `3b5424562a`: compiler-derived call-site inventory, four regression tests,
  full component/engine/Python results and further tutorial findings.

Earlier handoff (superseded by S0 closure above): no passed M15 stage. No source gameplay
changes or acceptance metadata changes have been made. Continue with the
smallest open S0 case: lock the full tutorial/dungeon route and remaining
Arena/guard dialogue conditions against winning installed content, then
finish original-game/formula probe and requirement inventories. Preserve the
baseline visual discrepancies and all generated outputs. Do not advance to
combat implementation while these routes remain unspecified.

### Reviewing the current progress

The existing workspace is on `master` with both changes committed. Nothing
has been pushed. To inspect this checkpoint in a separate directory without
switching or resetting the current worktree:

```sh
git worktree add --detach ../m15-review 3b5424562a
git log --oneline 72515455b3..3b5424562a
git show --stat 3b5424562a
```

The generated evidence remains in the original workspace's
`build/oblivion-compat/m15/S0/`, not in the new worktree. In the original
workspace, rerun the audit tests with:

```sh
cmake --build build --target esmtool components-tests openmw-tests -j2
./build/components-tests --gtest_filter='ObScriptStaticCalls.*'
```

For the full checks used for this change, run `./build/components-tests`,
`./build/openmw-tests`, and the Python discovery command above. Build a new
worktree with its own CMake cache if testing there; do not reuse the original
worktree's cache. To inspect the new output, run the `esmtool obscript`
command above and look at `units[].static_calls`. The official audit JSON
contains proprietary script source and must remain local.

There is no newly playable M15 combat/crime/jail implementation to launch at
this checkpoint. In particular, running `scripts/run-oblivion.sh` still runs
the pre-M15 mechanics. The remaining work is the rest of S0 and all of S1–S14.

### S1 contract checkpoint (stage remains open)

The runner now recognizes a strict M15 contract with typed nested actions and
case assertions, setup/exercise/observation boundaries, normal-input controls,
fingerprinted content and executable, fresh output/run/process identities,
immutable capture receipts, required artifacts, and one result for every case.
Actor/target/action and optional incident/projectile/transaction identities
scope ordered events. Numeric deltas must agree with independently parsed
native-save state; exact canonical field comparisons preserve nested item
metadata and boolean types. Missing fields never become zero/default values.
Generic subprocesses, console input, input-binding overrides, generated event
files, inherited script hooks, stale streams and edited captures are rejected.

Validation at `build/oblivion-compat/m15/S1/contract-01/`:

- `python-full.log`: 104 Python tests passed (28 M15 tests plus 76 existing).
- `schema-agreement.log`: independent `jsonschema 4.26.0` draft-2020-12
  validation agreed with the runtime structural validator on 816 mutations;
  the parent scenario schema also accepted every structurally valid mutant.
  The independent dependency is isolated in `build/m15-schema-venv`; the
  production runner remains standard-library-only.
- The first city regression failed with a binary/resources revision mismatch,
  despite passing AI evidence checks. A normal `openmw` rebuild completed;
  the fresh `m14-city-rebuilt` course then passed, with all actions completed,
  zero exit status and no timeout. This is automated AI acceptance, not a
  resolution of the previously documented visual defects.

The tests use deliberately mocked event/state fixtures at the engine boundary;
none establishes combat, stealth, crime, actual runtime save observation or
restart acceptance. Engine emission and completed-save acknowledgments are
still required. Restart continuation currently fails explicitly instead of
pretending an in-process reload is a second process. Audio artifacts likewise
cannot pass without an implemented capture path. Keep S1 and S2–S14 open.

### S1 native observation plumbing (stage remains open)

Native TES4 worlds can now opt into an engine-owned observation stream. It
records the real PID, run/epoch, monotonic tick/sequence, save/load completion,
actual disk-save SHA-256, and separately serialized live native state. Shutdown
summaries are explicit; unwinding/destruction cannot manufacture a successful
summary. Script/frame diagnostics are counted, and the runner independently
rejects all engine error, unsupported-command and sanitizer log findings.
This sink has no actor/world mutation API. Observation failures leave normal
save/load processing intact and fail evidence validation. TES3 worlds do not
create this sink.

Snapshots require the matching completed-save acknowledgment, copied save
bytes and native parser, and exact equality with the independently serialized
live state. A second snapshot cannot reuse an earlier save acknowledgment.
Canonical JSON now retains full float values and a fixed numeric locale.
The declared seed is passed through the actual engine `--random-seed` option.
The fully expanded scenario is retained and checksummed; declared archives
must also resolve to fingerprinted inputs. Ignored key timing fields are
rejected; held keys use the runner's actual timing contract.

Evidence in `build/oblivion-compat/m15/S1/observation-01/`:

- GCC RelWithDebInfo engine, component and engine-test builds completed.
- `components.xml`: 1,628 tests passed; `openmw.xml`: 546 tests passed.
  `test-inventory.json` compares exact names against unfiltered binary test
  inventories and rejects skipped/disabled/incomplete cases.
- `python-tests.log`: 111 tests passed, including 30 M15 evidence tests and
  five fixture-builder tests. Seven C++ observation tests cover protocol,
  interrupted shutdown, malformed/nonfinite input, diagnostics, file hashes,
  state immutability and full float serialization.
- `course/`: the first live fixture run copied two real normal-key quicksaves
  and matched each independently parsed save to its engine live-state capture.
  **The course failed.** Missing legacy UI/weather/animation resources and a
  missing default FaceGen ear texture produced engine errors; the screenshot
  was nearly black. The initial observation delta also used `health` instead
  of native `health.current`; this field mismatch was corrected and focused
  native-state tests rerun. No original evidence or expected result was edited.

The fixture start/camera and shared-presentation resource dependencies need
repair, with a fresh run retained separately. Two-process continuation and
its negative controls are still open. No combat, stealth, crime, jail,
official campaign, or full S1 acceptance is claimed by this checkpoint.

The editable fixture source is
`scripts/data/oblivion_compat/m15_observation_fixture.json`. Regenerate it into
an empty destination with:

```sh
python3 scripts/tes4_m15_fixture.py \
  "/path/to/Oblivion/Data/Oblivion.esm" \
  build/m15-fixture/m15-fixture.esm
```

It selects boot/appearance records from the pinned local master and authors a
floor grid. The 42 KB output has 482 records, including 25 placed floor pieces,
zero scripts and zero quests. Both Python and `esmtool -q -C dump` reopen it;
`fixture-graph.json` reports 76 references, zero unresolved targets and zero
enable-parent cycles. Generated licensed records remain local build artifacts.
The checked-in `oblivion_m15_observation.json` course is deliberately still
failing on the blockers listed above; it is a regression target, not accepted
runtime evidence.

### S1 presentation blocker repair

The Oblivion profile now registers an engine-authored shared UI archive before
normal game/mod archives. Its editable C++ primitives produce 106 named DDS
textures for frames, controls, bars, cursor, crosshair, book navigation and the
damage overlay; three icon aliases use actual native Oblivion archive bytes.
Replacement resources retain their normal priority. Unknown textures/models
still fail normally: this is not a catch-all missing-resource replacement.
It provides functional shared presentation, not the M19 Oblivion UI-parity goal.

The released human-ear NIF default now resolves to the existing native Imperial
ear texture, preserving a mod's explicit replacement if supplied. Oblivion's
optional common-asset preload list filters unavailable TES3 weather/swimming
assets; actual asset requests still use the normal diagnostic loader.

`S1/observation-02/` retains the first passing strict observation run, its
screenshot, all 1,632 passing component tests, 546 passing engine tests, and a
passing Morrowind isolation scenario. Seven focused resource tests decode every
generated DDS, check transparent sight/damage regions, preserve mod priority,
require native icon bytes, and verify ear-alias behavior. Visual inspection
confirmed readable Oblivion HUD bars, native hand icon and crosshair without
magenta missing textures. The Morrowind capture preserves its existing HUD and
previously documented close-to-terrain camera. No TES3 content/archive was added
to the Oblivion fixture to achieve this result.

The first passing image still showed sparse geometry: native NIF inspection
measured the selected "floor brick" at approximately 91 by 49 units, not a
complete modular room. That fixture-authoring mistake is being corrected in a
separate recipe change. It does not waive visual acceptance of gameplay courses.

### S1 room-fixture correction

The recipe now places one complete `ICGroundFloor01` interior instead of the
small brick-piece grid, and the start is anchored to its named reference.
This produces 458 records with one placed room, zero scripts and zero quests.
The course uses actual held-key timing, normal camera input, and a short
post-save presentation settle. Neither assertion thresholds nor outcomes were
relaxed. The hash-consistent native snapshots must also contain the declared
room's exact key, base, cell, enabled and deleted state; an empty scene fails.

`S1/observation-03/` contains the fresh **passing single-process observation
course**, structural/native graph readback, and 113 passing Python tests.
The inspected screenshot shows the room's continuous floor, walls and ceiling,
readable shared HUD and native icon. This accepts the narrow read-only save
observation course, not M15 gameplay or S1 as a whole. Fresh-process continuation
and offline evidence-corruption replay are the next open S1 work.

### S1 offline evidence replay

`oblivion_compat.py m15-verify <run-directory>` independently reopens final
receipts, launch provenance, event identities, every native save/live snapshot,
required references, diagnostic counts and causal case assertions. It does not
launch inputs or rewrite the evidence. Original executable/content fingerprints
remain provenance, so checking out another revision does not invalidate replay.
Receipts provide integrity against the trusted runner's record, not a signature
against an attacker who rewrites the entire evidence bundle.

`S1/replay-02/course` passes both execution and offline replay. Ten copies with
edited save/state bytes, stale or missing capture, old run identity, missing
summary, hidden command, engine error, incomplete actions or changed phase all
fail replay; the original still passes. The first phase-control attempt changed
an already-setup action and therefore correctly passed; the corrected control
changes an actual exercise action. Both attempts remain local evidence.
All 119 Python tests pass. Freshness uses an exclusive filesystem origin and
same-filesystem timestamps, avoiding a transient wall-clock/inode comparison
failure; repeated timing experiments did not reproduce its original cause.
Fresh-process continuation remains outstanding; this does not close S1.

### S1 fresh-process load crash repair

The new restart reproducer exposed a graphics-thread crash in both
`S1/restart-01` and `S1/restart-02`, before a load acknowledgment. The retained
native backtrace shows `GLObjectsVisitor` traversing a group while the loading
screen draws during cell insertion. A separate synchronous-rendering diagnostic
loaded the same save successfully; it is diagnostic evidence, not acceptance.

Loading now joins graphics work and uses synchronous draws for the outer loading
interval, restoring the configured rendering model when that interval ends.
Nested loading scopes retain the original model. This protects scene mutation
without changing normal gameplay rendering policy.

The strict two-process courses `S1/restart-03` and `S1/restart-04` both pass with
the normal renderer configuration: exact first-save bytes load in a distinct
PID/epoch, independently captured live state equals the decoded save, and normal
movement/save input produces a new acknowledged save. The second-process image
was inspected: complete room, readable HUD and no missing-texture presentation.
All 546 unfiltered engine tests pass with an exact inventory check, and
`S1/restart-morrowind-01` passes the existing Morrowind isolation/save course.
The tested loading-screen diff SHA-256 is
`afaedea8655318d3aae7b598ff8d8e7b1e06a609f537cbc6cb6b052b097d4d9b`.
Restart driver/schema/tests are the next separate commit.

### S1 fresh-process driver and replay

`m15-restart <first-manifest> <continuation-manifest> --output <fresh-directory>`
launches separate configurations/user-data directories. It first independently
verifies the completed source course, then copies its immutable snapshot bytes
into the second process's normal save slot. Only the trusted driver may add
`--load-savegame`; manifests cannot supply that option or launch a continuation
without source provenance. The second epoch retains the logical run identity,
requires a different PID, and records the source digest and engine/content
fingerprints. Load snapshots require an actual native `load-complete` boundary.
Both courses must pass independently, exact canonical state must survive the
load, and the continuation must produce a later ordinary save.

Reproduce using the same installed content and generated fixture:

```sh
python3 scripts/oblivion_compat.py m15-restart \
  scripts/data/oblivion_compat/oblivion_m15_observation.json \
  scripts/data/oblivion_compat/oblivion_m15_continuation.json \
  --output build/oblivion-compat/m15/S1/my-fresh-restart \
  --variable openmw="$PWD/build/openmw" \
  --variable resources="$PWD/build/resources" \
  --variable fixture_data="$PWD/build/oblivion-compat/m15/S1/observation-03/fixture" \
  --variable oblivion_data='/home/maciek/.local/share/Steam/steamapps/common/Oblivion/Data'
python3 scripts/oblivion_compat.py m15-verify \
  build/oblivion-compat/m15/S1/my-fresh-restart --restart
```

All 125 Python tests pass. Independent JSON Schema validation agrees on 816
structural mutations. Six copies of the actual passing restart evidence with a
changed transfer, changed load acknowledgment, reused PID, edited source save,
missing resave or absent second process all fail; the original still passes
(`S1/restart-controls-01`). The two retained successful native courses are
`S1/restart-03` and `S1/restart-04`; renderer repair is commit `345087c991`.
These establish observation and persistence infrastructure, not combat/crime
acceptance. Final S1 review still needs to align the event tick with simulation
updates rather than paused presentation frames.


### S1 closure: simulation ticks and acceptance ledger

Observation ticks now advance with unpaused simulation updates, after input and
pause-state processing, rather than every presentation frame. Paused/no-game
frames still permit lossless evidence without inventing simulated time. The new
component test failed first (`S1/ticks-red.log`), then passes after the fix.
The normal-input pause course opens the menu for three seconds at 60 FPS:
only 29 active ticks separate the two save boundaries, below the predeclared
60-tick active-work bound. Its distinct-process continuation also passes
(`S1/ticks-restart-01`, `ticks-pause-check.json`). The editable course is
`oblivion_m15_pause_observation.json`.

| Requirement | Owner / level | Oracle / result | Retained evidence |
| --- | --- | --- | --- |
| M15-S1-01 typed nested contracts | Python / L1 | Strict discriminants, fields and paths; independent schema agrees on 4,368 mutations | `restart-schema-complete.json`; 8 manifest tests |
| M15-S1-02 phase, freshness, source identity | Runner / L1+L3 | Only ordinary inputs; exclusive outputs, real seed, input/executable fingerprints, scoped actor identities | 12 session tests; `replay-02/course` |
| M15-S1-03 independent causal state | Verifier / L1+L3 | Named actor deltas, exact/tolerant paths, missing values fail; decoded saves equal separate live observations | 12 causal tests; `observation-03`, `restart-03`/`04` |
| M15-S1-04 fail-closed aggregate | Runner / L1+L3 | Every case/artifact/action, diagnostics and completion summary required | 5 replay tests; 10 corrupted actual courses in `replay-02` |
| M15-S1-05 fresh-process restart | Driver / L1+L3 | Same source bytes, distinct PID/epoch, real load acknowledgment and ordinary resave | 6 restart tests; 6 corrupted actual pairs in `restart-controls-01`; `ticks-restart-01` |
| M15-S1-06 required test counts / isolation | Verification / L1+L3 | Missing filters/skips fail; full inventories and prior-profile scenarios pass | 1 inventory test; `ticks-inventory.json`; `restart-morrowind-01`; `contract-01/m14-city-rebuilt` |

Latest full suites: **1,633 component tests**, **546 engine tests**, and
**125 Python tests** (including 44 M15 contract/evidence/restart tests and five
fixture tests). Eight native observation tests cover immutable state, exact
bytes, identity, closing counts, invalid events, missing saves and pause ticks.
All requested tests ran; none were skipped. Native suite inventories were
compared against each binary's unfiltered listing.

The tested simulation-tick source/test diff SHA-256 is
`b9c55975361b94d221f76875d25aa510cb1d3369c4968661634a181dfcb5e2a2`.
S1 is accepted only as evidence infrastructure. No physical combat, stealth,
crime, arrest or jail capability is claimed. S2 begins with typed CSTY decoding
and independently sourced rule inputs; all later gameplay and universal gates
remain pending.

### S2 first chunk: typed native combat styles

`CombatStyle` now decodes the six verified TES4 CSTD layouts and optional CSAD,
keeps historical missing tails explicit, validates lengths/finite values/domains,
and retains every original logical subrecord including padding/unknown fields.
It is registered in record includes, the native winning store, loader dispatch
and `esmtool`. Later-game records retain their raw inspection path and are not
misclassified as TES4 combat policy. The typed store test covers differently
ordered master lists, a winning override and an empty deletion tombstone.

The original raw reader accepted the deliberately invalid NaN fixture
(`S2/csty-01/baseline-invalid-read.*`); the typed reader now rejects it. All 11
installed official plugins pass native readback. Independent grouped-layout
Python decoding agrees with native dumped probabilities, timers, flags,
applicable switch-distance/acquisition fields and representative advanced
modifiers for **all 129 styles**, covering every historical layout.
See [the pinned data/rule provenance](M15-NATIVE-RULE-PROVENANCE.md).

Six focused component tests cover length/domain boundaries, signed bonuses,
compressed records, raw preservation, duplicates, missing data and wrong-game
layouts. All **1,639 component tests** and **547 engine tests** pass, with exact
unfiltered inventory checks (`S2/csty-01/test-inventory.json`). The existing M14
city schedule course also passes after loading the new native style store.
Its capture retains the earlier elevated/downward camera framing, so this is
an AI regression check, not M15 combat presentation acceptance.

The tested native source/test diff SHA-256 is
`97eb774bd66092220ac240a750baca2b6a70953e9f023ea22b3cfcd82e93b964`.
An initial test compile used the wrong FormKey method name and was repaired;
all attempts remain in `build*.log`. ASan/UBSan compilation is underway in the
separate `build/m15-sanitize` tree and is still an open S2 gate. This chunk does
not resolve runtime defaults, formulas, mastery, equipment/sound/jail audits,
or original-game probes. S2 remains in progress.

### S2 second chunk: independent semantic inventory

Commit `8027351a29` contains the preceding typed CSTY chunk. The new
`m15-audit` command independently decodes grouped CSTD layouts, applies winning
master identities and deletion tombstones, checks actor style references, and
inventories typed GMSTs and faction crime flags/multipliers. It deliberately
returns failure while the independent rule/default gates remain open.

```sh
python3 scripts/oblivion_compat.py m15-audit \
  --oblivion-data '/home/maciek/.local/share/Steam/steamapps/common/Oblivion/Data' \
  --output build/oblivion-compat/m15/S2/audit-01
```

The 11 installed plugins yield 129 styles, 3,636 actor bases, 383 winning
settings, and 495 factions. Structural/decoded-data checks pass; 2,218 actors
have no explicit combat style and remain listed for verified default-policy
resolution. The master independently contains 382 distinct GMST FormKeys;
the additional setting comes from the DLC. This is partial inventory coverage,
not closure of equipment, creature attack, relationship, sound, animation or
jail-reference semantics. Six new Python tests cover independent layouts,
malformed values, master resolution/deletion, missing-style policy, setting and
faction domains, and invalid headers. The complete Python suite passes 131
tests (`S2/audit-01/python-tests-final.log`). Baseline command absence and the
current intentionally failing overall audit are retained in that run directory.

### S2 equipment, creature and relationship audit

The expanded independent inventory now covers 1,401 weapons, 134 ammunition
records, 1,112 armor records and 992 creature bases. It decodes physical input
units, resistance flags, armor hundredths, creature reach/attack damage,
individual sound slots, animation/model lists, faction relationships and actor
membership. Enchantment/staff execution remains explicitly M16-owned; records
are retained rather than treated as nonmagical. Referenced enchantments,
sounds, inherited-sound creatures and factions must resolve after overrides
and deletions. All installed data checks pass (`S2/audit-04/m15-audit.json`).

New cases M15-S2-AUD-07/08 cover padded equipment layouts, malformed sizes,
nonfinite input, all creature sound slots and dangling sound references;
AUD-09/10 cover native local-index resolution and faction deletion. The full
Python suite passes 135 tests (`S2/audit-02/python-tests-final.log`). The first
expanded tests failed on absent inventory fields. A proposed strict FormID
index check also failed against the shipped master (`audit-03` attempt):
`0100110b` is a source-file identity under the existing native FormKeyResolver.
The audit now follows that reviewed resolver policy, with a regression test;
it does not introduce a different identity interpretation.

The dedicated GCC Debug ASan/UBSan build completed. All six CSTY component
tests pass under both active runtimes (confirmed by binary dependencies), all
11 installed plugins parse cleanly, and the malformed NaN fixture is rejected
with the expected parser diagnostic and no sanitizer report. Evidence is in
`S2/csty-01/sanitize-{components,native-read,invalid}.*` and each plugin's
`.sanitize.log`. These are parser checks, not runtime combat sanitizer gates.

The audit exposed existing native CREA losses: RNAM attack reach is skipped,
and repeated sound slots collapse into one legacy sound/chance pair. The next
bounded implementation preserves these typed fields and verifies malformed
sound sequences and native readback before using them in simulation. S2 rule,
default, ownership/jail and independent gameplay oracle gates remain open.

### S2 ownership and named prison topology

Commit `0592507cbd` contains the preceding equipment/creature/relationship
audit. The next audit chunk preserves separate reference and cell ownership,
faction ranks and global gates, checks 1,045,747 winning placed references and
3,605 explicit ownership entries, and resolves base/cell/door-destination links
without dangling targets. No gameplay access rule is inferred from these data.

Both S0-selected prisons are rechecked against their actual bed, chest, door,
guard and marker references. The first check wrongly assumed prison/release
markers were STAT records; all four are actually DOOR `PrisonMarker` (base
`000004`, independently dumped in `S2/audit-06/prison-marker-native.log`).
The corrected check requires DOOR bases and reciprocal prison/release teleports,
plus matching prison cells for bed/evidence/cell-door roles. It preserves the
Imperial City guard placement in the adjacent audited cell. Failed attempts
remain in `audit-06`; `audit-07/m15-audit.json` passes all data checks for both
prisons while the overall S2 rule/oracle gate correctly remains false.

Cases AUD-11/12 prove separate ownership layers, deleted-owner failure,
reciprocal marker requirements and wrong prison-role base rejection. The full
Python suite passes 137 tests (`S2/audit-06/python-tests.log`). The CLI prints a
compact summary and links the complete local inventory rather than duplicating
its million-reference report to stdout. Runtime crime, incarceration and
release are still pending; these checks establish the data topology only.

### S2 native creature combat input preservation

The native CREA reader now retains optional attack reach, all typed sound
slots/probabilities and stable inherited/individual sound identities. Soul is
correctly separated from its padding byte. Loading resets the reused record,
and malformed native DATA/RNAM/sound lengths, domains, duplicate reach and
unfinished/unordered sound entries fail explicitly. Later-game sound parsing
keeps its separate path. `esmtool` exposes the native fields for inspection.

The first malformed-reach/orphan-sound test failed against the old reader
(`S2/creature-01/red-tests.log`). Three focused creature tests now pass,
including lengths 0–29, padding, zero/100/101/255 chance boundaries, multi-slot
preservation and missing-data behavior. The complete normal build passes
1,642 component and 547 engine tests, with exact inventories and no skips.
All nine creature/style tests also pass with ASan/UBSan; all 11 official plugins
parse under those sanitizers without findings. Independent grouped-layout
Python readback agrees with native output for all **1,001 physical creature
records and 771 sound entries** across the 11 plugins (992 winning creature
bases after overrides). Evidence: `S2/creature-01/{test-inventory,
independent-native-comparison,sanitize-native-read}.json` and adjacent logs.

The unchanged M14 city schedule/save course passes with the rebuilt native
engine (`creature-01/city/scenario.json`). Both captures were opened: textures,
geometry, HUD and navigation overlay are present; the inherited downward
camera/player-body framing remains unsuitable as M15 visual acceptance.
The tested source/test diff SHA-256 is
`278ea4c425c935e887a1a2eef3a5a663769ab5574df9ec52b45baee9eb0ae950`.

An isolated original executable launch was also attempted in
`S2/original-01`, using a fresh local Proton prefix. It produced application
load error `P:0000065432` before gameplay; its screenshot and launch/Proton logs
are retained. The dialog was dismissed and that run exited. No original-game
behavioral probe has passed, and static initializer inspection is not counted
as one. The native rule/default oracle remains the next open S2 requirement.

### S2 pure legacy style-policy resolution

Commit `af7e01d1f1` contains the native creature chunk. A separate pure helper
now resolves missing style versus historical CSTD tails without mutating source
data. Original-executable static inspection demonstrated distinct range and
acquisition defaults, and load-time replacement of zero switch/rush values;
these are documented with original addresses in the rule provenance file.
A 63-row initializer catalogue records named setting facts and explicitly
requires winning content overrides. It is not substituted for live settings.

The deliberately unimplemented resolver first failed its new behavior test
(`S2/style-policy-01/red-tests.log`). Four tests now cover both policy paths,
all six historical sizes, zero versus next-representable-positive values,
incomplete defaults, invalid domains and preserved signed bonuses. All 1,646
component tests pass with an exact inventory; all four policy tests pass under
ASan/UBSan. Evidence: `style-policy-01/components-final.*`,
`test-inventory-final.json`, and `sanitize-final.*`. Tested source/test/docs
diff SHA-256: `af5d804a8ec5dbc23f8239f2d1714caeb692dbff022af51d82df19ff34e14352`.

The isolated original executable now reaches its 1.2.0416 main menu after
starting the normal Steam client (`original-03/startup.png` inspected).
Earlier load-error attempts remain retained. Headless input/activation is
still being investigated in `original-04`; reaching the menu is not a passed
behavioral probe. Steam is running on its own Xvfb display and the original
game uses the local isolated prefix. Native actor/settings integration,
advanced-style rules, physical/crime formulas, and original gameplay probes
remain open S2 work; S3–S14 are still pending.

### S2 reviewed input count lock

Commit `35ea91c811` contains the pure style resolver. The input audit now checks
an editable reviewed count lock against the exact ordered plugin names/hashes
and complete summary. Changed hashes, counts, order, missing families, empty
locks and wrong count types fail; the audit never updates its own lock.
The locked counts are the individually reviewed data families above, not a
runtime feature-completion count. A new negative-control test first failed on
the absent checker and now passes; the complete Python suite passes 138 tests.
`S2/count-lock-01/audit/m15-audit.json` passes both data and count-lock checks,
while overall acceptance stays false for the explicit rule/oracle gates.

### S2 advanced policy and first live original-setting observations

The advanced resolver now follows the independently inspected original choice
between authored CSAD modifiers and native setting defaults. Two additional
cases first failed on the unimplemented path, then passed with explicit missing
standard/advanced data and nonfinite-input diagnostics. Signed modifiers remain
signed. All 1,648 component tests pass with exact inventory; all six style-policy
tests pass under ASan/UBSan (`style-policy-01/advanced-*`). This is still pure
policy work, not a simulation service or an actor/settings-store adapter.

Original reference input became reliable with windowed rendering and background
input enabled in the isolated INI. The run completed normal Imperial character
creation, seven read-only live setting queries, tutorial-message dismissal and
a normal quicksave. The two settings captures were opened and each numeric
response checked; exact observations and hashes are in
`M15-ORIGINAL-SETTING-PROBES.json`. The preserved `original-04/prison-start.ess`
is local original-game evidence, not an OpenMW save. Earlier failed input/save
attempts remain in the input log; the first F5 was correctly refused while the
tutorial message paused the game. No actor values or quest state were injected.
The new probes narrow the oracle gap but do not satisfy physical/crime behavior
or original style-selection acceptance. All later stages remain pending.

### S2 typed native setting defaults

Commit `ced744e238` contains the advanced resolver and first live setting probes.
`buildCombatStyleDefaults` now builds all standard/advanced inputs from the 63
reviewed initializer facts, with case-insensitive native winning GMST overrides.
Present but wrongly typed, nonfinite or out-of-domain overrides fail explicitly;
only absent known settings use verified compiled fallback values. It accepts no
TES3 setting store. Duplicate/unnamed/null input records diagnose an invalid
winning inventory. Integer flag getters preserve native nonzero semantics.

Four tests first exposed the absent builder, then passed compiled-versus-live
30/5 bonus distinction, case handling, independent historical-tail behavior,
percentage boundaries, signed modifiers, invalid overrides and ambiguous input.
All 1,652 component tests pass with exact inventory; all ten settings/policy
cases pass under ASan/UBSan (`S2/settings-01`). Actor-store wiring is the next
bounded chunk. Gameplay formulas and their behavioral probes remain open.

### S2 native actor/store input adapter

Commit `53c31d95fd` contains the typed default builder. The new read-only
`oblivioncombatdata` adapter resolves native NPC/creature bases through stable
keys and the winning typed CSTY store. Null styles select verified native
defaults; invalid, deleted, wrong-type and unsupported-profile inputs diagnose
explicitly. Creature reach remains required. Native static and dynamic setting
overrides feed the pure builder without consulting the shared TES3 store.

All four new cases failed against the initial unimplemented adapter. The first
implemented run exposed a test-fixture variant initialization error; that failed
run is retained. The corrected fixture and added dynamic-override assertion
pass with all 551 engine tests and an exact inventory. Evidence is in
`S2/actor-inputs-01`; tested source/test diff SHA-256: `308b9553baf37e4a151ffc202ce25698d225386b7f088d3410647595eb08ed93`.
These are store/helper tests, not actor/controller gameplay acceptance. S2's
physical/crime rules and behavioral probes remain open.

### S2 native GMST parser validation

Commit `6e58f387dc` contains actor/store input resolution. Three new binary
GMST cases demonstrated baseline acceptance of malformed numeric payloads,
nonfinite values, missing/duplicate/unordered fields and stale reused-record
data. The TES4 parser now rejects these explicitly, requires native f/i/s
types and resets each record before loading; deleted tombstones may omit data.
Later-game value-type handling remains separately gated.

All 1,655 component and 551 engine tests pass; the component inventory matches
exactly. All three GMST cases and all 11 installed official plugin parses pass
under ASan/UBSan. Evidence and retained failures: `S2/gmst-01`. Tested
source/test diff SHA-256: `c43e17a57f430bbe9137e58c6a35c8cef9d21a8071ddd9ab81f64361ac9990ea`. These checks establish
parser acceptance and rejection, not damage or crime gameplay.

### S2 first physical arithmetic and native setting inputs

Commit `e978e10ae5` contains strict GMST loading. New pure helpers implement
luck-adjusted skill, fatigue scaling and the pre-mitigation weapon product.
The independently inspected original addresses, setting overrides, float-store
boundaries and numeric limitations are recorded in the rule provenance file.
A separate native builder supplies the 11 typed inputs using verified compiled
fallbacks only for absent settings. Invalid present overrides cannot fall back.

Four behavior cases first failed against unimplemented helpers; the setting
builder's fifth case separately failed before implementation. A test expectation
for a half-ULP fatigue result was corrected by hand derivation before running
the implementation. An initial CMake registration-script assertion is retained
separately and is not counted as a behavioral failure. All 1,660 component
tests pass with exact inventory; all 15 physical/settings/style-policy cases
pass under ASan/UBSan. Evidence: `S2/physical-rules-01`; tested diff SHA-256:
`2d678cbf62274bbfb51f5327f5808d8a16871aa44dbcf4d177893bf75ad4f5a9`. No world code consumes these physical helpers yet.
Block, armor, hand-to-hand, mastery, projectiles, crime and original damage
outcomes remain open S2 work; S3–S14 remain pending.

### S2 hand-to-hand, block and difficulty arithmetic

Commit `6493834a4b` contains the first physical helper chunk. The next helpers
produce separate hand health/fatigue damage, distinguish shield/weapon/unarmed
block fractions, and scale physical damage for an explicitly resolved player
role. Native typed builders provide their setting inputs. Original routines
and clamp/rounding order are documented in the provenance file. These helpers
do not establish block eligibility, mastery, or actual world damage dispatch.

Four new arithmetic/domain tests failed against stubs, and a fifth separately
exposed the absent setting builders. All 1,665 component tests pass with exact
inventory; all 20 physical/settings/style-policy tests pass under ASan/UBSan
(`S2/physical-rules-02`). Tested source/test/provenance diff SHA-256:
`cd3299f0db78a354dd85db8a8984589f4f01f9456a8bee5ebc19dcc2d7521b55`. Original live setting observations are being
extended independently; original damage outcomes remain an open gate.

### S2 individual armor rating and total cap

Commit `6a459e5c4e` contains hand/block/difficulty arithmetic. The new armor
helper follows the independently inspected caller's hundredths truncation,
skill scaling, floor/minimum and subsequent condition multiplication. A
separate total cap preserves zero-as-disabled. Typed native setting builders
and a 30-entry physical-input fact catalogue record compiled values separately
from installed overrides. Mastery aggregation and actual hit mitigation are
not established by these helpers.

Three new cases failed against stubs, then passed base/skill/condition/cap
boundaries, repaired and broken condition, override types, invalid domains
and overflow. All 1,668 component tests pass with exact inventory; all 23
physical/settings/style-policy cases pass under ASan/UBSan. Evidence:
`S2/physical-rules-03`; tested diff SHA-256:
`f6f952641656d74cfb1d72ea2416d8a1a676ba41ff8cce6c5f9723cb94465287`. S2 remains in progress.

### S2 complete live lookup coverage for the first 30 physical inputs

Commit `21318aabf3` contains armor arithmetic. Original run `S2/original-05`
used normal Continue from the preserved prison quicksave, then 27 additional
read-only `getgs` queries. All seven response captures were opened and inspected;
combined with the prior three physical queries, they cover every input in
`M15-PHYSICAL-RULE-INPUTS.json`. The source probe ledger now has 34 observations,
including the four earlier AI-style settings. Float responses display two
decimal places; binary precision still comes from the independent record and
initializer inspection. No actor/quest state or damage result was injected.

One query wrapper returned 143 after all of its child inputs and capture
returned zero. The game remained alive and later queries completed normally.
This unexplained orchestration exit is retained in the reference report, so
no whole-process acceptance gate is claimed. Each recorded numeric observation
has its inspected capture/hash and input provenance. The original game was
then exited normally. Actual physical damage probes remain the next oracle
work; live setting lookups alone do not close S2.

### S2 native NPC input validation and corrected health investigation

Commit `530b992705` contains complete live lookup coverage for the first 30
physical inputs. Native NPC DATA/ACBS/AIDT now reject incorrect lengths and
duplicates; reused records clear prior stats, styles, inventory and spells.
The independent census confirms all 2,664 physical official NPC records use
33/16/12-byte layouts. Existing uint32 health is deliberately retained: original
loader inspection disproved the editor schema's padding interpretation. The
rejected patch and its failing 100,000-health regression are preserved.

Malformed/reuse tests exposed the old reader behavior before the fix. A first
sanitizer run additionally caught GTest binding an unaligned packed field;
the test now copies its value before comparison. The diagnostic remains in
`S2/npc-inputs-01/sanitize.log`. All 1,671 component and 551 engine tests pass
with exact inventories. All 22 selected native parser/physical cases and all
11 official plugin parses pass under ASan/UBSan. A fresh actual two-process
S1 save/load/resave also passes (`npc-inputs-01/native-restart`); both captures
were opened and show the textured room and HUD. No combat claim is attached
to that observation-only course. Tested diff SHA-256:
`bd6aa81967c59deaa1c334db15c0ec698435da529db8856614714d13913bba29`.

### S2 reproducible isolated damage-reference fixture

Commit `54fc77aa57` contains native NPC validation. An editable JSON recipe
and bounded TES4 writer now produce an eight-record reference plugin: room,
one mortal fixed-stat opponent, explicit combat style, and an unenchanted
sword with a normal ground placement. Appearance/model inputs come from the
hashed licensed master; scripts, packages, spells, factions and weapon
enchantments are not inherited. This synthetic nonattacking opponent isolates
incoming-hit measurements; it is not an official campaign or an AI acceptance
fixture. No expected damage, scripted hit or death is written into the plugin.

Three new tests first failed against the unimplemented writer. All 141 Python
tests now pass. Native record parsing succeeds; comparing master-only and
master-plus-fixture graphs adds exactly eight keys and zero unresolved fixture
references, retaining exactly the same 3,665 known master-only graph findings.
Evidence: `S2/reference-fixture-01`; generated plugin SHA-256:
`f7aaac64d2b5c6353f97fdab17d21e78813283a4f241b9b318ef97128494f2a3`.
Tested source/recipe/test diff SHA-256: `b46ccb9285f0000e8247fff05d7c788649eb13b802244472e01bcfc81eea1650`.

Original runtime setup is in progress. `original-06` retained the Steam
application-load failure from an unchanged executable copy; `original-07`
uses the installed executable and isolated prefix, with only the generated
plugin temporarily added to Data. The tracked launch provenance requires its
hash-checked cleanup. Structural write/reinspection success is not yet runtime
acceptance, and no damage measurement has passed.

### S2 melee attack fatigue cost

Commit `ed8b08da7c` contains the first reference-fixture writer. The pure melee
fatigue helper and typed native settings builder now follow the independently
inspected original weight/power arithmetic. Three new tests first failed
against stubs. All 1,674 component cases pass with exact inventory; all 26
physical/settings/style cases pass under ASan/UBSan (`S2/physical-rules-04`).
Live read-only observations 35–37 confirm all three inputs, including installed
base cost 7 overriding compiled 8. Controller debit timing is still pending.

The original startup failures in `original-06/07` coincided with the normal
Linux Steam client no longer running; they do not establish an executable-copy
restriction. Restarting Steam allowed `original-08` to reach gameplay. The
first reference room appeared black and the player fell below its floor.
Diagnosis found a one-unit fog clip distance in the fixture's XCLL payload;
correcting and retesting the fixture is the next bounded task. The opponent
resolved with its authored 500 health. No hit or damage measurement has passed,
and the fixture's runtime gate remains open. The temporary installed plugin
still requires hash-checked cleanup after the reference run. S2 is in progress;
S3–S14 remain pending.

Tested implementation/test/provenance diff SHA-256: `83b0d27adc6cf4c66f6eb40b9ce83df820fb095c765220fd957cca40f55fcf77`.

### S2 original-runtime fixture format corrections

Commit `626a748410` contains melee fatigue arithmetic. Two new reference-writer
regressions exposed incorrectly ordered XCLL directional-fade/fog-clip values
and nonnative archive path separators. The writer now emits fade 1, fog clip 0
and the original backslash model path. All 143 Python tests pass; native parse
and graph reinspection pass with eight added keys and zero added unresolved
references (`S2/reference-fixture-03`). Generated plugin SHA-256:
`e3a91a255555297a4c538ffaaff96c2c360ff960783ac37fad1e43d1a70a76ea`.

Fog correction alone (`reference-fixture-02`, original-10) did not resolve the
black scene. Placing the existing master room in a disposable diagnostic scene
showed that the asset was available. A clean run with the path correction
(`original-11`) renders the authored room and stops the player's fall at its
floor. These captures were opened and inspected; `runtime-notes.json` retains
per-image observations and hashes. This is original model/room acceptance,
not damage acceptance. The initial opponent placement is on stairs above the
player, so flat-floor placement and normal weapon interaction are still being
verified before an attack case. The diagnostic original-10 run used noclip and
spawned room geometry; it was discarded without saving or attacking. Its exit
143 and the original-09 wrong-working-directory exit 5 remain retained.

S2 remains in progress. The corrected task-added plugin is still installed
for original reference testing and requires hash-checked removal afterward.

Tested writer/test diff SHA-256: `5413bfe22bb62f30094b86872f62838ca180edcfbda3ca0573f82ceac3738eaf`.

### S2 mastery thresholds and power damage selection

Commit `b161f9de16` contains the reference model/lighting fixes. New immutable
helpers resolve native mastery rank and select the directional power damage
multiplier, with typed setting builders. Independently inspected original
routines establish the ordered thresholds and separate directional unlocks;
the caller supplies integer skill directly, without luck adjustment. Actual
mastery effects and controller dispatch remain unimplemented.

Four new tests first failed against stubs. All 1,678 component tests pass with
exact inventory; all 30 physical/settings/style cases pass under ASan/UBSan
(`S2/physical-rules-05`). Tests use distinct directional setting overrides to
expose incorrect selection despite the installed values being mostly equal.
Live original observations 38–46 confirm all nine added setting inputs, with
three inspected captures. The fact catalogue now has 42 physical inputs and
the live setting ledger has 46 observations. S2 remains in progress.

Tested implementation/test/provenance diff SHA-256: `c3d275022fdd9f1aadc68787897721597aa70ee69ce5cfcc5f3445ab5777f814`.

### S2 first normal-input original hand-hit measurement

Commit `b8cf35f46c` contains mastery/power multiplier selection. The original
first-punch observation now has a source-controlled numeric/evidence ledger
and a component regression against the independently displayed 500.00 ->
498.58 health change. Its expected range was recorded before normal movement
and attack. All required captures for this bounded observation were opened:
clear first-person target, extended punch, recovery/hit reaction, exact target
health readback. Exact contact fatigue and the full hand rule matrix remain
open. The subsequent power attempt was interrupted or otherwise failed to
land while the active opponent attacked; unchanged target health fails that
attempt's prediction. Both attempts and the wrapper's exit 143 are retained.

The editable recipe now places opponent and sword off the stairs. Original-12
freshly renders the opponent on the flat floor from that recipe; normal sword
pickup is still pending. The writer comment no longer promises a passive
opponent from zero authored chance. All 143 Python tests and native parsing/
graph checks pass (`reference-fixture-04`, eight added keys, no added unresolved
references). Generated plugin SHA-256:
`e510e15cd5cf7cda97af21e4180621f40b26574cb34523df84cba67de703ee76`.
All 1,679 component cases and 31 focused ASan/UBSan cases pass with exact
inventories (`physical-rules-06`). The new observed-value regression already
agreed with the implementation; it is not described as a newly fixed failure.

Original-12 is running for a fresh power first-strike probe. Its read-only
console batch initially failed because original parsing concatenated LF-only
lines; the CRLF retry is retained separately. This diagnostic did not mutate
actor/quest state. The task-added plugin and root `M15Read.txt` require
hash-checked cleanup after reference work. S2 remains in progress; S3–S14
remain pending.

Tested test/writer/recipe/provenance diff plus new probe ledger SHA-256: `7b39be8f41e9ad326c8b82378c743ebdae64d44f0a95e908e60094de0013797f`.

### S2 original power first strike and fatigue ordering

Commit `8ecbe18277` contains the normal-punch observation and corrected floor
placements. A fresh first power punch now produces 500.00 -> 496.45 health,
inside its prediction recorded before attack. The source probe ledger retains
that successful bounded observation alongside the earlier failed engaged-target
attempt. The exact contact frame was not captured; the sampled images show
attack/recovery and numeric readback. Post-attack fatigue is 129.09.

Independent original vtable/call-path inspection establishes that melee damage
calculation precedes that attack's fatigue debit. The new observed-value test
checks 3.55 damage and distinguishes a premature debit. All 1,680 component
cases and 32 focused ASan/UBSan cases pass with exact inventory
(`physical-rules-07`). This is a rule/order contract, not native controller
acceptance. The new golden characterization already agrees with the pure
helpers; the native ordering still requires implementation in S4.

Original-12 remains active for a separate sword case. Normal F9 restored the
unchanged prison quicksave (SHA-256
`67d927cad4a370131882da58b04a25a330c5defe288b9b6ca21f7768815c4d66`)
between cases. The successful power case uses a frozen input-log prefix so
later setup does not change its evidence. Read-only CRLF player batches work;
unqualified/EDID target batches fail in the original console compiler and are
retained as failures. Direct selected-reference readback verified target
health/distance. The failed target batch was removed with a hash check; the
plugin and player batch still require cleanup. S2 remains in progress.

Tested test/provenance/probe-ledger diff SHA-256: `deeb0e5609c9f6591a5541ab20b6484c22c889cf9a35ea28f92f8a1356556b81`.

### S2 original first sword strike

Commit `f32caad6b4` contains the power-hit observation and fatigue ordering
contract. The next first-hit probe used normal sword pickup/equip and a fresh
opponent after F9 reload. Original health 500.00 -> 483.38 matches the
predeclared 16.625 damage prediction. All eight referenced captures were
opened; inventory damage 17 is presentation rounding. The independent golden
already agrees with the pure helper, so no synthetic red-test claim is made.
All **1,681 component tests** and **33 focused ASan/UBSan tests** pass with
exact inventories and no skips (`S2/physical-rules-08`). Commands: CMake
`components-tests` builds in `build` and `build/m15-sanitize`; full normal
suite and sanitizer filter `ESM4PhysicalCombat.*:ESM4CombatSettings.*:ESM4CombatStylePolicy.*`;
`check-physical-inventory.py` verifies both XML inventories.

The reference fixture now has observed original rendering, grounding, normal
pickup/equip and first-strike interactions. This is bounded runtime evidence,
not full native combat or whole-process acceptance. Original-12 remains
paused for read-only rule setting observations; task-added plugin and player
batch still require hash-checked cleanup. S2 remains in progress, S3–S14
pending. Next bounded task: independently trace durability/mitigation rules.

Tested test/provenance/probe-ledger diff SHA-256: `651d369177c77047db9da126e7aa12194a6e5a4bd3d3c309ca982636d84b3951`.

### S2 durability amount rules

Commit `61ff526b48` contains the original sword-hit characterization. Added
pure weapon/armor wear amounts and typed native setting factories from reviewed
original arithmetic/callers. Four new tests fail against the unimplemented
stubs, then pass, including a case sensitive to premature float rounding.
All **1,685 component cases** and **37 focused ASan/UBSan cases** pass with
exact inventories, no skips (`S2/physical-rules-09`). Build/test commands are
the same as physical-rules-08, with the new evidence directory. Both build
logs and test XMLs were inspected. Setting catalogue: **44** reviewed physical
inputs; live numeric ledger: **48** observations. Both new captures opened.

These are wear amount helpers, not condition mutation/runtime acceptance.
Armor selection, block/mastery adjustments and actual wear observations remain
open. The reference process is paused, with task-added plugin/player batch
still pending hash-checked cleanup. S2 remains in progress; S3–S14 pending.
Next bounded task: armor mitigation and condition application contracts.

Tested implementation/test/provenance diff SHA-256: `3a896a12534be7a1e995ffe5685a590e66e3cab042560d68acd67fa6ddeb20e6`.

### S2 armor mitigation and condition arithmetic

Commit `5bb0643eb7` contains wear amount rules. Added independent original
armor mitigation, Novice/Journeyman armor wear selection, and condition-after-
wear rules, including the below-one break boundary. Five tests failed against
stubs; all **1,690 component tests** and **42 focused ASan/UBSan tests** now pass
with exact inventories/no skips (`S2/physical-rules-10`). Same build/test and
inventory commands as physical-rules-09. Both build logs/test results inspected.
Four new original setting captures opened; the catalog now has **48** physical
inputs and numeric lookup ledger **52** observations. A post-hit inventory
capture shows sword condition 99; retained as exploratory diagnostic because
of integer display precision and no pre-hit wear prediction.

The native world still needs aggregation, bypass eligibility, equipped-item
mutation, breakage/events and persistence integration. S2 remains in progress;
S3–S14 pending. Original-12 is paused in inventory, task-added files still
pending cleanup. Next bounded task: armor wear selection and mastery matrix.

Tested implementation/test/provenance diff SHA-256: `87690083785aadb5a9e6ffd5d8f04ec71bb47235bbf7633f1d2645c38be9d3fe`.

### S2 armor wear slot selection

Commit `01cc2731ef` contains mitigation/mastery/condition rules. Added native
single-attempt armor wear selection with verified missing-slot fallthrough,
head/hair preference and shield complement behavior. Five new cases failed
against stubs; all **1,695 component tests** and **47 focused ASan/UBSan tests**
pass with exact inventories/no skips (`S2/physical-rules-11`, same commands as
physical-rules-10). Distribution coverage includes exhaustive draw boundaries
and a fixed-seed 100,000-draw check with predeclared tolerance. All six original
numeric captures opened. Catalog: **54** reviewed physical setting facts
(including explicitly unused shield chance); live ledger: **58** observations.

Equipment candidate resolution, up-to-seven RNG attempts, mutation and restore
remain native integration tasks. S2 remains in progress, S3–S14 pending.
Original-12 is paused in console; installed task files still pending cleanup.
Next bounded task: creature damage and remaining mastery/reaction rules.

Tested implementation/test/provenance diff SHA-256: `35920834e269d58900f933b451fb2a8f9c3aeecaa9927a91171140398685c493`.

### S2 creature natural damage arithmetic

Commit `defe2290e3` contains armor wear selection. Added creature natural
attack damage from independently identified Creature vtable/caller: fatigue
product, float store, signed integer truncation. Two new tests fail against
stubs; all **1,697 component tests** and **49 focused ASan/UBSan tests** pass
with exact inventories/no skips (`S2/physical-rules-12`, same commands as
physical-rules-11). Both build logs and test results inspected. Creature actor
stats, controller contact and runtime behavior still require implementation.

Original-12 exited through normal `qqq` with wrapper exit **0**, unlike the
retained earlier exit-143 runs. Both task-added installed files (reference
plugin and read-only player batch) were removed only after matching expected
SHA-256 hashes; `original-12/exit-cleanup.json` records cleanup. Frozen probe
input prefixes remain unchanged. No whole-M15 acceptance is inferred from
this clean exit. S2 remains in progress; S3–S14 pending. Next bounded task:
bow/projectile damage inputs and remaining mastery/reaction rules.

Tested implementation/test/provenance diff SHA-256: `18aef648bda8fc1f21f5a93052e59dc072605d4ac8d7d88bfae4a2230c100ac1`.

### S2 bow launch rule arithmetic and engine regression

Commit `2f6d6e1ce6` contains creature natural damage. Added a separate pure
projectile rule module for draw fraction, separately scaled bow/ammo damage,
launch speed and gravity coefficient, plus typed native settings. Six tests
failed against stubs. An initially overstrict gravity boundary assertion
failed in both builds; exact-rational review proved float rounding produces
a plateau. Failed runs retained, expectation corrected, production unchanged.
All **1,703 component cases** and **55 focused ASan/UBSan cases** pass with
exact inventories/no skips (`S2/projectile-rules-01`). The sanitizer filter
now also includes `ESM4ProjectileRules.*`; the inventory checker was updated
accordingly. Both build logs/test XMLs inspected.

The native engine, `openmw-tests`, and `esmtool` rebuild successfully; all
**551 engine tests** pass with exact inventory (`S2/integration-03`). Original-13
normal prison load verified seven formatted setting lookups; the combined
capture was opened. GravityMult's 0.00 display does not independently verify
.002, and that limitation is explicit. Original-13 exited normally with code
0; no installed game files were added. Only its isolated load-order file was
changed to master-only; the previous isolated file is retained as evidence.
Physical setting facts: **61**; numeric lookup observations: **65**.

Bow controller/timer accumulation, trajectory/collision/recovery, launch state
persistence and first-hit original/native projectile gameplay remain open.
S2 remains in progress; S3–S14 pending. Next bounded task: extend the editable
reference fixture with bow/ammo for normal-input arrow observations, and
finish remaining reaction/mastery/crime rule families.

Tested tracked implementation/test/provenance diff plus three new source files
(path followed by contents) SHA-256: `131272179df49067dbfaff5fd70f7590e68eb62c0c6706e07d8b5e51be8b2c57`.

### S2 editable bow/ammo reference fixture

Commit `b8bd1ef434` contains bow launch arithmetic. Extended the isolated
reference writer with strict version-2 weapon type and ammunition recipe fields.
The new editable bow recipe clones only reviewed model/icon payloads from
Iron Bow `025231` and Iron Arrow `017829`, creates a nonenchanted bow and
20-arrow pickup stack, and retains the proven room/target geometry. The
version-1 sword fixture remains byte-for-byte reproducible; a prior-output
hash regression verifies this. New version-2 positive test fails before writer
support, then passes. All **146 Python tests** pass (`reference-fixture-05`).

Generated plugin SHA-256:
`c7b0663bfd6ec0c5b530f8e9ebd840bd4db2908426610bdcc76cfe0cbdd73776`.
Native `esmtool --quiet dump` parses it; native graph adds **10 keys**, has no
new unresolved references, preserves all 3,665 known master findings, and is
restart/reorder stable. These are structural/semantic checks; original/runtime
acceptance is still false. S2 remains in progress; S3–S14 pending. Next task:
normal original bow/ammo pickup/equip and independently predicted first shot.

Tested writer/test diff plus new recipe (path followed by contents) SHA-256:
`7ec566567ca6d62a23a9fae25f99ba330f6b892f4f5aab60d6ec9cd4fe29db1e`.

### S2 bow fatigue and first-arrow oracle

Commit `d25003ae14` contains the editable bow/ammo fixture. Original-14
normal pickup/equip and first held shot exposed a failed prediction: initial
full fatigue did not persist through draw (500 -> 486.26 instead of 485.15).
That failure is retained. Tracing established player-only novice hold debit
15/second and novice shot debit 5 after launch damage capture. Added pure
rules and strict native settings; three tests fail against stubs, then pass.
A fourth regression characterizes the original repeat and already agrees
with launch arithmetic; no fabricated baseline failure is claimed for it.

Fresh normal quickload/pickup/equip, paused read-only draw fatigue 119.58/140,
and a new prediction before release give expected health 486.234. Observed
486.26 is within predeclared .05 display/transition tolerance. One arrow is
consumed and a lodged arrow/hit reaction is visible. This is bounded original
first-shot evidence, not exact frame timing or native runtime acceptance.
Both observations/capture hashes/input logs are in the damage ledger.

All **1,707 component tests** and **59 focused ASan/UBSan tests** pass with
exact inventories and no skips (`S2/projectile-rules-02`). New setting
facts bring the catalog to **66**, original setting lookups to **70**.
Original-14 received normal `qqq`, but the Proton wrapper returned **143**;
this unexplained whole-process exit remains a failed gate. The game process
is absent. Both task-installed files were removed after exact hash checks.
The unchanged prison quicksave and normal user configuration are preserved.

S2 remains in progress; S3–S14 pending. Native controller/fatigue regeneration,
release timing, trajectory/recovery and broader bow matrix remain open.
Next bounded work: reaction and combat mastery rules, followed by the remaining
S2 stealth/crime rule families and actor-policy resolution.

Tested implementation/test/provenance diff SHA-256: `ecadb7b247d68726019e44a7f2cc0ad42606078ac05d7b6d82aa806c4dcb0436`.

### S2 damage knockdown rule

Commit `93d84f3ff0` contains bow fatigue and the retained first-arrow oracle.
Added pure original damage-knockdown selection, explicit inclusive percentile
comparison, float storage/cap order, signed compiled factors, zero-denominator
outcomes, typed setting defaults and overrides. Caller trace establishes
truncated pre-armor contact damage and victim Agility/Luck/fatigue inputs.
World eligibility, mastery/unconsciousness, RNG persistence and reaction
execution remain separate and open.

Four new tests fail against stubs, then pass; all **1,711 component tests**
and **63 focused ASan/UBSan tests** pass with exact inventories and no skips
(`S2/physical-rules-13`). Boundary cases include all 100 draws, next float
below the cap, negative factors, 0/0, typed malformed settings and fixed-seed
100,000-draw sampling with a predeclared tolerance.

Original-15 normal Continue/master-only prison run confirms GMST-71–75,
including installed .3 damage multiplier and chance. All values are visible
in the inspected capture; this is setting lookup, not knockdown gameplay
acceptance. Catalog **71** facts; original setting observations **75**.
Original-15 remains running for the next bounded reaction-setting probes;
no installed game files were added. S2 in progress; S3–S14 pending.
Next task: remaining reaction/mastery arithmetic and semantic matrices.

Tested implementation/test/provenance diff SHA-256: `50dceae01c6bc9aeeaa8e6d2d6a82fabda04fa81db7b5afae7270862965551c4`.

### S2 knockback and actor-value input normalization

Commit `41d6387ab3` contains damage-knockdown selection. Added signed
knockback-force arithmetic, upper cap and typed force/time settings, plus
base-value floor and fatigue-ratio helpers established from original caller
traces. Mastery uses base skill, not luck/effect-adjusted damage skill. Zero
base fatigue returns ratio 1; other signed/current values remain unclamped.
A singular knockback fatigue divisor diagnoses rather than sending nonfinite
force to physics; corresponding world-state eligibility remains open.

Five new tests fail with stubs, then pass. All **1,716 component tests** and
**68 focused ASan/UBSan tests** pass without skips and match exact inventories
(`S2/physical-rules-14`). Original-15 GMST-76–81 are captured and inspected;
the console rounds -.008 to -.01, so that lookup explicitly cannot verify
exact precision. Catalog **77** facts; original setting observations **81**.

S2 remains in progress; S3–S14 pending. Original-15 remains running with
master-only content for additional read-only queries; no installed files
were added. Next bounded task: enumerate and implement the remaining mastery
abilities, including probabilities, eligibility and narrow paralysis state.

Tested implementation/test/provenance diff SHA-256: `36a01ed74e63278b630b4fc58b48bc36501447d703b99caf03cb4bf385e4c17e`.

### S2 mastery ability inventory

Commit `565c99787c` contains knockback and actor-value normalization.
Added `M15-MASTERY-ABILITY-MATRIX.json` with **40 separately identified rows**
from independently decoded original SKIL rank descriptions. Covers Blade,
Blunt, Hand to Hand, Block, Marksman, Sneak, light/heavy armor, four existing
M13 Armorer regression connections, and two narrow Acrobatics attack/dodge
prerequisites. Each row names threshold, input, equipment/posture, consequence,
probability/duration status, save requirements, implementation owner and cases.
Original text remains only under `S2/sources-01`; tracked rows summarize it.

All rows remain **pending** until their real execution and restart tests pass.
Existing pure arithmetic is identified without claiming whole abilities pass.
Unknown execution semantics remain explicit. JSON, unique IDs, all 40 rows,
required fields and thresholds validated. Linked from the main case inventory.
S2 remains in progress; S3–S14 pending. Next: trace and implement mastery proc
and defense policies against these rows; preserve original prerequisite and
shared-draw/precedence behavior rather than creating independent random rolls.

### S2 mastery proc decisions

Commit `4ce5797b1c` enumerates mastery requirements. Added a dedicated pure
mastery module for shared-draw attack knockdown/paralysis, side-power disarm,
and defensive stagger/disarm. Preserves native strict versus inclusive rolls,
paralysis precedence, base-skill thresholds, equipment/quest-item/drawn guards,
and whether each branch consumes RNG. No boolean proc is counted as real
dropped equipment, paralysis, controller interruption or save acceptance.

Seven new tests fail against stubs, then pass; expanded deterministic
permutations, chance endpoints, custom thresholds, mapping/type validation,
and fixed-seed distribution checks pass. All **1,723 component tests** and
**75 focused ASan/UBSan tests** pass with exact inventories and no skips
(`S2/mastery-rules-01`). Original-15 GMST-82–87 captured/inspected: installed
defensive stagger chance is 25, compiled default 5. Catalog **83** facts;
original setting observations **87**.

Original code adds an Expert unarmed defensive-stagger query branch absent
from the skill message; the ability inventory now has **41 pending rows**.
Paralysis tracing identifies original fallback SpellItem 000137, PARA with
10-second duration; all-content override and actual effect semantics remain
open. Source traces and absent-master-record scan are retained under
`S2/sources-01`. S2 remains in progress; S3–S14 pending. Original-15 remains
running for read-only recovery/block probes; no installed files added.
Next: complete defensive/recovery rules and their remaining caller semantics,
then stealth/crime rule families and S2 actor-policy/audit gate.

Tested staged implementation/test/provenance diff SHA-256: `fe3bd22b132eb2884e27cdc61d7505dfcc414edf36f4d006349e6143702192e1`.

### S2 block contact costs and integration build

Commit `3f2498c29b` contains mastery proc decisions. Added block fatigue and
blocking-item wear amounts with original base-skill mastery gating, separate
current-skill arithmetic, and absorbed-fraction input (incoming damage is not
used by the fatigue formula). Preserves signed factors and validates ranges.
Three new tests fail against stubs, then pass; all **1,726 component tests**
and **78 focused ASan/UBSan tests** pass with exact inventories/no skips
(`S2/physical-rules-15`). Original-15 lookup GMST-88–93 adds recovery setting
provenance and confirms block costs. Catalog **89** facts, lookup count **93**.
Actual contact/state/inventory mutation remains open; ability rows stay pending.

Integration build at `3f2498c29b` rebuilt `openmw`, `openmw-tests`, `esmtool`;
its first wrapper returned **143** despite all three targets reaching Built.
That failed exit is retained. A separate incremental verification exits **0**;
all **551 engine tests** pass, with exact inventory/no skips (`integration-04`).
These checks do not count as native combat gameplay acceptance.

Additional caller trace requires defensive stagger before disarm: successful
stagger suppresses the disarm roll, and ranged contacts skip stagger. This
ordered RNG requirement is documented for the native contact controller.
S2 remains in progress; S3–S14 pending. Original-15 is still paused for
read-only queries; no installed files added. Next: essential recovery and
remaining armor/stealth/crime rules, then actor policy/audit closure.

Tested staged implementation/test/provenance diff SHA-256: `18a0953668bca8983d5985b2425b914509c053b7877a8804ef4ca910ec144af0`.


### S2 essential recovery health arithmetic

Commit `0d693ffa6c` contains block contact costs. Added typed essential
recovery delay/fraction settings and the health target/adjustment calculation
shared by entering unconsciousness and recovery. Preserves base-to-float and
product/subtraction storage, allows signed current health and fractions above
one, and diagnoses invalid settings/base health/nonfinite results.

Two new tests fail against a stub, then pass. All **1,728 component tests**
and **80 focused ASan/UBSan tests** pass with exact inventories and no skips
(`S2/physical-rules-16`). Original GMST-88–89 provide setting confirmation;
original code independently establishes the arithmetic and zero timer boundary.
No original essential-knockout gameplay or native lifecycle acceptance is
claimed. Controller states, timer progression, animation and persistence remain
open. S2 remains in progress; S3–S14 pending. Original-15 remains paused for
read-only queries with no task-installed files. Next: armor mastery aggregation
and remaining stealth/crime rule families, then actor policy/audit closure.

Tested implementation/test/provenance diff SHA-256: `1d3549e8bda66450924b4e8a707fb2f85113fba6db750747127c917778915298`.


### S2 armor mastery arithmetic

Commit `0186fb26a6` contains essential recovery health arithmetic. Added
weighted armor coverage, Light Armor Master rating bonus before additive
Defense/cap, and worn-instance armor weight reductions using base mastery.
Coverage uses seven slot weights and clamps at 100; it is not a piece count.
The real inventory caller still must supply correct active slots, deduplicate
item-rating contributions, invalidate caches and apply one-instance weight.

Four new tests fail against stubs, then pass. All **1,732 component tests**
and **84 focused ASan/UBSan tests** pass with exact inventories/no skips
(`S2/physical-rules-17`). Original-15 GMST-94–98 were opened/inspected and
confirm five new defaults. Catalog **94** facts, setting observations **98**.
Affected mastery rows retain pending native execution/restart gates.
S2 remains in progress; S3–S14 pending. Original-15 remains running paused
for read-only queries; no task-installed files. Next: sneak eligibility and
multipliers, followed by remaining stealth/crime rules and actor-policy audit.

Tested implementation/test/provenance diff SHA-256: `4ca51083b1027d277aa6414057250f6c8676641a734d398cc0db8f3b3526ba63`.


### S2 sneak attack contact rules

Commit `e52ccc78f6` contains armor mastery arithmetic. Added a pure stealth
module with native weapon eligibility, base Sneak mastery multipliers,
victim-specific signed awareness/combat-target thresholds, sneaking/swimming
posture and Master armor/block bypass. Resolves all eleven typed settings;
installed bow multipliers differ from compiled defaults. Original-15
GMST-99–109 captures are inspected; catalog **105**, observations **109**.

Four new tests fail against stubs, then pass; all **1,736 component tests**
and **88 focused ASan/UBSan tests** pass without skips and match inventories
(`S2/stealth-rules-01`). Source traces also establish missing detection data
returns INT_MAX, preventing an unobserved pair from receiving a free bonus.

**Required integration correction:** current M14 detection produces [0,100]
normalized scores, while original contact rules require signed awareness and
an existing-combat-target threshold of -50. Native detection must be verified
and corrected in its existing authority, with regression/migration coverage.
No invented offset or second detector is accepted. Mastery rows remain pending
normal gameplay/restart. S2 in progress; S3–S14 pending. Original-15 paused,
master-only, no installed files. Next: complete native perception rules and
remaining stealth/crime data/rules, then S2 actor-policy/audit closure.

Tested staged implementation/test/provenance diff SHA-256: `742eda6220bfa28d0af35f4085edc31f72590428bff2220ef69f63352e177f49`.


### S2 signed awareness arithmetic and noise mastery

Commit `29543031f2` contains sneak contact rules. Added the reviewed native
signed-awareness scalar calculation and Sneak footwear/movement mastery
adjustment in the existing detection component. Added 15 typed native factors;
Original-15 GMST-110–124 captures are inspected. Catalog **120**, observations
**124**. Legacy normalized detection remains an explicitly incomplete runtime
adapter until inputs, detection history and save version migrate together.

Four new tests fail against stubs, then pass. All **1,740 component tests**
and **96 focused ASan/UBSan tests** pass with exact inventories and no skips
(`S2/detection-rules-01`), preserving the four existing M14 characterization
tests. Independently emulated original instructions supply 20 named expected
cases plus **1,024** differential cases (512 complete boolean combinations,
512 numeric vectors, seed 5051870), zero mismatches under both 53/64-bit x87
controls (`S2/oracle-emulator`). This supplementary instruction-level oracle
does not replace required original-game behavior or OpenMW gameplay.

Engine integration at `29543031f2` rebuilt `openmw`, `openmw-tests`, `esmtool`
with exit 0; all **551 engine tests** pass with exact inventory/no skips
(`S2/integration-05`). Detection helper changes are newer than that integration
build. S2 remains in progress; S3–S14 pending. Original-15 remains paused,
master-only, no task-installed files. Next: pickpocket/crime rule semantics,
remaining reaction/data requirements and actor-policy audit; native detector
integration/migration remains a mandatory open item.

Tested implementation/test/provenance diff SHA-256: `d41ec27b118fb615e6d4844cabaed69f7016b61c9ee5940a0d744c4ccc19431f`.


### S2 pickpocket chance rules

Commit `1e15e8ad38` contains signed awareness/noise arithmetic. Added typed
pickpocket factors, checked item-value/count amount, native stored-term and
integer clamp arithmetic, and distinct transfer versus untouched-menu-exit
roll comparisons. Actual session, prohibited-item/transfer and crime effects
remain open. Original-15 GMST-125–132 are captured/inspected; catalog **128**,
observations **132**. Native item-value provenance and session-flag ordering
are retained in source traces; no item-weight penalty is invented.

Four new tests fail against stubs, then pass. All **1,744 component tests** and
**100 focused ASan/UBSan tests** pass, exact inventories/no skips
(`S2/pickpocket-rules-01`). Twelve original-instruction examples and **1,024**
differential cases pass under both x87 precision controls, with zero discarded
mismatches (`S2/oracle-emulator/pickpocket-*`). Probabilistic boundary and
fixed-seed distribution checks pass. This does not constitute normal gameplay
or crime acceptance. S2 remains in progress; S3–S14 pending. Original-15
remains paused master-only; no task-installed files. Next: crime fines,
reporting/responsibility and jail arithmetic, then remaining reaction/data
rules and actor-policy audit closure.

Tested implementation/test/provenance diff SHA-256: `c1a6508be97d9f2891bae32665e984ae7eda83ec0e15634e8d202155c788548a`.


### S2 crime fine and jail sentence arithmetic

Commit `c6cf7942ba` contains pickpocket arithmetic. Added per-incident fines,
maximum applicable faction crime multiplier (minimum one), and uncapped
elapsed sentence days/hours with a separate ten-attempt skill-change limit.
Typed installed settings preserve fractional theft fines and diagnose invalid
amounts/divisors/overflow. Crime legality, witnesses, committed bounty, actual
jail service and skill selection remain open. Original-15 GMST-133–142
captures are inspected; catalog **138**, observations **142**.

Three new behavior tests fail against stubs; the settings test already passes.
All **1,748 component tests** and **104 focused ASan/UBSan tests** pass with
exact inventories and no skips (`S2/crime-rules-01`). Original-instruction
emulation independently confirms ten base-fine and six faction-factor cases.
This does not constitute original gameplay or native world acceptance.

Original-15 received normal console qqq but the wrapper returned **143**;
whole-process exit acceptance failed. The process is absent, no task files
were installed, and the pristine quicksave hash is unchanged. All earlier
read-only setting observations retain their bounded scope. S2 remains in
progress; S3–S14 pending. Next: remaining jail/witness rules, faction data and
selected actor policy audit, followed by native service integration.

Tested implementation/test/provenance diff SHA-256: `12c519e0ab92ceeef2ae4e44e9b1da81fa848f4ad97e9da9858dfcdb12dd66f7`.


### S2 typed faction crime and relationship data

Commit `6f38e4e07a` contains crime fine/sentence arithmetic. Added a TES4-only
faction decoder, winning store, stable relationship FormKeys and semantic
`esmtool` output. All **495 winning factions** across eleven hash-checked
official plugins agree with the independent raw census, including **129
missing CNAM fields**. Original constructor/loader traces establish 1.0 as
the missing-field runtime default; the decoder preserves explicit absence.
Raw rank/unknown payloads remain available; later-game records stay raw.

Three parser tests fail against the stub, then pass. All **1,751 component
tests**, **107 focused ASan/UBSan tests**, and **552 engine tests** pass with
exact inventories/no skips (`S2/faction-data-01`). The engine test covers
reordered masters, distinct same-ID factions, winning overrides, relationship
resolution and deletion. `openmw`, `openmw-tests`, and `esmtool` build. The
first engine build wrapper returned 143 despite completed targets; the retained
retry returns 0. Existing compiler warnings are retained in build logs.

S2 remains in progress; no new runtime crime or jail acceptance is claimed.
Original-16 is conducting a master-only normal-input crime/jail reference
probe. Native rule coverage, selected actor policy explanations and subsequent
service/persistence/gameplay stages remain mandatory.

Tested implementation/test/provenance diff SHA-256: `a6d7c8ab4719b6e5fdca5e8e697f9d937333aea162519f94b4fadc78b7a9c434`.


### S2 jail skill draw and penalty rules

Commit `28ca74cc90` contains typed faction data. Added a one-draw jail skill
selection step that preserves pending zero draws and native 12–20 actor-value
selection, plus the modified-skill eligibility check and base-byte decrement.
Three new behavior tests fail against stubs, then pass. All **1,754 component
tests** and **110 focused ASan/UBSan tests** pass with exact inventories and no
skips (`S2/jail-selection-01`). Nine original-instruction selection cases and
nine penalty cases independently confirm expected boundaries and draw usage.
A 100,000-sample fixed-seed selection test matches an independent exact
probability calculation with predeclared 600-count per-bin tolerance.

Original-16 has observed normal assault contact (guard health 127.00→125.71),
bounty 0→40 and normal arrest/jail entry, matching the pre-action prediction.
The initial punch without confirmed contact and unsuccessful bed activation
attempts are retained. Sentence service, release and skill changes are still
being probed; no full original jail acceptance or native runtime acceptance
is claimed. RNG/state persistence and skill mutation notifications remain
controller work. S2 in progress; S3–S14 pending.

Tested implementation/test/provenance diff SHA-256: `0a04d9073d0a9a89c9d3fbcf87cb67188bffd68460faaf5a25b24e181d1cba9a`.


### S2 responsibility/disposition alarm arithmetic

Commit `b20539db9b` contains jail selection and penalty arithmetic. Traced
the previously unidentified alarm predicate argument to disposition toward
the offender. Added the strict Responsibility × typed multiplier comparison,
without a random draw or premature float rounding. Witness eligibility and
report/bounty commitment remain separate, unimplemented controller work.

The behavior test fails against a stub; the typed-settings test already passes.
All **1,756 component tests** and **112 focused ASan/UBSan tests** pass with
exact inventories/no skips (`S2/alarm-rule-01`). Fifteen supplied-input
original-instruction cases independently confirm signed inputs and thresholds.
Installed/compiled multiplier provenance was already captured as GMST-141.

Original-16's first normal jail course has now observed release, bounty
40→0 and exactly one base skill change: Heavy Armor 15→14; all other 20 skills
are unchanged. Clock passage 24.0003 hours exceeds its predeclared ±0.0002
hour budget, so that timing assertion **fails and is retained**. A fresh
quicksave trial has been predeclared with a UI-transition budget before its
actions. The actually activated Chorrol jail bed is `067c47`, distinct from
the earlier unverified `02b05a` candidate; inventory correction follows the
probe. Neither original observations nor pure tests close native gameplay.
S2 in progress; S3–S14 pending.

Tested implementation/test/provenance diff SHA-256: `abdebe58e857a8c9db41eae5402d7d230f7136e4e422b94683d5ee7cad0ab2e8`.

### S2 verified Chorrol sentence bed

Commit `a0539ec3d3` contains the responsibility/disposition alarm rule.
Corrected the case inventory's Chorrol bed to `067c47`, which Original-16
identified by read-only console selection and activated normally to serve
its first sentence. Retained `02b05a` as the previous unverified candidate.
The fresh all-official-content audit (`S2/audit-08`) resolves `067c47` to
Bedroll `01d5bf`, player owner `000007`, and prison cell `02898e`.
All reviewed inventory counts and content hashes remain unchanged; zero data
failures and all 13 audit unit tests pass. The audit deliberately exits 1
because rule/oracle stage gates remain open. This is a verified reference
correction, not native OpenMW jail acceptance. The first timing failure and
all unsuccessful normal-input attempts remain preserved in Original-16.


### S2 native melee reach and second original jail course

Commit `0dc2f417bb` corrects the verified Chorrol bed reference. Added native
weapon, unarmed and creature reach rules, including separate creature distance
semantics, giant multiplier, intermediate rounding and resolved actor scale.
The new reach behavior test fails against the stub; the settings test already
passes. All **1,758 component tests** and **114 focused ASan/UBSan tests** pass
with exact inventories and no skips (`S2/reach-rule-01`). Fourteen independent
original-instruction cases agree; the initial faulty emulator harness remains
preserved. Installed GMST probes 143–145 confirm distance/hand/giant inputs.
Contact geometry and controller timing remain subsequent gameplay work.

Original-16 case 02 completes normal assault/arrest/bed service with bounty
0→40→0, Hand to Hand 10→9 and 24.0002 hours elapsed within its predeclared
±.01-hour tolerance. Case 01's stricter timing failure remains failed. Normal
`qqq` removes the game process and the pristine quicksave is unchanged, but
wrapper exit 143 is explicitly a failed whole-process exit gate. Confiscated
inventory counts and native OpenMW runtime jail acceptance remain unverified.
S2 remains in progress; selected actor policy explanations and remaining rule
families are open. S3–S14 remain pending.

Tested implementation/test/provenance diff SHA-256: `3aec01ef210eaba45c1864ddf0ca0053229c812645ac5e06dba0b457b0fd860b`.


### S2 actor equipment and spell dependency audit

Commit `864c1cc731` contains native melee reach rules. The semantic audit now
retains ordered CNTO item/count entries, SPLO spell/leveled-spell references,
and NPC race/class keys, checking all links after winning overrides/deletions.
Malformed actors are removed from the decoded map after recording failure,
so partial decoding cannot crash subsequent semantic checks. Two new tests
fail against the old audit, then pass; all **148 Python tests** pass.

`S2/audit-11` checks all **3636 actors**, **20421 inventory entries**,
and **5505 spell entries**, with zero data failures and unchanged locked
counts/hashes. `S2/audit-09` retains the initial 1,690 diagnostics caused by
incorrectly rejecting native LVSP references. The TES4 record definitions
confirm SPLO accepts SPEL/LVSP and CNTO includes SGST; tests cover both.
The intermediate malformed-input KeyErrors are likewise retained and fixed.

Selected tutorial assassins and the first Arena opponent have authored spells;
these dependencies remain explicit, and neither spelling out an inventory nor
resolving its FormKeys executes magic or closes a physical combat policy gate.
S2 remains in progress; the all-content audit still exits 1 for its open
rule/oracle gates. S3–S14 remain pending.

Tested implementation/test diff SHA-256: `37007c78e8cb20d24bd75a5c60a3d4f94605b365e377e7b6e3da05c9662a8485`.


### S2 contact-facing cone rule

Commit `2c410847ea` extends the actor dependency audit. Added native contact
cone arithmetic with the original conversion constant, intermediate rounding,
one wrap, strict boundary, typed setting input and explicit invalid-input
failures. The behavior test fails against the stub and then passes. All
**1,760 component tests** and **116 focused ASan/UBSan tests** pass with exact
inventories/no skips (`S2/hit-cone-01`). Fourteen supplied-angle original
instruction cases independently confirm expected results.

The installed cone angle is 35 degrees versus compiled 20; its live console
setting probe remains pending in the catalog. The original block branch
requires native process action 6 before its cone test. Animation timing,
contact geometry and complete block posture eligibility remain integration
work. No native runtime blocking acceptance is claimed. S2 remains in progress;
S3–S14 remain pending, with selected actor policy explanations and remaining
physical/crime rule families still open.

Tested implementation/test/provenance diff SHA-256: `9e172cbbc71de1c78ee5afae65163fc132ea19473a11db170183506bcef6ecec`.


### S2 native ownership claim predicate

Commit `eb204855f1` contains contact-facing cone arithmetic. Added the native
ownership claim predicate for already resolved actor identity, permission global
and faction ranks. It preserves nonzero negative/fractional globals, inclusive
signed rank comparison and the caller's faction-ownership mode. It does not
classify unowned property as criminal or merge evil-owner/public-cell/witness
policy into the ownership claim.

Two new tests fail against the stub, then pass. All **1,762 component tests**
and **118 focused ASan/UBSan tests** pass with exact inventories/no skips
(`S2/ownership-claim-01`). Eighteen original-helper instruction executions
independently confirm expected branches using boundary stubs only for record,
identity and rank queries. World owner inheritance and crime legality remain
open. Original-17 is conducting the pending read-only cone setting probe.
S2 remains in progress; S3–S14 remain pending.

Tested implementation/test/provenance diff SHA-256: `463bf494909c0b40c3620bc885439b3971ace02250740e985a9e4fe4d6487faa`.


### S2 original cone setting confirmation

Commit `e4251f2c10` contains the ownership claim predicate. Original-17
read-only probe GMST-146 confirms the installed cone angle **35.00**, matching
the declared prediction and master override. Only Oblivion.esm was active;
normal Continue loaded the pristine prison. Capture and query inputs exited
zero and the screenshot was inspected. Normal `qqq` ended the game with
wrapper exit **0** and unchanged pristine quicksave. The trailing Return-key
release failed because the isolated X display had already closed; its failure
is retained in the input log and cleanup record. This closes that setting
lookup only. S2 remains in progress, and native gameplay gates remain open.


### S2 resolved style-policy semantic audit

Commit `498c950b2f` records the original cone setting confirmation. Extended
the audit to resolve native default and authored style inputs using the reviewed
initializer catalog and winning typed settings. Raw records remain unchanged;
historical tail defaults, selected zero sentinels and Advanced-flag behavior
are explicit. Missing styles/advanced data and malformed defaults fail.

Two new tests fail against the absent resolver, then pass; an additional
review test covers signed advanced values and ambiguous settings. All **151
Python tests** pass. Fresh all-official-content audit `S2/audit-12` has zero
data failures and unchanged count lock. It resolves **130 policies** and all
**3,636 actor links**, including **2,218 native-default consumers**. Independent
C++ production decode/build/resolve calls agree on **7,540 fields** across all
130 policies (`S2/policy-audit-01/comparison.json`), with matching source payload
hashes and native runner exit 0.

The audit still returns overall false/exit 1: resolved inputs do not execute
combat AI, effects or gameplay. Full selected campaign policy explanations,
remaining rules and original/gameplay gates remain open. S2 remains in progress;
S3–S14 remain pending.

Tested implementation/test/provenance diff SHA-256: `9ed3f0794ad0769d15b04b869224a13ad16e915550eb7553b2d5bce24ce07da3`.

### S2 selected campaign policy explanations

Commit `a482f878bf` resolves style inputs in the semantic audit. Added
[M15-CAMPAIGN-COMBAT-POLICIES.md](M15-CAMPAIGN-COMBAT-POLICIES.md) covering the
selected fixed actors, all twenty leveled opponent alternatives, Rindir,
Owyn and the four jailors. It distinguishes native default and missile style
inputs, equipment selection, race inheritance and unresolved magic behavior.
Independent raw winning-record inspection identifies tutorial bound equipment,
potions and the Arena ability's Speed/Athletics and resistance effects.

Evidence: `S2/policy-audit-01/campaign-records.json` and its reproducible local
`inspect_campaign.py`, using the previously locked eleven plugin hashes.
Reviewed the matching typed equipment/actor audit fields and spell/effect
payloads. This documentation-only chunk does not change production behavior;
no new gameplay/test pass is asserted. S2's remaining rule families and original
policy behavior gates remain open; S3–S14 remain pending. Next: complete the
remaining block/posture, fatigue-state and crime-policy rule review.

### S2 sustained incapacitation requirement

Commit `446e3dfc4e` records selected campaign policy explanations. Implemented
the native sustained-incapacitation predicate: strict negative fatigue,
paralysis or existing essential unconsciousness. Exactly zero permits the
recovery branch when neither independent cause remains. This is not an
animation completion or random impact-knockdown decision.

Two new tests fail against the stub, then pass. All **1,764 component tests**
and **120 focused ASan/UBSan tests** pass with exact inventories/no skips
(`S2/incapacitation-01`). **96 original instruction cases** independently
confirm entry and recovery conditions, including signed zeros/subnormals,
extremes and paralysis/state combinations. No live native recovery acceptance
is asserted. S2 remains in progress; S3–S14 remain pending. Next: finish block
contact eligibility and remaining crime-policy rules.

Tested implementation/test/provenance diff SHA-256: `1131820285233ba4586844538552e988390fcf5e314afafbd9fbd3275e9353ce`.

### S2 block contact eligibility

Commit `1e5ec46c10` contains sustained incapacitation rules. Implemented a
block-contact disposition that preserves active posture, paralysis/sneak
bypass, facing-cone eligibility and the zero-absorption unarmed reaction branch
against weapon/projectile attacks. It composes with existing block arithmetic
and does not mutate actor/inventory state.

Two new tests fail against the stub, then pass. All **1,766 component tests**
and **122 focused ASan/UBSan tests** pass with exact inventories/no skips
(`S2/block-contact-01`). **160 original instruction cases** independently
verify the gates and unarmed absorption exception. Native normal-input block
and reaction ordering remain runtime gates. S2 remains in progress; S3–S14
remain pending. Next: native ownership inheritance and crime access rules.

Tested implementation/test/provenance diff SHA-256: `c5b8433d87acb798443b360312dd3ef19f453c8f2043f953f4bfc9dc33309c94`.

### S2 native ownership-field inheritance

Commit `c905c7143b` contains block-contact eligibility. Implemented independent
owner/rank/global inheritance with actor and furniture/door/activator owner
exceptions, direct teleport-destination fields, current-cell fallback and
signed rank -1 sentinel handling. This is a pure read projection over resolved
layers; authoritative world/inventory state remains the existing owner.

Three new tests fail against the stub, then pass. All **1,769 component tests**
and **125 focused ASan/UBSan tests** pass with exact inventories/no skips
(`S2/ownership-inheritance-01`). **35 original instruction cases** each execute
all three native queries and confirm precedence, exceptions and signed ranks.
Normal runtime ownership and access remain integration gates. S2 remains in
progress; S3–S14 remain pending. Next: cell/door public access and the remaining
crime-policy rules.

Tested implementation/test/provenance diff SHA-256: `21bee9b530d2be8b89b653698860c4a40867696f667d7cd3936f6adafa090a0e`.

### S2 native cell trespass classification

Commit `00cb062904` contains independent ownership-field inheritance.
Implemented the original cell trespass predicate with class-guard exemption,
public/hand-changed flags, permission-global presence, NPC identity and signed
faction-rank comparison. The global is intentionally not evaluated numerically
in this query. Door access, ownership claims and reported crime remain distinct.

Three new tests fail against the stub, then pass. All **1,772 component tests**
and **128 focused ASan/UBSan tests** pass with exact inventories/no skips
(`S2/cell-trespass-01`). **272 original instruction cases** independently
confirm every cell-flag byte, the exemption branches and rank boundaries.
No normal gameplay trespass/report acceptance is claimed. S2 remains in
progress; S3–S14 remain pending. Next: door/reference access and the remaining
crime-policy rules.

Tested implementation/test/provenance diff SHA-256: `62b95803a2e9be4ceaf9efdee7336c578d70d99cc8b39daf9c775cbb33e8035b`.

### S2 player trespass exit exemption

Commit `cb7250ddf0` contains cell trespass classification. Added the native
player door-exit exemption requiring current trespass, teleport/lock data,
lock level other than exactly 100, and an absent/exterior/public destination.
HandChanged alone does not make the destination public for this branch.
Actual unlock/activation and other door permissions remain separate.

Two new tests fail against the stub, then pass. All **1,774 component tests**
and **130 focused ASan/UBSan tests** pass with exact inventories/no skips
(`S2/door-exit-01`). **266 original instruction cases** independently confirm
all destination flag bytes, lock thresholds and missing-input branches.
S2 remains in progress; S3–S14 remain pending. Next: compose the native
reference off-limits policy and complete remaining crime rules.

Tested implementation/test/provenance diff SHA-256: `21b58fac13c4b47f6a1c5b9ff0cebf52212549552e72715e4babd7ed92ec6460`.

### S2 player reference off-limits policy

Commit `26ec466778` contains the player trespass exit exemption. Implemented
native player-facing reference access over resolved inputs: owner exemptions,
independent door/cell claims, native door permission, lock/destination policy,
living-NPC sneak activation and object/horse distinctions. The predicate does
not report an incident or create another ownership/inventory authority.

Five new tests fail against the stub, then pass. All **1,779 component tests**
and **135 focused ASan/UBSan tests** pass with exact inventories/no skips
(`S2/reference-access-01`). **291 original instruction cases** independently
verify the access branches and every destination flag byte. Normal-input
crime, complete door permission and world resolution remain runtime gates.
S2 remains in progress; S3–S14 remain pending. Next: prohibited pickpocket
items/transfer session rules and remaining crime legality/reporting rules.

Tested implementation/test/provenance diff SHA-256: `c4f3e29fe980760cc74ddf2ece1977102d18054557ec296498c490b66c2b8a61`.

### S2 pickpocket item eligibility

Commit `f62945d1d1` contains player reference off-limits policy. Added separate
native taking/placing item restrictions: biped playability, worn and bound
entries, quest-item asymmetry, drawn equipped weapon and positive weight.
Transfer-click session ordering is recorded for integration.

Three new tests fail against the stub, then pass. All **1,782 component tests**
and **138 focused ASan/UBSan tests** pass with exact inventories/no skips
(`S2/pickpocket-items-01`). **73 original instruction cases** independently
verify visibility and transfer guards. No ordinary UI transfer, crime or
persistent session acceptance is claimed. S2 remains in progress; S3–S14
remain pending. Next: session eligibility and remaining crime rules.

Tested implementation/test/provenance diff SHA-256: `21b2310ad3b76c12996f0b3349b4cb62d81c5c29ca613c79ae2724fb109b9b92`.

### S2 pickpocket check scheduling

Commit `2485ca0368` contains item eligibility. Added native take/place/exit
check scheduling, keeping failed-roll detection suppression for knocked targets
separate from successful rolls. Untouched exit skips its roll against a knocked
target; taking still rolls. Transfer-click flag ordering is independently checked.

Two new tests fail against the stub, then pass. All **1,784 component tests**
and **140 focused ASan/UBSan tests** pass with exact inventories/no skips
(`S2/pickpocket-session-01`). **20 original instruction cases** confirm the
session branches. Runtime session, inventory and crime events remain integration
gates. S2 remains in progress; S3–S14 remain pending. Next: remaining assault,
murder, trespass and reporting rules.

Tested implementation/test/provenance diff SHA-256: `524696b6666d4197a670bcc4d9fd874d084581efeb4f1b229885a3759c253a7e`.

### S2 actor faction crime predicates

Commit `4a39e43fee` contains pickpocket check scheduling. Added the native
all-evil/nonempty and any-special-combat faction reductions. Mixed membership
and rank-independent flag semantics now have direct coverage, including their
consequences for reference owner exemptions.

Two new tests fail against the stub, then pass. All **1,786 component tests**
and **142 focused ASan/UBSan tests** pass with exact inventories/no skips
(`S2/faction-policy-01`). **1,284 original cases** execute both complete faction
predicate bodies without boundary-call stubs. S2 remains in progress; S3–S14
remain pending. Assault/murder legality, witness reporting and actual Arena
acceptance remain open.

Tested implementation/test/provenance diff SHA-256: `f3baf9b48aca12050c19610153620803bafaf63161358ab3fcc6ff943d4d3ed3`.

### S2 NPC assault/murder alarm-entry rules

Commit `91306ad7a7` contains actor faction predicates. Added native NPC-victim
assault/murder entry gates, preserving victim/offender identity, their different
trespass checks, playable-race and guard policy, jail/pursuit and special-combat
exemptions, and the non-player exact Sneak-100 posture exemption.

Three new tests fail against the stub, then pass. All **1,789 component tests**
and **145 focused ASan/UBSan tests** pass with exact inventories/no skips
(`S2/attack-alarm-01`). **8,752 original instruction cases** pass after correcting
a recorded harness register-setup failure. These entry predicates do not replace
higher-level self-defense, witness selection or reported bounty. S2 remains in
progress; S3–S14 remain pending. Next: trespass warning/report and witness rules.

Tested implementation/test/provenance diff SHA-256: `b20d76e758a02c4c89e95bfafa306a120fec4e50a822844fea660b0a811a0b0d`.

### S2 trespass warning decisions and timer

Commit `3f1e72ee4d` contains attack-alarm entry gates. Added warning/leave/
escalation decisions, Off Limits bypass, strict timer expiry, stored float
rounding and the typed warning-timer GMST (compiled 10, installed 30).

Three policy tests fail against the stub; four new tests pass after correcting
an independently disproved >= expiry expectation. All **1,793 component tests**,
**149 focused ASan/UBSan tests** and **151 Python tests** pass, with exact C++
inventories/no skips (`S2/trespass-warning-01`). **1,920 original instruction
cases** confirm the corrected policy. Harness setup and boundary-expectation
failures are retained. Actual warning speech/count callbacks, admission,
reporting, elapsed gameplay timing and restart remain open. S2 remains in
progress; S3–S14 remain pending. Next: witness selection and reporting rules.

Tested implementation/test/provenance diff SHA-256: `6bce82914c4072f25136a0bde8d9b990801f5e8bc1f709a5bf1f8404d5b34479`.

### S2 crime witness candidate filtering

Commit `106eac4fcf` contains trespass warning decisions. Added native candidate
filtering with distinct offender/witness life-state checks, asymmetric reference
flag masks, paralysis and identity gates, and strictly positive signed detection.

Three new tests fail against the stub, then pass. All **1,796 component tests**
and **152 focused ASan/UBSan tests** pass with exact inventories/no skips
(`S2/witness-candidate-01`). **19,600 original instruction cases** verify the
filter branches with real native life-state/dead/paralysis queries. Candidate
selection alone does not report bounty. S2 remains in progress; S3–S14 remain
pending. Next: crime alarm recipients, spatial reach and report delivery.

Tested implementation/test/provenance diff SHA-256: `63d055b33500c5eba2bd1293a087de78bcda3858a2c64837ed6df904251406ed`.

### S2 integration refresh through witness candidates

Revision `c3359e4512` builds `openmw`, `openmw-tests` and `esmtool` successfully.
All **552 engine tests** pass with exact inventory/no skips in
`S2/integration-05`; this supplements its 1,796 component and 152 focused
sanitizer tests. The build was completed before the next alarm-spatial-rule
edits. This is compile/regression evidence, not normal-input M15 runtime
acceptance. S2 remains in progress and S3–S14 remain pending.

### S2 alarm recipient spatial reach

Commit `e339640c0c` records the engine integration refresh. Added native direct
cell/exterior-worldspace reach, teleport-destination fallback, inclusive integer
radius and no door retry after direct-space distance rejection. The typed
radius adapter preserves compiled 10000 and installed 4000.

Three policy tests fail against the stub; four new tests pass. All **1,800
component tests**, **156 focused ASan/UBSan tests** and **151 Python tests** pass,
with exact C++ inventories/no skips (`S2/alarm-reach-01`). **1,728 original
instruction cases** verify spatial selection and radius branches. Runtime door
collection, alarm delivery, recipients' reactions and bounty remain open.
S2 remains in progress; S3–S14 remain pending.

Tested implementation/test/provenance diff SHA-256: `91cd3e5f1e6a952416eff209ea26d066fc405231f00822577c6da24b3a369264`.

### S2 native fight score

Commit `df6963c2f6` contains alarm recipient spatial reach. Added immutable
fight-score inputs/settings, the typed GMST adapter, strict friend and
responsibility comparisons, negative distance penalties and native float
rounding/integer conversion order.

All **1,805 component tests**, **161 focused ASan/UBSan tests** and **151 Python
tests** pass, with exact C++ inventories/no skips (`S2/fight-score-01`). Four
policy tests first fail against the zero stub. **6,426 original instruction
cases** pass in both x87 precision modes; all match the compiled C++ helper.
Two preliminary comparison-direction errors were found by the independent
oracle, corrected and retained as failed evidence. Normal-input AI target
selection, alarm response and combat dispatch remain open. S2 remains in
progress; S3–S14 remain pending.

Tested implementation/test/provenance diff SHA-256: `1e19f6fde1db807b0221c404cbbc5dc3c02a009ea79d70f22f05d1b5f9bdb626`.

### S2 alarm recipient response eligibility

Commit `253459f566` contains the native fight score. Added delivery eligibility
for guards and other recipients, including sleeping state 9, existing alarm
packages, active combat, incident guard suppression and emitter evil policy.

All **1,807 component tests** and **163 focused ASan/UBSan tests** pass with
exact inventories/no skips (`S2/alarm-response-01`). Two policy tests first
fail against the stub; **2,816 original instruction cases** independently
verify delivery branches and real package/sleep queries. Process dispatch,
reporting and bounty commitment remain open. S2 remains in progress;
S3–S14 remain pending.

Tested implementation/test/provenance diff SHA-256: `42fe15f52eaa18a652871da395391580ac164b9051070c5b10b30b3fa0c6eaf7`.

### S2 common report gates and bounty-driven infamy

Commit `dad9edb200` contains alarm recipient response eligibility. Added the
shared NPC-offender/report-idempotence/responsibility gate, and the normal
player infamy accumulator with strict increment >1, integer/float rounding,
and at most one threshold subtraction per increment.

All **1,812 component tests**, **168 focused ASan/UBSan tests**, and **151 Python
tests** pass, with exact C++ inventories/no skips (`S2/report-infamy-01`). Four
policy tests first fail against stubs; **384 original report-gate cases** and
**480 original infamy cases** verify the rules. The infamy cases pass in both
x87 precision modes. The recorded null-incident harness-stop failure is fixed.
Atomic world reporting, alternate bounty routing, statistics and persistence
remain open. S2 remains in progress; S3–S14 remain pending.

Tested implementation/test/provenance diff SHA-256: `d1ba2d7bd868bb0d8816c7efa403ac10e6c98ded487de7839158971456b3ee35`.

### Reusable build/test and original-oracle skills

Commit `53e28f6e61` contains report gates and infamy accumulation. Added versioned
skills under `.codex/skills/openmw-build-test` and
`.codex/skills/oblivion-native-oracle`, with personal-directory symlinks to their
source. They preserve build configurations, exact-inventory verification,
source/binary evidence fingerprints, original PE identity/section inspection,
emulator pitfalls and isolated live/restart probe procedures.

Both skill manifests validate. **17 helper tests** pass, including rejected
missing/skipped/duplicate results, source drift, inherited GoogleTest filtering/
sharding, build/command failures, timeout and existing-output preservation.
Original PE metadata agrees with the prior section map; wrong hash and output
overwrite attempts fail. A real runner pass builds and verifies **1,812 component
tests**, **290 ASan/UBSan ESM4 tests** and **151 Python tests**, with no skips and
exact C++ inventories (`build/oblivion-compat/skill-validation-02/checks`). The
sanitizer filter now covers all `ESM4*` suites, including future additions.
The prior stale SDL2 link failure is retained under `skill-validation-01`;
refreshing both existing CMake configurations resolved it without changing
project options. This improves verification tooling; M15 runtime gates remain
unchanged.

### S2 bounty storage and realm routing

Commit `3bdc1d5581` adds the reusable verification skills. Added normal versus
player Shivering Isles bounty storage/query/update rules, preserving stored
fractions, minimum positive exposed bounty, normal-only zero clamping and
normal-player statistics dispatch.

All **1,815 component tests** and **293 ASan/UBSan ESM4 tests** pass with exact
inventories/no skips through the new runner (`S2/bounty-storage-02`). Three
policy tests first fail against stubs; **512 original mutation cases**, each
with before/after queries, pass in both x87 precision modes. Runtime bucket
storage, player realm flag/script adapters and atomic crime/save integration
remain open. S2 remains in progress; S3–S14 remain pending.

Tested source fingerprint: `c07d337e14c5a00dc35308005fd034c019db1970551afaabaa773968ed63e599`.

### S2 arrow age expiry and fade

Commit `efcf9f32d1` contains bounty storage and realm routing. Added immutable
arrow age/opacity advancement, strict stored-age expiry, three-second fading,
zero-opacity removal, and a typed native maximum-age setting adapter.

All **1,819 component tests**, **297 ASan/UBSan ESM4 tests**, and **151 Python
tests** pass with exact C++ inventories/no skips (`S2/arrow-lifetime-02`). Three
policy tests first fail against a stub (`arrow-lifetime-01`). **2,016 original
instruction cases** pass in both x87 precision modes and match a separately
compiled C++ driver exactly. Runtime projectile authority, collision/recovery,
visual fading/removal and fresh-process persistence remain open. S2 remains
in progress; S3–S14 remain pending.

Tested source fingerprint: `9da6a065b5d3a38c9a8a5e9011f23e2d87275b2329c71e2a42ac4103ae94dd69`.

### S2 final arrow inventory-recovery roll

Commit `e3a4748ba6` contains native arrow lifetime/fading. Added the final
actor-inventory recovery rule, arrow-enchantment draw suppression, strict
percentage comparison and typed native setting adapter.

All **1,823 component tests**, **301 ASan/UBSan ESM4 tests**, and **151 Python
tests** pass with exact C++ inventories/no skips (`S2/arrow-recovery-02`). Three
new tests first fail against the stub (`arrow-recovery-01`). **1,200 original
instruction boundary cases** pass; a predeclared 100,000-sample original branch
run recovers 49,870 arrows within its fixed tolerance. Earlier impact/creature
eligibility, real inventory insertion, collisions and save/restart remain open.
S2 remains in progress; S3–S14 remain pending.

Tested source fingerprint: `ba2a546aec0ec97dcd289df1bf6eb6df1429fa8a6d4554c2f021484f9c6fc178`.

### S2 arrow reference-count cleanup

Commit `53aaa6d1c6` contains the final arrow inventory-recovery roll. Added
strict post-creation count gating, preferred/fallback pool selection, oldest
positive-age settled-arrow selection and stable equal-age ties. Cleanup selects
one arrow to start fading; it does not impose a hard cap on flying arrows.

All **1,827 component tests**, **305 ASan/UBSan ESM4 tests**, **151 Python
tests** and **552 engine tests** pass with exact C++ inventories/no skips
(`S2/arrow-cleanup-02`). `openmw`, `openmw-tests` and `esmtool` rebuilt
successfully. Three policy tests first fail against the stub
(`arrow-cleanup-01`). **6,586 original instruction cases** pass in both x87
precision modes and match a separately compiled C++ driver exactly. Runtime
collection/removal and save/restart resource bounds remain open. S2 remains
in progress; S3–S14 remain pending. Next: native fatigue regeneration.

Tested source fingerprint: `176ea643f66b10929a7305421fc8ada40c672fc6911fdd1941e74e4ed487e912`.

### S2 fatigue regeneration

Commit `24c958dfd0` contains arrow cleanup and the accumulated engine rebuild.
Added native fatigue-regeneration eligibility, floored actor-value comparison,
maximum modifiers, separate rate/duration rounding and positive-only restoration
requests, with typed compiled/installed setting inputs.

All **1,831 component tests** and **309 ASan/UBSan ESM4 tests** pass with exact
inventories/no skips (`S2/fatigue-regeneration-02`). Three policy tests first
fail against the stub (`fatigue-regeneration-01`). **32,404 original instruction
cases** pass in both x87 modes and match a separately compiled C++ driver
exactly. Live AV mutation/clamping and gameplay recovery remain integration
requirements. The latest unchanged Python suite remains 151 tests and engine
suite 552 at the prior cleanup checkpoint; neither was rerun for this pure
C++ rule chunk. S2 remains in progress; S3–S14 remain pending.

Tested source fingerprint: `3c7ab9516029ef2008779e6a07f0142151054d5b1cd2015276a36df7be633f40`.

### S2 native actor level lookup

Commit `bcbab64b8a` contains fatigue regeneration. Added a pure raw ACBS level
lookup preserving fixed-level behavior, player-offset word arithmetic, minimum/
maximum order, zero-bound sentinels and the final minimum-one fallback.

All **1,834 component tests** and **312 ASan/UBSan ESM4 tests** pass with exact
inventories/no skips (`S2/actor-level-02`). Three new tests first fail against
the stub (`actor-level-01`). **3,528 complete original-helper cases** pass and
match a separately compiled C++ driver exactly. Actor construction still uses
the old fixed-level adapter; replacing it, auto-calculated stats and their
persistence remain open. S2 remains in progress; S3–S14 remain pending.
Next: NPC auto-calculation and the winning skill-definition input gap.

Tested source fingerprint: `2ce0167770005c35ac8a84b4dd8812319161d3947d9cb73ba1f7cf90ea41aee2`.

### S2 NPC dynamic base-stat calculation

Commit `6c1b26a075` contains native actor-level lookup. Added NPC base health,
magicka and fatigue calculations with typed native GMST inputs, levels-gained
semantics, low-level scaling, favored Endurance and specialization adjustments.
All **1,838 component tests** and **316 ASan/UBSan ESM4 tests** pass with exact
inventories/no skips (`S2/npc-dynamic-stats-02`). **6,300 original helper cases**
match compiled C++ exactly in both x87 precision modes. Attempt `-01` retains
one incorrect hand-transcribed test expectation, corrected from original output.
Actor construction and full attribute/skill auto-calculation remain open.
S2 remains in progress; S3–S14 remain pending. Unchanged Python/engine suites
were not rerun for this pure C++ chunk.

### S2 NPC attribute and skill auto-calculation

Commit `2d5dfd1395` contains dynamic base-stat calculation. Added immutable NPC
attribute/skill rules with winning-definition inputs, authored Personality,
nearest-even rounding, major/minor growth, favored/specialization bonuses and
ordered signed racial bonuses. All **1,843 component tests** and **321
ASan/UBSan ESM4 tests** pass with exact inventories/no skips
(`S2/npc-auto-stats-01`). **384 original full auto-calculation cases** match
all 32 resulting stat fields in compiled C++, across both race sexes and x87
modes. Original notification boundaries are stubbed, not arithmetic or lookup.
Next: preserve and resolve the missing native record inputs. S2 remains in
progress; S3–S14 remain pending.

### S2 winning SKIL records and lossless TES4 race bonuses

Commit `c8ab840857` contains NPC attribute/skill calculation. Added typed TES4
SKIL parsing, complete winning-definition resolution, engine store/loader and
esmtool registration, plus independent Python auditing. TES4 RACE now preserves
seven ordered signed skill bonuses and separates padding from the bonus list.

All **1,849 component tests**, **327 ASan/UBSan ESM4 tests**, and **153 Python
tests** pass (`S2/npc-stat-records-01`). Engine-only registration fixes then
pass **553 engine tests** and rebuild openmw/openmw-tests/esmtool
(`npc-stat-records-03`); the two failed build attempts are retained. All eleven
official plugins parse, and 21 typed SKIL definitions agree with independent
raw decoding (`native-stat-record-dumps-01`). `audit-13` passes the expanded
content count lock with zero data failures. S2 remains in progress; S3–S14
remain pending. Next: creature base-stat scaling and actor construction inputs.

### S2 creature base stats

Commit `48fa6de6ce` contains the native record integration. Added creature
skill-group, natural damage and dynamic base-stat scaling, preserving native
truncation and byte/word wrapping. All **1,853 component tests** and **331
ASan/UBSan ESM4 tests** pass (`S2/creature-base-stats-01`). Production C++
matches **2,880 original-executable cases** exactly; **420 original dispatch
cases** independently verify skill-group mapping. Updated the native setting
manifest with verified NPC and creature inputs. S2 remains in progress;
S3–S14 remain pending. Next: winning-store actor construction inputs.

### S2 winning-store actor stat resolver

Commit `6591e83b5f` contains creature scaling. Added a read-only engine resolver
for native NPC/creature construction inputs: stable actor keys, winning race,
class, SKIL and GMST records, effective levels, signed racial bonuses and native
base-stat rules. Fixed actors retain authored fields; the player base explicitly
requires the separate M12 path. Diagnostics identify missing or unsupported
inputs instead of using TES3 definitions. Base records are never mutated.

All **557 engine tests** pass and openmw/openmw-tests/esmtool build successfully
(`S2/actor-stat-store-01`). New actual-store tests cover original NPC expected
values, skill override/deletion, fixed NPCs, scaled creatures and invalid
inputs. Pure rule suites passed in the preceding chunk and were not rerun.
The resolver is not yet called by live actor construction; that integration
and S3 state authority remain next. S2 remains in progress; S3–S14 pending.

### S2 live native actor construction

Commit `e37c66685b` contains the winning-store resolver. Native NPC and creature
classes now use it at construction, including the live player base level for
scaled actors. A construction-only shared-stat initializer installs native
values without invoking TES3 derived-stat or gameplay death transitions.
Creature class skill reads use the canonical 21 native AV mappings rather than
TES3 skill specialization; natural damage is retained with its native inputs.
Later-game construction keeps its prior path. Runtime mutations, level-change
refresh and save authority remain S3 work.

All **560 engine tests** pass, with successful openmw/openmw-tests/esmtool builds
(`S2/live-actor-stats-01`). New tests construct actual native actor instances
and query class health, fatigue, magicka, attributes and skills, including
canonical creature grouping despite deliberately conflicting shared skill
metadata. Initialization rejects invalid/repeated writes before changing stats.
The existing animation-state compiler warning remains. A subsequent whitespace
cleanup changes no compiled behavior.

The unchanged official M14 tutorial escort scenario passes in
`S2/live-actor-tutorial-01`: stage progression, all four escort route/door
requirements, inventory/AI save checkpoints and in-process reload pass with
no unreviewed errors. This is construction/AI regression evidence, not M15
combat or fresh-process stat persistence acceptance. The inspected screenshot
shows nearby dungeon geometry and does not establish actor visual quality.
S2 remains in progress; S3–S14 remain pending.

### S2 runtime creature Marksman correction

Commit `cbc118ce5a` contains live construction. Subsequent original runtime
wrapper inspection found Marksman aliases Combat on creatures, despite its
base-record Stealth grouping. Corrected the live class query and its regression
expectation. All **560 engine tests** pass with rebuilt binaries
(`S2/creature-actor-av-01`); **1,776 independent original forwarding cases**
verify the getter/setter/modifier aliases and argument preservation. S2 remains
in progress; actor-value mutation/persistence investigation continues for S3.

### S2 scalar actor-value modifier arithmetic

Commit `d64619730f` contains the creature runtime alias correction. Added the
verified scalar modifier-add rule needed by native stat authority. All **1,855
component tests** and **333 ASan/UBSan ESM4 tests** pass
(`S2/actor-modifier-add-01`), with **1,156 bit-exact original helper cases**
matching compiled production C++. This preserves float-store order, optional
nonpositive clamping and signed zero. Full actor mutation and persistence are
still pending; this helper does not close S3.

### S2 sparse NPC modifier arithmetic

Commit `136faf0d86` contains scalar modifier addition. Added the NPC sparse-map
variant preserving absent-entry behavior and zero removal. All **1,856 component
tests** and **334 ASan/UBSan ESM4 tests** pass (`S2/actor-sparse-modifier-01`);
**2,312 original cases** match production bits and entry presence exactly.
This identifies a required persistence distinction before S3 integration.
S2 remains in progress; S3–S14 remain pending.

### S2 native scalar actor-value state

Commit `ae37102c59` contains sparse modifier arithmetic. Added a native scalar
state core retaining three separate modifier categories and entry presence,
with explicit player/NPC and low/active composition semantics. All **1,860
component tests** and **338 ASan/UBSan ESM4 tests** pass
(`S2/actor-value-state-01`); **6,200 original composition cases** match
production output bits exactly. Immutable mutation and corruption tests cover
category isolation and invalid state. This core is not yet the engine's live
stat authority or save representation. S2 remains in progress; S3 integration
and S4–S14 remain pending.

### S3 shared stat projection foundation

Commit `8f69a81ac0` contains the scalar state core. Attribute and skill views
can now expose its exact native current value, including negative values and
player/NPC rounding differences. Legacy mutations reject a native view rather
than silently destroying modifier categories. Failed projection validation
leaves the previous view unchanged. This mode is not yet enabled on gameplay
actors; service mutation, dynamic views and persistence must be integrated first.

All **562 engine tests** pass with rebuilt openmw/openmw-tests/esmtool
(`S3/stat-projection-01`, source fingerprint
`9d3ef1fe2ae1f1c5278d4f3d5a54d5335a4462e386620f34b7f20d4c259c85aa`).
New cases cover exact composition, negative current, rejected legacy writes,
atomic validation, comparison and preserved legacy behavior. S3 is in progress;
its live-authority and save/restart gate remains open.

### S3 dynamic stat projection foundation

Commit `9546faabc5` contains attribute/skill projections. Dynamic stat views
now accept validated native base, maximum and current values without shared
clamping or reconstructing the maximum through a rounded modifier. Legacy
writes reject these views; validation failures leave the view unchanged.
The native service will supply AV-specific formulas; this adapter does not
guess them. Gameplay activation still awaits service and save integration.

All **565 engine tests** pass with rebuilt openmw/openmw-tests/esmtool
(`S3/dynamic-projection-01`). Tests cover negative and above-maximum current,
small exact maxima lost by subtraction/addition, zero ratios, corrupt inputs,
integer overflow, write rejection and unchanged legacy serialization/clamps.
S3's live-authority and persistence gate remains open.

### S3 bounded action identity ledger

Commit `2b15069932` contains dynamic projections. Added the action identity
ledger: monotonically allocate IDs and retain only unfinished actions. Every
issued ID absent from that set is consumed. This permits an old projectile to
resolve after later attacks, rejects replay after restoration, and bounds
history by unfinished work. Cancellation consumes an ID; exhaustion never
wraps or reuses one. Restore validates a replacement before changing state.

All **1,865 component tests** and **343 ASan/UBSan ESM4 tests** pass
(`S3/action-ledger-01`). Cases include 100,000 completions with one old pending
action, deterministic randomized completion/restoration against a separate
full-history model, malformed IDs and exhaustion. Service transactions and
versioned save integration remain pending; this ledger alone is not a hit
transaction or runtime acceptance.

### S3 profile-owned action service and runtime schema v8

Commit `d7a16da355` contains the bounded ledger. World now constructs the native
combat service only for Oblivion, clears it on new game/world clear (including
failed-load cleanup), and captures/restores its owned ledger. Runtime schema
**8** adds canonical physical action state to binary/JSON/Python readers.
Versions 1–7 deliberately start an empty action namespace without borrowing
AI/script identities. Invalid restoration leaves live service state unchanged.
The service has no gameplay hit producers or actor-stat authority yet.

`S3/action-service-save-01` passes **1,868 component**, **346 ASan/UBSan ESM4**,
**156 Python**, and **568 engine tests**, with rebuilt openmw/openmw-tests/esmtool.
Coverage includes canonical wire bytes, corrupt/truncated/oversized action lists,
all seven legacy versions, foreign profiles, out-of-order replay rejection and
service clear/restore. Existing inventory initializer warnings remain.

Actual runtime evidence under `build/oblivion-compat/m15/S3/`:

- `action-service-restart-01`: unchanged observation/continuation manifests pass
  two distinct processes, exact input-save receipt, live-vs-disk canonical
  agreement, movement and resave. Both carry v8 idle physical action state.
- `action-service-legacy-01`: the editable
  `oblivion_m15_legacy_action_service.json` loads a copied S1 v7 save and resaves
  v8 with `{next:1,pending:[]}`. `migration-verification.json` records the source,
  output and binary hashes; the original and pristine input remain byte-identical.
  Source: `S1/ticks-restart-01/first/snapshots/after.omwsave`, SHA-256
  `ceccf09c066272636aa4e8a49e0d34f2825202cf0dd258622ca2685a4af0b439`.
- `action-service-morrowind-01`: unchanged M14 Morrowind isolation scenario passes;
  actual save contains no GPRO/T4VR/T4ST/OMW4STATE/native-state markers.

These runs used openmw SHA-256
`ada1dafbbe59ec94ee586632901563d8c31b11ca5bde18ce7d08df4d10c2dc5f`.
Inspected screenshots show the textured native fixture with shared HUD and a
Morrowind camera near terrain. They establish no combat/death visual acceptance
or playable Balmora course. All runs are silent. S3 remains in progress:
actor authority, transaction/event/RNG state and active-gameplay persistence
remain required; S4–S14 gates remain open.

### S3 whole-value write guards

Commit `bee12809b7` contains the v8 service/save integration. Closed a bypass
in the stat-view foundation: assigning a replacement attribute, skill or
dynamic stat cannot overwrite a native view. Copies preserve native mode.
Shared actor setters reject native values before legacy derived-stat/death
side effects; mutable skill references cannot replace an existing view.
Explicit native projection methods remain the intended service entry points.

All **570 engine tests** pass with rebuilt openmw/openmw-tests/esmtool
(`S3/stat-authority-guards-01`). New cases check rejected copy/rvalue assignment,
unchanged health/death/magicka/fatigue/skill state on failure, and retained legacy
assignment behavior. Native gameplay views remain disabled until actor authority
and its writers/save representation are fully integrated.

### S2 player derived-base arithmetic for S3 authority

Commit `eaed0929d3` contains the stat write guards. Added native player dynamic
base contribution rules and the verified AV40 Magicka scale. Separate form
contributions and current-attribute adjustments prevent double-counting in the
planned authority. An assumed Magicka divisor of 100 was rejected by original
instruction execution; the verified divisor is 10.

All **1,872 component tests** and **350 ASan/UBSan ESM4 tests** pass
(`S2/player-dynamic-base-01`), and **2,536 original instruction cases** match
production bits exactly. Added compiled/winning GMST provenance. These helpers
are not yet wired to live actor updates; S2/S3 remain in progress.

### S2 current integer actor-value query

Commit `7a58176cc2` contains player derived-base rules. Added current integer
composition with native player/NPC truncation boundaries, rather than casting
the float query or flooring its result. All **1,875 component tests** and
**353 ASan/UBSan ESM4 tests** pass (`S2/actor-integer-composition-01`), with
**8,280 independent original cases** matching production exactly. These are
necessary inputs to derived-stat authority; runtime query integration remains
S3 work.

### S3 prepared actor projection transaction

Commit `67263300f1` contains the integer AV composition rules. Added
`OblivionActorProjection`, which prepares attribute, NPC skill and dynamic
views without changing the actor. Preparation validates complete input and
preserves skill progress; its one-shot, nonthrowing commit performs no
allocation, callbacks, TES3 derived-stat recalculation or implicit death.
A repeated commit cannot restore stale views. The synchronous target must
outlive preparation/commit; the native authority must commit its candidate
state and these views without intervening callbacks.

All **573 engine tests** pass with rebuilt openmw/openmw-tests/esmtool
(`S3/actor-projection-transaction-02`). Coverage includes late validation
failure, abandoned preparation, updating existing native views, low-process
composition, creature dynamic views, write guards and a real native NPC whose
class skill/health/attribute/capacity getters immediately see the transaction.
The underlying NPC record remains unchanged. This is the shared-view commit
bridge, not yet persistent actor authority or gameplay activation. Creature
skill authority, native writers, save representation and S3 acceptance remain
open; no combat/runtime gate is claimed from these tests.

### S3 native actor-value save representation

Commit `ab2b38e0a7` contains the prepared projection bridge. Schema **9** adds
`native_actor_values`: stable actor/base identities, player/nonplayer owner,
low/active process, and 72 native AV entries with resolved base and three
independently optional modifiers. Wire entries use a float32 base, a three-bit
presence mask, then present float32 maximum/script/damage values. Actor records
sort by FormKey; JSON uses `[base, maximum-or-null, script-or-null, damage-or-null]`.
Stored positive/negative zero remains distinct from absence in the binary.
Versions 1–8 have no native entries; they do not invent modifier categories from
legacy combined UI values. Actual legacy actor initialization/reconciliation is
still part of the pending live-authority integration.

C++ and Python validate canonical identities, actor/reference/base agreement,
player ownership, duplicate actors, exact AV/category shape, enum domains,
finite values/composition, masks, size limits, truncation and schema gates.
The World-owned service retains these records without pointers, provides const
lookup, captures them, clears them on teardown, and prepares actor/action state
together before restoring either. This does not yet activate native gameplay
stat writes or reconstruct live projections on load.

Verification: **1,879 component /357 ASan+UBSan ESM4 tests** pass in
`S3/native-actor-values-save-02`; **159 Python /574 engine tests** pass in
`S3/native-actor-values-save-03`, with rebuilt openmw/openmw-tests/esmtool.
Attempts `-01` and `-02` retain failed Python migration fixtures (missing
character-generation/name and AI defaults), corrected before `-03`. New checks
cover exact sparse wire bytes, signed zero, all eight legacy versions, malformed
payloads, canonical ordering and atomic service restore. Existing inventory
initializer warnings remain.

Real runtime evidence under `build/oblivion-compat/m15/S3/`:

- `native-actor-values-restart-01`: unchanged S1 save/quit/fresh-load/resave
  passes with full live-C++ versus decoded-binary agreement in schema 9.
- `native-actor-values-legacy-01`: copied v8 save migrated to v9 with empty native
  values and unchanged action ledger; original/pristine hashes are unchanged.
  Independent `migration-verification.json` passed. Input SHA-256:
  `7a071b415a1acbbf96d0ffd9e32471e43f4d2647213c83ad1e43f4d55fc5286c`.
- `native-actor-values-morrowind-01`: unchanged profile-isolation scenario passes;
  actual Morrowind save has no native TES4 state markers.

Runtime executable SHA-256:
`df92c78e8cd93bc54832fa42e90370000dbf067915de6d2df6dc11c9bd7705d6`.
Inspected images show the textured native fixture/HUD and Morrowind terrain;
all scenarios are silent. These are idle-service persistence checks, not combat,
active native actor continuation, audio, or full Morrowind gameplay acceptance.
S3 remains open, as do S4–S14.

### S3 native NPC value publication and mutation

Commit `971cc456fa` contains schema 9. The native service can now publish a
validated TES4 NPC value record to the actual class's shared views, read float
and integer values, and apply a modifier-category transition against its owned
state. It validates the live reference/base identities and TES4 record version.
It prepares all views before updating the owned record, then commits without
callbacks or throwing operations. No base content record is changed. Magicka
uses the verified nonplayer outer scale; dynamic maximum uses integer base plus
eligible maximum modifier. Encumbrance and process Paralysis explicitly remain
unsupported scalar queries. Death/eligibility/event policy is not inferred by
this scalar transition.

**1,881 component /359 ASan+UBSan tests** pass (`S3/npc-value-authority-01`),
**575 engine tests** pass with all three binaries rebuilt
(`S3/npc-value-authority-04`), and **3,344 original-executable maximum/Magicka
cases** match production. The real native NPC integration fixture checks class
and service readers, distinct integer rounding, modifier changes, low-process
views, binary save/restore into a fresh actor instance, unchanged base records,
legacy write rejection and atomic failure (including finite stored values whose
outer Magicka projection overflows). Preserved attempts include a missing
record-header build failure and a synthetic base record missing its FormKey;
production identity validation was not relaxed to accommodate that fixture.

This service path is explicitly callable and tested on actual native NPCs but
is **not automatically enabled by actor updates or scripts yet**. Creature and
player authority, live writer routing, legacy reconciliation and gameplay
continuation remain S3 work. The prior idle schema 9 runtime evidence does not
establish live NPC combat acceptance for this change.

### S3 creature authority and stable prepared stat views

Commit `54f6b48852` contains NPC publication. The explicit service interface is
now `publish/change/getNonPlayerValues` (singular `Value` for changes/queries),
supporting both native NPC and creature references with typed/version/base
identity checks. Creature skill reads and modifier mutations use the original
runtime group aliases established by the earlier 1,776 forwarding oracle cases:
AV12–18 plus Marksman28 -> Combat12, AV19–25 -> Magic19, remaining AV26–32 ->
Stealth26. NPC skills remain independent. Creature skill projections now retain
float values and expose only const access outside construction and the service.

Prepared attribute/skill updates use fixed arrays and validated target pointers
instead of allocating/copying/replacing maps. The synchronous commit writes only
prepared scalar fields, preserves skill progress and existing stat references,
and still cannot replay. Creature skill-cache replacement is part of the same
callback-free, nonthrowing commit after native state publication.

All **575 engine tests** pass with rebuilt openmw/openmw-tests/esmtool
(`S3/creature-value-authority-01`). Expanded actual-class fixtures cover all 21
creature skills, fractional group modifiers, Marksman read/write aliasing,
unchanged raw base records, low/active transitions, failed publication and binary
restore into a fresh creature instance. NPC Marksman remains independent, and
prepared views retain existing attribute/skill addresses and progress.
These are service/class integration checks; automatic world/script activation,
player authority, migration reconciliation, death/eligibility policy and the
remaining S3–S14 gates are still open. No runtime performance budget is claimed
from eliminating map copies alone.

### S3 player raw inputs and derived-value authority

Commit `be764789de` contains the preceding creature chunk. Schema 10 adds an
optional four-int32 `player_form_values` field to each native actor entry,
after its 72 scalar states in binary (strict presence byte, then AV8–11 inputs).
Only player entries may contain it. Resolved bases remain distinct; old v9
entries preserve their values and migrate with absent raw inputs, never guessed
zeroes. Downgrading populated inputs fails before service capture changes its
target. C++ and Python retain equivalent wire/JSON validation.

The service can explicitly publish/mutate the actual projected `MWWorld::Player`
using the existing player/player-base aliases. Publication requires raw inputs,
resolves current integer attributes and float AV40, recomputes all four derived
bases using the previously verified original rules and caller-supplied winning
settings, then prepares and commits all shared views atomically. Player Magicka
is not scaled a second time by the NPC outer-query rule. Player Paralysis remains
an ordinary scalar; current Encumbrance requires the separate inventory path.
Negative health is preserved without inferring death policy.

The actual-player fixture uses the real NPC class, custom data, ESMStore and
WorldModel. It exercises fractional versus integer attributes, derived changes,
repeated publication, retained raw inputs, overflow/missing-input rejection,
read-only shared views, binary restore into a fresh Player and a further
modifier change after restore. The initial preflight build failure was two
ambiguous optional initializers in the test, corrected before the passing
five-test player/service preflight; logs remain under `S3/player-preflight-*`.

This is explicit service authority, not automatic gameplay activation. Winning
base-input initialization, legacy reconciliation, World writer routing and
active gameplay acceptance remain open. Verification results follow below.

Verification `S3/player-value-authority-01` passes **1,882 component /360
ASan+UBSan ESM4 /160 Python /576 engine tests**, with all three engine binaries
rebuilt. Source fingerprint:
`9187d1f78e5cabecfd3027251baa314cfbb3be9eedb979424704ecbfc3a8fb4e`.
The additional Python nonplayer rejection fixture initially indexed an empty
reference list; it was corrected to construct a real reference before the
passing preflight and recorded full run. Existing inventory initializer warnings
remain; sanitizer coverage is components only, with leak checks disabled.

Real-engine `S3/player-value-restart-01` passes save/quit/fresh-load/resave and
full live/binary comparison in schema 10. `S3/player-value-legacy-01` passes
actual v9 load/resave, independent schema/ledger/native-state checks and unchanged
source/pristine hashes. Input SHA-256:
`3f81fc4e94500d2fc2c1a49e31a37fbe08c159f6a7dd07607b0e706bbdd25e69`.
`S3/player-value-morrowind-01` passes unchanged profile-isolation checks, including
absence of native markers in the actual Morrowind save. Runtime executable
SHA-256: `5920032ea33c4ea071adabe24cda96e47e12d02a56ae71cf2c9918e5ceab2eac`.
Inspected images show the textured native fixture/HUD and the existing
terrain-facing Morrowind view. These silent idle scenarios have empty native
actor vectors; they establish schema migration/isolation, not active player
combat, audio, or full Morrowind gameplay acceptance. S3 remains in progress.

### S3 winning player dynamic settings

Commit `ed674532eb` contains player publication/schema 10. Native player settings
now have a typed builder and an ESMStore adapter that resolves current winning
TES4 records rather than shared TES3 aliases. Defaults remain the independently
verified health 2, Magicka .5, capacity 5; installed Magicka 1 overrides .5.
Finite negative/zero overrides retain native arithmetic; wrong types, nonfinite
values and ambiguous case-insensitive names fail explicitly.

`S3/player-settings-01` passes **1,883 component /361 ASan+UBSan ESM4 /576
engine tests**, rebuilding openmw/openmw-tests/esmtool. The actual-player fixture
uses the store adapter, ignores a conflicting TES3 setting, republishes after
a native override changes Magicka 1 -> 2, and removes the override to recover
the .5 compiled default. Raw player form contributions remain unchanged across
these derived recalculations. No save format or automatically activated gameplay
path changed; preceding runtime evidence is not claimed as an active-stat test.

### S2/S3 ForceAV command delta

The original command handlers now have 10,230 independent instruction cases;
3,408 Force deltas match the C++ helper bit for bit. It preserves the exact
int32 request until subtraction and the final float store. Actual player/NPC
fixtures apply the delta through existing owned Script storage and shared
projections, checking unchanged maximum and NPC Magicka's outer-scale behavior.
`S3/force-actor-value-01` passes **1,884 component /362 ASan+UBSan ESM4 /576
engine tests**, with all three binaries rebuilt. No schema or automatic script
activation changed. Native SetAV base ownership, command eligibility/events,
and complete World/script writer routing remain open; the handler oracle stops
at actor virtual mutation calls and does not establish those later transitions.

### S3 unloaded reads and native restore preflight

Commit `eb96e91298` contains ForceAV delta arithmetic. The service now reads
saved nonplayer float/integer/base values by stable key against winning content,
without loading a cell or constructing a live actor. Content actors require the
matching ACHR/ACRE and base key; dynamic actors require a valid native base but
no content placement record. Missing, ambiguous, mismatched and later-game
bases/references are rejected. Creature runtime Marksman still aliases Combat,
while its integer base query remains Stealth. Player integer base queries expose
floored derived values, including base Encumbrance capacity.

World load prepares a separate native service and validates its content bindings
before changing runtime serials, globals, inventories or projected player stats.
The prepared maps and action ledger replace the existing service with a verified
nonthrowing move after the other restore work. Existing file fingerprint checks
still reject missing/changed content before application. This is native-service
preflight, not yet a fully staged transaction for all legacy/AI/script state.

`S3/unloaded-actors-01` passes **577 engine tests**, rebuilding all three engine
binaries. The new no-World fixture exercises unloaded reads and rejection while
preserving prior values/action ownership. Actual NPC/creature fixtures check
content-aware restore, different base/current rounding and creature skill aliases.
No component rules or save encoding changed; their preceding passing evidence
remains applicable.

`S3/unloaded-actors-reject-01` passes a real loader negative control using the
editable `oblivion_m15_native_actor_binding_fault.json` and
`oblivion_m15_reject_native_actor_binding.json` sources. The copied save is
structurally decodable but falsely assigns native actor state to the static
fixture object. The loader reports the exact invalid-base error before the
runtime-state applied boundary; no save is written, and source/mutated input
hashes remain unchanged. Independent checks are in
`binding-rejection-verification.json`. The inspected screenshot shows the
specific rejection dialog. `S3/unloaded-actors-restart-01` passes the unchanged
valid save/quit/fresh-load/resave course with full live/binary agreement; its
inspected image shows the textured fixture/HUD. Runtime executable SHA-256:
`3900cf21985f6797dcf63739ab15e0d7c98cd34f322d0b245edc2e1029976bc3`.
These silent tests do not establish automatic actor activation or combat.

### S3 native fatigue regeneration writer

Commit `0355d154d8` contains unloaded reads/content-aware restore preflight.
Explicitly published native player/NPC/creature fatigue now regenerates through
its owned Damage channel and republishes shared views atomically. The ordinary
actor restoration update recognizes a native projection and dispatches through
the World-owned service before any TES3 maximum check or write. A missing owner
is an error. Dead actors do not regenerate. Unpublished shared actors retain
existing behavior until the remaining activation/migration work is complete.

The request uses current integer Endurance, native winning fatigue settings,
current/base flooring and the eligible Maximum modifier. The mutation preserves
sparse presence and does not impose a displayed-maximum clamp. Combined original
updater/wrapper execution passes 2,700 cases; provenance records the fixed player
Damage field, Low/Middle/High process dispatch and explicit oracle stubs. It
corrects the earlier provisional wording about a final maximum clamp.

Actual Player/NPC tests cover process-dependent maxima, fractional Endurance,
negative fatigue, zero duration, negative rates, absent/zero/negative Damage
entries, overshoot, unchanged Maximum/Script channels and invalid-input rollback.
The restored actual-creature fixture regenerates its persisted Damage channel.
The native store adapter ignores TES3 aliases, uses current winning overrides
and rejects wrong setting types. The adapter currently resolves winning GMSTs
on each call; automatic-activation performance remains an open gate.

`S3/fatigue-authority-01` passes **579 engine tests** with exact inventory
agreement and no failures/skips, rebuilding openmw/openmw-tests/esmtool.
Tested source fingerprint:
`2928e9cd16a44a3398bb7189aeb69a1123d03d8107cea706408d50a441c9c07c`.
The first preflight found an ambiguous optional-array test initializer; explicit
`std::nullopt` entries fixed it. Both build logs and the passing 15-test focused
preflight remain in `S3/fatigue-authority-preflight*`. The broad preflight rebuild
also reports GCC warnings in existing sol3 getter/AnimationQueueEntry code.
No component arithmetic or save encoding changed; prior component/sanitizer/
Python evidence is not relabeled as coverage of these engine methods.

Automatic publication, other native writers,
legacy migration/reconciliation, negative-fatigue recovery and active gameplay
acceptance remain open. Next bounded work: continue the movement/rest/script
writer audit before activating native shared views during normal gameplay.

### S2 movement expenditure rules for the remaining writers

Commit `5ddad32b43` contains native regeneration routing. Running/jumping now
have typed native setting builders and immutable debit rules that use exact
integer encumbrance, float-Strength capacity, base-skill mastery, native float
stores and the expenditure wrapper's zero-fatigue limit. Zero-capacity NaN/Inf
intermediates follow the original branch behavior without entering actor state.
Installed jump settings are 30/0, overriding compiled 4/4; Expert/Master use
the native .5 multiplier. These rules do not yet activate the movement writers.

Independent original execution passes 38,880 run/jump cases matching C++ bits,
396 expenditure-wrapper cases, and two exact-int weight boundary cases. The
initial focused test caught an incorrect halfway rounding expectation; its
replacement distinguishes exact integer division from premature float rounding
and passes both original x87 modes. Failed and corrected preflight logs remain
in `S3/movement-fatigue-preflight*`. `S3/movement-fatigue-rules-01` passes **1,887 component /365 ASan+UBSan ESM4
/160 Python tests**, with exact C++ inventory agreement and no skips/failures.
Tested source fingerprint:
`aad8bef45ebc4747fced79e550464efd401d20251c7bea8fb3c6d81d5ae0bbb9`.
Sanitizer coverage is component rules, not the engine, and leak checks remain
disabled. Next bounded work is the accepted-jump physics boundary and running
writer, including actor expenditure eligibility and shared view publication.

### S3 running/regeneration transaction and accepted player jump

Commit `1a3f809a94` contains the verified movement arithmetic. The service now
prepares running expenditure followed by regeneration as one candidate, then
publishes authority and shared views once. The actor update supplies the
controller's effective running state; CharacterController bypasses its TES3
running/swimming/sneaking fatigue writer for native projections. Regeneration
continues when expenditure is suppressed. Dead actors are unchanged.

The physics accepted-jump boundary dispatches published player fatigue through
the World-owned service. Original call-site inspection identifies the rate
consumer in PlayerCharacter's +228 update, so native nonplayer jumps bypass
the TES3 debit without receiving a player-only cost. Player god mode suppresses
expenditure; the common running route uses true eligibility for Character and
the independently verified initial false Creature flag. Mutable creature-flag
lifecycle remains open, and automatic publication is still disabled.

The actual Player/NPC fixtures test expenditure-before-regeneration, base mastery
with a conflicting modified skill, zero duration, expenditure suppression,
rollback after a valid debit followed by invalid regeneration, immediate shared
views, and mutation after binary capture/restore. Player jump tests cover
Expert reduction, limiting to zero, repeated zero-fatigue requests, and a dead
actual Player control. `S3/movement-authority-01` initially passed 580 engine
tests. A subsequent original negative-zero probe exposed a capacity-sign edge
case omitted from the earlier matrix; the preserved failure led to a correction
and expanded 62,208-case bit-exact comparison.
`S3/movement-authority-02` passes **1,887 component /365 ASan+UBSan ESM4 /580
engine tests**, rebuilding all three binaries, with exact inventory agreement
and no skips/failures. Tested source fingerprint:
`3c6c631630913139cbff337e22bfe59c5758f97514c6aea443d40a39bb550670`.
Sanitizers cover components only, with leak checks disabled. The unchanged
Python layer retains its preceding 160-test pass.
Winning settings are resolved
at the World boundary; per-update GMST resolution performance, remaining
rest/script/magic writers, activation and full runtime acceptance remain open.


`S3/movement-authority-jump-01` passes the unchanged M12 keyboard/gamepad
course. Both input sources reach physics and debit shared fatigue (145 -> 115
and 136.392 -> 106.392). The inspected capture shows the textured Imperial
Dungeon and HUD. This silent course exercises the unpublished legacy bridge,
not automatically activated native authority or jump-height/audio acceptance.
Its pre-signed-zero-fix executable SHA-256 is
`08c637b46ded3ebbc5bb168e9dc87eb0f249769fb5fea045c32b378685c578a0`.

`S3/movement-authority-jump-02` repeats the unchanged keyboard/gamepad course
successfully on the corrected executable:
`4e17fd436cc4f535fce0fd1dc34da4e8251de86250582619b8e914fa650cceff`.
Both input sources reach the physics jump boundary, with no forbidden or
unreviewed errors. The inspected screenshot again shows the dungeon/HUD.
This remains unpublished-bridge regression coverage; native activation and the
full normal-input combat/movement campaigns are still open.

### S3 verified Health and Magicka restoration requests

Commit `947056fbf0` contains the running/jump authority integration. The next
chunk adds native Health-gap restoration and Magicka regeneration rules,
including typed winning GMST inputs. Original instruction execution supplies
37,008 exact C++ float-bit comparisons; 32 additional hourly-dispatch slice
cases establish restoration order, 3,600-second arguments and the suppression
flag used by jail. These are independent arithmetic/branch oracles, not live
rest/jail acceptance. Details and boundary stubs are in the provenance report.

Four new component tests cover rounded maximum versus current, exact integer
base addition, Stunted Magicka sign, active-item suppression/override, request
amounts exceeding the remaining deficit, typed overrides and invalid input/
overflow rejection. The first preflight intentionally fails because the new
APIs do not exist; the subsequent implementation passes all focused tests.
Logs remain in `S3/authority-draft/restoration-preflight-01.log`,
`restoration-preflight-02.log` and `restoration-tests-01.log`.
`S3/restoration-rules-01` passes **1,891 component /369 ASan+UBSan ESM4 /160
Python tests**, no skips/failures and exact C++ inventory agreement. Tested
source fingerprint:
`031bbb4b33ee5b437ec22df0901914a9533417ce5da4f80e75a25b49348cb9b1`.
Sanitizers cover components only, with leak checks disabled. Authority
transactions, active-item/effect integration, live rest dispatch, automatic
publication and normal-input acceptance remain open. Next is an atomic
Health/Magicka/Fatigue restoration transaction preserving raw Damage channels.

### S3 atomic resource restoration authority

Commit `e1f2864bd5` contains the verified Health/Magicka request rules. The
service now prepares Health (when requested), Magicka and Fatigue restoration
in original order on a candidate, then publishes authority and all shared
views once. Any later invalid setting/input discards earlier prepared changes.
It preserves Maximum/Script channels, nonplayer sparse Damage presence,
Player fixed Damage clamping, process Maximum eligibility and NPC outer
Magicka scaling. Dead actors are unchanged. Callers still own effect advancement,
active-item state and ordinary-rest versus jail eligibility; this API alone
does not activate the World rest path.

Actual Player/NPC tests cover high/low process, active-item and integer Stunted
Magicka gates, zero duration, optional Health restoration, invalid final Fatigue
rollback, absent versus stored-zero Damage, and continued mutation after binary
capture/restore. A reconstructed Creature also restores all three resources
without changing its runtime skill aliases. Independent original helper plus
modifier-wrapper/storage execution passes 3,600 cases, complementing the
preceding request-rule and fatigue-authority oracles.

The unimplemented baseline fails the new API assertions at compilation.
The first implemented fixture crashed before service execution because the
synthetic NPC was still player-level-scaled without a World. GDB identified
`resolveOblivionActorConstructionStats`; the fixed-level fixture correction
passes, and the testing skill now records that concrete setup pitfall. Failed
preflight/test/backtrace logs remain in `S3/authority-draft/` with prefix
`restoration-authority-`; corrected focused tests pass. The final
`S3/restoration-authority-01` rebuilds **openmw/openmw-tests/esmtool** and passes
**581 engine tests**, no failures/skips and exact inventory agreement. Tested
source fingerprint:
`e65e9157428af2159f9aafbdd427e2f5094dcc62eba82b84344fa0252916097f`.
The unchanged component rules retain the 1,891 component /369 sanitizer /160
Python pass from `S3/restoration-rules-01`; there is no engine sanitizer claim.
Remaining live stat writers, shared-base ownership, legacy reconciliation,
automatic publication and rest/gameplay acceptance are still open.

### S3 typed shared-base SetAV preparation

Commit `3d1a9f3396` contains the resource-restoration authority transaction.
The next chunk prepares native SetAV base changes with original byte/word
wrapping, exact signed Health integers, extra-AV float storage, Creature skill
aliases and no-base-write cases. It deliberately preserves a typed value;
converting every requested base to float would lose Health integer precision.
Shared base ownership, process cache writes and script routing are subsequent
integration work, not implemented by this preparation helper.

Independent execution covers 7,488 direct base-form paths and 26,208
Player/NPC/Creature runtime-to-base paths, including Low/High/no-process
nonplayer dispatch. All 26,208 typed C++ results match integer values or float
bits exactly. The initial original harness's incorrect AV11 storage expectation
is preserved; jump-table decoding and rerun establish the no-base-write branch.
The new tests cover all 72 IDs, storage boundaries, aliases and rejected kinds/
IDs. `base-setter-preflight-01.log` retains the unimplemented-API compile failure;
`base-setter-preflight-02.log` and `base-setter-tests-01.log` pass.
`S3/base-setter-rules-01` passes **1,893 component /371 ASan+UBSan ESM4 tests**,
no skips/failures and exact inventory agreement. Tested source fingerprint:
`b035957421dea29f304011b3d07d8566b871808a427bfe62feb5b0f2ca0d78f4`.
Leak checks are disabled; this is component sanitizer coverage. Existing
RuntimeInventoryItem fixture initializer warnings remain unchanged. Next is
versioned shared-base override state and authoritative propagation to actors
using that base, retaining distinctions between base-float/floored queries and
raw form integer queries. Automatic publication and the remaining M15 stages
remain open.

### S3 version-11 shared base override ownership and persistence

Commit `3192aa2065` contains the verified SetAV preparation rules. Schema 11
now appends canonical shared-base overrides, keyed by stable base identity and
native NPC/Creature kind. Each entry retains its AV key and integer/float tag;
Health 16,777,217 remains an exact int32 while extra-AV float 16,777,216 remains
a float. Entries validate storage widths, Creature group keys, finite values,
unique bases/AVs, bounded counts and canonical identities. Versions 1–10 migrate
with no invented overrides. C++ binary/JSON and Python readers/writers agree.

The profile-owned service captures, restores and clears the base map with actor
values and action ownership. Content-aware restore checks winning native kind,
missing/ambiguous bases and the projected player-base alias before committing.
It prepares all maps before publication, and refuses an older-schema capture
without partially changing its target. The new engine tests first demonstrated
the previous dropped-map and missing-binding-check failures; the implementation
passes both. Live SetAV mutation/propagation and automatic activation remain
open; persistence alone does not update the legacy shared views.

Component tests cover exact tagged wire bytes, ordering, duplicate/invalid
storage, type/kind/ID corruption, NaN, truncation and migration from all ten
older schemas. Python's first upgraded fixture omitted its required AI RNG;
that failure is retained and corrected before the full run. Evidence under
`S3/authority-draft/` uses `shared-base-*` prefixes, including the missing-API
compile failure, engine behavior failures and corrected focused passes.
`S3/shared-base-state-01` passes **1,895 component /373 ASan+UBSan ESM4 /162
Python /583 engine tests**, with exact C++ inventory agreement and no skips or
failures. All three engine binaries rebuild. Tested source fingerprint:
`63d0c37be169539e2fc8def1470b2223f4c5bf4fc5453a89e43c709cf480ac70`.
Sanitizers cover components only, with leak checks disabled.

Runtime evidence on executable SHA-256
`1faf6a6d2d3ec36f685d637cc6c293cbe79a8b4fd53e9345395244fd2b9e3363`:

- `S3/shared-base-state-restart-01`: real save/quit/fresh load/resave passes,
  with schema-11 empty native actor/base vectors and matching live/binary state.
- `S3/shared-base-state-legacy-01`: copied schema-10 input loads/resaves as 11;
  native actor entries and action ledger are preserved, overrides remain empty,
  and source/pristine hashes are unchanged.
- `S3/shared-base-state-populated-01`: injected typed overrides survive actual
  engine load/resave exactly. The editable roundtrip input is committed. This
  establishes storage ownership, not normal-input SetAV or gameplay activation.
- `S3/shared-base-state-reject-01`: a structurally valid override naming the
  fixture's static base fails native content binding before the Applied boundary,
  with no save write and unchanged source/pristine/mutated-input hashes. Editable
  fault input and scenario are committed.
- `S3/shared-base-state-morrowind-01`: unchanged M14 smoke manifest passes its
  no-native-record save assertion. It is profile isolation, not TES3 combat/crime
  acceptance.

Inspected restart/migration/populated captures show the textured observation
room and HUD. The rejection capture shows the expected invalid-base dialog.
The Morrowind capture remains terrain-facing with sky and TES3 HUD. All these
courses are silent and establish no audio acceptance. The build/test skill now
records schema-11 migration, populated persistence and rejection reproduction.
Next is authoritative shared-base writes and propagation to every affected
loaded/unloaded actor, with raw integer query handling and preserved modifiers.

### S3 shared-base authority transactions and exact Health queries

The service now prepares a typed shared-base change, all affected saved actor
snapshots, and every supplied resident view before committing. It retains no
actor pointers between calls. The caller must supply the complete resident set
for that base, including the target; duplicates, wrong bindings and omitted
targets fail without publication. Unloaded snapshots change in the same
transaction, and future actor publication reapplies saved overrides. Creature
runtime Marksman writes Combat while its base query remains Stealth; all seven
base skills in a written group change without altering modifier ownership.
Player writes retain raw form contributions and recompute derived values from
winning settings. Shared writes preserve action ownership and source records.
Restore rejects conflicting actor/base snapshots before replacing any service
state. The World resident enumeration and script/event/cache adapters remain
next work; this is not yet a normal-input SetAV acceptance claim.

Original-instruction checks on the previously hash-identified 1.2.0416 image:

- `S2/oracle-emulator/base-health-query.py`: 144 complete current-integer/base
  query paths across Low/Middle/High/no-process, signed precision boundaries,
  and both x87 precision settings. Health int32 16,777,217 stays exact in a
  processed current-integer query; GetBaseAV/no-process instead sees 16,777,216.
  Actual NPC form getter, TESHealthForm virtual storage accessor, conversion and
  process dispatch execute. Actor identity/type and zero sparse modifiers are
  boundary stubs. Initial missing embedded Health vtable setup is retained.
- `base-health-float.py`: 360 full current-float paths. Form virtual 51E790
  returns FILD without a float store; Script and Damage precede the first
  rounding in 6433E0, and active Maximum follows it. All 270 processed cases
  match the C++ exact-integer composition helper bit for bit; the remaining
  90 no-process cases characterize the original fallback. This caught and fixed
  premature base rounding after SetAV. Original sparse lookups are supplied
  independent modifier values; form and process composition execute.
- `base-extra-query.py`: 576 original extra-AV queries and exact C++ matches
  demonstrate truncation of stored float values before actor composition.
  Nonfinite/out-of-int32 float conversions remain explicitly unsupported by
  live publication. A retained out-of-domain experiment showed that the
  original non-SSE CRT path uses a 64-bit conversion and cannot be replaced by
  an assumed SSE int32 sentinel. CPU-path compatibility remains an open gate.

Engine tests exercise multiple resident NPCs, a genuinely destroyed/unloaded
reference, future reference publication, untouched content records, sparse
modifiers, byte/word wrapping, raw Health integer/float divergence, fractional
extra storage, actual Player derived values, all Creature groups, binary reload,
conflicting snapshots, invalid resident lists and an overflow in the final
resident. Failed preparation leaves actor/base maps, resource/skill views and
pending actions unchanged. Initial private-member helper compilation and a
save fixture missing its reference records are retained under
`S3/authority-draft/shared-base-writer-*`; both are corrected.

`S3/shared-base-writer-01` passes **1,897 component /375 ASan+UBSan ESM4 /585
engine tests**, with exact inventories and no failures/skips, and rebuilds
openmw/openmw-tests/esmtool. Tested dirty-source fingerprint:
`9979b87b7e1350ff8c17cd186b8b247150e74f086889043caa2f1d2437c2e6e7`.
Sanitizer coverage is components only, with leak checking disabled. No Python
implementation changed in this chunk; the prior 162-case result remains its
last full suite run.

Runtime binary SHA-256:
`3cca5f8243de3eb834c62c10b100a875628c34efecb2f99636689fec8aa6fb88`.
`S3/shared-base-writer-populated-01` loads/resaves matching injected actor/base
snapshots, preserving both vectors exactly. `S3/shared-base-writer-conflict-01`
rejects a structurally valid player raw-form/base conflict before Applied or
save-writing boundaries. Both preserve source/pristine hashes; rejection also
preserves the mutated input slot. Their editable inputs and rejection manifest
are committed. Independent verification files record decoded equality, hashes
and inspection: the positive capture is the textured observation room with
crosshair/resource HUD; the negative capture is the readable expected conflict
dialog. These silent courses test persistence/preflight, not automatic native
publication, normal-input commands or audio. Automatic activation stays off;
script routing, remaining writers and the S3/S4–S14 gameplay gates remain open.

## S3 native script and condition queries

Registered native actors now answer GetAV/GetActorValue and GetBaseAV/
GetBaseActorValue through the service, including both AI condition adapters.
The canonical 72-name resolver is ASCII case-insensitive and rejects unsupported
aliases. Invalid native script queries yield OBSV115; conditions return typed
unsupported. Actors without registered native state still use the compatibility
bridge. Queries do not load cells. Disabled references use the independently
verified raw form path; GetBaseAV retains its distinct floored resolved base.
The initial interpretation of the disabled bit as death was corrected before
commit and runtime acceptance (see provenance); no death custom-state marker
participates in this adapter.

`S3/native-script-query-01` passes **1,898 components /376 ASan+UBSan ESM4**,
with exact inventories and no skips. Its engine result was superseded by the
flag interpretation correction; unchanged component sources retain those checks.
`S3/native-script-query-02` builds the corrected engine and passes **586 engine
tests**, no skips, fingerprint
`a58b2c9f57162f0277e0f3326a047658079eacd6af406cb23db48b59cf7282bc`.
Engine SHA-256:
`ae1c48188a6524bb1e37eed833bbb49c78bb6c47d4223181a99c4ad32e40d016`.
The service test covers enabled/base/disabled values, exact raw Health, Creature
aliases, Player derived versus raw inputs, invalid queries and read immutability.
Independent original-instruction coverage adds 96 queries, eight disabled flag
cases and 438 canonical name comparisons. The first missing-include build and
initial oracle attempts remain under `S3/authority-draft/native-query-*`.

Real engine `S3/native-script-query-runtime-01` returns all eight expected
fractional/current/base results and resaves identical native vectors.
`native-script-query-continuation-01` repeats this from that save in a fresh
process. `native-script-query-negative-01` completes all actions but the harness
exits 1 solely because the deliberately wrong 96.5 result is absent (actual
95.5). Each verification checks source/pristine hashes and resaved state.
All three captures were inspected: rendered observation room, HUD/crosshair,
no error dialog. These silent cases inject unpublished native authority;
they are not normal-input stat construction, publication, death or combat
acceptance. Script/console writers, process cache semantics and automatic
activation remain open; S2/S3 continue in progress, S4–S14 pending.

## S3 typed native command writer transactions

The service now accepts typed Set/Mod/Force commands and explicit script/console
origin. Set delegates to the shared-base transaction; Mod/Force prepare the
native modifier delta, apply god-mode/nonplayer fatigue eligibility, and publish
through existing atomic actor/view writers. Force queries the live current
float path independently of reference enablement. Suppressed writes return
without publication or callback requests. Successful writes return any negative
Health reaction delta only after the transaction commits; world notifications,
death/essential transitions and console/text adapters remain unwired.

Three real class tests cover Player/NPC/Creature base versus modifier ownership,
console Damage versus Script, god-mode bypass for Set, suppressed fatigue,
scaled NPC Magicka Force behavior, Creature Marksman aliases, sparse positive
Damage insertion, invalid-command/settings rollback, and binary reload of a
Creature command sequence. Independent original wrapper comparison covers
**6,272 cases** including both CPU conversion paths and integer overflow order.
A follow-up original Health callback probe covers 1,024 gates (Health below 1,
actor state query, caller forwarding); it does not execute the death transition.
The initial NPC test had an invalid absolute level and the Creature save fixture
omitted its reference state. Both strict production rejections were retained and
the fixtures corrected; `S3/authority-draft/command-writer-tests-{01,02,03}.log`
records the failures and passing focused run. The preceding query adapter's
shadowed local name was also cleaned up without changing behavior.

`S3/native-command-writers-01` passes **1,899 components /377 ASan+UBSan ESM4 /
589 engine tests**, exact inventories and no skips. Tested source fingerprint:
`d4d82da58b316f5c81223a54e60b048502986b1fb2faefeddf1fa6791f9aed09`.
Engine SHA-256:
`32bec6e773bf9c0c1aab92d1fca39993a29d3a13688a8af4aa0efbe99caa7f6d`.
This chunk exposes service transactions, not automatic script/console writer
activation or normal-input command acceptance. The next integration boundary
is world callbacks and actor lifecycle; cache-only values and native activation
also remain open. S2/S3 are in progress and S4–S14 remain pending.

## S3 lifecycle storage and death-event queue contract (schema 12)

Schema12 adds stable actor/base lifecycle entries with logical Alive, Dead and
EssentialUnconscious phases, fractional recovery countdown and attribution,
plus a separate monotonic death-event namespace and FIFO queue. Binary,
canonical JSON and Python agree. Validation rejects invalid phases/timers,
unknown sources, duplicate/dangling actors, actor-value/base disagreements,
noncanonical wire keys, zero/duplicate/out-of-order/out-of-range event IDs and
version loss. Coexisting legacy `obscript.dead` must be a bool matching terminal
dead status; essential unconsciousness is not GetDead. Older schemas decode to
empty lifecycle state and event namespace1 without inferring death from Health.
Legacy-only markers remain intact: automatic conversion and live activation
are intentionally not introduced before their writer/view adapters.

The service owns these maps and queue through clear/capture/restore. Content
preflight validates winning actor/base and attribution bindings before replacing
any action/value/lifecycle state. `takeNextDeathEvent` removes the front event
before the caller dispatches it. The service test captures at that boundary,
serializes/reloads, and consumes only the remaining event; exhausted IDs are
retained. This establishes the queue/save boundary, not actual script dispatch
from a live death. Live lifecycle projection, transitions, legacy marker
conversion, callbacks, corpse/essential behavior and command routing remain
next; no normal-input death acceptance is claimed.

`S3/native-lifecycle-state-01` passes **1,902 components /380 ASan+UBSan ESM4 /
165 Python /591 engine tests**, exact C++ inventories and no skips. Fingerprint:
`3002823d201d1b9e1a682f330fa3a7643a5b8f53c132a199b3e46ee2bd89d7bb`.
Engine SHA-256:
`72bd156808201a698b3f530b576ec70798e6c7efb7440ac22c324c5f08957318`.
The initial Python fixture lacked required reference owner metadata; it was
corrected. A pre-existing base wire test assumed the current schema ended at
base overrides; its explicit v11 offsets are now pinned to version11, with new
v12 combined base/value/lifecycle coverage. Failures and corrected preflights
are retained under `S3/authority-draft/lifecycle-*`.

Actual engine evidence (all under `S3/`):

- `native-lifecycle-populated-01` and `native-lifecycle-continuation-01` preserve
  injected lifecycle fields and the two ordered pending events through separate
  load/resave processes. No consumer dispatch or live publication is claimed.
- `native-lifecycle-legacy-01` upgrades the accepted v11 query continuation to
  v12, preserving native values/bases and adding empty lifecycle/event defaults.
- `native-lifecycle-reject-01` rejects a structurally valid static-object actor
  binding before Applied/save boundaries; pristine and private input hashes
  remain unchanged.
- `native-lifecycle-restart-01` passes the normal idle-service new-save -> quit
  -> fresh-load -> resave course, including v12 snapshot/default checks.
- `native-lifecycle-morrowind-01` saves without GPRO/T4VR/T4ST/OMW4STATE markers.
  Its terrain-facing Balmora spawn is the existing short isolation case, not a
  town gameplay regression suite.

All seven required captures were inspected: observation rooms/HUDs, readable
binding-error dialog and the terrain-facing Morrowind scene. Runs are silent.
Verification JSON records input preservation, state vectors and binary identity.
S2/S3 remain in progress, S4–S14 pending.

## S3 live lifecycle projection

The native actor projection now publishes Alive, Dead and EssentialUnconscious
into shared CreatureStats alongside the prepared native values. Negative Health
alone does not imply death or get clamped. Essential actors remain nondead and
knocked down even when a controller clears its ordinary knockdown flag. Leaving
essential unconsciousness clears its knockdown bookkeeping; an unchanged Alive
publication preserves ordinary knockdown. Repeated dead publication preserves a
completed death animation. Native resurrection requires the native authority;
unprojected TES3 stats retain their legacy behavior.

Construction/load publication validates lifecycle identity and fields before
committing authority or views and never creates death events. Value and shared
base transactions carry the owned lifecycle projection. Tests serialize native
state and restore it into fresh actual NPC, Player and Creature instances,
including fractional essential recovery time. Shared save fields agree with
dead and essential projections. Automatic activation, health transitions,
world/script routing and actual corpse/recovery acceptance remain open.

`S3/native-life-projection-05` passes all **592 engine tests**, matching inventory
and with no skips. Source fingerprint:
`be1a21b4357cadb1070ff6e9db0fc56a81b8cbd381f0fbeb73f50d1780c5ce15`.
Engine SHA-256:
`8b0595b003e7c755cf2c6291cd6806ac6f3768c97b49896dce45111fe3fb3563`.
Components and sanitizer code are unchanged from the preceding lifecycle-state
check. Attempts 01–04 retain fixture failures: a legacy death setter requiring a
world clock, uninitialized spell lists in standalone stats save/read, and a v9
fixture retaining newly added v12 lifecycle data. Corrected tests initialize the
real dependencies and construct a valid old-format case; validators are intact.

## S3 lifecycle/event transaction boundary

Explicit player and nonplayer lifecycle transitions now require initialized,
matching authority. A phase change publishes shared views and appends its death
event as one transaction, with all throwing preparation before publication.
Same-phase requests preserve the original attribution and recovery timer.
Essential transitions issue no death event. Reviving does not discard already
pending callbacks; dying again issues a new monotonic ID. Namespace exhaustion
rejects the change before either views or authority change. Eligibility, health,
source binding, physical realization and actual event dispatch remain callers'
responsibility; this does not implement script Kill or normal combat yet.

`S3/native-life-transactions-01` passes **593 engine tests**, exact inventory,
zero failures/skips. Fingerprint:
`8df3cb06d8589e8f690406707b738dc4e8d20a79fd5c356499330690756754a4`.
The new test consumes a death event, simulates callback-induced revival/death,
saves and restarts, then consumes only the successor. It also checks essential
entry without an event, same-phase idempotence and exhausted-ID rollback.

An independent **3,840-case** original-instruction probe of `006005F0` confirms
its guarded essential entry, including original state setter `005E6680`:
already dead/essential states 1/2/6 return; enabled essential actors in other
ordinary states set the timer, enter state6, then request a Damage Health
adjustment `R(R(R(baseHealth)*fraction)-currentHealth)`. States3/5 request that
adjustment without resetting timer/state; their gameplay meanings are not yet
established. The probe captures the Damage writer rather than applying it,
stubs cleanup/notifications and process/settings getters, and stops before UI,
tail bookkeeping or terminal-death branches. No full physical transition is
claimed. Hash identity is the original executable recorded above. Ignored
script/corpus: `S2/oracle-emulator/essential-entry-transition.py` and its table;
log: `S3/authority-draft/essential-entry-transition-01.log`.

## S3 negative-Health callback integration

Initialized Player and nonplayer authority now expose Health reaction entries.
Only Alive actors with current Health strictly below 1 transition. Ordinary
death retains the actual Health value and commits one attributed event through
the existing lifecycle transaction. Essential entry prepares the native Damage
adjustment, fractional recovery timer and unconscious phase before committing
values, authority and shared views together. Invalid settings leave them intact.
Repeated reactions on dead or essential actors preserve their existing cause and
timer. The caller still resolves essential eligibility/source binding and invokes
the callback after a negative Health writer; world routing is not yet active.

Recovery uses the native Damage writer rather than assigning its target to
current Health. Tests demonstrate the Player cap preserving a Script-caused
deficit and the nonplayer absent sparse Damage entry accepting a positive initial
value. Negative signed base Health is now accepted by the recovery formula,
matching the original SetAV-reachable domain. The one-float-step-below-20 result
for base -100/current -50 is asserted exactly, not rounded to an ideal decimal.

Expanded original `006005F0` probe: **7,680** branch cases and **1,200** exact
float-bit comparisons of captured Health adjustments against production C++.
Same boundaries as the preceding entry: process/settings getters and cleanup
are fixtures, Damage storage is captured, UI/tail/terminal physics are outside
the probe. Ignored corpus and comparison live in
`S2/oracle-emulator/essential-entry-transition-table.json` and
`essential-entry-comparison.json`; logs `S3/authority-draft/essential-entry-transition-03.log`.

`S3/native-health-reactions-02` passes **1,902 component /380 ASan+UBSan /593
engine tests**, fingerprint
`92a4ffd737dcfc1780e1a0b17a60c612fbb3ad42e067bbd4957d9f5268f6e026`.
Added channel-ownership cases pass the full **593 engine tests** in
`S3/native-health-reactions-04`, fingerprint
`6663d8cc3295dc8c5f1e969bcc9bd3d87efd831f2d4b367304f040d73f2cce49`.
Engine SHA-256:
`60b2c3cc1faa5f8117aba91a5192b6fcead84a33809868a58c3a82e3a2954b8f`.
Attempt01 retains the incorrect ideal-decimal expected value; attempt03 retains
a fixture expectation that overlooked its earlier shared base override. Original
instructions and the existing authority contract determined both corrections.

## S3 script GetDead reads lifecycle authority

`GetDead` now reads the profile-owned lifecycle entry first, normalizing the
player alias and without loading an actor/cell. Dead returns1; Alive and
EssentialUnconscious return0. Actors without typed lifecycle state retain the
legacy marker fallback during staged activation. GetDeadCount, Kill/Resurrect,
automatic initialization and callback dispatch remain separate pending adapters.

`S3/native-dead-query-01` passes all **593 engine tests**. Engine SHA-256:
`1a164f87dcc7e9516a89ebfa3a4fa65e4bf459562bea1d10820dead4d743f065`.
Actual engine cases `S3/native-life-query-{dead,essential}-01` and their
`-continuation-01` runs return the expected values and preserve native lifecycle,
queue and actor/base value fields across separate load/resave processes. Source
and injected pristine inputs are unchanged. `native-life-query-negative-01`
completes every action but exits1 solely for its deliberately wrong expected
GetDead1 against essential GetDead0. All five captures were inspected and show
the observation room/HUD without errors. These are silent injected unpublished
state query courses, not physical death/recovery acceptance.

## S3 direct scripted death entry

Native service Kill entry points share the validated lifecycle/essential
transaction but bypass the negative-Health callback's below-one gate. Tests
cover positive-Health death, positive-Health essential entry, repeated requests
and first-cause preservation. This is the service entry; script/world routing
and physical aftermath remain pending. It does not impose a zero-Health write
as an invented prerequisite for entering death.

The original Kill handler `00501960` calls `006005F0` directly with its optional
killer and zero float magnitude. Original Resurrect `00510150` invokes virtual
`+20C` with `(true, actorCell != nullptr, argument == 1)`; bool arguments must be
read from their low bytes because SETcc preserves the rest of the register.
The independent ignored `lifecycle-script-handlers.py` probe passes **160**
argument-extraction/cast/dispatch combinations. Extraction/casts and final
transition virtuals are boundary fixtures, not full resurrection execution.
Actual +20C targets are Player `00664A80`, Character/Creature `005F6020`.
The original GetDeadCount helper `004F5010` reads a separate signed16 counter via
`00440F70`, not a scan of current dead references; its counter integration is
still open. Do not extend the old marker scan as though it established parity.

`S3/native-script-death-entry-01` passes all **593 engine tests**, exact inventory,
zero failures/skips. Component formulas are unchanged from the preceding
component/sanitizer run. The handler corpus and log remain ignored under
`S2/oracle-emulator/lifecycle-script-handlers-table.json` and
`S3/authority-draft/lifecycle-script-handlers-01.log`.

## S3 script actor-value writer routing

Registered native actors now route SetAV/ModAV/ForceAV and their long aliases
through the native service. Unknown AV names and out-of-domain numeric inputs
produce OBSV115 instead of mutating a legacy field. Shared nonplayer base writes
collect registered resident references; Player writes use current winning native
GMSTs. Negative-Health callbacks use the native lifecycle transaction. Initial
lifecycle adoption prefers an explicit boolean legacy marker, otherwise retains
the shared saved death flag; it never infers death from current Health. A
successful writer removes the superseded marker. Typed lifecycle already
present remains authoritative. Automatic registration/publication on general
construction/load, unloaded writer adapters, full migration conflict courses,
console routing, Kill/Resurrect/GetDeadCount and queued callback delivery remain
open. This does not close S3 or enable every native gameplay writer.

Both `S3/native-script-writer-01` and final `-02` pass all **594 engine tests**.
Final fingerprint:
`d6398b96690c8ab5d7d8808aea238c206e7e63c1fb469ccb15fb9a1d2f49dd8e`.
Final engine SHA-256:
`edcab3cc23d81aea99b16e08a7bc5958b8bfaa8d43b2b95d0a51375ddc24ea1a`.
The winning essential GMST resolver has replacement/default/invalid-value tests.
Component and sanitizer code are unchanged from the Health-reaction checks.

Final actual engine courses `S3/native-script-writer-{runtime,continuation,
negative}-02` pass their independent verification. Long/short aliases publish
actual Player views and save Strength `[40,.5,20.5,-1]` and Health
`[87,10,5,-2]`, with the intended shared base overrides. A fresh-process
continuation sets base Health negative (current -7, base -20), observes GetDead0,
and restores its raw base contribution without erasing modifiers. Lifecycle
remains Alive and the event namespace remains unused. The negative control
completes every action and fails only for its deliberate expected61 versus
actual60. All input hashes remain intact and all three captures were inspected.
These are silent scheduled-command courses starting from injected registered
values; they demonstrate live writer publication, not normal actor activation,
physical death or essential recovery. Matching earlier `-01` runs were retained
before tightening the shared saved-death fallback.

`native-script-writer-morrowind-01` passes the existing idle Morrowind isolation
save course with no GPRO/T4VR/T4ST/OMW4STATE records. Its inspected terrain-facing
Balmora capture and binary identity are recorded in verification.json. It ran
before the final Oblivion-only adoption fallback change and is not a full TES3
combat/gameplay regression suite.

### S3 queued death-event delivery (partial)

The World script update now drains native death events in FIFO order, consuming
an event before invoking its OnDeath handler. A reentrancy guard prevents a
nested script update from draining the same queue; callback-created events join
the tail. Historical events remain deliverable after revival. Missing handlers
consume their events. This does not establish original callback ordering,
physical death, or successful execution of an authored callback body.

`S3/native-death-dispatch-02` passes all **594 engine tests**, with source
fingerprint `aec49b43e9c2a588a40132fac012c4cb61a101254c063730e0df78bb856cc503`
and engine SHA-256
`3deb312ef67d3a16d9c7b582d4b232baffc6d09c503c55fe9495cfd313ac4a35`.
The earlier `-01` also passed; `-02` removes a script-writer shadow warning.
Components and Python implementations are unchanged.

`S3/native-death-dispatch-runtime-01` delivers saved IDs 4 then 9 (both without
an attached handler), saves an empty queue with next ID 10, and preserves life,
actor values and base overrides. `native-death-dispatch-continuation-01` starts a
fresh process from that save and observes no replay. The negative course
`native-death-dispatch-negative-01` completes every action but fails solely for
its deliberately nonexistent expected ID 5. Independent verification confirms
all source/pristine hashes and saved vectors; all three silent observation-room
captures were inspected. Actual callback-body execution and saving from inside
OnDeath remain the next test, not a passed claim here. S3 remains open.

### S3 actual OnDeath callback-save boundary (partial)

A separate editable lifecycle recipe/builder adds one native NPC and one source
ObScript to the observation boot content. The original observation builder
remains script-free. The observer increments `deaths` in OnDeath and invokes
AutoSave only at count 1; it does not create a death or change actor values.
Generated licensed content remains ignored. Fixture SHA-256:
`8c47f944cfdd75bcd558e7ebf5ed74983a71c2aa4800a37cfea1506c4aba52b1`.
The actual engine compiles its one script without diagnostics. All **167 Python
tests** pass in `S3/native-callback-fixture-tests-01`, including dependency,
identity, transform and record/script-binding checks. Engine code/binary is
unchanged from native-death-dispatch-02.

`native-callback-initial-01` creates a fresh save with the resident NPC.
`native-callback-runtime-01` injects historical event IDs 4/9 for that Alive NPC,
then executes both real OnDeath bodies. The first callback's Autosave contains
count 1, pending ID 9 only, and next ID 10. The final Quicksave contains count 2
and an empty queue. `native-callback-continuation-03` starts a fresh process from
that callback Autosave, executes only ID 9, reaches count 2, and neither replays
ID 4 nor overwrites Autosave. Native values, base overrides and lifecycle are
unchanged. Input/pristine hashes match. The negative course completes all actions
and fails solely for absent expected ID 5. Independent assertions are retained
in `S3/authority-draft/native-callback-verify.py` and per-run verification.json.

Continuation attempts 01/02 are retained failures: the first expected the wrong
saved-game description, and the second assumed copying an Autosave under a
Quicksave filename made it an existing Quicksave slot. The corrected course
preserves the Autosave filename and expects both that input and the newly written
Quicksave. Every capture was inspected: silent room-wall/HUD views, NPC outside
the camera. This demonstrates callback persistence, not physical death, corpse
appearance, audio, native Kill routing or complete S3/S6 acceptance.

### S3 native resident script Kill adapter (partial)

Script Kill for registered resident actors now invokes the native lifecycle
transaction with its optional killer reference. The World adapter resolves
essential settings and adopts legacy lifecycle through the same helper as AV
writers. Repeat Kill does not duplicate an event or replace its first killer;
the script update drains the queue after the command. Unregistered actors still
use the previous migration path. Native-owned Resurrect now explicitly reports
OBSV116 until its original reset semantics are implemented, rather than changing
a legacy marker that disagrees with typed lifecycle. This guard is not completed
resurrection support. Automatic registration, unloaded commands, original death
side effects, GetDeadCount and S3/S6 completion remain open.

`S3/native-script-kill-02` passes **594 engine tests**, with source fingerprint
`6dd693f6fd609075e04eca34f84014269ea14ea0258c3cb90b1c67adbac6c998` and engine
SHA-256 `704c9fb6f3dbdf3eff597c3fcc5f9d18b0267a19d495174849c1757631d71f4e`.
Attempt 01 retains a missing-forward-declaration build failure. No component or
Python implementation changed in this chunk.

Actual engine courses `native-script-kill-runtime-02` and `-negative-02` start
with injected registered NPC values but no native lifecycle. Two Kill commands
produce one event ID 1, one actual observer callback, terminal life with Player
attribution, next ID 2, and unchanged current Health 100. No legacy death marker
is emitted. Callback Autosave and final Quicksave agree. The negative completes
all actions and fails only for absent ID 2. `native-script-kill-continuation-01`
restarts callback Autosave, observes GetDead1/Health100 and repeats Kill without
replay. `native-script-resurrect-guard-01` verifies the explicit error leaves the
life, killer, event namespace, local count and values intact. Pristine hashes
and complete native vectors are checked by
`S3/authority-draft/native-kill-verify.py`; every capture was inspected (silent
room-wall/HUD, no corpse-pose claim). Runtime/negative attempts 01 are retained:
the acceptance driver's string argument `player` resolved a base EditorID;
canonical runtime Player identity fixes the fixture. Compiled ObScript resolves
the Player builtin separately. The native handler's original direct entry at
positive Health is covered by the previously documented 160 original cases.

### Harness display ownership during concurrent courses

Concurrent legacy-counter and Morrowind courses exposed an Xvfb allocation race:
the harness tested socket existence before checking whether its own child had
failed. Another scenario's socket could therefore be mistaken for readiness,
sending input and screenshots to the wrong display. The affected
`S3/native-death-count-{legacy,morrowind}-01` runs are failed evidence, retained.
A search of all retained S3 xvfb.log files found the conflicting-server error
only in that pair's Morrowind log.

The harness now uses Xvfb's `-displayfd`: the server reserves its display and
reports readiness through a private inherited pipe. Invalid/absent replies and
timeouts clean up the child, including a forced kill when termination stalls.
Three new harness tests pass; the working candidate's complete **172 Python
tests** pass in `S3/xvfb-display-ownership-01` (fingerprint
`8e82eb0aec9e8512b299f8f535a6dc44cc5edf167bd8d1b3432defe9c234addb`,
including the separately pending counter tests). `S3/xvfb-concurrency-01`
starts eight real servers concurrently, verifies eight unique usable displays,
and confirms four remain usable after their peers terminate. Both affected
actual engine courses pass on independent displays as `-02`, with their own
correct profile captures inspected. This is test isolation, not a gameplay gate.

### S2/S3 native per-base death counters (partial)

Original executable `a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`
stores a separate base-keyed 16-bit counter. `440FA0` adds the low 16 bits to an
existing entry, or inserts that value into a new entry; `440F70` reads it.
`4F5010` sign-extends the result before writing the script double. Terminal
entry calls the updater with +1 at `600C95`. Player/Character/Creature virtual
+170 all resolve `4D9B40` (base at actor+1C), and +190 resolves `977C50` (true),
so the counter key there is the actor base. The other direct updater caller,
`4413BA`, is in the saved-counter restoration loop. This is not a count of
currently dead resident references.

Ignored original-code probes `S2/oracle-emulator/dead-count.py` and
`dead-count-insert.py` execute all **65,536 existing-entry increments**, **131,074
signed/null/missing queries**, and **32 new-entry/list-insertion cases**. Both
x87 control words are covered. Allocation in the insertion probe supplies fresh
8-byte mapped objects; actual native list insertion, update and lookup execute.
Console output is disabled. These probes establish counter storage/query rules,
not every terminal-death or resurrection side effect.

Runtime schema **13** appends canonical base-keyed uint16 counters in C++, JSON
and Python. Duplicates, null/noncanonical identities, truncation, excessive
collections, invalid JSON widths and old-version writes with new fields fail.
Content preflight requires actual winning NPC/creature bases, even when no
resident reference exists. Versions 1–12 start with empty counters: historical
deaths cannot be reconstructed from their current life flags. New terminal
transitions prepare counter, lifecycle and event changes together before shared
view publication; revival, essential entry, repeat Kill and event consumption
do not decrement/increment the counter. Namespace exhaustion rolls everything
back. Clear and content-failure restore checks preserve their existing atomicity.
GetDeadCount now queries this authority by base rather than scanning markers.
Automatic actor registration and remaining script argument/Player-base aliases
are still open; unregistered legacy marker deaths cannot populate this history.

Checks: `S3/native-death-counts-01` passes **1,905 components** and **383
ASan/UBSan** tests. Attempt 02 passes **169 Python** tests after adding the
required AI RNG seed to new fixtures. Attempt 03 passes all **595 engine** tests
after setting the test NPC's TES4 flag. Failed attempts remain intact; production
component/sanitizer code was unchanged by those fixture fixes. The subsequent
harness-isolation check passes all **172 Python** tests. Final engine source
fingerprint: `17909f5b83cd3a2014f6a7a6c316eb1a01f8ff2b91126ce3c3975a50f11ebe35`.
Engine SHA-256: `7bb20e15e387dc89f63f57371aa0a8cae1d074999c31ce19dbeb1e470848e5b6`.

Real engine `S3/native-death-count-{runtime,negative,signed,wrap}-01` verifies
0→1, 32767→-32768 and -1→0 queries, unchanged AVs and one callback for duplicate
Kill. Callback Autosave and final Quicksave preserve exact uint16 storage.
Continuation-01 restarts the signed callback save without replay/increment.
Legacy-02 loads an actual v12 dead-actor save, defaults history to zero, and
resaves v13 without inventing a count. Binding-reject-01 refuses a counter bound
to the room STAT before runtime application. Morrowind-02 passes idle save
isolation with no GPRO/T4VR/T4ST/OMW4STATE. The negative completes every action
and fails only the deliberately wrong expected count 2. All pristine hashes and
saved vectors are independently checked in
`S3/authority-draft/native-death-count-verify.py`; all eight captures inspected.
Successful native views face the observation-room wall; rejection shows its
load-error dialog; Morrowind faces Balmora terrain. All are silent and do not
establish normal combat, corpse pose or full gameplay regression. S2/S3 remain
open and later gates are not closed by these checks.

### S2 essential recovery timer and wake gate (partial)

The original common actor update at `603E97..603F5D` only advances an essential
raw state 6 when process knocked-state is 1 or 3. Virtual +98 (`6439C0`) subtracts
the float at `B33E9C` from process+88 and stores one float; +9C (`629290`) reads
it. Recovery proceeds at <=0, setting actor state 0 through `5E6680`, then
issuing the same fraction-based Health Damage adjustment as essential entry.
High/MiddleHigh use `64B080` to sign-extend the process+11C knocked-state byte;
Low/MiddleLow supply zero. The xOBSE TimeInfo declaration identifies B33E9C as
frame seconds; the probe supplies that global and does not emulate its producer.
Original negative timer overshoot is retained by the pure tick rule. Persisted
lifecycle countdown clearing belongs to the later adapter.

`S2/oracle-emulator/essential-wake.py` executes **27,648 original cases** spanning
raw actor/knocked states, timers/deltas, base/current Health, fractions and both
x87 control words. Timer/getter/state-setter instructions execute; base/current/
GMST inputs and Damage application are boundary fixtures. The standalone C++
driver matches every remaining-time bit, recovery decision and captured Health
adjustment; driver SHA-256
`52434cea57d323718192b48974b451d08759d91c78f936591c2c5d70143e42da`.
Retained probe attempts 01/02 lacked actor-base virtual boundary stubs. Attempt
03 had an incorrect manual expectation: the temporary ESP shift at `603F2A`
means the fraction product overwrites the same stack slot subsequently read as
target. Attempt 04 executes and verifies the fraction-based result, rather than
mistaking that slot for untouched base Health.

`S2/essential-recovery-tick-01` passes **1,907 component** and **385 ASan/UBSan**
tests, with source fingerprint
`17d6892d5644271d403603120186f396d759da9ef9adc68e1242b06ac8fd3d4d`.
All 256 signed knocked-state bytes, frozen/nonessential paths, exact expiration,
overshoot, float rounding and invalid/nonrepresentable inputs are tested.
This is a pure rule. Service countdown publication, live controller mapping,
recovery Health callbacks (including reentry), God Mode, save continuation and
normal essential recovery remain open; no S3/S6 gameplay gate closes here.

### S3 essential recovery service and atomic Health reaction (partial)

Commit `d6381cf4e3` contains the independently checked timer rule. The service
now advances owned countdowns and prepares recovery Health/life/publication as
one transaction. It clears the previous killer on waking. A negative recovery
write below one Health reenters essential state, or commits terminal death with
one event/counter increment if the essential flag was removed. Invalid values
and exhausted event IDs preserve the countdown, AVs, life, queue and counter.
The wake ordering combines the original wake probe with the previously checked
negative-Health wrapper/common death gate; the wake probe itself captures the
Damage call rather than executing the complete nested writer.

Player essential entry and recovery now honor God Mode's negative Damage-write
suppression. Explicit Kill still enters the essential phase; God Mode does not
turn that command into a Health-gated action. World entry adapters supply the
actual God Mode state. Real-instance Player/NPC/Creature tests cover waiting,
raw knocked-state eligibility, changed settings, overshoot, recovery and
immediate reentry, positive sub-one healing, repeat Kill, sparse-entry removal,
untouched non-Health values, binary save/restore and fresh NPC publication.
These are engine-instance/service tests, not a normal-input recovery course.

`S3/native-essential-recovery-01` passes 597 engine tests. Adding the Creature
case exposed an incorrect test expectation in attempt 02: native nonplayer
zero Damage removes its sparse entry rather than retaining stored zero.
Attempts 03/04 pass all **598 engine tests**, with the final attempt also
removing new test-macro warnings. No existing case is skipped. Final source
fingerprint: `8521633e668708faa6030b93d216671ece1bd749c0d46e55ffd482811d6b97d7`.
Engine-test binary SHA-256:
`e312f6c6a79c51264e91aa04bad021b1c37defb59e1ff015ac26b3aaa39e7f44`.

Live controller tick/knocked-state mapping, physical essential collapse and
normal-input/fresh-process recovery remain open. No automatic registration or
new runtime timer call is enabled by this chunk. S2/S3 and later gameplay gates
remain open. Next bounded work is the original knocked-state/controller and
resurrection reset mapping needed by the lifecycle adapters.

### S2/S3 permanent nonplayer Magicka/Fatigue modifier slots

Commit `bcfffd5828` contains essential recovery service preparation. Further
resurrection tracing found a missing distinction in the scalar mutation adapter.
Native ActorValues constructor `65BE10` allocates permanent Magicka/Fatigue nodes
at +8/+C. Lookup `65C010` returns these even at zero. The zero-removal branches
of `65C9B0` write +0 into those nodes rather than freeing them. Consequently,
positive Damage never inserts a new positive modifier into either slot. The
previous sparse-add oracle stubbed lookup/removal and established only ordinary
sparse-entry arithmetic; extending its result to AV9/10 was incorrect.

The immutable mutation API now requires canonical actor-value identity. NPC/
Creature AV9/10 use the permanent-slot rule, including positive-zero storage;
Player scalar and other nonplayer sparse behavior remain separate. Every live
service writer supplies the actual AV index. Omitted AV9/10 entries in staged
old saves denote the constructor's zero when mutated. All preexisting values
remain readable; the save schema does not change. Regression coverage includes
Script/Maximum/Damage channels, missing/zero inputs, untouched channels, invalid
indices, actual Creature publication and existing NPC command/resource writers.

`S2/oracle-emulator/actor-dedicated-modifier-add.py` executes **2,312 cases**
through the actual lookup, arithmetic and zero-removal instructions without
hooking game functions. The C++ driver matches every result bit, including zero
sign; driver SHA-256 `2d6813d556db6f6bf2006ff29c90ecaafd05d908233eb32660526cc22ec8c731`.
The two new component tests first fail against the old behavior. Check 01 passes
**1,909 components** and **387 ASan/UBSan** tests, then fails four engine cases
whose old expectations encoded the same sparse-slot assumption. These include
an incorrect positive regeneration overshoot and removed-zero assertions.
Corrected expectations use the independent original-code result; no assertions
or cases were removed. Check 02 passes all **598 engine tests**.
Evidence: `S3/native-dedicated-modifier-slots-{01,02}`; final source fingerprint:
`5a6115668100c6b601f685d01a03f49d9b1dac66ce022a39a2f8296ef8b141f4`.

Real-engine `S3/native-dedicated-slots-{runtime,negative,continuation}-01` exercises
registered NPC script ModAV round trips, exact 100/105/107 queries, explicit
zero Script entries in saves and fresh-process continuation. Independent saved
vectors preserve every other AV, shared bases, Alive state, empty death queue/
history and namespace 1. The negative completes all actions and fails solely
its deliberately wrong expected 108. Pristine input hashes and all three
captures are checked in `S3/authority-draft/dedicated-slots-verify.py`; captures
show the textured observation-room wall and HUD, without visible combat.
These silent script-adapter courses do not close gameplay/audio gates.
Actual `openmw` SHA-256: `7005bc1158db8b173e30bc306be43b6564b800c2dfa0ce8ad858f5eb04713411`.
`openmw-tests` SHA-256: `0620b8f45b90cf0958d500e6c3ff99d096715386a155a83936c7355c955bb9b2`.
The build helper's `engine.binary_sha256` identifies `openmw-tests`, not the
runtime executable; interpret older entries copied from that field accordingly.

### Original resurrection reset preparation (implementation still open)

Pinned original-code probes now distinguish reset ownership before implementing
the remaining lifecycle adapter. `resurrection-reset.py` executes **3,840 NPC
paths** through `5F6020`: the keep-state branch requires its third boolean, body,
cell and attached-cell predicates; it changes life to 0 and issues Health Damage
`R(integer base-current)` with null source, then sets knocked-state 3. The full
branch clears the actor's Script container via `65C6A0`, replaces its process
with an actual constructed LowProcess and conditionally requests base reset.
The probe supplies allocator/free, body/cell/base/current getters and captures
base/visual/manager/Health side effects. Its second boolean is false, so it does
not establish cell-reload behavior. Attempts 01/02 lacked required allocation/
process-selection fixtures; corrected attempt 03 passes.

Crucially, Script-container clear preserves its permanent AV9/10 values.
`actor-container-reset.py` performs **432 all-AV checks** through real constructor,
all 72 insertion/lookups and clear instructions; only allocation/free are
fixtures. `player-resurrection-reset.py` performs **192 Player cases** through
`664A80` and the common reset: only Health/Magicka/Fatigue Damage is zeroed;
other Maximum/Script/Damage arrays persist. The Player wrapper passes three
zeros to the common reset regardless of its own arguments. HighProcess setup,
copy/destruction, notifications and 3D rebuilding are boundary fixtures; the
LowProcess constructor and common lifecycle setter execute.

These results guide the next reset implementation. They do not establish
inventory/base regeneration, script-local resets, visual acceptance, original
normal-input resurrection or the currently guarded native script adapter.
Controller mapping and all remaining S2/S3–S14 requirements remain open.

### S2/S3 full resurrection AV/lifecycle reset (partial)

Commit `7bafa684c0` fixes permanent nonplayer AV9/10 modifier ownership. The full
resurrection reset now has an immutable per-value policy and atomic service
adapters for real Player/NPC/Creature views. Nonplayer process ownership resets
to Low, clearing process modifiers and ordinary Script entries while preserving
Script Magicka/Fatigue. Newly constructed Damage Magicka/Fatigue retain zero.
Player process ownership resets to Active and only dynamic AV8–10 Damage clears.
Base values/shared base overrides are preserved by this portion of the reset.

Life resets to Alive with cleared essential countdown/attribution. This is a
storage reset, not a negative Health writer: negative resulting Health can
remain Alive. Resetting an already live actor is valid and repeatable. Death
counters and pending historical callbacks are preserved, not decremented or
silently consumed. Required authority must exist before any view mutation.
Tests cover all 72 values and both owners, absent/stored modifiers, malformed
inputs, actual Player/NPC/Creature publication, death-animation flag transition,
essential recovery reset, repeated reset and binary restart into a fresh NPC.

The new component test first fails against a no-op baseline. A standalone driver
matches **14,256 per-value checks** derived from the original Player arrays and
NPC Script-container clear/fresh LowProcess results described above. Driver
SHA-256: `2c71bf8d3a61a634a4732d944dd6c638c65b73d2bb5818f395c8b14285a06133`.
`S3/native-resurrection-reset-01` passes **1,910 component**, **388 ASan/UBSan**
and **600 engine tests** with exact inventories and no skips. Source fingerprint:
`f48f592bcb0938795ba57b66f3c7124f93fd6c31aa2b038df88215951fbf6716`.
`openmw-tests` SHA-256: `64c142f83435ef6e49ec993fc04eadb489c8227104f88612eecbe49ee2681ac2`.

This adapter implements the AV/lifecycle portion only. The keep-state branch,
world base/inventory reset, controller/3D replacement and actual script routing
remain open; the native Resurrect command still rejects until those required
operations are implemented. No normal-input resurrection or later gameplay
acceptance is claimed. Next: prepare keep-state resurrection, including a new
terminal entry after its intermediate Alive phase, before world integration.

### S3 preserve-state resurrection AV/lifecycle transaction (partial)

Commit `0af8eac625` adds the full-reset AV/lifecycle portion. The preserve-state
service now performs the original intermediate Alive transition, integer-base
minus current Health Damage write, and any resulting essential/death reaction
as one prepared transaction. All other values and process ownership persist.
A new death after revival increments history and appends a fresh callback even
when both initial and final phases are Dead; earlier pending callbacks remain.
The old death-animation completion marker clears on successful revival.

The regression covers three initial phases and six Health/channel cases, zero
delta writer effects, essential recovery, null new attribution, historical FIFO
ordering, exhausted event IDs, malformed recovery settings and binary restart.
The initial implementation stub fails the new test (retained in
`S3/authority-draft/preserved-resurrection-red.log`). Check 01 passes 601 tests;
the subsequently added presentation regression fails check 02 specifically for
Dead -> Alive -> Dead retaining its old completion marker. After the fix,
`S3/native-preserved-resurrection-03` passes all **601 engine tests**, exact
inventory, no skips. Source fingerprint:
`9b7b6cfd3514f12830b359b106f05bb3d1793cbe23f50f985bdd873529988741`.
`openmw-tests` SHA-256:
`32587cbc0426bfbf3ecf8239f98c9fd0e784f66aad2faff9e317802c4b1ef104`.

This is service-level coverage using actual actor views, not world/controller
resurrection acceptance. Body/cell eligibility, native knocked-state 3, base/
inventory reset, controller/3D rebuilding and script routing remain open.
Native Resurrect remains guarded until those operations are implemented.
S2/S3 and S4–S14 remain incomplete. Next: world/controller reset integration.

### S3 inventory reset prerequisite: detached content preflight

Commit `851b6e3ac5` adds preserve-state resurrection AV/lifecycle transactions.
Review of the remaining world reset found that native inventory restoration
cleared live inventory before resolving later items and owners. The shared
native preparation helper now constructs all Player/NPC/Creature replacement
item references, condition, charge, light usage, ownership and equipment-slot
selection before `applyOblivionRuntimeState` changes globals, serials, player
identity or inventories. Prepared items remain detached and unregistered.
The two former item-construction loops use that one prepared representation.

The new engine regression checks real projected weapon/ring references, left
ring selection, condition/charge/owner preservation, movement of the prepared
vector, and unchanged live inventory, pointer registry and generated serials
on success, destruction, missing plugin, missing item and missing owner.
Attempt 01 fails to compile due to incorrect fixture assumptions about record
FormKey members and a protected insertion helper; corrected attempt 02 fails
against the empty preparation stub as intended.
`S3/native-inventory-preparation-03` passes **602 engine tests**, no skips,
exact inventory. Tested source fingerprint:
`97aa9ba254e7777a6ab700af607c46d76c14a308864c2c98b455ba587df92667`.
`openmw-tests` SHA-256:
`739783223a9baffa15cf90192b4d8e1514c66fa8cd07f73a7a18771a8a787131`.

Real content courses `S3/inventory-preflight-{runtime,reject,continuation}-01`
all pass. The input is the preserved S0 M13 item-course save, SHA-256
`a3de683eb63a20c722d6f30bca95ec8da09b16e1935afaf2a534c1aa816f2ea6`.
Independent decoded-save verification preserves player inventory contents and
equipment and all **37,036** original references' item contents. One active
NPC (`026be1`) equips its existing torch (`02cf9f`, light slot 262144), matching
the unchanged `updateEquippedLight` path. Fresh continuation preserves all
**37,162** references' contents and equipment without additional differences.
Comparisons aggregate otherwise-identical stacks while checking equipment
separately; they do not mistake restacking for item loss. A structurally valid
save with an unresolved final player item rejects with the exact content error,
without apply/save success, and all pristine/input hashes remain unchanged.
All three captures were directly inspected: two Imperial Prison room/HUD views
and the precise failed-load dialog. No combat or audio acceptance is implied.
Verification: `S3/authority-draft/inventory-preflight-verify.py` and
`S3/inventory-preflight-verification.json`. Runtime `openmw` SHA-256:
`0d5e82a9aa8ddeb76a4e897be9bf57b6b9d54c6f15a2c33dafea523c1bc78105`.

This closes content preflight only. Final inventory insertion/registration,
equipment callbacks, whole-world restore rollback, resurrection base reset
and controller/3D operations remain open. No S2/S3 or later gate is closed.
Next: prepare inventory publication and native world resurrection continuation.

### S3 prepared reference-registry publication prerequisite

Commit `82e9fe6a45` prepares native inventory content before world mutation.
`WorldModel::preparePtrReplacement` now prepares a batch in a private registry
snapshot, removing only registered inputs and inserting detached references.
Live pointers, revision and generated counter remain unchanged until commit.
Commit rejects stale registry/counter state, changed prepared identities and
repeat/moved-from calls before mutation; successful publication swaps the
registry and assigns reference ownership without allocation or callbacks.
Use one batch for a multi-actor restoration, rather than copying the complete
registry separately for each actor. References and WorldModel must outlive the
preparation; detached references may retain allocated IDs after discard.

Four `MWWorldPtrTest` cases cover preparation/discard/move/commit, unrelated
reference preservation, null/non-detached/unregistered inputs, duplicate IDs,
namespace exhaustion, stale revisions/counters/identities, stable-ID
replacement followed by retired-reference destruction, and reservation retry.
The initial stub fails check 01. Moving the tests to the pointer suite exposes
a missing concrete Weapon include in build 02; build 03 fixes that and then
the new retry regression fails: a later ordinary registration overwrites the
retried reference because its retained ID did not advance the counter.
The corrected preparation advances past all retained generated IDs.

`S3/native-pointer-replacement-04` passes all **606 engine tests**, exact
inventory and no skips. Source fingerprint:
`a0772fdd053de85fef61f2c1cd1d20fd6ba539c67d764d28dc176f891d044ac4`.
`openmw-tests` SHA-256:
`dd9b70731180d76961e58e7ce571e2acb12e4cf50d3070cd16b0021adbdf999c`.

This is a tested publication primitive, not a completed inventory/world
transaction. Its native inventory consumer, owner/listener preservation,
controller operations and full resurrection command remain to be integrated.
No real-runtime gate or S2/S3–S14 completion is inferred from these tests.
Next: stage and swap inventory contents with this prepared registry batch.

### S3 detached inventory contents and publication swap

Commit `3db30d718f` adds prepared registry replacement. Native inventory content
can now be staged as a resolved InventoryStore without pointer registration or
callbacks, including non-stackable equipment splitting. `swapPreparedContents`
publishes prebuilt lists/slots without allocation or callbacks, keeps the
owner/listeners/resolution bindings, rebinds equipment and selected-item
iterators, swaps matching weight/modified/resolved state, and invalidates
recharge caches. Callers must prepare the corresponding registry publication
and reacquire external iterators; this is not a general unchecked store move.

The existing real weapon/ring preparation case now performs registry commit,
contents swap and retired-store destruction. It checks exact stack counts,
condition/charge, equipped weapon splitting, left ring, selected item, cached
weights, owner/listener retention, final registered container pointers, no
callbacks and no registry changes when retired contents are destroyed.
Check 01 fails against the empty staging stub. Check 02 finds the equipment
read override was private; exposing it to subclasses as protected matches the
base interface and allows reuse of its equipment validation.
`S3/native-inventory-publication-03` passes all **606 engine tests**, exact
inventory, no skips. Source fingerprint:
`ffacbc896725ee92dbbdec247b8b2c23362bf2e7830722f374a27899b0fdf49e`.
`openmw-tests` SHA-256:
`8473fdbc0db2d54da4a5d69f12fe8160916e07d37770abec707ee4f08c351fa3`.

No world reset consumer is enabled by this chunk. Native save restoration
currently uses content preflight followed by its existing insertion path;
whole-world publication and controller callbacks remain open. Physical
resurrection is an S6 deliverable, not a reason to call S3 activation complete.
The owned sanitizer configuration has now successfully configured engine/tests
ON (same ASan/UBSan flags); its engine compilation and lifetime tests are next.
After that, return to remaining S3 native stat adapters/activation and its gate.

### S3 Lua modified-value queries use native authority

Following `b2d52b7d62`, actual Lua `Actor.stats.attributes.*.modified` and
`NPC.stats.skills.*.modified` now read a native projection's exact current
value. Their previous TES3 recomposition omitted Script modifiers and included
Maximum even for a native Low process; it also incorrectly clamped negative
native results. Legacy values retain the existing cached-property composition
and zero clamp. This change does not implement queued Lua stat writers.

The retained baseline `S3/authority-draft/lua-native-modified-red.{cpp,log,xml}`
executes actual bindings against the prior engine library: Strength returned
44 instead of Active57/Low50; Blade returned33 instead of Active50/Low45.
The first scratch fixture used Armorer's index12 for Blade and was corrected
to14 before retaining that baseline. Scratch replacement/expanded/final runs
pass; `lua-native-reproduction-hashes.json` identifies the initial artifacts.
The committed tests additionally cover negative native values, TES3 clamping,
and the actual shared Player class with a native projection.

`S3/native-lua-projection-01` rebuilds the engine and passes all **608 engine
tests**, exact inventory, zero failures/skips. Tested source fingerprint:
`9b2b44e5aeffb5792a0f3610af28f40534753d144a5878a0de61fa05a04b97ff`.
`openmw-tests` SHA-256:
`456e6b49cf5b6a03fdf7af8a742bd13da8cbb169c72115bb1f72ac9b2297fd27`.

The first full sanitizer engine build (`S3/native-inventory-sanitized-engine-01`)
was deliberately interrupted during compilation to integrate this fix and
increase parallelism after checking memory. Its verification is **failed / no
tests run**, with the reason in `interruption.json`; it is not sanitizer pass
or leak-coverage evidence. The incremental full run remains required.

A separate real-engine baseline, `S3/health-command-atomic-red-01`, exposes the
next writer transaction gap. With the death-event namespace exhausted, ModAV
Health -200 throws after changing Health100 to -100. GetDead remains0 and a
normal F5 save retains Script=-200. All scenario actions complete; the expected
GetAV100 is absent and the error is logged. The capture was inspected (rendered
room/HUD, no dialog); the silent diagnostic course is not gameplay acceptance.
Its manifest, preparation script, source/pristine hashes and decoded state are
retained under `S3/authority-draft/health-command-atomic-*` and the run directory.
Next: prepare command Health storage and lifecycle reaction together, then
continue the remaining S3 adapters/activation gates. S2/S3 and S4–S14 remain open.

### S3 complete engine sanitizer checkpoint

Commit `bc38bce7a0` passes **all 608 engine tests** under ASan, UBSan and leak
detection in `S3/native-inventory-sanitized-engine-02`, with exact inventory and
zero skips/failures. This includes prepared inventory/registry publication and
native Lua projections. The clean test process exits without sanitizer or leak
findings. It is not an instrumented rendered-game acceptance run.

Configuration: GCC, Debug `-O1 -g`,
`-fsanitize=address,undefined -fno-omit-frame-pointer`, bundled double Bullet;
engine/tests enabled, four build jobs. The exact runner environment is retained
in `sanitizer-environment.json`: `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. The first full compilation
has a long `teststore.cpp` tail; GCC reports a debug-variable-tracking limit and
retries without assignment tracking. That compiler note is retained.

Source fingerprint:
`c9beceb90e332468e4e11ea80b325aea1878f461f8ee9f531e1a93a6496f63d8`.
Sanitized `openmw-tests` SHA-256:
`3d0eee1fb63c7e8984e8f233cc38da460d3424ae6c01907584a037f39d6e1671`.
The reusable build skill now records the tested engine-sanitizer invocation and
its distinction from the component-only sanitizer mode. Next: the already
reproduced Health-command transaction failure, then remaining S3 gates.

### S3 native command Health and lifecycle commit together

After sanitizer checkpoint `c1c226fc90`, negative-Health Mod/Force commands now
prepare value storage, terminal/essential reaction, shared projections, and
history/event allocation before publishing any of them. The world no longer
commits a stat change followed by a separately fallible death reaction. Player
base preparation is shared with the existing publisher; command eligibility,
Script versus console Damage storage, native sparse slots and god-mode integer
conversion order are preserved. Returned Health-delta metadata describes an
already committed reaction. Negative-Health commands require initialized life;
world legacy-marker adoption remains the existing separate initialization step.

The focused prior-code reproduction `S3/authority-draft/health-command-atomic-red02`
fails both full-state equality and shared Health100 (actual -100) after event-ID
exhaustion. The first scratch attempt lacked a required reference fixture and
was rejected before the assertion; it is retained separately. Actual engine
baselines `S3/health-command-atomic-red-{01,02}` confirm the persisted partial
write with legacy-only and already initialized lifecycle inputs respectively.

Three actual Player/NPC/Creature tests cover **144 combinations** of prior life,
essential status, script/console origin, Mod/Force, and absent/existing zero
Damage slots. They also cover repeated commands without duplicate deaths,
binary round trips, invalid recovery settings and exhausted-event rollback,
missing-life rejection, and Player god-mode eligibility before integer overflow.
`S3/native-health-command-atomic-01` and
`S3/native-health-command-atomic-sanitized-01` both pass **all 611 engine tests**,
exact inventories and no skips. The latter enables ASan, UBSan and leak detection;
no findings occur. Shared tested implementation fingerprint:
`1d3cd4bbede25d0c486bbce49a337b2e5bef52262e5e0b448c6829d734cb8185`.
Regular/sanitized `openmw-tests` hashes:
`8b5b937488ab1ed15f9159f8f884ef63c0ef7aa8ef1f107ec597efa6822a269b` /
`42b1e498f078ca8984367e2622b35d8682c63c7973866dd2ccee08ea662814e9`.

All four actual-engine courses pass:
`S3/health-command-atomic-{runtime,continuation}-01` preserves Health100 and all
native value/base/life/event/count fields after rejection and fresh-process
load/resave. `S3/health-command-death-{runtime,continuation}-01` commits Health-100,
Dead, historical count1 and one callback. Both the callback's Autosave and F5
Quicksave agree; restarting the callback Autosave preserves script local1,
consumed event state and the input Autosave bytes without firing another event.
Per-run state verification and pristine hashes are retained. Actual `openmw`:
`6606594a313e027c724f16f8063e9dc5f98bdc9a61a5ff7068306490e369f049`.
All captures were directly inspected: rendered room/HUD, no error dialog. These
silent command-adapter courses do not prove corpse pose, combat or audio.

No S2/S3 or later stage gate closes here. Next: remaining native write adapters
and activation, including rest/drowning and queued Lua writes, plus canonical
player-base identity. The original drowning arithmetic/caller audit started
while the sanitizer build ran is retained under `S3/authority-draft/` and
`S2/oracle-emulator/drowning-rules.py`; it is not an implemented gameplay feature.

### S3 floating Health writes preserve the lifecycle transaction

After `45ead0fbfd`, the native service accepts fractional Damage-channel Health
writes without the integer command conversion path. Negative deltas prepare
Health, lifecycle, source attribution, shared views and death history/events
together. Positive/zero writes retain the current lifecycle; healing does not
resurrect. Player god mode suppresses negative finite deltas, while malformed
nonfinite input is rejected before suppression. Source actor validity remains
the caller's responsibility; no automatic actor registration is enabled.

Three actual NPC/Creature/Player tests cover fractional damage/healing, the
strict current-Health-below-one threshold and adjacent float rounding, killer
attribution, essential recovery, repeated writes without duplicate events,
binary round trips, missing lifecycle, invalid inputs/settings, god mode and
exhausted-event rollback of persistent state and shared projections. The stub
baseline `S3/native-floating-health-red-01` fails all three new cases. Its test
fixture narrowing warning was corrected before the passing runs.

`S3/native-floating-health-01` and `S3/native-floating-health-sanitized-01` pass
**all 614 engine tests**, matching exact inventories with zero skips/failures.
The latter enables ASan, UBSan and leak detection and reports no findings.
Tested implementation fingerprint:
`1054a9f543bac85c41910c1af621282617e3daac028a73afd1ec2de18b3cfbe2`.
Regular test binary: `88df9db078412fbc7e38f7271ec6cee5592ca829a4dbcc03988e6b1b9b06c9cc`.
Sanitized test binary: `61c4bcdee2541e94c94a1ec5e91c275bac2522025980ca3797e0232858751b16`.
These are service/engine-suite checks, not rendered drowning acceptance.

Next: native drowning settings, float stores, timer and gameplay caller.
The independent `S2/oracle-emulator/drowning-timer.py` probe adds 1,620 original
timer arithmetic/branch paths: damage starts strictly below zero; nonzero
integer WaterBreathing replenishes the timer, capped by the breath maximum.
Water/controller predicates, AI and full damage dispatch remain outside that
probe. S2/S3 and S4–S14 remain open.

### S2/S3 native drowning arithmetic and settings

After `5ea35f35e4`, pure native rules provide integer-Endurance breath maximum,
integer-base-Health drowning damage with separate rate/frame stores, and the
strict-negative timer transition with incremental WaterBreathing restoration.
Typed settings distinguish compiled defaults from winning records. Negative
finite modded settings follow the original arithmetic; nonfinite input,
negative duration and persistent arithmetic overflow are rejected.

`S2/native-drowning-rules-02` passes **all 1,914 component tests** and **392 ESM4
component tests under ASan/UBSan**, exact inventories and no skips/findings.
This component sanitizer mode disables leak detection. Fingerprint:
`1ef3dbc781d184c5a9074154c658e0f4410959ab7c7fc647e1ac4b9c8349eb89`.
Test binary hashes (regular / sanitized):
`e5e8608b2a6bf54cc59a10f3dc4847f105e8e6555f51cd0eaba6aecbc335f74d` /
`e7cc85fb17f4770aea5710c3114c0aa6ea3f90325960361fbef918739ad36885`.
The retained `-01` run fails one overly rounded expected literal; the original
helper independently confirms the corrected exact value. Four new test cases
cover arithmetic, timer/gate boundaries, typed defaults/overrides and malformed
inputs. Original-instruction comparisons match **13,180** helper/timer/frame
rows; details and boundary declarations are in the provenance document.

The gameplay caller still needs integration. Read-only follow-up identifies
three native water-height probes (.01, .7, .875 of actor height), swimming-state
and aquatic-creature branches, and a Player god-mode breath reset. The current
TES3 submerged/knockout predicate must not be silently reused as equivalent.
S2/S3 and all later acceptance gates remain open.

### S3 persistent native breath ownership (schema 14)

After `13fb01a23e`, runtime schema14 adds a native actor-keyed float breath map,
including unloaded references. C++ binary/JSON and Python readers/writers agree;
service capture/restore prepares the map before publication and clear removes
it. Entries require existing native actor values and canonical identities.
Duplicate/dangling identities, nonfinite values, oversized/truncated payloads
and writes into pre14 payloads are rejected. Finite negative timers remain valid
because modded negative maxima can produce them. Versions1–13 leave the map
absent; process adoption is responsible for initialization. No frame writer or
automatic actor registration is enabled by this persistence change.

`S3/native-breath-state-02` passes **175 Python tests, all 1,917 component tests,
and all 615 engine tests**. `S3/native-breath-state-sanitized-01` passes **615
engine tests with ASan/UBSan/leak detection** and **395 ESM4 component tests with
ASan/UBSan** (component mode disables leaks). Exact C++ inventories, zero skips
or findings. The retained `-01` Python attempt exposed two new fixtures missing
the required nonzero AI RNG seed; corrected fixtures pass. The existing v13
death-counter corruption test explicitly retains version13 so its tail offsets
continue to exercise the intended bytes. Implementation fingerprint:
`3e16df303d193deb1b2e4afac872eda913acd2a69ef8c84655e1a4f9d184c901`.
Regular engine-test hash:
`9a4e320c6eee1c480eedd88d1f0775b42d77f6d1d7a6846b01a2f993a9503af9`;
sanitized engine-test hash:
`05202a58375e860ce9bb38aa976e4a71aa8f88aa7f525fad6c0c302931e21fc2`.

Actual-engine `S3/native-breath-state-{runtime,continuation}-01` loads and resaves
a .125 timer exactly, then repeats in a fresh process. Independent decoding
checks breath, values, base overrides, life, counts and event fields; source and
pristine hashes are verified. Both use the existing read-only Health-command
continuation manifest; its ordinary F5 action exercises world save capture.
Actual executable SHA-256:
`d46aa355d292e28d1ee46bbf6eeb9b77c58ea84ff593b0685311d2626fb24cb9`.
Both captures were inspected: room/HUD, no error dialog. The runs are silent
persistence checks and do not prove drowning gameplay or audio. Preparation and
verification scripts remain under `S3/authority-draft/`; per-run input/state
verification records retain all hashes.

Next: native creature capability flags, verified water eligibility and the
frame writer/UI. The original eligibility probe now passes4,096 complete paths
without function stubs; pure aquatic creatures consume breath out of deep water,
ordinary actors only while deeply submerged and swimming, Player god mode resets
the timer. Original process constructor/reset stores20, not the shared NPC -1
sentinel. S2/S3 and S4–S14 remain open.

### S3 native creature movement capabilities

After `b712802f66`, ESM4Creature reads its native base flags for bipedal, flying,
swimming and walking capabilities. Biped implies both swimming and walking, as
in original helpers519C70/519CC0. This replaces the prior constant biped/swim/walk
true, fly false responses and allows shared aquatic/flying/land classification
to reflect the native record. NPC capability behavior is unchanged.

The original `S2/oracle-emulator/creature-capabilities.py` executes **768** complete
base capability queries over all256 low-byte flag patterns, without function
stubs. An actual Creature-class test covers ten distinct capability/classification
combinations, including unrelated Essential/PCLevelOffset bits.
`S3/native-creature-capabilities-01` passes **all616 engine tests**, exact inventory
with no skips/failures. Source fingerprint:
`c973a415cb0be3758a2b1ff4018f466eb1fcfda1027a2db8a4451746127319e4`; test binary:
`3a23fcf111c05ab0057742b7c9755f27086dcff143cc1a696faa8c567e38ea2b`. This chunk has a regular engine check; the previous
sanitizer evidence covers the preceding timer-state revision, not these flag
changes. Rendered movement, flight and aquatic navigation remain campaign gates.
Next: the native drowning frame writer and UI. S2/S3 and later gates remain open.

### S3 transactional native breath service

After `c56c6b1344`, Player/NPC/Creature breath updates use native integer
Endurance/WaterBreathing, base Health, the original constructor timer20 and the
verified strict-negative timer/damage rules. Timer adoption allocates before
Health publication; existing timers update without a map copy. Health, life,
death events/counts and the timer roll back together on rejected transitions.
Dead actors do not advance; dry actors and Player god mode reset to maximum.
Three actual actor tests cover fractional boundaries, damage ownership,
essential recovery, terminal death, rollback, malformed inputs and persistence.

`S3/native-breath-service-01` and `S3/native-breath-service-sanitized-01` pass
**all619 engine tests**, exact inventories, zero skips/failures. Sanitizers use
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, with no findings. This also
covers the preceding creature-capability change. Tested source fingerprint:
`f00bbac4f7080c520ce63cb1ee27e18eb8963275d5515529236531edc9dd9cd9`.
Regular/sanitized engine-test hashes:
`16b86be1f68123b4efab36d720cb9ee8800ec4779faabc60c3f9007d7aa12a68` /
`2bf952fb608a017077e8848e87387992461747097bb02f00a3454b24ad9b6d83`.

The service is not yet a frame caller. Water prototypes `S3/native-breath-water-
red-{01,02}` retained unchanged native maps while legacy stats ran independently:
loading authority maps alone does not publish their shared stat projections.
The corrected `-03` explicitly executes native `ModAV Health 0` for Player/NPC;
it reproduces frame errors from legacy writes to protected native stats. Its
inspected capture shows the room/HUD, and the harness correctly rejects the
errors. These are retained failures, not gameplay acceptance. The dry baseline
likewise proves map persistence only. Next: native frame/UI integration and
independently decoded underwater/dry/restart outcomes. Automatic registration
and publication remain off; S2/S3 and S4–S14 remain open.

### S3 native drowning frame and UI integration

After `004d897dd8`, physically resident native projections route breath updates
through World and the transactional service, including creatures. Winning
native GMSTs, .875 height comparison, creature capabilities, controller swimming,
Player god mode and essential recovery feed the update. Native drowning resolves
`NPCHumanDrowning` when present and activates the Player hit overlay. The HUD reads
native remaining/maximum directly; missing/nonpositive maxima hide the bar and
negative native timers are not confused with the legacy initialization sentinel.
Unregistered actors retain their existing path while automatic activation is off.

The longer runtime course exposed a second integration defect: UI stat caches
assigned over protected native views. Dynamic/attribute/skill snapshots now
copy-construct replacements in StatsWatcher, character creation, stats/review
windows and attribute widgets. Actor-authority write guards remain enforced.
The actual UI course performs22 native writes, including two decrease/restore
cycles for Player Health, Magicka, Fatigue, Strength and Blade, then independently
checks final values and absence of frame errors.

`S3/native-breath-frame-01` passes **all1,919 component tests**, then fails the
engine build because the sound manager returns a nonnull wrapper rather than
an `auto*`. That compiler failure is retained. The corrected `-02` passes177
Python/all619 engine tests; `-03`, after the UI snapshot fix, again passes
**177 Python/all619 engine tests**, exact inventory, no skips/failures.
Final implementation fingerprint:
`9eb7a06b2f936ab41098dee4f473239bbcde8d986bd239772887aa028c8c571a`;
engine-test SHA `5a72c6a186a986445df26e3df33e9e724f79044af3cccceb60a7eb267913a570`.
`S3/native-water-rules-sanitized-01` passes **397 ESM4 component tests** under
ASan/UBSan (leaks disabled), including the two new water/eligibility tests.
This does not yet sanitize the new World/UI paths. Original-water comparisons
match864 paths; geometry/controller limits are recorded in provenance.

Current actual-engine evidence uses executable SHA
`583024621b56fe0885f20b21acafb286faa2417954152265b03381f4117d6c14`:

- `S3/native-breath-water-frame-02`: NPC timer0/Health78.7744026, Player timer
  4.9491277/Health100, both Alive, no events. Damage matches independently measured
  Player timer consumption within the predeclared five-Health crossing tolerance.
- `S3/native-breath-dry-frame-02`: both Endurance50 timers19, Health100 unchanged.
- `S3/native-breath-water-death-02`: NPC native Health.6396561 crosses the strict
  below1 death threshold; exactly one count/callback, null killer and consumed
  queue. Player timer0/Health77.6447601 agrees with its shared saved view.
- `S3/native-breath-water-death-continuation-01`: fresh load of actual callback
  Autosave retains NPC Health/life/count1/local counter1, next event2/empty queue,
  with no callback replay/new Autosave. Player breath continues.
- `S3/native-ui-snapshots-01`:22 writes, restored values, timers19 and no deaths.

All listed scenarios and independent state checks pass. Source/pristine hashes,
commands, configs and saves are retained per run. Inspected captures show dry
hidden/underwater visible breath bars and updated Player Health after damage;
they face the room wall and do not prove NPC corpse pose. Runs are silent and
the minimal fixture has no drowning SOUN; audio acceptance remains open.
The earlier death-01 run is retained as the UI-cache failure. Earlier short
water/dry/continuation-01 runs used the pre-cache-fix executable and are historical.
The committed bounded water builder/recipe reproduces the pinned generated
plugin byte for byte, with structural and semantic readback and malformed-input
checks; this is not general TES4 MCP authoring support.

Next: full engine sanitizer coverage of frame/UI changes, then remaining native
Magicka/rest/Lua writer and activation work. Creature/surface-transition gameplay,
full physical resurrection/ragdolls, normal campaign setup and later gates remain
open. S2/S3 and S4–S14 are not accepted by these focused courses.

The final binary also passes `S3/native-breath-water-continuation-02`: fresh
load of frame-02 Quicksave gives NPC Health67.8345718/timer0 and Player
Health100/timer4.4021368, with no deaths or errors and matching timer/damage
rate. Capture inspected. `S3/native-breath-manifests-01` reruns all177 Python
tests after the final editable continuation/death/UI manifests are installed.

### Frame/UI sanitizer checkpoint and Magicka baseline

`S3/native-breath-frame-sanitized-01` builds the engine, engine tests and esmtool
from clean commit `0a47a5d8073814f7f857bc4abf3f1237b8f698b4`, then passes all
**619 engine tests**, exact inventory, no skips or sanitizer findings, with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
Source fingerprint: `33776db43f09e75c44fe3fce18d542c214d0028b4a89d74ab3c9d7dcecb0a371`;
engine-test SHA: `65d128098eeb1aef8cf06aceb7c5ea61084e7b52ebe7f5a132b937062db36f17`.
This includes compiling the World/UI changes; the graphical runtime courses
above used the regular binary, not the sanitizer binary.

The same regular binary also passes `S3/native-breath-morrowind-01`, using the
existing M14 Morrowind regression manifest: startup, save and profile isolation,
with no GPRO/T4VR/T4ST/OMW4STATE in the actual save. Save SHA:
`73f1394a6556df3ec527361aa83470ecf6f059d900db3fddc093ebd6019f7261`.
The inspected capture shows the Balmora startup view against nearby rocks and
the HUD. This does not claim route or complete asset acceptance; the manifest's
reviewed missing-asset findings remain allowed.

`S3/native-magicka-frame-red-01` is the retained **failing** next-writer baseline.
Both actors start at Magicka20 (base100, Damage-80), Willpower50, with explicit
native publication and no casting. Player breath measures3.08869 simulated
seconds; independent expected Magicka is25.4052105, with tolerance.05 declared
before execution. Both native values and the Player shared saved view remain20.
Scenario actions complete without errors, but intended-state verification fails.
Save SHA: `712507d7438f25785ca6414bd991fb12426165a62513323fca5def65513bdaec`.
Capture and pristine inputs retained. Remaining Magicka/rest/Lua, activation and
later milestone gates remain open.

### Isolated native Magicka regeneration writer

The new NPC/Creature and Player service entry points restore only Magicka's
Damage channel and publish native authority plus shared views together. The
restoration helper is shared with the existing rest transaction. The caller
supplies active casting-item state explicitly; selected spell and animation
follow-through are not substituted for that input. Player preparation resolves
raw form contributions with the supplied current dynamic-base settings before
calculating the request. Nonplayer current queries retain the outer Magicka
scale while the rate uses its unscaled restoration maximum.

Three actual actor-instance tests cover fractional frames, integer Willpower,
active-item suppression, signed/fractional Stunted Magicka, active/low processes,
Maximum and Script preservation, permanent Damage slots, full/dead actors,
invalid duration/settings rollback, binary reload continuation and separate
Health/Fatigue preservation. Additional cases change Player base settings and
exercise nonplayer scale. These are service checks; frame integration remains
next, and the retained runtime Magicka baseline is still failing at this point.

`S3/native-magicka-service-01` retains an initializer compilation failure in the
new test setup. Explicit optional initializers fix it. `-02` passes all622 engine
tests; `-03`, including the derived-value cases, passes all622 again.
`S3/native-magicka-service-sanitized-01` passes the same complete622 inventory
under ASan/UBSan with leak detection and halt-on-error enabled. No skips,
failures or sanitizer findings. Tested source fingerprint:
`81af5e048a9804d6e477fea875e1284457fd6ea4a57b4f6d3de6b59eda02f7e9`;
regular engine-test SHA `79b2edb745530b9ef3e08fca7326ec1b6117e2519eb1cb7f3f57bbe8ee62a197`;
sanitized engine-test SHA `d5593715b333593241082bf40ac4ed95e08cd44f310d35d7dfa6a14dc0c0b248`.

### UI snapshot self-alias lifetime repair

The actual ASan graphical course `S3/native-magicka-frame-sanitized-01`
exposed a heap-use-after-free during initial `StatsWindow::addSkills`:
`setValue` received a reference to its own map entry, erased that entry, then
copied and displayed the dangling value. The earlier complete623 sanitized
engine unit tests did not exercise this GUI path. The failing process log and
original pristine save are retained; the failure is not suppressed.

StatsWindow, ReviewDialog and CharacterCreation now copy the input before
replacing cached entries and updating widgets. MWAttribute likewise constructs
its copy before replacing its optional value. Native actor write guards remain
intact. `S3/native-ui-alias-01` and `native-ui-alias-sanitized-01` each pass all623
engine cases, exact inventory/no skips; the latter enables ASan leak checks and
UBSan halt-on-error. Both test the same dirty source fingerprint, including the
pending frame integration:
`fa75145887adcbcda287e31687d2c819ebec8cba8528fcad88b95f28a112433c`.
Their engine-test binary SHAs are respectively
`4a5818e5614dda9e92d2e198a09f098592369976dd92ae86411610ddc6ba4777` and
`901276ac2180b28e7c4d01d6a92de1b1d2876f45db89bed3b536f803842fd69a`.

`S3/native-magicka-frame-sanitized-02` repeats the exact failed pristine input
and passes actual runtime/save checks without ASan/UBSan findings. Graphical
runtime leak detection is disabled (unlike the unit run). Actual engine SHA:
`efaea276b2da69b3cab49e0fa921934627e4e3693f350cc282c05316bddc7027`.
Both actors reach Magicka25.3187714 against independent expectation25.3188025
at elapsed3.0393157, within the predeclared.05 tolerance; native/shared Player
values agree, only Damage changes and no death is introduced. Save SHA:
`50c042e8625d1ffabe86f1e0c0ec5be45184bb6799b5530070c66cc1052e83ac`.
The inspected capture shows the underwater fixture wall, breath bar and updated
Magicka HUD. This silent focused course is not campaign/audio acceptance.

### Native frame Magicka and explicit M16 casting boundary

World now resolves winning native Magicka/Fatigue settings in one record pass
and submits one frame transaction. Magicka and movement/Fatigue are prepared
before either is published; a later invalid Fatigue input rolls back Magicka.
Player derived bases are prepared first. Actual Player/NPC/Creature tests cover
fractional simultaneous restoration, casting suppression of Magicka alone and
rollback. The new settings test rejects wrong native types and ignores TES3
GMSTs. Service tests also reject spell/effect/consumable calls on actual native
actors before legacy lookup, notifications, inventory removal or state changes.

Full spell/effect/enchantment execution belongs to M16 in the plan. Native World
and CastSpell entry points now report unsupported execution before TES3 costs
or effects; the controller avoids casting/enchantment fallback for that result.
Consequently the current native frame has no active casting item. M16 must wire
its actual item lifetime, not selected-spell or animation flags, into the existing
explicit service input. This boundary is not claimed as a magic implementation.

`S3/native-resource-frame-01` passes623 engine tests; `-02` retains a compilation
failure from a record-constant spelling, corrected in `-03` (all623 pass).
`native-resource-frame-sanitized-01` passes all623 under ASan/UBSan with leak
checks. Final pre-UI-repair fingerprint:
`21e1e9494af8b9c6f99e76e9de7e01d16788965607c921f8940fb74af7347347`.
The UI repair above subsequently rechecks all623 against the combined final
source. `native-resource-manifests-01` passes all177 Python tests, no skips.

Actual regular runtime checks (all state verifiers pass, captures inspected):

- `native-magicka-frame-01`: both Magicka25.4053497 against25.4053524 expected,
  measured3.0887728 seconds; source Magicka20/base100/Willpower50. Engine SHA
  `ceba731f7730b0ee2cfc734b993e06ba04260e6c83d6db66b8295bfd9599d73d`.
- `native-magicka-continuation-01`: fresh process using the actual prior save,
  both26.3776627 against26.3776542 expected, no reinjection or deaths.
- `native-magicka-stunted-frame-01`: Stunted1 keeps both Magicka20 while Fatigue
  rises from150 to180.6454926 against180.6454802 expected. Shared Player agrees.
  The latter two use engine SHA
  `f2ef16aa9ef3a40b81f885ab29f2579a4bea32264e906222356d7d9fcd8725cb`.

Inputs, source/pristine/save hashes, independent.05 tolerances and verification
results remain under each evidence directory. These cases explicitly publish
actors with ModAV Health0; automatic registration remains off. Captures face the
fixture wall and show HUD/breath changes; silent runs do not prove audio.
The actual sanitized native replay and its retained GUI failure are described
in the preceding section.

The editable `morrowind_m15_magic_regression.json` course sets up Hearth Heal,
saves before casting, then uses normal R/mouse input and saves afterward.
Independent installed-record audit finds Restore Health20–80 for one second,
cost13. `native-resource-morrowind-magic-01` passes: Health25->73.0628967 and
Magicka100->87, unchanged maxima and no native save markers. The same course
under the repaired ASan/UBSan engine passes in
`native-resource-morrowind-magic-sanitized-01`: Health25->70.0952454, Magicka87,
no sanitizer findings (graphical leak checks disabled). Before/after save SHAs:
`71bdacebbed711f5e4b439a682e7d03083b9c9e245bffc83e33d5a5a3268f98e` /
`5f7bc9aaff95c9ff9cddba60524c8b573403b7cf39085cef370015471d1fd292`.
Its engine SHA is the same `efaea276...` identified above. Both captures were
inspected: Balmora startup rocks, raised casting hands and changed Health/Magicka
HUD. Existing reviewed missing-asset allowances remain; this is not full-route,
asset or audio acceptance. Lua writers, rest, automatic registration, complete
S2/S3 acceptance and S4–S14 remain open.

### Failed Lua stat batches drain safely

LocalScripts now moves the pending stat batch out of SelfObject before actor
lookup or setter dispatch. Previously a rejected setter left the cache nonempty:
normal subsequent writes could not schedule another StatUpdateAction, and an
explicit second flush replayed an already committed prefix before failing again.
The fix retains exception reporting and stops at the first failure, discarding
the unprocessed remainder. It does not make a batch atomic or add native Lua
property semantics. Individual successful writes retain their own transaction.

The independent focused red/green reproduction is retained in
`S3/authority-draft/lua-cache-failure-*`, including the genuine six-assertion red
and two earlier harness setup failures. Three integrated actual LocalScripts /
SelfObject tests now check failed-prefix recovery, same-key last-write wins and
actor removal before flushing. `S3/lua-cache-drain-01` and
`lua-cache-drain-sanitized-01` pass all626 engine cases, exact inventory/no skips.
ASan/UBSan and leak detection are enabled in the latter; no findings.
Tested source fingerprint:
`a3bb1ce50736b050c83bda1fac03cacb21829432cbaf434f20047f4de76729f5`;
regular/sanitized engine-test SHAs:
`4c3ff3c967a819c855c0649eb699700a17a55d4ff26e0869af490157e88da546` /
`1633874e6cd7b5297a8c875b981d9572f6605ecd1fdec9c5e9e3d725029253b4`.
Native property writers and the remaining rest/activation gates remain open.

### Player resource bases refresh before every request

All six Player resource entry points now prepare derived bases before computing
their operation, and publish a changed base even when no resource delta occurs:
Fatigue/Magicka regeneration, rest, movement/Fatigue, combined frame and jump.
This repairs stale rest rates and prevents zero-duration, suppressed-casting or
suppressed-expenditure calls from silently retaining old settings. Dead actors
remain untouched. Preparation and invalid-settings rejection precede publication.

The actual Player test changes Intelligence50 Magicka base150 to200 through the
new setting. A one-second rest request must be3.5, yielding123.5, rather than
using the old2.625 rate. Six zero-duration/suppressed entry cases require current120,
only the derived base changed, raw form inputs/modifiers preserved; invalid base
settings must reject without changes. Existing dead and rollback cases remain.
`S3/player-resource-base-refresh-01` and `player-resource-base-refresh-sanitized-01`
pass all626 engine tests, exact inventory/no skips; ASan/UBSan and leak detection
are enabled in the latter, no findings. Source fingerprint:
`3cd28ae1d2cae1fad708d1c0b38d007640bd1899da54e2cea8f8d432a2291f30`.
Regular/sanitized engine-test SHAs:
`41fefd34f55c557ba675329ca2ab98bbfdb0c52577e7228feba5ae4eabb10c73` /
`7c2762acd88de1d02fb355cf20b95602defc8dc05041220451cb4aa7492d7dbb`.

The next retained normal-input baseline, `S3/native-rest-dialog-red-01`, fails
before restoration: T invokes the shared enemy query and produces
`Bad LiveCellRef cast to NPC_ from NPC_4`. The nearby native NPC reaches TES3
`isAggressive`/`getDerivedDisposition`. All actions finish and save, but no dialog
opens or rest hour completes; both Health25 and Stunted Magicka20 remain unchanged.
All three captures inspected. Engine SHA:
`a3b0f2c175f503a7d6f26530e371b821160a129dc2ab281a0e89dbaa611ff887`;
save SHA `c4f7f2619dc7c737b323d2494c87189f3b528fa170b9e873179005a03d1af886`.
The ignored editable draft is `S3/authority-draft/native-rest-dialog.json`;
pristine inputs and precise source hashes are retained. Native enemy queries,
rest integration, Lua property writers and activation still require work.

### Version-dependent skin partition fields initialize before moves

The actual graphical sanitizer course `S3/native-engagement-runtime-sanitized-01`
failed before applying its save: UBSan caught an invalid boolean read while
moving `NiSkinPartition::Partition` during character-preview mesh loading.
Oblivion NIFs omit the newer LOD/global-buffer fields; these and the optional
vertex descriptor now initialize explicitly. Parent SSE-only size/descriptor
fields also initialize. A parser regression reads and moves two partitions for
Bethesda versions11/34/83, checking omitted defaults and supplied values.

`S3/native-partition-defaults-01` passes all1,923 components and629 engine tests.
`native-engagement-contract-sanitized-02` passes410 selected component tests
(including the new parser test) and227 selected engine tests;
`native-engagement-engine-sanitized-01` separately passes all629 engine tests
with leak detection, ASan and UBSan enabled. Exact inventories/no skips.
Tested dirty-source fingerprint (including pending combat-state changes):
`05bc8861c749fa42555367353f5e86983609ba9bf2fa72c5623cba2e36ec0b3f`.
The earlier `native-engagement-contract-sanitized-01` completed400 native
component cases but was interrupted after engine build; it is not a full pass.

Actual corrected graphical replay `native-engagement-runtime-sanitized-02`
passes without sanitizer findings; graphical leak detection is disabled.
Engine SHA `26b6431fa6e511aab6f2b27611c99e707f9a167ea8d015fea6d00287f811948a`;
save SHA `37b1aad202a16cdb8b554f545f38a5810673e7680b4c5324cc8d34d9a860d128`.
Capture inspected: dry fixture wall, normal HUD, no breath meter. The inherited
capture filename contains “water” but the fixture is dry. This silent course
proves loading and persistence, not visual combat or audio acceptance.

### Native combat membership persists with lifecycle cleanup

Runtime schema15 stores symmetric combat membership once per canonical actor
pair. Both endpoints require native values and nonterminal lifecycle state.
Validation rejects self/reversed/duplicate/noncanonical/dangling/dead endpoints,
oversized counts and truncation. Versions1–14 deliberately migrate to empty
membership. C++, canonical JSON and Python binary readers/writers agree.
The service exposes engage/stop/query/opponent enumeration; prepare-and-swap
restoration and engagement preserve prior authority on failure. Stopping one
actor preserves unrelated pairs. Death removes membership before callback-time
capture; essential knockout keeps it, and revival does not invent new pairs.
Membership does not represent selected attack targets, attack phases or legal
eligibility; its callers must supply those policies in later chunks.

Three new component, three engine and three Python cases cover these contracts,
including actual Player health publication, rollback and callback-time saves.
`S3/native-engagement-contract-01` passes1,922 components,629 engine and180 Python
cases, exact inventories/no skips; fingerprint
`781b606960e2840a9a0cfcb5445dd8a26ba5e8cdca607f8c70a99efb9337d6db`.
The later full regular/sanitizer runs and NIF repair are recorded above.
Python fixture-development failures remain in `authority-draft/engagement-python-
preflight-01.log` and `-02.log`; corrected focused run `-03.log` passes31 cases.

Actual regular engine SHA:
`0691db736410b01024768692ce2da757618aacca3c75cc5585a70c9e16635252`.
The dry lifecycle fixture SHA is
`8c47f944cfdd75bcd558e7ebf5ed74983a71c2aa4800a37cfea1506c4aba52b1`.
Starting from the retained v14 dry save, the setup injects only one NPC/player
pair; paired pristine SHA:
`704992b54dbb1db54b40146f7b011b01da26d085a2aa548c036ae2f48628ad3e`.
All course directories are under S3 and contain input/state verification:

- `native-engagement-runtime-01`: exact pair survives initial load/resave;
  saveSHA `c2e8c061c9d3f243efcd234148a96846c539315c965105ad235439fd310d7268`.
- `native-engagement-continuation-01`: unchanged prior save starts a fresh
  process; saveSHA `dcd6a013f0029b2988fea0ce50c1f1aa30daa195cf84fb4009a68d572ab20a8f`.
- `native-engagement-legacy14-01`: actual v14 load/resave becomes15 with empty
  membership; saveSHA `8d0a3d94582a9d9bdce7189ae9aebc374939bfeca964ddfb5d2b6183ec611ee9`.
- `native-engagement-reject-01`: changing only the serialized first endpoint
  to a missing actor rejects before application; no save is written and input
  bytes remain unchanged. CorruptSHA
  `cb636c31eb60f0906e3e9e9c0b23909ec79a11cbb4f2d5a66d2fbba335c2be4d`.
  Editable manifest: `oblivion_m15_native_engagement_reject.json`.
- `native-engagement-morrowind-01`: normal Morrowind startup/save passes with
  no native markers; saveSHA
  `28375621d725266074ec57032ed3f8c00ff915f8c5a1c59a1e5e99352088da33`.
- `native-engagement-runtime-sanitized-02`: exact paired persistence under the
  repaired sanitizer engine, hashes and limitations recorded above.

Captures inspected: native dry wall/HUD, explicit invalid-save error dialog,
and Morrowind startup rocks/HUD. No audio or combat initiation is proved.
Automatic publication remains off; existing fixture ModAV Health0 commands
explicitly adopt actors. Native public-query integration remains next:
`native-combat-query-red-01` retains the actual failure where IsInCombat returns0
for both paired actors and the original player-reference alias. Whole-world
failed-load preservation, rest, Lua writers and S2/S3/later stage gates remain open.

### Native combat query adapters

World now exposes optional native combat queries: absence selects the legacy
profile; native false never falls through to AiSequence. Player combat status,
nearby fighting actors, native ObScript IsInCombat, and loaded/unloaded AI
condition contexts read the same service. The original player-reference alias
resolves to the runtime player. Nearby native opponents bypass TES3 disposition
and Fight calculations. Unengaged hostility/pursuit, original package gates and
full native rest-range policy remain S7/S10 work; the shared nearby list currently
uses the existing actor processing range. This is membership integration, not
complete native enemy classification or autonomous combat.

`S3/native-combat-queries-02` passes all629 engine and180 Python tests;
`native-combat-queries-sanitized-03` passes all629 engine tests with ASan, UBSan
and leak checks, exact inventories/no skips. Source fingerprint:
`eae5b63990c6c8cfaae09531e17bb92643db3b47f3948016e20b66b814426bb6`.
Regular engine SHA `4b5f34f8f17610f5cfd27ecfe25cab1a17c706ce4d7148fe38c484faca87fdef`;
sanitized engine SHA `1a83c9f15a448fa9987b651515669c532db2691e46c3ff87c0917fe3f5ba6d4d`.
The first regular build caught an auto-pointer deduction error against
Environment's NotNullPtr; corrected before passing. Sanitizer build01 was stopped
for that correction, build02 for increased parallelism after the regular build
finished. They are retained, not counted as passes.

Actual courses use the dry lifecycle fixture and explicit ModAV Health0 adoption.
The three editable `oblivion_m15_native_combat_query_{paired,unpaired,terminal}.json`
manifests and two event lists are committed. Independent state checks compare
binary save state with C++ report output, identity hashes, lifecycle and action
namespace. Evidence directories under S3:

- `native-combat-query-paired-01` / `unpaired-01`: NPC, player and player alias
  report1/0 respectively; membership persists exactly. Save SHAs
  `f3e497725ab349b81ad47204ae4b8d307034e6154f0465a157e8578aeab6aff0` /
  `31b635191c3125c31470baca1e61bc04ebade45524cd3a40f6f5a48155f2e385`.
- `native-combat-query-continuation-01`: prior paired save starts a fresh process,
  all three queries still1; saveSHA
  `147c41ab5e319f256363405ab5ce6ff9dc7a3960dd93b6ef7a77ab8f3f39d87f`.
- `native-combat-query-terminal-02`: one NPC Kill callback, dead NPC/alive player,
  both memberships removed and all three queries0; saveSHA
  `86aca8ae6e8385b101ebccdcc2679b36f4042383b0fb330e6e70f05efa8938dd`.
  Earlier terminal01 correctly rejected because the inherited pristine had
  next_death_event=UINT64_MAX. Its failed scenario/query1 results remain; the
  successful fixture changes only that counter to1 with empty queue/counts.
- `native-combat-query-paired-sanitized-01` / `terminal-sanitized-01`: same actual
  cases pass with no sanitizer findings, graphical leak checks disabled.
  Save SHAs `1a7cd679d1b1e986cf01d68643f6b55b43d24efeeb8f091408d516f39ca54231` /
  `306a16789aa5313e901f5f6466682fc4507bd7b81d565d757fc0cc96bb538796`.
- `native-combat-query-ai-paired-03` / `ai-unpaired-03`: actual EvaluatePackage
  chooses a stationary Wander package guarded by IsInCombat==1, or its
  unconditional fallback. Resulting package identity persists; it is not
  injected in setup. Diagnostic draft builder/manifests live in authority-draft;
  pluginSHA `5622279f714bebb1ab4484330c895337c8c6d85cbaf7f2da7f469d4311d61938`.
  Retained earlier attempts caught a content fingerprint mismatch, an Xvfb
  startup failure, and a stale unselected package without explicit evaluation.
  These cases prove loaded condition evaluation; unloaded runtime acceptance
  and autonomous selection remain open.
- `native-combat-rest-refusal-01` / `rest-available-01`: normal T input is blocked
  with the pair and opens the dialog after removing only membership from the
  same full-resource source. No native-to-TES3 cast occurs. Inspected captures
  show no dialog versus a rest dialog; no readable refusal notification was
  captured. This is not full rest/UI acceptance.
- `native-combat-query-morrowind-01`: normal Hearth Heal input changes Health
  25->80.8487015 and Magicka100->87, with unchanged maxima/no native save markers.
  Before/after SHAs `166a0a27b4382fb5738b0d7803246fb2baac9cd5a4a742714aa01b4558287a4c` /
  `6dda4480a5f51bcea731573a76c0d281546bedca50687738bbf1149b3bac2655`.

All successful runtime captures inspected: native wall/HUD, rest dialog, and
Morrowind rocks/casting hands/HUD. Silent cases do not prove audio or attack
animation. Automatic publication stays off. No stage gate changes.

The next failures are `native-rest-dialog-red-02` (Until Healed) and
`native-one-hour-rest-red-01` (normal click on Rest). Both now reach the legacy
`fRestMagicMult` lookup and fail without healing: both actors retain Health25 /
Stunted Magicka20. No rest hour is advanced. Captures also expose unresolved
sRestMenu2/3 captions. The pinned original sleep_wait_menu.xml has Rest/Wait and
Cancel buttons, no Until Healed action; its asset provenance is recorded in the
native-rule document. Next: native rest UI/caller policy and resource dispatch,
then remaining Lua writers/activation and the other open stage gates.

### S3 native fixed-hour wait presentation

The next bounded UI change follows the pinned original `menus/sleep_wait_menu.xml`
(SHA256 `563583496adc3ca916b1f3dd03481a2e311b417518f752de9ae98d7230348b4b`,
from Misc BSA `a84290011a4ed5a1ca8d8cff3822eda67bbacd47c750744b7d786cc0a5527ca5`).
That menu has fixed-hour Rest/Wait and Cancel actions, no Until Healed action.
Native keyboard rest now opens Wait; sleeping requires a supplied bed. Native
captions use engine-authored Interface localization and proper hour pluralization;
this is functional shared presentation, not implementation of the original XML UI.
The hidden automatic-duration callback also rejects native invocation.

`S3/native-rest-ui-01` builds openmw/openmw-tests/esmtool and passes all629 engine
tests, no skips. Tested source fingerprint is
`122c416fbd45180e7269d5e3f5e7dea0782910ee864503e146d894700534edc2`.
The actual graphical binary SHA256 is
`929d37afb53c0493e0acb7d9222914b4ff8ac03077fd45c82ea2cea899049ab3`.
`native-wait-dialog-02` uses the tracked native wait-dialog manifest, injured
registered Player/NPC input (pristine SHA256
`ffc9345176fef60e1b0c3eccf4aa2e5c238aaf1cc5cd358b6aeb09e46fec6f91`), normal T,
Up and Escape input. All four captures inspected: readable Wait prompt, 1 hour,
2 hours, no Until Healed, closed dialog after cancellation. The actual resave
`97f25911e36fdee917d452c9ac0ab3101de2f9d1d36c31d9c55b5e200e4ece62` preserves life,
combat membership, physical actions and death counts; clock advances only ordinary
frames, not a rest hour. `-01` is the passing single-hour precursor.
`native-wait-morrowind-01` loads the prior injured Balmora magic-course save and
opens/cancels the legacy Wait UI; its inspected capture retains the legacy illegal
rest prompt and captions. This does not exercise Morrowind Until Healed or actual
hourly restoration. All courses have no unreviewed/forbidden errors; silent Xvfb
captures are not audio acceptance.

Native hourly restoration remains the next failing writer. This UI commit does
not close that case, bed activation, native rest refusal text, level-up integration,
automatic actor publication or any S2/S3/later gate.

The fixed-hour menu change is committed as `2930f7a9d0`.
`S3/native-rest-ui-sanitized-01` subsequently builds the instrumented engine and
passes all629 tests with ASan leak detection and UBSan halting enabled.
Actual graphical `native-wait-dialog-sanitized-03` passes with leak detection off
and UBSan/ASan halting enabled. All four captures inspected, including2 hours;
cancellation preserves life, engagements, actions and death counts with no rest
hour advanced. Executable SHA256
`34ac62ca32d1ee50d9102258fc7f269646e6b91fcada2a4b0db96e725f29ad5b`, resave
`fcdcf2ccb96ef7379cd47f1c0c5a2b6128d03193444a8d562a048bb8d5d7a377`.
The retained `-01` failed on resource revision mismatch (regular resources with
sanitizer binary); `-02` had a nominal harness pass but screenshot showed the
brief Up press was missed. The fixture now uses held keys and a rendering pause.
This is a fixture correction, not a waiver of screenshot review.
The corrected held-key fixture also passes regular `native-wait-dialog-03`;
all four captures inspected (1 hour,2 hours,cancel). Reusable build/oracle skill
references now document matching resources, held input, Steam initialization
and absolute launch paths; both skill validators pass.


### S3 original hourly wait observations and remaining failing writer

`S3/original-rest-02` uses the pinned original 1.2.0416 executable
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`
and master-only isolated prefix. Normal T/Wait input, held mouse actions and
console queries establish these observations for Chorrol Guard028c83:

- High process, character generation disabled: at maximum Magicka1000,
  current100, fMagickaReturnBase0.1, fMagickaReturnMult0, TimeScale30, one hour
  produces225.03. The predeclared122-second restoration plus3.036 seconds of
  ordinary frames predicts225.036 (error0.006, tolerance0.5).
- Before disabling character generation, the diagnostic case produces221.81,
  consistent with120 seconds plus ordinary frames. Read-only process inspection
  verifies Player+6E5 changes1->0 after SetInCharGen0. This first observation
  discovered a precondition; it is not a predeclared acceptance case.
- High process, injured NPC/Player: Health25/25 becomes127/80, their queried
  maxima. Stunted Magicka1 preserves Magicka100. Predeclared tolerance0.02.
- Moving to TestingHall leaves the guard in MiddleHigh process tier1: Health
  stays25 while Magicka100->227.17. PurgeCellBuffers changes to Low tier3:
  Health again stays25, Magicka100->223.14. These Magicka amounts are exploratory;
  exact per-actor clock cadence was not controlled. Distant actors must not be
  excluded from all regeneration merely because they lack High processing.

Before/menu/after captures for every case were inspected. The read-only process
snapshots identify actor, process vtable/tier getter, update flag and clock;
no original memory was written. `rest-observation.json` retains displayed values,
calculations and failed setup commands. The original exited via QuitGame.
`cleanup.json` verifies exact restoration of isolated Plugins.txt, INI,
quicksave and autosave; the owned reference Steam client was shut down.
No sleep/bed, creature, dead, essential, AI-disabled or reentry acceptance is
claimed by these waiting cases.

Tracked `oblivion_m15_native_wait_hour.json` reproduces the current OpenMW
failure in `S3/native-wait-hour-red-01`: confirming Wait reaches missing TES3
`fEndFatigueMult`; no full hour advances. Inspected captures show the one-hour
Wait dialog and return to the world. Source save remains pristine; failed output
SHA256 is `474dad5dc55422d3f6de8a26d24cee270c681514232824c037233bc7c15dd8dc`.
The retained state verification records all actor values and clock. Next is
native hourly resource dispatch with unloaded authority and resident projections
committed together, followed by real Wait and fresh-process verification.
S2/S3 and all later gates remain open; automatic actor publication remains off.


### S3 atomic multi-actor resource restoration

`OblivionCombatService::restoreResourceBatch` prepares ordered resource updates
for Player, resident NPCs/creatures and unloaded owned actors before publishing
any values or projections. It requires initialized lifecycle, leaves terminal
actors unchanged, validates every duration and rejects missing/duplicate actors,
empty actor requests, and invalid/duplicate/unaffected residents. Processing,
clock, effects and rest/jail eligibility remain caller-owned; no pointer persists.
Separate updates retain their individual float stores instead of merging elapsed
durations. The caller must supply every affected resident nonplayer projection.

The new engine case exercises the independently observed120+2 versus120-second
NPC restoration, Player3600-second restoration, immediate resident stat agreement,
unloaded capture/restore, six late transaction failures with rollback, separate
float-store rounding, dead actors with positive stored Health, and missing life.
`S3/resource-batch-red-01/tests-corrected.log` proves the unimplemented API fails.
Retained earlier fixture failures include a scaled-NPC setup crash without a
world (GDB identified the construction lookup), initializer/query compilation
errors, and `resource-batch-01` missing the unloaded content reference. Those
fixtures were corrected explicitly, not treated as implementation acceptance.

`S3/resource-batch-02` and `resource-batch-sanitized-02` each build
openmw/openmw-tests/esmtool and pass all630 engine tests, no skips. ASan leak
detection and UBSan halting are enabled in the sanitizer run. Both use source
fingerprint `a8083f139ac119c9c681119aa1ae1ff82b75d5eb2c6e24b36cf27046501c0ba6`
on parent `5a0e2e6d4d`. `resource-batch-sanitized-01` was stopped after the regular
fixture failure so it could not validate outdated sources. This is service
coverage; hourly GUI integration and actual Wait/restart remain the next task.
No stage gate or automatic publication policy changes.

### S3 native hourly Wait integration

The Wait dialog now dispatches native resource restoration before advancing one
hour and fast-forwarding native AI. Player restoration uses3600 seconds; enabled
registered NPCs use3600/TimeScale with an additional2-second Health/Magicka/Fatigue
update for nearby resident actors. Chargen suppresses the explicit Player/nearby
updates; disabled/deleted references are excluded. Zero/negative scales provide
zero ordinary elapsed time. Jail remains explicitly unsupported here. Native
Wait no longer enters TES3 rest formulas, sleep encounters or level-up completion.

All paths below are under `build/oblivion-compat/m15/S3`. `native-hour-03` passes
630 engine and180 Python cases; `native-hour-sanitized-01` passes630 engine cases
with leak detection and UBSan halting. Tested source fingerprint on parent
`38412c9bb2` is
`2d94371c04161041647fc2dff1581c78034895974e9ca9c0a5c3477b161b5c65`.
Regular runtime executable SHA256 is
`eb7f1138d6e4f6b2b2d91bbf0cedc8429d3968cdb710503223427f2444f4d75a`;
sanitizer executable SHA256 is
`93971279dfd55bb86feb833e6cb9b83f6c2e8bca5c5885020eb46e33eee9933f`.
Graphical sanitizer cases disable leak detection, retain UBSan halting and use
the matching sanitizer resources directory.

Independent saved-state checks and direct screenshot review pass:

- `native-hour-runtime-01` and `native-hour-continuation-01`: ordinary Wait
  restores both Health25->100, preserves Stunted Magicka20 and lifecycle/action
  state, then survives a fresh process without replaying the hour. These earlier
  runs used `native-hour-02` executable
  `56faf47ec6581191d4cff2eb90edb354fe76d2e31ebc3181b225dff56109dd4d`.
- `native-hour-morrowind-01`: actual legacy Wait advances one hour, preserves
  Health25/Magicka100/Fatigue200 and has no native records. Same earlier binary.
- `native-hour-zero-01`: zero TimeScale still restores explicit Player/nearby
  resources and advances exactly one game hour within float precision.
- `native-hour-chargen-02`, `native-hour-distant-03`, `native-hour-disabled-02`:
  chargen leaves both Health25; distant/disabled NPC Health25 remains unchanged
  while Player reaches100. Stunted Magicka20 remains unchanged throughout.
- `native-hour-magicka-high-02` and `native-hour-magicka-distant-02`: with
  NPC maximum1000, Willpower50, initial Magicka100, TimeScale3600, the independent
  formula is100+17.5*(saved clock delta+2 for nearby, +0 for distant). Errors are
  0.000011 and0.000260, below the predeclared1-point tolerance.
- `native-hour-sanitized-runtime-01`, `native-hour-sanitized-magicka-01`, and
  `native-hour-sanitized-continuation-01`: normal Wait, numerical Magicka and
  fresh-process continuation pass without sanitizer errors. Numerical error is
  0.000267; continuation clock delta0.070532 hours proves no repeated rest hour.

Every accepted directory retains pristine/resaved state, launch inputs, binary
identity, scenario logs, state-verification and visual-verification JSON. NPCs
are offscreen; their numerical acceptance comes from saved state. No audio
acceptance is claimed. Editable native continuation and legacy Wait manifests
are tracked beside the ordinary native Wait manifest.

Retained failures: `native-hour-01` failed NotNullPtr type deduction;
`native-hour-zero-red-01` rejected zero scale before correction. Distant-01 used
an ineffective100-unit setting (engine minimum3584); corrected fixtures place
NPC x4096 and update its AI last valid position. Chargen-01/distant-02 captured
stale HUD before projection; corrected captures wait0.5 seconds. Disabled-01
attempted an unsupported resident-only NPC ModAV. Magicka high/distant-01 failed
the numerical expectation by about17 points because zero-write native projection
was delayed until1 second. Their corrected fixtures bootstrap at0, preserving
both the original expectation and tolerance. These are explicit setup changes,
not acceptance of automatic actor initialization.

Remaining gates include automatic publication, per-actor clock cadence and
100000-second manager-clock wrap, beds/sleep/refusal policy, creature/dead/
essential and truly unloaded runtime cases, AI-disabled behavior, native Lua
writers, unloaded ModAV, recharge integration and failed-load world preservation.
Current nearby selection uses the fork's processing range; complete original
process-tier eligibility remains to be integrated. No S2/S3 or later gate closes.

### S3 stable-key unloaded actor commands

`executeUnloadedValueCommand` validates winning content identity and updates
native values without constructing a live actor. Mod/Force retain the existing
channel/eligibility/alias rules. Set updates the shared base and all owned
references, preparing supplied resident siblings before committing. A supplied
resident target is rejected. Negative Health uses the same atomic lifecycle,
combat-membership and death-event transaction as resident commands, with no
resident projection required. Later publication projects committed values/life.

The new engine case destroys the target live reference, applies Mod/Force/Set,
checks sibling stat agreement and invalid-input rollback, kills the unloaded
target once, clears both combat memberships, and round-trips native values and
the pending death event through binary persistence. Repeated negative Health
does not duplicate the death count/event. A changed winning reference is rejected
without changing restored values; later live publication retains terminal state.
`S3/unloaded-commands-02` and `unloaded-commands-sanitized-01` pass all631 engine
tests with no skips. The latter enables ASan leak detection and UBSan halting.
Tested source fingerprint on parent `24f6b1651d`:
`807c70ec47e798d9782174282b91dcc982052122e5a8ce5e2e4b0324afceee4a`.
`unloaded-commands-01` passed the initial631 cases before persistence/content
rejection coverage was extended. These are service checks, not world acceptance.

`S3/unloaded-command-runtime-red-01` preserves the world/script integration
negative control: disabled registered NPC ModAV Health -5 still errors because
the adapter requires a resident actor. Saved Health remains25 instead of the
predeclared20. The after-rest screenshot was inspected; the Player restores but
the disabled NPC is offscreen. The prepared stable-key adapter remains the next
step. Legacy lifecycle adoption for unloaded actors and remaining S3 gates stay
open; automatic native actor publication remains off.

### S3 world/script commands for registered disabled actors

ObScript Set/Mod/Force now routes a registered target by stable key. The world
uses the resident adapter when a live reference is registered; otherwise it
validates initialized lifecycle and winning actor type/essential flags, gathers
resident siblings for shared-base writes, and invokes the unloaded transaction.
It does not load a target cell to perform the mutation. Unregistered actors keep
the previous path; older unloaded entries without lifecycle still require
explicit migration. No automatic-publication gate changes.

`S3/unloaded-world-01` passes631 engine and180 Python tests;
`unloaded-world-sanitized-01` passes631 engine tests with ASan leak detection
and UBSan halting. Both tested the adapter fingerprint
`3566a9095d31c6fa39d82e75727828ae1d1d612995bed9e2ac9b3ddb49e16d46`
on parent `c94f80f80e`. After editable manifests were promoted,
`unloaded-world-manifests-01` passes all180 Python tests again.

Runtime input is `native-hour-disabled-02/pristine.omwsave`, SHA256
`2c24c80fcd4da4747468c811a5058fc0a77515ee4eaa4cc9b703b747ebe739b0`,
using the previously identified lifecycle plugin. Normal-input Wait/save checks:

- `unloaded-command-runtime-01` fixes the retained red case: ModAV Health -5
  changes disabled NPC Health25->20. GetAV still returns raw base100; Wait
  leaves NPC20 while Player reaches100.
- `unloaded-command-sequence-01` executes Mod -5, Force40, Set200. Expected
  transitions25->20->40->140 were declared before launch. Saved base200,
  Script15, Damage-75 and current140 match exactly; disabled GetAV returns200.
- `unloaded-command-continuation-01` loads that actual result in a fresh process
  and preserves every actor-value channel, shared base override, lifecycle,
  death counters and pending actions. Only running time advances0.068233 hours.
- `unloaded-command-sanitized-runtime-01` and
  `unloaded-command-sanitized-continuation-01` repeat the sequence and actual
  restart with instrumented openmw, matching resources, ASan leak checks off
  for graphics, and UBSan halting. They pass with no sanitizer errors; restart
  advances0.070886 hours and preserves the same exact state.

Regular executable SHA256:
`7158e0fb78f186e3b03b2819b5acc2083c97b3164c43235ac585e96a0bf98c3d`.
Sanitizer executable SHA256:
`f8d40a243601a5cdf82f7f179cc474e4a2b61e2d95a4c8597461260d8b7c6420`.
Each directory records input/output hashes, predeclared expectations, independent
saved-state verification and direct screenshot review. Captures show the readable
Wait UI and Player resource restoration/preservation. Disabled NPC is offscreen;
its values are checked in saves. Audio is disabled. A disabled reference in the
loaded cell is not acceptance of travel to another unloaded cell, creature or
essential command execution; those runtime branches remain to be demonstrated.

The editable sequence/events and continuation are
`oblivion_m15_disabled_value_commands.json`, `m15_disabled_value_commands_events.txt`
and `oblivion_m15_disabled_value_continuation.json` under
`scripts/data/oblivion_compat`. Copy the input into a private M15Legacy slot;
never launch against the historical source save. The build/test skill now
records this setup and exact expectations. Remaining S2/S3 and S4-S14 gates stay
open, including native Lua writers, lifecycle adoption/activation and clock wrap.

### S3 Lua resource-current authority bridge

Lua dynamic Health/Magicka/Fatigue `current` requests on registered native
projections now route through the combat service. The service converts public
current units to a Damage-channel delta (including NPC Magicka scaling), keeps
base/Maximum/Script unchanged, and applies native healing caps, fatigue policy,
god mode and atomic health/lifecycle transitions. Requests above the recoverable
pool may clamp; reads after the delayed Lua write flush return the applied value.
Lua base/modifier/attribute/skill writers remain guarded; automatic native actor
publication remains off.

`S3/resource-current-02` passes632 engine and180 Python cases.
`resource-current-sanitized-01` passes632 engine cases under ASan with leak
checks enabled and UBSan halting. Tested dirty source on parent6f249dcdbf:
`a7b8a7d31dce0e44f5c2663c7701b55e878741bb64c1270bf931ed17c6d8a07b`.
The retained first attempt failed three fixture expectations for sparse Health
and Player Magicka scaling; those expectations were corrected against the
existing native provenance, without changing implementation to fit them.

Actual Lua `lua-resource-runtime-01` and `lua-resource-sanitized-runtime-01`
pass all six writes/readbacks and independent saved-state checks: NPC Health35.5,
Magicka22.5 (raw Damage-85 at1.5 scale), Fatigue100; Player Health40.25,
Magicka17.5, Fatigue200. Maximum/Script and unrelated values/lifecycle/actions
are preserved. The old-binary negative control `lua-resource-runtime-red-01`
rejects both queued writers at the native projection guard. It intentionally
fails the scenario; the independent negative-control report checks the exact
failure and preserved Health/Magicka. A failed marker wait does not prevent
later scenario F5, so this is not an unchanged-private-save assertion.

Both accepted runs have directly inspected before/after-save captures showing
the Player bars and readable room. The NPC is offscreen. Graphical sanitizer
leak checks are disabled, UBSan halts, and audio is disabled.

Regular runtime executable SHA256:
`969182824f41fec8b31fc957b04fd400f68ed8326063ca66b3ac15d030642c39`.
Sanitizer runtime executable SHA256:
`382bfa8b028c69aef399312570da77f3b965c8a2dd0a410f1185e2de0c435d5b`.

The fresh-process `lua-resource-continuation-01` correctly **fails**: Player
Lua onSave state restores, but native NPC local Lua state is lost and its custom
script queues the writes again. Equal final resource values cannot hide that
replay. This exposes a native-reference Lua persistence gap, which must be fixed
before restart acceptance. Editable current/continuation manifests are
`oblivion_m15_lua_resource_current.json` and
`oblivion_m15_lua_resource_continuation.json`; the latter remains a failing
regression course until local Lua persistence is implemented. This chunk does
not close S3, Lua writer coverage, or the remaining M15 gates.

### S3 native local Lua save codec

Added a version1 optional native local-script companion section for LUAM, keyed
by stable reference FormKeys. It preserves binary Lua state, both named timer
clocks and binary callback names/arguments, including empty strings and past
deadlines. Explicit owner terminators, canonical nonnull/nonplayer keys, unique
nonnegative script IDs, exact timer sizes/types, finite deadlines and aggregate
owner/script/timer/byte limits prevent ambiguous or unbounded reads. Save preflight
validates the complete section before emitting it; load returns temporary state.
Legacy LUAM without the section remains empty. The codec is not yet called by
LuaManager in this chunk; the retained NPC continuation still fails until wiring.

`S3/local-lua-codec-01` passes the initial8 focused cases. The final
`local-lua-codec-02` passes all1932 component tests and409 selected ESM4 sanitizer
cases (ASan leak checks off, UBSan halting), including9 native Lua codec cases.
Coverage includes binary round trips, empty/legacy state, unknown versions,
duplicate/noncanonical owners and script IDs, malformed timers/terminators,
every byte truncation inside a populated section and aggregate limits.
Tested source on parent6b070911b7:
`8ac7daf56d893e78c2ba368b7b265423120df8c71654cba7a32688236eea651a`.

### S3 native local Lua persistence and remapping

LuaManager now owns native reference-local Lua persistence through the version1
LUAM companion. Its real save path calls local onSave handlers; diagnostic T4ST
capture does not. The Player retains its existing save path. Read validates a
temporary map and winning content-reference types, rejects foreign-profile state
and corrupt serialized Lua data, and normalizes script IDs and object userdata
before holding pending references. Scene activation and custom add/has/remove
operations restore pending containers before deciding whether scripts are absent.
Unconsumed state survives resave without running the NPC script. Clear drops the
pending map; Lua reload remaps it against the new script configuration.

The shared script configuration now distinguishes an absent mapping from a
mapping whose scripts were all removed. Removed scripts cannot inherit unrelated
scripts' numeric IDs. Saved-ID bounds are checked separately from intentional
removal. A container can load already-normalized IDs without mapping them twice.

`S3/local-lua-persistence-03` passes all1936 component,632 engine and180 Python
tests. `local-lua-persistence-sanitized-01` passes all1936 component and632 engine
tests with ASan leak detection enabled and UBSan halting. Both tested source
fingerprint `76e0ff2debf598a34ee67638d1460f0d21a95b0b30b67d1a0d9522f73e4597f9`
on parent `c6686c0eac`. The earlier01/02 builds passed their inventories but real
restart exposed two integration errors: consuming NLSV with isNextSub before its
reader, then comparing raw TES4 metadata to internally flagged class types.
Both failures remain as `lua-persistence-continuation-01/02`, with independent
proof of rejection before native application and unchanged inputs. The fixes use
peekNextSub and raw ESM4 FourCC validation, with additional component coverage.

Regular actual-runtime evidence:

- `lua-persistence-runtime-01` writes all six resource values and a native NPC Lua
  state blob. `lua-persistence-continuation-03` loads it in a fresh process: both
  actors restore exactly once, with no queued/committed resource writes, preserving
  all actor channels, lifecycle, actions and counters. Publication setup is now
  explicitly at time0, before restored onUpdate reads; automatic publication is
  still gated off.
- `lua-persistence-split-runtime-01` and `lua-persistence-disabled-02` reverse two
  unchanged omwscripts configuration files. The disabled NPC never activates;
  its pending script ID changes30->29 while its path/data/timers remain exact.
  The first disabled attempt edited a configuration file and correctly hit the
  content fingerprint guard; `disabled-01` is retained, not accepted.
- `lua-persistence-timers-01`, `timers-continuation-01` and `timers-no-replay-01`
  save two named timers per actor, then reverse two independent native masters.
  Both clock callbacks fire exactly once, and saved object userdata still equals
  the actual owner. Each actual save invokes onSave once per actor; persisted
  counters advance1->2->3. The second save has zero pending NPC timers; a third
  process restores both fired flags and delivers no timers or resource writes.
- Reordering the first master selected a new Bendu_Olo save directory. The initial
  continuation verifier accidentally read the unchanged M15Legacy input, with
  elapsed time0. Those superseded reports are retained as `*-wrong-slot.json`.
  Correct verification requires exactly one changed Quicksave, positive elapsed
  time and the actual saved Lua section. The verified timer continuation output
  SHA256 is `00d7879ef1cbbc3551ab49dd0ae6011163e7fecf01b3fb9ff462b1abbf5c2568`.
- `lua-persistence-reject-{script-id,binary-data,base-owner,duplicate-owner}-01`
  rejects each intended malformed input for the exact expected reason before
  native application, without writes/replay and with source/private hashes intact.
- `lua-persistence-morrowind-01` performs ordinary Wait and save: clock advances
  1.077969 hours, injured Health25 stays25, and neither native runtime records nor
  an NLSV subrecord appear. `lua-persistence-reject-foreign-02` rejects an empty
  native Lua section inserted into TES3 LUAM. Its first attempt retained the
  intended rejection but failed because the draft dropped the established
  missing-weather-model baseline classification; the corrected course preserves
  those existing patterns and the exact expected load error.

`lua-persistence-sanitized-timers-01` and
`lua-persistence-sanitized-timers-continuation-01` repeat actual writes/save,
content reordering, userdata restoration and both timer deliveries using the
instrumented engine with its matching resources. They pass independent resource,
native-Lua and callback-count checks. Graphical leak detection is off; UBSan
halts. Final regular runtime executable SHA256:
`a9f44acb9a30e9631c6f8106b7a3aa112ccdf57a8854d55b754f71c3a471b964`.
Sanitizer executable SHA256:
`57e5c9f5f244e748311c126e503a4641a4779257d17574e38f9eef9c83e6a0a2`.
Both were identified before their launches. Accepted captures were inspected
directly: room and Player bars, rejection dialogs, or the limited Morrowind Wait
view. The NPC is offscreen and audio is disabled.

Editable manifests now cover resource current/restart, pending inactive state,
timers/replay and malformed/foreign cases. The empty independent master has a
hash-pinned editable recipe `m15_lua_order_fixture.json`; the build/test skill
records reproduction, setup, expected values, save selection and loader pitfalls.
The promoted pending-enable course intentionally still catches a separate bug:
`lua-persistence-enable-01` acknowledges Enable but leaves the loaded NPC disabled.
Its pending Lua data survives and maps29->30, but the NPC never receives onLoad.
The script adapter misses disabled references outside the Ptr registry; fixing
that lookup and rerunning activation is the next bounded change.

This does not close S3 or M15. Dangling dynamic Lua owner validation, broader
native construction/lifecycle adoption and other Lua stat writers remain open,
as do prior-world preservation on failed load, the remaining resource/rest gates,
and S4-S14. A disabled reference in a loaded cell is not a travel/unloaded-cell
acceptance test. These explicit-script courses do not establish automatic native
publication or complete Morrowind/profile-teardown regression coverage.

The initial serializer-only runtime01 used executable SHA256
`d19736fbce4b37223deb6b353999cea67be8da3cc4e51cd6067914bb95fc9dae`; its valid output was subsequently loaded by the
corrected continuation03 binary above. After promotion, `local-lua-manifests-01`
passes180 Python tests and independently confirms that all10 editable manifests
expand identically to their executed drafts. The updated build/test skill passes
the skill-creator validator.

### S3 continuation: enabling resident disabled references

The script Enable/Disable adapter now resolves a resident reference outside the
Ptr registry before changing its actual RefData and scene membership. The former
Enable acknowledgement only changed a saved flag that capture then overwrote.
The retained `S3/lua-persistence-enable-01` failure demonstrates that defect.

`lua-enable-01` and `lua-enable-sanitized-01` each pass all632 engine tests on
source fingerprint `d67db207dfdac6b8c3925ee7114bf7bc06755c174002adcf1329acee59c851d9`
(parent `a903fa517d`). The instrumented unit run uses ASan leak detection and
halting UBSan. `lua-enable-manifest-01` passes180 Python tests.

`lua-persistence-enable-02` repeats the exact failed input and restores both
actors without resource writes, saves the NPC enabled, and preserves pending Lua
data across script-ID29->30 remapping. `lua-persistence-enable-continuation-01`
loads that output in a fresh process without issuing Enable and preserves the
same state. `lua-persistence-sanitized-enable-01` repeats activation with the
instrumented engine (graphical leak detection off, UBSan halting). Independent
resource, Lua-data and actual enabled-flag checks all pass. Runtime executable
SHA256: regular `1ee56db4872afcda9ff366baa21526873758474ffc0d8f97e2bfdb817c3bb144`,
instrumented `bec0469435b76e4bc4fd1cbee2e36a46354815ee36d3a6030243f4edfa36e1b6`.
All six captures were directly inspected: rendered room and Player bars, no
error dialog; NPC offscreen and audio disabled. The editable pending-continuation
manifest retains the no-Enable/no-replay restart check. This closes the bounded
resident Enable defect; the S3 and later milestone gates above remain open.

### S3 continuation: bind native Lua owners to saved references

Native local Lua companions now require every owner, including dynamic owners,
to exist in T4ST's reference vector. World invokes the Lua validation hook before
applying native state or preparing inventory replacements. A missing T4ST record
is treated as an empty owner set, preserving old saves without native scripts
while rejecting orphaned companions. Disabled/deleted references may retain
scripts without materialized pointers. Empty companions avoid indexing the
reference vector. Component coverage checks both missing owner kinds, valid
inactive owners and absent/empty companions.

The old executable genuinely accepted two inconsistent saves:
`S3/lua-persistence-reject-dynamic-red-01` replayed NPC resource initialization
for a nonexistent dynamic owner, and `lua-persistence-reject-missing-state-red-01`
loaded without T4ST then emitted NPC Lua assertions. Input SHA256s respectively:
`f4c1b96b0861e15f24cee6217be833a41e5ff37538de7557673220859bb697f0` and
`08f4167acc8fbe1ade0db69bcff2df65cbbbacb03800e34e05b856cc115d04b5`.
Neither is counted as a passing rejection.

Final `lua-owner-binding-02` and `lua-owner-binding-sanitized-02` each pass
all1937 component and632 engine tests without skipped/missing cases. Tested
source fingerprint `1c95bca87d16c39a6b117ec4f3063e4ba1a0989060f1a6769fd3af0c7b27cf3b`
(parent `bd6eb3c4e7`). ASan leak detection and halting UBSan are enabled for the
instrumented unit suites. The earlier binding01 runs also pass both suites, but
do not include the missing-T4ST guard; their evidence remains distinct.

`lua-owner-{regular,sanitized}-{dynamic,missing-state}-02` rejects the exact
retained inputs with the corresponding `Missing native local Lua owner` error,
before native application, script restoration or resource writes. Inputs remain
unchanged. The paired `writes-02` courses load an old absent-companion save,
perform the six expected Lua resource writes and save through the engine.
`continuation-02` fresh processes restore both actors without writes/replay and
preserve the NPC Lua companion exactly. Independent state checks preserve all
other actor channels, lifecycle and action state. The earlier
`lua-owner-binding-enable-01` also proves pending disabled activation and
script-ID remapping on the initial owner-validation build.

Final runtime executable SHA256: regular
`177ec033f69944284b762ee2d8bdcfb12a25d078afc63d10efa44f206458d17e`, instrumented
`f59a9ec9006e07996eb652a366af7fda4e69e399acf8d012a6c5c7e4a270bfb1`.
Graphical ASan leak detection is explicitly off, UBSan halts. All12 final
captures were directly inspected: exact rejection dialogs or rendered room and
Player bars; NPC offscreen, audio disabled. Existing editable rejection/current/
continuation manifests drive these courses. The build/test skill now includes
structural fault recipes, verified byte-for-byte against both retained inputs,
and passes skill validation.

This closes the companion owner-binding defect, not the complete failed-load
transaction requirement: StateManager still clears the previous world before
loading. Automatic native actor publication, remaining S3 lifecycle/adapters,
and S4-S14 remain open.

### S3 continuation: native Lua attribute/skill modifier adapters

Lua attribute/skill `modifier` and `damage` writes now request Maximum and signed
Damage changes through the combat authority. Base and Script remain separate;
all native float-delta rounding and sparse-storage behavior remains in the
existing writer. Player writes refresh derived pools through the existing
publication transaction. Resources and Script-channel requests are rejected by
this narrow API. Creature attributes are supported; NPC-only skill requests
cannot silently select a creature skill-group alias.

The read-only attribute/skill projection now retains native Script/owner/process
composition inputs as well as its current value. Queued Lua `modified` reads
substitute cached channels into native composition, retaining the Player versus
NPC float-store distinction and low-process exclusion of Maximum. Legacy reads
keep their existing zero clamp. The projection remains ephemeral; authoritative
save fields and schema are unchanged.

Three new engine cases cover all29 attribute/skill indices for Player/NPC and
both process levels, preservation/clearing of channels, invalid categories,
nonfinite requests, legacy/native queued reads, the 2^24 rounding distinction,
creature attributes, rejected NPC skill aliases and a finite subtraction overflow
that leaves authority and projection intact. Final `S3/lua-stat-modifiers-02`
passes635 engine and180 Python cases; `lua-stat-modifiers-sanitized-02` passes635
engine cases with ASan leak detection and halting UBSan. Tested source fingerprint
`3ba73d7ceb8f50ddd641a7de271465e2f0fffb6f57d5904cdb7724eedecda0aa`
on parent `01130cf3c9`. Earlier modifier01 runs pass634 cases before the
additional creature/overflow test and manifest promotion.

`lua-stat-modifier-red-01` reproduces the old guard failure for both actors.
The first new runtime `lua-stat-regular-zero-script-01` passes Lua assertions
but fails independent state comparison: the original fixture started with
injured Fatigue, which regenerated, and bootstrap materialized the zero Player
Health Script entry. That failure is retained. The corrected editable setup
fills Fatigue and predeclares the bootstrap zero; engine code was unchanged.

`lua-stat-regular-zero-script-02` and `lua-stat-{regular,sanitized}-script-02`
pass actual queued-read and committed-write assertions. The latter preserve
Speed Script7.25 and Blade Script-1.25 while writing Maximum2.5/3.5 and
Damage-1.25/-1.5. Independently decoded saves confirm only those four channels
per actor changed; resource channels, other AVs, lifecycle, actions and death
counts remain exact. `lua-stat-{regular,sanitized}-continuation-02` fresh
processes restore once per actor without writes/replay and retain the native
NPC Lua companion exactly. All10 accepted captures were directly inspected:
rendered fixture and Player bars, NPC offscreen, audio disabled.

Runtime executable SHA256: regular
`198ac908b5a91f15c2b84a2be1a0486418410017059cbefad58b137898129145`, instrumented
`e0177b826790c6b1a2c1452eeeefff948c90ac4df12bc4b5c70d1a90bbb039d9`.
Both were hashed before launch and verified unchanged after the test-only
addition. Graphical leak detection is off, UBSan halts. Three promoted manifests
are byte-identical to their executed drafts; the explicit setup recipe reproduces
both inputs byte-for-byte. The build/test skill records the recipe, values,
comparison scope and fixture pitfall.

This closes this pair of Lua property adapters. Base writes, dynamic-stat
modifiers, skill-progress semantics, automatic publication, broader lifecycle
adoption, load transactions and the remaining M15 gates stay open.

### S3/M14 regression: classify native pathgrid object links

The store's PGRL classifier compared raw TES4 FormKey metadata signatures with
OpenMW's internal record IDs. Direct DOOR/FURN links and placed REFR/ACHR/ACRE
links consequently fell through to Other. Native AI's foreign-edge door lookup
requires Door, so it could miss the connection's door before checking access.
The seven comparisons now use ESM4 raw signatures; typed-store dispatch keeps
its internal IDs.

A new engine test loads synthetic TES4 bytes through the real reader, stable-key
index, winning typed stores and ESMStore::setUp. It checks15 links: direct and
placed door/furniture, other objects, null/missing bases, missing objects,
deleted bases/references, actor references, independent-master local-ID collision
and a placed-reference override whose master order differs from load order.
The pre-fix test reproduces10 wrong classifications in
`S3/authority-draft/pathgrid-store-red-test-03.log`. Earlier attempts retained
there had fixture/API errors and are not defect reproductions.

`S3/pathgrid-object-kinds-01` and `pathgrid-object-kinds-sanitized-01` each pass
all636 engine cases, no skips, matching inventory/XML. Sanitizers use leak
detection and halting UBSan. Both record source fingerprint
`40f085f253cdd641fe8f6822c5aac0fcf9a74aacd868df1b03e7a0c7780ee03d`
on parent `c76fc15dc2`. This verifies load-order classification, not actual
pursuit/door traversal; those remain part of S7's gameplay gate.

### S2/S3 native manager and per-actor clock arithmetic

`components/esm4/actorclock` models the original manager setter's strict100000
reset/nonfinite handling and the common actor update's elapsed-time prefix.
The prefix preserves the binary32 .3 reset boundary and signed-zero behavior;
nonfinite per-actor timestamps are rejected as invalid input. Provenance records
actual virtual targets, instruction bounds, CRT execution and partial-path limits.
All186 recorded original-instruction outputs match C++ bit-for-bit.

Three added component cases cover manager bit patterns, rewind/initialization
boundaries, ordinary subtraction and invalid per-actor inputs. Full regular
`S2/actor-clock-01` and instrumented `actor-clock-sanitized-01` each pass1940
component cases with complete inventory/XML and no skips. The latter enables
ASan leak detection and halting UBSan. Source fingerprint
`f8d69b0074d45bd14a47004e34f5365e4b023b03b7954e1f2ec41403bb4b3d81`
on parent `5977e7ce71`. This is the verified arithmetic prerequisite; world clock
advancement, actor scheduling and persistent timestamps remain to be integrated.

### S3 native actor clock persistence (v16)

T4ST v16 stores the manager's binary32 time and a stable-key map of each actor's
last completed common update. C++, canonical JSON and the independent Python
reader/writer agree. Service capture/restore/clear retain/reset these fields;
failed restore and unsupported downgrade leave the prior service/target intact.
Versions1–15 retain manager0 and an empty timestamp map. Validation rejects
nonfinite times, times above the native100000 limit, duplicate/noncanonical keys,
unregistered actors, oversize collections and truncation. Finite negative times
remain valid, matching the original setter.

Three new component cases, one service case and three Python cases cover this
contract. Final `S3/actor-clock-state-02` passes1943 component,637 engine and183
Python cases; `actor-clock-state-sanitized-02` passes1943 component and637 engine
cases with ASan leak checks and halting UBSan. All inventories/XML match, no
skips. Source fingerprint
`4a5e4551397ffb3167cad0536b5047221c0cf0d7b91b73ee76c2fd50d54f45b1`
on parent `146a4b4639`. The01 runs also passed before JSON zero-sign refinement
and fixture promotion; their fingerprint is
`4c3c50aa2423afacc209fd83f89e7a52fee80d98a8788d309b7e29c65696ce06`.

Actual engine courses retain independent input/output decoding:

- `actor-clock-{regular,sanitized}-{load,continuation}-01` preserve manager.125,
  NPC timestamp-1 and Player timestamp100000 through save/restart (01 binaries).
- `actor-clock-regular-zero-red-load-01` retains a genuine failed check: binary
  -0.0 survived, but JSON `-0` parsed as integer0. The exporter now emits an
  explicit fractional representation; the regression assertion checks that.
- `actor-clock-{regular,sanitized}-zero-{load,continuation}-01` on final binaries
  preserve exact clock bits in both binary and parsed engine JSON, including
  negative zero, through two processes.
- `actor-clock-regular-migration-load-01` loads the unmodified accepted v15 Lua
  save and writes v16 without inventing actor timestamps.
- `actor-clock-{regular,sanitized}-final-reject-01` reject a dangling clock owner
  before native application, leave input bytes unchanged and perform no writes.
  Earlier01 rejection courses also passed.

Positive courses preserve all72 AVs per actor, lifecycle/actions and NPC Lua
companion data. Both actors restore once with no queued/committed writes. All22
accepted captures were directly reviewed: fixture room/Player bars for positive
runs and explicit load errors for rejection; NPC offscreen and audio disabled.
Final runtime executable hashes: regular
`21ee181c6e4c4b858cf90061b8477a838b0283e972cd2eb855990784b67df128`, instrumented
`9f03251e91ef1a7127e5e11e99fb6855a97208c7b73b3da31c978ee6541afab1`.
Graphical sanitizer leak checks are off, UBSan halts. The promoted input recipe
reproduces both valid fixtures exactly; the rejection manifest matches the
executed draft. Build/test skill instructions cover setup and bitwise checks.

This closes persistence of the clock fields. Advancing the manager, applying
per-actor elapsed time, native scheduling and hourly-rest integration remain
open, along with the other S3 and later M15 gates.

### S3 resident frame and hourly resource clocks

The unpaused actor pass now advances the native manager once. Published resident
Player/NPC/creature resource writers derive elapsed time from their own completed
update timestamp, then commit that timestamp after successful resource publication.
Missing timestamps use the original constructor's -1; duplicate same-tick calls
consume no elapsed time. Dead resident updates stamp time without restoring
resources, preventing their dead interval from replaying on revival. Float stores,
strict rollover and negative hourly scale follow independently checked callers.
Hourly restoration prepares resource candidates, affected timestamps and manager
time together, then publishes them atomically. Failure leaves prior resource and
clock authority/projections intact. Explicit-duration lower-level batches retain
their existing contract and do not change clocks.

Ordinary Player hourly restoration runs during character creation and with AI
disabled. NPC hourly dispatch requires AI enabled and no character creation;
disabled/deleted references remain excluded. Player's common update follows its
explicit3600-second restoration. NPC common elapsed restoration precedes the
existing High-process special2-second calls. Skipped NPC dispatch keeps its last
timestamp; a later resident frame can consume the elapsed interval. Native full
scheduler cadence and truly unloaded normal-frame resources remain open.

All120 independently recorded original caller outputs match the scalar helpers
exactly. Full `S3/actor-clock-integration-02` passes1944 component,637 engine and183
Python cases. `actor-clock-integration-sanitized-02` passes1944 component and637
engine cases with leak checks and halting UBSan. Inventories/XML match, no skips.
Both record tested source fingerprint
`a7384be416f36cdb5411843a86085dbe228313d4e2b4e767150638a9ce6d227f`
on parent `9a77892d6b`. Added assertions exercise actual Player/NPC/creature
projections, skipped ticks, duplicate calls, initialization boundary, rollover,
save/restore, failed frame writes, dead/revival timing and batch rollback. The01
engine run failed because the new Player fixture incorrectly omitted Willpower's
50-point Fatigue base adjustment; corrected input and retained failure are explicit.

Actual normal-input save/restart evidence under S3:

- `actor-clock-frame-regular-{normal,reset}-final-{load,continuation}-01` and
  `actor-clock-frame-sanitized-reset-final-{load,continuation}-01`: advancing
  manager, exact completed resident timestamps in save/JSON, rollover from100000,
  unchanged72 AVs, lifecycle/actions and NPC Lua companion, no Lua write replay.
- `actor-clock-wait-{regular,sanitized}-final-{load,continuation}-01`: actual T,
  one-hour Wait, F5 and fresh-process resave. NPC Fatigue starts5000/max10000;
  final current matches5000 +10*manager_elapsed +20 within declared0.5. The20
  addition occurs once in the hour course and is absent on continuation. All
  other AVs, identities, life/actions and pending counters are independently
  checked. Player Health100/Fatigue10000, both Magicka20; NPC Health100.
- `actor-clock-wait-regular-chargen-final02-{load,continuation}-01` and
  `actor-clock-wait-sanitized-chargen-final-{load,continuation}-01`: only legacy
  GLOB chargenstate=1 differs. Player still restores, NPC Health stays25 and
  Fatigue has no special20 addition. The retained timestamp permits frame
  catch-up, checked against saved manager time with the same declared tolerance.

Retained failures: a floating Player raw-form integer was rejected before launch;
the first numerical Wait oracle used Magicka's17.5 rate instead of this fixture's
Fatigue default10; a first chargen fixture used an invalid T4ST global FormKey
instead of the legacy GLOB; a first chargen continuation manifest wrongly expected
NPC Health100. These are fixture/oracle errors, not production defects. An early
frame capture caught Loading Area; the final frame manifest pauses3 seconds and
its captures show the room. All32 final captures were directly inspected. NPCs
remain offscreen, audio disabled. Wait menus visibly select1 hour. An initial visual estimate mistakenly classified the first-pass Health bar as
short. Subsequent pixel measurement finds61 red pixels after Wait and16 in the
pre-Wait menu, matching full Health100 and injured Health25. See the quantitative
HUD review below; no HUD synchronization defect was reproduced.

Executable SHA256 before launches: regular
`1742639bdb09e90a07269b59f294e944b8164ad74aacfabe0a78b42c13c0c4e4`, instrumented
`68c54050deb0ea15ac3a281bc7246975a30718221d01ce81350a023aaf0bd2af`.
Graphical sanitizer leak detection is off, UBSan halts. Promoted frame/chargen
continuation manifests and clock/Wait input recipes reproduce the setup; build
skill notes preserve independent timing checks and fixture pitfalls. This is a
bounded clock integration, not an S3/S14 pass: automatic publication, unloaded
cadence, broader life/load/Lua adapters, rest/sleep/AI-off gameplay and later
combat/stealth/crime stages still require implementation and acceptance.

### Quantitative HUD review and reusable capture check

The build/test skill now includes `scripts/verify_fixture_hud.py`, scoped to the
fixed 1280x720 default-skin fixture. It hashes the decoded input, measures
contiguous dominant-color pixels, requires declared fill bounds, and rejects
missing/wrong-size captures and reused evidence outputs. Four synthetic-image
cases exercise wrong expectations, fragmented fills and invalid inputs. Full
`S3/fixture-hud-checks-02` passes187 Python cases without skips at fingerprint
`da62cdd882addc788cb8a372cd30e6f515e2b05e32d9142d103837a0ea49ca6b`
on parent `8cf3491f18`. Skill metadata validation also passes.

Twelve measurements of the original accepted Wait menu/post-Wait/restart
captures pass. The deliberately wrong full-Health expectation on an injured
pre-Wait menu fails (`hud-wrong-health-red-01.json`). The previous visual estimate
of short post-Wait Health was incorrect: measured red fill is16 pixels before
and61 after. No production HUD change was necessary.

The normal-input Wait manifest now pauses3 seconds before its initial capture,
allowing command-triggered native projections to reach the displayed HUD.
`actor-clock-wait-regular-hud-checked-{load,continuation}-01` and
`actor-clock-wait-sanitized-hud-checked02-{load,continuation}-01` pass saved-state
checks and all10 quantitative capture checks (`hud-review-*-03.json`). All10
images were directly reviewed: fixture room, injured pre-Wait bars, one-hour
menu, full post-Wait/restart Health and Fatigue, depleted Magicka. NPCs remain
offscreen and audio is disabled. Numerical NPC Fatigue errors stay below0.06
against the declared0.5 tolerance; continuation has no hourly addition replay.
Inherited input expectation metadata was corrected after execution with original
input/binary hashes retained. Earlier visual metadata now records the review
correction; captures and failed diagnostic artifacts remain unchanged.

Regular executable SHA256 is
`22951458a3d9424a77f212e4d9cd77f46174b4c71e7e21c9bf247614db28845a`;
sanitized executable remains
`68c54050deb0ea15ac3a281bc7246975a30718221d01ce81350a023aaf0bd2af`.
The first concurrent sanitized course failed during Xvfb readiness, before
OpenMW launched; that setup failure is retained separately. The serialized
retry above passed. Graphical ASan leak checks are off and UBSan halts.
These checks cover this fixture's bars and clock/Wait continuation, not broader
NPC, audio, automatic publication, scheduler or M15 gameplay acceptance.

### Native global fixture codec validation

The Python T4ST writer/reader now validates global FormKeys before accepting an
envelope, including the previously admitted plain `chargenstate` key. It rejects
null/zero identities, invalid kinds/hex/namespaces, overflow, and collisions after
native content-name/hex normalization. Legacy unpadded hex, mixed-case names and
content paths remain accepted; their wire spelling is preserved. This change is
scoped to globals, not a claim that every older envelope key has been audited.

Three new cases check encode rejection, malformed-wire decode rejection,
normalization collisions and compatible legacy round trips. Full
`S3/global-formkey-checks-01` passes190 Python cases without skips at fingerprint
`f69e55da4ed59c96b71c7e90cecaca73e7de14a5127ba765f4b0258508b40b51`
on parent `8cf3491f18`. A separately compiled actual `components/esm/formkey.cpp`
parser agrees with Python on270 inputs, including path/case/width/overflow and
namespace boundaries (`S3/global-formkey-native-01/comparison.json`). This is
native save-codec compatibility evidence, not original-game mechanics evidence.

Both this chunk and the preceding HUD workflow chunk remain uncommitted:
the current sandbox rejects `.git/index.lock` creation with a read-only filesystem.
Source editing/testing remains available; no push or publication was attempted.

### S3 correction: Low-process Maximum writes

Independent original-executable dispatch checks found that LowProcess's Maximum
setter is a no-op. The service previously changed its retained Maximum storage,
creating a latent modifier that appeared after an actor became active. Native
nonplayer Maximum writes now preserve that storage while Low. Player and active
NPC/creature paths retain their existing behavior; Script and Damage still run.

`S3/low-maximum-red-01` fails both updated targeted tests against the old writer.
`low-maximum-01` passes all637 engine and190 Python cases; the full engine
inventory/XML matches with no skips. `low-maximum-sanitized-02` passes all637
engine cases under ASan and halting UBSan, with leak detection disabled. Both
record tested fingerprint
`5b2cd0ccde0d2e03bedc9198cb7b0364f83e5dc2dcda9047219ed1e10aa18d80`
on parent `8cf3491f18`. The initial sanitizer attempt failed during inventory
listing when LeakSanitizer reported its ptrace limitation; it remains retained
and supplies no leak coverage. The independent2304/5760-case wrapper/process
probes and their boundary limits are recorded in the native provenance document.
This corrects the previous unit expectation that Low writes should change the
stored Maximum channel. Tests now also promote actual actors back to active
processes and reject replay of ignored writes.

Editable AI-disabled Wait/continuation manifests have been prepared and their
schemas validate. The first launch under
`actor-clock-wait-actor-clock-ai-disabled-regular-load-01` failed before OpenMW:
Xvfb could not bind a display socket. A separate Unix-socket bind diagnostic
returns EPERM in the current sandbox. No AI-disabled runtime acceptance is
claimed, and no graphical course has exercised the new Low-process fix yet.
The sandbox also makes `.git` read-only, so the HUD, global codec and Low-process
chunks remain uncommitted despite authorized staging/commit attempts. Full M15
and its outstanding runtime/implementation gates remain open.

Float base-write preparation now has independent72576-case original-instruction
coverage, with an exact C++ base-field comparison using the existing conversion
and field-width helpers. Provenance records the supported conversion domain,
High cache distinction, boundary fixtures and retained probe failures. No Lua
`.base` adapter or broader base-write runtime acceptance is claimed by this
arithmetic/dispatch evidence.

### S2 continuation: mode-aware finite float base setters

New immutable helpers explicitly model the two native CPU float-to-integer
conversion paths, including finite int32/int64 overflow behavior, then apply
native base-field widths and creature aliases. They leave existing integer
query/command behavior intact. They have no default CPU mode and are not yet
called by the Lua/world base adapters.

`S2/base-float-conversion-01` and `base-float-conversion-sanitized-01` each pass
all1946 component cases with exact inventories/XML and no skips at fingerprint
`1743454bf2755d5dbceeee461cc018966b4a25021be8683be0e4a12301607867`
on parent `8cf3491f18`. The sanitizer run uses ASan plus halting UBSan with leak
detection off because of the recorded sandbox ptrace limitation. Existing
missing-owner aggregate-initializer warnings occur in unchanged inventory/state
tests; the modified actor-value source/test emits no new warning.

Independent original-instruction tables precede production edits. A standalone
optimized C++ driver matches all7088 finite conversion outputs and all72576
typed base-field outputs (`S2/oracle-emulator/base-float-mode-comparison.json`).
Provenance records domains, CPU branches, boundary fixtures and earlier probe
failures. The new scalar rules require subsequent adapter/runtime acceptance;
S2/S3 and full M15 remain open. This chunk also awaits a local commit because
`.git` is read-only in the current sandbox.

### S3 continuation: Lua attribute and skill base requests

Native Lua attribute/skill `.base` setters now normalize queued reads through
the native finite float conversion and field widths, and route world writes
through the existing shared-base command transaction. The adapter explicitly
selects the modern SSE reference conversion. Legacy base setters remain on
their existing path. Dynamic resource base/modifier and skill progress adapters
remain open.

`S3/lua-stat-base-01` and `lua-stat-base-sanitized-01` each pass all638 engine
cases with exact inventories/XML and no skips at tested fingerprint
`ec97ba90035044fc5a5fcfdd4876d3c5c2ab75181815acb797289ea75dfd038b`
on parent `8cf3491f18`. ASan/halting UBSan run with leak detection disabled.
The new real Lua cache test covers fractional/negative wrapping, overflow,
nonfinite rejection, process-dependent reads and unchanged authority before
publication. It does not execute the queued world transaction or prove restart
acceptance. Graphical checks and local commits remain blocked by the sandbox.

### S3 continuation: shared-base resident roster

Both world command overloads now gather the public registry plus references in
already loaded cells, deduplicating live references. Stable-key commands resolve
disabled resident targets through the resident path, and shared-base updates
refresh disabled/deleted sibling projections without enabling or registering
them. No new deleted-actor command eligibility rule is introduced.

Cell iteration now tracks whether its merged cache includes content-deleted
references. A request that needs those entries upgrades a cache primed by an
ordinary scan; ordinary visitors continue filtering them afterward. Disabled
references alone are accessible to ordinary cell visitors.

`resident-base-roster-01` retained one test-fixture failure from a manual record
lookup; that fixture was corrected. `resident-base-roster-red-02` then genuinely
fails the new scan-order regression: two residents instead of three.
`resident-base-roster-03` and `resident-base-roster-sanitized-03` each pass
all639 engine cases with exact inventories/XML and no skips at fingerprint
`1332537b148b1e1746e3a0801e87a44d5957bf14f0db93fd7b4dd2c1bf6c3c3d`
on parent `8cf3491f18`. The sanitizer run disables leak detection and halts on
ASan/UBSan errors. The only regular-build warning is a pre-existing shadowed
local in the saved actor clock path. Tests cover actual native NPC projections,
cache inclusion order, duplicate suppression, registry identity preservation
and unchanged unloaded cells. Actual world-command/Lua publication and graphical
restart acceptance require subsequent coverage. The chunk remains uncommitted
because the sandbox marks `.git` read-only. Full M15 remains open.

### S3 continuation: real headless world stat transactions

The engine suite now has a portable fixture that loads synthetic native content
through the real World, resource, Lua and disabled sound managers, without a
renderer or audio device. Public player pointer queries return empty before
player setup, allowing nonplayer transactions during this phase. A stable-key
command targets a disabled resident absent from the registry; actual Lua cached
attribute/skill base setters then commit through the world adapter. Tests verify
unchanged authority before application, native integer wrapping, all shared
resident projections afterward, lifecycle adoption and preserved disabled/deleted
flags and registry exclusions. Actors are explicitly registered with authority;
this does not prove automatic publication, player gameplay or fresh-process
save acceptance.

The first regular run passes641 cases. The first sanitizer run genuinely fails
in fallback player projection: std::clamp binds a reference to packed NPC DATA
Health, which follows21 skill bytes and is unaligned. The production bridge now
copies Health to aligned scalar storage before applying the same1..65535 clamp.
`headless-world-stats-02` and `headless-world-stats-sanitized-02` each pass all641
engine cases, exact inventories/XML, no skips, fingerprint
`01c822c98ff656086e992367b829339238b314170a6483fd3f758bacc8944c2c`
on parent `8cf3491f18`. ASan/halting UBSan use leak detection off. Earlier
standalone headless setup failures are retained under
`authority-draft/headless-world-probe-01`; they involved link ordering and absent
resource/sound/Player inputs, not successful gameplay.

A subsequent standalone actual-world negative control under
`authority-draft/world-lifecycle-rejection-probe-01` rejects AV72 as expected
but fails its no-mutation assertion: the world creates lifecycle state before
the service rejects the request. Lifecycle adoption must join writer rollback.
This and broader M15 gates remain open; current chunks cannot be committed
because the sandbox keeps `.git` read-only.

### S3 continuation: lifecycle adoption joins writer rollback

First lifecycle adoption now uses a scoped rollback guard around world Kill,
actor-value commands, resource-current requests and breath updates. Failure
removes only the new lifecycle node and restores the exact prior shared death,
essential, animation and knockdown flags. Existing lifecycle authority survives
an abandoned guard. Successful writers commit adoption before notifications;
legacy death-marker lookup is prepared before the writer and erased by iterator
after success. This does not establish whole-world failed-load rollback.

The standalone actual-world rejection probe above demonstrated the original
bug. The first implementation run retained a test compilation error; the next
retained a mistaken integer-overflow expectation for a legitimate float view.
The corrected late-failure test uses overflowing Magicka projection instead.
Actual-world tests cover rejected AV/source inputs, failed shared-base
preparation, exact flag restoration, successful stable-key and Lua commits.
A real projected Player test covers rollback, commit, existing-authority
preservation and rejection of a mismatched player pointer.

`lifecycle-adoption-rollback-04` and
`lifecycle-adoption-rollback-sanitized-04` each pass all642 engine cases,
matching inventories/XML with no skips at source fingerprint
`8d387e045df4bf666c696c979a462a182db39c61735683b78733cf1938ddfe14`
on parent `8cf3491f18`. ASan/halting UBSan use leak detection off. Automatic
actor publication, graphical acceptance, physical lifecycle and full M15 remain
open. This verified chunk also awaits a local commit because `.git` remains
read-only under the active sandbox policy.

### S3 continuation: fresh native values and exact form Health

A fresh nonplayer resolver now constructs native values from winning NPC or
creature records and the independently verified stat rules. It fills native
attributes, skill form groups and AI bytes, preserves sparse modifier absence,
and initializes the permanent Magicka/Fatigue slots to zero. Process selection
is explicit. It rejects missing/mismatched native bases and player aliases.
This is a construction input resolver; it neither activates actors nor replaces
restored authority, abilities, effects or lifecycle state.

Runtime-state v17 adds optional signed nonplayer form Health before the first
float store. A matching float base remains available for shared views and base
queries. Native current float/integer and disabled form queries retain the
integer input; shared-base writes update it when present. Older snapshots keep
their float-only interpretation, with no invented reconstruction from changed
content. An older-schema capture rejects loss of present raw Health before
changing output. Both C++ and Python validate ownership, float-base agreement,
presence flags and version requirements, preserving absence through migration.

The first runs pass all1948 component cases in regular and ASan/halting UBSan
builds at fingerprint
`03dad56f2c841f32e8b3a6153eb500cc25146e97ed8750bfc13b3883db39cc2f`
under `initial-native-values-01` and `initial-native-values-sanitized-01`.
Their engine attempts retain a missing-player-identity test-fixture
failure and a sanitizer failure binding a GoogleTest reference to packed NPC
Health. The corrected fixture supplies required identities and copies packed
Health to aligned storage. Two assertion braces also remove new macro warnings.
`initial-native-values-02` passes all644 engine and191 Python cases;
`initial-native-values-sanitized-02` passes all644 engine cases with leak
detection disabled. Both engine inventories/XML have no skips and record
fingerprint `f3ba8cfaad54cfd0b94c024cde30265c4682cc063f85ebb46385a48925df839b`
on parent `8cf3491f18`. Component production/tests did not change after their
1948-case passes; the corrected runs change engine test fixtures only.

Independent original instruction probes verify828 fresh-container/NPC form
cases and record3120 Health query cases. Optimized C++ comparisons match780
float-current,768 supported integer-current and1040 base-integer outputs.
No-process current and12 overflowing integer-current cases remain outside that
production comparison. A separate production C++ serializer/Python codec check
matches all56 v16/v17 payloads and canonical JSON exactly, with byte-for-byte
re-encoding. Evidence and retained oracle setup/expectation failures are under
`authority-draft/initial-native-values-*`, `initial-health-query-*` and
`raw-health-cross-codec-01`. This is not an actual engine save/quit/restart gate.
Automatic publication and the remaining M15 stages remain open. `.git` is still
read-only, so this tested chunk cannot yet be committed locally.

### S3 continuation: atomic nonplayer construction and restored projection

An explicit world construction adapter now prepares native nonplayer values
and lifecycle together before publication. Fresh NPCs and creatures use the
winning-record resolver; restored values, process, shared-base overrides,
lifecycle and completed update timestamps are preserved on repeated calls.
Construction creates no historical death callback or count. A prepared legacy
death marker is consumed only after successful publication. Automatic actor
activation remains off pending initial-death, player and load-order coverage.

Restore now checks the complete nonplayer resource projection before replacing
service authority, including finite scalar compositions whose outer Magicka
scaling overflows. This is service rollback, not whole-world failed-load
rollback. Actual-world tests cover disabled/unregistered actors, sibling shared
bases, native creature skill aliases, a binary service restart, conflicting
lifecycle adoption, malformed process and missing attribute definitions. The
restart uses a manually assembled runtime snapshot, not World save/quit/load.

The first build retains a fixture compilation failure assigning a StringRefId
through a nonexistent getIf method. The corrected assignment passes all647
engine cases in `atomic-native-construction-02` and
`atomic-native-construction-sanitized-01`, with exact inventories/XML and no
skips, at fingerprint
`08b8354cba9ae39462fc4a9fb08e9cf0b3777813eb0395244d201dba3d88db1d`
on parent `8cf3491f18`. Sanitizers use ASan and halting UBSan with leak detection
off. Graphical acceptance, physical lifecycle, automatic construction and all
remaining M15 gates stay open. The chunk awaits a local commit because `.git`
is read-only in the current sandbox.

### S3 continuation: legacy death markers through the actual T4ST reader

The real World T4ST reader now has a permanent integration case with a
fingerprint-checked synthetic native content file. Successful first adoption
consumes the legacy true marker without generating a death event or count.
A later alive projection therefore does not conflict with the consumed marker.
A newly parsed conflicting marker rejects without replacing live values/life
and remains available for a subsequent valid adoption. Nonboolean markers
reject before first publication. A changed-content rejection retains the
previously parsed snapshot as well as live service authority. This does not
exercise StateManager's broader failed-load cleanup or a gameplay restart.

The initial standalone setup was retained without a runnable probe because
this configuration has no compile_commands.json. The first permanent test
builds retain an incorrect esm/ header path; corrected esm3/ includes pass
all648 engine cases in `world-legacy-marker-02` and
`world-legacy-marker-sanitized-02` with exact inventories/XML and no skips at
fingerprint `3d419dfafa2d0e268e58b654c27717368b42749f93b9f0d2819c5f05738ea88a` on parent `8cf3491f18`.
ASan/halting UBSan run with leak checks disabled. This is additional S3 reader
coverage, not closure of automatic construction, physical death, whole-world
load rollback or M15. Local commits remain prevented by the read-only .git
mount.

The follow-up legacy schema matrix uses the actual T4ST reader for versions
7 through17, each with true and false death markers (22 combinations). Marker
adoption overrides the fresh shared class default, consumes the marker,
preserves process on repeated construction, and produces no historical event
or count. The complete suites `world-legacy-schema-01` and
`world-legacy-schema-sanitized-01` pass all649 engine cases, exact inventories
and no skips, at fingerprint `f48b4717c810a6f25e4b5dc4c9b9d8d393341563f128c746113ba31fa349c932` on parent
`8cf3491f18`. Sanitizer leak detection is disabled. Versions1-6 are not claimed
by this new world marker matrix; existing codec migrations remain separate.

### S3 continuation: lazy restored native class views

Ordinary NPC/creature class construction now projects existing values and life
from the world-owned native authority after installing custom data. It does
not publish fresh actors. A nullable World accessor preserves startup and
service fixtures that do not have a World installed. Two actual-world cases
clear/rebuild custom data after a manually assembled binary service restore,
then verify ordinary class reads, raw/shared-base attributes, creature skill
aliases, death flags and unchanged authority/events. This is not a full world
save/load or normal-input restart.

The failing baseline `lazy-native-projection-baseline-01` retains one failed
NPC projection case among650 engine cases. Interrupted normal/sanitizer01
builds have no completion reports. Replacement `lazy-native-projection-02`
and `lazy-native-projection-sanitized-02` pass all651 engine cases with exact
inventories/XML, no skips, and unchanged tested fingerprint
`74d72cb42e062b0c2f8c27d6261cade39afa033737d79f4de2a9761c0d8fd08e`
on parent `8cf3491f18`. Sanitizers use ASan and halting UBSan, leak detection
disabled. Cached resident projections during full world restore, fresh actor
activation, physical lifecycle and the remaining M15 stages stay open.
The source chunk cannot be committed because .git is mounted read-only.

### S3 continuation: cached resident views at restored service installation

The world now installs prepared native authority together with prepared
resident NPC/creature views. Actors absent from replacement authority remain
outside automatic activation. All matching views and stable identities are
prepared before service replacement and noexcept projection commits. Repeated
pointers are accepted; distinct live owners of one stable key reject. A rejected
replacement remains usable for a corrected roster. The actual-world adapter
tests cover a disabled cached NPC and a two-actor rejected/retried batch,
including preserved old Health/lifecycle and absence of historical events.
This does not make the surrounding World inventory/global/script restore or
StateManager cleanup transactional, nor test a gameplay restart.

`cached-native-projection-baseline-01` retains the stale Health failure
(restored100 versus cached93). The initial implementation passes all653
normal/sanitizer engine cases at fingerprint
`93f93a97ac117b44125e88bb490c5f93c4a4e94aafa02823330ef5a165c3bb75`.
Review added `cached-native-duplicate-baseline-01`, retaining the duplicate-owner
rejection failure. Corrected `cached-native-projection-02` and
`cached-native-projection-sanitized-02` pass all653 engine cases with exact
inventories/XML and no skips at fingerprint `2e9e9e87000d9ed31a5f4029013c8144550604ce5180a6e7a7861a1990ed4c3e` on parent `8cf3491f18`.
ASan and halting UBSan use leak detection off. Commit remains blocked by the
read-only .git mount. Automatic publication and the remaining M15 gates stay
open.

### S3 continuation: native AI views and Lua authority requests

Shared AI stats now project native AV33–36 through the integer query contract.
Hello maps to Energy35, Fight to Aggression33, Flee to Confidence34 and Alarm
to Responsibility36. Native views preserve negative current values, process
rules and separate Maximum/Script/Damage channels. Legacy Base/Modifier writes
reject once a native view owns the target. Lua previews and applied writes use
world authority, retain fractional Maximum and existing Script/Damage, and
apply native byte-width shared-base writes to resident siblings. Ordinary
legacy stat behavior remains covered. Integer composition still uses the
existing supported int32 domain; arbitrary integer overflow and no-process
semantics remain open, as do gameplay AI decisions and automatic activation.

The actual-world NPC/Lua test reproduces stale class reads. Its first baseline
also contains swapped Hello/Alarm fixture expectations, corrected after the
record/AV catalog review; those initial-name assertions are not accepted as
native expected behavior. Normal03 retains a missing physicalcombat.hpp
include failure; normal04 retains an old rejection test for newly supported
AI modifiers. Normal06 retains a damaged generated archive after interruption;
normal07 rebuilds that archive from objects. Normal01/02/05 and sanitizer01–03
were interrupted without successful verification. The initial standalone stat
comparison retains a missing actorstats.cpp link dependency before correction.

`native-ai-projection-07` and `native-ai-projection-sanitized-04` pass all654
engine cases, exact inventories/XML, no skips, at fingerprint `24c86ce829a4e902b9f8512392481d29cda1b14bf79d94f2c35fc26109398f89` on
parent `8cf3491f18`. ASan/halting UBSan run with leak detection disabled.
Tests include NPC and projected Player AI modifier requests, negative class
and Lua reads, byte-width queued bases, fractional previews/applies, resident
siblings, rejected writes and preserved channels. Independent original AI
query and optimized C++ comparisons match1008 cases; the new Stat<int> view
matches336 original integer cases with672 mutation-guard checks. These are
adapter/oracle checks, not full runtime or fresh-process restart acceptance.
The .git read-only mount still prevents the requested local commits.

The AI restore follow-up reproduces a finite Script scalar outside the current
supported integer projection domain replacing service authority before any
class reconstruction. `native-ai-restore-preflight-baseline-01` retains that
failure. Projection preflight now validates AI integer views for unloaded as
well as resident actors before map replacement. The normal and sanitizer
`native-ai-restore-preflight-01` suites each pass all654 engine cases, exact
inventories and no skips, at fingerprint `a4ab4a49274bd9b8012ad1a668f0a3c280e285bfb274a9c39bb8fe7ad013a861` on parent `8cf3491f18`.
Leak detection is disabled. This is rejection/rollback for the supported
projection domain; native overflow behavior and whole-world load rollback
remain open. The source still awaits a commit because .git is read-only.

A follow-up preview boundary test retains `native-ai-legacy-preview-baseline-01`:
the newly introduced legacy fallback rounds untouched INT_MAX through float
and fails to reject infinity. The helper now preserves untouched typed fields,
checks override conversion and sums, and retains the legacy zero clamp. Existing
legacy getters are unchanged. Normal and sanitizer `native-ai-legacy-preview-01`
pass all654 engine cases, exact inventories and no skips, at fingerprint
`264042eec0669a36b0eb75877e0a9e8136402e4216e7feeb9b44bf1a35c6723e` on parent `8cf3491f18`, with leak detection off. This guards the new
preview utility; it does not broaden native overflow or gameplay acceptance.
The local commit remains unavailable because .git is read-only.

### S3 continuation: real World Player data construction before rendering

World setup now constructs/retains its Player data independently of optional
scene attachment. With rendering installed, the existing controller/physics/
scene removal and setup path still runs. A real native-profile content load
constructs the projected Player, reads its actual class stats, and repeats
setup with stable identity and no fresh native authority publication. This
removes a rendering dependency from data-adapter validation; it is not a
headless gameplay or graphical acceptance shortcut.

`native-player-data-construction-baseline-01` retains the null-renderer SIGSEGV
(-11), with no completed test XML. The corrected normal and sanitizer
`native-player-data-construction-01` runs pass all655 engine cases, exact
inventories/XML and no skips, at fingerprint `0cd7e32723d237ec627c06b1287ae817344d7c7de840f733b4c4e56132a874e9` on parent `8cf3491f18`.
Sanitizer leak detection is off. Player authority construction/restoration,
automatic actor activation, scene acceptance and remaining M15 stages stay
open. The requested local commit remains blocked by read-only .git.

### S3 continuation: cached real World Player restore installation

The restoration adapter now prepares the actual World-owned Player together
with all resident native NPC/creature views before replacing live authority.
It installs saved derived values and modifier channels directly, without
recalculating fresh defaults or dispatching historical death events. World
load supplies its Player to this adapter. Missing Player data, invalid resident
identity, and self-aliasing reject before authority/view commit; rejected
replacement data remains available for a corrected retry.

`native-player-restored-view-baseline-01` retains the missing resolver include
compile failure. Baseline02 demonstrates stale Player Strength43, Health103
and alive class views despite restored authority50,110,Dead. The corrected
normal and sanitizer `native-player-restored-view-01` suites each pass all656
engine cases with exact inventories/XML and no skips at fingerprint
`eea141756329da61b54b38f57539c9b924bc1913deaecaa2782281cdf4d992ff`
on parent `8cf3491f18`. ASan/halting UBSan run with leak detection disabled.
The mixed Player/NPC test verifies unchanged complete captured service state
and views after rejection, retained replacement data, corrected retry and
restored resource/life reads. This is service/view installation using a real
World-owned Player and a binary snapshot roundtrip; it is not actual World
apply, a save/quit/fresh restart, or whole-world load rollback. Those paths,
older Player form-input migration and fresh authority construction remain
open. Local commits are still unavailable because .git is read-only.

### S3 continuation: actual World T4ST apply respects Player authority

World data restoration is now callable independently of the scene-attachment
part of saveLoaded, using its already-read native snapshot. Native Player
resources, attributes and skills are installed from prepared authority;
legacy Player telemetry no longer writes those guarded views first. Snapshots
without native Player authority retain the existing legacy data path.

`native-player-world-apply-baseline-01` retains the private-method compile
failure; baseline02, after exposing the unchanged data method, reproduces
the native dynamic-stat mutation guard. Normal and sanitizer
`native-player-world-apply-01` each pass all657 engine cases, exact inventory/
XML and no skips, at fingerprint
`e8bc7ba83204149e064b6c13a85539e8ac187b0f225aa2ee072dc2e330d9f686`
on parent `8cf3491f18`. ASan/halting UBSan run with leak detection off. The
new regression passes a real T4ST record through World read/apply with
conflicting legacy Health/Strength/Blade telemetry and verifies the native
resource, attribute, skill and dead views plus unchanged captured authority
and no historical events. Synthetic cell/race data use real stores; no
rendering or gameplay restart occurs. Whole-world rollback, native Player
preflight before unrelated mutations, old form-input migration, fresh
activation and normal-input/fresh-process acceptance remain open. Commits
remain unavailable under the read-only .git mount.

Player projection preflight now also runs during detached service restore.
`native-player-world-preflight-baseline-01` demonstrates an unsupported finite
Player AI Script scalar changing World hour to7 before late rejection. The
corrected normal and sanitizer `native-player-world-preflight-01` runs each
pass all658 engine cases, exact inventory/XML and no skips, at fingerprint
`90eb3a5b297dba0f0dd853e1841f4a49891e22309a7db4b7e09a5d36dbb381bb` on parent `8cf3491f18`. Leak detection is disabled.
Tests verify clock preservation, unchanged live Player authority/view and
corrected retry. A separate actual World read/apply regression preserves
legacy resource/attribute/skill telemetry without native Player authority
through all15 schema versions3–17. This is neither migration of missing raw
Player form inputs nor complete whole-world transactional rollback. Those
gates, fresh authority construction and gameplay/restart acceptance remain
open. The local Git commit still cannot be written under this sandbox.

### S3 continuation: actual World capture/read/apply/recapture

The existing World native capture method is available alongside its data-apply
method for data-layer validation. The actual World test now includes a moved
NPC in a native cell and live Player/NPC authority. It captures through World,
mutates both resources, writes/reads T4ST, applies through World and requires
binary-identical recapture plus matching cached class reads and no events.

Normal and sanitizer `native-world-capture-roundtrip-01` retain the fixture
identity failure: its NPC reference supplied a stable base key but omitted
the corresponding FormId field. The corrected fixture supplies both, without
weakening World serialization. Normal and sanitizer roundtrip02 pass all658
engine cases with exact inventories/XML and no skips at fingerprint
`7c682846cf40ee491b9876b0a06c2f71d491ae9ddbb525bcf5c3802a3e6c90d7`
on parent `8cf3491f18`. Leak detection is disabled. This exercises actual
World native capture and T4ST data restoration in one process, not a complete
engine save, scene reattachment, quit/fresh load or normal-input acceptance.
Whole-world rollback and remaining M15 stage gates stay open. Local commits
remain prevented by the read-only Git metadata mount.

Player projection binding now has a shared detached validator, called before
World mutation and again during actor-view installation. The identity
preflight baseline01 retains both wrong-base-alias and missing-form-input
snapshots changing the clock to8/9 before rejection. Normal and sanitizer
`native-player-identity-preflight-01` each pass all658 engine cases, exact
inventory/XML and no skips, at fingerprint `16086adfa1464e462d8bc7672ebd4edf6d8cb9af7328f51b38c6b0569aa5a16e`
on parent `8cf3491f18`, with leak detection disabled. Rejected identity
snapshots preserve the clock and captured live authority and permit retry.
Generic binary/service-map readers still support absent old form inputs;
actual Player projection already required those inputs, and this change
moves that rejection earlier rather than defining their migration. Legacy
World telemetry snapshots3–17 still pass. Migration, complete load rollback,
fresh activation and gameplay/restart acceptance remain open. Git metadata
remains read-only, so this chunk also awaits its requested commit.

World native data apply now requires a constructed Player before mutation.
`native-player-ready-preflight-baseline-01` retains the version2 snapshot's
null-Player SIGSEGV (-11), without completed XML. Normal/sanitizer ready01
retain a test-only attempt to capture native service data into that old
version2 container; the readiness guard itself passes. The corrected normal
and sanitizer ready02 each pass all659 engine cases, exact inventories/XML
and no skips, at fingerprint `bf4328c437c9e3b97a86e3a3b556e1c67abce0d8e95d109e96a078a427c8b63c` on parent
`8cf3491f18`. Leak detection is disabled. Missing Player data now fails
without clock changes or authority publication. Legacy World read/apply
resource/attribute/skill coverage now passes all17 schema versions1–17
without native Player authority. This does not establish missing-form-input
migration, whole-game rollback, scene acceptance or fresh-process restart.
The local commit is still unavailable because .git is read-only.

### S3 continuation: actual World data clear and reconstruction

World clear now clears optional weather/render/projectile/scene subsystems
when present, and always clears World data and native services. Its normal
initialized scene path retains the same order. A real native-profile World
now supports two Player/NPC publication, engagement, action and clock cycles
without scene initialization: clear discards residents, values/base overrides/
life/breath/update times/counts/events/engagements and action ownership, resets
clocks/event sequence, and reconstructs fresh Player class data without
automatically publishing new native values.

`native-world-data-clear-baseline-01` retains the null-subsystem SIGSEGV(-11),
without completed XML. Normal and sanitizer `native-world-data-clear-01` each
pass all660 engine cases, exact inventories/XML and no skips, at fingerprint
`93fa2485c92df97fe8fd42a376d5c741565e761775c25750e2151237ace4fa5e` on parent `8cf3491f18`. Leak detection is off.
This is actual data/service clear and reconstruction, not rendered new-game
acceptance, full load failure recovery, save/quit/fresh restart or S3 closure.
The requested commit remains unavailable because .git is read-only.

### S3 continuation: staged native global bindings and conversions

World restore now resolves and converts every global into detached copies
before committing globals or the native allocation serial. Commit uses
noexcept Variant moves and preserves FormKey order, including editor-ID
aliases. Float target overflow and double-to-int64 conversion overflow are
rejected before casting. Supported int64 inputs retain the existing int32
clamp; supported fractional doubles retain truncation. This is save-bridge
validation, not a new original-game global arithmetic or overflow rule.

Global preflight baseline01 retains an earlier global changed by a later
wrong type and finite-double overflow accepted as infinity. Baseline02 adds
positive/negative huge integer doubles and the excluded2^63 boundary, which
are accepted through an unsafe cast and change earlier globals. The corrected
normal and sanitizer `native-world-global-preflight-01` runs each pass all661
engine cases, exact inventory/XML and no skips, at fingerprint
`ebf2c12347e3b2772b5eb51bb9b9b242cfc1b1266e971835494978b908ca2c32` on parent `8cf3491f18`. Leak detection is off.
Actual World read/apply tests cover rejection without earlier global/clock
mutation, valid retry, int64 extrema, fractional signed truncation and the
included-2^63 boundary. Synthetic global definitions are registered through
real native/projected stores. Full World/StateManager rollback, other consumed
scalar domains, save/quit/fresh restart and gameplay acceptance remain open.
The requested local commit remains unavailable under the read-only Git mount.

Clock scale and consumed legacy Player scalar conversions now also preflight
before World mutation. Resource/attribute/skill fields are checked only when
native Player authority is absent; selected modifier/old-modified precedence
is preserved. Breath and level fields retain their existing consumption.
Finite float conversion and truncated integer range checks prevent overflow
before casting. They do not define original-game stat or level limits.

Scalar baseline01 retains huge positive/negative clock scale and18 oversized
legacy Player fields changing globals/class data without rejection. Normal
and sanitizer `native-world-scalar-preflight-01` each pass all661 engine cases,
exact inventories/XML and no skips, at fingerprint `e3f40675603c314fa960045954d4065c89fd7a96b8b9c19f86d61ce3baf73e33`
on parent `8cf3491f18`, with leak detection disabled. Tests include valid
retry, ignored unknown telemetry and superseded old-modified telemetry.
This validates consumed conversion inputs; derived legacy arithmetic, missing
Player form-input migration and whole-world rollback remain open, alongside
normal-input/fresh-process acceptance. Local commits are still unavailable
because Git metadata is read-only.

### S3 continuation: socket-free SDL input and actual engine restarts

The installed SDL `offscreen` driver can create Mesa llvmpipe OpenGL core and
compatibility contexts using `EGL_PLATFORM=surfaceless`. The retained
`sdl-offscreen-context-01` probe and `sdl-offscreen-engine-load-01` actual-engine
load establish a new runtime route without X11 sockets. Xvfb and original-game
Wine remain unavailable; this does not resolve the original-game oracle gate.

`scripts/sdl_offscreen_input.cpp` is a test-only preload library. Its opt-in
append-only input protocol emits SDL key down/up and quit events after the
native event queue empties. It maintains held keys and modifier state, records
every delivered line, waits for complete lines, and rejects bad sequences,
invalid scancodes and other video drivers. It does not access engine state.
Compile with C++17, `-shared -fPIC`, SDL2 pkg-config flags and `-ldl`; set
`LD_PRELOAD` to the library and `OPENMW_SDL_INPUT` to a new empty private input
file. Lines are `1 down 62`, `2 up 62` (F5), for example; each subsequent line
must increment its sequence. F12 is scancode69. Append presses/releases with a
frame interval between them. This currently supports keyboard/quit only;
existing scenario actions still require integration before replacing their
X11 controls. Do not substitute console world/stat mutations for input.

The new real-SDL tests exercise seven cases: keyboard/held/modifier release,
partial lines/native-event priority, opt-out pass-through, recorded quit,
invalid sequence, invalid scancode and wrong driver. The corrected full Python
check `sdl-offscreen-input-python-01` passes all198 cases with no skips at
fingerprint `629d0ee7849ecbff53d414c1e07fecf1bd44ba15cca3b29ddf4f2ce29888b36e`.
The initial wrong-driver test failure identified rejection during the test's
startup event drain; validation now occurs when input is available.

Actual engine courses `sdl-offscreen-save-restart-01`,
`sdl-offscreen-native-restart-01` and
`sdl-offscreen-native-sanitized-restart-01` each perform private-copy load,
F12 screenshot, F5 save, SDL quit, fresh-process load, screenshot and resave.
Each phase exits0, consumes every input exactly once and leaves its input save
unchanged. The first upgrades an idle v9 save with no native actor entries.
The populated course upgrades the historical v16 regular-zero fixture with
Player and NPC authority and persisted Lua modifiers. Independent binary
inspection preserves all actor values, life/breath/base/engagement/death data,
Player position and resource currents; absent optional v17 non-player form
health normalizes to null. Manager/completed update clocks advance normally.
Lua reports both restored actors. There is no fresh actor publication here.

The normal executable SHA256 is
`6c59d8f499cbf6d8ab23e1aa589376d753868babee817cfd5ca37e379c18a73e`;
the sanitized executable is
`be2ca2ddbace380b111c6f9fa660fb59ab096882dfdf3894d28ed8750a02da77`.
Their engine source was verified by the preceding normal/sanitized all661
scalar-preflight checks at fingerprint `e3f40675603c314fa960045954d4065c89fd7a96b8b9c19f86d61ce3baf73e33`.
The sanitizer course preloads libasan before the replay helper and halts on
ASan/UBSan errors, with leak detection disabled. Both runtime logs are clean
of replay/load/save/frame/sanitizer failures. Retained probe scripts,
configuration/input/log hashes, save and image hashes and state/visual
verification are in each course directory.

I inspected the screenshots: the empty-authority fixture has full resources;
the populated fixture displays health25/100, magicka20/100 and fatigue200/200.
Restart images are byte-identical within each course, including the sanitized
populated course. Initial visual checks expecting98% full-bar fill failed
because two border pixels in the63-pixel measurement region are not fill;
those checks remain as `state-visual-baseline-01.json`. Corrected full-bar
bounds95%-100%, health23%-27% and magicka18%-22% pass both phases, with contiguous
fills. These are narrow idle persistence/HUD checks, not complete S3 runtime
acceptance, combat/campaign acceptance, original-game visual comparisons or
audio validation. S2/S3 remain open; the next task is reusable scenario input
integration and the outstanding Wait/AI-disabled course. Local commits still
cannot be created because the sandbox mounts Git metadata read-only.

The replay helper now also supports focus and printable ASCII text events;
text is hex encoded, limited to SDL's31-byte payload, and rejects controls.
`text-focus-baseline.log` retains the failing unsupported-event test. Nine
real-SDL tests now verify event payloads, focus window/event IDs, keyboard
state and the prior queue/protocol cases. Five additional replay-controller
cases verify key mappings/chord rejection, exact receipt causality, missing
receipt timeout, validation before input, and lossless text chunking.

`scripts/sdl_offscreen_replay.py` integrates that backend into
`oblivion_compat.py scenario`. Set `sdl_offscreen_input: true` and `xvfb: false`
in a diagnostic manifest with private `output/userdata`. The runner builds
and hashes the trusted helper, records its environment, waits for each input
receipt, uses F12 and retains the engine's complete PNG before inspection,
and sends SDL quit after actions. It rejects unsupported actions, competing
backends, unsafe keys, reused input files and nonprivate user-data paths.
Strict M14/M15 contract scenarios remain rejected for this backend until
corresponding contract/evidence integration is implemented; no schema or
normal-input restriction has been weakened. The initial runner validation
failure is retained in `runner-baseline.log`. Corrected full Python
`sdl-offscreen-runner-python-01` passes all206 cases without skips, fingerprint
`014a56dbf0ff58ffeb9381bee380e64cfa0d9e3a0e2d2ca8c66566da3ebc8c81`.

The pending AI-disabled Wait/continuation editable manifests now select this
backend. Actual courses `sdl-offscreen-ai-disabled-wait-01` and
`sdl-offscreen-ai-disabled-wait-continuation-01` pass all actions and clean
exit0; their sanitized counterparts also pass with ASan/halting UBSan and
leak detection disabled. Inputs are private copies of the historical v16
`actor-clock-wait-sanitized-hud-checked02-load-01/pristine.omwsave`, with
synthetic populated authority and Player raw form values. No fresh actor
construction is demonstrated. Bootstrap ObScript zero-delta ModAV and
read-only queries remain explicit setup; Wait itself uses ordinary UI input.
I inspected the console's `AI -> Off`, the one-hour Wait dialog, the post-Wait
HUD and the fresh-load HUD. Configurations, helper build/input receipts, logs,
source save/fixture/binary hashes and verifier scripts are retained.

Independent binary and HUD verification passes normal and sanitized pairs:
one calendar hour plus regular frame time, Player H/M/F100/20/10000, NPC
Health25 and Magicka20, full19-second breath, stable life and no death events,
completed actor clocks and matching legacy Player views. The NPC does not
receive the High-process two-second hourly restoration while AI is disabled;
ordinary frames still update it, including the elapsed-clock catch-up after
Wait. Independent raw fixture decoding pins `fFatigueReturnBase=10` and
`fFatigueReturnMult=0`; NPC fatigue matches5000 plus10 times manager time
within0.5 points for accumulated single-precision rounding. The fresh process
continues normal recovery. This is not a claim that the console AI toggle
persists across restart. `wait-restart-verification.json` in each first course
records all expectations and their measured values. Current normal and
sanitized engine hashes remain those recorded above. Original-game
AI-disabled behavioral comparison, audio, full S3 and M15 acceptance remain
open. Git metadata remains read-only; exports are recovery patches rather
than the requested commits.

### S3 continuation: finite legacy resource composites before mutation

Finite input fields could still overflow a legacy resource's computed
maximum or the old-modified-minus-base conversion. The retained
`native-world-resource-composite-baseline-01` fails the actual World
read/apply case: six resource/encoding combinations accept overflow and
change earlier globals or projected Player stats. Restore now prepares all
three legacy dynamic stats together, retaining existing fallbacks and
modifier precedence, and validates their computed modifiers and uncapped
maxima before World mutation. Native Player authority still supersedes this
legacy telemetry branch.

Normal `native-world-resource-composite-01` and sanitized
`native-world-resource-composite-sanitized-01` each pass all661 engine cases,
with matching inventories/XML and no skips, at fingerprint
`3049c5ab6fe35eeda4bf700a92bc393792165064567030a1641fa7e2b3448824`
on parent `8cf3491f18`. Leak detection is disabled. The case checks rejection
without resource/global/clock changes, valid retry, and supported large signed
cancellations yielding finite zero maxima. It is save-bridge output-domain
validation, not an original-game limit on authored stats.

Actual legacy-telemetry courses `sdl-offscreen-save-restart-02` and
`sdl-offscreen-legacy-sanitized-restart-01` each pass load, native F12 capture,
F5 save, SDL quit, fresh load and resave with clean exit0 and unchanged private
input saves. Independent binary/resource/HUD checks pass. Both phases have
the same already-inspected full-resource image SHA256
`1438ae8fdfb0e7419dc430638e1ed3b42bebd10cc3eb7c403ac624825254abec`.
The normal engine SHA256 is
`8fcf29574bd40b979b5da60d0409bd6d94b22573e56c34bd96457f33e89482e3`;
the sanitized engine SHA256 is
`ad403c17989d3b29beee68499ae23df00a0469e153b2c3a74709b4ad87ea7468`.
Other legacy attribute-derived arithmetic, World/StateManager rollback, fresh
native construction and the remaining S2/S3 gates are still open. Local commits
remain unavailable; the source and evidence are retained as pending chunks.

Player cell binding now also resolves before global/clock/Player writes.
`native-player-cell-preflight-baseline-01` retains an actual World rejection
of a missing content cell after the earlier global changed3 to42 and the
clock changed7 to8. The corrected normal
`native-player-cell-preflight-01` and sanitized
`native-player-cell-preflight-sanitized-01` each pass all661 engine cases,
exact inventory/XML and no skips, fingerprint
`6bf614c31c2f5dab269c95c1a7ac9989b80e5c2063a1d4fbe58a20cd14051789`
on parent `8cf3491f18`, with leak detection disabled. The same case retains
valid retry and checks the previous Player cell/global/clock on rejection.
Cell-cache loading itself is not a detached transaction; this does not close
reference restore, whole-World or StateManager failure recovery. It is a
bounded data-binding preflight fix, not another graphical acceptance claim.
Local commits remain unavailable under the read-only Git mount.

Reference bindings now prepare target cells, existing references, expected
bases and resolvable owners before global/clock/Player/reference value writes.
`native-reference-binding-preflight-baseline-01` retains four actual World
failures: missing reference, mismatched base, missing target cell and dynamic
owner all change an earlier global; the owner failure also changes NPC position.
Prepared bindings retain the existing skip for unprojected dynamic references.
Cell loading/enumeration can still change caches; this is not whole-World
transactional rollback.

Normal `native-reference-binding-preflight-02` and sanitized
`native-reference-binding-preflight-sanitized-02` each pass all661 engine
cases, exact inventory/XML and no skips, fingerprint
`0d14b57c31e316fb37211370196ac2edd13b44f63ad1f3d968cf0d959dbf7480`
on parent `8cf3491f18`, with leak detection disabled. The extended actual
read/apply case checks failure preservation, valid retry, a move into a second
native cell, and unchanged acceptance of an unprojected dynamic reference.
The first corrected01 pair also passed before these additional cases.

Actual populated authority courses `sdl-offscreen-native-restart-02` and
`sdl-offscreen-native-sanitized-restart-02` pass both idle load/save/SDL-quit/
fresh-load/resave phases with independent binary/HUD checks. Their images
match the already-inspected earlier populated captures. Engine SHA256s are
`d1041535c8e10aa02d4d4162929699efc083c8f098eaa1878e0d2f27bc6fd9f8`
and `43cd34fea19ddb408686c2470357eb0de273600d1bef928154abecc68335b679`.
This exercises the changed binding order in the actual engine. Reference
custom-state application, full load recovery, fresh construction and the
remaining S2/S3 gates stay open. Git metadata remains read-only, and recovery
patches do not fulfill the requested commits.

### S3 continuation: consumed reference custom state prepares before writes

World restore now validates consumed lock, scale and animation values while
binding references, before changing globals, clocks or reference positions.
Animation strings/vector entries allocate into detached state; the later
assignment moves that prepared state without allocation. Unsupported lock or
animation types reject early, and a finite double scale outside the float
range cannot store infinity. Unconsumed legacy telemetry retains its previous
semantics: wrong-type count/scale are ignored, inactive animation groups are
ignored, legacy playing=true supplies progress1, negative loops clamp0, and
large finite progress clamps1 before its float store.

`S3/native-reference-custom-preflight-baseline-01` reproduces the earlier-global
and position mutation and infinite-scale bug in the actual World test.
`native-reference-custom-preflight-01` and
`native-reference-custom-preflight-sanitized-01` both pass all661 engine cases,
with exact inventory/XML agreement and no skipped cases. Tested dirty-source
fingerprint: `6e20ba273dc3d07193e96a32ec851a9d2ad60e7c98c0db33bf41c5bbb800859c`
on parent `8cf3491f18`. ASan leak detection is disabled and UBSan halts on errors.
This closes these consumed-field rejection cases, not whole-World rollback,
rendered animation acceptance or native fresh construction. Local Git commits
remain unavailable because Git metadata is read-only; recovery patches are
exports of pending work, not commits.

### S3 continuation: duplicate live reference owners reject before world writes

Reference indexing now rejects two distinct live objects with the same native
stable key before applying any runtime state. Repeated views of the same live
object remain accepted. The actual World regression reproduces a duplicated
resident across two loaded cells: the former final-service rejection changed
clock and position first. `native-resident-owner-preflight-baseline-01` retains
that failure. `native-resident-owner-preflight-01` and its `sanitized-01` partner
pass all661 engine tests, including cross-cell moves and restored resident
views. Exact inventory/XML counts agree, no cases skip, and ASan leak checks
are off with halting UBSan. Tested dirty source fingerprint:
`4e6536cc16f210fd7d7702d1aa816d79bd8d71b27e4bd6b294f108aed767e538`.

The rebuilt engines also complete both SDL offscreen idle populated
save/quit/fresh-load/resave phases in `sdl-offscreen-native-restart-03` and
`sdl-offscreen-native-sanitized-restart-03`. Independent decoding preserves
actor values, raw inputs, life, breath, base overrides, engagement/death maps,
Player resources and position, with the deliberate schema16->17 upgrade.
Both phases exit0 by SDL input. HUD checks pass; all four screenshots retain
SHA256 `0b785b32a9db3da21dc052162855370392cdf3e37ed68e6863f94bef0480b7a5`.
Normal executable SHA256:
`0753eba3d06fe48f99a0a4fc53f011063bbc49afff6200c59e9c34d5b687cab9`;
sanitized executable SHA256:
`45f8c710294e1e456728cd4bf086509bf5730a51d241c4507b1d5f3f13edf176`.
Private inputs, receipts, probes and source/build provenance are retained.
This is idle persistence evidence, not full load rollback, fresh construction,
combat, audio or completion of S3. Git metadata remains read-only.

### S2/S3 construction investigation: absent process and initial attachment

Two independent probes execute the pinned original1.2.0416 SHA256
`a8f313845c1545e9a60e1e995961eef4c033115da9443f6d756341df3c2b7dc6`.
`authority-draft/no-process-current-queries-01` passes5760 cases across all72 AV
IDs, ten signed integer-derived base boundaries, both x87 precision words,
both CPU conversion modes and float/integer return conventions. Original
current-query dispatch and base-integer flooring execute; the base float
lookup is a declared boundary fixture. With no process, current queries use
that base-integer fallback, omit modifiers and skip the AV9 process multiplier.
Report SHA256:
`2f8eba60d0dedb7bf8a418364f247299876f14ac73ffe943735897007c9c58cb`.

`authority-draft/initial-life-attachment-01` passes448 cases through the
original attachment slice, Player identity check, unsigned form Health
predicate and state setter with no process. Zero form Health selects raw
state2, or6 with essential + the native global mode; Player identity and
referenceID7 suppress the branch. The Starts Dead header bit does not select
this predicate. Changed-flags and manager-request3 calls are observed boundary
fixtures, not applied effects. Report SHA256:
`ba3ca77a507b767f37e21b16cd9cccea9edf15d3be4ad0fff0b31786db9e1fea`.
Neither probe executes full factory/record autocalculation order, process
creation, callbacks/count history or physical lifecycle. The current public
Low/Active process enum still has no absent-process state; automatic fresh
publication remains disabled while those construction requirements are open.

### S2/S3 continuation: fresh LowProcess storage matches original constructors

The initial nonplayer resolver no longer invents Maximum-slot presence for
fresh Low actors. Magicka/Fatigue keep their permanent Script and Damage zero
nodes, while Maximum remains absent. Active construction retains its existing
permanent zero entries in all three channels. Existing restored snapshots are
still preferred; this change does not normalize old persisted modifiers.

This corrects the earlier blanket statement that all three fresh channels
have permanent AV9/10 nodes regardless of process. The independent original
constructor probes establish the owning containers, not just their scalar
outputs. On the pinned executable, `common-actor-construction-01` executes48
common Actor/Character constructors with poisoned memory, both x87/CPU modes,
and successful/failed LowProcess allocation. Successful construction allocates
LowProcess; raw life state is0 and magic objectF8 is absent. Common Script and
Low Damage retain only AV9/10 zero nodes. TESForm identity/global registration
and manager insertion are declared boundaries. Report SHA256:
`36606691c361ec60e952f17f9e33ee3d8b16bf0237677be75b68f76260493ff0`.

`process-construction-04` executes48 Low/MiddleLow/MiddleHigh/High constructors
and their actual nested constructors. Each tier owns its Damage container;
MiddleLow and higher additionally own Maximum94. AV9/10 nodes are positive
zero and other sparse entries absent. Allocation, a High random return and a
zero cached-angle normalization are declared boundaries. Attempts01–03 retain
probe failures; the angle-helper loop exhausted the instruction budget before
its non-AV cache boundary was isolated. Report SHA256:
`b3358423aa5c78853a1fbf6deab7f1a1ead18f15fa9f8e22f7f6aad93d7f7763`.

`player-constructor-modifier-arrays-01` separately executes the original Player
initialization loop without function stubs:1296 positive-zero stores across
all72 Maximum/Script/Damage entries, three poisoned patterns and both x87
words. Surrounding bytes remain poisoned. Report SHA256:
`6c1573e5f16d7fea612154a7f784b466ee6e0044e39fd5768a5adca51a47568b`.
Full Player construction and form/race/class loading do not execute here.

`native-fresh-low-maximum-baseline-01` retains the wrong fresh Low-slot failure.
The first full normal/sanitized attempts also expose an older creature test's
assumption that fresh Low and Active storage are identical; that expectation
now distinguishes Maximum presence and independently compares bases, Script
and Damage. `native-fresh-low-maximum-02` and its `sanitized-02` partner pass
all661 engine cases, including actual World publication and binary persistence
of absent Low slots. Exact inventories/XML agree, no tests skip; ASan leaks
are off and UBSan halts. Automatic fresh activation, absent-process authority,
full record/autocalculation/load order and essential attachment still remain
open. These constructor checks do not close gameplay or S3.
Tested dirty-source fingerprint:
`00a2eb4845b69a1ddc227c8dbef680418d196db433f45e20f52c6fe845ef7825`
on parent `8cf3491f18`. Git metadata is still read-only; this tested chunk is
retained in the pending worktree and recovery export rather than a commit.

### S2/S3 continuation: shared constructor modifier storage

`initialActorValueModifierStorage` now supplies the original constructor
storage for both owners: all216 Player slots contain positive zero;
nonplayers retain only Magicka/Fatigue permanent Script/Damage nodes,
and their Maximum nodes exist only above Low. The fresh nonplayer resolver
uses this shared rule. Two component cases check every slot, positive-zero
bits, mutation behavior and invalid construction domains against the
independent constructor probes recorded above.

`native-constructor-storage-rule-01` passes all1950 component cases and425
ESM4 sanitizer cases. Its engine run and the separate sanitized engine01
retain a failure caused by changing the resolver's invalid-process exception
from runtime_error to invalid_argument. Explicit resolver domain validation
preserves the existing exception contract. Corrected engine02 and
sanitized-engine02 each pass all661 engine cases with matching inventories,
no failures or skips. ASan leak detection is disabled; UBSan halts.
Corrected engine source fingerprint:
`baaf488d697227527901cb020a2175342f6b49926c487806a244ca9ebfad99f0`.
The passing component run predates only that resolver exception correction;
it is not represented as a rerun of the final engine source.
Evidence is under `build/oblivion-compat/m15/S3/`, with run names prefixed
`native-constructor-storage-rule`. Automatic fresh actor publication and
full Player construction remain open. Git metadata remains read-only;
this chunk is exported as pending-chunks-21, not committed.

### S3 continuation: authored Player inputs before character generation

`resolveOblivionInitialPlayerValues` reads the winning canonical Player NPC_
form by stable key, retains raw signed Health and unsigned Magicka/Fatigue
words, maps all authored attributes/skills/AI settings and seeds all216 dense
modifier slots. Publication derives resources from native GMSTs; shared facade
stats never supply raw form contributions. Missing, foreign/mismatched and
non-TES4 records are rejected. Autocalculated/scaled Player forms remain an
explicit unsupported construction domain. No automatic call site is enabled;
character generation, abilities and full Player readiness are still open.

The independent standalone master audit `authority-draft/player-winning-form-audit-02`
reports authored Player Health45, Magicka0 and Fatigue150 in hash-identified
Oblivion.esm. It corrects audit01's level offset (barterGold was read as level).
Its report SHA256 is
`1392e433bad257e49af9038c70cf83775ac0507fc2ed1b761e8a09f420095b7b`.
This is not a winning override or ready-actor audit. Derived arithmetic and
dense slot expectations come from the independent original instruction probes
already recorded; tests use a synthetic winning store and an explicit Magicka
GMST, not assertions about a fully generated original-game character.

Two engine cases check fresh raw inputs, a real Player's shared stat views,
binary persistence, winning override rereads, signed Health precision above
2^24, full resource words and malformed domains. Baseline01 retains a harness
compile failure (wrong serialization API); corrected baseline02 fails at the
explicit unimplemented resolver. Attempt01's normal test used the wrong
Magicka fallback assumption; the corrected fixture supplies its intended
winning GMST. Sanitizer01 catches a packed Health reference passed to bit_cast;
copying to aligned storage fixes production. Sanitizer02 catches the same
reference problem in the new test's comparison; its aligned copy fixes the
harness. Failures remain in their evidence directories.

`native-player-raw-construction-03` and `native-player-raw-construction-sanitized-03`
each pass all663 engine cases with exact inventory/XML agreement and no skips.
ASan leak detection is disabled, UBSan halts. Tested source fingerprint:
`99d3679d1857707357b5a66adc1b916586e935fc5f6f88d956e3ba619dbe6bb2`.
Evidence lives under `build/oblivion-compat/m15/S3/`. This establishes the
construction-input and publication boundary, not normal gameplay or S3 closure.
Git metadata remains read-only; pending-chunks-22 is an export, not a commit.
