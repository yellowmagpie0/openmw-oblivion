# M4 TES4 runtime state and native saves

Status: accepted on 2026-08-08. Implementation revision:
`55e353e2fe9e8cd6a8dbe318f9191a191f665110`.

M4 adds a profile-owned, load-order-independent persistence path for mutable
Oblivion state. It is deliberately separate from the historical Morrowind save
path. The complete official-data acceptance campaign passed after exact
interior, exterior, and plugin-reorder process restarts, negative-load
diagnostics, automated and manual visual inspection, and Morrowind regression.

## Delivered source

### Versioned native state

- `components/esm4/runtimestate.*` owns schema version 1 and the `T4ST` save
  record. Its deterministic little-endian payload begins with `OMW4STATE` and
  represents content identities, the dynamic-form allocator, calendar and
  time scale, player state, FormKey globals, references, inventories, and
  extensible typed custom state.
- The codec uses stable content and dynamic `FormKey` values rather than raw
  load-order indices. Collections, strings, and total payload size are bounded;
  positions and numeric values must be finite; duplicate identities are
  rejected; malformed, truncated, trailing, wrong-profile, and wrong-version
  data fail explicitly.
- Oblivion saved-game headers contain the profile (`GPRO`) and TES4 runtime
  schema (`T4VR`). Untagged historical saves and new Morrowind saves retain the
  existing Morrowind interpretation and byte layout.
- Saved content is identified by normalized plugin name and a streaming SHA-256
  of the actual plugin bytes. Validation is independent of plugin order and
  reports the exact missing plugin or mismatched fingerprint.

### Live capture and restoration

`MWWorld::World` captures the native state immediately before save and applies
it after content and cells have been instantiated during load. Profile gating
keeps these operations out of Morrowind execution.

| State family | Capture and restore contract |
| --- | --- |
| Content | One real SHA-256 per loaded TES4 plugin; normalized and sorted independently of load order. |
| Time | Year, month, day, hour, time scale, and native calendar-global aliases are restored before time-manager setup. |
| Player | Stable native identity, FormKey cell, six-component transform, level, and base/modifier/current health, magicka, and fatigue. |
| Globals | Every native TES4 global is keyed by stable FormKey and restored through its declared numeric or string type. |
| References | Every loaded native TES4 reference is sorted by stable FormKey and retains base, cell, enabled/deleted state, transform, owner, lock difficulty, locked flag, count, scale, and record type. |
| Inventories | Player and reference inventories retain stable item FormKeys and signed counts in the native state layer. Gameplay-facing ESM4 containers remain a later M13 concern. |
| Dynamic forms | The next allocation serial and unprojected dynamic-reference state survive intervening saves without identity reuse. |
| Custom state | Typed boolean, integer, double, and string values round-trip deterministically for later profile slices. |

Restoration resolves globals, cells, bases, owners, and references against the
current `FormKeyResolver`; moves references between cell stores when needed;
then applies transforms, enablement/deletion, ownership, both parts of lock
state, count, and scale. Missing references, unresolvable keys, base mismatches,
and invalid typed values produce state-specific diagnostics.

The implementation also fixes two native-reference defects exposed by the
acceptance campaign:

- TES4 reference owner mutation now updates `XOWN`-backed state rather than
  silently doing nothing.
- References without `XLOC` now default to unlocked with lock level zero.
  Previously those two fields were uninitialized; the strict independent save
  decoder caught the invalid boolean representation in an official-data run.

### Independent acceptance tooling

- `scripts/tes4_runtime_state.py` is a strict independent `T4ST` decoder,
  encoder, canonicalizer, comparer, and deterministic mutation generator. It
  rewrites only `T4ST` and proves every unrelated save byte is preserved.
- `scripts/oblivion_compat.py runtime-state` exposes inspect, mutate, compare,
  corrupt, missing-content, and bad-fingerprint operations.
- `scripts/oblivion_compat.py m4-acceptance` owns the complete repeatable gate:
  focused tests, interior/exterior mutation and restart, DLC reorder, four
  failure modes, Morrowind visual save/load, Morrowind integration tests, and
  an aggregate JSON/HTML report.
- Scenario manifests under `scripts/data/oblivion_compat/` keep each process
  run bounded and independently reproducible.

## Verification command

The final accepted campaign was run from an empty output directory:

```sh
python3 scripts/oblivion_compat.py m4-acceptance \
  --source "$PWD" \
  --build "$PWD/build" \
  --oblivion-data /home/maciek/.local/share/Steam/steamapps/common/Oblivion/Data \
  --morrowind-data '/home/maciek/.local/share/Steam/steamapps/common/Morrowind/Data Files' \
  --output "$PWD/build/oblivion-compat/m4-acceptance-final-verified"
```

The aggregate evidence is:

- `build/oblivion-compat/m4-acceptance-final-verified/acceptance.json`
- `build/oblivion-compat/m4-acceptance-final-verified/acceptance.html`
- `build/oblivion-compat/m4-acceptance-final-verified/visual-review-montage.png`

