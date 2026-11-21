# Kvatch to Weynon Priory — current handoff

**The player found Martin and reunited him with Jauffre in Weynon Priory Chapel.**
Find the Heir (MQ03) is complete at stage 100. Martin recruited through reply 01e8d7. At the reunion, Jauffre's
greeting 01ee25 and reply 01ee2b advanced Weynon Priory (MQ04) to stage 20.
The next journal objective is to check the Amulet of Kings in Weynon House.

## Resume the playthrough

Run `build-windows/Play-Weynon-Reunion.cmd`. The launcher was cold-tested with
`build-windows/sigil-weynon-player/user/saves/Bendu_Olo/Martin-at-Weynon.omwsave`.
The named checkpoint is separate from the ordinary quicksave. Its local profile
loads Oblivion.esm and the input harness's original script manifest. The player
script now just returns an empty table and leaves the controls to the player.
Keep the manifest with this save. Native save admission requires its identity.

## Course evidence

All case directories below are under `build-windows`. Each save is under
`user/saves/Bendu_Olo`. The route from the inherited stage-65 plaza save used
player movement, attacks, spells, wait, doors, conversations and world-map
travel. The input harness steered those actions. Earlier gate checkpoints
below describe their console positioning.

| Milestone | Saved evidence |
| --- | --- |
| Six original plaza attackers spawned, fought and defeated | `sigil-martin-wave02`, `sigil-martin-wave-cold02`, `sigil-martin-fight02` (MS48 80) |
| Savlian authorized chapel entry and the player entered through door 07214e | `sigil-martin-savlian01` (MS48 90), `sigil-martin-chapel03` |
| Martin accepted the invitation. Both secret doors received their open defaults and all five priory attackers were enabled | `sigil-weynon01-recruit` (MQ03 60, MQ04 5) |
| Martin followed through the chapel and city doors. The player recovered by waiting | `sigil-weynon01-plaza`, `sigil-weynon01-outside` |
| World-map confirmation brought both characters to Weynon and the exterior attackers engaged | `sigil-weynon01-arrival` (MQ04 7). Repeat `sigil-weynon-follow02` verifies an immediate save of both positions and the follow package |
| The player went straight to the priory chapel. The two Mythic Dawn agents died there and both companions survived | `sigil-weynon01-chapel`, `sigil-weynon01-defense` (MQ03 100, MQ04 10) |
| The player spoke to Jauffre. Martin and Jauffre remained alive beside the player after a cold restart and resave | `sigil-weynon01-jauffre`, `sigil-weynon-player` (MQ04 20) |

`scripts/verify_kvatch_plaza_wave.py` checks the wave and script owners.
`scripts/verify_weynon_map_travel.py` checks departure and immediate arrival.
`scripts/verify_weynon_reunion.py` checks both journal stages, the living party
in the chapel and both defeated attackers. Final full inventories passed
2,338 engine and 3,318 component tests, with zero skipped
(`weynon-final-full.log`). Local commits are on `kvatch-sigil-course` in
`build-windows/kvatch-sigil-checkpoints`. The incremental bundle is
`build-windows/kvatch-course-current.bundle`, based on c35ede26bf.

The world map now reads Cyrodiil's artwork and bounds from the original WRLD.
Choose **Find a place** to locate a named map marker. **Province overview**
fits the map in the maximized window. Drag to pan, scroll to zoom, or use
Page Up / Page Down to browse places. **Travel (T)** asks for confirmation.
With no place selected, it still offers the eligible marker near the active objective.

The rendered Windows course travelled from the Kvatch gate to Anvil Main Gate
and, after reloading the Anvil quicksave, to Bruma East Gate. Screenshots and
ordinary saves are in `sigil-map-anvil2` and `sigil-map-bruma3`. Map selection
was independent of the active quest. Travel advanced the clock by 5.07 and
29.69 game hours respectively. `sigil-map-pointer4` reopened the Bruma save
and exercised province overview, mouse-wheel zoom and drag. In `sigil-map-focus5`
the player picked Anvil Dock Gate with the mouse and pressed T. Its confirmation
matched the one opened by clicking Travel in `sigil-map-pointer4`. The input harness
opened the map through the UI and sent controls to the game window.
`map-nav-final-test.log` records all 2,339 engine and 3,318 component tests passing.

Earlier diagnostic saves from before
the encounter fix are documented at commit 3344ee2e60. Later quest stages
and an uninterrupted playthrough from character creation remain unverified.

## Earlier gate and plaza evidence

Savlian's original package 17c462 took him through the city door. Its
OnPackageDone advanced MS48 to 65. The pre-entry quicksave is under
`sigil-march-save01/user` in the build directory. Position/angle commands
staged the player at door 0147db, then Space entered the plaza. Before the
fix the saved player overlapped three guards exactly and the camera was
inside Merandil. After the fix the player settled 135 units from all three,
facing the chapel. A cold reload and second save reproduced the same separation
(`sigil-plaza-cold01`). Compare `sigil-plaza01` and `sigil-plaza05` screenshots
and saves. The reference stage-60 save is under `sigil-counterattack01/user`.
No quest, health, enemy or AI state edits were used.

