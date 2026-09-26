# Verifying a new typed TES4 record

A component parser build does not exercise the engine's store registration.
Search for an existing native record type across **all** of `apps`, `components`
and `scripts` before adding another. CSTY, FACT and SKIL provide working examples.
The registration points in this fork include:

- `components/esm/defs.hpp`, the ESM4 record header and
  `components/esm4/records.hpp`, plus `components/CMakeLists.txt`.
- `apps/openmw/mwworld/esmstore.hpp`: both a visible declaration/include and
  `StoreTuple`. An include in the component record catalog alone is insufficient.
- `apps/openmw/mwworld/esmstore.cpp`: generic native dispatch handles types with
  `sRecordId`; apply the TES4 version gate where later-game layouts differ.
- **`apps/openmw/mwworld/store.cpp`: explicit `TypedDynamicStore<T>`
  instantiation.** Omitting this can compile the parser and the entire engine
  library, then fail only when linking `openmw`.
- `apps/esmtool/tes4.cpp`: typed dispatch, later-game raw fallback and useful
  semantic dump fields. Audit any consumers of the former raw path.
- The affected Python semantic audit and its reviewed hash-bound count lock.
  Decode expected values independently of C++, retaining failed audit evidence.

Run component/sanitizer tests for field lengths, malformed values, compression,
required/duplicate fields and raw preservation. Use the **actual ESMStore loader**
for master-order remapping, overrides, tombstone deletion and stable keys;
`apps/openmw_tests/mwworld/teststore.cpp` contains small editable binary fixtures.
A plain array resolver test cannot establish store registration or deletion.

Include runner `--mode engine` so `openmw`, `openmw-tests` and `esmtool` are all
linked and engine tests run. After changing only engine registration to fix a
build, retain the earlier parser evidence and run a new engine attempt; rerun
component tests if their inputs/implementation also changed. Label each evidence
scope accurately. Finally compare typed `esmtool` fields for actual installed
records against the independent raw decoder and review the full official audit.

Avoid starting a broad revision-locked runner while still planning source edits.
When a broad run is already active, prepare subsequent work in ignored scratch
files and apply it only after the runner ends. Do not treat draft-only checks as
verification of the committed production source.

For live native actor tests, reuse the fixtures in
`apps/openmw_tests/mwworld/testoblivionactorstats.cpp`: create an `Environment`,
set its real `ESMStore` and `WorldModel` (with `ReadersCache`), and populate the
shared Attribute/Skill definitions before constructing actor custom data.
`InventoryStore` uses `SafePtr`, which requires that WorldModel even with empty
inventory. Fixed-level actors can be tested without a World implementation;
scaled construction needs the actual player. Query through `Ptr.getClass()`
to exercise the shared gameplay readers. Keep these results distinct from
normal-input gameplay and fresh-process persistence tests.

When testing authority bindings, set the synthetic record's `mFormKey` as well
as passing its key to `insertStatic`. The latter indexes the store but does not
fill an otherwise empty embedded identity. Real record loading supplies both.
Set the reference's own FormKey separately and assert both identities before
publication; do not weaken production identity validation for an incomplete
fixture. Include the concrete native record header in code using `Ptr.get<T>()`.
