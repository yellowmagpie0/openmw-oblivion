# Full sewer escape — current handoff

## Target and acceptance gaps

Play the original opening continuously from prison through the real sewer exit,
using ordinary controls and original quest progression. Include equipment/loot,
combat/traversal, escort reunion, Emperor death/Amulet, character choices,
Baurus CharacterGen88, MQ01 stage88 at the physical exterior exit, and
fresh-process save/load outdoors. No injected health,
quest/completion state or skipped required encounters qualifies.
**Full escape remains incomplete.**

Remaining: physical sewer/exterior exit, outdoor reload, shaman victory and
continuous prison-to-outdoors acceptance. Baurus dialogue88 and class choice now
pass; CharacterGen88 is distinct from MQ01's final exit stage88. Current2197 normal
engine tests pass. Component3262 evidence is reused at its original475c source;
components did not change in the target-route patch. Fresh accumulated sanitizer
integration remains due: default-open-sanitizer-integration-01 was deliberately
cancelled before tests to batch this observed navigation repair, with driver and
owned build group verified terminal. Do not treat its receipt or compiled objects
as a pass. Earlier1eb42 sanitizer coverage retains its original provenance.

## Current chunk — first final-sewer rat and western bridge approach accepted

Normal stairs/descent reaches the western rat0666a7. The first walking course
stalled nearby while it bit Player and is failed/excluded. Clean landing replay
stops at the encounter, then ordinary block/approach/axe input commits two contacts
(1.79677 normal,5.3402 power) and kills its4HP through actual damage. Rat life is
Dead with Player killer; base02509b count increments9→10. Genuine F5 and fresh
continuation retain every field exactly at entry, no old hit/death replay,
health80.85667 and empty pending death queue. Player clears the corridor to the
bridge. Fatigue legitimately regenerates120.6461→170 during that travel.
No new engine code; normal2197 evidence remains at original class-chunk provenance.
[SEWER-FINAL-WESTERN-RAT-VERIFICATION.json](SEWER-FINAL-WESTERN-RAT-VERIFICATION.json).

Current clean input:
`build/oblivion-compat/sewer-escape/sewers03-bridge-approach-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Cell0272f2; Player(-1001.876,333.660,6543.079), health80.85667/base110, axe drawn,
Warrior class/birthsign, one Amulet and no exit key. Rat0666a7 dead once; other
final sewer creatures remain alive. Recipes: `oblivion_sewer_final_western_rat.json`
and `oblivion_sewer_final_bridge_approach.json`.

## Prior chunk — final sewer entry and fresh landing accepted

Normal Player movement/jump crosses the opened assassin passage into the chamber
beyond barred side gate03f1b3. Original gate script permits opening only from the
other side; the Player course never unlocks it. Door02282e uses Baurus key01e702,
opens/unlocks through ordinary activation, and its original script removes the
key once. Player walks through, activates manhole0270e2, and physically enters
ImperialSewers03. Genuine F5 saves and a fresh landing reload pass; all three entry
boundaries restore every field exactly. Health remains84.29768, one Amulet,
Warrior class/birthsign, key absent and unlocked/open key-door pose retained.
No new engine changes; normal engine2197 evidence from the class chunk applies.
[SEWER-FINAL-SEWERS-ENTRY-VERIFICATION.json](SEWER-FINAL-SEWERS-ENTRY-VERIFICATION.json).

Prior clean landing input:
`build/oblivion-compat/sewer-escape/sewers03-reload-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Cell0272f2 ImperialSewers03; Player at(-2595.823,1077.309,6878.335), health84.29768/base110,
Warrior class023787 and birthsign01fd9d, axe/armor and one Amulet0250a0; key removed.
Recipes: `oblivion_sewer_sanctum_secret_passage.json`,
`oblivion_sewer_final_sewers_entry.json`, `oblivion_sewer_final_sewers_reload.json`.

## Prior chunk — class choice and Baurus dialogue88 accepted

Native GetIsClassDefault228 now compares its CLAS parameter with the Player's
recomputed/cached recommendation, irrespective of run-on actor. It shares the
existing tutorial-use/cache/save owner with GetClassDefaultMatch. Independent
original4F83D0 execution passes12 parameter/type/null/identity cases against the
hash-verified executable; the existing recommendation oracle retains its original
provenance. No fixed/default response or Baurus special case.

Normal dialogue86 reaches original Barbarian recommendation01e8ee and ShowClassMenu87.
Hat-axis input chooses Warrior. After closing the original Goodbye response,
ordinary camera aim/activation reopens Baurus's greeting006924 (same specialization,
different class); normal linked answers reach88 and MQ02 stages10/20. Original
Baurus.chargenOver becomes1. Genuine F5 saves and fresh-process reload pass with
whole entry state exactly restored; no class menu/result replay, one key/Amulet,
all prior death counts retained and no pending death events. Health base/current
both increase10 through the class change; previous damage is preserved.
All2197 normal engine cases pass; the focused owner/command/dialogue regression
covers both master layouts, recommendation recomputation and current-class mismatch.
Evidence and retained failures:
[SEWER-CLASS-CHOICE-VERIFICATION.json](SEWER-CLASS-CHOICE-VERIFICATION.json).

