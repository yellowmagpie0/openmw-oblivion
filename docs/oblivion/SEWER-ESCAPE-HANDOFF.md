# Full sewer escape — current handoff

## Target and acceptance gaps

Play the original opening continuously from prison through the real sewer exit,
using ordinary controls and original quest progression. Include equipment/loot,
combat/traversal, escort reunion, Emperor death/Amulet, character choices,
MQ01 stage88 exit and fresh-process save/load outdoors. No injected health,
quest/completion state or skipped required encounters qualifies.
**Full escape remains incomplete.**

Remaining: original59 onward conversations and Emperor death/Amulet, class choice,
sewer exit88, outdoor reload and continuous
prison-to-outdoors acceptance. Shaman encounter victory remains unaccepted;
actual contact/staff behavior is implemented, but earlier fight attempts lost.
Full2191 ASan/UBSan engine tests and a rendered moving-escort restart pass for
1eb42debb1. New schema89/daily-package/bound-dagger sanitizer integration remains due; preserve
those earlier checks with their original provenance.

## Current chunk — Sanctum entry and original58 accepted

Player follows ordinary observed waypoints to original unlocked0226a2 and uses
Space to cross022812 into ImperialDungeon03/022282. All three escorts are there;
the original party/proximity gate advances56→58. Native final assassin01e703
(base01e6ff) originally failed to render because its ability06461c includes BWDA.
Bound Dagger now resolves through winning MGEF001855 to weapon02627c, alongside
existing MYTH/MYHL armor and helmet grants.

An earlier actor01770c/base017623 has dagger-only0521ca before armor/mace03e91b.
Admitting dagger exposed the shared weapon slot. A bounded last-authored-weapon
policy preserves the later mace and earlier armor, with original source indices;
within-ability and armor overlaps still reject. Both orders and invalid weapon
associations are tested. This is not instruction-proven arbitrary stacking parity.

Clean door-entry rerun passes without render errors. Final assassin owns exactly
one equipped dagger at condition2500, source(06461c,effect2), plus the authored
armor/helmet. Fresh process reload restores the whole state exactly, keeps that
single dagger and preserves older mace ownership. All2193 normal engine cases
pass, including8 bound-item tests. Captures inspected. Evidence and failures:
[SEWER-SANCTUM-ENTRY-VERIFICATION.json](SEWER-SANCTUM-ENTRY-VERIFICATION.json).

Current input:
`build/oblivion-compat/sewer-escape/sanctum-reload-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`.
Quest58; Player and all three escorts in022282, Player at the real landing near
(2812.340,7463.908,-1184), health71.19003, Warrior, axe and armor. Baurus is nearby;
Emperor near(2847.786,7633.505,-1183), Glenroy near(2827.590,7857.444,-1247).
Editable Sanctum approach/entry/reload recipes are under
`scripts/data/oblivion_compat/oblivion_sewer_sanctum_*.json`.

## Next action

Inspect the actual status of the original conversation continuation before reuse:
`build/oblivion-compat/sewer-escape/sanctum-conversation-01` may still be running.
It waits for original59, then normally F5 saves. Follow original conversation,
escort and ambush progression toward the Emperor death/Amulet, then class choice
and sewer exit. No injected stages or relocation qualifies. Batch fresh sanitizer
integration for schema89/daily-package/bound-dagger changes at the next story
checkpoint; older sanitized gates do not cover these changes.

Prior required dependency is committed asb841bd36a5: OncePerDay completion
history keyed by actor/package/calendar day, schema89, loaded/unloaded selection,
legacy completion retirement without callback replay, exact fresh restart and
Baurus following through the original door. Normal3261 component/2191 engine and
560 Python tests passed at recorded source fingerprints. Details:
[SEWER-DAILY-PACKAGE-VERIFICATION.json](SEWER-DAILY-PACKAGE-VERIFICATION.json).
Earlier [Marker F](SEWER-ESCORT-MARKER-F-VERIFICATION.json) and
[original54/56](SEWER-MARKER-G-PROGRESSION-VERIFICATION.json) retain provenance.
Rendered sanitized original50→52 exceeded600 seconds while progressing; its
retained F5 proves doorway traversal and exact moving restart with25–28 further
units. That full52 scenario remains failed and compiled1eb42debb1 predates the
current changes. Its receipt is linked in the daily-package artifact.

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
