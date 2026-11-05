# Full sewer escape — current handoff

## Target and acceptance gaps

Play the original opening continuously from prison through the real sewer exit,
using ordinary controls and original quest progression. Include equipment/loot,
combat/traversal, escort reunion, Emperor death/Amulet, character choices,
MQ01 stage88 exit and fresh-process save/load outdoors. No injected health,
quest/completion state or skipped required encounters qualifies.
**Full escape remains incomplete.**

Remaining: Player entry into ImperialDungeon03 and original58 story gate,
Emperor death/Amulet, class choice,
sewer exit88, outdoor reload and continuous
prison-to-outdoors acceptance. Shaman encounter victory remains unaccepted;
actual contact/staff behavior is implemented, but earlier fight attempts lost.
Full2191 ASan/UBSan engine tests and a rendered moving-escort restart pass for
1eb42debb1. New schema89/daily-package sanitizer integration remains due; preserve
those earlier checks with their original provenance.

## Current chunk — Baurus resumed Follow and entered ImperialDungeon03

OncePerDay completion history is now native AI state, keyed by actor/package
and calendar date. Schema89 appends a canonical history map; loaded and unloaded
selection skip completed daily packages and preserve the next package's original
priority. History is recorded before package callbacks and retained through the
existing transactional actor restore. Date/year rollover eligibility is unit-tested.

Legacy schema88 lacks completion dates. Its still-completed daily package is
retired on its first selection day without replaying its callback. Initial
migration changes only version89 and empty history; all other state is exact.
Baurus then selects original Follow017d7d instead of staying on Find04f201.
He passes both doorways, genuinely saves while moving, reloads exactly, continues
following and crosses original0226a2→022812 into ImperialDungeon03/022282.
The completion record remains(032a17,04f201,433/7/28), with no old Find callback.

Full3261 normal component tests,2191 engine tests and560 Python tests pass.
The Python pass retains its original fingerprint before final C++ wire-test-only
offset corrections. Current exact-wire assertions include the new four-byte tail;
reviewed synthetic fixture migrations carry empty daily history and still reject
future schemas. Captures inspected. Evidence and retained failures:
[SEWER-DAILY-PACKAGE-VERIFICATION.json](SEWER-DAILY-PACKAGE-VERIFICATION.json).

Current input:
`build/oblivion-compat/sewer-escape/baurus-marker-g-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Quest56; Player near(1292.136,7259.708,-1183) in022288, health71.19003, Warrior,
axe and armor. All three escorts are now in022282. Baurus is at the real door
landing(2812.340,7463.908,-1184.023), still following the Emperor. Glenroy is near
(2827.590,7857.444,-1238.728); Emperor at(2847.786,7633.505,-1184.023).

## Next action

Use ordinary Player movement through the original unlocked0226a2→022812 door.
An ignored scout recipe is `build/oblivion-compat/sewer-escape/sanctum-door-approach.json`;
its first run may still be active: inspect its actual process/session before reuse.
Then reach the stage56 proximity/party gate and continue toward the Emperor
death/Amulet sequence. Do not inject stages or relocate actors. Batch new daily
state sanitizer integration at the next story checkpoint. Start/interruption
parity for all daily package types and an actual24-hour runtime rollover are not
established by the scoped daily-package proof.

Earlier route evidence:
[Marker F](SEWER-ESCORT-MARKER-F-VERIFICATION.json) and
[original54/56 progression](SEWER-MARKER-G-PROGRESSION-VERIFICATION.json).
The rendered sanitizer original50→52 run reached its600-second limit while
progressing; its retained F5 proves both doorway traversals and a fresh restart
with exact state and25–28 further units of movement. It remains a failed whole
stage52 scenario, and its compiled1eb42debb1 does not cover the daily-package
change. Receipt is linked in the current verification artifact.

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
