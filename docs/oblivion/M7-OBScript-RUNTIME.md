# M7 native ObScript execution and event model

Status: accepted with live runtime evidence on 2026-08-11. The accepted
working tree is based on revision
`cdd9860bd098cea9beaec94a853fe2ce398e5fce`; M6 and M7 are represented by the
dirty-tree manifests in the generated evidence.

M7 executes the native `ObScript::Program` produced by M6 synchronously on the
OpenMW main thread. It supplies persistent object, quest, quest-result,
dialogue-result, and effect instances, stable `FormKey` reference resolution,
event dispatch, immediate world mutation, and a versioned save representation.
The prison wall now follows its shipped object script; the provisional M5
name-based opening path has been removed.

## Delivered runtime

### Typed virtual machine

`components/obscript/vm.*` implements the deterministic stack VM for M6's
native Program IR. It provides:

- signed integer, floating-point, string, and stable reference values;
- typed local initialization and conversion, including vanilla-compatible
  null-reference comparison with numeric zero;
- arithmetic, comparison, boolean, branch, call, load/store, conversion,
  discard, and return instructions;
- host callbacks for external variables, cross-script members, references,
  conditions, and commands;
- structured runtime diagnostics carrying the unit, event, command, source
  location, and monotonic dispatch sequence.

Execution is synchronous. A command's host mutation is visible to the next
instruction in the same Program, and a nested event completes before its
caller resumes. Runtime entry-point arguments are retained in Program IR.

### Main-thread contexts and events

`MWWorld::OblivionScriptManager` owns the runtime cache and is updated from the
ordinary engine frame loop. It compiles the official corpus once, indexes
standalone scripts and embedded results by stable identity, then provides:

| Context | Instance identity | Dispatch paths |
| --- | --- | --- |
| Object | reference `FormKey` | `OnLoad`, `GameMode`, `OnActivate`, `OnAdd`, `OnDeath` |
| Quest/global | quest `FormKey` | deterministic `GameMode` and start/stop state |
| Quest result | quest plus stable stage/entry unit | synchronous nested `SetStage` result |
| Dialogue result | INFO plus result ordinal | explicit selected-result dispatch |
| Effect | target plus effect-script unit | `ScriptEffectStart`, `ScriptEffectUpdate`, `ScriptEffectFinish` |

Quest scripts and active scripted references run in stable `FormKey` order.
`OnLoad` is stored per instance and fires once. Re-entrant dispatch has a hard
depth limit of 64 with a structured failure instead of recursive corruption.
Door and activator paths dispatch `OnActivate` before default behavior and use
a suppression guard when the script explicitly calls `Activate`, so the
default action occurs exactly once.

References are resolved through the content-aware `FormKeyIndex` and an
editor-ID index. `player`, `self`, `actionref`, SCRO operands, globals, remote
loaded or unloaded references, quest member variables, and object member
variables all use the same stable identity model. No runtime-index FormId is
written into persistent script state.

### Immediate M7 command surface

The host applies the M7 world and state operations in the current dispatch,
including:

- enable/disable/query, activate, move-to-marker, position/angle/scale,
  lock/unlock, cell and distance queries;
- `PlayGroup` and `IsAnimPlaying`, with animation state captured for reload;
- quest stage/running/completed-stage state and synchronous stage results;
- add/remove/count inventory and nested `OnAdd` dispatch;
- kill/resurrect/death queries and nested `OnDeath` dispatch;
- globals, typed cross-script locals, actor values, infamy, destruction,
  ghost, ownership-related state, linked path-point state, and the tutorial
  conditions reached by the official scripts;
- deterministic random-percent evaluation and message display.

Commands whose real effect belongs to later roadmap systems, such as full AI
packages, spell casting, faction simulation, weather, positional audio, and
topic presentation, remain explicitly marked `deferred` in diagnostic traces.
They are not reported as M7 world mutations and no M7 acceptance assertion
depends on them. Unknown commands still produce a structured runtime error.

### Persistent script state

The native TES4 state envelope is version 2. It stores the monotonic event
sequence, quest running/stage/completed-stage values, per-unit and per-context
instances, `OnLoad` state, and every local with its number/string/reference
type. Version-1 saves migrate with empty script state. A null reference uses a
dedicated empty value and is retyped from the Program declaration on restore;
it cannot become an empty editor-ID string.

Reference mutations and embedded object-animation state share the same
versioned envelope. Saves therefore preserve the wall's completed forward
animation, enabled state, quest results, inventory changes, remote reference
changes, and typed locals across a process restart and another save.

### Original tutorial path and embedded animation

The M5 hard-coded tutorial-wall branch, special display names, and collision
exception are absent. The shipped switch script on reference
`content:oblivion.esm:04e90d` receives `OnActivate`, evaluates the real action
reference and Charactergen stage predicates, and issues `PlayGroup Forward` on
itself and wall `content:oblivion.esm:01fc41` in the same dispatch.

