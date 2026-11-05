# Full sewer escape — current handoff

## Current checkpoint — Natural Caverns entered and reloaded, 2026-11-04

Full escape remains incomplete: goblin dungeon, escort reunion, Emperor/Amulet,
character choices, actual MQ01 stage88 exit, outdoor fresh reload and continuous
full playthrough. Death/reload is verified. Current chunk: traverse original
Natural Caverns encounters and reach the escort reunion.

`substructure-lower-loop-01` PASSES original raw nodes45–54 from the healed
checkpoint. `substructure-lower-rat-01` PASSES ordinary approach/Club attacks:
rat159832 dead with Player killer, no combat engagements, health44.12.
`encounter-audit.json` and capture inspected; save SHA256
`6ad1ae04630a63d3276e833df3afb4425f870ede37e24f0e49038e0f8b1dce37`.
`substructure-last-rat-approach-01` FAILS after rat038174 engages and knocks
Player off waypoint6. Retained terminal failure, not a continuation save.
Corrected approach02 ends at raw node59 before the attack; PASSES and saves
health44.12. `substructure-last-rat-01` PASSES ordinary stationary defence:
038174 dead with Player killer, both lower rats dead, no remaining engagements,
health40.359993. Save SHA256
`4ebb5bc791edf86e0def46b52724f7cb22c863ade323193b7660abf8954fdb15`.

`substructure-cavern-door-approach-01` PASSES raw nodes60–77 to the original
Wood Door. `substructure-cavern-door-01` PASSES normal focused Space activation
of0381b8 and actual cell transition into Natural Caverns0380bd.
`natural-caverns-reload-01` PASSES fresh-process destination load and resave.
Its `transition-restart-audit.json` checks unchanged Player health/inventory
and both lower rats' life/value records through traversal, cell transition and
reload. All passing courses have no forbidden/unreviewed errors. Relevant
captures inspected, including actual destination. Existing2167 engine evidence
and runtime binary are reused; no engine changes or new sanitizer claim.
Creature corpse poses remain a known visual limitation.

Next input save:
`build/oblivion-compat/sewer-escape/natural-caverns-reload-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `374edfff2d4076736d3a9b74d84893e016fd70796fe9ef6fa97eb21b51e2f174`.
Player cell0380bd, position(-365.91,-13.12,40.27), yaw1.500001, pitch0,
health40.36, Club and armor equipped. All runtime processes are terminal.
Next: inspect original cavern pathgrid from this entrance and approach the first
goblin encounter using ordinary input; pause traversal to fight when engaged.
Reusable original actor/reference inventory:
`build/oblivion-compat/sewer-escape/goblin-section-record-audit.json`.
Known goblins03b526 near(147,883,-287),03b513 near(788,1813,-251), patrol03b543,
log-runner03b518, runover03b51a, boss03b52e and others. Original cavern exit03816c
at(4384,6144,64) points to026c36; destination/reunion route still needs inspection.

Editable sources under scripts/data/oblivion_compat:
`oblivion_sewer_substructure_lower_loop.json`, `...lower_rat.json`,
`...last_rat_approach.json`, `...last_rat.json`, `...cavern_door_approach.json`,
`...cavern_door.json`, and `oblivion_sewer_natural_caverns_reload.json`.
Input chain is healed-reload01 -> lower-loop01 -> lower-rat01 ->
last-rat-approach02 -> last-rat01 -> cavern-door-approach01 -> cavern-door01 ->
natural-caverns-reload01. This is composite evidence, not a continuous full
prison-to-outdoors acceptance run.

## History — western passage and healing sack complete, 2026-11-04

Full escape remains incomplete: lower solo-dungeon loop, goblins, reunion,
Emperor/Amulet, character choices, actual MQ01 stage88 exit, outdoor fresh
reload and continuous full playthrough. Death/reload is verified. Current chunk:
return to the stairs and traverse the lower loop toward Natural Caverns.

`substructure-west-root-detour-01` PASSES18 ordinary-input waypoints around
hanging roots and through the western passage to the stairs at raw node45.
`traversal-audit.json` verifies unchanged Player health24.12 and inventory.
Save SHA256 `825cb7bfbe11a39d6c51d68b04cc91121116e11ec288ae16375c4169f9812366`.
Its input is the normal backward/left `substructure-pillar-step-01` save.
The detour goes north to(-3538,4230), east(-3450,4230), south(-3450,3910),
west(-3700,3910), then rejoins original raw nodes32–45.

`substructure-west-sack-01` PASSES normal focused Space activation of sack15983b.
`loot-audit.json` verifies exactly Healing009310 x1 and lockpicks x3 transferred,
with empty saved sack. Save SHA256
`7992630887760b2e7ecc19e3d06b47218eefc11d4b3f536ab06824e6f551fe25`.
This uses existing immediate container transfer, not full container-menu acceptance.
`substructure-west-healing-01` reuses the existing inventory healing manifest:
normal menu input consumes the potion and restores health24.12->44.12.
`substructure-west-healed-reload-01` PASSES fresh-process reload and F5.
`healing-restart-audit.json` verifies exact Player values/inventory, native actor
life records and empty sack; consumed potion does not reappear. Captures inspected.
All passing courses have no forbidden/unreviewed errors. No engine changes;
existing2167-case engine evidence and runtime binary remain applicable. No new
sanitizer or full-escape acceptance claim.

Next input save:
`build/oblivion-compat/sewer-escape/substructure-west-healed-reload-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `6da178061387e2b744e3c32a83939bf66cfc3823d5c4bddca9ee9310e8dc7273`.
Player(-4919.25,2896.73,-377.02), health44.12, Club and armor equipped;
camera pitched down0.889 toward sack. Return toward node45(-5035,2909,-382),
then nodes46 onward to the lower loop. Two remaining known rats038174/159832
are near(-3083,2303,-448)/(-3972,1923,-441) in earlier save observations.
Exit0381b8 at(-1998,891,-421) leads to Natural Caverns0380bd.
All runtime processes are terminal. No testing/build job active.

Editable sources: `oblivion_sewer_substructure_pillar_step.json`,
`...west_root_detour.json`, `...west_sack.json`, `...west_healed_reload.json`,
`...pillar_volume_survey.json`, under scripts/data/oblivion_compat.
Healing uses existing `oblivion_sewer_substructure_healing.json`.

Retained failures: `substructure-west-pillar-clear-01/02` and `...pillar-jump-01`
stop at the roots; jump input did spend30 fatigue but did not clear them.
Point-ray survey01 misses narrow roots; radius24 volume survey01 identifies
original RootHavok01 base06035d/ref0493f0 and RootHavok05 base06035a/ref051dbf.
Raw installed-master records independently identify those model names.
Normal detour succeeds without collision changes. Whether these hanging roots
should yield physically remains an adjacent unverified fidelity question.
Manual diagnostic actions and failed commands are retained in clear01's receipt;
its attempted final backward-step save was not successful. Do not use that as
route acceptance. The ignored wide-course generator initially failed with a
NameError before launch; corrected preparation produced clear02's retained failure.

## History — stair rats defeated, western pillar approach, 2026-11-04

Full escape remains incomplete: remaining solo dungeon, goblins, reunion,
Emperor/Amulet, character choices, actual MQ01 stage88 exit, outdoor fresh
reload and continuous full playthrough. Player death/reload is verified.
Current chunk: negotiate the western pillar, reach healing sack15983b, then
continue through the lower loop to Natural Caverns. No engine edits this checkpoint.

`substructure-stair-rats-01` PASSES ordinary Club defence against original rats
038175,038818,159838. Saved native life records all three dead with Player killer,
no residual combat engagements, Player health24.119995. Encounter audit and
capture inspected; upright creature corpse presentation remains a known gap.
Save SHA256 `923c274d65621092e8f46cb23381fc6c2b36384c8f6946dc900debc91e795fca`.
Editable source: `scripts/data/oblivion_compat/oblivion_sewer_substructure_stair_rats.json`.

