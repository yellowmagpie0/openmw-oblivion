# Full sewer escape — current handoff

## Current checkpoint — wall crossed, 2026-11-04

The Player now crosses the crumbled wall with ordinary forward input. Native
multipart NPCs use a rounded capsule with the configured total height and
maximum horizontal width; the smaller horizontal extent expands to match the
radius. Navigation uses its conservative enclosing cylinder. TES3 NPCs and
creatures retain their previous shapes. This is a gameplay-supported hull
policy, not a claim of independently measured original capsule dimensions.
No collision disabling, state injection, dimension reduction or slope-rule
relaxation was used.

`native-npc-wall-crossing-01` passes with the normal configuration and no shape
override. Its inspected capture shows the Player inside the solo dungeon.
The genuine F5 save is `native-npc-wall-crossing-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
under `build/oblivion-compat/sewer-escape/`, SHA256
`040e99f6498c2c6f6649c1f6b1de6454be0892ec501ba930904037846e7084e0`. Player position is (586.08,3183.96,-853.38), health49.45;
both wall rats remain dead. Editable course:
`scripts/data/oblivion_compat/oblivion_sewer_wall_rounded_hull.json`.
The prior opt-in capsule experiment `player-capsule-01` also passes; the
experiment switch is removed from production.

Validation: `native-npc-hull-focused-01` passes all7 actor-bounds cases,
including rounded overhead clearance, unchanged full height/maximum width,
solid-wall blocking, floor support and legacy NPC/creature shape retention.
`native-npc-hull-engine-01` passes all2,158 engine cases, exact inventory,
no failures/skips; fingerprint
`d649ba8af87abf1d029171e5fb1cb3890c1756bb38d83b31e49dd66a643a1b09`.
Runtime SHA256:
`206e68cf9679a7d341bb95e3eea21511edb74e3a224a0aa795b1800cd9ba4f64`.
No fresh sanitizer acceptance for this chunk. All these processes are terminal.

Next: load the far-side save in a fresh process, approach original key corpse
15985a, exercise actual loot/equip and continue the solo dungeon to reunion.
An ignored ordinary-input course is prepared as `solo-key-approach.json`.
Full escape still requires fresh upstream activation/traversal, intermediate
and death/reload persistence, equipment and encounters, reunion, Emperor
sequence, quest-item handoff, character choices, actual exit and outside reload.
Earlier diagnostic failures and original-game evidence remain below.

## Historical wall-clearance investigation — 2026-11-04

The wall now releases thirteen positive-mass bodies after its Unequip animation.
The implementation replaces compound collision, removes the old navigation
obstacle, projects body poses into the renderer, captures schema85 snapshots,
and restores bodies through scene load. World retention/clear/swap and scene
removal include the new collection. Snapshot restoration is staged before old
collision is removed. Admission is limited to native ACTI references with
multi-body positive-mass keyframed assets; mass-zero doors are excluded.
Timing is an approximation supported by the original-game collapse observation,
not a claim of exact native handoff timing.

**Gameplay remains blocked at the opening, after verified rat combat.**
`wall-rat-combat-03` passes its normal-input course: Player punches kill both
rats037e41/037e42 (three damaging hits each), saved life phase Dead and Player
killer on both, Player health49.45. The inspected `rats-defeated.png` shows the
collapsed wall, corpses and armed Player. Quicksave SHA256
`6daea8437f6856768fc2c3b75a635059c89cbc09d798cc4774c19fe71aea96f0`.
The source is `scripts/data/oblivion_compat/oblivion_sewer_wall_rat_combat.json`.
A supplementary ordinary F5 input is recorded in `manual-input.json`; it retained
`approach-checkpoint.omwsave` at57.45 health, SHA256
`4a8d2de478e0b7e25f20ea8320b51adbd477baf82d659ef9d0c7f9dabb187137`.
The course's input pulses are not evidence of one simulation attack per pulse;
actual hit/death logs and saved state establish combat results.

Retained failures: combat01 changed the .omwscripts list and correctly failed
content admission; combat02 stalled below a fixed x300 attack trigger. Combat03
preserves the saved observer list, begins attacking atx275 and keeps approaching
while aiming. No game state, health or quest completion was injected.

`physical-wall-crossing-01` collapsed all13 bodies but straight walking stalled
near x309; its save had5.45 health. `physical-wall-jump-01` reloaded that save and
the rats killed Player. Following the successful combat, `cleared-wall-jump-01`
still stalls near (309.10,3191.65,-808.47): dead rats do not resolve clearance.
`cleared-wall-survey-01` passes read-only world collision rays. Movement bounds
are shape0 (box), half extents(29.28,28.48,66.5). Center rays at y3200 are clear
from z-810 through -680, but fallen wall body collision intrudes at y3210..3240,
z-700..-680, x~339; static passage edges also narrow the available lane. Rays
alone do not prove full-hull clearance or justify shrinking actor bounds.

Current clearance investigation: lane03/04/05 are terminal failures. Backing
away and aligning with ordinary controls succeeds, but forward movement still
snags. The genuine lane05 approach save is retained as
`cleared-wall-lane-05/lane-checkpoint.omwsave` (SHA256
`dac2d8bfc2a2d16c20f8e50a3f65ecabe13f7cfc6cf86b1efa5c6e318cff1e51`),
with both rats dead and Player health49.45. Use it for repeatable clearance
investigation without repeating combat.

Opt-in `OPENMW_TRACE_PLAYER_OBSTACLES=1` now reports bounded Player contact
and up/forward/down step traces; movement response is unchanged.
`player-obstacle-01` identifies a static frame contact (026bf6) at the lower
corner of the default box. `player-cylinder-01` uses the existing cylinder
setting with unchanged dimensions; it also fails to cross, contacting fallen
wall03e31c. This is a diagnostic configuration, not a production shape change.
`player-step-trace-01` builds the diagnostic runtime and passes13 selected
animated-reference tests, fingerprint
`c377052c38829b165d00c3084543e98748338d51ce3b5934edb4f0f216ec10a3`.
These tests do not establish movement acceptance.

`player-step-diagnostic-01` repeats the cylinder approach with phase logging.
At center(308.732,3172.56,-746.889), the upward sweep fraction0.0366924
allows only1.25 units of the34-unit step before overhead contact. Forward and
downward sweeps hit the brick face with normal approximately
(-0.970,0.042,0.241), which is not a walkable landing. This explains the
rejected step; increasing step height alone is not supported. The run is terminal failed at its checkpoint wait; its contact traces and
inspected screenshot establish the snag, not traversal. All diagnostic processes
are now terminal.

Next: establish appropriate TES4 actor hull clearance against this geometry,
using the original-game passage evidence and a focused collision fixture. Do
not shrink dimensions to fit one opening or relax slope acceptance blindly.
Remaining wall acceptance: fresh activation, actual crossing, fresh-process
persistence and cell unload/load. Then continue solo-dungeon equipment/loot/
combat, reunion, Emperor sequence, character choices and the full escape.

Validation under `build/oblivion-compat/sewer-escape/`:
- `animated-world-focused-01`: all13 selected cases pass, exact inventory,
  no failures/skips; rebuilds game/esmtool. Includes mass-zero/nonkeyframed
  exclusions and corrupt-snapshot rollback preserving original collision.
- `animated-world-engine-01`: all2,157 engine cases pass, exact inventory,
  no failures/skips. Both use fingerprint
  `7ad14484251fd5bac3a9e61129ee1d2632f0466338e49d7ef8a73d2cc7058af6`.
- Runtime executable SHA256:
  `5f74817a4f4243cd4e9a7665a2b808370b96e9e98cc7d50a9196517df908ebaa`.
- Initial manual build01 failed on a scoped-enum spelling, corrected in build02;
  failed evidence is retained. No new sanitizer acceptance for this chunk.

Reusable dependency commits: `59ed63d344` collision handoff/lifecycle,
`59fb84e5af` schema85 C++/Python codecs, `00fe61320f` scheduler ownership,
`323ba551ca` original rat Find-package activation. Original-game evidence:
[SEWER-WALL-NATIVE-OBSERVATION.json](SEWER-WALL-NATIVE-OBSERVATION.json).
The earlier lane runs and diagnostic builds are terminal; current runtime status is above.

## Target and acceptance

Play the original tutorial continuously from the prison opening through the
sewer exit, using ordinary controls and original quest progression. Include
usable equipment/loot, required combat and traversal, escort reunion, Emperor
sequence, quest-item handoff, character choices, final exit and fresh-process
save/load outside. Verify death/reload and intermediate persistence. No console
progression, injected completion state or skipped required encounters qualifies.
Commit reasonably self-contained chunks; do not push.

## Current chunk — route audit, 2026-11-04

The checkout was clean at entry. No implementation changes or new runtime
acceptance yet. First deliverable: traverse the crumbling wall and solo dungeon,
acquire/use equipment, survive encounters and rejoin the escorts from the genuine
stage-30 checkpoint. Expand dependencies only when this course exposes blockers.

Original installed Oblivion.esm SHA256:
`a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`.
Read-only extracted SCPT/QUST source inventory is retained locally in
`build/oblivion-compat/sewer-escape/original-route-audit.json`.
This is content inspection, not compiler or runtime acceptance.

Findings from original records:

- `CGCrumbleWall01SCRIPT` (03faf5) responds to non-Player activation,
  animates the wall/dust, sets MQ01 stage 24 and enables linked path points.
  Determine its normal trigger and verify collision actually opens the route.
- `CGTrigZoneAmbushBSCRIPT` (0c7604) starts Charactergen stage 32.
  CharGenQuest advances 34 to 36 after two assassin deaths; reunion requires
  the Ambush B room flag and detection by Glenroy or Baurus before stage 40.
- CharGenQuest invokes birthsign selection at 43, then continues escort stages
  50–73. Its late ambush dynamically spawns assassins. This needs real actor
  lifecycle, combat, authored deaths and persistence verification.
- `CGSewerExitScript` (00a396) offers race/birthsign/class revisions and exit.
  The exit choice sets **MQ01 stage 88** and activates the door. It also invokes
  weather, crime reset and conditional rat resurrection. Charactergen stage 88
  alone cannot establish that the Player escaped.
- Corpse activation already opens inventory in `esm4interactive.cpp` and
  `esm4npc.cpp`; character-menu commands already dispatch in
  `oblivionscriptmanager.cpp`. Inspect and exercise these before implementing
  replacements. Existing support is not proof of this route.

## Reusable evidence and next action

Opening through first ambush and escort departure is accepted separately in
[TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md](TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md)
and its linked verification artifact. Preserve original evidence provenance.
The genuine stage-30 save is
`build/oblivion-compat/tutorial-melee/player-stage27-moving-continuation-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.

