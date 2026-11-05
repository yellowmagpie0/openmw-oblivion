# Full sewer escape — current handoff

## Target course and remaining gaps

Play the original opening continuously from prison to the real sewer exit,
using ordinary controls and original quest progression. Include equipment/loot,
combat/traversal, escort reunion, Emperor death/Amulet, character choices,
MQ01 stage 88 exit and fresh-process save/load outdoors. No console progression,
injected completion state or skipped required encounters qualifies.
**The full escape is incomplete.** Remaining: healing supplies and rest of
the caverns, reunion, Emperor/Amulet, character choices, final exit/outdoor reload,
and the continuous prison-to-outdoors acceptance run. Goblin corpses still
appear upright; dead-actor physical pushback/presentation needs follow-up.

## Current checkpoint — mace hazard verified, 2026-11-04

Original swinging maces now cause script-driven contact damage and timed
pushback. Actual courses verify environmental goblin death, Player damage,
active-contact saving, and fresh-process restart without repeating damage.
Schema 86 persists contact ownership alongside animated bodies. Player health,
inventory, actor life and fixed anchors survive restart; pushback continues.
Ordinary death and confirmation through the load prompt also restore the saved
living Player. Continuous trap damage is explicitly unsupported; these maces
and the upcoming logs use noncontinuous damage.

Validation: all 3,252 normal component and 2,178 engine tests pass; 554 Python tests
pass with their earlier recorded provenance. All 284 selected engine tests pass
under ASan/UBSan with leak detection, closing the previously deferred anchor/
Activate/hazard sanitizer gate. Exact inventories, no failures/skips. Normal and
sanitizer source fingerprint:
`08d2ff8e0abb319e4a747c1d9082968106932abad16af289093a1a292a38b455`.
Game SHA256:`501452531f4992357140e927914cdbf5898bd53599610674fffe6ee7aa4344ae`.

Detailed receipts, scope, save hashes and failed attempts are retained under
`build/oblivion-compat/sewer-escape/`. The concise evidence index is
[SEWER-TRAP-HAZARD-VERIFICATION.json](SEWER-TRAP-HAZARD-VERIFICATION.json).
The independent original-handler audit is
[SEWER-TRAP-DAMAGE-NATIVE-AUDIT.json](SEWER-TRAP-DAMAGE-NATIVE-AUDIT.json).
Editable courses are `scripts/data/oblivion_compat/oblivion_sewer_cavern_*`.

## Current continuation — rolling logs verified, 2026-11-04

The original 10.2.0.0 CTrapLogs01 asset now loads for placed references;
actor-ragdoll version constraints remain unchanged. Ordinary activation releases
all six logs. Body 43 kills goblin 03b51a with 31.5 environmental damage.
Fresh-process reload preserves the six-body graph, positions, rotations, native
packed velocities, script state, Player and death ownership without replay.
Projected world velocity differs by at most 0.000002 units/sec from unit conversion;
the reviewed audit uses 0.00001 tolerance only for that projection.

All 61 ActorRagdollTest cases pass normally and under ASan/UBSan (component leak
checking disabled). Two focused asset engine cases pass. Broader hazard gates
above retain their original provenance. Source fingerprint and runtime receipts:
[SEWER-ROLLING-LOG-VERIFICATION.json](SEWER-ROLLING-LOG-VERIFICATION.json).
Failed version/compile attempts and the overstrict initial velocity comparison
remain retained. Raw trap diagnostic body IDs use ambient hexadecimal formatting:
log body=2b means 43; earlier mace body=38 means 56, not decimal 38.

`cavern-log-goblin-01` defeats surviving goblin 03b518 through ordinary melee,
with Player-owned death. `cavern-healing-approach-01` traverses six original pathgrid
nodes down the log slope. Captures inspected; corpses still appear upright.
Current genuine continuation save:
`build/oblivion-compat/sewer-escape/cavern-healing-approach-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256:`d428dd117f1eb4514a7b5d20798006f16818e9d5c57cb5bd95de92dfa28c9feb`.
Player (4551.829,1563.679,-469.257), health 12.159996. Both log goblins dead.

## Next bounded chunk — healing supplies, remaining caverns and reunion

Open healing chest 051d72 at (4645,1510,-464), loot its three 009310 healing
potions and one 00930e potion, heal through inventory, then continue the caverns.
Chest contents are verified in the current save, not yet looted.
Remaining caverns/goblins lead to door 03816c at (4384,6144,64), connecting to
026c36 in ImperialDungeon02 / Imperial Subterrane cell 022288, near (3200,3203,-761).
The reunion destination is original-record inspection, not traversal acceptance.
The route is advancing again; remaining full-course requirements above are open.

Historical implementation details and retained failures:
[SEWER-ESCAPE-HISTORY.md](SEWER-ESCAPE-HISTORY.md). The earlier accepted tutorial
slice is documented in
[TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md](TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md).