Fresh-process `substructure-west-healing-approach-04` verifies those deaths,
native values, entire Player inventory and health survive reload/traversal.
Its `restart-audit.json` is persistence evidence only: the route FAILS before
waypoint4 (raw path node32), stopped at a pillar after reaching raw nodes29–31.
Inspected `passage-diagnostic.png` shows the obstruction. Normal X11 key release
and F5 retain a diagnostic save, with receipt in `manual-input-receipt.json`.
Runs01–03 fail initial facing;04 adds additional measured startup mouse
corrections and gets past that gate. All failures remain retained. Course heading
checks can match earlier observations; review current facing before walking.
No runtime/build process remains active. Existing2167 engine-test evidence is
reused; executable SHA256 remains
`9b8ba0c0ff592a76925fb475b3cb5f6e08d6ff7b0d6c96dd4805848b28618d8a`.
No new sanitizer or whole-sewer acceptance claim.

Next input save:
`build/oblivion-compat/sewer-escape/substructure-west-healing-approach-04/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `0aa2a050c838d4ec1d7c94c700210fcee65fa5fdcaf9b5ec234de1b38f56dae3`.
Player(-3587.25,4102.00,-249.20), health24.12, Club and armor equipped.
Next: correct heading/clear the pillar toward node32(-3671,4041,-250), then
raw nodes33–45. Do not repeat the failed unchanged path. Healing sack15983b is
at(-4842.525,2892.996,-355.115), near node45(-5035,2909,-382), containing
Healing009310 x1 and lockpicks x3. Loot normally, consume via inventory and
verify +20 health/persistence. Prepared ignored sack course exists but is unrun:
`build/oblivion-compat/sewer-escape/substructure-west-sack-prepared.json`.
Continue lower-loop rats038174/159832 and exit0381b8 to Natural Caverns.

## History — western stairs reached, new rat ambush pending, 2026-11-04

Full escape remains incomplete: western loop/solo dungeon, goblins, reunion,
Emperor/Amulet, character choices, actual MQ01 stage88 exit, outdoor fresh
reload and continuous full playthrough. Player death/reload is verified.
Current chunk: clear the active three-rat encounter at the stair exit, then
continue along the original western loop. No new engine changes this checkpoint.

`substructure-west-room-entry-01` PASSES raw nodes18/19 from the pillar save.
Rat038176 remains initially passive. `substructure-west-room-rat-01` fails:
a short approach leaves it out of reach and all six swings miss. `-02` PASSES
with a normal movement wake-up, heading corrections and one-second running
approach before Club attacks. Native save records rat038176 dead with Player
killer, health50.44 and no residual combat. Capture and encounter-audit.json
inspected. Rat038175 was still alive; this does not clear the entire room.

`substructure-west-stairs-01` PASSES fresh reload and eight raw pathgrid corners
(nodes20..28, excluding vertical duplicate25). Read-only startup verifies
rat038176 death and saved health. Further original rats engage near the end.
Normal F5 retains health46.68 at(-3388,4478,-259), with engaged rats038175,
038818 and159838. The post-save capture has lower health from later attacks;
use the actual save as the continuation. `encounter-restart-audit.json` verifies
unchanged dead rat values/killer and inventory contents/equipment, with only
legitimate worn-cuirass condition loss. Health is not claimed unchanged through
this route. Capture inspected; all passing runs have no forbidden/unreviewed
errors. Existing2167-case engine evidence is reused; no new sanitizer claim.

Next input save:
`build/oblivion-compat/sewer-escape/substructure-west-stairs-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `de6e1dddc1cdb6d183d8d95f1fcf2a86034d84c45ca17e3a834ba7c04a435244`.
Player health46.68, first person, level camera, Club and armor equipped.
All runtime/build processes are terminal. Next: ordinary Club defence against
038175/038818/159838, retaining a selected target until dead to avoid spreading
hits. Use the successful world-heading observer, not animated bounding-box aim,
and no arbitrary distance gate. A short movement input can wake mouse look.
Then resume raw pathgrid nodes29 onward toward the western/southern loop;
rat038174 remains near(-3859,1934), and Natural Caverns exit is0381b8.

Editable passing manifests under scripts/data/oblivion_compat:
`oblivion_sewer_substructure_west_room_entry.json`,
`oblivion_sewer_substructure_west_room_rat.json`,
`oblivion_sewer_substructure_west_stairs.json`. Inputs are respectively pillar01,
room-entry01 and room-rat02 saves. Keep the raw route's pillar/stair corners.

Adjacent observation, not an isolated defect yet: missed swings in room-rat01
did not alert the nearby rat. The awareness adapter reads the transient attack
request flag; native attack-phase noise may need follow-up if it blocks gameplay.
Do not tune detection coefficients or force combat to compensate.

## History — western rats defeated and pillar passed, 2026-11-04

Target remains the complete original prison-to-outdoors course. Remaining:
western room/solo dungeon, goblins, reunion, Emperor/Amulet, character choices,
actual MQ01 stage88 exit, outdoor fresh reload, continuous full playthrough.
Player death/reload and the northern/western rat encounters are verified.
Current chunk is original-room traversal/combat; no new engine changes.

`substructure-west-approach-01` PASSES raw nodes1..6 from the healthy northern
reload save; health65.48. `substructure-west-encounter-entry-01` PASSES node7
and F5 before damage, admitting original rats038171/038172. Both captures and
native save audits inspected. This avoids the failed long course's waypoint
overshoot during combat.

`substructure-west-rats-combat-01` was explicitly stopped (exit130): an arbitrary
170-unit harness wait prevented swings while rats attacked at about192 units.
Retain stopped-run.json. `-02` PASSES after removing that wait: normal turns and
Club attacks kill both original rats, with native Player killer for each.
Health56.08, Club condition61.18; all three northern rats now dead and no residual
combat engagements. Extra swings after victory are misses, not additional hits.
Screenshot and encounter-audit.json inspected; upright creature corpse poses
remain an existing visual limitation.

`substructure-west-pillar-01` PASSES fresh-process restoration and ten raw
pathgrid corners (nodes8..17), clearing the earlier pillar obstruction and
entering the western room. `encounter-restart-audit.json` verifies unchanged
native rat values/deaths/killers, whole Player inventory/equipment/condition and
health through traversal. Capture inspected; all passing runs have no forbidden
or unreviewed errors. Existing2167-case engine evidence and binary identity are
reused from the acquisition/perception implementation below; no new sanitizer
or whole-sewer acceptance claim.