Oblivion places this animation inside the NIF rather than a separate KF.
The NIF reader now resolves controlled-block palette offsets and transform
interpolators, the animation resource path can synthesize an embedded
sequence, and controlled collision nodes remain dynamic while the mechanics
update advances their collision transform. This is generic NIF/controller
behavior; it contains no tutorial reference or editor-ID check. Broader NIF
rendering and animation completion remains M8.

## Verification

### Synthetic and regression suites

The normal full build succeeds. The final automated results are:

| Gate | Result |
| --- | ---: |
| Component tests | 1,498/1,498 |
| Engine unit tests | 495/495 |
| Python compatibility/state tests | 22/22 |
| Focused ASan/UBSan ObScript and TES4 state tests | 24/24 |
| M6 official offline acceptance regression | passed, 11,098/11,098 units |

The focused VM tests cover typed coercion, branches, deterministic integer
boundary arithmetic, same-dispatch host visibility, stable failure context,
re-entrant completion, and null-reference semantics. Native and
Python state tests cover version migration, typed local round trips,
chunking, plugin reorder, corrupt/truncated/duplicate input, non-finite values,
and exact canonical state. Generated logs and XML are below
`build/oblivion-compat/m7-final-tests/`; the full M6 regression is below
`build/oblivion-compat/m7-m6-acceptance-final/`.

### Live official-content matrix

The checked-in `oblivion_m7_script_runtime.json` scenario starts the prison
with the canonical eleven official plugins and schedules events from
`m7_runtime_events.txt`. A clean process compiles 11,051 executable runtime
units with zero failures and zero runtime diagnostics. The report observes 50
distinct command/condition names while running continuous official quest and
object scripts.

The scheduled real-content matrix is deliberately broader than the tutorial:

- the tutorial switch object and its animated wall;
- Battlehorn quest stages 30, 40, 60, and synchronously nested stage 90;
- a Battlehorn dialogue result that adds the shipped reward;
- Knights, base-game, and Mehrunes Razor effect scripts;
- a remote Battlehorn lever object script and an explicit remote `GameMode`
  object event.

The trace proves depth-1 stage 30 dispatch, depth-2 stage 90 dispatch before
the caller continues, player and remote-container inventory changes, remote
disable operations, dialogue/effect context entry, action-reference matching,
and the tutorial's two ordered `PlayGroup` calls. The scenario forbids any M7
runtime diagnostic and the old M5 wall trace.

Durable evidence is below:

- `build/oblivion-compat/m7-runtime-acceptance-final/obscript-report.json`
- `build/oblivion-compat/m7-runtime-acceptance-final/process.log`
- `build/oblivion-compat/m7-runtime-acceptance-final/tutorial-wall-before.png`
- `build/oblivion-compat/m7-runtime-acceptance-final/tutorial-wall-after.png`
- `build/oblivion-compat/m7-runtime-acceptance-final/wide-script-matrix-complete.png`

The before frame shows the closed prison partition. The after frame shows the
open tunnel and the wall translated to its shipped final pose. Direct visual
inspection also confirms that this is animation rather than visibility
replacement.

### Save/reload and state inspection

The runtime scenario quicksaves after all scheduled events. A second process
loads that exact save through `oblivion_m7_script_reload.json`, remains free of
runtime diagnostics, captures the open wall, and saves again. The decoded
state before and after reload retains the script instance count, 414 quest
states, completed Battlehorn stages `[30, 40, 60, 90]`, effect-script typed
locals (including null references), remote mutations, inventory rewards, and
the wall animation's final state. The monotonic event sequence advances after
restore rather than restarting.

Evidence is below `build/oblivion-compat/m7-reload-acceptance-final/`, including
`obscript-report.json`, the decoded state reports, process log, resave, and
`tutorial-wall-after-reload.png`.

### Original-content oracle and Morrowind isolation

The tutorial comparison uses the unmodified script payload shipped in
`Oblivion.esm`, its stable SCRO references, and the accepted fixed-original
prison captures from M5 as the oracle. Additional Proton automation during M7
was not accepted as evidence because the final retained run stopped at
character creation; launcher, cinematic, loading, and character-creation
frames are deliberately not treated as a wall or script-trace comparison.
The M7 claim instead rests on the exact shipped switch Program executing in
OpenMW, its ordered runtime trace, and the visually inspected before/after
frames from that same process.

The Morrowind visual boot/save/load campaign also passes using the final M7
binary. Its save contains none of the Oblivion profile or `T4ST` markers, and
the process log contains no Oblivion service or script-manager activation.
Evidence is below `build/oblivion-compat/m7-morrowind-regression-final/`.

## Success assessment

M7's delivery, verification, and success criteria are satisfied. The native
Program runs synchronously in every M7 context; stable references, event
ordering, re-entrancy, numeric semantics, same-frame mutations, and typed
locals are exercised; the tutorial uses its original script and real embedded
animation; the M5 wall workaround is absent; and both script and world state
survive a process-restart save/load exactly.

There are no open M7 gates. Full rendering/animation asset coverage, AI,
magic, dialogue presentation, audio, weather, and other later subsystem
effects remain owned by their respective later milestones.