From the stage-50 save below, MoveTo and SetPos/SetAngle staged a conversation
with Savlian. Space opened greeting 01e8a7, keyboard 1 selected the ready
reply 01e8ac, both authored stage-60 result entries dispatched, then Escape
and F5 saved. Focused dialogue/topic suite passed 56/56. A cold reload from the stage-60
save showed Savlian moving from his barricade toward the city (march01).
At 960x540 the HUD now displays the complete quest and destination together
with the bearing and 1 m distance. Compare the F12 screenshots in
`sigil-counterattack01` and `sigil-counterattack-hud01`: the rendered caption
grew from 269 to 346 pixels. Both quest bearing/distance checks passed.
Checkpoints are on `kvatch-sigil-course` in
`build-windows/kvatch-sigil-checkpoints` with incremental bundle
`build-windows/kvatch-course-current.bundle` (base c35ede26bf).

The preceding stage-50 save is in
`build-windows/sigil-course-cold01/user/saves/Bendu_Olo/Quicksave.omwsave`.
The player had 6.71 health and one Sigil Stone (041ff9).

From a copy of the earned stage-40 Blood Feast save, MoveTo, SetPos and
SetAngle staged the player within reach of SigilStoneMS48. Space started its
removal animation. The original script awarded a concrete leveled stone after
about 3.9 seconds. The ring timer returned the player through the unique
reciprocal gate pair (inside 01e8a5, outside 01e8a3). The gate script saw
GetDestroyed and advanced MS48 to 50. The exterior gate played the original
Unequip sequence, which contains the destruction sound key. Sound was disabled
in this run. After F5 and a cold restart, live queries confirmed GetDestroyed=1,
GetItemCount=1 and IsAnimPlaying unequip=0. Both saves decode to Tamriel cell
00db46 with the gate at its end pose. No quest-stage, inventory or health edits.

**Reusable evidence** (paths relative to `build-windows`):

- Final Space-to-F5 run and cold continuation: `sigil-course01/openmw.log`,
  `sigil-course-cold01/openmw.log` and the corresponding quicksaves. Both
  exited normally. `sigil-input04` / `sigil-close01` additionally exercised
  a cold restart after reward delivery but before gate closure.
- Empty leveled reward reproduction: `sigil-base2`; isolated reward fix:
  `sigil-reward`; named and ungrouped animation polling: `sigil-animation`.
- Current Windows integration: all 2,336 engine tests and all 3,317 component
  tests passed with matching inventories (`plaza-particle-full.log`,
  `plaza-final-components.log`). The previously failing effect-shader tests
  now decode a bundled DDS and exercise the real emitter. In a separate
  diagnostic from the plaza save, PMS applied the original effectFireDamage
  to the city gate. The renderer reported one emitter, 182 live particles
  and 3,462 surface triangles (`sigil-plaza-fire01/shader-report.json` and
  two F12 screenshots). No sanitizer run at this checkpoint.
- Final executable repeated Space entry at the occupied door and the stage-65
  save separation check (`sigil-plaza-final01`).

The post-stone section has normal-input and restart evidence. An uninterrupted
tower ascent, the earlier stage-30 keeper loot course, the stage-25 Ilend
follower branch, random gates without a unique authored pair and continued
Kvatch combat remain open. Stage 50's SetQuestObject on the old key is deferred.

The Runed Portal in The Blood Feast now completes its activation when the
authored membrane effect is unsupported. Same-cell Oblivion player teleports
use the authored landing height. A Windows runtime probe staged a copy of an
earned stage 40 save at the lower portal and exercised both original portals.
The player moved from Z 3007.27 to 4297.67, settled at 4290.30, then returned to
3006.12. Evidence is in `build-windows/runed-portal-probe/openmw.log` and
`build-windows/checks-e6fc16e7ae884fd79d43833315fc227d`. The visual effect and
the continued route to the Sigil Stone remain open.

**Accepted implementation: waiting followers yield to their followed Player.**
Commitc54a184bb9 fixes the collision coordinator's own-target exemption for idle
yielding. Same original bridge save passes ordinary500-unit passage and another500
after own-save restart, normal/SAN; both lives/health, native Follow1778d1 and
earned state persist. Full engine2,324 in each build and Python623 pass.
[Verification](KVATCH-BRIDGE-FOLLOWER-YIELD-VERIFICATION.json).

Windows branch freshly fetched after that chunk: remote remainsc8fdfc22a8,
already merged; GitHub remote heads independently listed, no source/build delta.
`build/oblivion-compat/kvatch-bridge-yield/windows-refresh-after-chunk-01.json`.

