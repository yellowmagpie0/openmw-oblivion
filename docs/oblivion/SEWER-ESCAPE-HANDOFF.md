# Full sewer escape — current handoff

## Target course and remaining gaps

Play the original opening continuously from prison to the real sewer exit,
using ordinary controls and original quest progression. Include equipment/loot,
combat/traversal, escort reunion, Emperor death/Amulet, character choices,
MQ01 stage 88 exit and fresh-process save/load outdoors. No console progression,
injected completion state or skipped required encounters qualifies.
**The full escape is incomplete.** Remaining: shaman encounter and rest of
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

The healing/patrol continuation and hashes remain in its verification index.

## Current continuation — eastern goblin verified, 2026-11-04

The initial eastern fight failed with Player death using the worn club. A normal
inventory switch equips the already-owned Rusty Iron War Axe 090613, then a
continuous seven-node approach and melee course defeats goblin 03b52f. This is
ordinary inventory/navigation/attack input, no health or completion injection.
The axe still deals about 0.82 per hit; the successful course is not evidence of
a large weapon-damage improvement. Death, failed wait, wrong initial weapon label,
and input-delivery failures are preserved in
[SEWER-EASTERN-CAVERN-VERIFICATION.json](SEWER-EASTERN-CAVERN-VERIFICATION.json).
No implementation changed; prior build/sanitizer results retain their provenance.

Current genuine continuation save:
`build/oblivion-compat/sewer-escape/cavern-eastern-combined-01/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256:`510e8a520909b21756bce107a8a703edeb05fda7850726693d1cede234e7c1db`.
Player (5508.464,3257.950,-448.217), health 9.670021, axe equipped; no healing
potions left. Seven encountered cavern goblins are dead. Capture inspected;
upright corpses remain a known presentation gap.

## Next bounded chunk — tutorial magic and shaman, then reunion

World::castSpell currently rejects all TES4 spell execution. Original Player
NPC_000007 owns Heal Minor Wounds 000136 (REHE 8, self, instant) and Flare.
Shaman 03b52e / base03b52d owns staff066c45 with enchantment066c42 (SHDG 5,
target, instant). Audit actual cost/casting/effect rules, implement the required
normal-input healing path and prove resource/save ownership, then verify the
shaman's actual staff behavior. Do not accept an inert shaman as a complete fight
or broaden into unrelated magic before these tutorial abilities work.
Raw original-record audit is ignored: `cavern-next-magic-record-audit.json` under
`build/oblivion-compat/sewer-escape/`; normalized facts are in the evidence index.

Prepared shaman approach (not yet run): `cavern-shaman-approach-prepared.json` in
that evidence root, with audited path `cavern-eastern-to-shaman-path.json`.
Remaining rats 047b65/66/67 and original triggers must be checked as encountered.
Door03816c at (4384,6144,64) connects to 026c36 in ImperialDungeon02 /
Imperial Subterrane cell022288, near (3200,3203,-761). Reunion remains original-record
inspection, not traversal acceptance. Full-course requirements above remain open.

Historical implementation details and retained failures:
[SEWER-ESCAPE-HISTORY.md](SEWER-ESCAPE-HISTORY.md). The earlier accepted tutorial
slice is documented in
[TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md](TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md).
