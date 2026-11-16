# Sanctum escort and corpse movement collision

Current chunk: prevent the observed Sanctum escort deadlock and make dead actors
passable while retaining their ragdolls and interaction. This is a bounded repair,
not a new claim that the whole tutorial/escape course is accepted. Historical
course evidence remains in [the sewer checkpoint handoff](SEWER-ESCAPE-CHECKPOINTS-2026-11-07.md).

## Observed failure and repair

The actual player save was in Sanctum (022282), CharacterGen 54. Glenroy 032a16
was 152.526 units from Emperor 032a18, just outside his escort package01d0ed's
150-unit waiting range, and 97.647 units from his own destination. The Emperor's
Travel 00bf1e had already completed. Glenroy therefore waited indefinitely and
never emitted the original PackageDone callback that sets CharacterGen 56.

Completed Travel recipients now receive the same temporary catch-up movement
as CurrentLocation Travel recipients while their escort remains active. Their
completed phase, authored destination and completion ownership remain intact.
Stalled/interrupted escorts still hold the recipient; completed escorts no
longer supply catch-up or proactive-yield ownership. No original quest, package,
or save data was edited.

Actor movement sweeps now ignore dead actors' capsules and ragdoll limbs. A
main-thread projection of CreatureStats.isDead supplies an atomic movement-only
flag to physics workers. It is initialized on admission and refreshed before
movement, including suspended actors and existing physical poses. The physical
bodies, collision registration, dynamics, ray queries and saved state remain
owned by their existing systems. Living/paralyzed/knocked-down actors retain
movement collision; revival restores it. There is no save-schema change.

## Verification (native Windows, 2026-11-15)

- Affected suites: 309/309 tests, including real Bullet sweep obstruction/pass-through,
  retained ray targeting, unchanged pose/velocities, applied impulse, death,
  corpse re-admission and revival at 0/1/2 configured physics worker threads.
  Escort tests preserve completed Travel state and do not replay completion.
  Evidence: `build-windows/checks-c8e322bf85b24e1ea96ffe361b501993/`.
- Full engine: 2307/2307; full components: 3303/3303. Exact inventories/XML agree,
  with no failures/errors/skips and unchanged before/after source fingerprint.
  Evidence: `build-windows/checks-f3bd138b84854476816f9da5d944f218/`.
  Tested HEAD 2ec9ea2c573a8efdada3d739df9954271c2d0e6c plus dirty implementation;
  source fingerprint `c421a1a24d9eb5e7ff986007db4cea18a12c0cdd2d2806a1fe1ae846702d4f30`.
  This handoff was added after the implementation checks.
- Rebuilt game SHA256:
  `7935744809fa71a480710c842e78a72f403ee064d1ed824c2f41df5f440bb657`.
- Rendered before/after continuation uses private copies of the actual stalled
  save, SHA256 `8b79c8871e23a7757249e356f305bbaaf91cd0beccc298214393ac0eddb50482`.
  The original executable stays at 54. The rebuilt executable naturally emits
  Glenroy's completion and reaches 56, 58 and 59, with the Emperor visibly back
  in the conversation. Player position is unchanged; no movement, teleport,
  quest setter or actor-state injection is supplied. Read-only command samples
  capture the actual runtime state. The original player profile is untouched.
  Driver, editable recipe, sampled states and process results:
  `build-windows/sanctum-corpse-runtime-work/` (`baseline-03`, `fixed-01`).

Earlier failed sandbox/build and fixture attempts remain in the numbered
`build-windows/sanctum-corpse-*.log` files and runtime baseline directories.
The corpse traversal/interaction evidence is automated physics coverage, not a
new manual rendered corpse-looting course. No sanitizer configuration exists on
this Windows checkout; Linux sanitizer and a full tutorial replay were not run.

Remaining acceptance gaps: broader tutorial/escape courses retain their existing
status. Next action: use the rebuilt executable with the ordinary player save;
if another escort obstruction occurs, retain that save and inspect its specific
package/geometry relationship before expanding this repair.
