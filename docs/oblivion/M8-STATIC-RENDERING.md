# M8 static rendering, animation, and collision

Status: accepted on 2026-08-13. Implementation revision
`f965de60d5c53af83057ff1372c8e4141868a3aa`.

M8 completes the Oblivion static/item NIF path: Bethesda materials and lights,
embedded object animation, modern particles, billboards, and the Havok collision
tree all produce native OSG/Bullet objects. The implementation is generic NIF
handling and contains no cell-, quest-, or reference-specific rendering behavior.

## Delivered implementation

- `BulletNifLoader` now traverses `bhkCollisionObject`, rigid body/phantom
  transforms, MOPP, packed/strip/mesh geometry, convex vertices, boxes, capsules,
  multi-spheres, spheres, lists, convex lists, transforms, and swept transforms.
  Oblivion and later Bethesda Havok units are converted at their format-specific
  scales. Animated collision mappings remain connected to the rendered node.
- Embedded `NiControllerSequence` groups are multiplexed onto a stable synthetic
  timeline. Transform, visibility, alpha, material-color, flip, and UV-transform
  controllers participate in `PlayGroup`, including overlapping tracks and text
  keys used by doors and traps.
- `NiParticleSystem` supports box/cylinder/sphere/mesh emission; initial state,
  quotas and bounds; speed, angle, lifetime, size and rotation shooting; grow/fade,
  color, gravity, drag, bomb, planar/spherical collision, spawn generations, and
  controller-driven properties. Local- and world-space effects retain the correct
  reference frame.
- Oblivion center-facing billboard modes and embedded directional/ambient NIF
  lights now reach the render graph and shader lighting uniforms.
- Normal/parallax/glow textures, alpha and refraction use the native material
  path. Thirteen misspelled or pre-release texture names in released Oblivion NIFs
  are repaired only when the referenced file is absent; a replacement archive
  that supplies the original name always wins.
- `niftest` gained combined-VFS, scene-graph, Bethesda-collision, strict, quiet,
  repeated-archive, and input-list modes. `esmtool` TES4 type filters now produce
  a real record-specific asset inventory rather than unfiltered group output.

## Official-content audit

All eleven official plugins were queried for static, movable-static, activator,
door, container, flora, furniture, book, clutter, alchemy, apparatus, ingredient,
key, light, soul-gem, sigil-stone, weapon, ammunition, scroll, armor, and clothing
models. The merged official VFS contains 8,119 distinct referenced models; all
8,119 built OSG scene graphs, together with 9,359 Bethesda collision objects:

| Result | Count |
| --- | ---: |
| Existing official static/item models | 8,119 |
| OSG scene graphs built | 8,119 |
| Bethesda collision objects built | 9,359 |
| Failed files | 0 |
| Unsupported collision objects/shapes | 0 / 0 |
| Missing model/material textures | 0 |

Sixty-four plugin records refer to files absent from the merged official archives;
these are mostly shipped prototype/test/deleted records and are recorded separately
from loader failures. A combined archive scan also parsed every supported NIF/KF
entry and built 9,600 NIF scene graphs and 10,393 collision objects with zero
failure or unsupported collision shape. Three path-interpolated butterfly/moth
assets were deliberately assigned to M9 foliage and are not used as evidence for
M8 completion.

Durable batch evidence is in:

- `build/oblivion-compat/m8-audit/official-existing-static-item-current.stdout`
- `build/oblivion-compat/m8-audit/official-existing-static-item-current.stderr`
- `build/oblivion-compat/m8-audit/official-existing-static-item-models.txt`
- `build/oblivion-compat/m8-audit/official-missing-static-item-models.txt`
- `build/oblivion-compat/m8-audit/render-collision-complete.stdout`

## Automated and runtime verification

The completed tree passed 1,511/1,511 component tests, 495/495 engine tests, and
22/22 Python compatibility/state tests. Sixty focused loader tests exercise the
new collision shapes, sequence multiplexing, billboards, particle shooting,
drag, death-spawn, planar collision, bombs, and official texture aliases.

Four deterministic OpenMW runtime galleries boot real official cells with forced
shaders and reject missing static-material families, shader failures, frame errors,
assertions, and crashes:

| Scene | Result | Capture |
| --- | --- | --- |
| Imperial prison/clutter | pass | `m8-runtime-prison-final/prison.png` |
| Vilverin Ayleid ruin | pass | `m8-runtime-ruin-final/ayleid-ruin.png` |
| Goblin Jim's Cave | pass | `m8-runtime-cave-final/cave.png` |
| Tutorial cave/trap set | pass | `m8-runtime-trap-final/tutorial-cave-and-traps.png` |

The captures were visually inspected for geometry, surface materials, normal-map
lighting, alpha foliage/detail, and depth ordering. The prison scenario also writes
a valid native save. A fifth route walks into the closed tutorial wall and confirms
the player remains blocked by the newly interpreted Bethesda collision tree;
evidence is below `build/oblivion-compat/m8-collision-final/`.

The magenta HUD, player-marker, and crosshair placeholders visible in these captures
belong to later UI/player milestones and are not geometry or material fallbacks in
the tested NIFs.