Next input save:
`build/oblivion-compat/sewer-escape/substructure-west-pillar-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `ec5a53cccfd8b719df22b07a760be00684d4c2d2b2d46d1d81973cb7becb7eb0`.
Player(-1764,4803,-378), facing west, level camera, health56.08, Club and armor.
All runtime/build processes are terminal. Next: raw nodes18/19 toward
(-1967,4669),(-2198,4680), stop for rat038176 near(-2414,4615) and potentially
038175 near(-2481,3913). Fight before continuing up the next stairs. Original
Natural Caverns exit remains0381b8, reached later through the western loop.

Editable passing manifests in scripts/data/oblivion_compat:
`oblivion_sewer_substructure_west_approach.json`,
`oblivion_sewer_substructure_west_encounter_entry.json`,
`oblivion_sewer_substructure_west_rats_combat.json`,
`oblivion_sewer_substructure_west_pillar.json`. Each uses the immediately prior
passing save listed above. These are genuine original-course continuations.

## History — northern combat and Player death/reload verified, 2026-11-04

Target remains the full original prison-to-outdoors course. Remaining acceptance:
western solo dungeon/goblins, reunion, Emperor/Amulet, character choices, actual
MQ01 stage88 exit, outdoor fresh reload, and the continuous full playthrough.
Player death/reload is now verified. Next chunk is the western two-rat encounter.

Implementation `6698d97448` enables ordinary hostile Player acquisition and
movement-aware perception. Four focused and all2167 engine tests pass; tested
fingerprints, binary identity and approximation limits are in the history below
and M15-NATIVE-RULE-PROVENANCE.md. No new sanitizer claim.

Northern encounter evidence (`17b317abf3`):
`substructure-northern-rat-combat-03` PASSES stationary normal Club defence from
the admission04 save in a fresh process. Three hits kill original rat038173;
subsequent swings miss harmlessly. Native save records Player killer, health65.48,
Club condition63.70 and both armor pieces. Earlier combat01/02 failed before
swinging at an unnecessary85-unit approach gate. No reach change was needed.
`substructure-northern-rat-reload-01` PASSES fresh load/F5. Its
`encounter-restart-audit.json` verifies unchanged native rat values/death/killer,
whole Player inventory/equipment/condition and health, with no residual combat.
Captures inspected; no forbidden/unreviewed errors. The rat still appears upright
after death/reload: known creature corpse-presentation gap, not a native-life claim.

`substructure-west-corners-01` FAILS after seven reached corners. Rats038172/038171
attack and push Player off waypoint8; held-forward input overruns the route.
Normal X11 key releases/F5 retain a diagnostic save at(-1737,4836,-367), health16.60,
SHA256 `1ff7945c6cb9f4b70be61a40a6ad4eee5f5d73401ce8bf843201ffbf7ec41c6e`.
`manual-input-receipt.json` records those inputs; Player later dies and the
ordinary reload prompt is captured. This is not western traversal acceptance.

`substructure-player-death-reload-05` PASSES from that diagnostic save:
normal F5 creates an owned checkpoint, original rats kill Player, controller A
confirms the death prompt, and normal Escape pauses the restored living Player.
A read-only Lua onSave receipt records14.71999359 health after the pre-save rat
hit; onLoad verifies that exact health and equipped Club/armor. Offline audit
matches the native saved health. Both death-prompt and restored-player captures
inspected; no forbidden/unreviewed errors. `death-reload-audit.json` records save
SHA256 `92706ca09f6145479f3c143ddba68ccb15efa507f9a6004fbe97b8aa4d565cf6`.
Retained failures: death-reload01/02 lacked a current character slot after an
external load into empty userdata; normal F5 fixes that fixture. Run03 reloads
but expects pre-F5 health incorrectly. Run04 restores correct health, but its
after_start wait misses the already-emitted confirmation; explicitly stopped
(exit130, stopped-run.json). Run05 uses distinct initial/reloaded markers.
No engine changes or new broad-test claim for these runtime fixtures.

Next playable input (use this healthier checkpoint, not the death probe):
`build/oblivion-compat/sewer-escape/substructure-northern-rat-reload-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `8f582ff97f7a8deffee8c1c63150fc3c73c6f8fd72b1af3806d5a27fe4ed2ae8`.
Player(-153,3412,-259), first person, level camera, health65.48.
All runtime/build processes are terminal. Next: use raw pathgrid nodes1..6,
prepared as `substructure-west-approach-prepared.json`, stop and fight the
approaching rats, then continue west. Hits began just after node7 in the failed
longer course. Original Natural Caverns exit is0381b8. Avoid the earlier shortcut
through the pillar near(-806,4498); keep the raw pathgrid corners.

Editable passing manifests under scripts/data/oblivion_compat:
`oblivion_sewer_substructure_northern_rat_combat.json`,
`oblivion_sewer_substructure_northern_rat_reload.json`,
`oblivion_sewer_player_death_reload.json`. Their input saves are the specific
continuations above. These checks do not replace a continuous full escape run.

## History — ordinary rat acquisition verified, 2026-11-04

Full escape remains incomplete: remaining solo dungeon/goblins, reunion,
Emperor/Amulet, character choices, actual MQ01 stage88 exit, outdoor fresh
reload, and Player death/reload. Current chunk repairs ordinary hostile Player
acquisition and perception, then verifies the northern rat encounter.

The original northern passage was traversed with ordinary inputs in
`substructure-north-passage-02` (previous binary; original provenance retained).
Its save is SHA256
`4ce25dc6822e2d83fd8dc301e21888702bb523f8db22009b69c2f0da3c2670fc`.
Original CGRat01 actors038171/038172/038173 have Aggression70, Confidence100
and no script. `player-acquisition-content-audit.json` records the independent
master audit. Positive native fight score plus detection now admits resident
hostile actors through the existing StartCombat/controller authority. Existing
controllers are preserved; sleeping/restrained/dead/inactive actors are excluded.
Original 524AD4/5EA187 positive-score branches reuse the reviewed546190 rule;
this is not full multi-target/friend/ally acquisition policy.

The legacy normalized detector could not perceive the nearby ordinary rat's
Player target. The existing detector now uses native awareness and Sneak noise
mastery with winning GMSTs. Native posture, skills, boots, effects and movement
feed the calculation; approximate ambient/point-light illumination and the
existing normalized saved score remain explicit limitations. See the signed
native detection section in M15-NATIVE-RULE-PROVENANCE.md. CharacterController
clears per-frame movement inputs before detection, so Movement now retains a
frame-derived moving flag. This is rebuilt on updates, not new saved state.

Validation under `build/oblivion-compat/sewer-escape/`:
- `player-acquisition-focused-04`: three admission/melee tests pass; earlier
  fixture failures and intentionally stopped build remain retained.
- `native-perception-focused-01`: failed missing GMST definition include.
  `-02`: four tests pass; `native-perception-engine-01`: all2167 pass.
- `substructure-rats-admission-01`: old detector never admits the visible rat.
  `-02`: native stationary awareness=-18 in the dark passage, no attack.
  `-03`: ordinary walking reaches51.66 units but cleared movement inputs still
  omit noise; no attack. `northern-rat-perception-failures.json` summarizes both.
- `native-perception-focused-03`: four tests pass, now including retained
  movement noise and standing versus sneaking. Runtime rebuilt.
  `native-perception-engine-02`: all2167 cases pass, exact inventory, no skips.
  Tested source fingerprint:
  `7919253b417c17ec6dbfa1994c1d134114c0a04b4fd00978d4794d4fb69baa2f`.
  No new sanitizer claim.
- `substructure-rats-admission-04`: PASSES unchanged ordinary walking course.
  Rat038173 awareness changes-18->3 with moving=true, detects Player, initiates
  combat and lands1.88-damage hits. Normal F5 saves Player health71.12, native
  target/controller and engagement. Screenshot inspected; no forbidden or
  unreviewed errors. `encounter-audit.json` verifies native save state.
  Runtime SHA256:
  `9b8ba0c0ff592a76925fb475b3cb5f6e08d6ff7b0d6c96dd4805848b28618d8a`.