**Earlier stage-30 course: native inventory OnAdd after container-to-Player transfer.**
Normal original bridge remainder/door, keeper forced greeting/combat and Menien
Activate pass. Ilend earns keeper kill; Player90.0578, Ilend5.8551 remain alive.
Menien INFO089938 earns MS48stage30. [Preparation](KVATCH-REAPERS-SPRAWL-PREPARATION.json).
Real key loot10 succeeds structurally (Player key1, corpse empty) but keepsstage30
and has no inventory script owner. Original KEY093ef5/script1896b8 requires40.
[Current contract and implementation plan](KVATCH-SIGIL-KEY-ONADD-CONTRACT.json).
No new implementation yet. Next: native publication/identity and one OnAdd Player
notification after source debit/destination insertion; normal/SAN and own-save
cold, affected/full engine gates. Reuse existing singleton OnAdd identity evidence.

Current audited pre-loot source (use this, not the failed-event result):
`build/oblivion-compat/kvatch-reapers-sprawl/normal-menien-approach-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`,
SHA256 `1d43364570acdd339100169b51afb5b1ef6b99ae5042c36357b6f61b6a37517b`.
Player[-3.08,109.39,2001.01]; Ilend[79.42,232.15,2014.19], Follow1778d1.
Ordinary loot manifest uses actual rendered corpse rays and reachable navmesh
floor, fresh in-range central hit(<145; profile activation150), Activate/UI X.
Attempts01â€“09 retain camera/range/replay-protocol failures;10 reproduces the real
event gap with exact saved/live/codec/binary/process provenance. Evidence:
`build/oblivion-compat/kvatch-reapers-sprawl`. No rendered game currently running.

Remaining: key event/40, keyed level100 hall door085f69, tower ascent/Sigil Stone,
gate closure/full quest acceptance. Preserve low-health Ilend; expand recovery
only if needed for the next deliverable. No SAN encounter/key acceptance claimed.

Prior source checkpoints and acceptance:

- [Persistent original corpse flames: normal/SAN/cold, rigid particle-only scope](KVATCH-FLAMING-CORPSE-PLAYBACK-VERIFICATION.json).
- [Original flame expectations](KVATCH-FLAMING-CORPSE-ORACLE.json) and [typed EFSH dependency](KVATCH-EFFECT-SHADER-DATA-VERIFICATION.json).
- Normal bridge departure save/audit and full remaining bridge/door arrival audit:
  `build/oblivion-compat/kvatch-dark-salvation/normal-route-survey-05/bridge-audit.json`,
  `build/oblivion-compat/kvatch-dark-salvation/normal-bridge-crossing-02/reapers-arrival-audit.json`.

Course progression and earlier accepted fixes:

- [Rending Halls defense and upper exit preparation](KVATCH-RENDING-HALLS-UPPER-EXIT-PREPARATION.json).
- [Physical Hall door opening and saved animation continuation](KVATCH-RENDING-HALLS-DOOR-PREPARATION.json).
- [Unlinked Hall path-point commands](KVATCH-UNLINKED-DOOR-PATHPOINTS-VERIFICATION.json).
- [Animated Sigil base collision offset](KVATCH-SIGIL-BASE-COLLISION-VERIFICATION.json).
- [Occupied follower door arrival](KVATCH-FOLLOWER-DOOR-ARRIVAL-VERIFICATION.json).
- [Harrada phantom contacts](KVATCH-HARRADA-CONTACT-VERIFICATION.json) and [morph playback](KVATCH-HARRADA-ANIMATION-VERIFICATION.json).
- [Native TrapUpdate and real gate entry](KVATCH-TRAP-UPDATE-VERIFICATION.json), [weather](KVATCH-WEATHER-VERIFICATION.json), [gate births](KVATCH-GATE-SPAWN-VERIFICATION.json).
- [Savlian topics](KVATCH-TOPIC-LEARNING-VERIFICATION.json), [MS48 faction delta](KVATCH-FACTION-REACTION-VERIFICATION.json), [guard defense](KVATCH-ALLY-DEFENSE-VERIFICATION.json).
- [Earlier healing/route source](KVATCH-INTERIOR-ROUTE-PREPARATION.json).

The course inherits an earned-Amulet source: MQ01stage100, MQ02stage20; MQ03 was
not artificially advanced. One declared Player MoveTo to original marker00cef7
staged the requested Kvatch location. Cross-country travel and full prerequisites
are not claimed. Ilend UI uses controller menus/virtual gamepad; preserve the
accepted UI recipe and avoid saving during a forced greeting.

**Windows performance integration is complete.**
Upstream `c8fdfc22a8` merged as `d4a1665868`, verification `9886ce4475`.
[Merge receipt](WINDOWS-PERFORMANCE-MERGE-VERIFICATION.json),
[Windows timing provenance](KVATCH-PERFORMANCE-HANDOFF.md).
Keep all work local; never push without request. Preserve prior producer archives.

Run one rendered game at a time; serialize heavy SAN rendering and compilation.
Use actual actor bounds and physical checkpoints when following paths; keep the
final path point active until the checkpoint. Navmesh endpoint tolerance and
record-root distance can differ. The old foreground wrapper interruption is
retained; detached controllers completed all current checks. Poll actual process
and exit evidence before restarting work. [Chorrol throne](CHORROL-THRONE-SEAT-HANDOFF.md)
is complete; Countess work remains deferred.