Next: inspect the wall trigger/reference placements and stage-30 Player position;
create an ordinary-input continuation course from a private copy of that save.
Repair the first observed blocker, use focused checks, then verify the playable
segment and fresh-process persistence. Later checkpoints are the Emperor
sequence and final sewer exit. Broad normal/sanitizer gates belong at integration
checkpoints; reuse applicable unchanged evidence. All full-escape acceptance
requirements remain open.

### First approach diagnostic

`wall-approach-01` terminates successfully with no forbidden diagnostics, but
**does not establish traversal**: Player moves only about 15 units in Y and
heading remains -1.296875. Inspected capture shows Player still beside the
initial pillar. The initial course waited for state application, which is too
early to establish input readiness. The editable `oblivion_sewer_wall_approach.json`
now waits for the post-load GetStage=30 query; this revision has not run yet.
Saved state extraction is retained in that run's `saved-state.json`.

Original PACK 07303d (`CGRatAmbushAPushBricks`) targets wall reference 03e31c;
the rat's authored package, not Player activation, opens the wall. No actor in
the diagnostic saved AI inventory selected that package. Investigate rat
placement, enablement and package selection after correcting input readiness.
No implementation blocker has yet been conclusively demonstrated.

### Follow-up diagnostic

`wall-approach-02` also completes without forbidden diagnostics but does not
reach the wall (Player Y=2325.72, unchanged heading). Readiness alone did not
resolve the issue. MouseManager routes motion into vanity camera before Player
yaw; its sensitivity is divided by 256. The course now wakes movement before
turning and uses the configured 0.05 sensitivity; this revision is unrun.