It passed in 359.667936 seconds using the 277,504,985-byte official
`Oblivion.esm` with SHA-256
`a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`.

## Exact process-restart results

Each run saved normally, was mutated by the independent codec, loaded in a new
OpenMW process, resaved normally, decoded again, and compared structurally.

| Run | Globals | References | Reference inventories | Dynamic references | Expected and actual canonical SHA-256 |
| --- | ---: | ---: | ---: | ---: | --- |
| Interior | 94 | 456 | 21 | 1 | `9c3bbe4e0c57c15fd52845f6889b146fc14a85453ec9985b7eea3d6e17f680ac` |
| Exterior | 94 | 1,599 | 22 | 1 | `565c2d3abb7fe3c56e586b6501bcb14e205204e3f42315d11e92f27e8fce8720` |
| Plugin reorder | 95 | 456 | 21 | 1 | `ac6406ca47bc3456d588bad8fadb964c9624edad90c972c59bf8d9c23d94f41f` |

All three comparisons were exact. Mutations span content-independent dynamic
allocation, calendar/time, player transform and actor values, native globals,
enablement, deletion, transform, owner, inventory, count, scale, lock level,
locked/unlocked state, custom state, and a new dynamic reference. The interior
fixture proves both lock and unlock transitions; the exterior fixture contains
no initially locked native object and therefore proves the available unlock to
lock direction.

The reorder run first saved with `Oblivion.esm`, `Knights.esp`, and
`DLCMehrunesRazor.esp`, then swapped the two DLC plugin positions for load and
resave. Stable keys, content fingerprints, and canonical state remained exact.

## Failure diagnostics

Four separate full-process visual scenarios passed. Each reached the load UI,
emitted the required precise diagnostic, avoided the forbidden crash/error
patterns, and produced a nonblank diagnostic screenshot:

| Fixture | Required behavior |
| --- | --- |
| Missing plugin | Names the required normalized content file. |
| Changed bytes | Names `oblivion.esm` and reports saved/current fingerprint mismatch. |
| Corrupt `T4ST` | Rejects the malformed native runtime payload. |
| Cross profile | Refuses the Oblivion save under the Morrowind profile. |

The parser/codec suite additionally covers large chunked records, exact byte
determinism, plugin removal and reorder, truncation, bad magic, trailing input,
wrong schema/profile, zero dynamic serial, duplicate keys, invalid inventory,
non-finite values, published SHA-256 vectors, and stream restoration. The final
focused gate passed 13/13 cases; the Python harness passed 14/14 cases.

## Visual inspection

Automated image inspection passed every gameplay and diagnostic capture. Key
post-load measurements were:

| Capture | Entropy | Mean luminance |
| --- | ---: | ---: |
| Oblivion interior | 0.640445 | 0.149703 |
| Oblivion exterior | 0.729555 | 0.509953 |
| Oblivion plugin reorder | 0.656687 | 0.143453 |
| Morrowind after load | 0.848430 | 0.386350 |

The four post-load frames were also inspected manually at their source
resolution. Interior and reordered-plugin prison geometry, camera, textures,
and lighting are structurally consistent; the exterior retains coherent ruin,
terrain, and sky geometry; Morrowind retains a coherent normal rendered scene.
Known magenta Oblivion water/material/UI placeholders remain visible. Those are
unchanged M5+ rendering/UI work and are not state-loss artifacts.

## Backward compatibility and broad regression

- The live Morrowind visual scenario booted in Balmora, created a 168,315-byte
  quicksave, asserted that `GPRO`, `T4VR`, `T4ST`, and `OMW4STATE` were absent,
  loaded the quicksave with F9, and produced coherent before/after frames.
- The established Morrowind integration campaign started 14 tests and passed
  all 14 with zero failures, incomplete cases, crashes, or timeouts.
- The full M2 official FormKey graph regression passed with 1,188,447 keys,
  1,190,841 revisions, 2,949,394 references, no unreviewed edges, and stable
  restart/reorder fingerprint `fnv1a64:b1ea905be1c80ad0`.
- `cmake --build build --target all -j2` completed after the final native
  reference initialization change.
- The complete component suite passed 1,483/1,483 tests, including the direct
  native-reference default regression; the complete engine suite passed
  495/495 tests; and the rebuilt focused M4 gate passed 13/13.

## Success assessment

M4's deliver, verification, and success criteria are satisfied. Mutable native
TES4 state round-trips exactly through interior and exterior process restarts,
stable identities survive real plugin reorder, corrupt/incompatible inputs fail
with actionable diagnostics, visual state remains coherent, and Morrowind
continues to use and load its historical save path.

There are no open M4 acceptance gates. Interactive containers/equipment,
gameplay lock behavior, complete assets, and Oblivion-native UI are intentionally
owned by later bounded milestones; the state substrate required by those
milestones is now in place.
