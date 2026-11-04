# Full sewer escape — current handoff

## Current checkpoint — original wall activation, 2026-11-04

Find packages can now dispatch placed activators after actor iteration, preserve
pending activation across low process/restore, and stop within valid activation
range. Interruptible waiting Wander packages periodically reselect, allowing the
original rats to leave HoldPosition for their newly eligible wall-push package.

Validation: `find-activator-range-01` passes 9 focused action/conversation tests
and rebuilds the game; `find-range-engine-01` passes all 2,144 engine tests with
exact inventory. Reports and source fingerprints are under
`build/oblivion-compat/sewer-escape/`. No sanitizer gate for this chunk yet.
Earlier failed diagnostics and fixture failures remain retained below.

Normal-input `wall-approach-09` reaches the sunken room. Fresh continuation
`wall-open-01` passes original wall activation, MQ01 stage24 and normal controller
turning. Wall locals progress to pushed=3, linked path points enable, and the
Player remains alive at health61.45. Inspected capture shows displaced blocks,
but does not prove a Player-sized opening or traversable collision. Both rats
activated the wall; its original script guard runs stage24 only once.

Next: continue from `wall-open-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
verify restored wall state without replay and walk through the opening with
normal collision. Inspect/fix animation or collision if the visible narrow gap
blocks traversal. Then complete solo-dungeon combat/loot and escort reunion.
Full escape, post-wall combat, character choices, exterior exit and final
save/load acceptance all remain open. These checkpoints are not full completion.

Input diagnosis: controller yaw is working; use observed heading/position gates
because simulation progresses slowly under software rendering. Earlier fixed
real-time input durations did not prove an engine input defect. The editable
approach and wall-opening manifests retain ordinary input and read-only observers.

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
