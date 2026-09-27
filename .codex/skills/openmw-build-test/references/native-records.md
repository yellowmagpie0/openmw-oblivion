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

The `autoNpc()` test helper starts with `TES4_PCLevelOffset` enabled. In a
fixture without a World, clear that flag and set `levelOrOffset` explicitly
before inserting the record. Constructing a separate `MWWorld::Player` does
not satisfy scaled construction's `Environment::getWorld()->getPlayerPtr()`
dependency. A crash in `resolveOblivionActorConstructionStats` at the first
`getCreatureStats` can therefore be fixture setup, before authority code runs.

When testing authority bindings, set the synthetic record's `mFormKey` as well
as passing its key to `insertStatic`. The latter indexes the store but does not
fill an otherwise empty embedded identity. Real record loading supplies both.
Set the reference's own FormKey separately and assert both identities before
publication; do not weaken production identity validation for an incomplete
fixture. Include the concrete native record header in code using `Ptr.get<T>()`.

The Oblivion player is a projected `ESM::NPC` owned by `MWWorld::Player`, not a
native `ESM4::Npc`. The actual-player authority fixture in the same test file
constructs Player from a static NPC inserted through `ESMStore::insertStatic`
(so the generic record index is populated). Register `MWClass::Npc`, construct
an empty `ESM::NpcState`, call `blank()`, and load it through the class's
`readAdditionalState`. This creates the real private NpcCustomData without
requiring a large World fake for inventory filling/PRNG. Keep `mMissingACDT`
and `mRecalcDynamicStats` false; populate shared attributes/skills first. Empty
spell/inventory state avoids unrelated World callbacks. This tests the actual
projected class, not full World activation or normal gameplay.

When clearing TES4_PCLevelOffset on the `autoNpc()` engine fixture, also assign
a positive absolute `levelOrOffset` (for example 2). That fixture starts at -1;
clearing only the flag makes class initialization reject a nonpositive level
before the behavior under test executes. Retain such fixture failures and fix
the fixture, not the production validation.

### Standalone CreatureStats lifecycle fixtures

`CreatureStats::setHealth` can consult the World clock on the first death; do
not invoke it without a World fixture. For a legacy dead-state regression, read
an initialized saved CreatureStats instead. Both `writeState` and `readState`
require an initialized spell list: insert an actual NPC base and call
`stats.getSpells().setSpells(base.mId)` for every standalone stats instance.
When extending a fixture that later tests an older schema, inspect its entire
body and remove newer fields when constructing that older-format state.
