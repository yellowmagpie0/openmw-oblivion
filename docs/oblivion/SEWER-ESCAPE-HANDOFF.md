# Full sewer escape — current handoff

## Target and remaining acceptance

Play the original opening continuously from prison through the physical sewer
exit and fresh-process outdoor save/load. Include ordinary loot/equipment,
combat/traversal and shaman victory, escort reunion, Emperor death/Amulet,
birthsign/class choices, Baurus CharacterGen88 and MQ01 exit88. No injected
health, quest, position or completion, skipped required encounters, or spliced
save state qualifies. **Full escape remains incomplete.**

Remaining: join the original gameplay continuously through shaman victory and
the physical exterior, then fresh outdoor save/load. All2202 normal engine
cases and all2202 ASan/UBSan engine cases pass at their recorded fingerprints.
The accumulated sanitizer gate enables leak checking and builds the instrumented
runtime; fresh rendered sanitizer gameplay remains separate. Component3262 and
Python560 coverage retain their original provenance, with no fresh pass inferred.

## Current chunk — continuous original opening and accumulated sanitizer accepted

`continuous-opening-01` passes a genuine new-game process from prison startup
through CharacterGen19. It completes original stages5,6,9,10,12–19, ordinary
stand-back movement, all Emperor greeting choices and one original Renault
switch activation. Player health80, the authored zero-health goblin corpse stays
Dead with null killer, and pending deaths are empty. No save reload or stage,
position, health or completion injection. Two documented diagnostic X11 F5 saves
retain guard progress; all other inputs are in the editable recipe. Captures
inspected; the early wall capture alone does not prove passage traversal.
[SEWER-CONTINUOUS-OPENING-VERIFICATION.json](SEWER-CONTINUOUS-OPENING-VERIFICATION.json).

Original19 checkpoint:
`build/oblivion-compat/sewer-escape/continuous-opening-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
SHA256 `bb5f3e8a803410317faaafbb8f91e12b083da819cb80d78dec5a243ce17c41c6`.
`oblivion_sewer_continuous_opening.json` starts a new game without an input save.

`native-topics-sanitizer-integration-01` passes all2202 engine cases, exact
inventory, no failures/skips, ASan leak checking and UBSan halt enabled at
commit a057b0af86. The runtime also builds. The previous cancelled gate ran no
tests; its objects were reused, not its result.
[SEWER-ACCUMULATED-SANITIZER-VERIFICATION.json](SEWER-ACCUMULATED-SANITIZER-VERIFICATION.json).

The preceding [native topics chunk](SEWER-NATIVE-TOPICS-VERIFICATION.json)
proves all five original CharacterGen88 AddTopic effects, native DIAS knowledge,
conditional sidebar, content-index remapping and actual Jauffre INFO01e910 after
fresh load/resave. Runtime provenance now distinguishes pre-UI-repair courses
whose binary hashes were not captured from the identified post-fix response run.
The [autosave chunk](SEWER-SCRIPT-AUTOSAVE-VERIFICATION.json) proves saveable-UI
deferral, game-owned retention/rollback, coalescing and exact fresh load. General
dialogue/services and original autosave queue timing remain unclaimed.

## Next action — continuation is running; poll before another UI course

`continuous-escort-continuation-01` is running from the genuine19 checkpoint,
with exact whole-runtime entry restoration (zero differences). It has passed
physical passage traversal, lower stairs and all three roomC waypoints, and
moved north beyond2435 to clear Renault's marker. Original ambush/departure30
and the whole course remain pending. Poll launcher session51391 (game PID67281)
and inspect its actual state before taking further action. Run UI courses singly.

Editable recipes under `scripts/data/oblivion_compat/`:
`oblivion_sewer_continuous_escort_continuation.json` loads the genuine19 save;
`oblivion_sewer_continuous_escort.json` prepares the full new-game prefix through30
but that assembled version is unrun. Unbound F8 advances only local read-only
observer phases. Observer wait/aim patterns are phase-scoped, preventing repeated
waypoint names from matching old observations. Original scripts own progression.

Continue by the next observed gameplay blocker. Then extend the joined course
through solo dungeon/shaman, reunion, Emperor, choices and physical exit, and
verify fresh outdoor save/load plus focused rendered sanitizer gameplay. Preserve
failed attempts and provenance. Do not splice separate shaman/late saves or
broaden into unrelated services or exact combat arithmetic.

## Reusable gameplay evidence

- [Shaman victory/reload](SEWER-SHAMAN-VICTORY-VERIFICATION.json):18 normal axe
  contacts,11 actual shock releases, charge150→7, unarmed retaliation, Player
  killer and death count1. Player3.780029 HP; two fresh-reload healing casts reach
  17.860031. Legacy87→89 defaults and13 independently audited ONAM poses are
  explicitly reviewed; only tiny animated-body velocity projection differences
  remain. Native death below1 HP explains remaining0.840938 on the dead shaman.
  Post-victory input: `build/oblivion-compat/sewer-escape/shaman-victory-reload-heal-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
  CharacterGen30, Natural Caverns; no active engagements/actions/projectiles or
  pending death events. This victory is separate from the later outdoor chain.