Current input save:
`build/oblivion-compat/sewer-escape/substructure-rats-admission-04/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `f2d46e27ccdc464aef2c0e8e599aefa12deb59cea42b8936388a6ee3a764ace5`.
Player(-153,3412,-259), first person, level camera, Club and armor equipped.
Fresh-process `substructure-northern-rat-combat-01` and `-02` both resume
original rat attacks (the second rat joins too), but FAIL before Player swings:
the fixed85-unit approach gate is never reached. Startup-frame settling alone
did not fix this course. These are ongoing-combat reload evidence, not rat-kill
acceptance. No diagnostic continuation save is used from either failed run.
`substructure-northern-rat-combat-03` is running from the same admission save,
using stationary ordinary Club attacks to check actual weapon reach without
the unproved approach gate. Poll before launching another game.
All build helpers are terminal. The acquisition/perception implementation is
verified for this ordinary encounter; rat kills and subsequent traversal are
still open. Next: inspect the fight, verify death/reload, then continue west
toward the original Natural Caverns exit0381b8. Use the raw pathgrid route; the earlier
simplified route cut a pillar near(-806,4498). Backtrack through node121
(-776,4423) before node122(-899,4545) if reproducing that failed continuation.

Editable passing manifests: `oblivion_sewer_substructure_north_passage.json`
and `oblivion_sewer_substructure_rat_admission.json` under
scripts/data/oblivion_compat. The admission manifest expects the northern save;
it does not substitute for a continuous prison-to-exit acceptance run.

## History — zombie defeated and freshly reloaded, 2026-11-04

Full escape is still incomplete. Remaining acceptance: solo dungeon beyond the
zombie room, reunion, Emperor/Amulet, character choices, actual MQ01 stage88 exit,
outdoor fresh reload, and Player death/reload. Current chunk closes the original
rat/zombie encounter and its saved continuation. No engine changes this chunk.

`substructure-room-entry-04` PASSES from the genuine F5 bend checkpoint in
`substructure-room-entry-03`. Player reached(928,2175,-67), health73. Save audit
confirms scripted rat0387d9 killed by zombie0491fc; zombie had health10. Room
entry capture inspected. Earlier room-entry01/02 missed a narrow waypoint;
03 reached the bend but its second turn exceeded the course allowance. Those
courses remain failed. Manual F5 receipts identify the legitimate diagnostic
continuations; room-entry02's attempted F5 did not create a save.

Combat is composite evidence, not a single passing whole-fight replay:
- `substructure-zombie-01`: stationary swings missed after zombie moved away.
- `substructure-zombie-02`: advancing swings landed two hits, then overran target;
  ordinary manual F5 retained health7.892747 zombie checkpoint.
- `substructure-zombie-03`: six further hits, then zombie retreated out of reach.
  Ordinary X11 forward movement and F5 retained health1.067353 checkpoint.
- `substructure-zombie-04`: PASSES using ordinary aiming, range-checked approach
  and Club attack. Finishing hit damage1.12535; native save records zombie dead
  with Player killer. Club condition64.96, cuirass250/greaves175, health73.
  Extra scripted swings after death are harmless misses; do not count them as
  hits. Capture inspected; camera ends looking down in third person.
- `substructure-zombie-reload-01`: PASSES fresh-process load and resave. Read-only
  Lua verifies both dead actors, equipped Club/armor and Player health. Offline
  `encounter-restart-audit.json` compares both native life/killer and actor-value
  records, and whole Player inventory including equipment/condition unchanged.
  Reload capture inspected. No new broad engine/sanitizer run: existing binary
  and prior 2166-case evidence are reused with their original provenance.

Editable passing courses: `oblivion_sewer_substructure_room_entry.json`,
`oblivion_sewer_substructure_zombie_finish.json`, and
`oblivion_sewer_substructure_zombie_reload.json` in scripts/data/oblivion_compat.
Their input saves are the specific continuations above; the finish course is
not claimed to replay the whole encounter from full health. Failed course
manifests and manual-input receipts remain in the evidence directories.

Next input save:
`build/oblivion-compat/sewer-escape/substructure-zombie-reload-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `2851ea1c02bb7772e3a6acc156244d1e7ea59d30bac81aaaba36e38607874a24`.
Player(591,2087,-51), yaw2.610, pitch1.571; level the camera before traversal.
All runtime processes are terminal. Next: continue west through the zombie room,
then north toward the next rat encounters/chest. Read-only original pathgrid
route is `substructure-zombie-to-north-chest-path.json`: approximately
(414,2203)->(253,2221)->(95,2168)->(-36,2173)->(-163,2329)->(-166,2493)
->(-153,2695)->(-152,2867)->(-153,3033)->(-157,3167)->(-161,3321)
->(-168,3513)->(-206,3650)->(-149,3836)->(-95,3959).
This extracted route is planning evidence, not runtime traversal acceptance.

## History — original rat/zombie callback runs, 2026-11-04

Full original sewer escape remains the target. The remaining solo encounters,
reunion, Emperor/Amulet, choices, actual exit, outdoor reload and death/reload
acceptance are still open. Player now wears the first chest's cuirass/greaves
and has traversed the east corridor to the lower passage with health73.

`substructure-corridor-01` reached all three original pathgrid waypoints and
saved, but FAILS an original OnPackageDone callback: script0521e2 executes
SetAV Confidence50 before StartCombat CGZombie01REF. Confidence also names
DIAL02d70e. Generic name resolution supplied that form instead of the actor-value
enum, producing OBSV115. No formula/int32 conversion change was needed.

The compiler now emits the first named argument of all ten supported actor-value
get/set/mod/force aliases as a literal enum name. Other reference arguments keep
normal form resolution. Tests cover aliases/member calls, an actual colliding
record, and real OnPackageDone dispatch with native Confidence and GetAV reads
in both content layouts.

`actor-value-enum-focused-01` passes23 ObScript component and10 tutorial-action
engine tests and rebuilds the runtime. `actor-value-enum-sanitize-01` passes the
same23 compiler tests under ASan/UBSan (no leak coverage).
`actor-value-enum-engine-01` passes all2166 engine cases, exact inventory,
no failures/skips. Tested fingerprint:
`3b4a3b384a3c6be1c8faec35b0c969c04eb698d33d9a07a50121862c0f145d54`.

`substructure-corridor-02` PASSES from the untouched armored checkpoint using
ordinary controller/mouse/forward inputs and read-only waypoint observations.
It explicitly waits for original StartCombat by rat0387d9 before F5. No
forbidden/unreviewed errors. `encounter-audit.json` retains native Confidence50,
rat/zombie life states and combat engagements. Editable source:
`scripts/data/oblivion_compat/oblivion_sewer_substructure_corridor.json`.

