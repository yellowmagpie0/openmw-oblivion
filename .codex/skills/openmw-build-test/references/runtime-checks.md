# Native runtime/restart evidence

Read the current M15 implementation plan and case inventory before selecting
cases. S1 established the observation harness, not combat acceptance. Existing
entrypoints include `scripts/oblivion_compat.py m15-restart`; inspect `--help`
and its manifest schema when changing a scenario.

Proven S1 invocation, from repo root (use a fresh output directory):

```bash
python3 scripts/oblivion_compat.py m15-restart \
  scripts/data/oblivion_compat/oblivion_m15_observation.json \
  scripts/data/oblivion_compat/oblivion_m15_continuation.json \
  --output build/oblivion-compat/m15/S1/NEW-RUN \
  --variable openmw="$PWD/build/openmw" \
  --variable resources="$PWD/build/resources" \
  --variable fixture_data="$PWD/build/oblivion-compat/m15/S1/observation-03/fixture" \
  --variable 'oblivion_data=/home/maciek/.local/share/Steam/steamapps/common/Oblivion/Data'
```

Check fixture availability rather than reconstructing from this example's
path. Keep editable manifests as source and generated plugins/captures under
ignored evidence paths. Read `docs/DEVELOPMENT-HANDBOOK.md` before changes to
content-authoring support/MCP. The imported MCP server targets TES3, not TES4.

Verify PID/process epochs, simulation ticks, pause state and action
acknowledgements. Save -> quit -> a fresh process loading -> resave must be
real; a second in-process load is not restart evidence. Assert semantics and
case coverage, not generic startup logs, file existence or telemetry-only state.
The existing harness has negative tests for corrupt evidence and wrong action
acknowledgements; extend rather than bypass those checks.

A prior loading crash required stopping rendering threads around the outer
LoadingScreen via SingleThreaded. If this reappears, inspect that implementation
and the preserved evidence before inventing an unrelated workaround.

## Legacy native action-service migration

The v8 action-service change has a reusable scenario:
`scripts/data/oblivion_compat/oblivion_m15_legacy_action_service.json`.
It expects an existing S1 M15Observation fixture save copied to
`OUTPUT/userdata/saves/M15Legacy/Quicksave.omwsave`. Create a fresh output
folder yourself, preserve a second `OUTPUT/input-pristine.omwsave` copy, and
record the source SHA-256 before starting. Never point `--load-savegame` at
historical evidence: a subsequent F5 may overwrite that slot.

A proven v7 input is
`build/oblivion-compat/m15/S1/ticks-restart-01/first/snapshots/after.omwsave`
(SHA-256 `ceccf09c066272636aa4e8a49e0d34f2825202cf0dd258622ca2685a4af0b439`).
Use the same fixture_data, oblivion_data, resources and openmw variables as
above, but invoke `scenario` with this manifest and the prepared output folder.
The manifest requires actual load/apply/save log boundaries and a native save.
After completion, independently decode the resulting save with
`scripts.tes4_runtime_state.load_save`. Require:

- The scenario report passed with no unreviewed errors.
- The resaved schema equals `tes4_runtime_state.CURRENT_VERSION`; file presence
  alone could merely identify the pre-copied input.
- `physical_actions` equals `{"next": 1, "pending": []}` for a pre-v8 input.
- Original source and pristine copy still match the input hash.

Record these assertions and input/output/binary hashes in a migration report.
This is idle-service migration coverage, not in-flight hit/projectile recovery.
The ordinary v8 save/quit/fresh-load/resave case still uses `m15-restart` above;
its independent live JSON and Python-decoded binary comparison checks the full
new action-state field as well as the existing state.

For profile isolation, the existing `morrowind_m14_regression.json` scenario
quicksaves and rejects GPRO/T4VR/T4ST/OMW4STATE/native-state markers. It requires
`morrowind_data` in addition to openmw/resources. Its terrain-facing screenshot
and short idle run do not establish full Morrowind gameplay regression coverage.

Schema 9 reuses this workflow. A proven v8 input is
`build/oblivion-compat/m15/S3/action-service-restart-01/first/snapshots/after.omwsave`
(SHA-256 `7a071b415a1acbbf96d0ffd9e32471e43f4d2647213c83ad1e43f4d55fc5286c`).
For v8-to-v9 migration additionally require `native_actor_values == []` and
preservation of the input action ledger. The passed example is
`S3/native-actor-values-legacy-01/migration-verification.json`. Empty native
entries prove only idle schema migration, not live actor-authority recovery.

When testing promotion from pre-character-generation/AI schemas, build a
complete current fixture or use the actual migration helper. Changing only the
version omits required name/race/class and nonzero AI RNG defaults. Run the
focused Python fixture before starting a broad build; do not start the runner
unconditionally after a failed preflight. Preserve failed evidence directories.