Both rats (037e41/037e42, base 04e870) are enabled and selected Wander 04f237
in run01. Original Find package 07303d targets the wall and requires completed
Charactergen stage29, which is present in the save. Code inspection identifies
an unresolved implementation gap: Find Act preserves NPC conversation but
falls through to completion for activators without script dispatch. Establish
this with a focused regression and original-content evidence before fixing;
then rerun upstream progression if the old save has consumed that action.
Relevant fixture helpers are in `apps/openmw_tests/mwworld/testobliviondialogue.cpp`;
existing UseItemAt activator dispatch is in `oblivionai.cpp`.

### Find activator repair in progress

`find-activator-red-01` confirms zero activation callbacks for a resident Find
action. The new regression checks actual actor identity, low-process pending
state and no repeat callback after binary state restoration in both load layouts.
`find-activator-focused-01` passes all 8 selected tutorial-action/conversation
cases with the first dispatch fix. It is not full-engine or runtime evidence.
A subsequent change also preserves activator-targeted Find actions in the
fully unloaded path. Next check is focused-02 with a rebuilt game binary;
then exercise the original rats/wall and investigate consumed old-save packages.

`find-activator-focused-02` passes all 8 selected tests and rebuilds openmw/esmtool.
`wall-approach-03` still does not traverse: wall locals remain zero, rats retain
HoldPosition, Player heading remains unchanged. InputWrapper::_packageMouseMotion
explicitly discards its first relative motion. The editable course now sends
zero motion before the actual turn; run `wall-approach-04` is the next diagnostic.
Earlier vanity/readiness explanations were insufficient and are not fixes proved
by those runs. Runtime wall activation remains unaccepted; no source commit yet.

