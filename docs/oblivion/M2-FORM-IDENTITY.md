# M2 form identity, masters, and overrides

Status: implementation started on 2026-08-08; foundational deliverable is
green, full milestone acceptance remains open.

## Delivered source

- `components/esm/formkey.*` defines normalized, load-order-independent
  content identity as `(defining plugin, 24-bit local ID)` and a separate
  namespaced 64-bit identity for runtime-created forms.
- `FormKeyResolver` converts current load-order `FormId` values without making
  those indices persistent, resolves raw on-disk master/self references, and
  reports missing plugins rather than guessing.
- `DynamicFormKeyAllocator` has explicit restartable serial state and overflow
  checks.
- `FormKeyIndex` retains override history and exposes the winning/deleted
  record, persistent/temporary/visible-distant child classification, and
  unresolved enable-parent relationships.
- `ESM4::Reader` exposes stable keys for record headers and FormID subrecords;
  lossless raw records retain the key alongside the current runtime ID.
- `apps/components_tests/esm/formkey.cpp` covers normalization, serialization,
  plugin reorder/removal, raw master remapping, deletion/override history,
  enable parents, dynamic allocator restart and extreme runtime IDs.

## Verification

```sh
cmake --build build --target components-tests -j2
./build/components-tests --gtest_filter='FormKeyTest.*'
```

Success criteria for the delivered foundation are exact stable-key round trips,
identical identity after plugin reorder, a non-resolving result for missing
content, deterministic winner/history behavior, explicit deletion, no signed
overflow for extreme runtime IDs, and green Morrowind regressions.
The focused `FormKeyTest` suite currently passes 11/11 cases.

## Open acceptance gates

M2 is not yet accepted. Remaining work is deliberately bounded:

1. Feed every typed TES4 store and instantiated reference through `FormKey`,
   not only the reader/raw and native-save boundaries.
2. Build the complete winning-record/reference graph for every installed
   official DLC order and emit unresolved-edge JSON with a reviewed vanilla
   exception list.
3. Add randomized multi-plugin order matrices involving master removal,
   deleted overrides, persistent/temporary children and enable-parent cycles.
4. Save, reorder plugins, reload, and compare the full authoritative graph.

The M2 milestone passes only when every official reference has the expected
winner or a documented exception and the graph remains stable across reorder
and process restart.
