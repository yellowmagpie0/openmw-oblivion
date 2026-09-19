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
| S2 native data/rules | in-progress | Typed CSTY and native inventory work below; preceding S1 closure `eda8168dad` | Typed store/audit/sanitizer checks, original defaults and independently reviewed rules |
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