Run04 also fails to move/turn as intended; zero-motion priming alone was
insufficient. InputWrapper discards mouse/button input during loading, while
the script acceptance query can precede the first Player frame. The current
course adds a read-only onFrame readiness signal and waits for it before inputs.
This revision is unrun. No process remains running from runs03/04.

Run05 proves onFrame readiness, all Player control switches enabled, camera
mode ThirdPerson, but still unchanged heading. Mouse-course fixes remain
unproven. Current course instead uses right-stick turning until a read-only
north-facing signal, then stops the stick; unrun. Run05 is terminal.
Further likely gameplay blocker: executeUnloadedFixedStep and resident update
only periodically reselect terminal packages or null waits. Rats are in active
Wander Wait, so higher-priority newly eligible push package may never preempt.
Add a focused conditional-selection regression before changing that policy.

Run06 fails harness preflight (missing virtual_gamepad), corrected in run07.
Run07 reaches first-frame readiness but times out turning north via controller.
Do not call this a movement pass. Next inspect actual rotation input handling,
including updateLuaControls and native CharacterController movement ownership.
`wander-reselect-red-01` fails fixture state validation (Base package retained
script-package list index), not the intended assertion. Fixture now sets list
index and procedure consistently; rerun before implementing selection changes.

### Sunken-room checkpoint

Focused03 passes 9 tests; `find-wander-engine-01` passes all 2,144 engine tests.
`wall-approach-09` passes heading and movement gates through ordinary input.
Inspected screenshot and save show Player (27,3136,-827) in the sunken room.
Both original rats select Find 07303d and reach approximately (447,3200,-810),
but stay in Path: wall collision prevents reaching the generic 64-unit arrival
radius. Their distance to wall origin is still inside activation range.
Current change extends Find arrival to valid activator range; regression now
starts 100 units away to cover it. This latest change is not yet tested.
Run09 save is the next genuine continuation checkpoint. Earlier short real-time
input windows were insufficient; controller turning is now proved working.

### Passage traversal in progress

`wall-crossing-01` terminates failed at its crossing gate, retaining screenshot
and genuine F5 save. Player stopped near x42,y3101, before the wall at x384;
this is not evidence of a crumbling-wall collision defect. Original corner wall
03e328 lies at (128,3072), with adjacent floor corridor toward y3264. Current
`oblivion_sewer_wall_crossing.json` strafes north to y3260, aligns east using
ordinary controller input, then attempts crossing with normal collision.
`wall-crossing-02` continues from the pristine wall-open-01 save.
Wall-open state already contains both rat-to-Player combat engagements; the
terminal Find state is retained while combat owns their movement. Do not infer
an additional package-selection defect from that retained state.

