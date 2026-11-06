# Full sewer escape — current handoff

## Target and remaining acceptance

Play the original opening continuously from prison through the physical sewer
exit and fresh-process outdoor save/load. Include ordinary loot/equipment,
combat/traversal and shaman victory, escort reunion, Emperor death/Amulet,
birthsign/class choices, Baurus CharacterGen88 and MQ01 exit88. No injected
health, quest, position or completion, skipped required encounters, or spliced
save state qualifies. **Full escape remains incomplete.**

Remaining: join the genuine course through the solo dungeon/shaman, reunion,
Emperor/Amulet, choices and physical exterior, then prove a single-process new-game
escape and fresh outdoor reload. Separate shaman/late saves do not prove this join.

## Current chunk — western encounters and actual root-area attacker accepted

Latest semantically verified course save:
`build/oblivion-compat/sewer-escape/joined-root-descent-course-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Player(-5034.059,2936.020,-378.381), health80. Five real healing casts and all17
remaining original root-detour/descent waypoints pass in one process. Inventory
and death history unchanged; no combat, physical actions or pending deaths.
Native entry exact; capture inspected.
[Root descent proof](SEWER-JOINED-ROOT-DESCENT-VERIFICATION.json).

Preceding [root entrance/combat proof](SEWER-JOINED-ROOT-ENTRANCE-VERIFICATION.json)
passes normal healing, physical return to known corridor/root entrance, then exact
fresh continuation killing actually engaged38818. Final47.7HP save clears combat;
base02509b count increments once to8. Earlier actual attacker159838 is Player-killed
with exact fresh entry, no pending state and46.04HP output.
[Upper stairs/actual attacker proof](SEWER-JOINED-ROOT-ATTACKER-VERIFICATION.json).

The [western proof](SEWER-JOINED-WEST-VERIFICATION.json) accepts a real six-phase
F5 prefix: Player kills38171/38172, normal six-cast healing reaches80, physical
pillar passage and combat descent reach lower room, and38176 dies. Raw seven-phase
run remains failed: later backtracking lets38175 kill Player. Its actual preceding
68.72HP F5 restores exactly; clean fresh continuation kills38175 and ends59.32HP.
Base02509b death count changes only2→5→6. No health/quest/position write or save
splicing. Corrected editable six-phase recipe matches all442 successful prefix
actions; it has not been rerun independently as a whole.

Upper-stair raw case likewise passes its141-action physical/F5 prefix, then fails
while chasing38818 beyond a wall while actual159838 attacks. The saved59.32HP
stair prefix restores exactly; short normal movement segments and fresh camera
feedback defeat159838. The unrelated rat38818 remains alive at that boundary.
Neither raw failure is relabeled a whole-course pass.

Current UI course: `joined-lower-east-rat-course-01` (launcher49163), from actual
`joined-lower-corridor-approach-01` save. The latter raw five-waypoint course PASSES
at80HP, but semantic save comparison remains pending. The current course follows
three more original lower-corridor points and fights nearby159832 using short
movement/fresh camera feedback. Inspect its actual process/scenario before a new
UI course. Then handle actual38174 if engaged, follow physical lower corridor to
original Caverns door0381b8, and continue shaman/late escape. No build helper live.

Earlier accepted joins: [zombie](SEWER-JOINED-ZOMBIE-VERIFICATION.json),
[recovery](SEWER-JOINED-ZOMBIE-HEALING-VERIFICATION.json),
[north passage/combat](SEWER-JOINED-NORTH-VERIFICATION.json),
[key loot/equipment/door](SEWER-JOINED-KEY-LOOT-VERIFICATION.json),
[first Substructure chest/gear](SEWER-JOINED-SUBSTRUCTURE-START-VERIFICATION.json).
All remain explicit genuine-save composites; full uninterrupted escape is unproven.

## Verification and reusable opening evidence

Camera preference save/load is fixed and accepted: temporary Vanity/Preview retain
the selected view; old explicit saves remain authoritative. Red regression,
all4 camera/recovery cases and2203 normal engine cases pass. Real idle FP→Vanity
save writesFIRS1 and fresh-loads FirstPerson with exact native state at both
boundaries. [Camera proof](SEWER-IDLE-VIEW-SAVE-VERIFICATION.json).

Current camera-fix ASan/UBSan engine integration PASSES all2203 cases, exact
inventory, zero failures/skips, leak checking enabled and runtime built:
`idle-view-sanitizer-integration-02/verification.json`, source d697f176f8 clean.
First01 also has independently verified2203 passing XML cases, but its supervisor
lost its session before final report; it is not the accepted integration gate.
02 repeats only to resolve that missing terminal evidence. Current rendered
sanitizer fresh reload/save also PASSES: FP observed, exact native entry and actual
FIRS1 save, clean exit0, capture inspected. The source idle FP→Vanity F5 prefix
really writesFIRS1 but its raw scenario fails a10-second SDL receipt timeout
because the wrong key left the default active. Raw failure retained; independent
prefix observation and clean fresh continuation are explicit composite evidence.
Correct key `sdl_input_timeout_seconds=90` is verified in fresh receipt provenance.
Rendered leak checking is disabled; engine leak coverage stays separate.
[Current sanitizer proof](SEWER-IDLE-VIEW-SANITIZER-VERIFICATION.json).
Component3262/Python560 retain their original provenance; no fresh pass inferred.

The genuine opening chain remains linked without state splicing:
[new game→19](SEWER-CONTINUOUS-OPENING-VERIFICATION.json),
[original19→30 escort](SEWER-CONTINUOUS-ESCORT-VERIFICATION.json),
[wall-rat victories/passage](SEWER-JOINED-WALL-RATS-VERIFICATION.json),
[normal healing38→62](SEWER-JOINED-HEALING-VERIFICATION.json),
[first-person room rat](SEWER-JOINED-ROOM-RAT-VERIFICATION.json).
Escort raw failure is solely the inherited MQ01wall24 exclusion: independent
original-record/trace audit accepts the authored wall activation separately.
Wall proof accepts genuine failed03 first-kill F5 followed by exact06 continuation;
all raw failures remain failed. No full single-process acceptance is inferred.

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
covers staff/unarmed combat. Upright creature corpses, blank birthsign descriptions
and take-all-only loot remain gaps. Other birthsign choices lack this runtime
coverage. No full M15/S7/E1 or general magic/dialogue acceptance is implied.

Run UI courses singly. Use dpad_y hat axes: virtual-pad EV_KEY D-pad buttons do
not reach SDL here. Linked choices begin unselected; Down then A chooses first.
F5 needs a closed conversation. Keep failed/death attempts and original evidence
provenance. Historical detail: [SEWER-ESCAPE-HISTORY.md](SEWER-ESCAPE-HISTORY.md)
and the linked verification artifacts.
