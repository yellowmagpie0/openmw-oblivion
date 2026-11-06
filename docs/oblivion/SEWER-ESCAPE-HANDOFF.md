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

## Current chunk — Player wall-rat victories and physical passage accepted

The genuine original30 save continues to both authored wall-rat deaths by real
Player power punches, then physical passage through the broken wall. Rat037e42
falls in `joined-wall-rats-03`; its genuine F5 (health68) is restored exactly in
`joined-wall-rats-06`, which kills037e41 and passes the crossing. Final health38,
position(585.390,3175.597,-855.278), both Player killers, shared base death count2,
no pending deaths/actions/combat engagements. Full native T4ST entry comparisons
at both boundaries have zero differences. Capture inspected; no engine changes.
[SEWER-JOINED-WALL-RATS-VERIFICATION.json](SEWER-JOINED-WALL-RATS-VERIFICATION.json).

Raw03 is FAILED later on crossing-facing input; its successful real F5 prefix
alone supplies06. Raw01/02/04/05 remain failed input/aim attempts;06 passes.
World-axis camera observations and ordinary input now converge with fresh samples
and a reasonable~0.04rad facing bound; actual contacts/deaths remain mandatory.
A separate room rat037e40/base02509b stays alive; handle it in the next room route.

Latest genuine save:
`build/oblivion-compat/sewer-escape/joined-wall-rats-06/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Recipes: `oblivion_sewer_joined_first_wall_rat.json` (executed F5 prefix) and
`oblivion_sewer_joined_wall_rats_finish.json` under `scripts/data/oblivion_compat/`.
These are composite continuations, not a full single-process escape.

The preceding [original19→30 course](SEWER-CONTINUOUS-ESCORT-VERIFICATION.json)
proves passage, ambush/five deaths, original conversations26–29, all three escort
cell transitions and Glenroy callback30. Raw course fails only an inherited
MQ01 stage24 exclusion. Independent master audit and trace prove rat037e42's
original wall activation legitimately sets24; all input gates pass, exit0, no
unreviewed errors. Separate reviewed acceptance passes; raw failure is preserved.
Corrected escort recipes still forbid premature MQ01 exit88. The genuine input
follows [new game→19](SEWER-CONTINUOUS-OPENING-VERIFICATION.json) without splicing.

All2202 normal and all2202 accumulated ASan/UBSan engine cases pass with original
fingerprints; sanitizer leak checking enabled at a057b0af86. No engine change
since that gate. [Sanitizer evidence](SEWER-ACCUMULATED-SANITIZER-VERIFICATION.json).

## Next action — fix idle camera preference save before the next course

Actual saves expose a required persistence gap: CAM_/FIRS is1 (first person) at19,
but0 at30 after a long idle escort wait with no R/POV input. Subsequent reloads
retain third person, affecting observed camera/aim behaviour. `getModeForSave()`
only preserves native recovery view; it returns temporary Vanity/Preview as-is,
so World's bool comparison writes false. Native T4ST entry checks do not cover
legacy CAM metadata. Preserve the selected user view across temporary idle view,
keep native recovery semantics, and test actual save/fresh-load behaviour.

All UI runners are terminal; no build/check helper is live. After the focused
camera fix, continue from the genuine health38 save by ordinary healing if
needed, the room rat, key-corpse loot, Club equipment and keyed door. Then join
solo dungeon/shaman, reunion, Emperor/Amulet, choices and physical exterior;
verify fresh full-course outdoor load and focused rendered sanitizer gameplay.
Do not splice separate shaman/late saves, inject health/quest/position, or broaden
into unrelated services/exact combat arithmetic. Preserve every failed attempt.

The complete new-game escort recipe is prepared but unrun. F8 advances only
local read-only observer phases; phase-scoped wait/aim patterns prevent stale
matches. Original scripts and ordinary inputs own all progression.

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
