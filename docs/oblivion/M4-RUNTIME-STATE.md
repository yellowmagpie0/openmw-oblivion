# M4 TES4 runtime state and native saves

Status: implementation started on 2026-08-08; versioned save substrate and a
live player-state shadow record are delivered, full world-state round trip is
not yet accepted.

## Delivered source

- `components/esm4/runtimestate.*` defines native, versioned TES4 runtime
  state for content identities, dynamic allocation, clock, player, FormKey
  globals, enabled/deleted references, transforms, ownership, locks,
  inventories and extensible per-reference values.
- The deterministic little-endian binary codec uses stable FormKeys, bounded
  collections/strings/payloads, finite-number checks, duplicate rejection,
  exact input consumption and chunked ESM subrecords. Canonical JSON supports
  structural state comparison.
- `ESM::SavedGame` adds profile (`GPRO`) and TES4 runtime schema (`T4VR`) only
  for Oblivion saves. Historical and newly written Morrowind headers remain
  untagged and default to Morrowind.
- The state manager stamps the active profile, rejects cross-game loading with
  an explicit diagnostic, and routes the native `T4ST` record.
- Oblivion world saves now emit a `T4ST` shadow containing the content list,
  clock, stable player identity/cell, transform and primary actor values. The
  read path validates schema/profile/content and retains the decoded state.
- Tests cover all state families, deterministic bytes/JSON, records larger
  than one subrecord, plugin reorder/removal, corrupt/truncated/trailing input,
  wrong profiles/versions, invalid numbers and duplicate identity.

## Verification

```sh
cmake --build build --target components-tests openmw -j2
./build/components-tests --gtest_filter='SavedGameProfile.*:ESM4RuntimeState.*'

python3 scripts/oblivion_compat.py scenario \
  scripts/data/oblivion_compat/oblivion_save_smoke.json \
  --output build/oblivion-compat/m4-save \
  --variable "openmw=$PWD/build/openmw" \
  --variable "resources=$PWD/build/resources" \
  --variable "oblivion_data=/home/maciek/.local/share/Steam/steamapps/common/Oblivion/Data"

python3 scripts/oblivion_compat.py scenario \
  scripts/data/oblivion_compat/oblivion_load_smoke.json \
  --output build/oblivion-compat/m4-load-acceptance \
  --variable "openmw=$PWD/build/openmw" \
  --variable "resources=$PWD/build/resources" \
  --variable "oblivion_data=/home/maciek/.local/share/Steam/steamapps/common/Oblivion/Data" \
  --variable "savegame=$PWD/build/oblivion-compat/m4-save-acceptance/userdata/saves/Prisoner/Quicksave.omwsave"
```

The focused tests require exact value equality after binary and ESM-record
round trips, byte determinism on a second serialization, stable keys across
plugin reorder, precise missing-content diagnostics and rejection of every
malformed fixture. The live smoke must create a save through normal F5 input,
contain `GPRO`, `T4VR`, `T4ST` and the `OMW4STATE` payload magic, and produce
no save error. The accepted probe at
`build/oblivion-compat/m4-save-acceptance` created one 71,378-byte quicksave,
found every marker, passed log gates, and captured a non-black frame (entropy
0.742962, mean luminance 0.118987).

The saved-game capture was directly inspected at 1280x720. Prison geometry,
textures and lighting are coherent and visible; magenta HUD placeholders
remain the known pre-M5 UI-resource failure. Saving did not corrupt or blank
the rendered scene.

The separate process-restart probe at
`build/oblivion-compat/m4-load-acceptance` loaded that quicksave, logged the
expected profile/save/cell transitions, passed all forbidden-log gates and
captured the same coherent prison area (entropy 0.732011, mean luminance
0.107808). Direct inspection confirmed intact geometry and a plausible nearby
viewpoint. This proves the current envelope and shadow record are readable in
a new process; it does not imply that every native state family is applied yet.

The native runtime-state and saved-header suites pass 11/11 focused cases.
The complete component executable passed all 1,468 tests after the M0-M4
foundations were integrated.

## Open acceptance gates

1. Populate the native state from all live mutable TES4 references, globals,
   inventories, ownership and locks rather than only the current player shadow.
2. Apply decoded state back to the world after content instantiation.
3. Replace placeholder `content-list:v1` fingerprints with real content
   hashes shared with the baseline harness.
4. Add scenario-runner canonical JSON extraction and exact
   mutate/save/restart/reload comparisons in interior and exterior cells.
5. Exercise reorder, removal, corrupt-save and cross-profile diagnostics in
   full-process tests, then rerun the complete Morrowind save/load campaign.

M4 passes only after each runtime family round-trips through a process restart
and Morrowind save bytes/behavior retain their historical path. The present
code is the required persistence foundation, not that final claim.
