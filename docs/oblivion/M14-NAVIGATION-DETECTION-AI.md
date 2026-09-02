# M14 navigation, detection, and AI packages

Status: accepted on 2026-09-01. Implementation revision is the working tree
based on `a9acf26cc1e94c97c6ef4c0c53c880b08f82845f`.

M14 promotes the native TES4 navigation and AI data that was previously parsed
but discarded into a complete profile-owned runtime. Oblivion NPCs and
creatures now share the existing M11 render/animation and M12/M13 actor state,
while a stable-keyed coordinator owns package selection, schedule evaluation,
path execution, process tiers, detection, doors, companions, and horses.

## Delivered implementation

- `PACK` and `PGRD` are native stable-keyed stores. `CTDA` and TES4 `CTDT`
  conditions share one typed representation; `PTDT` location/target typing and
  FormID adjustment are corrected. Pathgrid point bytes are preserved, `PGAG`
  is retained and audited, PGRR/PGRI/PGRL indices are validated, and pathgrid
  ownership comes from winning cell metadata rather than file order.
- `ESM4::PathgridService` exposes deterministic nearest-node, component, route,
  foreign-link, object-link, overlay, generation, and navigator-adapter APIs.
  Interior/local and exterior/world-space coordinates are handled at the
  adapter boundary, with no fake TES3 pathgrid inserted into the store.
- TES4 NPC and creature classes expose native actor stats, movement, collision,
  inventory/equipment, factions, capabilities, and stable runtime identity.
  The `OblivionAiService` is the sole package/phase authority; TES4 actors do
  not create a concurrent TES3 `AiSequence`.
- Ordered schedule/condition selection, transient script packages, fixed-step
  high processing, deterministic low processing, route recovery, restraint,
  and version-5 persistence are implemented. Low-process movement retains
  waypoints and crosses only resolved door edges; it never substitutes a final
  destination teleport.
- Wander, Travel, Find, Follow, Accompany, Escort, Eat, Sleep, Use Item At,
  Flee Not Combat, Pursue, dialogue approach, companion grouping, and horse
  mount/dismount state are implemented. The five observed `CastMagic` records
  stop at the explicit `waiting-for-M16-action` boundary, and `Ambush` stops at
  `ready-for-M15-combat`; neither reports unavailable combat or magic effects
  as completed.
- M14 ObScript commands and queries use the same native service: package
  evaluation and insertion, current package/procedure, path-point overlays,
  LOS/detection, flee, restraint, and horse state. Invalid native targets are
  typed runtime errors, not silent no-ops or deferred placeholders.
- The scenario harness provides actor observation, clock control, obstruction
  control, navigation debug capture, structured AI events, bounded progress
  checks, and forbidden-action checks. It does not move actors or force package
  success after initial setup.

## Official-content audit

The canonical profile is the eleven installed official files listed by
`m14-audit`: `Oblivion.esm`, the local 85-byte `DLCShiveringIsles.esp` stub,
and the nine installed DLC/Knights files. The count lock is
[`oblivion_m14_data_counts.json`](../../scripts/data/oblivion_compat/oblivion_m14_data_counts.json).
The accepted audit is
[`m14-audit.json`](../../build/oblivion-compat/m14-audit-final6/m14-audit.json)
with the inspectable HTML report beside it. The lossless parser census is
[`baseline.json`](../../build/oblivion-compat/m14-lossless-final3/baseline.json):
11/11 installed plugins and 17/17 archives were enumerated, with zero
unsupported record families and zero unallowlisted semantic skips.

| Audit value | Result |
| --- | ---: |
| Winning records | 1,187,866 |
| Winning `PACK` / `PGRD` / `CELL` | 7,668 / 8,288 / 35,565 |
| Actor records / unresolved package lists | 3,636 / 0 |
| Conditions / invalid references | 6,830 / 0 |
| Pathgrid nodes | 675,161 |
| Local / foreign pathgrid edges | 2,915,191 / 291,637 |
| Resolved `PGRL` links / linked nodes | 510 / 1,577 |
| Linked base types | `ACTI` 405, `DOOR` 99, `STAT` 6 |
| PGAG bytes | 88,046 |
| Ambiguous pathgrid cells | 0 |

The package fingerprint is
`sha256:04b4e35cd39d55e277084afd18b06bb70477f9a7a36c12b654201d69cec481c9`;
the pathgrid fingerprint is
`sha256:19780f08b7491950df0eb6f4b32e7b91634d636cc3a440986926a354f2d45883`.
Observed package types are Accompany 40, Ambush 90, CastMagic 5, Eat 855,
Escort 75, Find 768, FleeNotCombat 11, Follow 230, Sleep 762, Travel 2,132,
UseItemAt 819, and Wander 1,881. The 180 actors without packages are explicit
no-package records, not unresolved package references. The 27,277 cells without
a usable graph are report-only cells with no usable pathgrid; no winning cell
has an ambiguous graph.

