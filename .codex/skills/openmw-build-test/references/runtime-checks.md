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
