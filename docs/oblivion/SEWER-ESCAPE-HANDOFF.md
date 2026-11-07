# Sewer regression closure handoff

The immediate deliverable is bounded regression consolidation. **Original full
sewer acceptance is incomplete.** Loaded checkpoint successes do not establish
an uninterrupted new-game escape. The current patrol/eastern correction is
finished; automated-player tactics are frozen. Do not begin the next chunk yet.

## Play now

```bash
./scripts/try-oblivion/tutorial.sh
./scripts/try-oblivion/checkpoint.sh exit
```

The first command starts the authored prison tutorial. The second resumes the
original final-sewer course with normal controls in a private save/profile copy.
`floor`, `combat`, and `story` select the other genuine checkpoints. Each command
accepts an optional Oblivion Data directory. Warrior/melee/Minor Healing is the
selected tested path; arbitrary character builds are not certified.
See [tryout controls and isolation](../../scripts/try-oblivion/README.md).

## Run future checks

```bash
python3 scripts/sewer_regression.py focused --output build/oblivion-compat/sewer-regression/focused-NEW
python3 scripts/sewer_regression.py integration --output build/oblivion-compat/sewer-regression/integration-NEW
```

Use a completed build, a new output directory, and one rendered game at a time.
`--only combat` narrows focused mode. Integration uses all 67 original new-game
sections and, only after success, its own outdoor save in a fresh process.
[Command reference](SEWER-REGRESSION.md) covers dependencies, checkpoint hashes,
transport/regeneration, result classifications and diagnostic locations. This
reuses the existing runner/manifests and save audits; no build or retry loop.

## Closure results

Focused `focused-closure-01` finished: **5 passed, 1 failed**. Floor/collision
recovery passed. Story progression/faction/ownership persistence and its fresh
reload passed. Physical sewer exit and its own fresh outdoor reload passed.
Combat 41–45 failed at the existing supplies/recovery gate after the corrected
patrol/nearby-guard victories and eastern return. Native errors/pending/unsupported
were zero. This timeout remains an unresolved course failure; do not infer an
engine bug or a combat pass. No further tactics tuning is authorized.

The manual checkpoint launcher passed rendered startup and ordinary F5/F9.
The first smoke targeted the wrong display; its harness failure is retained.
[Closure evidence](SEWER-REGRESSION-CLOSURE-VERIFICATION.json) records hashes,
results and diagnostic paths. [Focused diagnostics](../../build/oblivion-compat/sewer-regression/focused-closure-01/RESULTS.md).
One final integration attempt remains to be made after the artifact commit; no
full attempt has been launched since the revised closure request.

The correction preserves original approach points and uses ordinary attack,
block, heal and movement. Patrol/guard kills are Player kills; the eastern goblin
may die to actual shaman friendly fire. Do not relabel that killer. Historical
continuous 21 failed41; [failure evidence](SEWER-CONTINUOUS-21-FAILURE.json).

Known gameplay/presentation defects: upright creature corpses, stale/T-pose
inventory preview, take-all-only loot, blank birthsign descriptions, held torch
missing from perception light, Player Flare/general NPC spell fallback missing,
and rough exterior terrain/water/lighting. No-sound courses do not prove audio
pacing. These are separate from controller navigation, combat sequencing, menu
input and observation limits. A native Player death is a failed automated play;
a timeout needs diagnostic review before attribution.

Unchanged engine evidence (2,223 normal and 2,223 ASan/UBSan passes) retains its
original provenance in [floor recovery](SEWER-DESCENDING-RECOVERY-VERIFICATION.json).
Earlier runner/defense evidence is linked in [the command reference](SEWER-REGRESSION.md).
New focused checks total 22 Python cases; broad checks were not repeated for
manifest/Python/shell changes. [Historical handoff](SEWER-ESCAPE-CHECKPOINTS-2026-11-07.md).

## Next ambitious playable chunk — recommendation only

**Sewer dock → Vilverin expedition → return to the lakeshore alive with loot.**
Start from a genuine outdoor escape save; cross the lake, approach the ruin/camp,
explore its connected interiors, fight inhabitants, handle traps and collect
loot. End back outside with that character and carried loot, demonstrating fresh
dungeon and outdoor save/reload. Resolve only blockers encountered along this
expedition, including exterior presentation needed to navigate. This gives the
player an independent adventure beyond the tutorial. Do not start it as part of
sewer closure.