Schema 10 uses the same idle checks. A proven v9 input is
`build/oblivion-compat/m15/S3/native-actor-values-restart-01/first/snapshots/after.omwsave`
(SHA-256 `3f81fc4e94500d2fc2c1a49e31a37fbe08c159f6a7dd07607b0e706bbdd25e69`).
Require the current schema, preserved ledger/native entries and unchanged source
and pristine hashes. Passed example:
`S3/player-value-legacy-01/migration-verification.json`. Populated v9 player
entries migrate without raw form inputs; the explicit player publication API
rejects their activation until those inputs are deliberately initialized.

## Rejecting structurally valid but non-actor bindings

`oblivion_m15_reject_native_actor_binding.json` is a real-loader negative case.
Prepare `OUTPUT/userdata/saves/M15Rejected/Quicksave.omwsave` from a **copy** of
an S1 fixture save. Use the existing state helpers and editable fault source:

```python
state = load_save(source)
fault = json.loads(Path("scripts/data/oblivion_compat/oblivion_m15_native_actor_binding_fault.json").read_text())
state.update(fault)
write_save(source, slot, state)  # source and slot MUST be different paths
assert load_save(slot)["native_actor_values"] == fault["native_actor_values"]
```

Record source/fault/mutated-input hashes before running the scenario with the
usual fixture variables. Afterwards require the exact native invalid-base error,
no `Applied TES4 runtime state:` or save-write boundary, and unchanged source and
mutated-input hashes. The error must come from content binding validation, not
binary/schema parsing. Passed example:
`S3/unloaded-actors-reject-01/binding-rejection-verification.json`.
Inspect the rejection dialog and pair the negative case with a valid restart.

## Shared base overrides (schema 11)

A proven v10 migration input is
`build/oblivion-compat/m15/S3/player-value-restart-01/first/snapshots/after.omwsave`
(SHA-256 `91c806490e045b779b7627a54ff50605967bdc3f640e1677a9b78ec748130867`).
Use `oblivion_m15_legacy_action_service.json` with a private copied slot as above.
Require current schema, `native_actor_bases == []`, unchanged actor entries and
ledger, and unchanged source/pristine hashes. The passing example is
`S3/shared-base-state-legacy-01/verification.json`.

To test populated typed persistence, merge the editable
`scripts/data/oblivion_compat/oblivion_m15_native_base_roundtrip.json` into a
copied decoded state and call `write_save(source, privateSlot, state)`. Run the
same load/resave manifest, then require exact `native_actor_bases` equality.
This distinguishes Health int32 16,777,217 from an extra AV's float 16,777,216.
Passing example: `S3/shared-base-state-populated-01/verification.json`.
Injected state verifies storage; it is not a normal-input SetAV or live actor
publication test.

The invalid-base counterpart uses
`oblivion_m15_native_base_binding_fault.json` and
`oblivion_m15_reject_native_base_binding.json`. It must fail content binding
before `Applied TES4 runtime state:` and preserve source/pristine/mutated-input
hashes, as in `S3/shared-base-state-reject-01/verification.json`. Pair it with
`m15-restart` and Morrowind save isolation. Keep all generated saves and images
in fresh ignored output directories. Hash the large engine binary once per
unchanged build and reuse that verified identity in the reports.

Shared-base consistency has an additional positive/negative pair:
`oblivion_m15_native_actor_base_roundtrip.json` merges matching actor/base
snapshots for the ordinary private-slot load/resave manifest;
`oblivion_m15_native_base_conflict.json` merges a structurally valid conflicting
raw player Health input. Run the latter with
`oblivion_m15_reject_native_base_conflict.json`. Require the exact conflict
message before Applied/save boundaries. Passed examples are
`S3/shared-base-writer-populated-01` and `S3/shared-base-writer-conflict-01`.
Verify both native vectors, input preservation and scenario status independently.
`load_save` takes a `pathlib.Path`, and its version key is `schema_version`.

## Native script query positive/negative/restart pair

Merge `oblivion_m15_native_query_input.json` into a private copied S1 state,
write the current save and run `oblivion_m15_native_query.json`. The event file
exercises short/long current/base aliases and fractional Script/Damage values.
Require all eight returned values, exact native vectors after resave and
unchanged pristine input. `oblivion_m15_native_query_negative.json` deliberately
requires 96.5 where the actual result is 95.5: require harness exit 1, all actions
complete, and exactly that missing expected line with no other error. This is a
successful negative control, not a passing scenario. Copy the positive resave
to another private slot and run the positive manifest in a fresh process.
Examples: `S3/native-script-query-{runtime,negative,continuation}-01`.
Inspect each capture. These fixtures inject an unpublished authority snapshot;
they prove script reads and persistence, not normal actor initialization or
live gameplay publication. Scheduled acceptance commands log returned values;
ordinary production GetAV calls do not produce this diagnostic traffic.

