# M11 actor appearance, skeletons, and animation

Status: accepted on 2026-08-22. Implementation revision
`d5966e5192cda3a9cee90d9b7fa45c4885c40dba`; head-part correction revisions
`9c1796ffe638e81b3b15287c3bb693c918d7a06a` and
`1498179f8825bb8ee64ccf0921d1367a52576ba0`.

M11 completes the native Oblivion visual actor path. Player and placed NPCs
are assembled from race, sex, head, eyes, hair, skin, and initial equipment
records; creatures use their native multipart skeleton families; and external
KF groups, facial morphs, voice-linked lip motion, death animations, and saved
XRGD poses execute through the normal OpenMW scene and animation runtime.

## Delivered implementation

- `RACE` and `NPC_` shape/texture coefficients now drive the official EGM and
  EGT FaceGen assets. TRI expression targets are retained as deformable scene
  geometry, with exact phoneme-channel matching and independent blinking.
  Race height/weight and sex-specific tables are applied without TES3 body
  records.
- Native eyes, hair, head/ear/mouth parts, naked-body slot meshes, and
  sex-specific armor/clothing models attach to the authoritative destination
  skeleton. Biped coverage suppresses hidden body, head, and hair pieces, and
  rigged attachments resolve skin influences against that skeleton instead of
  copying a competing source rig.
- FaceGen head parts cancel the Biped head bone's bind-axis rotation while
  retaining its translation and later animation. Per-actor geometry is made
  private before texture or morph work, preventing cached meshes from
  accumulating another NPC's deformation. Eye EGM data accepts the official
  base-vertex prefix followed by its four statistical eye-look targets. The
  mouth cavity, upper/lower teeth, and tongue retain their native Biped
  head-bone orientation instead of receiving the actor-aligned eye/hair
  correction.
- The projected player uses the official `_1stperson` skeleton and KF directory
  in first person, and the normal or beast skeleton family in third person.
  Both paths assemble native Oblivion visual records rather than retaining the
  earlier magenta actor placeholder.
- Actor and creature KF files are registered by directory and loaded lazily by
  gameplay group. Oblivion names are mapped to OpenMW's movement groups, each
  KF filename is authoritative over its internal sequence label, and linear,
  compact, and cubic uniform B-spline transform tracks are evaluated with
  unset Bethesda components excluded.
- Native `CREA` records use animated multipart creature assembly. Every shipped
  creature skeleton directory is inventoried with its available idle, movement,
  attack, and death groups.
- The voice runtime exposes the actor's real VFS voice path, playback offset,
  and decoded-stream loudness. A matching official legacy LIP envelope enables
  playback-clock-synchronised blends across the exact 16 FaceGen speech TRI
  channels; invalid or absent companions do not animate speech. The opaque
  legacy FaceFX archive is retained intact and alternate official compression
  headers are accepted instead of being mistaken for a fixed frame header.
- Placed `ACHR` and `ACRE` XRGD blocks are parsed with finite-value validation,
  unavailable Havok sentinels are skipped, and the saved root plus 18 mapped
  biped transforms are applied after scene updates. A saved corpse suppresses
  KF callbacks that would overwrite its final pose. XRGB metadata is retained.
- Native NPC and creature objects are exposed to actor lists and Lua type
  checks, allowing real actor/creature runtime scenarios and script-driven
  voice playback without a test-only visual object.

## Official-content audit

The accepted audit enumerates the combined VFS and the actor-facing records in
all eleven official plugins. The base master is `Oblivion.esm` SHA-256
`a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`;
all plugin hashes and sizes are locked in
`scripts/data/oblivion_compat/oblivion_m11_asset_counts.json`.

| Audit item | Count |
| --- | ---: |
| Official archives / merged VFS entries | 17 / 147,535 |
| Race records / playable races | 15 / 10 |
| Race/sex asset references / missing | 364 / 0 |
| Armor and clothing records | 1,749 |
| Equipment model references / missing | 2,593 / 0 |
| Equipment sex-model gaps after valid fallback | 0 |
| Actor skeletons / KF groups | 4 / 860 |
| Creature skeleton families / KF groups | 43 / 1,467 |
| EGM / EGT / TRI assets | 99 / 20 / 86 |
| Paired official voice/LIP stems | 50,909 |
| Unpaired audio / LIP assets, count-locked | 33 / 6 |

All required first-person, humanoid, and beast skeletons exist; all creature
families have animation groups; all FaceGen companion relationships resolve;
and the populated Oblivion biped slots are present in the sex/model matrix.
The complete count-locked report is
`build/oblivion-compat/m11-assets-acceptance-20260822/m11-assets.json`.

Reproduce it with:

```sh
python3 scripts/oblivion_compat.py m11-assets \
  --source "$PWD" \
  --build "$PWD/build" \
  --oblivion-data "/path/to/Oblivion/Data" \
  --output build/oblivion-compat/m11-assets \
  --count-lock scripts/data/oblivion_compat/oblivion_m11_asset_counts.json
```

## Automated and end-to-end verification

The completed revision passed:

