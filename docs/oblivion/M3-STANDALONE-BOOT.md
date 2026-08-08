# M3 Oblivion game profile and standalone boot

Status: implementation started on 2026-08-08; a visible direct-cell vertical
slice is running, with final acceptance gates listed below.

## Delivered source

- `components/esm/gameprofile.*` provides
  `auto|morrowind|oblivion`, format-driven selection, alias parsing, mixed-game
  rejection and explicit wrong-profile diagnostics.
- `--game-profile` is wired through CLI, engine, content loader and world.
  Auto selection occurs from loaded ESM format and the chosen profile is
  logged.
- Oblivion-only bootstrap records supply neutral globals, settings, 27 TES3
  adapter skills, race/class/player records, the minimal effect and UI values
  consumed by shared startup, and Oblivion skeleton/idle assets. Morrowind's
  strict lookup path is unchanged.
- The GameSetting adapter makes every remaining temporary TES3 compatibility
  fallback observable once by name and count instead of depending on
  `Morrowind.esm`.
- `oblivion_cell_smoke.json` and `oblivion_exterior_smoke.json` launch with
  only shared `resources/vfs`, the original Oblivion data, base archives and
  `Oblivion.esm`; neither loads `resources/vfs-mw` or a Morrowind master.

## Verification

```sh
./build/components-tests --gtest_filter='GameProfileTest.*'

python3 scripts/oblivion_compat.py scenario \
  scripts/data/oblivion_compat/oblivion_cell_smoke.json \
  --output build/oblivion-compat/m3-cell \
  --variable "openmw=$PWD/build/openmw" \
  --variable "resources=$PWD/build/resources" \
  --variable "oblivion_data=/home/maciek/.local/share/Steam/steamapps/common/Oblivion/Data"

python3 scripts/oblivion_compat.py scenario \
  scripts/data/oblivion_compat/oblivion_exterior_smoke.json \
  --output build/oblivion-compat/m3-exterior \
  --variable "openmw=$PWD/build/openmw" \
  --variable "resources=$PWD/build/resources" \
  --variable "oblivion_data=/home/maciek/.local/share/Steam/steamapps/common/Oblivion/Data"

python3 scripts/oblivion_compat.py scenario \
  scripts/data/oblivion_compat/oblivion_wrong_profile.json \
  --output build/oblivion-compat/m3-wrong-profile \
  --variable "openmw=$PWD/build/openmw" \
  --variable "resources=$PWD/build/resources" \
  --variable "oblivion_data=/home/maciek/.local/share/Steam/steamapps/common/Oblivion/Data"
```

The accepted interior checkpoint at
`build/oblivion-compat/m3-cell-accepted` logged profile auto-selection,
new-game startup and entry into `ImperialDungeon01`; its automated 1280x720
inspection passed with entropy 0.722350 and mean luminance 0.111388. The
accepted exterior checkpoint at
`build/oblivion-compat/m3-exterior-acceptance` likewise entered
`OldBridgeExterior` and passed with entropy 0.714026 and mean luminance
0.118849. Both reject the former black-frame baseline.

`build/oblivion-compat/m3-wrong-profile-accepted` also passed: the executable
returned 1 and emitted the complete Morrowind-versus-Oblivion mismatch in
0.64 seconds. The manifest sets `OPENMW_SUPPRESS_ERROR_DIALOG=1`, a test-only
switch added to keep expected fatal diagnostics non-modal; ordinary launches
retain the existing error dialog.
The focused `GameProfileTest` suite passes 4/4 cases, and the final official
content/Morrowind audit passes 11/11 plugins, 17/17 archives and 14/14
Morrowind integration tests.

## Direct visual inspection

The latest save-smoke interior capture was opened at original 1280x720
resolution. It visibly contains coherent Oblivion prison arches, columns,
walls, floor, gate, lighting and textures from a plausible player-height view;
it is neither black nor outside the scene. The HUD still resolves to magenta
missing-texture regions because the Oblivion UI resource layer does not yet
exist. That is a real failure assigned to M5/UI work; this checkpoint
establishes cell loading and rendering, not a visually accepted playable
prison.

The exterior capture was also opened at 1280x720. It contains textured,
continuous ground plus a large stone fort and scattered ruins at player
height. The sky is uniformly black and the HUD is magenta, so sky/weather and
UI remain visibly incomplete even though exterior cell, terrain and static
geometry loading are established.

## Open acceptance gates

- Retain fresh green interior and named exterior artifacts after bootstrap
  changes, including absence of repeated Lua startup errors.
- Expand the existing green 14-test Morrowind auto-detect regression into the
  deterministic visual new-game/save-load campaign specified by the universal
  contract.
- Replace the compatibility adapter with native Oblivion settings/player
  services as the actor/UI milestones land. Neutral values must never leak
  into Morrowind.

M3 is accepted only when both direct-cell scenarios, auto-detection,
wrong-profile diagnostics, visual non-black checks and the Morrowind boot gate
are all green.
