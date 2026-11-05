# Full sewer escape — current handoff

## Target and acceptance gaps

Play the original opening continuously from prison through the real sewer exit,
using ordinary controls and original quest progression. Include equipment/loot,
combat/traversal, escort reunion, Emperor death/Amulet, character choices,
MQ01 stage88 exit and fresh-process save/load outdoors. No injected health,
quest/completion state or skipped required encounters qualifies.
**Full escape remains incomplete.**

Remaining: Baurus class-choice greeting/selection, sewer exit88, outdoor reload,
shaman victory and continuous prison-to-outdoors acceptance. Current2196 normal
engine tests pass. Component3262 evidence is reused at its original475c source;
components did not change in the target-route patch. Fresh accumulated sanitizer
integration remains due: default-open-sanitizer-integration-01 was deliberately
cancelled before tests to batch this observed navigation repair, with driver and
owned build group verified terminal. Do not treat its receipt or compiled objects
as a pass. Earlier1eb42 sanitizer coverage retains its original provenance.

## Current chunk — Emperor death/Amulet, final victory and mourning86 accepted

Normal input reaches J and triggers original70/71/72/73. The Emperor's original
final greeting awards one Amulet74, kills Glenroy through its result script,
opens the secret wall, restrains Emperor and starts the final assassin's attack.
Normal Goodbye and F5 retain that genuine setup.

Old combat navigation detoured from the opened wall toward the side gate. The
winning native PGRD has a short connected path182→181→109→108 through the opening,
including a50-unit drop. Collision rays through it are clear; endpoint LOS0 is
expected around the corner. Combat now prefers a materially shorter complete
nearby authored path without requiring endpoint visibility. Physics owns every
step and hit. Disabled/disconnected nodes, foreign cells and distant endpoints
cannot invent a route; shorter continuous routes remain preferred. This is a
bounded gameplay policy, not exact original pathfinding parity.

Fresh original74 continuation restores every field exactly. Assassin walks the
opening and commits an actual dagger hit to Emperor; original OnHit Kill and
gamemode reach76/80. Player heals/blocks while Baurus joins and wins the final
fight; original OnDeath reaches84. Clean fresh restart restores all fields
exactly, keeps three death counts at1, no bound assassin gear and one Amulet,
then completes mourning85/86. Captures inspected, failures retained:
[SEWER-EMPEROR-DEATH-VERIFICATION.json](SEWER-EMPEROR-DEATH-VERIFICATION.json).
The failed melee01 continued attacks after victory and hit Baurus once; its save
is excluded from the main chain. Clean victory01 stops on death and passes.

Current input:
`build/oblivion-compat/sewer-escape/final-victory-reload-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Quest86; Player at(2800.493,10065.103,-1439), health74.29768, Warrior, axe/armor,
one unequipped Amulet. Glenroy, Emperor and final assassin dead once; Baurus alive.
Reusable J/Amulet/death/victory/reload recipes are under
`scripts/data/oblivion_compat/oblivion_sewer_{sanctum_marker_j,emperor_amulet,emperor_death,final_assassin_victory,final_victory_reload}.json`.
Prior [default-open gate68](SEWER-SANCTUM-GATE-VERIFICATION.json),
[Sanctum entry/bound dagger](SEWER-SANCTUM-ENTRY-VERIFICATION.json),
[daily packages](SEWER-DAILY-PACKAGE-VERIFICATION.json) and earlier marker records
retain their original provenance.

## Next action

Fix the observed Baurus class-choice force greeting: original package01e8e4 is
Ambush targeting Player, phase ReadyForM15Combat at86; original84 result sets
aggression5. The friendly Ambush greeting currently admits only aggression0.
Reuse native hostility/disposition policy to distinguish greeting from attacking;
do not simply admit every hostile Ambush or special-case this reference. Complete
original class choice and exit, then outdoor fresh reload and remaining full-course
acceptance. Batch fresh sanitizer integration after the related scene repairs.
Original SetOpenState0 unlocked the isolated diagnostic back gate; that adjacent
command behavior remains recorded in the previous artifact, not implemented.

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
