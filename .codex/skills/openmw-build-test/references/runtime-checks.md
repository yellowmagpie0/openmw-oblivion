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