Next input save:
`build/oblivion-compat/sewer-escape/substructure-corridor-02/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `2380c98dcdf83ce041d6c65c1d1cfff5d568c68dbd0bd2b8ed19d5d4184a9d27`.
Player approximately(1052,1479,-48), facing north, health73, Club equipped.
Next: fresh-load this ongoing encounter and continue north toward(1062,2007),
then west into the zombie/rat room. Observe the original fight and deal with its
survivors using ordinary combat. Remaining route placements and pathgrid are in
substructure-record-audit.json and substructure-north-path.json. Runtime and
build processes for this checkpoint are all terminal. Inventory portrait refresh
and earlier corpse/wall fragment presentation gaps remain adjacent open issues.


## History — native chest armor equipped, 2026-11-04

The full original prison-to-outdoors target remains open: remaining solo
encounters, reunion, Emperor/Amulet, choices, actual exit, outdoor reload and
death/reload acceptance. This chunk fixes ordinary native armor inspection/use.

`substructure-armor-01` was stopped after menu inspection and ineffective manual
mouse clicks; retain its failed course and input receipt. Controller replay
`substructure-cuirass-01` then exposed missing Morrowind iCuirassWeight during
normal Equip. Shared Armor now reads the native heavy/light flag and reuses the
existing native armor formula for item rating, avoiding iBaseArmorSkill too.
Both APIs honor explicit restore dependencies. Legacy armor keeps its old path.
The existing formula provenance is in M15-NATIVE-RULE-PROVENANCE.md's armor
sections; no new arithmetic or original-executable parity claim is introduced.
`first-chest-armor-content-audit.json` independently reads original BMDT/DATA:
cuirass0661c1 is heavy,900 hundredths/max250; greaves0661c3 heavy,550/max175.

`armor-facade-focused-01` fails to compile a test using the wrong setter name;
`-02` passes9 tutorial action tests and rebuilds openmw. The new case covers
both content layouts, native classification independent of weight, sounds,
hundredths/condition rating and alternate restore dependencies.
`armor-facade-engine-01` passes all2165 engine cases, exact inventory,
no failures/skips. Tested fingerprint:
`dc87c6da1e5933985a4fb4ad7dd7535358514b2414bcbb112bdbb928ca645b56`. No new sanitizer claim.

`substructure-cuirass-02` and fresh-load `substructure-greaves-01` PASS normal
controller Equip and F5, with inspected menus/world captures and armor-audit.json
in each run. Cuirass occupies shared mask4, greaves mask8; conflicting shirt/pants
are unequipped, Club remains equipped, health73. Displayed armor rises0->4->6.
The third-person capture shows equipped armor. The inventory portrait still
appears to retain the old outfit: adjacent preview refresh gap, not a claim of
correct portrait presentation. Editable passing manifests are
oblivion_sewer_substructure_cuirass.json and oblivion_sewer_substructure_greaves.json.

Latest save: build/oblivion-compat/sewer-escape/substructure-greaves-01/userdata/saves/Bendu_Olo/Quicksave.omwsave
SHA256 `2ab99f76a52f56898e1a16d10e744f7e3420d6fe449d0a7d9222ec920c5ad61d`.
`substructure-corridor-01` has just started from this save. It follows normal
controller/mouse/forward controls to original east-corridor waypoints near
(1062,288),(1046,822),(1052,1486), using read-only observations.
`substructure-north-path.json` retains the independently decoded original PGRD
route. Poll its launcher before starting another game; then inspect/save the
lower passage and continue toward the original zombie/rat room. Full escape
and post-armor combat remain unaccepted. All armor build helpers are terminal.


## History — healing potion consumed and reloaded, 2026-11-04

Target remains the full original prison-to-outdoors course. Later solo dungeon
encounters, reunion, Emperor/Amulet, character choices, actual exit and outdoor
reload, plus death/reload acceptance, remain open. Current chunk closes ordinary
healing-potion use at the first Imperial Substructure chest.

Actor::consume now dispatches a live Oblivion world to native potion handling.
Complete admission precedes any resource write: ordered effects must all be
instant self REHE/RESP/REFA with the original winning restoration definitions.
Scripted, timed, unsupported or altered definitions stay unconsumed. Resources
publish together through the existing native Damage channels, preserving base
and maximum modifiers and clamping restoration without reviving dead actors.
The normal container removal and itemConsumed event consume exactly one item.
This is not general magic/effect execution or timed potion ownership.

`potion-restoration-audit-01` verifies the three original MGEF definitions and
all253 ALCH/330 EFIT rows against independent raw decoding of the hash-pinned
Oblivion.esm. Editable audit: `scripts/tes4_potion_audit.py`. This is content
provenance, not an original-executable restoration-effect probe.

`potion-use-focused-01` crashes in the test's missing inventory-window callback;
its gdb trace is retained. The corrected fixture in `-02` passes8 tutorial action
cases, including consumption, clamping, unsupported-tail rejection and binary
state restoration in both content layouts. `potion-use-engine-01` then exposes
a real dispatch regression: an isolated native actor had no World object.
Actor::consume now checks getWorldOrNull before live-world dispatch and preserves
the prior rejection path. `potion-use-focused-03` passes11 cases including all
three affected native Magicka frame tests, and rebuilds the runtime. `potion-use-engine-02` passes all2164 engine cases, exact inventory, no failures/skips.
Tested source fingerprint:
`1a23c3b3218341e093498e861e2d1a89f04b0222234594fae82440f2a1448d0f`.
No new engine sanitizer claim.

Actual menu course `substructure-healing-02` PASSES: health53->73, Healing009310
consumed once, Sorcery00931a still count1. Inspected screenshot and normal F5
`healing-audit.json` retain save SHA256
`341a662d454ca2d7d3bb8599847b18f5c522d9dc3de1b69bc5dad559b298ce9d`.
Fresh-process `substructure-healed-reload-01` PASSES the same health/inventory
checks and normal F5, with inspected screenshot and `healing-reload-audit.json`.
Both have no forbidden/unreviewed errors. These runtime runs precede the
null-World guard correction; their original binary/source provenance is retained.
The corrected ordinary dispatch is covered by the focused integration tests.
Editable manifests: `oblivion_sewer_substructure_healing.json` and
`oblivion_sewer_substructure_healed_reload.json` under scripts/data/oblivion_compat.

Next input save:
`build/oblivion-compat/sewer-escape/substructure-healed-reload-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `e1ebb8a7cb03e8be36ebd41afdb780097f3d8a7eb55f9c5ca8e5c576f44efa2e`.
Next action: equip the chest's rusty iron cuirass0661c1 and greaves0661c3 through
ordinary inventory controls, then navigate north toward the original zombie/rat
encounters. `substructure-record-audit.json` retains route placements. Keep
OPENMW_OBSCRIPT_REPORT unset to avoid its per-frame serialization overhead.
All runtime courses and build helpers for this checkpoint are terminal.


## History — native potion effect data, 2026-11-04

Target: the full original prison-to-outdoors course with normal loot/equipment,
combat, reunion, Emperor/Amulet, character choices, actual exit and fresh outdoor
reload. Later solo encounters, story/reunion, exit and death/reload remain open.
This checkpoint supersedes the running-process notes in the history below.

The prior `fresh-key-return-01` completed; `return-audit.json` verifies return to
CELL022ff6 and retention of all13 wall body identities. Two fragments were
already below the level before the revisit; this does not prove all bodies
settle correctly. `substructure-rat-direct-01/combat-audit.json` verifies the
first Substructure rat038819 killed by Player with the equipped Club, whose
condition is68.74. `substructure-first-chest-01/loot-audit.json` verifies chest
049352 emptied and all8 authored item types received, including armor, bow,
arrows, axe and healing potion. Existing chest activation transfers the contents;
this is not evidence of a full container GUI. Armor has not yet been equipped.
Rat/Goblin corpse presentation gaps remain recorded alongside earlier evidence.