| Suite | Result |
| --- | ---: |
| Component tests | 1,524 / 1,524 |
| Engine tests | 505 / 505 |
| Python compatibility-harness tests | 23 / 23 |
| Focused FaceGen/ragdoll/NIF controller-loader tests | 15 / 15 |
| Focused facial-channel/cell-reference tests | 3 / 3 |
| Full official NIF-family parse scan | 12,069 / 12,069 |
| Full official NIF scene-graph construction scan | 9,612 / 9,612 |
| M11 runtime manifests | 4 / 4 |
| Morrowind runtime regression | 1 / 1 |

The head-part correction follow-ups additionally passed 7 / 7 focused FaceGen
regressions, 510 / 510 engine tests, and 1,524 / 1,524 component tests. Their
dedicated Arena holding-cell runtime loads 26 released NPCs across all ten
playable races, requires assembled FaceGen meshes and active idle controllers
for every race, and rejects every FaceGen, attachment, transform, frame,
assertion, and crash diagnostic. The player transition suite also passes
first/third person, idle/walk/run, quicksave, and reload with the same policy.

`niftest` reported zero failed files in both the parse and scene-graph passes.
The scene pass intentionally has no loose texture VFS and can report texture
lookups, but no mesh, skeleton, skin, controller, or graph construction failed.

The runtime evidence uses real released records and assets:

- `build/oblivion-compat/m11-runtime-player-accepted-20260822/` switches the
  player between first and third person, plays idle, walk, and run groups, and
  verifies separate 66-controller first-person and 64-controller third-person
  rigs. It quicksaves, reloads in a new animation state, and retains native
  `T4ST` state.
- `build/oblivion-compat/m11-runtime-actor-final-20260822/` renders Clesa, a
  female Redguard, with 13 assembled parts and eight FaceGen morph meshes. It
  plays a 64-controller idle and an official Redguard voice/LIP pair, captures
  a 9.1 MB waveform, and records a 0.0300 normalized RMSE between speech frames.
- `build/oblivion-compat/m11-runtime-creature-accepted-20260822/` renders seven
  multipart Goblins, loads the family idle group with 51 bound controllers,
  captures motion in consecutive frames, and verifies quicksave/reload.
- `build/oblivion-compat/m11-runtime-ragdoll-accepted2-20260822/` renders a real
  placed Breton corpse with 18 applied saved XRGD bone transforms and verifies
  that the pose survives quicksave/reload.
- `build/oblivion-compat/m11-morrowind-regression-20260822/` exercises a normal
  Morrowind exterior, animation/render path, weather, audio waveform, and save
  after the shared rig-attachment change.
- `build/oblivion-compat/m11-head-parts-final-accepted-20260822/` renders the
  26-NPC, ten-race Arena gallery in consecutive animated frames after the bind
  and cache-isolation correction. The corrected voiced Clesa close-up is in
  `build/oblivion-compat/m11-head-parts-voice-final-20260822/`.
- `build/oblivion-compat/m11-oral-parts-final-20260823/` captures Clesa at idle
  and during official voice/LIP playback with the mouth, teeth, and tongue
  seated inside the lips while the corrected eyes and hair remain aligned.
  `m11-oral-parts-all-races-20260823/` and
  `m11-oral-parts-player-20260823/` cover the ten-race and player paths.

The player, actor, creature, corpse, and post-reload captures were inspected
directly. They show coherent multipart bodies with heads, eyes, hair/equipment,
skinned limb transforms, animated pose changes, and a stable articulated corpse;
no missing actor part, invalid transform, animation deadlock, frame error,
assertion, or crash matched the M11 gates. Existing missing TES3 UI textures and
M9 navigation-shape warnings in these early full-game slices are outside the
actor renderer and are not hidden by the M11 assertions. No original game
executable, Wine, or Proton was used.

## Interactive check

After running the player scenario once, open its generated config directly:

```sh
build/openmw --replace=config \
  --config build/oblivion-compat/m11-runtime-player-accepted-20260822/config
```

Press `Tab` to switch first/third person, use `W` and `Shift+W` for walk/run,
and use `F5`/`F9` to check save/load. Substitute the `config` directory from the
actor, creature, or ragdoll evidence above to inspect those real records. The
actor manifest can also be recreated from
`scripts/data/oblivion_compat/oblivion_m11_actor_voice_gallery.json`; unlike the
sound-disabled visual cases, keep audio enabled to hear and inspect the voice
path. To inspect many head variants at once, run the checked-in
`oblivion_m11_head_part_gallery.json` scenario and open its generated config:

```sh
build/openmw --replace=config \
  --config build/oblivion-compat/m11-head-parts-final-accepted-20260822/config \
  --no-sound=1
```

## Scope boundary

M11 owns actor visual assembly, skeleton selection, skinning, animation groups,
facial/lip motion, creature rendering, death groups, and persisted official
XRGD poses. M12 owns player statistics, character creation, controls, movement,
and camera semantics; M13 owns mutable inventory and equip/unequip state; M14
owns navigation and actor AI; and M15 owns combat and newly simulated dynamic
death/ragdoll physics. Those later gameplay systems are not used as substitutes
for the M11 visual and animation acceptance gates.
