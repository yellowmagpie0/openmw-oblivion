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

## Current chunk — joined Substructure combat, loot and equipment accepted

Latest genuine course save:
`build/oblivion-compat/sewer-escape/joined-substructure-northern-rat-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Player(-162.212,3316.627,-258.602), health70.6. The original north passage traverses
physically with health80 unchanged; its actual F5 contains rat038173→Player combat.
Exact fresh restoration then real axe contacts kill that rat with Player ownership.
All combat, physical actions and pending deaths clear. Both entry comparisons exact;
captures inspected. [North traversal/combat proof](SEWER-JOINED-NORTH-VERIFICATION.json).

Previous genuine80-health recovery save remains in `joined-post-zombie-healing-01`.
Six actual C releases of saved Heal Minor Wounds restore38.4→80 health with normal
magicka regeneration, unchanged inventory/death history and no pending actions.
[Recovery proof](SEWER-JOINED-ZOMBIE-HEALING-VERIFICATION.json).

Pre-recovery Zombie victory save remains in `joined-substructure-runners-zombie-02`:
Player(727.351,2191.294,-66.602), health38.4, original chest armor and war axe owned
and equipped. All pending deaths/actions/combat clear. Exact native entry from
gear save. [Joined zombie proof](SEWER-JOINED-ZOMBIE-VERIFICATION.json).

One process physically follows the corridor, waits at a safe waypoint for original
DORatRunner01/02 (0387da/db) to follow PACK0387de back through the previous door,
then hands control to combat at the room mouth. Zombie0491fc kills tutorial
rat0387d9 naturally; five real Player axe contacts kill the zombie. Both runners
remain alive in01fbb9, position(2176,2944,-857.362). Capture inspected: zombie
corpse remains upright; logical death is proven, collapsed poses are not accepted.

Failed corridor01 continues walking into combat and Player dies; no checkpoint
accepted. Failed corridor-fights01 chases an authored runner after it leaves the
cell. Failed runners-zombie01 hands over too deep inside combat and Player dies;
only its dead-Player runtime was terminated with SIGTERM, raw scenario failed.
Corrected02 hands over at the corridor mouth and passes. No implementation change.
Raw-master routing audit stays ignored with master hash and original record names.

Current UI course: `joined-substructure-west-approach-01` (launcher26106), ignored
editable `joined-substructure-west-approach.json`, starts at the rat-victory save
and follows original route corners1–5 toward(-619,4200,-230). Inspect its actual
process/scenario before any new UI course. Next take actual intervening encounters
(38171/38172 if they engage) and the western/lower route to original Caverns door.
The40 original route corners and old successful western/lower courses remain
reusable; do not assume that alive NPCs permit walking through them during combat.

Earlier accepted composite joins remain linked:
[key-corpse loot/Club/key door](SEWER-JOINED-KEY-LOOT-VERIFICATION.json),
[first Substructure rat/chest/armor/axe/potion](SEWER-JOINED-SUBSTRUCTURE-START-VERIFICATION.json).
Gear input correction accounts for inventory reorder; raw failed01–05 remain failed.
The northern-rat victory save is the main checkpoint until west traversal passes.

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