Next playable input save:
`build/oblivion-compat/sewer-escape/substructure-first-chest-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256 `6b0d8a60d2d448b20017f63e05afcd02ecf76c1ca9e2213b07bd32c1d3f62b2b`.
Player health53/80; both collected potions remain count1.
`substructure-healing-01` is terminal failed: ordinary inventory use does not
consume or heal. Actor::consume rejects native magic. The ALCH loader also
formerly discarded EFID/EFIT. This chunk retains ordered TES4 effects using the
existing shared decoder, separates script-effect FULL from the potion name,
and exposes effect fields in esmtool. It does not yet implement consumption.

Independent source hash is
`a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`.
Healing009310 authors REHE20, duration0, self, AV8; Sorcery00931a authors RESP50,
duration0, self, AV9. `scripts/tes4_potion_audit.py` independently decodes raw
EFIT and compares typed output. `potion-master-audit-01/verification.json` passes
all253 original ALCH records/330 effects, with no missing/extra/different entries.
This is content/loader evidence, not an original-executable magic-rule oracle.

`potion-parser-focused-01` passes7 parser cases. The subsequent
`potion-parser-integrated-01` passes the same7 normally and under ASan/UBSan
(no leak check), and links the engine/tools, but its engine selection is empty:
retain the overall failure. `potion-store-engine-01` passes the new potion
master-remapping/override/deletion case; its35-test suite fails an existing
PreparedInventoryReferences test missing WEAP class registration in isolation.
`potion-engine-full-01` passes all2163 engine cases, exact inventory, no failures/skips,
and rebuilds openmw/esmtool. Tested source fingerprint:
`92a31f89bc0a052958997b84652f99e055bce21aed8a0bbddab1ad2b275e0572`. All helpers are terminal.

Next: connect ordinary Actor::consume to native potion admission and resource
restoration, validating all effects before item removal. Preserve native actor
value/lifecycle ownership, reject unsupported effects without item loss, test
single consumption and persistence, then retry the actual menu from the chest
save. Continue armor and the original dungeon route afterward. Keep
OPENMW_OBSCRIPT_REPORT unset in new courses: it writes the entire report every
frame and caused severe slowdown. Read-only observers and normal F5 audits remain.

## Current checkpoint — equipped Club and keyed Substructure entry, 2026-11-04

Target remains full original sewer escape through outdoor reload. Loot/equipment
and the first keyed solo-dungeon transition now have runtime evidence; remaining
solo encounters, reunion, Emperor/Amulet, choices, exit and death/reload stay open.

`fresh-club-equip-03` PASSES fresh loot reload, normal inventory/D-pad-axis/Equip,
and F5. Club159829 occupies right-hand slot16 (mask65536), condition70; all other
inventory entries and equipped clothing are unchanged. `equipment-audit.json`
and inspected screenshots retain proof. Save SHA256:
`3af68000a66754a0d575e10f697563f82184c35ee8664e45b3dd379af0ad42ec`.
Run01 was intentionally stopped after selecting pants instead of Club; its
changes were discarded. Run02 fails input delivery because the virtual device
uses a D-pad axis, not BTN_DPAD_RIGHT. Both remain nonpasses. Passing source:
`oblivion_sewer_fresh_club_equip.json`. A normal sheath/draw cycle in the next
course visibly presents the Club; immediate equip initially retained fist pose.

`fresh-key-door-01` uses the actual looted IronKey159826 to activate door0248b2
and enter original CELL022ff6 (Imperial Substructure), but FAILS repeated
`live animated reference has no renderer` errors. Cell unload only removed
physics for references still owning static Object/Actor entries. The released
wall's separate bodies survived after its renderer disappeared. Scene now
removes physical owners unconditionally after retaining their snapshots.

`animated-unload-focused-01` passes13 animated-reference/loose-object tests and
rebuilds the game. `animated-unload-engine-01` passes all2,162 engine cases,
exact inventory, no failures/skips. Tested fingerprint:
`a61a484b0eb002aeea3128ab605fccea1b420aae86559112bdec914591ce3bad`. No new sanitizer claim.
`fresh-key-door-02` PASSES the previously failing real cell transition, normal
F5 and inspected destination screenshot, with no forbidden/unreviewed errors.
Its save is CELL022ff6 at(14.08,383.27,134.47), Club equipped, key retained and
collapsed-wall snapshot retained. `destination-audit.json` records save SHA256
`bd09bcdaaac5419463d2b50b66c64bef5afb1c3ded9fca7404e997b5098d903d`.
Passing source: `oblivion_sewer_fresh_key_door.json`.

Fresh escort acceptance gap closed: `fresh-rats-approach-02/process.log` records
original Glenroy032a16 OnPackageDone script032ae2 for PACK015139, followed by
CharGen30. Later saves retain stage30 and all three escorts in CELL022288;
the guards reached their authored destinations. This reuses original evidence,
not a new fabricated progression step.

`fresh-key-revisit-01` fresh-loads the destination and actually returns through
door025041 into the old prison cell without renderer errors. Normal manual F5
`old-cell-checkpoint.omwsave` and `old-cell-audit.json` retain the restored wall
state, Player(2176,2944,-859.21), SHA256
`634517507fc479012c0b3435d0866ff6cfb5718af6ba052c8bdb201be7306230`.
Whole course FAILS its second activation: blind mouse turns focused masonry,
not the exit. Preserve that input failure; old-cell restoration is composite
proof, not whole-course acceptance. `fresh-key-return-01` is RUNNING (launcher
session52553), from this genuine checkpoint, with an explicit horizontal
controller turn before aiming/activating door0248b2. Ignored source:
`fresh-key-return-prepared.json`. Poll it before starting another game.

Next: finish return to Substructure, then continue the solo dungeon with the
Club/key inventory. Known adjacent visual gaps: the authored Goblin corpse
has zero animation groups and upright pose; weapon equip from already drawn
fists required a normal sheath/draw cycle for presentation. Historical evidence
and earlier checkpoints follow below.

## Current checkpoint — key corpse loot accepted, 2026-11-04

Target remains full original sewer escape, including useful equipment, solo
combat/exploration, reunion, Emperor/Amulet, choices, actual exit and persistence.

`fresh-key-approach-01` PASSES the east passage and controller turn to the
correctly dead Goblin Shaman. Saved Player is(1966.94,3115.12,-813.42). `corpse-approach-audit.json` retains original
corpse Dead/null killer. Source: `oblivion_sewer_fresh_key_approach.json`.

`fresh-corpse-loot-01` fails to activate because the corpse is not focused.
Run02 uses normal SDL R and read-only camera-guided mouse input; its inspected
capture and activation-focus log identify Goblin15985a, but actual activation
throws `fDiseaseXferChance not found` in the shared ActionOpen disease hook.
Original TES4 content lacks that Morrowind setting. The legacy disease helper
now returns for the Oblivion profile before querying TES3 spells/settings.
This isolates legacy mechanics; it does not implement native disease rules.

`corpse-contact-red-01` reproduces the exact exception in both content layouts.
`corpse-contact-focused-01` passes all7 tutorial action cases and rebuilds the
game. `corpse-contact-engine-01` passes all2,162 engine cases, exact inventory,
no failures/skips. Tested fingerprint:
`9e8cd94d1b8d077ae1b30fcffe410032ea18054f2937d5802da9d9cc435f1065`.
No new sanitizer claim.

`fresh-corpse-loot-03` PASSES the same aimed normal activation and controller
Take All. The inspected container screenshot shows the real Goblin inventory.
Normal F5 save SHA256 `5f6f75a3018bd5fe30d0331959620dadc189271708b82e7543dd0610f1a1ab3d`
and `loot-audit.json` prove an empty corpse and Player receipt of all six item
types: IronKey159826, Club159829 at condition70, three lockpicks, Weak Potion
of Sorcery and two scrolls. No forbidden/unreviewed errors. Editable passing
source: `oblivion_sewer_fresh_corpse_loot.json`.

Next: fresh-process reload of this loot save, equip the Club through the real
inventory menu, then continue the original solo dungeon/key route. All loot
runs and engine helpers are terminal. CharGen30 fresh escort arrival remains
open; later reunion/story/choices/exterior and death/reload remain unaccepted.
Adjacent visual issue: the dead Goblin is posed upright with
`animation_groups=0` in the assembly log. Record it for later presentation/
goblin-animation work; do not conflate saved death and successful looting with
verified corpse animation. Historical route/evidence follows below.

## Current chunk — restore initial AI selection, 2026-11-04

Target remains the complete original prison-to-outdoors course, including loot,
equipment, reunion, Emperor/Amulet, character choices, exit and fresh reload.

The fresh rat replay exposed lost pending AI selection on restore. Both rats
in `fresh-rats-01/rat-approach.omwsave` had null package/script package, source
None, selection generation0 and phaseAct. Never-updated seeded actors had been
saved before selection; restore discarded their transient pending flag.
`prepareRestore` now reconstructs that flag for never-selected identities,
including those already advanced into empty Act, without restarting selected
packages. Binary-restore regression covers two content layouts, high/low
processing and initial/legacy-empty phases (eight variants).

`initial-selection-red-01` fails as expected on the original code.
`initial-selection-focused-01` retains a test timing failure: high-process
promotion first reconciles the saved abstract position. The corrected test
allows that step; `initial-selection-focused-02` passes all6 tutorial action
cases and rebuilds the game. `initial-selection-engine-01` passes all2,161
engine cases with exact inventory, no failures/skips. Tested fingerprint:
`afb2fb389075cf86eaeae63b72587b55c31ed28f13cf153266a5ae71c7b6678e`. No new sanitizer claim.

`fresh-rats-selection-01` is terminal FAILED on striking distance, with no
forbidden/unreviewed runtime errors. Both rats select original PACK07303d;
rat037e42 activates the wall, running original MQ01 stages24/28. Normal F5
`wall-trigger-checkpoint.omwsave` and `wall-trigger-audit.json` retain this
state (SHA256 `4fdd23b3e568a555c8334185791f264bd9f66ed445bde8edb873bfca212e4ace`).
Player remained at(33.43,3153.43,-831), too far from the rats for melee.

`fresh-rats-approach-01` was intentionally stopped (game peaceful, launcher130)
to repair inherited double-escaped mouse regex. Run02 reaches wall centerline
through normal W/D, receives real rat hits, and punches rat037e42 to death.
Whole run FAILS both-rats-dead: the second rat remains beyond the opening.
Normal manual W/F5 receipts, inspected `combat-position.png`, and
`combat-audit.json` retain actual progress. `combat-checkpoint.omwsave` SHA256
`7f113ec7f2d3b3b165fea568e9416815113c66e39a2f99278e88c0f0fcb360d0`
has Player(227.16,3194.16,-828.45), yaw1.893, rat037e42 Dead with Player killer,
and rat037e41 Alive at(414.89,3131.52,-810). The angled approach meets the wall
edge; next traverse the opening along its centerline, then fight the survivor.

`fresh-wall-combat-01` was intentionally stopped (peaceful/130): its heading
gate incorrectly required unit horizontal X despite Player pitch. The corrected
observer uses horizontal atan2; it also clears the prior strike release timer
when a new punch starts. `fresh-wall-combat-02` reaches the correct heading,
but the surviving rat obstructs forward passage. A recorded ordinary mouse1
hold/release lands a power punch (damage3.55); both rats are then Dead. Normal
W+Shift crosses the original opening. Inspected `wall-crossed.png` shows the
Player in the solo dungeon. F5 `wall-crossed-checkpoint.omwsave` SHA256
`61e8e5df5f28996f84d4ae7a796b53dc32c03ba42d38c555ee2a80d91259d1e8`
and `wall-crossed-audit.json` prove Player(594.19,3201.34,-860.83) beyond the
wall and both rat killers are Player. This is manual-assisted composite runtime
evidence; the whole fixed-punch/stage30 manifest is not a pass. The game was
intentionally stopped peacefully after retaining that checkpoint; launcher exit1.
Manual input and stop receipts remain in the run directory.

Next course `fresh-key-approach-01` is RUNNING (launcher session26374), from that
crossing save. It reuses the established east-lane and controller-turn route to
the Goblin key corpse, with360-second movement gates and abort-on-failure.
Ignored editable source: `fresh-key-approach-prepared.json`. Poll this process
before starting another game. Then loot the correctly dead corpse and continue
the solo dungeon. Original CharGen30 fresh escort arrival remains unaccepted
in this chain. No further engine changes or new engine test claim after
commit8b339323f2. Earlier evidence follows below.

## Current checkpoint — fresh startup repaired, 2026-11-04

Initial bound-equipment publication now waits until the Player has a cell.
After initial placement, startNewGame retries enabled, undeleted loaded actors.
Whole-world snapshot validation remains strict. The regression covers deferred
activation, retry publication and duplicate prevention in both content layouts.

`startup-bound-focused-01` passes all6 bound-item cases and rebuilds the runtime;
`startup-bound-engine-01` passes all2,160 engine cases, exact inventory, no
failures/skips. Tested source fingerprint:
`387ede33c3ddb68ecc14d9afd3f5cf304fc3b3c82b07a240697dff149044c0aa`.
No fresh sanitizer claim.

`fresh-corpse-prison-02` passes original new-game startup and normal F5 save,
with no forbidden/unreviewed errors. The inspected screenshot shows the prison.
`startup-state-audit.json` in that evidence directory verifies all four original
assassins each own one bound ability and three equipped bound items (armor,
helmet, mace). The Goblin15985a is Dead with null killer; death counts and
pending events are empty. Save SHA256: `0364f6b7acfff1f3529e53bfeeda3a06a58ee5ceedf37314fa97214a6083452a`.
Runtime SHA256: `01e79951c6a33a731221e9c68ad11719b9bae6e66af74309fc3d7434715340a2`.

Next: continue the original prison opening from this fresh save and replay the
accepted route to the key corpse. Old saves explicitly retain the incorrect
Alive state and cannot establish corrected corpse-loot acceptance. Full loot/
equip, reunion, Emperor, choices, exterior and persistence requirements remain
open. All startup verification processes are terminal.

Fresh continuation: `fresh-opening-01` was intentionally stopped because
movement preceded stage6/autosave. `fresh-opening-02` waits correctly, triggers
Valen's original taunts and reaches stage10/guard dialogue, but FAILS the
300-second wait for stage13. It has no unreviewed/forbidden errors. Normal X11
F5 checkpoints and `manual-input.json` are retained. `guard-progress-audit.json`
compares actual saved positions: Renault moves from(-459.77,395.34,179.43) to
(-39.13,152.33,26.11); Emperor and Glenroy move down the stairs too. No navigation
stall is established. The latter `guards-later.omwsave` SHA256 is
`2276b4d94e325a60841c7e0d0a6c9b10fa74204e4fc797512a636638a43a054e`; the key corpse remains Dead.

`fresh-guards-01` restores the later checkpoint, reaches original stages12/13,
then its short movement pulses fail to reach the stand-back trigger. It was
intentionally stopped; ordinary F5 `stage13-input.omwsave` is retained (SHA256
`6229278efe9d5b200b74f84e6d5e0c5f0f4086a09b28bbd04254a2e6ec5e269c`).
The original trigger097a3b/script097a3d sets14 on Player entry at stage13.

`fresh-standback-01` PASSES: ordinary W/D movement with read-only position gates
enters that trigger, disables it through its source script and reaches14;
Glenroy opens the cell and completes his route to15; Emperor approaches to16.
No forbidden/unreviewed errors. The inspected capture and `stage14-audit.json`
prove actual movement and trigger state. Final `stage16-audit.json` verifies
stage16 and the original Goblin still Dead. Quicksave SHA256:
`e66fe9c6251f1f4f669c6abb8567746c1ae07353eb40da4752d7397e5a234438`.
Editable passing segment: `oblivion_sewer_fresh_standback.json`.
`oblivion_sewer_fresh_opening.json` assembles the observed fresh opening with
these position gates and600-second waits; this full assembled revision has not
run end-to-end. Retain the failed timing courses as failures, not passes.

`fresh-greeting-01` PASSES the fresh-process original Emperor greeting with
controller responses, stage18, Renault switch04e90d activation and stage19.
Dialogue captures were inspected; its early wall capture is not traversal proof.
`stage19-audit.json` verifies the quest and still-dead key corpse. Quicksave SHA:
`0556a4c2a654b8859b67fb1beef4700df344f62d5d22ef6b4946c673ca028d0f`.

`fresh-passage-01` PASSES another fresh-process load, Baurus's original control
release and actual controller traversal through the secret passage. Its observer
reads Player.getControlSwitch, supporting both already-restored and newly
released controls without modifying them. The inspected capture shows Player
and Emperor beyond the wall. `passage-audit.json` records Player
(658.51,316.01,-128.09), stage19 and original corpse Dead; Quicksave SHA:
`7ee9c4932d0784a3c4682914de00ef2b7dae8c418bb02ada2b7f33b02fe67577`.
Both courses have no forbidden/unreviewed errors. Editable sources are
`oblivion_sewer_fresh_greeting.json` and `oblivion_sewer_fresh_passage.json`.

`fresh-descent-01` PASSES fresh-process passage reload and normal controller
movement through the stairs/lower doorway to(658.51,1443.33,-447.02). The capture
was inspected; `landing-audit.json` verifies stage19 and the still-dead key
corpse. Quicksave SHA:
`fc5e3e4751c5663519ce2bcd472cc133158ca9df56f45cef4fb456ed4b34f311`.
Editable source: `oblivion_sewer_fresh_descent.json`. No forbidden/unreviewed
errors. No new engine build was needed for these input-only replay courses.

`fresh-before-ambush-01` completes all three movement gates, stopping at
(1002.27,1664.15,-590.63), but does not reach20 before intentional stop.
Normal X11 F5 checkpoints and receipts are retained. `approach-checkpoint.omwsave`
SHA `4398ab82ad4d48130ec6497547e916db6535d96a422dfdf2d18088c77544fc18`
has stage19/corpseDead. Renault advances from(1377.90,1622.60,-446.86) to
(1091.56,1669.16,-548.62), close behind the Player; waiting in the stairs may
obstruct her. The game stopped peacefully; launcher SIGINT130 is recorded.
Do not count this as a stage20 pass.

`fresh-ambush-entry-01` is RUNNING from the later genuine escort checkpoint.
Normal controller movement reaches(53.73,2133.85,-639), health80. Renault advances
to(532.12,1663.63,-639.04); other escorts advance too. `room-entry-audit.json`
retains this evidence. Player was within47 units of Renault's destination
(44.16,2180.08), so a documented ordinary X11 W hold moved Player north to
(53.73,2260.29,-639), then released W and saved. `room-clear.omwsave` SHA:
`3bc707dfff93f1b6ecdaa170e958ed27bea999f2c438ddcaeaf580296ee37d0a`; `room-clear-audit.json` retains stage19,
health80 and corpseDead. No state injection. Observer reports actual life of
Renault/four assassins. Existing manifest: ignored `fresh-ambush-entry-prepared.json`;
manual inputs are in `fresh-ambush-entry-01/manual-input.json`.

Follow-up: Renault reached about21 units from her zero-radius marker but
circled outside the16-unit arrival check with Player80 units beyond it. A
second documented ordinary W hold moved Player north to(53.73,2435.52,-703).
Renault immediately completed her package; original stage20 SetEssential=false
and stage22 ran. `ambush-start.omwsave` SHA:
`c4d1f79b0e2c1e8335b38aecce129925fc60100035dbfe09a5f17bf78f4ca424`.
No arrival threshold or gameplay code was changed. Treat Player avoidance as
the supported explanation, not an independently measured native rule.

`fresh-ambush-entry-01` reaches23/24 and actual Renault death but the whole
runner FAILS: its inherited stage22-only forbidden23 gate rejects intended
progression. No unreviewed runtime errors. The failure and manual input receipts
remain unchanged. `fresh-ambush-death-01` corrects that course gate and PASSES a
fresh-process replay from the genuine22 checkpoint: assassin0178c4 commits its
real hit, filtered OnHit/scripted death advances24, and saved Renault is Dead.
Original key corpse remains Dead, Player health80. Quicksave SHA:
`fa114eac3a0f784add595ae0fec858a2e03b73683800be91242214a2bdf73fce`.
`death-state-audit.json` records the saved semantics. The prior entry capture
shows Renault's body and an assassin in the actual room; it is not victory
proof. Editable passing source: `oblivion_sewer_fresh_ambush_death.json`.

`fresh-victory-01` fails preflight only: unsupported keys_up action; corrected
to separate key_up events before the next run. `fresh-victory-02` is RUNNING.
Actual guard combat kills all four assassins and reaches original26/27/28/29.
`victory-checkpoint.omwsave` SHA:
`022f40a8d9d30acf5083236b4e561238212d300eca483ea9399b84db36a9007b`.
`victory-audit.json` verifies five dead actors (Renault plus assassins), guard
killer identities, cleared assassin bound owners, Player health80 and the
original key corpse still Dead. The victory capture was inspected. Normal
W+Shift then moves Player80 units; controls are released as recorded.

A documented ordinary F5 retains `departure-checkpoint.omwsave` at29 (SHA
`cb43c40b8e721202b2fcb2326a8907d4eff133cbfebe9a9becc389a67ee281e9`). `departure-audit.json` shows guards
near y3400 and Emperor near y3096, advancing toward their cell transition;
Player(48.40,2525.51,-703) is out of their route. The run still waits for30;
this is not full-course acceptance. Poll existing process before restarting.
Prepared next: ignored `fresh-rats-prepared.json`, read-only rat-distance gates
and normal movement/punches from the expected north-facing departure save.
It is unrun. Its expanded scope permits the original wall's MQ01 stage24;
the older already-collapsed-wall rat course forbade that stage.

Full fresh escort departure, wall traversal and corpse loot remain open,
followed by solo dungeon, reunion, Emperor sequence, choices, exit and persistence.

## Previous checkpoint — authored corpse initialization, 2026-11-04

Normal input reached the original Goblin Shaman/key corpse. The accepted
`solo-key-final-approach-02` save is at(1964.32,3114.64,-813.90), health49.45,
SHA256 `696f930b726bec0140a4948fc0d136fd0cb09be09082978c66a037965fa6b465`.
The inspected screenshot identifies Goblin Shaman under the crosshair.
Earlier south-lane routes hit walls; the mouse-waypoint route failed its heading
assumption. Genuine F5 room/east checkpoints and stop receipts are retained;
those courses are not passes. Controller turning completes the final approach.

`solo-corpse-loot-01` fails before activation: the old saved actor is Alive
with zero Health. Original CREA159859 has authored Health0 and reference15985a
owns Iron Key159826, Club159829, three lockpicks, two scrolls and a potion.
See [SEWER-ZERO-HEALTH-CORPSE-AUDIT.json](SEWER-ZERO-HEALTH-CORPSE-AUDIT.json).
Native base-stat initialization previously copied zero Health without adopting
initial death. It now marks fresh zero-health actors dead; native construction
adopts that state without a kill event. Explicit restored life stays authoritative,
including Alive recovery/resurrection states. Existing bad saves are not rewritten.

`authored-corpse-focused-01` passes2 focused cases, including fresh creature
construction, binary life restore, no kill credit/death event and preservation
of an explicit saved Alive state during lazy reconstruction.
`authored-corpse-engine-01` passes all2,159 engine cases, exact inventory,
no failures/skips; fingerprint
`58e0ca140633246b67c43d9dbb65532ab3996c0a2830720271ddca60f434ac48`.
No new sanitizer claim.

`fresh-corpse-prison-01` starts the original new game, skips its intro with
Escape and saves normally. The observer and saved snapshot prove the Goblin
is Dead, killer null, with no death counts or pending death events. Its genuine
prison Quicksave SHA256 is
`3c79e8c958d1ab9a43dac5f0da05c1919578ee8a733c78a3525b5b93ac026467`.
However, the whole run remains FAILED: four Assassin render admissions report
`TES4 runtime-state player has a null required FormKey` before initial Player
placement. This is a separate startup dependency, not corpse-loot acceptance.
Editable fresh-start course: `oblivion_sewer_fresh_prison_corpse.json`.

Next: fix early Assassin bound-equipment grants, then replay legitimate prison
progression with the corrected fresh corpse state. GDB `fresh-bound-grant-trace-02`
confirms `Actors::addActor -> activateOblivionActor -> initializeOblivionNonPlayerActor
-> updateOblivionBoundEquipment -> captureOblivionRuntimeState -> validate` during
`Scene::changeToInteriorCell` in `startNewGame`, before Player cell placement.
Defer grant publication until the Player/world capture is ready, preserving the
later normal equipment update and validation. Trace01 failed startup because a
copied, shutdown-written settings file contained duplicate camera sensitivity;
trace02 used pristine manifest settings. Both debugger runs and all runtime/test
processes are terminal. Keep prior wall crossing evidence; full original route,
loot/equip, reunion, Emperor, choices, exterior exit and persistence remain open.

## Previous checkpoint — wall crossed, 2026-11-04

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