`wall-crossing-02` is terminal failed at crossing; genuine F5 checkpoint is
Player (302.13,3349.92,-817.30), health61.45. Inspected image clearly places the
opening south/right of the Player. The north alignment cleared the initial
corner but overshot the opening's centerline. `wall-center-crossing-01` is the
next ordinary-input continuation from that diagnostic save, strafing south to
y3200 before advancing east. Editable source:
`oblivion_sewer_wall_center_crossing.json`. No new engine defect established.

`wall-center-crossing-01` fails its south-alignment gate: x305,y3273 stops
before y3200. `wall-survey-01` passes a read-only 180-ray collision survey;
its editable manifest is `oblivion_sewer_wall_collision_survey.json`.
From x280 at y3200, rays at z=-820,-780,-740 reach x700 unobstructed; z=-700
hits the animated wall03e31c near x339. At y3240 and north, static broken
wall026bf6 blocks near x339 at torso height. At y3200,z=-820, starting x0
hits loose static brick03fb0a near x192. This establishes actual geometry,
not a player-sized clear corridor. Next route must negotiate rubble toward
y3200 (normal jump is E), then attempt the gap. If body clearance still fails,
compare animated collision/rendering at the retained end pose before changing
engine behavior. No running process remains from center-crossing01/survey01.

`wall-rubble-jump-01` observes genuine E jump (fatigue140->110), but southward
movement still stops near x301,y3273; run remains active until its diagnostic
capture/save. `wall-survey-02` extends read-only rays southward: at x260/280/300,
z=-820/-780/-740/-700, the first static hit is around y3121, beyond the desired
y3200 alignment. Point rays do not include the Player's width; near x302 the
body can graze the east wall. Next ordinary-input route should back west to
x250 before strafing south, rather than jump against the wall edge.
The extracted original wall asset (ignored wall-assets/idcrumblewall01.nif)
has named brick NiTransformController tracks; dynamic-body absence alone does
not establish why the remaining opening is narrow. No collision fix justified.

## Historical dependency checkpoint — before World integration


Production commit `323ba551ca` makes the original rats select their wall-push
package and activate the wall. `find-activator-range-01` passes nine focused
tests and rebuilds the game; `find-range-engine-01` passes all 2,144 engine tests.
No fresh sanitizer gate for this production chunk yet.

Normal-input `wall-open-01` proves original activation, MQ01 stage24 and wall
locals reaching pushed=3. Full sewer escape remains open: wall traversal,
solo-dungeon combat/loot, reunion, Emperor sequence, character choices, final
exit and save/load acceptance are not complete.

Current blocker: `wall-center-crossing-02` backs west from the legitimate
`wall-crossing-02` save, successfully strafes south onto the opening centerline,
then stalls near (278,3196,-831). It is terminal failed, with a screenshot but
no confirmed new F5 save. Stop repeating blind movement attempts.

`wall-survey-03` passes a denser read-only collision survey including actors.
At y3200, animated wall03e31c obstructs x~340 at z=-800,-790,-770,-760,-750,
-720,-710,-700; rats are farther behind it. Earlier coarse rays missed these
bricks. Screenshot shows suspended bricks across the gap. Neither survey alone
proves full Player-volume clearance or diagnoses the engine cause.

Independent original-game probe now establishes the discrepancy: direct
`PlayGroup Unequip 1` collapses the bricks onto the floor, with GetDestroyed=0.
Normal forward input reaches x352.47 inside the opening. Exact native handoff
timing and full crossing beyond x550 were not measured. Console setup is
explicitly diagnostic, not normal tutorial progression evidence.
See [SEWER-WALL-NATIVE-OBSERVATION.json](SEWER-WALL-NATIVE-OBSERVATION.json).
Original game and the temporary Steam client exited; the isolated configuration
and saves were restored, including the unchanged pristine-save hash.

