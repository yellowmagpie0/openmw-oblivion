# Full sewer escape — current handoff

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
