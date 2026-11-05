# Full sewer escape — current handoff

## Target course and remaining gaps

Play the original opening continuously from prison to the real sewer exit,
using ordinary controls and original quest progression. Include equipment/loot,
combat/traversal, escort reunion, Emperor death/Amulet, character choices,
MQ01 stage 88 exit and fresh-process save/load outdoors. No console progression,
injected completion state or skipped required encounters qualifies.
**The full escape is incomplete.** Remaining: rolling-log encounter and rest of
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

## Next bounded chunk — rolling logs, then healing supplies/reunion

`cavern-log-approach-02` passes 15 original pathgrid waypoints to
Player (3527.4077,1413.7827,-289.8932), health 29.079994; captures inspected.
Pristine continuation:
`build/oblivion-compat/sewer-escape/cavern-log-approach-02/userdata/saves/Bendu_Olo/Quicksave.omwsave`
SHA256:`1cd131c92c2d90e1f090d4f02a4b75d2f89020fbda51f750e3ffd7dc4da4ebbd`.

`cavern-log-activation-01` reaches normal activation/unequip for 03b49e, then
FAILS with unsupported NIF version. Original CTrapLogs01.NIF is 10.2.0.0,
user 10/stream 9: six mass 20, layer 14, Keyframed convex hull bodies. Reader parsing
works; `components/nifbullet/actorragdoll.cpp::loadRagdollDefinition` rejects its
version. Add scoped reference-asset support, focused loader coverage, and replay
from the pristine pre-log checkpoint. Keep actor-ragdoll constraints intact.
Prepared failed course: `build/oblivion-compat/sewer-escape/cavern-log-activation-prepared.json`.
Asset/content audits are retained in `tripwire-assets/log-inspection.txt` and
`cavern-log-record-audit.json`. Do not use the failed consumed state for acceptance.

Beyond the logs: healing chest 051d72 at (4645,1510,-464), three 009310 potions;
remaining caverns/goblins lead to door 03816c at (4384,6144,64). It connects to
026c36 in ImperialDungeon02 / Imperial Subterrane cell 022288, near (3200,3203,-761).
These destinations are original-record inspection, not traversal acceptance.

Historical implementation details and retained failures:
[SEWER-ESCAPE-HISTORY.md](SEWER-ESCAPE-HISTORY.md). The earlier accepted tutorial
slice is documented in
[TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md](TUTORIAL-PLAYABLE-CHUNK-HANDOFF.md).
