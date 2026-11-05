# Full sewer escape — current handoff

## Target and acceptance gaps

Play the original opening continuously from prison through the real sewer exit,
using ordinary controls and original quest progression. Include equipment/loot,
combat/traversal, escort reunion, Emperor death/Amulet, character choices,
Baurus dialogue88, the physical exterior exit and fresh-process save/load outdoors. No injected health,
quest/completion state or skipped required encounters qualifies.
**Full escape remains incomplete.**

Remaining: physical sewer/exterior exit, outdoor reload, shaman victory and
continuous prison-to-outdoors acceptance. Baurus dialogue88 and class choice now
pass; stage88 is not the physical exterior exit. Current2197 normal
engine tests pass. Component3262 evidence is reused at its original475c source;
components did not change in the target-route patch. Fresh accumulated sanitizer
integration remains due: default-open-sanitizer-integration-01 was deliberately
cancelled before tests to batch this observed navigation repair, with driver and
owned build group verified terminal. Do not treat its receipt or compiled objects
as a pass. Earlier1eb42 sanitizer coverage retains its original provenance.

## Current chunk — class choice and Baurus dialogue88 accepted

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

Current clean input:
`build/oblivion-compat/sewer-escape/baurus-class-reload-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Quest88/MQ02=20; Player at(2800.493,10065.103,-1439), health84.29768/base110,
Warrior class023787 and birthsign01fd9d, axe/armor, one unequipped Amulet0250a0
and Baurus key01e702. Cached recommendation remains Barbarian023788.
Glenroy, Emperor and final assassin dead once; Baurus alive. Recipes:
`scripts/data/oblivion_compat/oblivion_sewer_baurus_class_{completion,reload}.json`.

## Next action

Play the physical route from the Sanctum through locked door02282e at
(4501.35,9344.78,-1330.90) using Baurus's key, then manhole0270e2 at
(5120,9344,-1416.80) into ImperialSewers03. Independent winning-record audit:
`build/oblivion-compat/sewer-escape/sanctum-exit-doors.json`.
Find the actual walkable passage; do not assume locked side gate03f1b3 is the route.
Continue to the real exterior exit and a fresh outdoor reload. Shaman victory
and continuous prison-to-outdoors acceptance remain required. Batch accumulated
sanitizer integration after related scene repairs; prior cancelled build is not a pass.

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