- [Exterior exit/reload](SEWER-EXTERIOR-EXIT-VERIFICATION.json): original review
  Finished, MQ01 exit88, cleared chargen history, weather/crime reset, native
  regenerating-rat resurrection, physical Tamriel entry and exact fresh reload.
  Native reset preserves position/death history; Player, attached preserve-state
  and owned-paralysis resets remain guarded. General leveled inventory and broad
  active-actor visual resurrection are unverified. Outdoor input:
  `build/oblivion-compat/sewer-escape/sewers03-outdoor-reload-02/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
  Health85.971672, Warrior class/birthsign, one Amulet, no key. Three failed
  encounter probes contributed genuine pre-failure F5 prefixes only; all failed
  statuses and eight exact composite boundaries remain explicit in the artifact.
- [Exit review ownership](SEWER-EXIT-MESSAGEBOX-VERIFICATION.json),
  [weekday/PC achievement](SEWER-EXIT-CALENDAR-VERIFICATION.json),
  [class/Baurus88](SEWER-CLASS-CHOICE-VERIFICATION.json),
  [key door/final sewer entry](SEWER-FINAL-SEWERS-ENTRY-VERIFICATION.json),
  [western rat](SEWER-FINAL-WESTERN-RAT-VERIFICATION.json).
- [Emperor/Amulet/final assassin](SEWER-EMPEROR-DEATH-VERIFICATION.json),
  [friendly greeting](SEWER-FRIENDLY-GREETING-VERIFICATION.json),
  [default-open Sanctum gate](SEWER-SANCTUM-GATE-VERIFICATION.json),
  [Sanctum/bound dagger](SEWER-SANCTUM-ENTRY-VERIFICATION.json),
  [daily packages](SEWER-DAILY-PACKAGE-VERIFICATION.json),
  [reunion](SEWER-REUNION-SEX-CONDITION-VERIFICATION.json),
  [birthsign](SEWER-BIRTHSIGN-VERIFICATION.json).
- [Opening melee slice](TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md),
  [traps](SEWER-TRAP-HAZARD-VERIFICATION.json),
  [logs](SEWER-ROLLING-LOG-VERIFICATION.json),
  [loot/healing](SEWER-CAVERN-HEALING-VERIFICATION.json),
  [eastern goblin](SEWER-EASTERN-CAVERN-VERIFICATION.json),
  [healing casts](SEWER-HEAL-CAST-VERIFICATION.json),
  [staff projectiles](SEWER-STAFF-PROJECTILE-VERIFICATION.json),
  [Subterrane entry](SEWER-SUBTERRANE-ENTRY-VERIFICATION.json),
  [second ambush](SEWER-SECOND-AMBUSH-VERIFICATION.json).

## Known limits and operation

General NPC spell fallback and Player Flare remain unsupported; the shaman proof
covers staff/unarmed combat. Upright goblin corpses, blank birthsign descriptions
and take-all-only loot remain gaps. Other birthsign choices lack this runtime
coverage. No full M15/S7/E1 or general magic/dialogue acceptance is implied.

Run UI courses singly. Use dpad_y hat axes: virtual-pad EV_KEY D-pad buttons do
not reach SDL here. Linked choices begin unselected; Down then A chooses first.
F5 needs a closed conversation. Keep failed/death attempts and original evidence
provenance. Historical detail: [SEWER-ESCAPE-HISTORY.md](SEWER-ESCAPE-HISTORY.md)
and the linked verification artifacts.
