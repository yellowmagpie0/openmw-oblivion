# M9 exterior worldspaces, terrain, LOD, and vegetation

Status: accepted on 2026-08-13. Implementation revision
`fc5c57567662686119f3216007392eeed5e6e701`.

M9 completes the Oblivion exterior-world path. Native LAND, worldspace feature
inheritance, water levels, distant references, object paging, vegetation, and
global-map data now participate in the normal OpenMW runtime without a
cell-specific compatibility layer.

## Delivered implementation

- Worldspace inheritance is resolved independently for LAND, LOD, water, map,
  climate, and sky features. Parent chains are cycle-safe and missing parents
  fall back deterministically. Child worldspaces can therefore inherit only the
  features selected by their `WNAM`/`PNAM` flags.
- Exterior terrain and water use the inherited feature owner. Native LAND keeps
  its 4096-unit cell scale, height/color/normal data, four-quadrant base textures,
  and ordered alpha-blended texture layers across cell edges.
- Native distant references are collected from the inherited LOD worldspace.
  Paged meshes retain transform-facing billboard behavior, resolving the
  foliage/paging limitation represented by OpenMW issue 6388 instead of baking
  camera-facing transforms into a static orientation.
- Oblivion GRAS is generated deterministically from each LAND texture blend and
  the LTEX-to-GRAS relationship. Record density, placement jitter, slope limits,
  water-distance policy, terrain-normal fitting, random heading, and height
  variation are honored. This path is enabled for TES4 worlds even when the
  legacy Morrowind groundcover-plugin switch is off; `NoGrass` and `NoLandscape`
  world flags still take precedence.
- The native global map derives its bounds, cell scale, and terrain image from
  the dominant/Tamriel LAND worldspace. Visible, enabled map-marker references
  are projected using inherited map coordinates and retain their official label.
- ROAD and GRAS are live stores rather than discarded parse-only records. Native
  reference marker flags are parsed and initialized safely.

## Official-content and automated verification

The released base master contains 84 worldspaces, 108 grass definitions, and two
ROAD records; all are accepted by the typed reader. Focused parser tests cover
native map-marker flags, while worldspace tests cover per-feature inheritance,
missing parents, and cycles.

The completed tree passed:

| Suite | Result |
| --- | ---: |
| Component tests | 1,512 / 1,512 |
| Engine tests | 497 / 497 |
| Python compatibility tests | 19 / 19 |
| M9 deterministic runtime scenarios | 2 / 2 |

Both runtime scenarios reject frame/script errors, assertions, crashes, shader
failures, and missing official landscape/plant/tree textures.

## Runtime and visual verification

Deterministic real-data routes were exercised at the Imperial sewer exit, Weye,
Leyawiin, and Old Bridge. These cover waterfront, temperate forest, southern
lowland, road/bridge, ruin, and long-distance paged-object scenes. The accepted
Old Bridge route walks and turns across streamed terrain, records native
groundcover population, captures before/after paging frames, and writes a
1.3 MiB native quicksave:

- `build/oblivion-compat/m9-runtime-acceptance/oldbridge-initial.png`
- `build/oblivion-compat/m9-runtime-acceptance/oldbridge-paged.png`
- `build/oblivion-compat/m9-runtime-acceptance/scenario.json`

The captures were visually inspected for LAND continuity, blended ground,
terrain/tree depth ordering, grass placement, road/bridge alignment, and LOD
transitions. No holes, persistent cell-edge seams, incorrect world transforms,
or severe paging pop-in were observed. Additional biome evidence is retained in
the `m9-runtime-*-final` build directories.

A second accepted runtime pass opens the real global map in the Imperial sewer
exit world, verifies terrain-derived coverage plus eight initially visible
official markers in game state, and captures the rendered map:

- `build/oblivion-compat/m9-global-map-acceptance-final/global-map.png`
- `build/oblivion-compat/m9-global-map-acceptance-final/scenario.json`

The magenta legacy water and UI textures in that capture are not an M9 terrain
fallback; water appearance and environmental/UI media selection enter scope in
M10. No original executable was invoked for comparison, in accordance with the
explicit test constraint for this delivery.
