# Full sewer escape — current handoff

## Target course and remaining gaps

Play the original opening continuously from prison to the real sewer exit,
using ordinary controls and original quest progression. Include equipment/loot,
combat/traversal, escort reunion, Emperor death/Amulet, character choices,
MQ01 stage 88 exit and fresh-process save/load outdoors. No console progression,
injected completion state or skipped required encounters qualifies.
**The full escape is incomplete.** Remaining: eastern cavern encounters and rest of
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
The rolling-log continuation and hashes remain in its verification index.

## Current continuation — healing and northern patrol verified, 2026-11-04

Ordinary Space activation loots chest 051d72: three healing potions and one
respite potion transfer, leaving the source chest empty in the save. Two inventory
doses restore health from 12.16 to 52.16. Fresh-process continuation preserves
Player, chest, life and script state exactly. Seven original pathgrid waypoints
reach the northern patrol. Goblin 03b543 dies in ordinary melee; goblin 03b541
joins from the next chamber. The remaining healing dose restores 20 health,
and a second ordinary melee course defeats that reinforcement. Both deaths
are Player-owned. Captures and saves inspected; corpses still appear upright.

Evidence and retained observer failure:
[SEWER-CAVERN-HEALING-VERIFICATION.json](SEWER-CAVERN-HEALING-VERIFICATION.json).
No engine changes or additional broad builds were needed. Container activation
currently takes all contents with a notification; selective browsing and Lua
Container.content support for TES4 containers remain separate limitations.

Current genuine continuation save:
`build/oblivion-compat/sewer-escape/cavern-arena-fight-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256:`bd016069aa8a8b0331917358352ca2402cf10609524100a7348bbdd4aacfc0cd`.
Player (4468.253,2764.886,-481.258), health 29.620010; no healing potions left.
The chest and all six encountered cavern goblin deaths remain consumed.

## Next bounded chunk — eastern cavern encounters and reunion

Continue from pathgrid node 240 toward eastern goblin 03b52f near (5865,3543,-454),
then shaman 03b52e near (5508,4520,-401), remaining cavern traversal and exit.
Prepared ordinary-input approach (not yet run):
`build/oblivion-compat/sewer-escape/cavern-eastern-approach-prepared.json`.
Audited path: `cavern-patrol-to-eastern-goblin-path.json` in the same evidence root.
Remaining rats 047b65/66/67 and any original triggers must be checked as encountered.
Door 03816c at (4384,6144,64) connects to 026c36 in ImperialDungeon02 /
Imperial Subterrane cell 022288, near (3200,3203,-761). Reunion remains original-record
inspection, not traversal acceptance. Full-course requirements above remain open.

Historical implementation details and retained failures:
[SEWER-ESCAPE-HISTORY.md](SEWER-ESCAPE-HISTORY.md). The earlier accepted tutorial
slice is documented in
[TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md](TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md).