## Lifecycle fields and death-event FIFO (schema 12)

`oblivion_m15_native_life_roundtrip.json` is the editable injected lifecycle/
queue overlay for the private-slot load/resave course. The actor is alive and
the two events are historical queued notifications; a callback may execute
after resurrection. Compare `native_actor_life`, `next_death_event` and
`pending_death_events` exactly, including FIFO order, across fresh processes.
Current examples `S3/native-lifecycle-{populated,continuation}-01` retain events
because automatic dispatch is not yet wired. Do not call this a death test.
The structurally valid static-object binding fault is
`oblivion_m15_native_life_binding_fault.json`; use
`oblivion_m15_reject_native_life_binding.json` and require rejection before
Applied/save boundaries with unchanged inputs.

A v11 migration source is
`S3/native-script-query-continuation-01/userdata/saves/M15Legacy/Quicksave.omwsave`
(SHA-256 `f95667daf6a5757a9f7ec659e1413bdce9ba38df6d3a83e349f2a5aa69dd6039`).
Copy it, do not rewrite the source. Expected v12 lifecycle defaults are empty
actor/event lists and next_death_event1; never derive death from negative Health.
Legacy-only obscript.dead remains untouched until its live adapters are wired;
coexisting typed/legacy fields must agree. Pin any tail-offset wire test to its
actual schema version when appending fields, retaining current-schema combined
coverage. Python reference fixtures must include `owner` and `lock_level`, even
when those fields are unrelated to the behavior being tested.

For native GetDead, use `oblivion_m15_native_{dead,essential}_query_input.json`
overlays and the matching `_query.json` manifests. Both have the same scheduled
GetDead command; phase Dead expects1 and EssentialUnconscious expects0. Copy
each resave into a fresh private slot for continuation. The
`oblivion_m15_native_life_query_negative.json` control loads essential input and
allows all actions to finish, but deliberately expects the wrong final result1.
Require exactly that missing line, scenario exit1 and no other error. Examples:
`S3/native-life-query-{dead,essential}-01`, their `-continuation-01` directories,
and `native-life-query-negative-01`. Inspect every capture and compare native
life, queue, value and base vectors plus pristine hashes. This is script-query
and persistence evidence; injected lifecycle remains unpublished to gameplay.

## Registered native script writer course

Start `oblivion_m15_native_writer.json` from a private v12 save containing the
`oblivion_m15_native_query_input.json` overlay. It publishes actual Player views
through Set/Mod/Force short/long aliases. Expected saved Strength is
`[40,.5,20.5,-1]`, Health `[87,10,5,-2]`, raw player inputs `[7,3,0,0]`, one
Strength40 base override, Alive lifecycle and no death events. Copy that resave
into a fresh slot for `oblivion_m15_native_writer_continuation.json`. Its negative
base Health write must produce current -7/base -20 with GetDead0, then restore
Health7; final base overrides additionally contain raw Health7. The negative
control uses the original injected input and deliberately expects61 instead of60.
Require only that missing log line and all actions complete. Examples:
`S3/native-script-writer-{runtime,continuation,negative}-02`. Preserve all source
hashes and inspect every capture. This exercises live writer publication from
registered authority, not automatic actor registration or all legacy adapters.


Native death queue delivery uses `oblivion_m15_native_death_events.json`, its
continuation and negative manifests. Prepare private inputs as in
`S3/authority-draft/death-dispatch-prepare.py`; retain pristine hashes. Verify
ordered IDs 4/9, saved empty queue/next ID 10, unchanged native vectors, and no
fresh-process replay. The negative expects absent ID 5 and must fail only that
assertion after all actions complete. Evidence lives in
`S3/native-death-dispatch-{runtime,continuation,negative}-01`. Older lifecycle
persistence binaries intentionally retained the queue before this consumer
existed; those are historical evidence, not current delivery expectations.
These fixtures have no attached OnDeath body and do not prove callback saves.

For actual callback-save coverage, build `tes4_m15_lifecycle_fixture.py` using
the pinned master, boot recipe and editable `m15_lifecycle_observer.obscript`.
Use `oblivion_m15_native_callback_initial.json` for a fresh save, then the runtime,
negative and continuation manifests. The observer counts OnDeath and saves at
count 1. Assert that callback Autosave contains only pending ID 9 and count 1;
final Quicksave must have no events and count 2. Restart Autosave and require
only ID 9, count 2 and no new Autosave. Preparation/independent checks are retained
in `S3/authority-draft/native-callback-{prepare,verify}.py`; passing courses are
runtime/negative-01 and continuation-03. Preserve source/pristine hashes.
A copied save retains its internal description: an Autosave renamed Quicksave
is still an Autosave slot and a later Quicksave may acquire a numeric suffix.
Keep its real filename and assert both files. Captures face the room wall and
cannot establish NPC pose; these are injected-history callback tests.
