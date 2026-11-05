# Full sewer escape — current handoff

## Target and acceptance gaps

Play the original opening continuously from prison through the real sewer exit,
using ordinary controls and original quest progression. Include equipment/loot,
combat/traversal, escort reunion, Emperor death/Amulet, character choices,
MQ01 stage88 exit and fresh-process save/load outdoors. No injected health,
quest/completion state or skipped required encounters qualifies.
**Full escape remains incomplete.**

Remaining: Baurus resuming Follow after his completed OncePerDay Find, full
party/player entry into ImperialDungeon03, Emperor death/Amulet, class choice,
sewer exit88, outdoor reload and continuous
prison-to-outdoors acceptance. Shaman encounter victory remains unaccepted;
actual contact/staff behavior is implemented, but earlier fight attempts lost.
Fresh full2191 ASan/UBSan engine tests now pass with leak checking for1eb42debb1;
rendered sanitizer continuation remains due. Preserve original check provenance.

## Current chunk — original54/56 reached; Baurus blocker isolated

Ordinary mouse/W/Shift input follows the original corridors to the Emperor.
Three owned C healing casts leave Player alive at71.19003 health. Original
CharGenScript advances52→54 and unlocks0226a2. A fresh continuation restores
that whole save exactly, then the original Glenroy Escort01d0ed and Emperor
Travel00bf1e cross into ImperialDungeon03/022282. Glenroy reaches his own
marker00bf05 and the original callback advances56. Emperor reaches0c7970.
This crossing uses the Emperor's authored Travel; it does not establish generic
cross-cell recipient steering. Captures and actual NPC cell/position state inspected.

Baurus remains in Subterrane022288 on completed Find04f201. That package has
OncePerDay and precedes Follow017d7d; the flag is parsed but not honored in
selection. Stage54 EVP does not dislodge the completed winner. Original records
show no higher relevant replacement until66, while stage56's next gate requires
Baurus and the Emperor in ImperialDungeon03. **Full party/58 remains unaccepted.**

Evidence and retained failed attempts:
[SEWER-MARKER-G-PROGRESSION-VERIFICATION.json](SEWER-MARKER-G-PROGRESSION-VERIFICATION.json).
The slower follow03 course died to real generic assassin01774a melee damage.
Successful05 passes using ordinary movement/healing without an injected kill;
do not describe the failed damage as falling. Initial SDL focus needs settling;
later steering uses camera-view observations and fresh alignment gates.

Current input:
`build/oblivion-compat/sewer-escape/subterrane-marker-g-escort-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Quest56; Player near(1292.136,7259.708,-1183) in022288, Warrior, axe and armor.
Glenroy near(2827.590,7857.444,-1238.728), Emperor at(2847.786,7633.505,-1184.023)
in022282; both alive. Baurus near(1459.880,4922.578,-920.538) in022288.
Editable follow and Marker G courses live under `scripts/data/oblivion_compat/`.

## Next action

Implement persistent OncePerDay completion/selection ownership so Baurus can
resume his original Follow package without replaying the completed greeting or
its callback. Cover save/reload and day rollover, then verify actual Baurus
movement and cell transition. Use ordinary Player movement through the original
unlocked0226a2→022812 door; reach the stage56 proximity/party gate and continue
toward the Emperor death/Amulet sequence. Do not inject stages or relocate actors.
Finish this required route dependency before expanding into unrelated AI flags.

The prior Marker F chunk is committed as1eb42debb1: separate ephemeral escort
routes, waiting for the recipient, collision-swept smaller step rises under
an overhang, and fixed-endpoint completion. Normal2191 and fresh sanitized2191
engine tests pass. Original52 callback and exact fresh restart are covered in
[SEWER-ESCORT-MARKER-F-VERIFICATION.json](SEWER-ESCORT-MARKER-F-VERIFICATION.json).
The original full escape acceptance above remains unchanged.

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
then A selects the first response. F5 requires a closed conversation. Preserve
failed courses and death attempts with their original status and provenance.
Earlier historical detail: [SEWER-ESCAPE-HISTORY.md](SEWER-ESCAPE-HISTORY.md).
