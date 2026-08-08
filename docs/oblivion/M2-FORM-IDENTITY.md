# M2 form identity, masters, and overrides

Status: accepted on 2026-08-08.

Tested implementation revision:
`31360040f9732ff1ae324ff186fcc0724f585b15`.

## Delivered source

- `components/esm/formkey.*` defines normalized, load-order-independent
  content identity as `(defining plugin, 24-bit local ID)` and a separate
  namespaced 64-bit identity for runtime-created forms.
- `FormKeyResolver` converts runtime `FormId` values without persisting their
  load-order indices, resolves raw master/self references, handles missing
  plugins explicitly, and disambiguates TES4 ESP index-zero local forms from
  master overrides.
- `FormKeyIndex` retains ordered override history and authoritative winners,
  deletions, persistent/temporary/visible-distant child relationships,
  enable-parent edges and inversion, all inspected FormID references, cycle
  detection, and unresolved missing/deleted targets.
- The complete graph has a bounded, versioned `OMW4FKIX` binary codec and a
  canonical representation. Save/reload and runtime-index reorder comparisons
  operate on the full serialized graph rather than samples.
- `ESM4::Reader` records FormID-bearing typed and deferred subrecords, retains
  stable group/cell/world keys across context restoration, and correctly
  visits a plugin's final record. The FormID layout registry covers the
  official TES4 condition signatures and deferred record layouts used by the
  installed content. Its condition table was derived from xEdit's authoritative
  TES4 definitions:
  <https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.6/Core/wbDefinitionsTES4.pas>.
- Every typed TES4 runtime store is keyed by both stable `FormKey` and the
  current runtime ID. Static/dynamic insertion, copy, collision detection,
  override deletion, and cell secondary indices stay synchronized. TES4
  `REFR` and `ACHR` instances retain stable identity through `CellRef`; TES3
  identity remains unchanged.
- `esmtool graph report.json plugin [plugin...]` audits the complete official
  record/reference graph, writes deterministic binary restart state, repeats
  the scan with reversed runtime indices, and emits structured unresolved-edge
  and enable-parent-cycle JSON.
- `scripts/oblivion_compat.py form-graph` fixes the official plugin order,
  requires every unresolved edge to match an exact count-locked reviewed rule,
  and emits machine-readable JSON plus a human-readable HTML report.

## Official-content acceptance

The English Steam GOTY Deluxe content was scanned in this order:
`Oblivion.esm`, `DLCShiveringIsles.esp`, `DLCBattlehornCastle.esp`,
`DLCFrostcrag.esp`, `DLCHorseArmor.esp`, `DLCMehrunesRazor.esp`,
`DLCOrrery.esp`, `DLCSpellTomes.esp`, `DLCThievesDen.esp`,
`DLCVileLair.esp`, and `Knights.esp`.

`Oblivion.esm` SHA-256 is
`a26e21ea8c3041f8737ffb3a266129dedb7f8a88590625ecfecd5eb7f66b4a70`.
Fingerprints for every plugin are recorded in
`build/oblivion-compat/m2-morrowind-regression/baseline.json`.

The accepted graph contains:

- 1,188,447 stable keys;
- 1,190,841 override revisions;
- 2,949,394 reference/parent edges;
- 182,718,916 serialized bytes;
- fingerprint `fnv1a64:b1ea905be1c80ad0`;
- identical state after deserialize/reserialize;
- identical state after reversing runtime plugin indices;
- zero enable-parent cycles;
- 4,364 unresolved edges, all matched by ten exact reviewed rules;
- zero unreviewed edges and zero stale exception rules.

The reviewed set consists of six engine-created/reserved targets and four
dangling released-content targets, each represented by one count-locked rule.
Any new edge, removed edge, count change, deleted winner, or cycle fails the
gate. The rules and their rationale are tracked in
`scripts/data/oblivion_compat/tes4_form_graph_exceptions.json`.

## Verification

```sh
cmake -S . -B build -DBUILD_OPENMW_TESTS=ON
cmake --build build --target openmw esmtool components-tests openmw-tests -j2

./build/components-tests \
  --gtest_filter='FormKeyTest.*:Tes4FormIdFieldsTest.*:ESM4RawRecord.*'
./build/components-tests
./build/openmw-tests

python3 -m py_compile scripts/oblivion_compat.py
python3 -m unittest discover -s scripts/tests -p 'test_*.py'

python3 scripts/oblivion_compat.py form-graph \
  --esmtool build/esmtool \
  --oblivion-data "/path/to/Oblivion/Data" \
  --output build/oblivion-compat/m2-acceptance

python3 scripts/oblivion_compat.py baseline \
  --build build \
  --oblivion-data "/path/to/Oblivion/Data" \
  --morrowind-data "/path/to/Morrowind/Data Files" \
  --output build/oblivion-compat/m2-morrowind-regression
```

Results on the acceptance host:

- focused M2 component tests: 32/32 passed, including 128 deterministic
  randomized multi-plugin matrices;
- complete component suite: 1,480/1,480 passed;
- complete OpenMW runtime suite: 494/494 passed;
- compatibility harness tests: 12/12 passed;
- full official graph gate: passed in 17.08 seconds;
- installed Oblivion audit: 11/11 plugins parsed and 17/17 archives listed;
- Morrowind executable regression under Xvfb: 14/14 scenarios passed, with no
  failures, incomplete tests, timeout, or nonzero exit.

Generated evidence is local by design:

- `build/oblivion-compat/m2-acceptance/acceptance.json`
- `build/oblivion-compat/m2-acceptance/acceptance.html`
- `build/oblivion-compat/m2-acceptance/acceptance.png`
- `build/oblivion-compat/m2-acceptance/form-graph.json`
- `build/oblivion-compat/m2-acceptance/form-graph.json.formkeys.bin`
- `build/oblivion-compat/m2-morrowind-regression/baseline.json`
- `build/oblivion-compat/m2-morrowind-regression/morrowind/morrowind-tests.log`

## Visual inspection

The acceptance HTML was rendered headlessly in Chromium at 1440x1000 and
inspected directly through `acceptance.png`. It clearly displays the green M2
PASS state, graph totals, fingerprint, both stability flags, zero unreviewed
edges, zero cycles, and all ten exception rows. The table is readable and has
no clipping, overlap, missing text, or incorrect failure styling.

M2 changes identity, loading, storage, and inspection rather than rendered
gameplay or UI. No gameplay golden legitimately changes in this milestone;
the executable Morrowind scenario gate supplies the relevant runtime
backward-compatibility check.

## Success criteria and boundary

M2's deliver, verification, and success criteria are satisfied: every
reference discovered in all installed official records has a live winner or a
count-locked documented vanilla exception, and the complete authoritative
graph is stable across runtime-index changes and process-style binary reload.
The typed runtime stores and instantiated TES4 references use that same stable
identity, while all component, runtime, and Morrowind regressions remain green.

M2 has no open acceptance gates. It does not claim Oblivion standalone boot,
gameplay simulation, persistence of all mutable state, or Oblivion UI parity;
those remain bounded work in M3 and later milestones.
