# Full sewer escape — current handoff

## Target and acceptance gaps

Play the original opening continuously from prison through the real sewer exit,
using ordinary controls and original quest progression. Include equipment/loot,
combat/traversal, escort reunion, Emperor death/Amulet, character choices,
MQ01 stage88 exit and fresh-process save/load outdoors. No injected health,
quest/completion state or skipped required encounters qualifies.
**Full escape remains incomplete.**

Remaining: follow the escort beyond Marker F, cross-cell escort progression,
Emperor death/Amulet, class choice, sewer exit88, outdoor reload and continuous
prison-to-outdoors acceptance. Shaman encounter victory remains unaccepted;
actual contact/staff behavior is implemented, but earlier fight attempts lost.
Fresh sanitizer integration for accumulated navigation/dialogue/escort changes
remains due. Do not relabel older full-suite checks as current-source checks.

## Current chunk — Marker F escort completed

The original Glenroy Escort017d7e now supplies same-cell recipient movement to
the Emperor's stationary Travel/CurrentLocation017d7c without replacing either
package. Glenroy waits for the recipient. Separate ephemeral escort/combat routes
prevent peaceful combat cleanup from erasing the recipient's waypoint cursor.

The second doorway exposed a real capsule-stepping failure against stone
IDNHallExitN02: the full34-unit rise hit an overhang while crossing a small
threshold and ejected the actor backwards. Bounded smaller-rise retries retain
forward/down collision sweeps. The real Bullet regression fails before the fix,
passes after it, and blocks an opening with insufficient headroom without roof
overlap. Open-door collision exceptions also survive the full approach distance.

Both actors traverse the original route to Marker F. Fixed-endpoint Escort now
completes on arrival instead of waiting for its daily schedule to expire. The
original OnPackageDone callback advances52 exactly once. Fresh-process F5
reload/resave restores the whole state exactly, retains52 and does not replay the
callback. Player remains alive at50.07003 health; both escorts remain alive.
This is composite movement/arrival evidence; the earlier route run remains a
failed whole scenario because arrival completion was not yet implemented.

Final focused43 cases and full2191 normal engine tests pass. Captures inspected.
Evidence, source fingerprints and retained failures:
[SEWER-ESCORT-MARKER-F-VERIFICATION.json](SEWER-ESCORT-MARKER-F-VERIFICATION.json).
Editable normal-input courses:
`scripts/data/oblivion_compat/oblivion_sewer_marker_f_escort.json` and
`oblivion_sewer_marker_f_reload.json`.

Current input:
`build/oblivion-compat/sewer-escape/subterrane-marker-f-reload-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Player is still in the ambush room, with Warrior birthsign, axe and armor.
Glenroy is near(943.799,7018.927,-1247); Emperor near(978.265,7118.037,-1247).
Same-cell high-process recipient steering is covered. Cross-cell following and
other recipient package types are not established by this chunk.

## Next action

Run fresh sanitizer integration, then use ordinary movement to follow the pair.
Original CharGenScript advances52→54 when Player is within500 of the Emperor.
Continue the authored escort toward the final chamber; stage56 also requires
Baurus/Uriel in ImperialDungeon03 before58. Resolve actual blocking behavior as
encountered. Do not skip to the Emperor death sequence or broaden into unrelated
systems. Reassess sequencing if these remaining story gaps stop shrinking.

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
