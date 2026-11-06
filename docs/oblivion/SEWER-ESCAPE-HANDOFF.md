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

## Current chunk — original19→30 gameplay accepted after assertion audit

`continuous-escort-continuation-01` completes passage/lower stairs, original
ambush and Renault death, all four assassin deaths, original conversations26–29,
all three escort cell transitions and Glenroy's original callback30 in one fresh
continuation. Its genuine19 input restores the entire runtime with zero
differences. Player health80, all escorts alive in Imperial Subterrane022288,
five recorded deaths, no assassin bound equipment, no pending deaths or physical
action owners. Only the two newly authored wall-rat→Player engagements remain.
The Blades fight this ambush; Player combat follows in the rat course.

The raw scenario is **FAILED**, solely because its inherited forbidden MQ01
stage24 assertion was incorrect for this expanded course. All input gates pass,
engine exits0, no unreviewed errors. Independent master audit confirms original
rat037e42/PACK07303d activates wall03e31c/SCPT03faf5, whose non-Player branch sets
MQ01 stage24. Trace proves exactly one original activation/result; source events
are read-only. The raw failure remains untouched. Separate reviewed acceptance
passes, and corrected escort recipes still forbid premature MQ01 exit88.
[SEWER-CONTINUOUS-ESCORT-VERIFICATION.json](SEWER-CONTINUOUS-ESCORT-VERIFICATION.json).

Genuine30 checkpoint:
`build/oblivion-compat/sewer-escape/continuous-escort-continuation-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
SHA256 `9aededc78a77c68acd482903af3354fc470fa4717c925ee3c5bc92ad9f697d6a`. Player(37.988,2562.151,-703.023).
MQ01 is naturally28; both wall rats remain alive. No save-state substitutions.
This follows the accepted [continuous new-game opening through19](SEWER-CONTINUOUS-OPENING-VERIFICATION.json)
through one exact fresh-load boundary; the full single-process escape is unproven.

All2202 accumulated ASan/UBSan engine cases pass with leak checking at a057b0af86;
the instrumented runtime builds. No new C++/Python implementation change since
that gate. [Sanitizer evidence](SEWER-ACCUMULATED-SANITIZER-VERIFICATION.json).

## Next action — rat/wall course is running; poll before another UI course

`joined-wall-rats-02` runs from the genuine30 checkpoint (launcher session21339).
Ignored recipe `joined-wall-rats-aimed.json` uses ordinary movement, actual camera
view observations, explicit facing gates, normal held punches and physical wall
crossing. `joined-wall-rats-01` remains FAILED: its punches missed while the
camera was still aligning, and the first death gate timed out. Whole entry was
exactly restored, with no engine errors. No health/position/quest correction.

Poll the existing game and inspect its actual outcome before another UI course.
If needed, repair the next observed input/gameplay blocker. Then continue normal
key-corpse loot, Club equipment, keyed door, solo dungeon/shaman, reunion,
Emperor/Amulet, choices and physical exit; verify fresh outdoor save/load and
focused rendered sanitizer gameplay. Assemble the complete new-game course once
joins work. Preserve failures and provenance; do not splice separate shaman/late
saves or broaden into unrelated services or exact combat arithmetic.

The editable continuous opening/escort recipes remain in
`scripts/data/oblivion_compat/`. Unbound F8 advances only local read-only observer
phases; phase-scoped wait/aim patterns prevent stale waypoint matches. The full
new-game escort assembly is prepared but unrun; original scripts own progression.

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
