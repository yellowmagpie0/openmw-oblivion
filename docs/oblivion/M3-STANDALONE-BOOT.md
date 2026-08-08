# M3 Oblivion game profile and standalone boot

Status: accepted on 2026-08-08 at implementation revision
`ab9c38569018fde2658cfb27daa5007146754577`.

## Delivered source

- Startup preflights the configured ESM/ESP formats when the profile is
  `auto`, selects Oblivion resources before VFS construction, and reports a
  precise error if an explicit profile rejects the content.
- Oblivion's six base archives are profile-owned defaults. They are discovered
  from the configured data directories when no explicit `fallback-archive`
  list is supplied. Auto and Morrowind profiles receive no Oblivion defaults.
- Native TES4 `GMST`, `GLOB`, and `CLAS` records now have typed runtime stores
  alongside the existing TES4 player and race records.
- `OblivionProfileServices` imports native GMST values, native globals, and
  the actual `Player`, race, and class records. It projects those sources into
  the narrow shared runtime contracts still consumed by OpenMW's player,
  mechanics, calendar, animation, and UI bootstrap systems.
- Calendar aliases map Oblivion's `GameDay`, `GameMonth`, `GameYear`,
  `GameHour`, `GameDaysPassed`, and `TimeScale` values to the shared clock
  names. Native values win over defaults.
- The old demand-created neutral GameSetting fallback was removed. The
  remaining shared boot settings are a fixed, reviewed allowlist; an unknown
  lookup fails normally and must be deliberately added after inspection.
  Positive skill-progression factors are explicit, preventing a formerly
  repeated per-frame mechanics error.
- The runtime player projection takes the native player name, skeleton,
  level, health, fatigue, attributes, 21 Oblivion skills, race attributes,
  and class/race names from `Oblivion.esm`. It no longer uses the former
  constant neutral prisoner record.
- A Morrowind-only global `main` script starts only when that script exists.
  Oblivion therefore has no synthetic script and no missing-global-script
  startup error; Morrowind behavior is unchanged.
- Interior and exterior manifests now contain only the shared resource tree,
  the original Oblivion data directory, and `Oblivion.esm`. They do not name
  a Morrowind master, `vfs-mw`, or any BSA.
- `m3-acceptance` packages all focused tests, both Oblivion visual boots,
  wrong-profile diagnostics, the Morrowind visual save/load campaign, content
  fingerprints, and the 14-test Morrowind integration suite into one JSON and
  HTML result.

## Reproducible acceptance

The complete gate is:

```sh
python3 scripts/oblivion_compat.py m3-acceptance \
  --build build \
  --oblivion-data "/path/to/Oblivion/Data" \
  --morrowind-data "/path/to/Morrowind/Data Files" \
  --output build/oblivion-compat/m3-acceptance
```

The accepted local evidence is
`build/oblivion-compat/m3-acceptance-final/acceptance.json` and
`acceptance.html`. It passed in 118.69 seconds with these original masters:

| Content | Bytes | SHA-256 |
| --- | ---: | --- |
| `Oblivion.esm` | 277,504,985 | `a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70` |
| `Morrowind.esm` | 79,837,557 | `5c3c8c2cbd20e25901b59b3ece33d36b7ef0e3d60ad8d11828bcc61a5ead1647` |

The gate results were:

| Check | Result | Acceptance detail |
| --- | --- | --- |
| Profile unit suite | Pass, 5/5 | aliases, auto-detection, mismatch, mixed formats, archive ownership |
| Native service unit suite | Pass, 1/1 | source values, clock alias, player/race/class, no catch-all |
| Harness unit suite | Pass, 13/13 | scenario, image, file, graph, and M3 aggregation checks |
| Oblivion interior | Pass | `ImperialDungeon01`, auto profile, six automatic archives, no forbidden diagnostics |
| Oblivion exterior | Pass | `OldBridgeExterior`, auto profile, no forbidden diagnostics |
| Wrong profile | Pass | exit 1 with the complete Morrowind-versus-Oblivion diagnostic in 0.54 s |
| Morrowind visual save/load | Pass | auto profile, normal F5/F9 input, historical save format, two valid frames |
| Morrowind integration | Pass, 14/14 | no failed or incomplete tests |

Both Oblivion runtime logs report the native services and contain no neutral
fallback, missing GameSetting, missing global script, `Error in frame`, Lua
`onFrame` failure, assertion, abort, or crash diagnostic. The interior run
loaded 382 native GMSTs, 94 native globals, and the native player, Imperial
race, and chargen class records from `Oblivion.esm`.

The final build/regression pass also completed every build target, including
OpenCS, and passed all 1,481 component tests and all 495 OpenMW tests. CMake
registered no separate CTest entries in this build. The M2 official-content
regression at `build/oblivion-compat/m3-m2-graph-regression-final` passed all
11 plugins: 1,188,447 keys and 2,949,394 references were stable across binary
restart and load-index reorder, with zero cycles, stale rules, or unreviewed
edges.

## Visual inspection

The exact acceptance captures were inspected directly at 1280x720 in addition
to the automated dimension, entropy, and luminance checks:

| Capture | Entropy | Mean | Direct review |
| --- | ---: | ---: | --- |
| Oblivion prison | 0.641115 | 0.152763 | Lit, textured prison arches, walls, floor, and distant corridor are present at player height. |
| Oblivion exterior | 0.751084 | 0.117628 | Textured terrain and a large, coherent stone ruin are visible at player height. |
| Morrowind boot | 0.845387 | 0.351888 | Textured exterior, sky, crosshair, status bars, weapon, and minimap are present. |
| Morrowind reload | 0.835363 | 0.320827 | The same world/UI view remains intact after F9 reload. |

The Morrowind campaign created one 176,622-byte quicksave through normal input
and then loaded it. The file contained none of the Oblivion-only `GPRO`,
`T4VR`, `T4ST`, or `OMW4STATE` markers.

Oblivion's magenta HUD placeholders and black exterior sky are intentionally
visible in these captures. They are known missing UI and weather presentation
assigned to later milestones; M3 establishes standalone profile ownership,
boot, cell entry, and non-empty rendering, not a claim of M5/UI completion.

## Acceptance decision

M3's bounded success criteria are all satisfied: the executable enters the
named TES4 interior and exterior using only original Oblivion content and
shared resources; archives, settings, globals, calendar, and the player are
owned by the Oblivion profile; auto-detection and mismatch diagnostics work;
the logs are healthy; and Morrowind still auto-detects, boots, saves, reloads,
renders, and passes its integration suite. M3 has no open gates.