Prior clean class-choice input:
`build/oblivion-compat/sewer-escape/baurus-class-reload-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Quest88/MQ02=20; Player at(2800.493,10065.103,-1439), health84.29768/base110,
Warrior class023787 and birthsign01fd9d, axe/armor, one unequipped Amulet0250a0
and Baurus key01e702. Cached recommendation remains Barbarian023788.
Glenroy, Emperor and final assassin dead once; Baurus alive. Recipes:
`scripts/data/oblivion_compat/oblivion_sewer_baurus_class_{completion,reload}.json`.

## Next action

Continue ImperialSewers03 from the accepted bridge approach to exit door04bed7 at
(3872,2048,7008). Winning records/pathgrid and actors are audited in
`build/oblivion-compat/sewer-escape/sewers03-{original-route,exit-doors,exit-path}.json`.
The western stairs and rat are accepted. Next cross bridge nodes75→74→73→72→71
and handle encounters as they approach, then continue the lower passage to stairs
near(842,470). There are remaining rat066a04, Goblin066a02,
Goblin Berserker066a03 and passive regenerating rat0666da in the cell. Remaining combat and
actual traversal remain unproven here; use ordinary controls and original scripts.
CGSewerExitScript shows the character-review choice; Finished (button3) requests
MQ01 stage88, whose real result disables chargen tracking, clears skill-use history
and retains the cached class. Verify physical exterior transition and fresh outdoor
reload, not just the stage. Original script audits are `character-exit-scripts.json`
and `mq01-exit-stage88.json` in the ignored sewer evidence root.
Shaman victory and continuous prison-to-outdoors acceptance remain required.
Batch accumulated sanitizer integration after related scene repairs; the prior
cancelled build is not a pass.

Stage88 AddTopic calls still use the existing deferred handler. Its Autosave request
is rejected while dialogue is open; later ordinary F5 works. These effects remain
open, not established by the stage88/progression proof. Original SetOpenState0
unlocked the isolated diagnostic back gate; that adjacent command behavior remains
recorded in the gate artifact, not implemented.

Earlier accepted checkpoints:
- [Emperor death/Amulet/final victory/mourning86](SEWER-EMPEROR-DEATH-VERIFICATION.json):
  original scripts, real assassin passage/contact and clean fresh reload. Combat
  may prefer a materially shorter connected nearby authored path over a continuous
  detour; physics still owns movement/hits. This is bounded gameplay behavior,
  not exact native pathfinding parity. Failed post-victory attack save excluded.
- [Friendly force greeting](SEWER-FRIENDLY-GREETING-VERIFICATION.json): shared
  read-only Player fight score permits friendly positive aggression; Baurus
  approaches/greets at86. Existing hostility/life/LOS/control guards retained.
- [Default-open gate68](SEWER-SANCTUM-GATE-VERIFICATION.json),
  [Sanctum entry/bound dagger](SEWER-SANCTUM-ENTRY-VERIFICATION.json) and
  [daily packages](SEWER-DAILY-PACKAGE-VERIFICATION.json).

## Reusable evidence and known limitations

- [Completed melee slice](TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md): prison, first
  assassin ambush, Player participation, escort departure and fresh combat reload.
- [Trap hazards](SEWER-TRAP-HAZARD-VERIFICATION.json),
  [rolling logs](SEWER-ROLLING-LOG-VERIFICATION.json),
  [cavern healing](SEWER-CAVERN-HEALING-VERIFICATION.json) and
  [eastern goblin](SEWER-EASTERN-CAVERN-VERIFICATION.json): real traversal,
  loot, equipment, damage, healing and melee.
- [Healing casts](SEWER-HEAL-CAST-VERIFICATION.json) and
  [staff projectiles](SEWER-STAFF-PROJECTILE-VERIFICATION.json): genuine casts,
  release/flight ownership and fresh restart. General NPC spell fallback and
  Player Flare casting remain unsupported; shaman victory is still open.
- [Subterrane entry](SEWER-SUBTERRANE-ENTRY-VERIFICATION.json) and
  [second ambush](SEWER-SECOND-AMBUSH-VERIFICATION.json): normal cavern exit,
  stage32/34/36 and both assassin deaths through original callbacks.
- [Player sex conditions](SEWER-REUNION-SEX-CONDITION-VERIFICATION.json) and
  [birthsign](SEWER-BIRTHSIGN-VERIFICATION.json): original reunion dialogue,
  Warrior choice and linked questions through50, exact fresh reload, no doubled
  bonuses. Other birthsign choices lack this runtime coverage. Menu descriptions
  are blank; upright goblin corpses and take-all-only container loot remain gaps.

UI courses run singly. Initial controller choice is unselected: hat-axis Down,
then A selects the first response. Use dpad_y axes; this virtual pad does not map
D-pad EV_KEY buttons to SDL state (retained delivery-timeout diagnostics). F5 requires a closed conversation. Preserve
failed courses and death attempts with their original status and provenance.
Earlier historical detail: [SEWER-ESCAPE-HISTORY.md](SEWER-ESCAPE-HISTORY.md).