Three reserved package flag bits (`0x00000800`, `0x00004000`, and
`0x00008000`) and one stock out-of-range Find target are retained in the audit
as reviewed data, with no unreviewed fallback. All reachable condition
functions and run-on contexts are supported (`0:Subject` accounts for all
6,830 audited conditions).

## Verification

The final native build passed:

```sh
cmake --build build --target esmtool components-tests openmw-tests openmw -j2
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s scripts/tests -p 'test_*.py'
git diff --check
```

| Gate | Result |
| --- | ---: |
| Component GoogleTest inventory | 1,580 / 1,580 |
| Engine GoogleTest inventory | 516 / 516 |
| Python compatibility/state tests | 40 / 40 |
| Focused M14 component tests | 50 / 50 |
| Focused M14 engine tests | 5 / 5 |
| Full CTest targets | 2 / 2 |
| Sanitized component suite | 1,580 / 1,580 |
| Sanitized engine suite | 516 / 516 |
| Count-lock audit | 1 / 1 |
| Lossless official-content census | 11 / 11 plugins; 17 / 17 archives |

The focused suites cover binary PACK/PGRD fixtures, malformed data, stable
overrides/deletions, schedules and conditions, detection vectors, pathgrid
graphs/foreign links/object links/overlays, phase machines, persistence
migration/validation, actor bridges, process tiers, doors, companions, and
mount reciprocity. The Python verifier covers schema 5, v1--v4 migration,
corruption, content identity, chronology, routes, overlays, detection, and
mount/companion relationships.

## Real-content acceptance

All six Oblivion M14 scenarios and the Morrowind isolation scenario completed
with exit code zero, clean process logs, required captures, structured event
streams, and validated quicksaves:

| Scenario | Evidence |
| --- | --- |
| Tutorial escort | `build/oblivion-compat/m14-scenario-tutorial-final5/` |
| 24-hour Imperial City population | `build/oblivion-compat/m14-scenario-city-city6/` |
| Doors and obstruction recovery | `build/oblivion-compat/m14-scenario-doors-final3/` |
| Companion travel | `build/oblivion-compat/m14-scenario-companion-final3/` |
| Detection matrix | `build/oblivion-compat/m14-scenario-detection-final5/` |
| Horses | `build/oblivion-compat/m14-scenario-horse-final21/` |
| Morrowind isolation | `build/oblivion-compat/morrowind-m14-final3/` |

The city replay covers five calendar checkpoints and named NPC package
histories. The door and companion courses exercise resolved XTEL transitions,
locked/keyed permissions, obstruction recovery, and save/reload. Detection
records exact script and native-service probes. The horse course records one
mount and one dismount and verifies the rider relationship. Historical M7,
M12, and M13 scenarios were rerun after the actor/process integration: the two
M7 reload/runtime runs, five M12 courses, and two M13 matrices all passed.

Visual review covered the tutorial interior, city, door course, horse mount,
detection scene, companion scene, and Morrowind frame. Actors, world placement,
door/terrain geometry, horse attachment, and post-process scene stability show
no M14 regression to the accepted M11--M13 visuals. The existing magenta
fallback HUD/resource markers visible in these captures are also present in
the accepted M13 captures and are not produced by the M14 systems.

## Boundaries and known limitations

M15 still owns combat, damage, crime, arrest, and mounted combat. M16 owns real
magic-effect execution; observed CastMagic packages expose the typed boundary
instead of faking a cast. M18 owns dialogue content/topic selection, and M19
owns the complete Oblivion UI. The local Shivering Isles file is an 85-byte
stub, so this acceptance is for the installed canonical profile and does not
claim coverage of expansion data that is not present.

## Reproduce

Build and run the audit:

```sh
python3 scripts/oblivion_compat.py m14-audit \
  --oblivion-data "/path/to/Oblivion/Data" \
  --output build/oblivion-compat/m14-audit \
  --count-lock scripts/data/oblivion_compat/oblivion_m14_data_counts.json
```

Run a real scenario with the native OpenMW binary:

```sh
python3 scripts/oblivion_compat.py scenario \
  scripts/data/oblivion_compat/oblivion_m14_city_schedule.json \
  --output build/oblivion-compat/m14-city \
  --variable "source=$PWD" \
  --variable "openmw=$PWD/build/openmw" \
  --variable "resources=$PWD/build/resources" \
  --variable "oblivion_data=/path/to/Oblivion/Data"
```

Validate a generated quicksave independently:

```sh
python3 scripts/oblivion_compat.py runtime-state m14-verify SAVE \
  --report build/oblivion-compat/m14-city/m14-state-verification.json
```
