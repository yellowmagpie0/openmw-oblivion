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
| S3 services/persistence | pending | No implementation/evidence | Native authorities and version migration |
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