The original asset contains thirteen positive-mass keyframed bodies and a
one-second lateral jostling clip, with no authored collapse-to-floor track.
Root transforms are identity; no physics-blend controllers are attached.
The prison secret wall has mass-zero keyframed bodies with the same collision
flags. A blanket release of every animated keyframed object is not justified.
Raw extraction, transform inspection and original instruction diagnostics stay
under ignored `wall-assets/`; parser output is content evidence, not an
independent instruction oracle. The live original-game result supplies the
independent behavioral evidence.

The scheduler now has a separate multi-body owner for non-actor placed
references. It reuses the existing native body simulation and graph snapshot
adapters, participates in worker stepping and collision queries, preserves
reference identity on transfer, and rejects conflicting actor/item ownership.
Invalid motion preparation publishes nothing; corrupt snapshot restoration
preserves the existing owner. Destruction/removal clears all owned bodies.
The snapshot value type is shared with the actor body-graph representation;
the owner collection remains separate and does not pretend the reference is
an actor. Schema85 now adds a dedicated saved collection, described below. World capture,
restore and gameplay callers are not wired yet.

Validation: `animated-reference-focused-02` passes six cases over zero, one and
two workers. `animated-reference-scheduler-01` passes all 263 selected scheduler
and loose-item cases, exact inventory, no failures/skips. Both use source
fingerprint `e97988fda6dee20a83a0e597d088d4ac14bbf71c75c88039c8feac139d951a41`.
Focused01 is retained failed: the corruption test expected invalid_argument,
while the existing validator correctly raised runtime_error; live state remained
unchanged. No runtime executable rebuild, broad engine or sanitizer claim for
this dependency chunk.

The C++ and Python schema85 codecs now persist `native_animated_references`
with placed-reference ownership, base/model/asset identity and full body graphs.
Old versions retain their binary layout; no physical history is invented for
legacy saves. Reviewed fixture upgrades add an explicit empty collection.

Validation under `build/oblivion-compat/sewer-escape/`:
- `animated-codec-regression-01`: all 3,242 component cases pass with exact
  inventory, no failures/skips; source fingerprint
  `d3ab16483faac6e3cda0cdda577fcf54eaa150682bdfb41705cd6597895af90d`.
  The combined run remains failed because one Python assertion still treated85
  as future. That assertion now accepts85 and rejects86.
- `animated-codec-python-02`: all551 Python cases pass, fingerprint
  `80681fb36612ebe0fd95f5d0c031fab7c443e73170b25663695cf4f97df0b1cf`.
- `animated-codec-cross-language-01`: Python/C++ byte-identical roundtrips for
  legacy84, empty85 and a two-body animated85 snapshot. Focused tests also
  exercise missing/mismatched owners, duplicate bodies and downgrade rejection.
Earlier failed checks remain retained. No runtime or sanitizer acceptance is
claimed for this format chunk.

PhysicsSystem now exposes the independent body owner and replaces existing
compound collision only after successful scheduler preparation, under one world
lock. Removal and Ptr transfer include the new owner; conflicting static/actor
admissions are rejected. `animated-physics-handoff-01` passes all12 selected
cases over zero/one/two workers; `animated-physics-engine-01` passes all2,156
engine cases with exact inventory and no failures/skips. Both use fingerprint
`a25df06cdd9412e10cea33621a1e85ea26d46fa286f52cc527099c00cbee008b`.
No game-binary rebuild or runtime/sanitizer acceptance is claimed for this chunk.

Next: connect completed positive-mass reference animation to the new scheduler
owner and existing prepared physical-pose rendering binding. Wire World capture,
retention, restore and clear/swap lifecycle to the new collection; preserve bodies
through cell unload/load and replace old compound collision only after successful
preparation. The dedicated PhysicsSystem handoff is ready. Remove the old navigation
obstacle when replacing collision and retain physical state on scene removal.
Keep mass-zero doors/switches on their existing path.
Then prove actual wall traversal from `wall-open-01` or fresh activation from
`wall-approach-09`, followed by the rest of the original tutorial. Full sewer
escape remains incomplete. All diagnostic/check processes are terminal.
